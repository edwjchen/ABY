#include <ENCRYPTO_utils/crypto/crypto.h>
#include <ENCRYPTO_utils/parse_options.h>
#include "../../abycore/aby/abyparty.h"
#include "common/aby_interpreter.h"

#include "argparse.hpp"

using namespace std::chrono;

enum mode {
    mpc
};

mode hash_mode(std::string m) {
    if (m == "mpc") return mpc;
    throw std::invalid_argument("Unknown mode: "+m);
}

std::vector<std::string> split(std::string str, char delimiter) {
    std::vector<std::string> result;
    std::istringstream ss(str);
    std::string word; 
    while (ss >> word) {
        result.push_back(word);
    }
    return result;
}

std::unordered_map<std::string, std::string> parse_share_map_file(std::string share_map_path) {
    std::ifstream file(share_map_path);
    assert(("Mapping file exists.", file.is_open()));
    if (!file.is_open()) throw std::runtime_error("Share map file doesn't exist. -- "+share_map_path);

    std::unordered_map<std::string, std::string> share_map;
    std::string str;
    bool role_flag = false;
    while (std::getline(file, str)) {
        std::vector<std::string> line = split(str, ' ');
        if (line.size() == 0) continue;
        if (line.size() == 2) {
            share_map[line[0]] = line[1];
        }
    }
    return share_map;
}

std::unordered_map<std::string, uint32_t> parse_mpc_inputs(std::string test_path) {
    std::ifstream file(test_path);
    assert(("Test file exists.", file.is_open()));
    if (!file.is_open()) throw std::runtime_error("Test file doesn't exist.");
    std::unordered_map<std::string, uint32_t> map;
    std::string str;
    uint32_t num_params = 0;
    while (std::getline(file, str)) {
        std::vector<std::string> line = split(str, ' ');
        std::string key_ = line[0];
        if (key_ == "res") continue;

        if (line.size() == 2) {
            std::string key = line[0];
            uint32_t value = (uint32_t)std::stoi(line[1]);
            map[key] = value;
        } else if (line.size() > 2) {
            for (int i = 1; i < line.size(); i++) {
                std::string key = line[0] + "_" + std::to_string(i-1);
                uint32_t value = (uint32_t)std::stoi(line[i]);
                map[key] = value;
            }
        }
    }
    return map;
}

int main(int argc, char** argv) {
    // add timing code
	high_resolution_clock::time_point start_exec_time = high_resolution_clock::now();

	e_role role; 
	uint32_t bitlen = 32, nvals = 31, secparam = 128, nthreads = 1;
	uint16_t port = 7766;
	std::string address = "127.0.0.1";
	int32_t test_op = -1;
	e_mt_gen_alg mt_alg = MT_OT;
	seclvl seclvl = get_sec_lvl(secparam);

    argparse::ArgumentParser program("aby_interpreter");
    program.add_argument("-M", "--mode").required().help("Mode for parsing test inputs");
    program.add_argument("-R", "--role").required().help("Role: <Server:0 / Client:1>").scan<'i', int>();;
    program.add_argument("-b", "--bytecode").required().help("Bytecode");
    program.add_argument("-t", "--test").required().help("Test inputs");
    program.add_argument("-s", "--share_map").required().help("Map of share id to circuit type");
    program.parse_args(argc, argv);    // Example: ./main --color orange
    
    std::string m, bytecode_path, test_path, share_map_path, param_map_path;
    m = program.get<std::string>("--mode");  
    role = !program.get<int>("--role") ? SERVER : CLIENT;
    bytecode_path = program.get<std::string>("--bytecode");
    test_path = program.get<std::string>("--test");
    share_map_path = program.get<std::string>("--share_map");

	std::unordered_map<std::string, uint32_t> params;
    std::unordered_map<std::string, std::string> share_map;

	switch(hash_mode(m)) {
        case mpc: {
            params = parse_mpc_inputs(test_path);
            share_map = parse_share_map_file(share_map_path);
        }
        break;
    }
	test_aby_test_circuit(bytecode_path, &params, &share_map, role, address, port, seclvl, 32,
			nthreads, mt_alg, S_BOOL);

    // add timing code
    high_resolution_clock::time_point end_exec_time = high_resolution_clock::now();
	duration<double> exec_time = duration_cast<duration<double>>(end_exec_time - start_exec_time);
	std::cout << "LOG: " << (role == SERVER ? "Server total time: " : "Client total time: ") << exec_time.count() << std::endl;

	return 0;
}

