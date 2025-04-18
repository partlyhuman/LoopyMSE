cmake_minimum_required (VERSION 3.12)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

project (LoopyMSE LANGUAGES C CXX ASM)
set (PROJECT_VERSION 0.4.0)
set (PROJECT_DESCRIPTION "Loopy My Seal Emulator")
set (PROJECT_HOMEPAGE_URL "https://github.com/PSI-Rockin/LoopyMSE")
set (PROJECT_ORG "PSI")

string(REPLACE "/DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

cmake_policy(SET CMP0167 NEW)

# Boost program_options package, statically linked
set(Boost_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)
find_package(Boost REQUIRED COMPONENTS program_options)

# SDL2, statically linked
add_compile_definitions (SDL_MAIN_HANDLED) # Prevents SDL from touching main()
set (SDL_STATIC ON CACHE BOOL "" FORCE)
set (SDL_SHARED OFF CACHE BOOL "" FORCE)
set (SDL_TEST OFF CACHE BOOL "" FORCE)
find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)
include_directories(LoopyMSE PRIVATE ${SDL2_INCLUDE_DIRS})

set (DIST_DIR ${CMAKE_BINARY_DIR}/dist)
set (ASSETS_DIR ${PROJECT_SOURCE_DIR}/assets)

add_subdirectory (src)