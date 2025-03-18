#include "msm665x.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <unordered_set>

#include "log/log.h"
#include "sound/sound.h"

// define this to only run on specific carts by header
// in this stage of development, no harm turning it on always
#undef LIMIT_TO_KNOWN_CARTS

std::unordered_set<uint32_t> EXPANSION_CARTS = {
	0xD90FE762,	 // Wanwan Aijou Monogatari
	0x11EEBE7A,	 // Wanwan-T-En (subject to change)
};

namespace fs = std::filesystem;
namespace Expansion::MSM665X
{

bool enabled = false;
bool is_enabled()
{
	return enabled;
}

std::vector<fs::path> wavs;

double computed_volume = 1;
uint8_t op_v = 0;
uint8_t op_s = 0;
uint8_t op_a = 0;
uint8_t vc_vl = 0;
uint8_t vc_rp = 0;
uint8_t vc_sm = 0;

bool enable(uint32_t cart_checksum)
{
#ifdef LIMIT_TO_KNOWN_CARTS
	enabled = EXPANSION_CARTS.count(cart_checksum) > 0;
	Log::info("[MSM665] enabled for cart %X? %d\n", cart_checksum, enabled);
#else
	enabled = true;
#endif
	return enabled;
}

void reset_params()
{
	computed_volume = 1;
	op_v = 0;
	op_s = 0;
	op_a = 0;
	vc_vl = 0;
	vc_rp = 0;
	vc_sm = 0;
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

	for (auto const& dir_entry : std::filesystem::directory_iterator(pcm_path))
	{
		if (!dir_entry.is_regular_file() || dir_entry.path().extension().string() != ".wav") continue;
		wavs.push_back(dir_entry.path());
	}
	std::sort(wavs.begin(), wavs.end());

	if (wavs.empty()) return;
	reset_params();
}

void shutdown()
{
	if (!enabled || wavs.empty()) return;
	wavs.clear();
	enabled = false;
}

void option_set(uint8_t data)
{
	op_v |= data & 0x1;
	op_s |= (data >> 2) & 0x1;
	op_a |= (data >> 3) & 0x1;
	computed_volume = (op_v ? 0.5 : 1.0) * SDL_pow(0.5, vc_vl);

	// Log::trace("[MSM665] option_set 0x%X op_v=%d op_s=%d op_a=%d", data, op_v, op_s, op_a);

	Log::debug(
		"[MSM665] Option set VOL=%s STANDBY=%s AOUT=%s", op_v ? "HALF" : "FULL", op_s ? "N" : "Y", op_a ? "DAC" : "LPF"
	);
	if (!op_s)
	{
		Log::warn("[MSM665] Standby not implemented");
	}
}

void voice_control(uint8_t data)
{
	static const char* const VOLUME_STRS[] = {"0dB", "-6dB", "-12dB", "-18dB"};
	static const char* const REPEAT_STRS[] = {"1", "2", "4", "INF"};

	vc_vl = data & 0x3;
	vc_rp = (data >> 2) & 0x3;
	vc_sm = (data >> 4) & 0x1;
	// Log::trace("[MSM665] voice_control 0x%X vc_vl=%d vc_rp=%d vc_sm=%d", data, vc_vl, vc_rp, vc_sm);

	computed_volume = (op_v ? 0.5 : 1.0) * SDL_pow(0.5, vc_vl);
	// self.computed_volume = (0.5 if self.op_v else 1.0) * pow(0.5, self.vc_vl)
	Log::debug(
		"[MSM665] Voice control set VOL=%s REPEAT=%s SMOOTH=%s", VOLUME_STRS[vc_vl], REPEAT_STRS[vc_rp],
		vc_sm ? "Y" : "N"
	);
	if (vc_rp || vc_sm)
	{
		Log::warn("[MSM665] repeat/smooth not implemented");
	}
}

void write8(uint32_t addr, uint8_t value)
{
	if (!enabled) return;
	if (addr >> 16 != 0x040A) return;

	static uint8_t cmd_status = 0;
	uint8_t data = value & 0x7F;

	if (!data)
	{
		reset_params();
		return;
	}

	if (cmd_status == 0)
	{
		uint8_t ctype = data >> 5;
		switch (ctype)
		{
		case 0:
			option_set(data);
			break;
		case 3:
			voice_control(data);
			// get ready for play
			cmd_status = 3;
			break;
		}
	}
	else if (cmd_status == 3)
	{
		cmd_status = 0;
		if (data > 0)
		{
			// OKI sounds are 1-indexed
			int index = data - 1;
			if (index >= 0 && index < wavs.size())
			{
				Log::debug("[MSM665] Play sample %d", data);
				Sound::wav_play(wavs[index], computed_volume);
			}
			else
			{
				Log::warn("[MSM665] Sample %d out of range [1-%d]", index, wavs.size());
			}
		}
		else
		{
			Log::debug("[MSM665] Stop");
			Sound::wav_stop();
		}
	}
}

}  // namespace Expansion::MSM665X