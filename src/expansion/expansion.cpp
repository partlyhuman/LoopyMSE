#include "expansion.h"

#include <memory>
#include <vector>

#include "expansion/msm665x/msm665x.h"
#include "log/log.h"

namespace Expansion
{

std::vector<std::unique_ptr<IExpansion>> expansions = {
	std::make_unique<MSM665X>(),
};

void initialize(Config::CartInfo& cart)
{
	for (const auto& expansion : expansions)
	{
		expansion->initialize(cart);
	}
}

void shutdown()
{
	for (const auto& expansion : expansions)
	{
		expansion->shutdown();
	}
}

void unmapped_write8(uint32_t addr, uint8_t value)
{
	for (const auto& expansion : expansions)
	{
		expansion->unmapped_write8(addr, value);
	}
}

}  // namespace Expansion