NOTE: This is a work in progress and will be added to the readme when done.

## Building - Windows

Get MSYS2 and install the MINGW64 toolchain, or add it to an existing installation with `pacman -S mingw-w64-x86_64-toolchain`

Install SDL2 and Boost: `pacman -S mingw-w64-x86_64-SDL2 mingw-w64-x86_64-boost`

For ease of building, recommended to install [VS Code](https://code.visualstudio.com/), and use the [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) extension.

In your CMake Tools [Additional Compiler Search Dirs](vscode://settings/cmake.additionalCompilerSearchDirs), ensure your (root) MSYS2 folder is included, e.g. `C:/msys64`.

Locate the CMake extension panel, and under *Configure*, select the pencil icon, or use the `CMake: Select a Kit` command. There should be an entry for mingw64, e.g. `GCC 14.2.0 x86_64-w64-mingw32 (mingw64)`. Select that. CMake should configure itself then. If this succeeds, look one line down in *Configure* or use `CMake: Select Variant` command select the `Release` configuration.

You should be able to make builds with the `CMake: Build` command or the build keyboard shortcut. To make a distributable build, use `CMake: Install` instead. The end product will reside in `build/dist`.

## Building - MacOS

To get prerequisites, use [Homebrew](https://brew.sh) to install SDL2 and Boost: `brew install boost sdl2`. If homebrew is correctly set up, the C++ build tools should all be available.

(NOTE: is it necessary to have cmake installed, or will VSCode install its own version?)

Follow the steps above to setup builds in VS Code, choosing the (TODO) kit.