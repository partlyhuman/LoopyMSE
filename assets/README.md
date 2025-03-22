# Loopy My Seal Emulator

## Prerequisites

LoopyMSE requires a Loopy BIOS and, for sound emulation, the Loopy sound BIOS is required. It expects the files to be named bios.bin and soundbios.bin, and looks for them:

- Where the .app or .exe lives,
- `~/Library/Application Support/PSI/LoopyMSE/` on MacOS

Or you can provide a different name or an absolute path in your `loopymse.ini` configuration or on the command line.

## Configuration

Find the configuration `loopymse.ini` either:

- Next to the .exe on Windows
- In `~/Library/Application Support/PSI/LoopyMSE/` on MacOS (after first run)

You can use the settings here to change your keyboard or controller bindings as well as certain options.

## Running

You can run LoopyMSE several ways:

- Opening the .exe or .app. If the BIOS can be found, you'll get an empty window, into which you can drag a Loopy ROM to play. If nothing happens, it may have failed to find the BIOS.
- Dragging a Loopy ROM onto the .exe or .app. You can pin the app to your Dock / Taskbar.
- On MacOS, the .loopy file extension will be associated with LoopyMSE and you can open Loopy ROMs in LoopyMSE by renaming them .loopy and simply opening them.
- On Windows, you can associate the .loopy file extension with LoopyMSE.
- It can be launched via the command line. On MacOS and Linux, you can get logging this way.

Launching via commandline:
- On MacOS, the binary is in `LoopyMSE.app/Contents/MacOS/LoopyMSE`.
- Use `LoopyMSE --help` for information on commandline parameters.

## Playing Loopy My Seal Emulator

You can use the keyboard or a gamepad / controller to emulate controller input. The Loopy Mouse is not yet emulated.

Default controls (remap in `loopymse.ini`):

| Loopy | Keyboard |
| ----- | -------- |
| A     | Z        |
| B     | X        |
| C     | C        |
| D     | V        |
| L     | A        |
| R     | S        |
| Start | Enter    |

Additionally, these special functions are available.

| Function    | Key |
| ----------- | --- |
| Screenshot  | F10 |
| Fullscreen  | F11 |
| Reboot      | F12 |
| Exit        | Esc |

## MacOS Security

LoopyMSE is not signed or notarized, so you will only be able to run it if you "Allow Applications From App Store & Known Developers" in System Preferences > Privacy & Security.

If the system says "Apple could not verify “LoopyMSE.app” is free of malware that may harm your Mac or compromise your privacy.", click "Done", open System Preferences > Privacy & Security and click "Open Anyway" and then "Open Anyway" again.

If you download a build from GitHub, MacOS "quarantines" it as an unknown download. If it still cannot run, try clearing the quarantine attribute with `xattr -r -d com.apple.quarantine LoopyMSE.app`.

**Please, only do the above steps if you know what you are doing, and you trust this executable.**

## Wanwan Expansion Audio

Currently, to emulate expansion PCM audio on Wanwan Aijou Monogatari, place numbered .wav files in a `pcm/` directory next to the Wanwan ROM.