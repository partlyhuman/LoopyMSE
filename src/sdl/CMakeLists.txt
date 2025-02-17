add_executable (LoopyMSE
				"options.h"
				"options.cpp"
				"main.cpp")

if (WIN32)
	target_sources (LoopyMSE PRIVATE "windows.rc")
	target_link_options (LoopyMSE PRIVATE -mwindows -static)
endif()


target_link_libraries (LoopyMSE PRIVATE core SDL2::SDL2-static Boost::program_options)
