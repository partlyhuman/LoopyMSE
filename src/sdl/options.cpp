#include "options.h"

#include <SDL2/SDL.h>

#include <fstream>
#include <iostream>

#include "common/imgwriter.h"
#include "input/input.h"
#include "log/log.h"

namespace po = boost::program_options;
namespace imagew = Common::ImageWriter;

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
	{"keyboard-map.pad_start", Input::PAD_START}, {"keyboard-map.pad_l1", Input::PAD_L1},
	{"keyboard-map.pad_r1", Input::PAD_R1},		  {"keyboard-map.pad_a", Input::PAD_A},
	{"keyboard-map.pad_d", Input::PAD_D},		  {"keyboard-map.pad_c", Input::PAD_C},
	{"keyboard-map.pad_b", Input::PAD_B},		  {"keyboard-map.pad_up", Input::PAD_UP},
	{"keyboard-map.pad_down", Input::PAD_DOWN},	  {"keyboard-map.pad_left", Input::PAD_LEFT},
	{"keyboard-map.pad_right", Input::PAD_RIGHT},
};
const std::unordered_map<std::string, Input::PadButton> CONTROLLER_CONFIG_KEY_TO_PAD_ENUM = {
	{"controller-map.pad_start", Input::PAD_START}, {"controller-map.pad_l1", Input::PAD_L1},
	{"controller-map.pad_r1", Input::PAD_R1},		{"controller-map.pad_a", Input::PAD_A},
	{"controller-map.pad_d", Input::PAD_D},			{"controller-map.pad_c", Input::PAD_C},
	{"controller-map.pad_b", Input::PAD_B},			{"controller-map.pad_up", Input::PAD_UP},
	{"controller-map.pad_down", Input::PAD_DOWN},	{"controller-map.pad_left", Input::PAD_LEFT},
	{"controller-map.pad_right", Input::PAD_RIGHT},
};

bool parse_config(fs::path config_path, Args& args)
{
	// Probably not the correct place for this, but should always run, until configuration exists
	input_add_default_controller_bindings();

	po::variables_map vm;
	po::options_description key_options("Keyboard Map");
	key_options.add_options()
		("keyboard-map.pad_start", po::value<std::string>()->default_value("return"), "Start")
		("keyboard-map.pad_l1", po::value<std::string>()->default_value("q"), "L1")
		("keyboard-map.pad_r1", po::value<std::string>()->default_value("w"), "R1")
		("keyboard-map.pad_a", po::value<std::string>()->default_value("z"), "A button")
		("keyboard-map.pad_b", po::value<std::string>()->default_value("x"), "B button")
		("keyboard-map.pad_c", po::value<std::string>()->default_value("a"), "C button")
		("keyboard-map.pad_d", po::value<std::string>()->default_value("s"), "D button")
		("keyboard-map.pad_up", po::value<std::string>()->default_value("up"), "D-pad up") 
		("keyboard-map.pad_down", po::value<std::string>()->default_value("down"), "D-pad down")
		("keyboard-map.pad_left", po::value<std::string>()->default_value("left"), "D-pad left")
		("keyboard-map.pad_right", po::value<std::string>()->default_value("right"), "D-pad right");

	po::options_description button_options("Controller Map");
	button_options.add_options()
		("controller-map.pad_start", po::value<std::string>()->default_value("start"), "Controller Start")
		("controller-map.pad_l1", po::value<std::string>()->default_value("leftshoulder"), "Controller L1")
		("controller-map.pad_r1", po::value<std::string>()->default_value("rightshoulder"), "Controller R1")
		("controller-map.pad_a", po::value<std::string>()->default_value("a"), "Controller A button")
		("controller-map.pad_b", po::value<std::string>()->default_value("b"), "Controller B button")
		("controller-map.pad_c", po::value<std::string>()->default_value("y"), "Controller C button")
		("controller-map.pad_d", po::value<std::string>()->default_value("x"), "Controller D button")
		("controller-map.pad_up", po::value<std::string>()->default_value("dpup"), "Controller D-pad up") 
		("controller-map.pad_down", po::value<std::string>()->default_value("dpdown"), "Controller D-pad down")
		("controller-map.pad_left", po::value<std::string>()->default_value("dpleft"), "Controller D-pad left")
		("controller-map.pad_right", po::value<std::string>()->default_value("dpright"), "Controller D-pad right");

	po::options_description emu_options("Emulator");
	emu_options.add_options()
		("emulator.bios", po::value<std::string>()->default_value("bios.bin"), "BIOS file")
		("emulator.sound_bios", po::value<std::string>()->default_value("soundbios.bin"), "Sound BIOS file")
		("emulator.run_in_background", po::value<bool>()->default_value(false), "Continue emulation while window not focused")
		("emulator.start_in_fullscreen", po::value<bool>()->default_value(false), "Default to fullscreen mode")
		("emulator.start_with_mouse", po::value<bool>()->default_value(true), "Start in mouse mode for games known to use mouse")
		("emulator.int_scale", po::value<int>()->default_value(3), "Integer scale")
		("emulator.correct_aspect_ratio", po::value<bool>()->default_value(true), "Stretch display pixels to 4:3")
		("emulator.crop_overscan", po::value<bool>()->default_value(true), "Crop border and overscan areas")
		("emulator.antialias", po::value<bool>()->default_value(true), "Apply AA (recommended when used with aspect ratio correction)")
		("emulator.screenshot_image_type", po::value<std::string>()->default_value("bmp"), "Image file type for screenshots");

	po::options_description printer_options("Printer");
	printer_options.add_options()
		("printer.image_type", po::value<std::string>()->default_value("bmp"), "Image file type for printed files")
		("printer.view_command", po::value<std::string>()->default_value("OPEN"), "Command to run with printed files");

	po::options_description options;
	options.add(key_options).add(button_options).add(emu_options).add(printer_options);

	if (!std::ifstream(config_path))
	{
		Log::error("Config not found at %s...", config_path.c_str());
		return false;
	}

	try
	{
		po::store(po::parse_config_file(config_path.string().c_str(), options, true), vm);
		po::notify(vm);

		args.bios = vm["emulator.bios"].as<std::string>();
		args.sound_bios = vm["emulator.sound_bios"].as<std::string>();
		args.run_in_background = vm["emulator.run_in_background"].as<bool>();
		args.start_in_fullscreen = vm["emulator.start_in_fullscreen"].as<bool>();
		args.start_with_mouse = vm["emulator.start_with_mouse"].as<bool>();
		args.correct_aspect_ratio = vm["emulator.correct_aspect_ratio"].as<bool>();
		args.antialias = vm["emulator.antialias"].as<bool>();
		args.crop_overscan = vm["emulator.crop_overscan"].as<bool>();
		args.int_scale = vm["emulator.int_scale"].as<int>();
		args.screenshot_image_type = imagew::parse_image_type(
			vm["emulator.screenshot_image_type"].as<std::string>(), imagew::IMAGE_TYPE_DEFAULT
		);

		args.printer_image_type =
			imagew::parse_image_type(vm["printer.image_type"].as<std::string>(), imagew::IMAGE_TYPE_DEFAULT);
		args.printer_view_command = vm["printer.view_command"].as<std::string>();

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
