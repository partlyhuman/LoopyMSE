add_library (sound STATIC
			 "sound.cpp"
			 "sound.h"
			 "loopysound.cpp"
			 "loopysound.h")

target_link_libraries (sound PRIVATE log SDL2::SDL2-static)
