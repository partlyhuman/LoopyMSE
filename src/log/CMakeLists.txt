add_library (log STATIC
			 "log.cpp"
			 "log.h"
			)

target_link_libraries (log PRIVATE SDL2-static)