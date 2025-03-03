#include "expansion.h"

#include <memory>
#include <vector>

#include "expansion/msm665x/msm665x.h"
#include "log/log.h"

namespace Expansion
{

void initialize(Config::CartInfo& cart)
{
	if (!cart.is_loaded()) return;

	const size_t CHECKSUM_OFFSET = 8;
	if (CHECKSUM_OFFSET + sizeof(uint32_t) > cart.rom.size()) return;
	// uint32_t checksum = *reinterpret_cast<const uint32_t*>(&cart.rom[CHECKSUM_OFFSET]);
	// uint32_t checksum = reinterpret_cast<std::vector<uint32_t>>(cart.rom)[CHECKSUM_OFFSET/sizeof(uint32_t)];

	uint32_t checksum = cart.rom[CHECKSUM_OFFSET] << 24 | cart.rom[CHECKSUM_OFFSET + 1] << 16 |
						cart.rom[CHECKSUM_OFFSET + 2] << 8 | cart.rom[CHECKSUM_OFFSET + 3];

	if (MSM665X::enable(checksum))
	{
		MSM665X::initialize(cart.rom_path);
	}
}

void shutdown()
{
	MSM665X::shutdown();
}

void unmapped_write8(uint32_t addr, uint8_t value)
{
	MSM665X::unmapped_write8(addr, value);
}

}  // namespace Expansion