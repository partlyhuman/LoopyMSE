#include "msm665x.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include <filesystem>
#include <unordered_set>

#include "log/log.h"

std::unordered_set<uint32_t> EXPANSION_CARTS = {
	0xD90FE762,	 // Wanwan Aijou Monogatari
	0x11EEBE7A,	 // Wanwan-T-En
};

namespace fs = std::filesystem;
namespace Expansion::MSM665X
{

bool enabled = false;
std::vector<int> sounds;

bool enable(uint32_t cart_checksum)
{
	enabled = EXPANSION_CARTS.count(cart_checksum) > 0;
	Log::info("[MSM665X] enabled for cart %X? %d\n", cart_checksum, enabled);
	return enabled;
}

void initialize(std::string rom_path)
{
	fs::path pcm_path(rom_path);
	if (!pcm_path.has_parent_path()) return;
	pcm_path = pcm_path.parent_path() / "pcm";
	if (!fs::is_directory(pcm_path)) return;
	Log::warn("[MSM665X] found pcm path");
	// loop through

	if (SDL_Init(SDL_INIT_AUDIO) < 0)
	{
		Log::error("[MSM665X] SDL audio init failed: %s", SDL_GetError());
		return;
	}

	if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 1, 2) < 0)
	{
		printf("[MSM665X] Mixer init failed: %s\n", Mix_GetError());
		return;
	}
}

void shutdown()
{
	Mix_CloseAudio();
}

}  // namespace Expansion::MSM665X