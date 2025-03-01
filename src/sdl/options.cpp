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
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_A, Input::PAD_A);
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_B, Input::PAD_B);
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_Y, Input::PAD_C);
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_X, Input::PAD_D);
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, Input::PAD_L1);
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, Input::PAD_R1);
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_DPAD_LEFT, Input::PAD_LEFT);
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, Input::PAD_RIGHT);
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_DPAD_UP, Input::PAD_UP);
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_DPAD_DOWN, Input::PAD_DOWN);
	Input::add_controller_binding(SDL_CONTROLLER_BUTTON_START, Input::PAD_START);
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

const std::unordered_map<std::string, Input::PadButton> KEYBOARD_CONFIG_KEY_TO_PAD_ENUM = {
	{"keyboard_pad_start", Input::PAD_START}, {"keyboard_pad_l1", Input::PAD_L1},
	{"keyboard_pad_r1", Input::PAD_R1},		  {"keyboard_pad_a", Input::PAD_A},
	{"keyboard_pad_d", Input::PAD_D},		  {"keyboard_pad_c", Input::PAD_C},
	{"keyboard_pad_b", Input::PAD_B},		  {"keyboard_pad_up", Input::PAD_UP},
	{"keyboard_pad_down", Input::PAD_DOWN},	  {"keyboard_pad_left", Input::PAD_LEFT},
	{"keyboard_pad_right", Input::PAD_RIGHT},
};
const std::unordered_map<std::string, Input::PadButton> CONTROLLER_CONFIG_KEY_TO_PAD_ENUM = {
	{"controller_pad_start", Input::PAD_START}, {"controller_pad_l1", Input::PAD_L1},
	{"controller_pad_r1", Input::PAD_R1},		{"controller_pad_a", Input::PAD_A},
	{"controller_pad_d", Input::PAD_D},			{"controller_pad_c", Input::PAD_C},
	{"controller_pad_b", Input::PAD_B},			{"controller_pad_up", Input::PAD_UP},
	{"controller_pad_down", Input::PAD_DOWN},	{"controller_pad_left", Input::PAD_LEFT},
	{"controller_pad_right", Input::PAD_RIGHT},
};

bool parse_config(fs::path config_path, Args& args)
{
	// Probably not the correct place for this, but should always run, until configuration exists
	input_add_default_controller_bindings();

	po::variables_map vm;
	po::options_description key_options("Keyboard Map");
	key_options.add_options()
		("keyboard_pad_start", po::value<std::string>()->default_value("return"), "Start")
		("keyboard_pad_l1", po::value<std::string>()->default_value("q"), "L1")
		("keyboard_pad_r1", po::value<std::string>()->default_value("w"), "R1")
		("keyboard_pad_a", po::value<std::string>()->default_value("z"), "A button")
		("keyboard_pad_b", po::value<std::string>()->default_value("x"), "B button")
		("keyboard_pad_c", po::value<std::string>()->default_value("a"), "C button")
		("keyboard_pad_d", po::value<std::string>()->default_value("s"), "D button")
		("keyboard_pad_up", po::value<std::string>()->default_value("up"), "D-pad up") 
		("keyboard_pad_down", po::value<std::string>()->default_value("down"), "D-pad down")
		("keyboard_pad_left", po::value<std::string>()->default_value("left"), "D-pad left")
		("keyboard_pad_right", po::value<std::string>()->default_value("right"), "D-pad right");

	po::options_description button_options("Controller Map");
	button_options.add_options()
		("controller_pad_start", po::value<std::string>()->default_value("start"), "Controller Start")
		("controller_pad_l1", po::value<std::string>()->default_value("leftshoulder"), "Controller L1")
		("controller_pad_r1", po::value<std::string>()->default_value("rightshoulder"), "Controller R1")
		("controller_pad_a", po::value<std::string>()->default_value("a"), "Controller A button")
		("controller_pad_b", po::value<std::string>()->default_value("b"), "Controller B button")
		("controller_pad_c", po::value<std::string>()->default_value("y"), "Controller C button")
		("controller_pad_d", po::value<std::string>()->default_value("x"), "Controller D button")
		("controller_pad_up", po::value<std::string>()->default_value("dpup"), "Controller D-pad up") 
		("controller_pad_down", po::value<std::string>()->default_value("dpdown"), "Controller D-pad down")
		("controller_pad_left", po::value<std::string>()->default_value("dpleft"), "Controller D-pad left")
		("controller_pad_right", po::value<std::string>()->default_value("dpright"), "Controller D-pad right");

	po::options_description emu_options("Emulation");
	emu_options.add_options()
		("bios", po::value<std::string>()->default_value("bios.bin"), "BIOS file")
		("sound_bios", po::value<std::string>()->default_value("soundbios.bin"), "Sound BIOS file")
		("run_in_background", po::value<bool>()->default_value(false), "Continue emulation while window not focused")
		("start_in_fullscreen", po::value<bool>()->default_value(false), "Default to fullscreen mode")
		("int_scale", po::value<int>()->default_value(3), "Integer scale");

	po::options_description options;
	options.add(key_options).add(button_options).add(emu_options);

	if (!std::ifstream(config_path))
	{
		Log::error("Config not found at %s...", config_path.c_str());
		return false;
	}

	try
	{
		po::store(po::parse_config_file(config_path.string().c_str(), options, true), vm);
		po::notify(vm);

		args.bios = vm["bios"].as<std::string>();
		args.sound_bios = vm["sound_bios"].as<std::string>();
		args.run_in_background = vm["run_in_background"].as<bool>();
		args.start_in_fullscreen = vm["start_in_fullscreen"].as<bool>();
		args.int_scale = vm["int_scale"].as<int>();

		// Keymap
		for (const auto& [cfg_key, pad_key] : KEYBOARD_CONFIG_KEY_TO_PAD_ENUM)
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

		// Controller map
		for (const auto& [cfg_key, pad_key] : CONTROLLER_CONFIG_KEY_TO_PAD_ENUM)
		{
			if (!vm.count(cfg_key))
			{
				Log::warn("Loopy %s <- [No controller button set]", cfg_key.c_str());
				continue;
			}
			std::string button_string = vm[cfg_key].as<std::string>();
			SDL_GameControllerButton button = SDL_GameControllerGetButtonFromString(button_string.c_str());

			// TODO handle axes for analog stick input

			if ((int)button == (int)SDL_CONTROLLER_AXIS_INVALID)
			{
				Log::error(
					"Could not parse game controller button '%s' defined by %s", button_string.c_str(), cfg_key.c_str()
				);
			}
			else
			{
				Input::add_controller_binding(button, pad_key);
			}
		}
	}
	catch (po::error& e)
	{
		Log::warn("No config file found at %s, or could not parse config file.", config_path.c_str());
		input_add_default_key_bindings();
		input_add_default_controller_bindings();
		return false;
	}

	return true;
}

}  // namespace Options
