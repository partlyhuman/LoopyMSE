#pragma once
#include "expansion/expansion.h"

namespace Expansion
{

class MSM665X : public IExpansion
{
	bool is_enabled() const override;
};

}  // namespace Expansion