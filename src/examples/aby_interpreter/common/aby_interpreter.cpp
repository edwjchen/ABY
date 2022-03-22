#include "aby_interpreter.h"
#include "../../../abycore/circuit/booleancircuits.h"
#include "../../../abycore/circuit/arithmeticcircuits.h"
#include "../../../abycore/circuit/circuit.h"
#include "../../../abycore/sharing/sharing.h"
#include "ezpc.h"

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
	DIV_,
	IN_,
	OUT_,
};

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
	if (o == "IN") return IN_;
	if (o == "OUT") return OUT_;
    throw std::invalid_argument("Unknown operator: "+o);
}

bool is_bin_op(op o) {
	return o == ADD_ || o == SUB_ || o == MUL_ || o == EQ_ || o == GT_ || o == LT_ || o == GE_ || o == LE_ || o == REM_ || o == DIV_ || o == AND_ || o == OR_ || o == XOR_;
}

std::vector<std::string> split_(std::string str, char delimiter) {
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

// void process_input(
// 	std::unordered_map<std::string, share*>* cache,
// 	std::unordered_map<std::string, uint32_t>* params,
// 	std::unordered_map<std::string, std::string>* share_map,
// 	e_role role,
// 	uint32_t bitlen,
// 	ABYParty* party) {
// 	for (auto p: *params) {
// 		std::string param_name = p.first;
// 		uint32_t param_value = (p.second);
// 		std::string circuit_type = get(share_map, std::to_string(param_index));
// 		Circuit* circ = get_circuit(circuit_type, party);
// 		share* param_share;
// 		if (param_role == role_str) {
// 			param_share = circ->PutINGate(param_value, bitlen, role);
// 		} else {
// 			param_share = circ->PutDummyINGate(bitlen);
// 		}
// 		(*cache)[std::to_string(param_index)] = param_share;
// 	}
// }


share* process_instruction(
	std::string circuit_type,
	std::unordered_map<std::string, share*>* cache, 
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
			case IN_: {
				std::string var_name = input_wires[0];
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
				break;
			}
			case OUT_: {
				share* wire = cache->at(input_wires[0]);
				result = circ->PutOUTGate(wire, ALL);
				break;
			}
			default: {
				throw std::invalid_argument("Unknown non binop: " + op);
			}
		}
	}
	
	return result;
}

share* process_bytecode(
	std::string bytecode_path,
	std::unordered_map<std::string, share*>* cache,
	std::unordered_map<std::string, uint32_t>* params,
	std::unordered_map<std::string, std::string>* share_map,
	e_role role,
	uint32_t bitlen,
	ABYParty* party) {
	std::ifstream file(bytecode_path);
	assert(("Bytecode file exists.", file.is_open()));
	if (!file.is_open()) throw std::runtime_error("Bytecode file doesn't exist -- "+bytecode_path);
	std::string str;
	share* last_instr;
	Circuit* circ;
	while (std::getline(file, str)) {
        std::vector<std::string> line = split_(str, ' ');
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
		last_instr = process_instruction(circuit_type, cache, params, share_map, input_wires, output_wires, op, role, bitlen, party);
	
		for (auto o: output_wires) {
			(*cache)[o] = last_instr;
		}
	}
	if (last_instr == NULL) throw std::runtime_error("Return value is null.");
	return last_instr;
}

int32_t test_aby_test_circuit(
	std::string bytecode_path, 
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
	share* out_share = process_bytecode(bytecode_path, cache, params, share_map, role, bitlen, party);	

	add_to_output_queue(out_q, out_share, role, std::cout);
	party->ExecCircuit();
	flush_output_queue(out_q, role, bitlen);
	delete cache;
	delete party;
	return 0;
}
