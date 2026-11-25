# Woof! 16.0.0 for NextUI (`DOOM.pak`)

A standalone ARM64 port of Fabian Greffrath's [Woof!](https://github.com) (16.0.0) for **NextUI** handhelds (`h700`, `tg5040`, `tg4040`, `tg5050`), featuring Boom, MBF, MBF21, and ID24 compatibility (`Legacy of Rust`, `Sigil`, `BTSX`, `REKKR`, `No Rest for the Living`, and more).

## Features
- **Zero-dependency SDL3 fbdev backend:** Renders directly to `/dev/fb0` in crisp 4:3 aspect ratio with automatic 90-degree rotation on portrait screens (Anbernic RG28XX) and native landscape scaling with ARGB8888 unblanking on TrimUI Brick / Smart Pro / RG35XX / RG40XX.
- **Automatic Hardware Input Detection:** Automatically detects Anbernic (`h700`) and TrimUI (`TRIMUI Player1`) devices and maps buttons, triggers, and dual analog sticks (Left Stick = Move/Strafe, Right Stick = Look).
- **`.doom` Mod Launcher Support:** Launch IWADs directly or create `.doom` shell scripts for multi-WAD and `.deh` mods.
- **Instant Quicksave & Auto-Resume:** Pressing **Power** triggers an automatic quicksave and clean power-off, resuming directly back into your active level on next boot.

## Installation
1. Download `woof-nextui-pak.zip` from the [Releases](../../releases) page and extract it to the root of your NextUI SD card (`Emus/` and `Roms/`).
2. Place your IWAD files (`DOOM.WAD`, `DOOM2.WAD`, `TNT.WAD`, `PLUTONIA.WAD`, `FREEDOOM1.WAD`, or `FREEDOOM2.WAD`) inside `/Roms/DOOM (DOOM)/`.
3. *(Optional)* Place PWAD and `.deh` files inside `/Roms/DOOM (DOOM)/.mods/` and create a `.doom` launcher script in `/Roms/DOOM (DOOM)/`:
   ```sh
   #!/bin/sh
   WOOF="\$1"
   shift
   WADDIR="/mnt/SDCARD/Roms/DOOM (DOOM)"
   "WOOF" -iwad "WADDIR/DOOM2.WAD" -file "WADDIR/.mods/mymod.wad" "@"
   ```

## Default Controls

| Action | Anbernic (`h700`) / TrimUI (`tg5040`) |
| :--- | :--- |
| **Move / Turn** | D-Pad |
| **Analog Move / Strafe** | Left Stick |
| **Analog Camera / Look** | Right Stick |
| **Fire** | B |
| **Use / Open / Strafe On** | A |
| **Run / Speed** | Y |
| **Automap Follow** | X |
| **Strafe Left / Right** | L1 / R1 |
| **Previous / Next Weapon** | L2 / R2 |
| **Toggle Automap** | Select |
| **Doom Menu** | Start |
| **Quicksave & Power Off** | Power |

## Source & Credits
- Upstream **Woof!** compiled unmodified from [fabiangreffrath/woof](https://github.com) (GPL-2.0).
- Custom NextUI `/dev/fb0` & Linux `evdev` virtual gamepad driver implemented via SDL3's `dummy` video driver (`src/SDL_nullvideo.c`, `src/SDL_nullframebuffer.c`).
