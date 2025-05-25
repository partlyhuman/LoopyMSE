#include "expansion.h"

#include <memory>
#include <vector>

#include "expansion/msm665x/msm665x.h"
#include "log/log.h"

namespace Expansion
{

std::unordered_set<uint32_t> MOUSE_CARTS = {
	0xA5341F72,	 // Chakrakun no Omajinai Paradise
	0xF294F931,	 // Pasokon Collection
	0x6A410BB2,	 // Little Romance
	0x8373E9DD,	 // Loopy Town no Oheya ga Hoshii!
	0x6E00CE71,	 // Lupiton no Wonder Palette
};

std::unordered_set<uint32_t> MSM665X_CARTS = {
	0xD90FE762,	 // Wanwan Aijou Monogatari
	0xB5BE48D7,	 // Wanwan-T-En (v1.0)
};

void initialize(Config::CartInfo& cart)
{
	if (!cart.is_loaded()) return;

	// Use cart checksum from header as cart ID
	uint32_t checksum = get_cart_header_checksum(cart);
	// Conditionally turn on cart expansions depending on the inserted cart
	if (MSM665X::enable(checksum)) MSM665X::initialize(cart.rom_path);
}

void shutdown()
{
	if (MSM665X::is_enabled()) MSM665X::shutdown();
}

uint32_t get_cart_header_checksum(Config::CartInfo& cart)
{
	const size_t CHECKSUM_OFFSET = 8;
	if (CHECKSUM_OFFSET + sizeof(uint32_t) > cart.rom.size())
	{
		Log::warn("[Expansion] ROM is too small to contain a valid header");
		return -1;
	}
	return cart.rom[CHECKSUM_OFFSET] << 24 | cart.rom[CHECKSUM_OFFSET + 1] << 16 | cart.rom[CHECKSUM_OFFSET + 2] << 8 |
		   cart.rom[CHECKSUM_OFFSET + 3];
}

void unmapped_write8(uint32_t addr, uint8_t value)
{
}

uint8_t exp_read8(uint32_t addr)
{
	Log::warn("[Expansion] unmapped read8 %08X", addr);

	return 0;
}
uint16_t exp_read16(uint32_t addr)
{
	Log::warn("[Expansion] unmapped read16 %08X", addr);

	return 0;
}
uint32_t exp_read32(uint32_t addr)
{
	Log::warn("[Expansion] unmapped read32 %08X", addr);

	return 0;
}
void exp_write8(uint32_t addr, uint8_t value)
{
	if (MSM665X::is_enabled())
	{
		MSM665X::write8(addr, value);
	}
	else
	{
		Log::warn("[Expansion] unmapped write8 %08X: %02X", addr, value);
	}
}
void exp_write16(uint32_t addr, uint16_t value)
{
	Log::warn("[Expansion] unmapped write16 %08X: %04X", addr, value);
}
void exp_write32(uint32_t addr, uint32_t value)
{
	Log::warn("[Expansion] unmapped write32 %08X: %08X", addr, value);
}

}  // namespace Expansion