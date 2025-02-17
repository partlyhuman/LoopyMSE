cmake_minimum_required (VERSION 3.8)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)
if (WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "-static")
endif()

project (LoopyMSE LANGUAGES C CXX ASM)

string(REPLACE "/DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

add_compile_definitions (SDL_MAIN_HANDLED) # Prevents SDL from touching main()

set (SDL_STATIC ON CACHE BOOL "" FORCE)
set (SDL_SHARED OFF CACHE BOOL "" FORCE)
set (SDL_TEST OFF CACHE BOOL "" FORCE)

# Find Boost's program_options
cmake_policy(SET CMP0167 NEW)
find_package(Boost REQUIRED COMPONENTS program_options)

add_subdirectory (src)
add_subdirectory (externals/SDL)

set (DIST_DIR "${CMAKE_BINARY_DIR}/dist")
set (ASSETS_DIR "${CMAKE_INSTALL_NAME_DIR}/../assets")

install (TARGETS LoopyMSE
        RUNTIME DESTINATION ${DIST_DIR})

install (DIRECTORY "assets/"
        DESTINATION ${DIST_DIR}
        FILES_MATCHING PATTERN "*")
