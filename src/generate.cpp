/* 
 *      Copyright (c) 2017 Robert Shaw
 *		This file is a part of Libecpint.
 *
 *      Permission is hereby granted, free of charge, to any person obtaining
 *      a copy of this software and associated documentation files (the
 *      "Software"), to deal in the Software without restriction, including
 *      without limitation the rights to use, copy, modify, merge, publish,
 *      distribute, sublicense, and/or sell copies of the Software, and to
 *      permit persons to whom the Software is furnished to do so, subject to
 *      the following conditions:
 *
 *      The above copyright notice and this permission notice shall be
 *      included in all copies or substantial portions of the Software.
 *
 *      THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *      EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *      MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *      NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *      LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *      OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *      WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <iostream>
#include <fstream>
#include <string>
#include "angular.hpp"
#include "mathutil.hpp"
#include "multiarr.hpp"
#include "config.hpp"
#include <cmath>

/**
  * \file generate.cpp
  * \brief ECP integral code generator
  */ 

const int maxL = LIBECPINT_MAX_L;
const int maxUnrol = LIBECPINT_MAX_UNROL; 
const int maxN = 15; 
const int CAX1 = maxN + 1;
const int CAX2 = CAX1 * (maxL + 1);
const int CAX3 = CAX2 * (maxL + 1);

namespace libecpint {

	/** 
	  * \struct SumTerm
	  * \brief  Stores details of terms in ECP integral expansion
	  */
	struct SumTerm {

		Pair SA; ///< (l, m) for spherical harmonic on shellA
		Pair SB; ///< (l, m) for spherical harmonic on shellB
		Triple radial; ///< (N, l1, l2) radial integral required
		Quintuple CA; ///< (0, cartesian index, x, y, z) for binomial coefficient on shellA
		Quintuple CB; ///< (0, cartesian index, x, y, z) for binomial coefficient on shellB
		double ang;   ///< Value of product of angular integrals
		int mu; ///< Value of mu, where the ECP ang. momentum is lam, and mu can range from -lam .. lam
		int na; ///< Index of cartesian function on shellA, in alpha order
		int nb; ///< Index of cartesian function on shellB, in alpha order
	
		/**
		  * orders SumTerm's by mu, then by radial indices, then by angular integral value
		  * @param s - term to compare with
		  * @return true if this term is less than the given term
		  */ 
		bool operator<(const SumTerm& s) const {
			bool result = (mu < s.mu);
			if (mu == s.mu) {
				result = radial < s.radial; 
				if (radial == s.radial)
					result = ang < s.ang; 
			}
			return result; 
		}
	
		bool operator<=(const SumTerm& s) const {
			return (*this == s) || (*this < s); 
		}
	
		/// @return true if the mu and radial indices are equal
		bool operator==(const SumTerm& s) const {
			return (mu == s.mu) && (radial == s.radial); 
		}
	
		/// @return the compressed index of CA
		int ca_index() const {
			return std::get<1>(CA) + std::get<2>(CA)*CAX1 
				+ std::get<3>(CA)*CAX2 + std::get<4>(CA)*CAX3; 
		}
	
		/// @return the compressed index of CB
		int cb_index() const {
			return std::get<1>(CB) + std::get<2>(CB)*CAX1 
				+ std::get<3>(CB)*CAX2 + std::get<4>(CB)*CAX3; 
		}
	
		/// Converts term to string with compressed indices
		std::string to_string(bool full = true) {
			std::stringstream ss;
		
			if (full) 
				ss << "\tvalue" << mu << " += " << ang << " * CA[" << ca_index()
					<< "] * CB[" << cb_index() << "] * radials" << radial << " * SA"
						<< SA << " * SB" << SB << ";"; 
			else
				ss << "\ttmp += " << "CA[" << ca_index() << "] * CB[" << cb_index() << "] * SA"
					<< SA << " * SB" << SB << ";"; 
		 
			return ss.str(); 
		}
	
		/**
		  * Compares two SumTerm objects
		  * @param s - the SumTerm to compare with
		  * @return a tuple of equalities (0 = false, 1 = true) in the order {mu, radial, SA, SB, ang, CA, CB}
		  */ 
		Heptuple compare(const SumTerm& s) const {
			int f1 = mu == s.mu ? 1 : 0;
			int f2 = radial == s.radial ? 1 : 0;
			int f3 = SA == s.SA ? 1 : 0;
			int f4 = SB == s.SB ? 1 : 0; 
			int f5 = fabs(ang - s.ang) < 1e-10 ? 1 : 0;
			int f6 = CA == s.CA ? 1 : 0;
			int f7 = CB == s.CB ? 1 : 0; 
		
			return std::make_tuple(f1, f2, f3, f4, f5, f6, f7);  
		}
	
		/// Prints out a SumTerm without compressing the indices - currently preferred 
		friend std::ostream& operator<<(std::ostream& os, const SumTerm& s); 

	};

	std::ostream& operator<<(std::ostream& os, const SumTerm& s) {
		os << "values(" << s.na << ", " << s.nb << ", " << s.mu << ") += "
			<< s.ang << " * CA" << s.CA << " * CB" << s.CB
				<< " * radials" << s.radial << " * SA" << s.SA << " * SB" << s.SB << ";";
		return os;
	}

 	/// Redundant at the moment
	void print_lists(std::vector<SumTerm>& terms) {
		std::sort(terms.begin(), terms.end()); 
		Heptuple zero{0, 0, 0, 0, 0, 0, 0}; 
		int i = 0; 
		int value = 0; 
	
		std::cout << "\tdouble r = radial" << terms[0].radial << ";" << std::endl;
		std::cout << "\tdouble a = " << terms[0].ang << ";" << std::endl; 
	
		while (i < terms.size() - 1) {
			SumTerm& t1 = terms[i]; 
		
			std::cout << t1.to_string(false) << std::endl; 
		
			Heptuple comparison = t1.compare(terms[i+1]); 
		
			t1 = terms[i+1]; 
			int f0 = std::get<0>(comparison);
			int f1 = std::get<1>(comparison);
			int f4 = std::get<4>(comparison); 
		
			if (f1 + f4 < 2)
				std::cout << "\tvalues[" << value << "] += r * a * tmp;" << std::endl << std::endl << "\ttmp = 0.0;" << std::endl; 
			if (f0 == 0) 
				value++; 
			if (f1 == 0) 
				std::cout << "\tr = radial" << t1.radial << ";" << std::endl;  
			if (f4 == 0)
				std::cout << "\ta = " << t1.ang << ";" << std::endl; 
			i++; 
		}
		std::cout << terms[i].to_string(false) << std::endl;
		std::cout << "\tvalue" << value << " += r * a * tmp;" << std::endl << std::endl;  
	
	}

	/** 
	  * For a given ECP integral, Q(LA, LB, lam), generates the integral code.
	  * This determines which radial integrals are necessary, based on angular integral screening.
	  * If LA, LB <= maxUnrol and (LA + LB + lam) <= 3*maxUnrol, then it unrols the angular integration as well.
	  * 
	  * @param LA - the shellA angular momentum
	  * @param LB - the shellB angular momentum
	  * @param lam - the ECP angular momentum
	  * @param angInts - the angular integrals
	  */
	void generate_lists(int LA, int LB, int lam, AngularIntegral& angInts) { 
		
		// Create the code file
		std::string ofname = "generated/Q" + std::to_string(LA) + std::to_string(LB) + std::to_string(lam) + ".cpp"; 
		std::ofstream outfile(ofname); 
		
		if (!outfile.is_open())
			std::cerr << "Problems writing to file!" << std::endl; 
		else {
			
			std::cout << "Generating Q(" << LA << ", " << LB << ", " << lam << ")... " << std::flush; 
			
			// Top matter
			outfile << "// Generated as part of Libecpint, Copyright 2017 Robert A Shaw" << std::endl; 
			outfile << "#include \"qgen.hpp\"" << std::endl; 
			outfile << "namespace libecpint {" << std::endl << "namespace qgen {" << std::endl;
			outfile << "void Q" << LA << "_" << LB << "_" << lam << "(ECP& U, GaussianShell& shellA, GaussianShell& shellB, "
				<< "FiveIndex<double> &CA, FiveIndex<double> &CB, TwoIndex<double> &SA, TwoIndex<double> &SB, double Am, double Bm, "
					<< "RadialIntegral &radint, AngularIntegral& angint, ThreeIndex<double> &values) {" << std::endl << std::endl; 
			
			double prefac = 16.0 * M_PI * M_PI;
			int na = 0; 
			int z1, z2;
			double ang_alpha, ang_beta, ang; 
			
			// Do we need to unrol the angular integrals too? 
			bool unrolling = LA <= maxUnrol && LB <= maxUnrol && (LA + LB + lam) <= 3*maxUnrol;
			
			// Store the terms and radials if unrolling, just radial indices if not
			std::vector<SumTerm> terms; 
			std::vector<Triple> radial_triples; 
			
			// Loop over cartesian functions in alpha order
			for (int x1 = LA; x1 >= 0; x1--) {
				for (int r1 = LA-x1; r1 >= 0; r1--) {
					z1 = LA - x1 - r1; 
			
					int nb = 0;
					for (int x2 = LB; x2 >= 0; x2--) {
						for (int y2 = LB - x2; y2 >= 0; y2--) {
							z2 = LB - x2 - y2; 
					
							for (int alpha_x = 0; alpha_x <= x1; alpha_x++) {
								for (int alpha_y = 0; alpha_y <= r1; alpha_y++) {
									for (int alpha_z = 0; alpha_z <= z1; alpha_z++) {
										int alpha = alpha_x + alpha_y + alpha_z; 
								
										for (int beta_x = 0; beta_x <= x2; beta_x++) {
											for (int beta_y = 0; beta_y <= y2; beta_y++) {
												for (int beta_z = 0; beta_z <= z2; beta_z++) {
													int beta = beta_x + beta_y + beta_z; 
													int N = alpha + beta; 				
											
													for (int lam1 = 0; lam1 <= lam + alpha; lam1++) {
														int lam2start = (lam1 + N) % 2; 
														for (int lam2 = lam2start; lam2 <= lam + beta; lam2+=2) {
													
															for (int mu1 = -lam1; mu1 <= lam1; mu1++) {
																for (int mu2 = -lam2; mu2 <= lam2; mu2++) {
																																														
																	for (int mu = -lam; mu <= lam; mu++) {
																		ang_alpha = angInts.getIntegral(alpha_x, alpha_y, alpha_z, lam, mu, lam1, mu1);
																		ang_beta = angInts.getIntegral(beta_x, beta_y, beta_z, lam, mu, lam2, mu2); 
																		ang = ang_alpha * ang_beta; 
																		
																		// Screen based on the angular integrals
																		if (fabs(ang) > 1e-15) {
																			if (unrolling) {
																				SumTerm newTerm; 
																				newTerm.SA = Pair(lam1, lam1+mu1); 
																				newTerm.SB = Pair(lam2, lam2+mu2);
																				newTerm.radial = Triple(N, lam1, lam2);
																				newTerm.CA = Quintuple(0, na, alpha_x, alpha_y, alpha_z); 
																				newTerm.CB = Quintuple(0, nb, beta_x, beta_y, beta_z); 
																				newTerm.ang = prefac * ang;  
																				newTerm.mu = lam+mu; 
																				newTerm.na = na;
																				newTerm.nb = nb;
																	
																				terms.push_back(newTerm); 
																			}
																			radial_triples.push_back(std::make_tuple(N, lam1, lam2)); 
																		} 
																	}
																	
																}
															}
														}
													}
											
											
												}
											}
										}
									}
								}
							}
					
							nb++;
						}
					}
			
					na++; 
				}
			}
			
			// Sort the radial triples and eliminate repeats
			std::sort(radial_triples.begin(), radial_triples.end()); 
			radial_triples.erase(std::unique(radial_triples.begin(), radial_triples.end()), radial_triples.end()); 
			
			// Determine the maximum number of base integrals needed across the set of all radial integrals
			int nbase = 0; 
			if (radial_triples.size() > 0) {
				Triple& tmax = radial_triples[radial_triples.size()-1]; 
				nbase = std::get<0>(tmax) + std::get<1>(tmax) - 1; 
				nbase = nbase < 0 ? 0 : nbase; 
			}
			
			// Sort the radials into two lists, depending on whether l1 <= l2 (radial_A), or l2 > l1 (radial_B)
			// swapping the order of l1/l2 in the latter case
			std::vector<Triple> radial_A, radial_B; 
			for (Triple& t : radial_triples) {
				if (std::get<1>(t) <= std::get<2>(t)) radial_A.push_back(t);  
				else radial_B.push_back(std::make_tuple(std::get<0>(t), std::get<2>(t), std::get<1>(t)));
			}
			
			// Compute the correctly ordered radials first
			outfile << "\tstd::vector<Triple> radial_triples_A {" << std::endl; 
			bool first = true; 
			for (Triple& t : radial_A) {
				if (!first) outfile << "," << std::endl; 
				else first = false; 
				outfile << "\t\tstd::make_tuple(" + std::to_string(std::get<0>(t)) + ", "
					+ std::to_string(std::get<1>(t)) + ", " 
						+ std::to_string(std::get<2>(t)) + ")"; 
			}
			outfile << "\t};" << std::endl << std::endl;  
			
			outfile << "\tThreeIndex<double> radials(" << lam+LA+LB+1 << ", " << lam+LA+1 << ", " << lam+LB+1 << ");" << std::endl; 
			outfile << "\tradint.type2(radial_triples_A, " << nbase << ", " << lam << ", U, shellA, shellB, Am, Bm, radials);" << std::endl << std::endl; 
			
			// Now compute the reverse-ordered radials
			outfile << "\tstd::vector<Triple> radial_triples_B {" << std::endl; 
			first = true;
			for (Triple& t : radial_B) {
				if (!first) outfile << "," << std::endl; 
				else first = false; 
				outfile << "\t\tstd::make_tuple(" + std::to_string(std::get<0>(t)) + ", "
					+ std::to_string(std::get<1>(t)) + ", " 
						+ std::to_string(std::get<2>(t)) + ")"; 
			}
			outfile << "\t};" << std::endl << std::endl;  
			
			outfile << "\tThreeIndex<double> radials_B(" << lam+LA+LB+1 << ", " << lam+LB+1 << ", " << lam+LA+1 << ");" << std::endl; 
			outfile << "\tradint.type2(radial_triples_B, " << nbase << ", " << lam << ", U, shellB, shellA, Bm, Am, radials_B);" << std::endl;
			// These need to be compressed into the radials array, with l1/l2 reversed back
			outfile << "\tfor (Triple& t : radial_triples_B) radials(std::get<0>(t), std::get<2>(t), std::get<1>(t)) = radials_B(std::get<0>(t), std::get<1>(t), std::get<2>(t));" << std::endl << std::endl; 
			
			if (unrolling) {
				// Print out the unrolled angular integral code if needed
				std::cout << "unrolling... " << std::flush; 
				for (auto& term : terms) outfile << "\t" << term << std::endl; 
			} else {
				// Just use the generic rolled-up angular integral code
				outfile << "\trolled_up(" << lam << ", " << LA << ", " << LB << ", radials, CA, CB, SA, SB, angint, values);" << std::endl; 
			}
			outfile << "}" << std::endl << "}" << std::endl << "}" << std::endl; 
			
			std::cout << "done." << std::endl; 
			outfile.close();
		}
	}

}

int main(int argc, char* argv[]) {
	
	// Factorial singletons will not have been initialised
#ifndef FAC_INIT
#define FAC_INIT
		libecpint::FAC[0] = 1.0;
		libecpint::DFAC[0] = 1.0;
		libecpint::DFAC[1] = 1.0;
		
		for (int i = 1; i < MAX_FAC; i++)  libecpint::FAC[i] = double(i) * libecpint::FAC[i-1]; 
		for (int i = 2; i < MAX_DFAC; i++) libecpint::DFAC[i] = double(i) * libecpint::DFAC[i-2];
#endif

	libecpint::AngularIntegral angInts(maxL, maxL); 
	angInts.compute(); 
	
	// Generate the qgen.hpp header file
	std::string header_name; 
	if (argc > 1) {
		header_name = argv[1]; 
		header_name += "qgen.hpp"; 
	} else {
		header_name = "generated/qgen"; 
	}

	std::ifstream qgen_part("generated/qgen.part");
	std::ofstream qgen_head(header_name); 
	if (!qgen_part.is_open() || !qgen_head.is_open()) 
		std::cerr << "Problem creating qgen header file!" << std::endl; 
	else {
		std::string line; 
		while(!qgen_part.eof()) {
			std::getline(qgen_part, line); 
			qgen_head << line << std::endl;  
		}
		qgen_part.close();
		
		// Loop over all possible (l1, l2, lam) integrals up to l1 = l2 = lam = maxL
		// with l1 <= l2, generating the code and adding the function to the header file.
		for (int j = 0; j <= maxL; j++) {
			for (int i = 0; i <= j; i++) {
				for (int k = 0; k <= maxL; k++) {
					libecpint::generate_lists(i, j, k, angInts); 
					qgen_head << "\tvoid Q" << i << "_" << j << "_" << k << "("
								<< "ECP&, GaussianShell&, GaussianShell&, FiveIndex<double>&, FiveIndex<double>&, "
								<< "TwoIndex<double>&, TwoIndex<double>&, double, double, RadialIntegral&, "
								<< "AngularIntegral&, ThreeIndex<double>&);" << std::endl; 
				}
			}
		}
		qgen_head << std::endl << "}" << std::endl << "}" << std::endl; 
		qgen_head << "#endif" << std::endl; 
		qgen_head.close(); 
		
		// Now generate the function pointer array in ecpint_gen.cpp
		std::ifstream ecpgen_part("generated/ecpint_gen.part"); 
		std::ofstream ecpgen_head("generated/ecpint_gen.cpp"); 
		
		if (!ecpgen_part.is_open() || !ecpgen_head.is_open())
			std::cerr << "Problem reading/writing ecpgen file!" << std::endl;
		else {
			while(!ecpgen_part.eof()) {
				std::getline(ecpgen_part, line); 
				ecpgen_head << line << std::endl;  
			}
			ecpgen_part.close();
			
			for (int i =0; i <= maxL; i++) {
				ecpgen_head << "\t\t{ "; 
				
				for (int j = 0; j<= maxL; j++) {
					ecpgen_head << "\t\t\t{"; 
					
					int I = std::min(i, j);
					int J = std::max(i, j); 
					
					for (int k = 0; k< maxL; k++) 
						ecpgen_head << "qgen::Q" << I << "_" << J << "_" << k << ", ";
					
					ecpgen_head << "qgen::Q" << I << "_" << J << "_" << maxL << "}";
					if (j != maxL) ecpgen_head << ","; 
					ecpgen_head << std::endl;
				}
				
				ecpgen_head << "\t\t}";
				if (i != maxL) ecpgen_head << ","; 
				ecpgen_head << std::endl; 
			}
			
			ecpgen_head << "\t};" << std::endl << "}" << std::endl;
			ecpgen_head.close();  
		}
	}
	return 0; 
}
