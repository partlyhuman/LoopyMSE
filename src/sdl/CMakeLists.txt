add_executable (LoopyMSE
				"main.cpp")

if (WIN32)
	target_sources(LoopyMSE PRIVATE "windows.rc")
	set(CMAKE_EXE_LINKER_FLAGS "-static")
	set_target_properties (LoopyMSE PROPERTIES WIN32_EXECUTABLE YES)
endif()


target_link_libraries (LoopyMSE PRIVATE core SDL2::SDL2-static Boost::program_options)
