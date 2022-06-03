/**
 \file 		millionaire_prob.cpp
 \author 	sreeram.sadasivam@cased.de
 \copyright	ABY - A Framework for Efficient Mixed-protocol Secure Two-party Computation
			Copyright (C) 2019 Engineering Cryptographic Protocols Group, TU Darmstadt
			This program is free software: you can redistribute it and/or modify
            it under the terms of the GNU Lesser General Public License as published
            by the Free Software Foundation, either version 3 of the License, or
            (at your option) any later version.
            ABY is distributed in the hope that it will be useful,
            but WITHOUT ANY WARRANTY; without even the implied warranty of
            MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
            GNU Lesser General Public License for more details.
            You should have received a copy of the GNU Lesser General Public License
            along with this program. If not, see <http://www.gnu.org/licenses/>.
 \brief		Implementation of the millionaire problem using ABY Framework.
 */

#include "simd.h"
#include "../../../abycore/circuit/booleancircuits.h"
#include "../../../abycore/sharing/sharing.h"

int32_t test_simd_circuit(e_role role, const std::string& address, uint16_t port, seclvl seclvl,
		uint32_t bitlen, uint32_t nthreads, e_mt_gen_alg mt_alg, e_sharing sharing) {

	/**
		Step 1: Create the ABYParty object which defines the basis of all the
		 	 	operations which are happening.	Operations performed are on the
		 	 	basis of the role played by this object.
	*/
	ABYParty* party = new ABYParty(role, address, port, seclvl, bitlen, nthreads,
			mt_alg);


	/**
		Step 2: Get to know all the sharing types available in the program.
	*/

	std::vector<Sharing*>& sharings = party->GetSharings();

	/**
		Step 3: Create the circuit object on the basis of the sharing type
				being inputed.
	*/
	Circuit* acirc = sharings[S_ARITH]->GetCircuitBuildRoutine();
	Circuit* bcirc = sharings[S_BOOL]->GetCircuitBuildRoutine();
	Circuit* ycirc = sharings[S_YAO]->GetCircuitBuildRoutine();
	Circuit* circ = sharings[sharing]->GetCircuitBuildRoutine();


	/**
		Step 4: Creating the share objects - s_alice_money, s_bob_money which
				is used as input to the computation function. Also s_out
				which stores the output.
	*/

	// //// test 1
	// uint32_t *avec, *bvec, *cvec, tmpbitlen, tmpnvals;
	// int nvals = 10;
	// avec = (uint32_t*) malloc(nvals * sizeof(uint32_t));
	// bvec = (uint32_t*) malloc(nvals * sizeof(uint32_t));
	// cvec = (uint32_t*) malloc(nvals * sizeof(uint32_t));
	// for (int i = 0; i < nvals; i++) {
	// 	avec[i] = i+10;
	// 	bvec[i] = i+10;
	// }

	// share* shra = circ->PutSIMDINGate(nvals, avec, bitlen, SERVER);
	// share* shrb = circ->PutSIMDINGate(nvals, bvec, bitlen, CLIENT);

	// share* shrres = circ->PutADDGate(shra, shrb);

	// share* shr_split = circ->PutSplitterGate(shra);

	// share* s_out = circ->PutOUTGate(shr_split, ALL);

	// party->ExecCircuit();

	// s_out->get_clear_value_vec(&cvec, &tmpbitlen, &tmpnvals);

	
	// for (int i = 0; i < nvals; i++) {
	// 	std::cout << "a[" << i << "] = " << avec[i] << std::endl;
	// }

	// for (int i = 0; i < nvals; i++) {
	// 	std::cout << "b[" << i << "] = " << bvec[i] << std::endl;
	// }

	// for (int i = 0; i < tmpnvals; i++) {
	// 	std::cout << "output[" << i << "] = " << cvec[i] << std::endl;
	// }



	// test 2
	uint32_t *avec, *bvec, *cvec, tmpbitlen, tmpnvals;
	int nvals = 2;
	avec = (uint32_t*) malloc(nvals * sizeof(uint32_t));
	bvec = (uint32_t*) malloc(nvals * sizeof(uint32_t));
	cvec = (uint32_t*) malloc(nvals * sizeof(uint32_t));
	for (int i = 0; i < nvals; i++) {
		avec[i] = i+3;
		bvec[i] = i+3;
	}

	share* shra1 = acirc->PutINGate(avec[0], bitlen, SERVER);
	share* shra2 = acirc->PutINGate(avec[1], bitlen, SERVER);	

	share* c_shra1 = acirc->PutCombinerGate(shra1, shra2);

	share* s_out_1 = acirc->PutOUTGate(c_shra1, ALL);

	share* s_split = acirc->PutSplitterGate(c_shra1);

	share* s_out_2 = acirc->PutOUTGate(s_split, ALL);

	// share* c_shra1 = circ->PutCombinerGate(shra1);
	// share* c_shra2 = circ->PutCombinerGate(shra2);
	// share* shra = circ->PutCombinerGate(shra1, shra2);

	// share* shrb1 = circ->PutINGate(bvec[0], bitlen, SERVER);
	// share* shrb2 = circ->PutINGate(bvec[1], bitlen, SERVER);

	// share* c_shrb1 = circ->PutCombinerGate(shrb1);
	// share* c_shrb2 = circ->PutCombinerGate(shrb2);
	// share* shrb = circ->PutCombinerGate(c_shrb1, c_shrb2);

	// share* shrres = circ->PutADDGate(shra, shrb);

	// share* s_split_1 = circ->PutSplitterGate(shrres);
	// share* s_split_2 = circ->PutSplitterGate(shrres);
	// share* s_split_3 = circ->PutSplitterGate(shrres);

	party->ExecCircuit();

	s_out_1->get_clear_value_vec(&cvec, &tmpbitlen, &tmpnvals);
	for (int i = 0; i < tmpnvals; i++) {
		std::cout << "output1[" << i << "] = " << cvec[i] << std::endl;
	}

	s_out_2->get_clear_value_vec(&cvec, &tmpbitlen, &tmpnvals);
	for (int i = 0; i < tmpnvals; i++) {
		std::cout << "output2[" << i << "] = " << cvec[i] << std::endl;
	}



	// share* shrres = circ->PutADDGate(shra, shrb);
	// share* s_split = circ->PutSplitterGate(shra);
	// share* s_out = circ->PutOUTGate(s_split, ALL);

	// // share* shra3 = circ->PutINGate(avec[2], bitlen, SERVER);

	// share* shrb1 = circ->PutINGate(bvec[0], bitlen, CLIENT);
	// share* shrb2 = circ->PutINGate(bvec[1], bitlen, CLIENT);
	// // share* shrb3 = circ->PutINGate(bvec[2], bitlen, CLIENT);

	// share* c_shra1 = circ->PutCombinerGate(shra1);
	// share* c_shra2 = circ->PutCombinerGate(shra2);
	// share* shra = circ->PutCombinerGate(c_shra1, c_shra2);
	
	// share* c_shrb1 = circ->PutCombinerGate(shrb1);
	// share* c_shrb2 = circ->PutCombinerGate(shrb2);
	// share* shrb = circ->PutCombinerGate(c_shrb1, c_shrb2);

	// share* shrres = circ->PutADDGate(shra, shrb);
	// share* s_out = circ->PutOUTGate(shrres, ALL);
	// party->ExecCircuit();

	// s_out->get_clear_value_vec(&cvec, &tmpbitlen, &tmpnvals);
	// std::cout << "tmp bitlen: " << tmpbitlen << std::endl;
	// std::cout << "tmp nvals: " << tmpnvals << std::endl;
	
	// for (int i = 0; i < nvals; i++) {
	// 	std::cout << "a[" << i << "] = " << avec[i] << std::endl;
	// }

	// for (int i = 0; i < nvals; i++) {
	// 	std::cout << "b[" << i << "] = " << bvec[i] << std::endl;
	// }

	// for (int i = 0; i < tmpnvals; i++) {
	// 	std::cout << "output[" << i << "] = " << cvec[i] << std::endl;
	// }


	delete party;
	return 0;
}