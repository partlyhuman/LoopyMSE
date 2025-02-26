#include <SDL2/SDL.h>
#include <common/bswp.h>
#include <core/config.h>
#include <core/system.h>
#include <input/input.h>
#include <log/log.h>
#include <sound/sound.h>
#include <video/video.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "config.h"
#include "options.h"

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
	int int_scale;
};

static Screen screen;
static SDL_GameController* controller;

void initialize()
{
	//Allow use of our own main()
	SDL_SetMainReady();

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0)
	{
		Log::error("Failed to initialize SDL2: %s", SDL_GetError());
		exit(0);
	}

	//Try synchronizing window drawing to VBLANK
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
	//Nearest neighbour scaling
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
	SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "1");
	//Helps stuttering after app backgrounded/foregrounded on MacOS
	SDL_SetHint(SDL_HINT_MAC_OPENGL_ASYNC_DISPATCH, "0");

	char title[64];
	snprintf(title, sizeof(title), "%s %s", PROJECT_DESCRIPTION, PROJECT_VERSION);

	//Set up SDL screen
	screen.window = SDL_CreateWindow(
		title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screen.int_scale * DISPLAY_WIDTH,
		screen.int_scale * DISPLAY_HEIGHT, SDL_WINDOW_RESIZABLE
	);
	screen.renderer = SDL_CreateRenderer(screen.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	SDL_SetWindowSize(screen.window, screen.int_scale * DISPLAY_WIDTH, screen.int_scale * DISPLAY_HEIGHT);
	SDL_RenderSetLogicalSize(screen.renderer, DISPLAY_WIDTH, DISPLAY_HEIGHT);
	SDL_RenderSetIntegerScale(screen.renderer, SDL_TRUE);

	screen.texture = SDL_CreateTexture(
		screen.renderer, SDL_PIXELFORMAT_ARGB1555, SDL_TEXTUREACCESS_STREAMING, DISPLAY_WIDTH, DISPLAY_HEIGHT
	);
	SDL_SetTextureBlendMode(screen.texture, SDL_BLENDMODE_BLEND);

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
	SDL_RenderClear(screen.renderer);
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
			Log::info("Connected to game controller %s", SDL_GameControllerNameForIndex(i));
			controller = SDL_GameControllerOpen(i);
		}
	}

	controller = nullptr;
}

void change_int_scale(int delta_int_scale)
{
	if (!screen.fullscreen)
	{
		screen.int_scale = std::clamp(screen.int_scale + delta_int_scale, 1, 8);
		SDL_SetWindowSize(screen.window, screen.int_scale * DISPLAY_WIDTH, screen.int_scale * DISPLAY_HEIGHT);
	}
}

void toggle_fullscreen()
{
	if (SDL_SetWindowFullscreen(screen.window, screen.fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP) < 0)
	{
		Log::error("Error fullscreening: %s", SDL_GetError());
		return;
	}
	screen.fullscreen = !screen.fullscreen;
}

void set_background_color(uint8_t r, uint8_t g, uint8_t b)
{
	SDL_SetRenderDrawColor(screen.renderer, r, g, b, 255);
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

bool load_cart(Config::SystemInfo& config, std::string path)
{
	config.cart = {};

	std::ifstream cart_file(path, std::ios::binary);
	if (!cart_file.is_open())
	{
		Log::error("Couldn't load cartridge at %s", path.c_str());
		return false;
	}

	config.cart.rom_path = path;
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
		Log::debug("Creating save state at %s.", config.cart.sram_file_path.c_str());
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

bool load_bios(Config::SystemInfo& config, fs::path path)
{
	Log::debug("Looking for BIOS in %s", path.c_str());

	std::ifstream bios_file(path, std::ios::binary);
	if (!bios_file.is_open())
	{
		Log::debug("Couldn't load BIOS at %s", path.c_str());
		return false;
	}

	config.bios_rom.assign(std::istreambuf_iterator<char>(bios_file), {});
	bios_file.close();
	return true;
}

bool load_sound_bios(Config::SystemInfo& config, fs::path path)
{
	Log::debug("Looking for Sound BIOS in %s", path.c_str());

	std::ifstream sound_rom_file(path, std::ios::binary);
	if (!sound_rom_file.is_open())
	{
		Log::debug("Couldn't load Sound BIOS at %s", path.c_str());
		return false;
	}

	config.sound_rom.assign(std::istreambuf_iterator<char>(sound_rom_file), {});
	sound_rom_file.close();
	return true;
}

std::vector<fs::path> search_paths(fs::path file_path, fs::path cart_path)
{
	std::vector<fs::path> vec;
	if (file_path.is_relative())
	{
		if (!cart_path.empty() && cart_path.has_parent_path())
		{
			vec.push_back(cart_path.parent_path() / file_path);
		}
		vec.push_back(PREFS_PATH / file_path);
		vec.push_back(RESOURCE_PATH / file_path);
	}
	vec.push_back(file_path);
	return vec;
}

int main(int argc, char** argv)
{
	bool has_quit = false;
	bool is_paused = false;

	Config::SystemInfo config;
	Options::Args args;

	if (!fs::exists(PREFS_PATH / INI_PATH))
	{
		Log::info("Creating default ini file");
		fs::copy_file(RESOURCE_PATH / INI_PATH, PREFS_PATH / INI_PATH);
	}
	Options::parse_config(PREFS_PATH / INI_PATH, args);
	Options::parse_commandline(argc, argv, args);

	Log::set_level(args.verbose ? Log::VERBOSE : Log::INFO);
	SDL::screen.int_scale = std::clamp(args.int_scale, 1, 8);
	SDL::initialize();
	SDL::set_background_color(0, 0, 0);

	bool result = false;
	for (const auto& path : search_paths(args.bios, args.cart))
	{
		if (load_bios(config, path))
		{
			result = true;
			break;
		}
	}
	if (!result)
	{
		Log::error(
			"Error: Missing BIOS file. Provide by argument, or place in %s.\n", (PREFS_PATH / DEFAULT_BIOS_PATH).c_str()
		);

		Options::print_usage();
		exit(1);
	}

	result = false;
	for (const auto& path : search_paths(args.sound_bios, args.cart))
	{
		if (load_sound_bios(config, path))
		{
			result = true;
			break;
		}
	}
	if (!result)
	{
		Log::warn(
			"Missing sound bios file. Provide by argument, or place in %s.\n"
			"Emulation will continue without sound.\n",
			DEFAULT_SOUND_BIOS_PATH.c_str()
		);
	}

	if (args.cart.empty())
	{
		Log::info("No ROM provided. Drop a Loopy ROM onto the window to play.");
	}
	else if (load_cart(config, args.cart))
	{
		System::initialize(config);
	}
	else
	{
		Log::error("Could not load ROM file.");
		exit(1);
	}

	if (SDL_GameControllerAddMappingsFromFile((RESOURCE_PATH / CONTROLLER_DB_PATH).string().c_str()) < 0)
	{
		Log::warn("Could not load game controller database: %s", SDL_GetError());
		//Nonfatal: continue without the mappings
	}
	SDL::open_first_controller();

	constexpr int framerate_target = 60;  //TODO: get this from Video if it can be changed (e.g. for PAL mode)
	constexpr int framerate_max_lag = 5;
	int last_frame_ticks = INT_MAX;
	while (!has_quit)
	{
		//Check how much time passed since we drew the last frame
		int ticks_per_frame = SDL_GetPerformanceFrequency() / framerate_target;
		int now_ticks = SDL_GetPerformanceCounter();
		int ticks_since_last_frame = now_ticks - last_frame_ticks;

		//See how many we need to draw
		//If we're vsynced to a 60Hz display with no lag, this should stay at 1 most of the time
		int draw_frames = ticks_since_last_frame / ticks_per_frame;
		last_frame_ticks += draw_frames * ticks_per_frame;

		//If too far behind, draw one frame and start timing again from now
		if (draw_frames > framerate_max_lag)
		{
			Log::warn("%d frames behind, skipping ahead...", draw_frames);
			last_frame_ticks = now_ticks;
			draw_frames = 1;
		}

		if (draw_frames && !is_paused && config.cart.is_loaded())
		{
			while (draw_frames > 0)
			{
				System::run();
				draw_frames--;
			}
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
						fs::path screencap_filename("loopy.bmp");
						fs::path screencap_path(config.cart.rom_path);
						if (screencap_path.has_parent_path())
						{
							screencap_path = screencap_path.parent_path() / screencap_filename;
						}
						else
						{
							screencap_path = PREFS_PATH / screencap_filename;
						}
						std::cout << screencap_path << "\n";
						Log::info("Dumping frame...");
						Video::dump_current_frame(screencap_path.string());
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
						Log::info("Rebooting Loopy...");
						System::shutdown();
						System::initialize(config);
						last_frame_ticks = INT_MAX;
					}
					break;
				case SDLK_MINUS:
					SDL::change_int_scale(-1);
					break;
				case SDLK_EQUALS:
					SDL::change_int_scale(1);
					break;
				case SDLK_ESCAPE:
					if (SDL::screen.fullscreen)
					{
						SDL::toggle_fullscreen();
					}
					else
					{
						has_quit = true;
					}
					break;
				default:
					Input::set_key_state(keycode, false);
					break;
				}
				break;
			}
			case SDL_CONTROLLERBUTTONDOWN:
				Input::set_controller_state(e.cbutton.button, true);
				break;
			case SDL_CONTROLLERBUTTONUP:
				Input::set_controller_state(e.cbutton.button, false);
				break;
			case SDL_WINDOWEVENT:
				switch (e.window.event)
				{
				case SDL_WINDOWEVENT_FOCUS_GAINED:
					if (!args.run_in_background)
					{
						Sound::set_mute(false);
						is_paused = false;
					}
					break;
				case SDL_WINDOWEVENT_FOCUS_LOST:
					if (!args.run_in_background)
					{
						Sound::set_mute(true);
						is_paused = true;
					}
					break;
				}
				break;
			case SDL_CONTROLLERDEVICEADDED:
				if (!SDL::controller)
				{
					Log::info("New controller added.");
					SDL::open_first_controller();
				}
				break;
			case SDL_CONTROLLERDEVICEREMOVED:
				// Only react to current device being removed
				if (SDL::controller &&
					e.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(SDL::controller)))
				{
					Log::info("Controller removed, using next available one.");
					SDL_GameControllerClose(SDL::controller);
					SDL::open_first_controller();
				}
				break;
			case SDL_DROPFILE:
				System::shutdown();

				std::string path = e.drop.file;
				if (load_cart(config, path))
				{
					Log::info("Loaded %s...", path.c_str());
					System::initialize(config);
					// So you can tell that MSE is running even before you click into it
					is_paused = false;
					last_frame_ticks = INT_MAX;
				}
				break;
			}
		}
	}

	System::shutdown();
	SDL::shutdown();

	return 0;
}
