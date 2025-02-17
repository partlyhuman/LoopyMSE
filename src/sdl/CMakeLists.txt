add_executable (LoopyMSE
				"main.cpp")

set_target_properties (LoopyMSE PROPERTIES WIN32_EXECUTABLE YES)

target_link_libraries (LoopyMSE PRIVATE core SDL2-static Boost::program_options)
