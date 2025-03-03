#include "msm665x.h"

#include <SDL2/SDL.h>

#include <filesystem>
#include <iostream>
#include <unordered_set>

#include "audio.h"
#include "log/log.h"

#define LIMIT_TO_KNOWN_CARTS
#define DEFAULT_VOL 64

std::unordered_set<uint32_t> EXPANSION_CARTS = {
	0xD90FE762,	 // Wanwan Aijou Monogatari
	0x11EEBE7A,	 // Wanwan-T-En
};

namespace fs = std::filesystem;
namespace Expansion::MSM665X
{

bool enabled = false;
std::vector<Audio*> sounds;

bool enable(uint32_t cart_checksum)
{
#ifdef LIMIT_TO_KNOWN_CARTS
	enabled = EXPANSION_CARTS.count(cart_checksum) > 0;
	Log::info("[MSM665] enabled for cart %X? %d\n", cart_checksum, enabled);
	return enabled;
#else
	return true;
#endif
}

void initialize(std::string rom_path_str)
{
	fs::path rom_path(rom_path_str);
	if (rom_path.has_parent_path())
	{
		rom_path = rom_path.parent_path();
	}
	else
	{
		rom_path = ".";
	}
	fs::path pcm_path = rom_path / "pcm";
	if (!fs::is_directory(pcm_path)) return;
	Log::debug("[MSM665] found pcm path %s", pcm_path.string().c_str());

	std::vector<fs::path> wavs;
	for (auto const& dir_entry : std::filesystem::directory_iterator(pcm_path))
	{
		if (!dir_entry.is_regular_file() || dir_entry.path().extension().string() != ".wav") continue;
		wavs.push_back(dir_entry.path());
	}
	std::sort(wavs.begin(), wavs.end());

	if (wavs.empty()) return;

	// We definitely have audio to play
	if (SDL_Init(SDL_INIT_AUDIO) < 0)
	{
		Log::error("[MSM665] SDL audio init failed: %s", SDL_GetError());
		return;
	}
	initAudio();

	sounds.clear();
	for (auto const& wav : wavs)
	{
		Log::debug("[MSM665] Loading %s into sound index %d", wav.string().c_str(), sounds.size());
		sounds.push_back(createAudio(wav.string().c_str(), 0, DEFAULT_VOL));
	}
}

void shutdown()
{
	for (const auto& thing : sounds)
	{
		freeAudio(thing);
	}
	sounds.clear();
	endAudio();
}

void unmapped_write8(uint32_t addr, uint8_t value)
{
	if (!enabled) return;
	if (addr >> 16 != 0x040A) return;

	static uint8_t cmd_status = 0;
	uint8_t data = value & 0x7F;

	if (!data) return;

	if (cmd_status == 0)
	{
		uint8_t ctype = data >> 5;
		switch (ctype)
		{
		case 0:
			// msm.option_set(data);
			break;
		case 3:
			// msm.voice_control(data);
			cmd_status = 3;
			break;
		}
	}
	else if (cmd_status == 3)
	{
		if (data > 0)
		{
			// OKI sounds are 1-indexed
			int index = data - 1;
			if (index >= 0 && index < sounds.size())
			{
				Log::debug("[MSM665] Play sample %d", data);
				playSoundFromMemory(sounds[index], DEFAULT_VOL);
			}
			else
			{
				Log::warn("[MSM665] Sample %d out of range [1-%d]", index, sounds.size());
			}
		}
		else
		{
			Log::debug("[MSM665] Stop");
			// msm.stop_sound();
		}
		cmd_status = 0;
	}
}

}  // namespace Expansion::MSM665X