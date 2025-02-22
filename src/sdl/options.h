#pragma once
#include <boost/program_options.hpp>
#include <filesystem> 

namespace fs = std::filesystem;

namespace Options
{

struct Args
{
	std::string cart;
	std::string bios;
	std::string sound_bios;
	bool run_in_background;
	bool verbose;
	int int_scale = 2;
};

void parse_commandline(int argc, char** argv, Args& args);
bool parse_config(fs::path config_path, Args& args);
void print_usage();

}  // namespace Options