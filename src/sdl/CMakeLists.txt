add_executable (LoopyMSE
				"config.h"
				"options.h"
				"options.cpp"
				"main.cpp")

target_compile_definitions(LoopyMSE PRIVATE 
	PROJECT_NAME="${PROJECT_NAME}"
	PROJECT_ORG="${PROJECT_ORG}"
	PROJECT_DESCRIPTION="${PROJECT_DESCRIPTION}"
	PROJECT_VERSION="${PROJECT_VERSION}"
)

if (WIN32)
	target_sources(LoopyMSE PRIVATE "windows.rc")
	target_link_options(LoopyMSE PRIVATE -mwindows -static)
endif()

if (APPLE)
	set(CMAKE_MACOSX_BUNDLE ON)

	set(BUNDLE_RESOURCES_DIR "LoopyMSE.app/Contents/Resources")
	set(ICON_NAME "macos.icns")
	set(ICON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${ICON_NAME})
	set_source_files_properties(${ICON_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	file(COPY ${ICON_PATH} DESTINATION ${BUNDLE_RESOURCES_DIR})
	
	file(GLOB RESOURCE_FILES "${ASSETS_DIR}/*")
	file(COPY ${RESOURCE_FILES} DESTINATION ${BUNDLE_RESOURCES_DIR})

	set_target_properties(LoopyMSE PROPERTIES MACOSX_BUNDLE ON)
	set_target_properties(LoopyMSE PROPERTIES MACOSX_BUNDLE_ICONFILE ${ICON_NAME})
	set_target_properties(LoopyMSE PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
endif()

target_link_libraries (LoopyMSE PRIVATE core SDL2::SDL2-static SDL2_mixer::SDL2_mixer-static Boost::program_options)

# INSTALLATION

install (TARGETS LoopyMSE
		RUNTIME DESTINATION ${DIST_DIR}
		BUNDLE DESTINATION ${DIST_DIR})

if (WIN32)
install (DIRECTORY ${ASSETS_DIR}/
		DESTINATION ${DIST_DIR}
		FILES_MATCHING PATTERN "*")
install (CODE "file(TOUCH \"${DIST_DIR}/__PLACE_BIOS.BIN_HERE__\")")
install (CODE "file(TOUCH \"${DIST_DIR}/__PLACE_SOUNDBIOS.BIN_HERE__\")")
endif()
