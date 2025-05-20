#pragma once

namespace Input
{

enum PadButton
{
	PAD_PRESENCE = 0x0001,
	PAD_START = 0x0002,
	PAD_L1 = 0x0004,
	PAD_R1 = 0x0008,

	PAD_A = 0x0010,
	PAD_D = 0x0020,
	PAD_C = 0x0040,
	PAD_B = 0x0080,

	PAD_UP = 0x0100,
	PAD_DOWN = 0x0200,
	PAD_LEFT = 0x0400,
	PAD_RIGHT = 0x0800
};

enum MouseButton
{
	MOUSE_L = 0x1000,
	MOUSE_R = 0x4000
};

void initialize();
void shutdown();

void set_key_state(int key, bool pressed);
void set_controller_state(int key, bool pressed);
void set_mouse_button_state(int button, bool pressed);
void move_mouse(int delta_x, int delta_y);

void add_key_binding(int key, PadButton pad_button);
void add_controller_binding(int key, PadButton pad_button);

}