add_library (video STATIC
			 "render.cpp"
			 "render.h"
			 "vdp_local.h"
			 "video.cpp"
			 "video.h")

target_link_libraries (video PRIVATE log)