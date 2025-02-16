#include <SDL.h>
#include <common/bswp.h>
#include <core/config.h>
#include <core/system.h>
#include <input/input.h>
#include <sound/sound.h>
#include <video/video.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace SDL
{

using Video::DISPLAY_HEIGHT;
using Video::DISPLAY_WIDTH;

struct Screen
{
	SDL_Renderer* renderer;
	SDL_Window* window;
	SDL_Texture* texture;
	bool fullscreen;
};

static int INTEGER_SCALE = 4;
static Screen screen;
static SDL_GameController* controller;

void initialize()
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("Failed to initialize SDL2: %s\n", SDL_GetError());
		exit(0);
	}

	//Try synchronizing drawing to VBLANK
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
	//Nearest neighbour scaling
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

	//Set up SDL screen
	SDL_CreateWindowAndRenderer(INTEGER_SCALE * DISPLAY_WIDTH, INTEGER_SCALE * DISPLAY_HEIGHT, 0, &screen.window,
								&screen.renderer);
	SDL_SetWindowTitle(screen.window, "Loopy My Seal Emulator");
	SDL_SetWindowSize(screen.window, INTEGER_SCALE * DISPLAY_WIDTH, INTEGER_SCALE * DISPLAY_HEIGHT);
	SDL_SetWindowResizable(screen.window, SDL_TRUE);
	SDL_RenderSetLogicalSize(screen.renderer, DISPLAY_WIDTH, DISPLAY_HEIGHT);
	SDL_RenderSetIntegerScale(screen.renderer, SDL_TRUE);

	screen.texture = SDL_CreateTexture(screen.renderer, SDL_PIXELFORMAT_ARGB1555, SDL_TEXTUREACCESS_STREAMING,
									   DISPLAY_WIDTH, DISPLAY_HEIGHT);

	//Allow dropping a ROM onto the window
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
}

void shutdown()
{
	//Destroy window, then kill SDL2
	SDL_DestroyTexture(screen.texture);
	SDL_DestroyRenderer(screen.renderer);
	SDL_DestroyWindow(screen.window);

	SDL_Quit();
}

void update(uint16_t* display_output)
{
	// Draw screen
	SDL_UpdateTexture(screen.texture, NULL, display_output, sizeof(uint16_t) * DISPLAY_WIDTH);
	SDL_RenderCopy(screen.renderer, screen.texture, NULL, NULL);
	SDL_RenderPresent(screen.renderer);
}

void open_first_controller()
{
	// Gets the first controller available
	for (int i = 0; i < SDL_NumJoysticks(); i++)
	{
		if (SDL_IsGameController(i))
		{
			SDL_Log("Connected to game controller %s", SDL_GameControllerNameForIndex(i));
			controller = SDL_GameControllerOpen(i);
		}
	}

	controller = nullptr;
}

void resize_window()
{
	if (!screen.fullscreen)
	{
		SDL_SetWindowSize(screen.window, INTEGER_SCALE * DISPLAY_WIDTH, INTEGER_SCALE * DISPLAY_HEIGHT);
	}
}

void toggle_fullscreen()
{
	if (SDL_SetWindowFullscreen(screen.window, screen.fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP) < 0)
	{
		printf("Error fullscreening: %s\n", SDL_GetError());
		return;
	}
	screen.fullscreen = !screen.fullscreen;
}

}  // namespace SDL

std::string remove_extension(std::string file_path)
{
	auto pos = file_path.find(".");
	if (pos == std::string::npos)
	{
		return file_path;
	}

	return file_path.substr(0, pos);
}

enum OrdinalArgument
{
	ARGUMENT_CART = 0,
	ARGUMENT_BIOS,
	ARGUMENT_SOUND_BIOS,
};

bool load_cart(Config::SystemInfo& config, std::string path)
{
	config.cart = {};

	std::ifstream cart_file(path, std::ios::binary);
	if (!cart_file.is_open()) return false;

	config.cart.rom.assign(std::istreambuf_iterator<char>(cart_file), {});
	cart_file.close();

	//Determine the size of SRAM from the cartridge header
	uint32_t sram_start, sram_end;
	memcpy(&sram_start, config.cart.rom.data() + 0x10, 4);
	memcpy(&sram_end, config.cart.rom.data() + 0x14, 4);
	uint32_t sram_size = Common::bswp32(sram_end) - Common::bswp32(sram_start) + 1;

	//Attempt to load SRAM from a file
	config.cart.sram_file_path = remove_extension(path) + ".sav";
	std::ifstream sram_file(config.cart.sram_file_path, std::ios::binary);
	if (!sram_file.is_open())
	{
		printf("Creating save state at %s.\n", config.cart.sram_file_path.c_str());
	}
	else
	{
		config.cart.sram.assign(std::istreambuf_iterator<char>(sram_file), {});
		sram_file.close();
	}

	//Ensure SRAM is at the proper size. If no file is loaded, it will be filled with 0xFF.
	//If a file was loaded but was smaller than the SRAM size, the uninitialized bytes will be 0xFF.
	//If the file was larger, then the vector size is clamped
	config.cart.sram.resize(sram_size, 0xFF);
	return true;
}

bool load_bios(Config::SystemInfo& config, std::string path)
{
	std::ifstream bios_file(path, std::ios::binary);
	if (!bios_file.is_open()) return false;

	config.bios_rom.assign(std::istreambuf_iterator<char>(bios_file), {});
	bios_file.close();
	return true;
}

bool load_sound_bios(Config::SystemInfo& config, std::string path)
{
	std::ifstream sound_rom_file(path, std::ios::binary);
	if (!sound_rom_file.is_open()) return false;

	config.sound_rom.assign(std::istreambuf_iterator<char>(sound_rom_file), {});
	sound_rom_file.close();
	return true;
}

int main(int argc, char** argv)
{
	const std::string DEFAULT_BIOS_PATH = "bios.bin";
	const std::string DEFAULT_SOUND_BIOS_PATH = "soundbios.bin";

	auto print_usage = [&]() { printf("Usage: %s [game ROM] [BIOS] [sound BIOS] [-v/--verbose]\n", argv[0]); };

	bool has_quit = false;
	bool is_paused = false;

	SDL::initialize();
	Config::SystemInfo config = {};
	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_ERROR);

	for (int i = 1, ordinal = 0; i < argc; i++)
	{
		std::string arg = argv[i];

		//Parse flag arguments anywhere they appear
		if (arg.length() > 0 && arg[0] == '-')
		{
			if (arg == "-v" || arg == "--verbose")
			{
				SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
				continue;
			}
		}

		//Parse ordinal arguments
		switch (ordinal)
		{
		case ARGUMENT_CART:
			load_cart(config, arg);
			ordinal++;
			break;
		case ARGUMENT_BIOS:
			load_bios(config, arg);
			ordinal++;
			break;
		case ARGUMENT_SOUND_BIOS:
			load_sound_bios(config, arg);
			ordinal++;
			break;
		default:
			//Ignore any further arguments
			break;
		}
	}

	if (config.bios_rom.empty())
	{
		if (!load_bios(config, DEFAULT_BIOS_PATH))
		{
			printf("Error: Missing BIOS file. Provide by argument, or place in %s.\n", DEFAULT_BIOS_PATH.c_str());
			print_usage();
			return 1;
		}
	}

	if (config.sound_rom.empty())
	{
		if (!load_sound_bios(config, DEFAULT_SOUND_BIOS_PATH))
		{
			printf(
				"Missing sound bios file. Provide by argument, or place in %s.\n"
				"Emulation will continue without sound.\n",
				DEFAULT_SOUND_BIOS_PATH.c_str());
		}
	}

	if (config.cart.is_loaded())
	{
		System::initialize(config);
	}
	else
	{
		printf("Missing Cartridge ROM file. Drop a Loopy ROM onto the window to play.\n");
	}

	//All subprojects have been initialized, so it is safe to reference them now
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

	//Incredibly lazy hack to allow button enum to coexist with keycodes: use negatives
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

	if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0)
	{
		printf("Could not initialize game controllers: %s\n", SDL_GetError());
	}
	else if (SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt") < 0)
	{
		// Potentially continue without the mappings?
		printf("Could not load game controller database: %s\n", SDL_GetError());
	}
	else
	{
		SDL::open_first_controller();
	}

	while (!has_quit)
	{
		if (config.cart.is_loaded() && !is_paused)
		{
			System::run();
			SDL::update(System::get_display_output());
		}

		SDL_Event e;
		while (SDL_PollEvent(&e))
		{
			switch (e.type)
			{
			case SDL_QUIT:
				has_quit = true;
				break;
			case SDL_CONTROLLERBUTTONDOWN:
				Input::set_key_state(-e.cbutton.button, true);
				break;
			case SDL_KEYDOWN:
				Input::set_key_state(e.key.keysym.sym, true);
				break;
			case SDL_KEYUP:
			{
				SDL_Keycode keycode = e.key.keysym.sym;
				switch (keycode)
				{
				case SDLK_F10:
					if (config.cart.is_loaded())
					{
						printf("Dumping frame...\n");
						Video::dump_current_frame();
					}
					break;
				case SDLK_F11:
					if (config.cart.is_loaded())
					{
						SDL::toggle_fullscreen();
					}
					break;
				case SDLK_F12:
					if (config.cart.is_loaded())
					{
						printf("Rebooting Loopy...\n");
						System::shutdown();
						System::initialize(config);
					}
					break;
				case SDLK_MINUS:
					SDL::INTEGER_SCALE = std::max(1, SDL::INTEGER_SCALE - 1);
					SDL::resize_window();
					break;
				case SDLK_EQUALS:
					SDL::INTEGER_SCALE = std::min(8, SDL::INTEGER_SCALE + 1);
					SDL::resize_window();
					break;
				case SDLK_ESCAPE:
					if (SDL::screen.fullscreen)
					{
						SDL::toggle_fullscreen();
					}
					break;
				default:
					Input::set_key_state(keycode, false);
					break;
				}
				break;
			}
			case SDL_CONTROLLERBUTTONUP:
				Input::set_key_state(-e.cbutton.button, false);
				break;
			case SDL_WINDOWEVENT:
				switch (e.window.event)
				{
				case SDL_WINDOWEVENT_FOCUS_GAINED:
					Sound::set_mute(false);
					is_paused = false;
					break;
				case SDL_WINDOWEVENT_FOCUS_LOST:
					Sound::set_mute(true);
					is_paused = true;
					break;
				}
				break;
			case SDL_CONTROLLERDEVICEADDED:
				if (!SDL::controller)
				{
					SDL_Log("New controller added.");
					SDL::open_first_controller();
				}
				break;
			case SDL_CONTROLLERDEVICEREMOVED:
				// Only react to current device being removed
				if (SDL::controller &&
					e.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(SDL::controller)))
				{
					SDL_Log("Controller removed, using next available one.");
					SDL_GameControllerClose(SDL::controller);
					SDL::open_first_controller();
				}
				break;
			case SDL_DROPFILE:
				System::shutdown();

				std::string path = e.drop.file;
				if (load_cart(config, path))
				{
					printf("Loaded %s...\n", path.c_str());
					System::initialize(config);
					// So you can tell that MSE is running even before you click into it
					is_paused = false;
				}
				break;
			}
		}
	}

	System::shutdown();
	SDL::shutdown();

	return 0;
}
