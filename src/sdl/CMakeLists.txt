add_executable (LoopyMSE
				"options.h"
				"options.cpp"
				"main.cpp")

if (WIN32)
	target_sources(LoopyMSE PRIVATE "windows.rc")
	target_link_options(LoopyMSE PRIVATE -mwindows -static)
endif()

if (APPLE)
	set(CMAKE_MACOSX_BUNDLE ON)

	set(ICON_NAME "macos.icns")
	set(ICON_PATH "${PROJECT_SOURCE_DIR}/src/sdl/${ICON_NAME}")
	set_source_files_properties(${ICON_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	file(COPY ${ICON_PATH} DESTINATION "LoopyMSE.app/Contents/Resources")

	set_target_properties(LoopyMSE PROPERTIES MACOSX_BUNDLE ON)
	set_target_properties(LoopyMSE PROPERTIES MACOSX_BUNDLE_ICONFILE ${ICON_NAME})
	set_target_properties(LoopyMSE PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/src/sdl/Info.plist")
endif()

target_link_libraries (LoopyMSE PRIVATE core SDL2::SDL2-static Boost::program_options)
