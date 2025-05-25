#include <SDL2/SDL.h>
#include <common/bswp.h>
#include <common/imgwriter.h>
#include <core/config.h>
#include <core/loopy_io.h>
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

#define PRESCALE_FACTOR 4
#define MAX_WINDOW_INT_SCALE 10

namespace imagew = Common::ImageWriter;
using namespace Config;

namespace SDL
{

using Video::DISPLAY_HEIGHT;
using Video::DISPLAY_WIDTH;
// Logical size includes border to allow for both 224 line and 240 line modes, and show some of the background effects.
// In reality the Loopy active area is drawn inside this and borders are visible, in 240 line mode more of this is used.
static constexpr int FRAME_WIDTH = 280;
static constexpr int FRAME_HEIGHT = 240;
// Scales the frame size up to 4:3 320x240
static constexpr float ASPECT_CORRECT_SCALE_X = (320.0f / FRAME_WIDTH);

struct Screen
{
	SDL_Renderer* renderer;
	SDL_Window* window;
	SDL_Texture* framebuffer;
	SDL_Texture* prescaled;
	int visible_scanlines = DISPLAY_HEIGHT;
	int window_int_scale = 1;
	int prescale = 1;
	bool correct_aspect_ratio;
	bool crop_overscan;
	bool antialias;

	bool is_fullscreen()
	{
		return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) > 0;
	}
};

static Screen screen;
static SDL_GameController* controller;

void shutdown()
{
	SDL_SetRelativeMouseMode(SDL_FALSE);
	SDL_ShowCursor(SDL_ENABLE);

	//Destroy window, then kill SDL2
	if (screen.prescaled) SDL_DestroyTexture(screen.prescaled);
	SDL_DestroyTexture(screen.framebuffer);
	SDL_DestroyRenderer(screen.renderer);
	SDL_DestroyWindow(screen.window);

	SDL_Quit();
}

void capture_mouse(bool cap)
{
	if (SDL_SetRelativeMouseMode(cap ? SDL_TRUE : SDL_FALSE) != 0)
	{
		Log::warn("Relative mouse mode unsupported by system: %s", SDL_GetError());
		return;
	}
}

bool is_mouse_captured()
{
	return SDL_GetRelativeMouseMode();
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
			if (screen.is_fullscreen())
			{
				SDL_ShowCursor(SDL_DISABLE);
			}
			return;
		}
	}

	controller = nullptr;
}

SDL_Point resize_window(bool apply = true, int scale = 0)
{
	if (scale <= 0) scale = screen.window_int_scale;
	float frame_w = scale * (screen.crop_overscan ? DISPLAY_WIDTH : FRAME_WIDTH);
	float frame_h = scale * (screen.crop_overscan ? screen.visible_scanlines : FRAME_HEIGHT);
	if (screen.correct_aspect_ratio)
	{
		frame_w *= ASPECT_CORRECT_SCALE_X;
	}

	SDL_Point frame = {(int)SDL_roundf(frame_w), (int)SDL_roundf(frame_h)};
	if (apply)
	{
		SDL_SetWindowSize(screen.window, frame.x, frame.y);
	}
	Log::info("[SCREEN] size width=%d height=%d", frame.x, frame.y);
	return frame;
}

void change_window_scale(int delta)
{
	float logical_w = (screen.crop_overscan ? DISPLAY_WIDTH : FRAME_WIDTH);
	float logical_h = (screen.crop_overscan ? screen.visible_scanlines : FRAME_HEIGHT);
	if (screen.correct_aspect_ratio)
	{
		logical_w *= ASPECT_CORRECT_SCALE_X;
	}
	int window_w, window_h;
	SDL_GetWindowSize(screen.window, &window_w, &window_h);
	float scale = SDL_min(window_w / logical_w, window_h / logical_h);
	int new_scale = (int)SDL_roundf(scale + delta);
	screen.window_int_scale = std::clamp(new_scale, 1, MAX_WINDOW_INT_SCALE);
	resize_window();
}

void toggle_fullscreen()
{
	bool to_fullscreen = !screen.is_fullscreen();
	if (SDL_SetWindowFullscreen(screen.window, to_fullscreen * SDL_WINDOW_FULLSCREEN_DESKTOP) < 0)
	{
		Log::error("Error fullscreening: %s", SDL_GetError());
		return;
	}

	if (controller)
	{
		SDL_ShowCursor(to_fullscreen ? SDL_DISABLE : SDL_ENABLE);
	}
}

static inline void set_draw_color_16bpp(uint16_t c)
{
	uint8_t r = ((c >> 10) & 31) * 255 / 31;
	uint8_t g = ((c >> 5) & 31) * 255 / 31;
	uint8_t b = (c & 31) * 255 / 31;
	// uint8_t a = (c >> 15) * 255;
	SDL_SetRenderDrawColor(screen.renderer, r, g, b, SDL_ALPHA_OPAQUE);
}

void update(uint16_t* display_output, int visible_scanlines, uint16_t background_color)
{
	if (visible_scanlines != screen.visible_scanlines)
	{
		screen.visible_scanlines = visible_scanlines;
		if (!screen.is_fullscreen() && screen.crop_overscan)
		{
			resize_window();
		}
	}

	void* pixels;
	int pitch;

	// More efficient alternative to SDL_UpdateTexture(screen.texture, NULL, display_output, sizeof(uint16_t) * DISPLAY_WIDTH);
	if (SDL_LockTexture(screen.framebuffer, NULL, &pixels, &pitch) == 0)
	{
		memcpy(pixels, display_output, sizeof(uint16_t) * DISPLAY_WIDTH * DISPLAY_HEIGHT);
		SDL_UnlockTexture(screen.framebuffer);
	}

	// Prescale
	if (screen.prescaled)
	{
		SDL_SetRenderTarget(screen.renderer, screen.prescaled);
		SDL_RenderClear(screen.renderer);
		SDL_RenderCopy(screen.renderer, screen.framebuffer, NULL, NULL);
	}

	// Change target back to screen (must be done before querying renderer output size!)
	SDL_SetRenderTarget(screen.renderer, NULL);
	set_draw_color_16bpp(background_color);
	SDL_RenderClear(screen.renderer);

	SDL_Rect src = {0, 0, DISPLAY_WIDTH * screen.prescale, visible_scanlines * screen.prescale};
	SDL_Rect frame = {0, 0, FRAME_WIDTH * screen.prescale, FRAME_HEIGHT * screen.prescale};
	if (screen.crop_overscan) frame = src;
	SDL_Rect dest = {0};
	SDL_GetRendererOutputSize(screen.renderer, &dest.w, &dest.h);

	float scale_x = (float)dest.w / frame.w;
	float scale_y = (float)dest.h / frame.h;
	float scale = SDL_min(scale_x, scale_y);
	if (!screen.antialias && !screen.correct_aspect_ratio)
	{
		scale = SDL_floorf(scale);
	}
	scale_x = scale_y = scale;
	if (screen.correct_aspect_ratio)
	{
		scale_x *= ASPECT_CORRECT_SCALE_X;
	}
	float w = scale_x * src.w;
	float h = scale_y * src.h;
	dest.x = (dest.w - w) / 2;
	dest.y = (dest.h - h) / 2;
	dest.w = w;
	dest.h = h;

	SDL_RenderCopy(screen.renderer, (screen.prescaled ? screen.prescaled : screen.framebuffer), &src, &dest);
	SDL_RenderPresent(screen.renderer);
}

void initialize(Options::Args& args)
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
	SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "1");

	//Set up SDL screen
	screen.correct_aspect_ratio = args.correct_aspect_ratio;
	screen.crop_overscan = args.crop_overscan;
	screen.antialias = args.antialias;
	screen.window_int_scale = std::clamp(args.int_scale, 1, MAX_WINDOW_INT_SCALE);
	screen.prescale = args.antialias ? PRESCALE_FACTOR : 1;

	char title[64];
	snprintf(title, sizeof(title), "%s %s", PROJECT_DESCRIPTION, PROJECT_VERSION);
	SDL_Point window_size = resize_window(false);
	Uint32 window_opts = 0;
	window_opts |= args.crop_overscan ? 0 : SDL_WINDOW_RESIZABLE;
	window_opts |= args.start_in_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
	screen.window = SDL_CreateWindow(
		title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_size.x, window_size.y, window_opts
	);
	window_size = resize_window(false, 1);
	SDL_SetWindowMinimumSize(screen.window, window_size.x, window_size.y);

	screen.renderer = SDL_CreateRenderer(screen.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	screen.framebuffer = SDL_CreateTexture(
		screen.renderer, SDL_PIXELFORMAT_ARGB1555, SDL_TEXTUREACCESS_STREAMING, DISPLAY_WIDTH, DISPLAY_HEIGHT
	);
	SDL_SetTextureBlendMode(screen.framebuffer, SDL_BLENDMODE_BLEND);
	SDL_SetTextureScaleMode(screen.framebuffer, SDL_ScaleModeNearest);

	if (screen.prescale > 1)
	{
		screen.prescaled = SDL_CreateTexture(
			screen.renderer, SDL_PIXELFORMAT_ARGB1555, SDL_TEXTUREACCESS_TARGET, DISPLAY_WIDTH * screen.prescale,
			DISPLAY_HEIGHT * screen.prescale
		);
		SDL_SetTextureBlendMode(screen.prescaled, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(screen.prescaled, SDL_ScaleModeBest);
	}

	// Allow dropping a ROM onto the window
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

	if (SDL_GameControllerAddMappingsFromFile((RESOURCE_PATH / CONTROLLER_DB_PATH).string().c_str()) < 0)
	{
		Log::warn("Could not load game controller database: %s", SDL_GetError());
		// Nonfatal: continue without the mappings
	}
	open_first_controller();
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

bool load_cart(SystemInfo& config, std::string path)
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

bool load_bios(SystemInfo& config, fs::path path)
{
	std::ifstream bios_file(path, std::ios::binary);
	if (!bios_file.is_open())
	{
		Log::debug("Couldn't load BIOS at %s", path.string().c_str());
		return false;
	}
	Log::info("Located BIOS at %s", path.string().c_str());

	config.bios_rom.assign(std::istreambuf_iterator<char>(bios_file), {});
	bios_file.close();
	return true;
}

bool load_sound_bios(SystemInfo& config, fs::path path)
{
	std::ifstream sound_rom_file(path, std::ios::binary);
	if (!sound_rom_file.is_open())
	{
		Log::debug("Couldn't load Sound BIOS at %s", path.string().c_str());
		return false;
	}
	Log::info("Located Sound BIOS at %s", path.string().c_str());

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
#ifdef __APPLE__
		// On MacOS, look in the folder containing the .app as well as Resources/ in the bundle
		// RESOURCE_PATH = ./LoopyMSE.app/Contents/Resources
		vec.push_back(RESOURCE_PATH / ".." / ".." / ".." / file_path);
#endif
	}
	vec.push_back(file_path);
	return vec;
}

int main(int argc, char** argv)
{
	bool has_quit = false;
	bool is_paused = false;

	SystemInfo config;
	Options::Args args;

	if (!fs::exists(PREFS_PATH / INI_PATH))
	{
		Log::info("Creating default ini file");
		fs::copy_file(RESOURCE_PATH / INI_PATH, PREFS_PATH / INI_PATH);
	}

	Options::parse_config(PREFS_PATH / INI_PATH, args);
	Options::parse_commandline(argc, argv, args);

	config.emulator.image_save_directory = PREFS_PATH;
	config.emulator.screenshot_image_type = args.screenshot_image_type;
	config.emulator.printer_image_type = args.printer_image_type;
	config.emulator.printer_view_command = args.printer_view_command;

	Log::set_level(args.verbose ? Log::VERBOSE : Log::INFO);

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
			"Error: Missing BIOS file. Provide by argument, or place at %s.\n",
			(PREFS_PATH / DEFAULT_BIOS_PATH).string().c_str()
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
			"Missing sound bios file. Provide by argument, or place at %s.\n"
			"Emulation will continue without sound.\n",
			DEFAULT_SOUND_BIOS_PATH.string().c_str()
		);
	}

	if (args.cart.empty())
	{
		Log::info("No ROM provided. Drop a Loopy ROM onto the window to play.");
		// Appears some platforms achieve "open with" not by passing the arguments to the binary but by performing the open file action
		// So don't prohibit starting into fullscreen without a ROM
		// args.start_in_fullscreen = false;
	}
	else if (load_cart(config, args.cart))
	{
		fs::path rom_path = fs::absolute(config.cart.rom_path);
		if (rom_path.has_parent_path())
		{
			config.emulator.image_save_directory = rom_path.parent_path();
		}
		System::initialize(config);
	}
	else
	{
		Log::error("Could not load ROM file.");
		exit(1);
	}

	SDL::initialize(args);

	constexpr int framerate_target = 60;  //TODO: get this from Video if it can be changed (e.g. for PAL mode)
	constexpr int framerate_max_lag = 5;
	int last_frame_ticks = SDL_GetPerformanceCounter();

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
			SDL::update(System::get_display_output(), Video::get_display_scanlines(), Video::get_background_color());
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
						int screenshot_image_type = config.emulator.screenshot_image_type;
						fs::path screenshot_filename(imagew::make_unique_name("loopymse_"));
						screenshot_filename += imagew::image_extension(screenshot_image_type);

						Log::info("Saving screenshot to %s", screenshot_filename.string().c_str());
						Video::dump_current_frame(
							screenshot_image_type, config.emulator.image_save_directory / screenshot_filename
						);

						//Video::dump_all_bmps(screenshot_image_type, config.emulator.image_save_directory);
					}
					break;
				case SDLK_F11:
					SDL::toggle_fullscreen();
					break;
				case SDLK_F12:
					if (config.cart.is_loaded())
					{
						Log::info("Rebooting Loopy...");
						System::shutdown(config);
						System::initialize(config);
						last_frame_ticks = INT_MAX;
					}
					break;
				case SDLK_MINUS:
					if (!SDL::screen.is_fullscreen())
					{
						SDL::change_window_scale(-1);
					}
					break;
				case SDLK_EQUALS:
					if (!SDL::screen.is_fullscreen())
					{
						SDL::change_window_scale(1);
					}
					break;
				case SDLK_ESCAPE:
					if (SDL::is_mouse_captured())
					{
						SDL::capture_mouse(false);
						LoopyIO::set_plugged_controller(CONTROLLER_PAD);
						Input::set_mouse_button_state(SDL_BUTTON_LEFT, false);
						Input::set_mouse_button_state(SDL_BUTTON_RIGHT, false);
						break;
					}
					if (SDL::screen.is_fullscreen())
					{
						SDL::toggle_fullscreen();
						break;
					}
					has_quit = true;
					break;
				default:
					Input::set_key_state(keycode, false);
					break;
				}
				break;
			}
			case SDL_CONTROLLERBUTTONDOWN:
				// Quit on select+start
				if (SDL::controller && SDL_GameControllerGetButton(SDL::controller, SDL_CONTROLLER_BUTTON_START) &&
					SDL_GameControllerGetButton(SDL::controller, SDL_CONTROLLER_BUTTON_BACK))
				{
					has_quit = true;
					break;
				}
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
					if (SDL::is_mouse_captured())
					{
						SDL::capture_mouse(false);
						LoopyIO::set_plugged_controller(CONTROLLER_PAD);
						Input::set_mouse_button_state(SDL_BUTTON_LEFT, false);
						Input::set_mouse_button_state(SDL_BUTTON_RIGHT, false);
					}
					break;
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
				if (!SDL::is_mouse_captured())
				{
					SDL::capture_mouse(true);
					LoopyIO::set_plugged_controller(CONTROLLER_MOUSE);
					break;
				}
				Input::set_mouse_button_state(e.button.button, true);
				break;
			case SDL_MOUSEBUTTONUP:
				Input::set_mouse_button_state(e.button.button, false);
				break;
			case SDL_MOUSEMOTION:
				Input::move_mouse(e.motion.xrel, -e.motion.yrel);
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
				System::shutdown(config);

				std::string path = e.drop.file;
				if (load_cart(config, path))
				{
					Log::info("Loaded %s...", path.c_str());

					fs::path rom_path = fs::absolute(config.cart.rom_path);
					if (rom_path.has_parent_path())
					{
						config.emulator.image_save_directory = rom_path.parent_path();
					}

					System::initialize(config);
					// So you can tell that MSE is running even before you click into it
					is_paused = false;
					last_frame_ticks = INT_MAX;
				}
				break;
			}
		}
	}

	System::shutdown(config);
	SDL::shutdown();

	return 0;
}
