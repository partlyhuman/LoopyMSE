#pragma once
#include "core/config.h"

namespace Expansion
{
class IExpansion
{
   public:
	virtual bool is_enabled() const = 0;
	virtual void initialize(Config::CartInfo& cart) = 0;
	virtual void shutdown() = 0;
	// virtual uint8_t unmapped_read8(uint32_t addr);
	// virtual uint16_t unmapped_read16(uint32_t addr);
	// virtual uint32_t unmapped_read32(uint32_t addr);
	virtual void unmapped_write8(uint32_t addr, uint8_t value) = 0;
	// virtual void unmapped_write16(uint32_t addr, uint16_t value);
	// virtual void unmapped_write32(uint32_t addr, uint32_t value);
};

void initialize(Config::CartInfo& cart);
void shutdown();
void unmapped_write8(uint32_t addr, uint8_t value);

}  // namespace Expansion