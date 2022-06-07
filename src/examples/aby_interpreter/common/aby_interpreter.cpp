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
	CONS_bv,
	CONS_bool,
	MUX_, 
	NOT_,
	SHL_,
	LSHR_,
	DIV_,
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
	if (o == "CONS_bv") return CONS_bv;
	if (o == "CONS_bool") return CONS_bool;
	if (o == "MUX") return MUX_;
	if (o == "NOT") return NOT_;
	if (o == "DIV") return DIV_;
	if (o == "SHL") return SHL_;
	if (o == "LSHR") return LSHR_;
	if (o == "IN") return IN_;
	if (o == "OUT") return OUT_;
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

share* process_instruction(
	std::string circuit_type,
	std::unordered_map<std::string, share*>* cache, 
	std::deque<std::string>* rewire,
	std::unordered_map<std::string, uint32_t>* params,
	std::unordered_map<std::string, std::string>* share_map,
	std::vector<std::string> input_wires, 
	std::vector<std::string> output_wires, 
	std::string op,
	e_role role, 
	uint32_t bitlen,
	ABYParty* party) {
	std::vector<Sharing*>& sharings = party->GetSharings();
	Circuit* bcirc = sharings[S_BOOL]->GetCircuitBuildRoutine();

	Circuit* circ = get_circuit(circuit_type, party);
	share* result;

	if (is_bin_op(op_hash(op))) {		
		share* wire1 = cache->at(input_wires[0]);
		share* wire2 = cache->at(input_wires[1]);

		// add conversion gates
		std::string share_type_1 = share_map->at(input_wires[0]);
		std::string share_type_2 = share_map->at(input_wires[1]);

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
				result = circ->PutMULGate(wire1, wire2);
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
	} else {
		switch(op_hash(op)) {
			case CONS_bv: {
				int value = std::stoi(input_wires[0]);
				if (circuit_type == "y") {
					result = put_cons32_gate(bcirc, value);
					result = add_conv_gate("b", circuit_type, result, party);
				} else {
					result = put_cons32_gate(circ, value);
				}
				break;
			}
			case CONS_bool: {
				int value = std::stoi(input_wires[0]);
				if (circuit_type == "y") {
					result = put_cons1_gate(bcirc, value);
					result = add_conv_gate("b", circuit_type, result, party);
				} else {
					result = put_cons1_gate(circ, value);
				}
				break;
			}
			case MUX_: {
				share* sel = cache->at(input_wires[0]);
				share* wire1 = cache->at(input_wires[1]);
				share* wire2 = cache->at(input_wires[2]);

				// add conversion gates
				std::string share_type_sel = share_map->at(input_wires[0]);
				std::string share_type_1 = share_map->at(input_wires[1]);
				std::string share_type_2 = share_map->at(input_wires[2]);

				sel = add_conv_gate(share_type_sel, circuit_type, sel, party);
				wire1 = add_conv_gate(share_type_1, circuit_type, wire1, party);
				wire2 = add_conv_gate(share_type_2, circuit_type, wire2, party);

				result = circ->PutMUXGate(wire1, wire2, sel);
				break;
			}
			case NOT_: {
				share* wire = cache->at(input_wires[0]);

				// add conversion gates
				std::string share_type = share_map->at(input_wires[0]);
				wire = add_conv_gate(share_type, circuit_type, wire, party);
				
				result = ((BooleanCircuit *)circ)->PutINVGate(wire);
				break;
			}
			case SHL_: {
				share* wire = cache->at(input_wires[0]);
				int value = std::stoi(input_wires[1]);
				result = left_shift(circ, wire, value);
				break;
			} 
			case LSHR_: {
				share* wire = cache->at(input_wires[0]);
				int value = std::stoi(input_wires[1]);
				result = logical_right_shift(circ, wire, value);
				break;
			} 
			case IN_: {
				std::string var_name = input_wires[0];

				// rewire 
				if (rewire->size() > 0) {
					std::cout << "in: " << output_wires[0] << " to " << rewire->at(0) << std::endl;
					std::cout << "in output wire: " << output_wires[0] << std::endl;
					share* rewire_share = cache->at(rewire->at(0));
					cache->insert(std::pair<std::string, share*>(output_wires[0], cache->at(rewire->at(0))));
					rewire->pop_front();
					result = rewire_share;
				} else {
					uint32_t value = params->at(var_name);
					int vis = std::stoi(input_wires[1]);
					if (vis == (int) role) {
						result = circ->PutINGate(value, bitlen, role);
					} else if (vis == PUBLIC) {
						int len = std::stoi(input_wires[2]);
						if (len == 1) {
							if (circuit_type == "y") {
								result = put_cons1_gate(bcirc, value);
								result = add_conv_gate("b", circuit_type, result, party);
							} else {
								result = put_cons1_gate(circ, value);
							}
						} else {
							if (circuit_type == "y") {
								result = put_cons32_gate(bcirc, value);
								result = add_conv_gate("b", circuit_type, result, party);
							} else {
								result = put_cons32_gate(circ, value);
							}
						}
					} else {
						result = circ->PutDummyINGate(bitlen);
					} 
				}
				break;
			}
			case OUT_: {
				// rewire 
				if (rewire->size() > 0) {
					std::cout << "out: " << rewire->at(0) << " to " << input_wires[0] << std::endl;
					share* rewire_share = cache->at(input_wires[0]);
					cache->insert(std::pair<std::string, share*>(rewire->at(0), rewire_share));
					rewire->pop_front();
					result = rewire_share;
				} else {
					share* wire = cache->at(input_wires[0]);
					result = circ->PutOUTGate(wire, ALL);
				}
				break;
			}
			default: {
				throw std::invalid_argument("Unknown non binop: " + op);
			}
		}
	}
	
	return result;
}


std::vector<share*> process_bytecode(
	std::string fn, 
	std::unordered_map<std::string, std::string>* bytecode_paths,
	std::unordered_map<std::string, share*>* cache,
	std::deque<std::string> rewire,
	std::unordered_map<std::string, uint32_t>* params,
	std::unordered_map<std::string, std::string>* share_map,
	e_role role,
	uint32_t bitlen,
	ABYParty* party) {

	auto path = bytecode_paths->at(fn);

	std::ifstream file(path);
	assert(("Bytecode file exists.", file.is_open()));
	if (!file.is_open()) throw std::runtime_error("Bytecode file doesn't exist -- "+path);
	std::string str;
	share* last_instr;
	Circuit* circ;

	std::vector<share*> out;
	while (std::getline(file, str)) {
		std::cout << str << std::endl;
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

		if (parse_call_op(op)) {
			auto fn =  parse_fn_name(op);

			// input and output wires are concatenated into a vector and then used for 
			// rewiring the input and output wires of the function
			std::deque<std::string> fn_rewire;
			fn_rewire.insert(fn_rewire.end(), input_wires.begin(), input_wires.end());
			fn_rewire.insert(fn_rewire.end(), output_wires.begin(), output_wires.end());

			for (auto r: fn_rewire) {
				std::cout << "rewire: " << r << std::endl;
			}

			// recursively call process bytecode on function body
			std::vector<share*> out_shares = process_bytecode(fn, bytecode_paths, cache, fn_rewire, params, share_map, role, bitlen, party);	

			assert(("Out_shares and output_wires are the same size", out_shares.size() == output_wires.size()));
			for (int i = 0; i < out_shares.size(); i++) {
				(*cache)[output_wires[i]] = out_shares[i];
			}
		} else {
			last_instr = process_instruction(circuit_type, cache, &rewire, params, share_map, input_wires, output_wires, op, role, bitlen, party);
			assert(("Len of output_wires should be at most 1", output_wires.size() <= 1));
			for (auto o: output_wires) {
				(*cache)[o] = last_instr;
			}
		}

		if (op_hash(op) == OUT_) {
			out.push_back(last_instr);
		} 
	}

	return out;
}

double test_aby_test_circuit(
	std::unordered_map<std::string, std::string>* bytecode_paths, 
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
	std::unordered_map<std::string, share*>* cache = new std::unordered_map<std::string, share*>();

	// process bytecode
	vector<share*> out_shares = process_bytecode("main", bytecode_paths, cache, {}, params, share_map, role, bitlen, party);	

	// multiple outputs
	for (auto s: out_shares) {
		std::cout << "adding to out" << std::endl;
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
