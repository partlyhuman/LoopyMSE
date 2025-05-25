#include "config.h"

namespace Config
{

const char* controller_type_str(ControllerType type)
{
	switch (type)
	{
	case CONTROLLER_NONE:
		return "No controller";
	case CONTROLLER_PAD:
		return "Pad";
	case CONTROLLER_MOUSE:
		return "Mouse";
	}
}

}  // namespace Config