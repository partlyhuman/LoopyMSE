#include "options.h"

#include <SDL.h>

#include <fstream>
#include <iostream>

#include "input/input.h"
#include "log/log.h"

namespace po = boost::program_options;

namespace Options
{

static po::options_description commandline_opts = po::options_description("Usage");

void print_usage()
{
	std::cout << commandline_opts << std::endl;
}

void parse_commandline(int argc, char** argv, Args& args)
{
	commandline_opts.add_options()
		("help,h", "Show this screen")
		("bios", po::value<std::string>(), "Path to Loopy BIOS file")
		("sound_bios", po::value<std::string>(), "Path to Loopy sound BIOS file")
		("verbose,v", "Enable verbose logging output")
		("cart", po::value<std::string>(), "Cartridge to load (--cart can be omitted, use path to ROM as first positional argument)" );

	po::variables_map vm;
	po::positional_options_description positional_options;
	positional_options.add("cart", -1);

	try
	{
		auto parser = po::command_line_parser(argc, argv).positional(positional_options).options(commandline_opts);
		po::store(parser.run(), vm);
		po::notify(vm);
	}
	catch (po::error& e)
	{
		Log::error("Couldn't parse command line: %s", e.what());
		print_usage();
		exit(1);
	}

	if (vm.count("help"))
	{
		print_usage();
		exit(0);
	}

	if (vm.count("bios")) args.bios = vm["bios"].as<std::string>();
	if (vm.count("sound_bios")) args.sound_bios = vm["sound_bios"].as<std::string>();
	if (vm.count("cart")) args.cart = vm["cart"].as<std::string>();
	args.verbose = vm.count("verbose");
}

const std::unordered_map<std::string, Input::PadButton> CONFIG_PAD_BUTTON = {
	{"pad_start_scancode", Input::PAD_START}, {"pad_l1_scancode", Input::PAD_L1},
	{"pad_r1_scancode", Input::PAD_R1},		  {"pad_a_scancode", Input::PAD_A},
	{"pad_d_scancode", Input::PAD_D},		  {"pad_c_scancode", Input::PAD_C},
	{"pad_b_scancode", Input::PAD_B},		  {"pad_up_scancode", Input::PAD_UP},
	{"pad_down_scancode", Input::PAD_DOWN},	  {"pad_left_scancode", Input::PAD_LEFT},
	{"pad_right_scancode", Input::PAD_RIGHT},
};

bool parse_config(std::string config_path, Args& args)
{
	po::variables_map vm;
	po::options_description key_options("Keys");
	key_options.add_options()
		("pad_start_scancode", po::value<int>()->default_value(SDL_SCANCODE_RETURN), "Start")
		("pad_l1_scancode", po::value<int>()->default_value(SDL_SCANCODE_Q), "L1")
		("pad_r1_scancode", po::value<int>()->default_value(SDL_SCANCODE_W), "R1")
		("pad_a_scancode", po::value<int>()->default_value(SDL_SCANCODE_Z), "A button")
		("pad_b_scancode", po::value<int>()->default_value(SDL_SCANCODE_X), "B button")
		("pad_c_scancode", po::value<int>()->default_value(SDL_SCANCODE_A), "C button")
		("pad_d_scancode", po::value<int>()->default_value(SDL_SCANCODE_S), "D button")
		("pad_up_scancode", po::value<int>()->default_value(SDL_SCANCODE_UP), "D-pad up") 
		("pad_down_scancode", po::value<int>()->default_value(SDL_SCANCODE_DOWN), "D-pad down")
		("pad_left_scancode", po::value<int>()->default_value(SDL_SCANCODE_LEFT), "D-pad left")
		("pad_right_scancode", po::value<int>()->default_value(SDL_SCANCODE_RIGHT), "D-pad right");

	po::options_description emu_options("Emulation");
	emu_options.add_options()
		("bios", po::value<std::string>()->default_value("bios.bin"), "BIOS file")
		("sound_bios", po::value<std::string>()->default_value("soundbios.bin"), "Sound BIOS file")
		("run_in_background", po::value<bool>()->default_value(false), "Continue emulation while window not focused")
		("int_scale", po::value<int>()->default_value(3), "Integer scale");

	po::options_description options;
	options.add(key_options).add(emu_options);

	// Not yet in config - controller config
	// Incredibly lazy hack to allow button enum to coexist with keycodes: use negatives
	Input::add_key_binding(-SDL_CONTROLLER_BUTTON_A, Input::PAD_A);
	Input::add_key_binding(-SDL_CONTROLLER_BUTTON_B, Input::PAD_B);
	Input::add_key_binding(-SDL_CONTROLLER_BUTTON_Y, Input::PAD_C);
	Input::add_key_binding(-SDL_CONTROLLER_BUTTON_X, Input::PAD_D);
	Input::add_key_binding(-SDL_CONTROLLER_BUTTON_LEFTSHOULDER, Input::PAD_L1);
	Input::add_key_binding(-SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, Input::PAD_R1);
	Input::add_key_binding(-SDL_CONTROLLER_BUTTON_DPAD_LEFT, Input::PAD_LEFT);
	Input::add_key_binding(-SDL_CONTROLLER_BUTTON_DPAD_RIGHT, Input::PAD_RIGHT);
	Input::add_key_binding(-SDL_CONTROLLER_BUTTON_DPAD_UP, Input::PAD_UP);
	Input::add_key_binding(-SDL_CONTROLLER_BUTTON_DPAD_DOWN, Input::PAD_DOWN);
	Input::add_key_binding(-SDL_CONTROLLER_BUTTON_START, Input::PAD_START);

	// Create empty ini if none found
	// Changes failed parsing into defaults
	if (!std::ifstream(config_path))
	{
		Log::error("Config not found at %s...", config_path.c_str());
		std::ofstream(config_path).close();
		return false;
	}

	try
	{
		po::store(po::parse_config_file(config_path.c_str(), options, true), vm);
		po::notify(vm);

		args.bios = vm["bios"].as<std::string>();
		args.sound_bios = vm["sound_bios"].as<std::string>();
		args.run_in_background = vm["run_in_background"].as<bool>();
		args.int_scale = vm["int_scale"].as<int>();

		// Keymap
		for (const auto& [cfg_key, pad_key] : CONFIG_PAD_BUTTON)
		{
			if (!vm.count(cfg_key)) continue;
			auto scancode = vm[cfg_key].as<int>();
			Input::add_key_binding(scancode, pad_key);
		}
	}
	catch (po::error& e)
	{
		Log::warn("No config file found at %s, or could not parse config file.", config_path.c_str());

		// Ensure there are SOME key configs
		Input::add_key_binding(SDL_SCANCODE_RETURN, Input::PAD_START);

		Input::add_key_binding(SDL_SCANCODE_Z, Input::PAD_A);
		Input::add_key_binding(SDL_SCANCODE_X, Input::PAD_B);
		Input::add_key_binding(SDL_SCANCODE_A, Input::PAD_C);
		Input::add_key_binding(SDL_SCANCODE_S, Input::PAD_D);
		Input::add_key_binding(SDL_SCANCODE_Q, Input::PAD_L1);
		Input::add_key_binding(SDL_SCANCODE_W, Input::PAD_R1);

		Input::add_key_binding(SDL_SCANCODE_LEFT, Input::PAD_LEFT);
		Input::add_key_binding(SDL_SCANCODE_RIGHT, Input::PAD_RIGHT);
		Input::add_key_binding(SDL_SCANCODE_UP, Input::PAD_UP);
		Input::add_key_binding(SDL_SCANCODE_DOWN, Input::PAD_DOWN);
		return false;
	}

	return true;
}

}  // namespace Options
