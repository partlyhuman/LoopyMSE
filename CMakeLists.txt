cmake_minimum_required (VERSION 3.8)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

project (LoopyMSE LANGUAGES C CXX ASM)

string(REPLACE "/DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

cmake_policy(SET CMP0167 NEW)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED COMPONENTS program_options)

add_compile_definitions (SDL_MAIN_HANDLED) # Prevents SDL from touching main()
set (SDL_STATIC ON CACHE BOOL "" FORCE)
set (SDL_SHARED OFF CACHE BOOL "" FORCE)
set (SDL_TEST OFF CACHE BOOL "" FORCE)
find_package(SDL2 REQUIRED)
include_directories(LoopyMSE PRIVATE ${SDL2_INCLUDE_DIRS})

if (WIN32)
	# even link in C runtime on windows (for msys-mingw-like build toolchains)
	set(CMAKE_EXE_LINKER_FLAGS "-static")
endif()

add_subdirectory (src)
# add_subdirectory (externals/SDL)

set (DIST_DIR "${CMAKE_BINARY_DIR}/dist")
set (ASSETS_DIR "${CMAKE_INSTALL_NAME_DIR}/../assets")

install (TARGETS LoopyMSE
        RUNTIME DESTINATION ${DIST_DIR})

install (DIRECTORY "assets/"
        DESTINATION ${DIST_DIR}
        FILES_MATCHING PATTERN "*")
