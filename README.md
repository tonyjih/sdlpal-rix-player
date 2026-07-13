# SDLPAL RIX Music Player

[繁體中文說明](README.zh-TW.md)

An unofficial, lightweight SDL3 music player for the RIX tracks stored in the
original PAL / *The Legend of Sword and Fairy* `MUS.MKF` archive.

It uses the RIX sequencer and OPL emulation code bundled with SDLPAL, preserves
the raw music IDs used by the game, and produces a self-contained Windows EXE
by default.

> This project is not affiliated with or endorsed by Softstar Entertainment or
> the SDLPAL project. No game assets or music are included. Supply `MUS.MKF`
> from a legally obtained copy of the game.

## Features

- Opens `MUS.MKF` and standalone `.rix` files
- Preserves raw MKF/SDLPAL music IDs
- Skips empty MKF entries during navigation
- Drag-and-drop loading
- Previous/next, +/-10, first/last, and direct numeric track selection
- Pause, loop, restart, volume control, elapsed time, and level meter
- Buffered SDL3 audio queue for stable Windows playback
- Automatically loads `MUS.MKF` beside the EXE
- Falls back to `GamePath` in a neighboring `sdlpal.cfg`
- No extra console window on Windows
- Static Windows build without SDL3 or MinGW sidecar DLLs

## Requirements

- A current SDLPAL source checkout
- SDL 3 development package
- CMake 3.20 or newer
- Ninja
- A C/C++ compiler with C++17 support

The public release was developed and tested against SDLPAL source around commit
`fb781fc19b61b36e4b05e7dca1eeb61cd287f2f3`.

## Windows build with MSYS2 UCRT64

Install the toolchain:

```sh
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-sdl3 \
  mingw-w64-ucrt-x86_64-pkgconf
```

Clone this repository anywhere and point it to SDLPAL:

```sh
cd /d/sdlpal-rix-player
./build-msys2.sh /d/sdlpal-src
```

Equivalent manual commands:

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DRIX_PLAYER_STATIC=ON \
  -DSDLPAL_ROOT=D:/sdlpal-src
cmake --build build
```

Output:

```text
build/sdlpal-rix-player.exe
```

The default static build does not require `SDL3.dll`, `libstdc++-6.dll`,
`libgcc_s_seh-1.dll`, or `libwinpthread-1.dll` beside the EXE. Ordinary Windows
system DLLs are still used.

To verify:

```sh
objdump -p build/sdlpal-rix-player.exe | grep "DLL Name"
```

## Alternative embedded layout

The original overlay layout is still supported. Clone this repository as:

```text
sdlpal/
  adplug/
  sdl_compat/
  tools/
    rix-player/   <- this repository
```

Then `SDLPAL_ROOT` is detected automatically.

## Run

```sh
sdlpal-rix-player.exe D:/PAL95/MUS.MKF
sdlpal-rix-player.exe D:/PAL95/MUS.MKF --track 42
sdlpal-rix-player.exe D:/PAL95/MUS.MKF --no-loop
```

With no file argument, the player searches:

1. `MUS.MKF` or `mus.mkf` beside the EXE.
2. `sdlpal.cfg` beside the EXE, then `GamePath/MUS.MKF`.

Example:

```ini
GamePath=./PAL95/
```

## Controls

| Key | Action |
|---|---|
| Left / Right | Previous / next playable raw track ID |
| Page Up / Page Down | Move 10 playable entries |
| Home / End | First / last playable entry |
| Digits + Enter | Jump to an exact raw track ID |
| Space | Pause / resume |
| L | Toggle looping |
| Up / Down | Volume +/-5% |
| F5 | Restart current track |
| Escape | Quit |

## Releases

Do not include `MUS.MKF`, `.rix` files, or other game data in the repository or
release assets. Because the Windows EXE statically incorporates code from the
selected SDLPAL checkout, a binary release should also provide the exact
corresponding SDLPAL source used for that build. The safest workflow is:

```sh
./package-source-bundle.sh /d/sdlpal-src 0.1.0
```

Attach the generated `source-with-sdlpal.tar.gz` beside the static EXE on the
GitHub Release page. The script exports only tracked Git files, records both
commit IDs, and does not copy local game data.

## License

This project is released under GPL-3.0-or-later. See [LICENSE](LICENSE) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
