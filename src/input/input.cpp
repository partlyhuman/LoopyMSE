#include "input/input.h"

#include <core/loopy_io.h>

#include <unordered_map>

namespace Input
{

static std::unordered_map<int, PadButton> key_bindings;
static std::unordered_map<int, PadButton> controller_bindings;

void initialize()
{
	//Indicate the gamepad is connected
	LoopyIO::set_controller_plugged(true, false);
}

void shutdown()
{
	//nop
}

void set_controller_state(int button, bool pressed)
{
	auto binding = controller_bindings.find(button);
	if (binding == controller_bindings.end())
	{
		return;
	}

	PadButton pad_button = binding->second;
	LoopyIO::update_pad(pad_button, pressed);
}

void set_key_state(int key, bool pressed)
{
	auto binding = key_bindings.find(key);
	if (binding == key_bindings.end())
	{
		return;
	}

	PadButton pad_button = binding->second;
	LoopyIO::update_pad(pad_button, pressed);
}

void set_mouse_button_state(int button, bool pressed)
{
	// TODO: replace the hardcoded 1 and 3 with SDL constants or bindings?
	if (button == 1)
	{
		LoopyIO::update_mouse_buttons(MOUSE_L, pressed);
	}
	if (button == 3)
	{
		LoopyIO::update_mouse_buttons(MOUSE_R, pressed);
	}
}

void move_mouse(int delta_x, int delta_y)
{
	LoopyIO::update_mouse_position(delta_x, delta_y);
}

void add_key_binding(int code, PadButton pad_button)
{
	key_bindings.emplace(code, pad_button);
}

void add_controller_binding(int code, PadButton pad_button)
{
	controller_bindings.emplace(code, pad_button);
}

}  // namespace Input