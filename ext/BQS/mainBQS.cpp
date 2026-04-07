
#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <algorithm>
#include <cassert>
#include <ctime>
#include <functional>
#include "np.h"
#include <filesystem>

namespace fs = std::filesystem;

bool createDirectories(const std::string& path) {
	try {
		fs::create_directories(path);
		return true;
	}
	catch (const fs::filesystem_error& e) {
		std::cerr << "Failed to create directories£º" << e.what() << std::endl;
		return false;
	}
}

int main(int argc, char* argv[]) {
	std::string output_file;
	std::string input_qnp_file;

	if (argc < 3) {
		std::cerr << "Usage: " << argv[0] << " <input-qnp-file> <output-file>" << std::endl;
		return 1; 
	}

	input_qnp_file = argv[1];
	output_file = argv[2];


	fs::path filePath(output_file);
	fs::path parentPath = filePath.parent_path();
	createDirectories(parentPath.string());


	std::ifstream fin;
	std::ofstream fout_res;
	NP::Problem* problem = nullptr;
	fin = std::ifstream(input_qnp_file);
	problem = NP::Problem::read(fin);
	fout_res = std::ofstream(output_file);

	//auto test = NP::Sieve;
	auto test = NP::SieveForBQNP;

	NP::Policy policy;
	double solve_time = 0.0;
	if (!fin) {
		std::cerr << "File not found.";
	}
	// call BQS solver
	else if(NP::NPSolver(problem, policy, true, test, solve_time))
	{
		//fout_res << "time(s): " << solve_time << std::endl;
		NP::dump(fout_res, policy);
		std::cout << std::endl << "Found solution." << std::endl;
	}
	else
	{
		fout_res << "No solution." << std::endl;
		std::cout << "No solution." << std::endl;
	}

	fout_res.close();

	return 0;

}
