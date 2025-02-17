#pragma once
#include <boost/program_options.hpp>

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
void parse_config(std::string config_path, Args& args);
void print_usage();

}  // namespace Options