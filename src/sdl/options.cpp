#include "options.h"

#include <SDL2/SDL.h>

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

void input_add_default_key_bindings()
{
	// Ensure there are SOME key configs
	Input::add_key_binding(SDLK_RETURN, Input::PAD_START);

	Input::add_key_binding(SDLK_z, Input::PAD_A);
	Input::add_key_binding(SDLK_x, Input::PAD_B);
	Input::add_key_binding(SDLK_a, Input::PAD_C);
	Input::add_key_binding(SDLK_s, Input::PAD_D);
	Input::add_key_binding(SDLK_q, Input::PAD_L1);
	Input::add_key_binding(SDLK_w, Input::PAD_R1);

	Input::add_key_binding(SDLK_LEFT, Input::PAD_LEFT);
	Input::add_key_binding(SDLK_RIGHT, Input::PAD_RIGHT);
	Input::add_key_binding(SDLK_UP, Input::PAD_UP);
	Input::add_key_binding(SDLK_DOWN, Input::PAD_DOWN);
}

void input_add_default_controller_bindings()
{
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
	{"pad_start", Input::PAD_START}, {"pad_l1", Input::PAD_L1},		  {"pad_r1", Input::PAD_R1},
	{"pad_a", Input::PAD_A},		 {"pad_d", Input::PAD_D},		  {"pad_c", Input::PAD_C},
	{"pad_b", Input::PAD_B},		 {"pad_up", Input::PAD_UP},		  {"pad_down", Input::PAD_DOWN},
	{"pad_left", Input::PAD_LEFT},	 {"pad_right", Input::PAD_RIGHT},
};

bool parse_config(std::string config_path, Args& args)
{
	// Probably not the correct place for this, but should always run, until configuration exists
	input_add_default_controller_bindings();

	po::variables_map vm;
	po::options_description key_options("Keys");
	key_options.add_options()
		("pad_start", po::value<std::string>()->default_value("RETURN"), "Start")
		("pad_l1", po::value<std::string>()->default_value("Q"), "L1")
		("pad_r1", po::value<std::string>()->default_value("W"), "R1")
		("pad_a", po::value<std::string>()->default_value("Z"), "A button")
		("pad_b", po::value<std::string>()->default_value("X"), "B button")
		("pad_c", po::value<std::string>()->default_value("A"), "C button")
		("pad_d", po::value<std::string>()->default_value("S"), "D button")
		("pad_up", po::value<std::string>()->default_value("UP"), "D-pad up") 
		("pad_down", po::value<std::string>()->default_value("DOWN"), "D-pad down")
		("pad_left", po::value<std::string>()->default_value("LEFT"), "D-pad left")
		("pad_right", po::value<std::string>()->default_value("RIGHT"), "D-pad right");

	po::options_description emu_options("Emulation");
	emu_options.add_options()
		("bios", po::value<std::string>()->default_value("bios.bin"), "BIOS file")
		("sound_bios", po::value<std::string>()->default_value("soundbios.bin"), "Sound BIOS file")
		("run_in_background", po::value<bool>()->default_value(false), "Continue emulation while window not focused")
		("int_scale", po::value<int>()->default_value(3), "Integer scale");

	po::options_description options;
	options.add(key_options).add(emu_options);

	if (!std::ifstream(config_path))
	{
		Log::error("Config not found at %s...", config_path.c_str());
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
			if (!vm.count(cfg_key))
			{
				Log::warn("Loopy %s <- [No key set]", cfg_key.c_str());
				continue;
			}
			std::string key_string = vm[cfg_key].as<std::string>();
			SDL_Keycode keycode = SDL_GetKeyFromName(key_string.c_str());
			if (keycode == SDLK_UNKNOWN)
			{
				Log::error("Could not parse key '%s' defined by %s", key_string.c_str(), cfg_key.c_str());
			}
			else
			{
				Input::add_key_binding(keycode, pad_key);
			}
		}
	}
	catch (po::error& e)
	{
		Log::warn("No config file found at %s, or could not parse config file.", config_path.c_str());
		input_add_default_key_bindings();
		return false;
	}

	return true;
}

}  // namespace Options
