#include "aby_interpreter.h"
#include "../../../abycore/circuit/booleancircuits.h"
#include "../../../abycore/circuit/arithmeticcircuits.h"
#include "../../../abycore/circuit/circuit.h"
#include "../../../abycore/sharing/sharing.h"
#include "ezpc.h"

#include <regex>
#include <deque>

using namespace std::chrono;

int PUBLIC = 2;

enum op {
	ADD_,
	SUB_,
	MUL_,
	EQ_,
	GT_,
	LT_,
	GE_,
	LE_,
	REM_,
	AND_,
	OR_,
	XOR_,
	CONS_,
	CONS_ARRAY,
	MUX_, 
	NOT_,
	SHL_,
	LSHR_,
	DIV_,
	SELECT_,
	SELECT_CONS,
	STORE_,
	STORE_CONS,
	FIELD_, 
	FIELD_VEC, 
	TUPLE_,
	IN_,
	OUT_,
	CALL_,
};

bool parse_call_op(std::string op) {
	if (op.find("CALL") != std::string::npos) {
		return true;
	}
	return false;
}

op op_hash(std::string o) {
    if (o == "ADD") return ADD_;
	if (o == "SUB") return SUB_;
	if (o == "MUL") return MUL_;
	if (o == "EQ") return EQ_;
	if (o == "GT") return GT_;
	if (o == "LT") return LT_;
	if (o == "GE") return GE_;
	if (o == "LE") return LE_;
	if (o == "REM") return REM_;
	if (o == "AND") return AND_;
	if (o == "OR") return OR_;
	if (o == "XOR") return XOR_;
	if (o == "CONS") return CONS_;
	if (o == "CONS_ARRAY") return CONS_ARRAY;
	if (o == "MUX") return MUX_;
	if (o == "NOT") return NOT_;
	if (o == "DIV") return DIV_;
	if (o == "SHL") return SHL_;
	if (o == "LSHR") return LSHR_;
	if (o == "IN") return IN_;
	if (o == "OUT") return OUT_;
	if (o == "SELECT") return SELECT_;
	if (o == "SELECT_CONS") return SELECT_CONS;
	if (o == "STORE") return STORE_;
	if (o == "STORE_CONS") return STORE_CONS;
	if (o == "FIELD") return FIELD_;
	if (o == "FIELD_VEC") return FIELD_VEC;
	if (o == "TUPLE") return TUPLE_;
	if (parse_call_op(o)) return CALL_;
    throw std::invalid_argument("Unknown operator: "+o);
}

bool is_bin_op(op o) {
	return o == ADD_ || o == SUB_ || o == MUL_ || o == EQ_ || o == GT_ || o == LT_ || o == GE_ || o == LE_ || o == REM_ || o == DIV_ || o == AND_ || o == OR_ || o == XOR_;
}

std::string parse_fn_name(std::string op) {
	assert(("Op is call op", op_hash(op) == CALL_));
	
	std::regex rex("\\((.*)\\)");
    std::smatch m;
    if (regex_search(op, m, rex)) {
       return m[1];
	} else {
		throw std::invalid_argument("Unable to parse function name out of Call op: "+op);
	}

}

std::vector<std::string> split_(std::string str) {
    std::vector<std::string> result;
    std::istringstream ss(str);
    std::string word; 
    while (ss >> word) {
        result.push_back(word);
    }
    return result;
}

Circuit* get_circuit(std::string circuit_type, ABYParty* party) {
	std::vector<Sharing*>& sharings = party->GetSharings();
	Circuit* acirc = sharings[S_ARITH]->GetCircuitBuildRoutine();
	Circuit* bcirc = sharings[S_BOOL]->GetCircuitBuildRoutine();
	Circuit* ycirc = sharings[S_YAO]->GetCircuitBuildRoutine();

	Circuit* circ;
	if (circuit_type == "a") {
		circ = acirc;
	} else if (circuit_type == "b") {
		circ = bcirc;
	} else if (circuit_type == "y") {
		circ = ycirc;
	} else {
		throw std::invalid_argument("Unknown circuit type: " + circuit_type);
	}
	return circ;
}

share* add_conv_gate(
	std::string from, 
	std::string to, 
	share* wire, 
	ABYParty* party) {
	std::vector<Sharing*>& sharings = party->GetSharings();
	Circuit* acirc = sharings[S_ARITH]->GetCircuitBuildRoutine();
	Circuit* bcirc = sharings[S_BOOL]->GetCircuitBuildRoutine();
	Circuit* ycirc = sharings[S_YAO]->GetCircuitBuildRoutine();

	if (from == "a" && to == "b") {
		return bcirc->PutA2BGate(wire, ycirc);
	} else if (from == "a" && to == "y") {
		return ycirc->PutA2YGate(wire);
	} else if (from == "b" && to == "a") {
		return acirc->PutB2AGate(wire);
	}  else if (from == "b" && to == "y") {
		return ycirc->PutB2YGate(wire);
	} else if (from == "y" && to == "a") {
		return acirc->PutY2AGate(wire, bcirc);
	} else if (from == "y" && to == "b") {
		return bcirc->PutY2BGate(wire);
	} else {
		return wire;
	}
}

void process_instruction(
	std::string circuit_type,
	std::unordered_map<std::string, std::vector<share*>>* cache, 
	std::unordered_map<std::string, uint32_t>* const_cache,
	std::unordered_map<share*, std::string>* share_type_cache, 
	std::deque<share*>* rewire_inputs,
	bool rewire_outputs,
	std::unordered_map<std::string, uint32_t>* params,
	std::vector<std::string> input_wires, 
	std::vector<std::string> output_wires, 
	std::vector<share*>* out, 
	std::string op,
	e_role role, 
	uint32_t bitlen,
	ABYParty* party) {
	std::vector<Sharing*>& sharings = party->GetSharings();
	Circuit* acirc = sharings[S_ARITH]->GetCircuitBuildRoutine();
	Circuit* bcirc = sharings[S_BOOL]->GetCircuitBuildRoutine();
	Circuit* ycirc = sharings[S_YAO]->GetCircuitBuildRoutine();

	share* result;

	if (is_bin_op(op_hash(op))) {
		Circuit* circ = get_circuit(circuit_type, party);

		share* wire1 = cache->at(input_wires[0])[0];
		share* wire2 = cache->at(input_wires[1])[0];

		// add conversion gates
		std::string share_type_1 = share_type_cache->at(wire1);
		std::string share_type_2 = share_type_cache->at(wire2);

		wire1 = add_conv_gate(share_type_1, circuit_type, wire1, party);
		wire2 = add_conv_gate(share_type_2, circuit_type, wire2, party);

		switch(op_hash(op)) {
			case ADD_: {
				result = circ->PutADDGate(wire1, wire2);
				break;
			}
			case SUB_: {
				result = circ->PutSUBGate(wire1, wire2);
				break;
			}
			case MUL_: {
				// const mult trick
				if (circuit_type == "y" || circuit_type == "b") {
					if(const_cache->find(input_wires[0]) != const_cache->end()) {
						uint32_t value = const_cache->at(input_wires[0]);
						// do 
						result = constMult(circ, wire2, value);
					} else if (const_cache->find(input_wires[1]) != const_cache->end()) {
						uint32_t value = const_cache->at(input_wires[1]);
						result = constMult(circ, wire1, value);
					} else {
						result = circ->PutMULGate(wire1, wire2);
					}
				} else{
					result = circ->PutMULGate(wire1, wire2);
				}
				break;
			}
			case GT_: {
				result = circ->PutGTGate(wire1, wire2);
				break;
			}
			case LT_: {
				result = circ->PutGTGate(wire2, wire1);
				break;
			}
			case GE_: {
				result = ((BooleanCircuit *)circ)->PutINVGate(circ->PutGTGate(wire2, wire1));
				break;
			}
			case LE_: {
				result = ((BooleanCircuit *)circ)->PutINVGate(circ->PutGTGate(wire1, wire2));
				break;
			}
			case REM_: {
				result = signedmodbl(circ, wire1, wire2);
				break;
			}
			case AND_: {
				result = circ->PutANDGate(wire1, wire2);
				break;
			}
			case OR_: {
				result = ((BooleanCircuit *)circ)->PutORGate(wire1, wire2);
				break;
			}
			case XOR_: {
				result = circ->PutXORGate(wire1, wire2);
				break;
			}
			case DIV_: {
				result = signeddivbl(circ, wire1, wire2);
				break;
			} 
			case EQ_: {
				result = circ->PutEQGate(wire1, wire2);
				break;
			}
			default: {
				throw std::invalid_argument("Unknown binop: " + op);
			}
		}

		for (auto o: output_wires) {
			(*cache)[o] = {result};
			(*share_type_cache)[result] = circuit_type;
		}
	} else {
		switch(op_hash(op)) {
			case CONS_: {
				int value = std::stoi(input_wires[0]);
				int len = std::stoi(input_wires[1]);
				if (circuit_type == "a") {
					if (len == 1) {
						result = put_cons1_gate(acirc, value);
					} else if (len == 32) {
						result = put_cons32_gate(acirc, value);
					} else {
						throw std::runtime_error("Unknown const bit len: "+input_wires[2]);
					}
				} else if (circuit_type == "b") {
					if (len == 1) {
						result = put_cons1_gate(bcirc, value);
					} else if (len == 32) {
						result = put_cons32_gate(bcirc, value);
					} else {
						throw std::runtime_error("Unknown const bit len: "+input_wires[2]);
					}
				} else if (circuit_type == "y") {
					if (len == 1) {
						result = put_cons1_gate(ycirc, value);
					} else if (len == 32) {
						result = put_cons32_gate(ycirc, value);
					} else {
						throw std::runtime_error("Unknown const bit len: "+input_wires[2]);
					}
				} else {
					throw std::runtime_error("Unknown share type: "+circuit_type);
				}
				for (auto o: output_wires) {
					(*cache)[o] = {result};
					(*share_type_cache)[result] = circuit_type;
				}
				break;
			}
			case CONS_ARRAY: {
				std::vector<share*> array;
				for (auto s: input_wires) {
					auto array_share = cache->at(s)[0];
					array.push_back(array_share);
				}
				for (auto o: output_wires) {
					(*cache)[o] = array;
				}
				break;
			}
			case MUX_: {
				Circuit* circ = get_circuit(circuit_type, party);
				// sel 
				share* sel = cache->at(input_wires[0])[0];
				std::string share_type_sel = share_type_cache->at(sel);
				sel = add_conv_gate(share_type_sel, circuit_type, sel, party);
	
				std::vector<share*> t_wires = cache->at(input_wires[1]);
				std::vector<share*> f_wires = cache->at(input_wires[2]);
				assert((t_wires.size() == f_wires.size()));

				std::vector<share*> res_wires;
				for (int i = 0; i < t_wires.size(); i++) {
					auto t_wire = t_wires[i];
					auto f_wire = f_wires[i];
					
					// add conversion gates
					std::string t_share_type = share_type_cache->at(t_wire);
					std::string f_share_type = share_type_cache->at(f_wire);

					t_wire = add_conv_gate(t_share_type, circuit_type, t_wire, party);
					f_wire = add_conv_gate(f_share_type, circuit_type, f_wire, party);

					auto res_wire = result = circ->PutMUXGate(t_wire, f_wire, sel);
					res_wires.push_back(res_wire);
				}

				// set result
				(*cache)[output_wires[0]] = res_wires;
				for (auto wire: res_wires) {
					(*share_type_cache)[wire] = circuit_type;
				}
				break;
			}
			case NOT_: {
				Circuit* circ = get_circuit(circuit_type, party);
				share* wire = cache->at(input_wires[0])[0];

				// add conversion gates
				std::string share_type = share_type_cache->at(wire);
				wire = add_conv_gate(share_type, circuit_type, wire, party);
				
				result = ((BooleanCircuit *)circ)->PutINVGate(wire);

				for (auto o: output_wires) {
					(*cache)[o] = {result};
				}
				(*share_type_cache)[result] = circuit_type;
				break;
			}
			case SHL_: {
				Circuit* circ = get_circuit(circuit_type, party);
				share* wire = cache->at(input_wires[0])[0];
				auto share_type_from = share_type_cache->at(wire);
				wire = add_conv_gate(share_type_from, circuit_type, wire, party);
				int value = std::stoi(input_wires[1]);
				result = left_shift(circ, wire, value);
				for (auto o: output_wires) {
					(*cache)[o] = {result};
				}
				(*share_type_cache)[result] = circuit_type;
				break;
			} 
			case LSHR_: {
				Circuit* circ = get_circuit(circuit_type, party);
				share* wire = cache->at(input_wires[0])[0];
				auto share_type_from = share_type_cache->at(wire);
				wire = add_conv_gate(share_type_from, circuit_type, wire, party);
				int value = std::stoi(input_wires[1]);
				result = logical_right_shift(circ, wire, value);
				for (auto o: output_wires) {
					(*cache)[o] = {result};
				}
				(*share_type_cache)[result] = circuit_type;
				break;
			} 
			case SELECT_CONS: {
				auto array_shares = cache->at(input_wires[0]);
				auto index = std::stoi(input_wires[1]);
				assert((index < array_shares.size()));
				auto select_share = array_shares[index];
				(*cache)[output_wires[0]] = {select_share};
				break;
			}
			case STORE_CONS: {
				auto array_shares = cache->at(input_wires[0]);
				auto index = std::stoi(input_wires[1]);
				auto value_share = cache->at(input_wires[2])[0];
				assert((index < array_shares.size()));
				array_shares[index] = value_share;
				(*cache)[output_wires[0]] = array_shares;
				break;
			}
			case SELECT_: {
				Circuit* circ = get_circuit(circuit_type, party);
				assert(("Select circuit type not supported in arithmetic sharing", circuit_type != "a"));
				auto array_shares = cache->at(input_wires[0]);
				auto index_wire = cache->at(input_wires[1])[0];
				index_wire = add_conv_gate(share_type_cache->at(index_wire), circuit_type, index_wire, party);
				share* res = array_shares[0]; 
				for (int i = 0; i < array_shares.size(); i++) {
					share* ind = put_cons32_gate(circ, i);
					share* sel = circ->PutEQGate(ind, index_wire);
					auto array_wire = array_shares[i];
					array_wire = add_conv_gate(share_type_cache->at(array_wire), circuit_type, array_wire, party);
					res = circ->PutMUXGate(array_wire, res, sel);
				}
				(*cache)[output_wires[0]] = {res};
				(*share_type_cache)[res] = circuit_type;
				break;
			}
			case STORE_: {
				Circuit* circ = get_circuit(circuit_type, party);
				assert(("Store circuit type not supported in arithmetic sharing", circuit_type != "a"));
				auto array_shares = cache->at(input_wires[0]);
				auto index_wire = cache->at(input_wires[1])[0];
				index_wire = add_conv_gate(share_type_cache->at(index_wire), circuit_type, index_wire, party);
				auto value_wire = cache->at(input_wires[2])[0];
				value_wire = add_conv_gate(share_type_cache->at(value_wire), circuit_type, value_wire, party);
				std::vector<share*> store_shares;
				for (int i = 0; i < array_shares.size(); i++) {
					share* ind = put_cons32_gate(circ, i);
					share* sel = circ->PutEQGate(ind, index_wire);
					auto array_wire = array_shares[i];
					array_wire = add_conv_gate(share_type_cache->at(array_wire), circuit_type, array_wire, party);
					array_wire = circ->PutMUXGate(value_wire, array_wire, sel);
					store_shares.push_back(array_wire);
					(*share_type_cache)[array_wire] = circuit_type;
				}
				(*cache)[output_wires[0]] = store_shares;
				break;
			}
			case FIELD_: {
				auto tuple_shares = cache->at(input_wires[0]);
				auto index = std::stoi(input_wires[1]);
				assert((index < tuple_shares.size()));
				auto field_share = tuple_shares[index];
				(*cache)[output_wires[0]] = {field_share};
				break;
			}
			case FIELD_VEC: {
				auto tuple_shares = cache->at(input_wires[0]);
				auto offset = std::stoi(input_wires[1]);
				auto len = std::stoi(input_wires[2]);
				assert((offset + len - 1 < tuple_shares.size()));
				std::vector<share*> field_shares;
				for (int i = 0; i < len; i++) {
					field_shares.push_back(tuple_shares[offset + i]);
				}
				(*cache)[output_wires[0]] = field_shares;
				break;
			}
			case TUPLE_: {
				std::vector<share*> tuple_shares;
				for (int i = 0; i < input_wires.size(); i++) {
					auto field_shares = cache->at(input_wires[i]);
					for (auto f: field_shares) {
						tuple_shares.push_back(f);
					}
				}
				(*cache)[output_wires[0]] = tuple_shares;
				break;
			}
			case IN_: {
				if (circuit_type == "y") {
					circuit_type = "b";
				}
				Circuit* circ = get_circuit(circuit_type, party);
				std::string var_name = input_wires[0];
			
				// rewire 
				if (rewire_inputs->size() > 0) {
					// add conversion gates
					share* rewire_share = rewire_inputs->front();
					std::string share_type_from = share_type_cache->at(rewire_share);
					(*cache)[output_wires[0]] = {rewire_share};
					rewire_inputs->pop_front();
					break;
				} else {
					uint32_t value = params->at(var_name);
					int vis = std::stoi(input_wires[1]);
					if (vis == (int) role) {
						result = circ->PutINGate(value, bitlen, role);
					} else if (vis == PUBLIC) {
						int len = std::stoi(input_wires[2]);
						if (len == 1) {
							result = put_cons1_gate(circ, value);
						} else {
							result = put_cons32_gate(circ, value);
						}
					} else {
						result = circ->PutDummyINGate(bitlen);
					} 
					(*share_type_cache)[result] = circuit_type;
				}

				for (auto o: output_wires) {
					(*cache)[o] = {result};
				}
				break;
			}
			case OUT_: {
				// rewire 
				Circuit* circ = get_circuit(circuit_type, party);
				if (rewire_outputs) {
					std::vector<share*> rewire_shares = cache->at(input_wires[0]);	
					for (auto wire: rewire_shares) {
						out->push_back(wire);
					}
				} else {
					std::vector<share*> wires = cache->at(input_wires[0]);
					for (auto wire: wires) {
						std::string share_type_from = share_type_cache->at(wire);
						wire = add_conv_gate(share_type_from, circuit_type, wire, party);
						result = circ->PutOUTGate(wire, ALL);		
						out->push_back(result);			
					}
				}
				break;
			}
			default: {
				throw std::invalid_argument("Unknown non binop: " + op);
			}
		}
	}
	
}


std::vector<share*> process_bytecode(
	std::string fn, 
	std::unordered_map<std::string, std::string>* bytecode_paths,
	std::unordered_map<std::string, std::vector<share*>>* cache,
	std::unordered_map<std::string, uint32_t>* const_cache,
	std::unordered_map<share*, std::string>* share_type_cache, 
	std::deque<share*> rewire_inputs,
	bool rewire_outputs,
	std::unordered_map<std::string, uint32_t>* params,
	std::unordered_map<std::string, std::string>* share_map,
	e_role role,
	uint32_t bitlen,
	ABYParty* party) {
	// std::cout << "LOG: processing function: " << fn << std::endl;

	auto path = bytecode_paths->at(fn);

	std::ifstream file(path);
	assert(("Bytecode file exists.", file.is_open()));
	if (!file.is_open()) throw std::runtime_error("Bytecode file doesn't exist -- "+path);
	std::vector<share*> out;

	std::string str;
	while (std::getline(file, str)) {
        std::vector<std::string> line = split_(str);
		// std::cout << "line: " << str << std::endl;
		if (line.size() < 4) continue;
		int num_inputs = std::stoi(line[0]);
		int num_outputs = std::stoi(line[1]);
		std::vector<std::string> input_wires = std::vector<std::string>(num_inputs);
		std::vector<std::string> output_wires = std::vector<std::string>(num_outputs);

		for (int i = 0; i < num_inputs; i++) {
			input_wires[i] = line[2+i];
		}
		for (int i = 0; i < num_outputs; i++) {
			output_wires[i] = line[2+num_inputs+i];
		}

		std::string op = line[2+num_inputs+num_outputs];
		std::string circuit_type;

		if (num_outputs) {
			circuit_type = share_map->at(output_wires[0]);
		} else {
			if (share_map->find(input_wires[0]) != share_map->end()) {
				circuit_type = share_map->at(input_wires[0]);
			} else {
				// This case is reached if the input is not used at all in the computation.
				if (op_hash(op) == IN_) {
					// if skipping an IN operation, remove a rewire
					if (rewire_inputs.size() > 0)
						rewire_inputs.pop_front();
				}
				continue;
			}
		}

		if (parse_call_op(op)) {
			auto fn =  parse_fn_name(op);
			// input and output wires are concatenated into a vector and then used for 
			// rewiring the input and output wires of the function
			std::deque<share*> rewire_inputs;

			for (auto i: input_wires) {
				auto wires = cache->at(i);
				rewire_inputs.insert(rewire_inputs.end(), wires.begin(), wires.end());
			}

			// recursively call process bytecode on function body
			std::vector<share*> out_shares = process_bytecode(fn, bytecode_paths, cache, const_cache, share_type_cache, rewire_inputs, true, params, share_map, role, bitlen, party);	

			assert(("Out_shares and output_wires are the same size", out_shares.size() == output_wires.size()));
			(*cache)[output_wires[0]] = out_shares;
		} else {
			process_instruction(circuit_type, cache, const_cache, share_type_cache, &rewire_inputs, rewire_outputs, params, input_wires, output_wires, &out, op, role, bitlen, party);
			assert(("Len of output_wires should be at most 1", output_wires.size() <= 1));
		}
	}

	return out;
}

void process_const(
	std::unordered_map<std::string, std::vector<share*>>* cache,
	std::unordered_map<std::string, uint32_t>* const_cache,
	std::unordered_map<share*, std::string>* share_type_cache, 
	std::string const_path, 
	std::unordered_map<std::string, std::string>* share_map,
	e_role role,
	uint32_t bitlen,
	ABYParty* party) {
	// std::cout << "LOG: processing const" << std::endl;
	std::ifstream file(const_path);
	if (!file.is_open()) {
		return;
	}

	std::vector<share*> out;

	std::string str;
	while (std::getline(file, str)) {
        std::vector<std::string> line = split_(str);
		if (line.size() < 4) continue;
		int num_inputs = std::stoi(line[0]);
		int num_outputs = std::stoi(line[1]);
		std::vector<std::string> input_wires = std::vector<std::string>(num_inputs);
		std::vector<std::string> output_wires = std::vector<std::string>(num_outputs);

		for (int i = 0; i < num_inputs; i++) {
			input_wires[i] = line[2+i];
		}
		for (int i = 0; i < num_outputs; i++) {
			output_wires[i] = line[2+num_inputs+i];
		}

		std::string op = line[2+num_inputs+num_outputs];
		std::string circuit_type;
		if (num_outputs) {
			circuit_type = share_map->at(output_wires[0]);
		} else {
			circuit_type = share_map->at(input_wires[0]);
		}

		process_instruction(circuit_type, cache, const_cache, share_type_cache, {}, {}, {}, input_wires, output_wires, &out, op, role, bitlen, party);
		assert(("Len of output_wires should be at most 1", output_wires.size() <= 1));
	}
}

double test_aby_test_circuit(
	std::unordered_map<std::string, std::string>* bytecode_paths, 
	std::string const_path,
	std::unordered_map<std::string, uint32_t>* params, 
	std::unordered_map<std::string, std::string>* share_map,
	e_role role, 
	const std::string& address, 
	uint16_t port, 
	seclvl seclvl, 
	uint32_t bitlen,
	uint32_t nthreads, 
	e_mt_gen_alg mt_alg, 
	e_sharing sharing) {

	// setup
	ABYParty* party = new ABYParty(role, address, port, seclvl, bitlen, nthreads, mt_alg);
	output_queue out_q;

	// share cache
	std::unordered_map<std::string, std::vector<share*>>* cache = new std::unordered_map<std::string, std::vector<share*>>();
	std::unordered_map<std::string, uint32_t>* const_cache = new std::unordered_map<std::string, uint32_t>();
	std::unordered_map<share*, std::string>* share_type_cache = new std::unordered_map<share*, std::string>();

	// process consts 
	process_const(cache, const_cache, share_type_cache, const_path, share_map, role, bitlen, party);

	// process bytecode
	vector<share*> out_shares = process_bytecode("main", bytecode_paths, cache, const_cache, share_type_cache, {}, {}, params, share_map, role, bitlen, party);	

	// multiple outputs
	for (auto s: out_shares) {
		add_to_output_queue(out_q, s, role, std::cout);
	}

	// add timing code
	high_resolution_clock::time_point start_exec_time = high_resolution_clock::now();
	party->ExecCircuit();
	high_resolution_clock::time_point end_exec_time = high_resolution_clock::now();
	duration<double> exec_time = duration_cast<duration<double>>(end_exec_time - start_exec_time);
	std::cout << "LOG: " << (role == SERVER ? "Server exec time: " : "Client exec time: ") << exec_time.count() << std::endl;

	flush_output_queue(out_q, role, bitlen);
	delete cache;
	delete party;
	return exec_time.count();
}
