add_executable (LoopyMSE
				"main.cpp")

target_link_libraries (LoopyMSE PRIVATE core SDL2-static)

set_target_properties (LoopyMSE PROPERTIES WIN32_EXECUTABLE YES)
