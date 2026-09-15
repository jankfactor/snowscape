# Snowscape

### Description
A 3D landscape engine for [Acorn Archimedes](https://en.wikipedia.org/wiki/Acorn_Archimedes) based on the [Midwinter](https://en.wikipedia.org/wiki/Midwinter_(video_game)) series of games by [Mike Singleton](https://en.wikipedia.org/wiki/Mike_Singleton) and [Maelstrom](https://www.mobygames.com/company/1628/maelstrom-games-ltd/games/). These games were never released for Acorn machines, so Snowscape explores what an authentic Archimedes version might have looked and felt like while taking advantage of the machine's ARM processor and distinctive 256-colour palette. It is written in C89 and ARM assembly.

![Mode 9 (16-color) and Mode 13 (256-color) versions of the map and terrain engine](terrain.png?v=96ba714)
*16-Color (aka, Amiga/ST) and Archimedes 256-Color (custom) versions*

### QuickStart
Thanks to the amazing [Archimedes Live!](https://archi.medes.live/) you can run a prebuilt version directly in the browser.

- [SnowScape A5000 — 16-color](https://archi.medes.live#preset=a5000&ff=28800&mouse-capture=force&disc=https://raw.githubusercontent.com/jankfactor/snowscape/main/Images/Snow16A5k.adf&autoboot=desktop%20filer_run%20adfs::0.$.!Snowscape) (further draw distance)
- [SnowScape A5000 — 256-color](https://archi.medes.live#preset=a5000&ff=28800&mouse-capture=force&disc=https://raw.githubusercontent.com/jankfactor/snowscape/main/Images/Snow256A5k.adf&autoboot=desktop%20filer_run%20adfs::0.$.!Snowscape) (further draw distance)
- [SnowScape A3020 — 16-color](https://archi.medes.live#preset=a3020&ff=28800&mouse-capture=force&disc=https://raw.githubusercontent.com/jankfactor/snowscape/main/Images/Snow16.adf&autoboot=desktop%20filer_run%20adfs::0.$.!Snowscape)
- [SnowScape A3020 — 256-color](https://archi.medes.live#preset=a3020&ff=28800&mouse-capture=force&disc=https://raw.githubusercontent.com/jankfactor/snowscape/main/Images/Snow256.adf&autoboot=desktop%20filer_run%20adfs::0.$.!Snowscape)

### Overview
Originally started via manual reverse engineering, the DOS version of *Midwinter* has now been fully reverse engineered elsewhere with LLM assistance. Snowscape uses the resulting documentation to reproduce the original terrain generation, world behaviour, rendering decisions, palette character, fog, and overall experience as closely as practical. But this is not intended to be an instruction-for-instruction port of the original x86 code by any means. Where appropriate, routines and data flow are redesigned for ARM and RISC OS&mdash;including 32-bit fixed-point maths, custom lookup tables, polygon rasterisation, screen banking, and machine-specific draw distances&mdash;to keep performance as high and predictable as possible on real Archimedes hardware. Essentially, the implementation remains purpose-built for Acorn machines.

As the Archimedes has a 'quirky' 256 color mode, I've implemented a 256 color version. However, there's also a Mode 9 16-color version which aims to look closer to the Atari ST and Amiga originals. Also there's an A5000 version with further draw distance.

⚠️ The original game used a big-endian `ZBUFFER.BIN` on PC (taken from the Amiga/ST version) with a starting grid of 50x50 points and a hash table for midpoint generation. **A custom version of this has been created as to not ship copyrighted code**, but if you legally own Midwinter on PC, you can rename that file `ZBUFFER` (i.e., remove the `.BIN`) and replace the one in the `src/!Snowscape/assets` folder to see the original Midwinter isle.

As per the license this software is released **AS IS**. I don't have the time to look through pull requests, etc., but please feel free to fork the project and play with it as you will. :)

### Known Issues
- When you reach the edge of the terrain, you'll be abruptly reset to the starting position. TODO - like the original Midwinter, we could reset the camera and generate the terrain to match the next section you're in. 
- Performance could probably be improved by moving more C into ARM assembly... but it's worth playing the original Midwinter on a stock Atari ST or Amiga 500 as a reminder of the original framerate... 😜 
- Needs a 4MB setup. Could probably be fixed to run on 2MB machines.

### Building Prerequisites
Either of the following GCC cross-compiler build systems can be used to create the RISCOS binary. Both can be built and used using Linux, WSL in Windows, and Docker with Ubuntu in MacOS (ArchieSDK has a MacOS-native build now).
- [GCCSDK](https://www.stevefryatt.org.uk/risc-os/build-tools/environment) - An older GCC 4 build SDK commonly used for RISCOS Open development
- [ArchieSDK](https://gitlab.com/_targz/archiesdk) - A more recent GCC 8 build by Tara Colin and various contributors, particularly of interest to the Acorn demoscene.

### Building
For optional `--adfs` packaging, install the tool once (requires Python 3.11 or newer):

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r Scripts/requirements-build.txt
```

1. Open `build.sh` and ensure the output directory matches where you'd like the resultant App to go. (probably your Arculator hostfs folder)
2. Choose the toolchain you want `GCCSDK` or `ARCHIESDK`.
3. Choose 256 color and/or A5000 mode. The A5000 mode has a further draw distance and will perform much slower on A30X0 machines.
4. Run `./build.sh` for the selected variant, or `./build.sh --all` to build all four variants in sequence. Each is copied to its own hostfs app folder: `!Snow16`, `!Snow256`, `!Snow16A5k`, or `!Snow256A5k`. Double-click an app to start it.

Add `--adfs` (`./build.sh --adfs` or `./build.sh --all --adfs`) to also write an 800 KB ADFS E disk image in the root `Images` folder: `Snow16.adf`, `Snow256.adf`, `Snow16A5k.adf`, or `Snow256A5k.adf`. Mount the image in your Archimedes emulator, open the disk, and double-click `!Snowscape`. RISC OS file types are preserved. Images are replaced only after packaging and validation succeed.

Test build/copy and ADF packaging without an ARM compiler:

```sh
.venv/bin/python tests/test_build_copy.py
```

### Running on Original Hardware
Download a prebuilt ADF directly; no build is required:

- A3020-class machines: [16-color](https://raw.githubusercontent.com/jankfactor/snowscape/main/Images/Snow16.adf) or [256-color](https://raw.githubusercontent.com/jankfactor/snowscape/main/Images/Snow256.adf).
- A5000 (further draw distance): [16-color](https://raw.githubusercontent.com/jankfactor/snowscape/main/Images/Snow16A5k.adf) or [256-color](https://raw.githubusercontent.com/jankfactor/snowscape/main/Images/Snow256A5k.adf).

Load the chosen image on a Gotek, or write it to a floppy disk. On the Archimedes, open the disk and double-click `!Snowscape`. The ADFs already preserve the required RISC OS file types.

To build your own ADFs, run `./build.sh --all --adfs`; the images are written to the `Images` folder.

### Enabling Profiling
1. Uncomment `// #define TIMING_LOG 1` in `/Projects/h/Render`
2. Uncomment `| Run <Obey$Dir>.TimerMod` in the `/Projects/!Run` script.
3. Download [TimerMod](https://armclub.org.uk/free/) and unpack into in the `/Projects` folder.
4. Rebuild and Run.

### Scripts
- The scripts folder contains a NodeJS script which is used to take the exported Archimedes palette (a list of RGB hex numbers) and generate a lookup table. It does this as a PNG first, because of the limited options, it doesn't always get the gradients looking nice, so we can tweak the table a bit before turning it into the binary lookup which is used by the engine in the /assets folder. 

#### Packing and unpacking a ZBUFFER asset

```sh
npm ci --prefix Scripts
node Scripts/gen_zbuffer.js unpack ZBUFFER.BIN heights.png offsets.png
node Scripts/gen_zbuffer.js pack heights.png offsets.png ZBUFFER.BIN
```

Requires Node.js 18.3 or newer and the existing `pngjs` dependency. A binary asset is exactly 10,000 bytes: a 50 × 50 grid of interleaved big-endian signed 16-bit heights and unsigned 16-bit detail/hash words, matching `TerrainLoad`. Files named `ZBUFFER` work too. Existing output files are overwritten in either direction.

- **Heights:** a **50 × 50, 8-bit greyscale PNG**. Packing uses `height = pixel * 32 - 512`: black is -512, pixel 16 is sea level (0), and white is 7648. Unpacking uses `round((height + 512) / 32)`, clamped to 0..255. This rounds heights to 32-unit steps and clips heights outside -512..7648; arbitrary source heights cannot round-trip exactly.
- **Offsets:** a **50 × 50, 8-bit greyscale PNG** containing the high byte of each detail/hash word. Packing always writes **4 (`0x04`)** as the low byte: `detail = pixel * 256 + 4`. For example, pixel 18 encodes `0x1204`. Unpacking discards the original low byte. The hash round trip is exact when all original low bytes are 4; other low bytes are replaced with 4. These values are subdivision seeds, not direct height offsets or a smooth roughness map.

Both PNGs must use greyscale mode without alpha/transparency (not RGB or indexed colour). Rows run top to bottom and cells left to right, with no flipping, resizing or gamma correction. Keep both images at 8-bit depth when editing. The former 100 × 50 offset layout is no longer accepted.

Run the checks for both directions, fixed low bytes, image validation and output overwriting:

```sh
node Scripts/test_gen_zbuffer.js
```

### Grateful Thanks and Shoutouts
The Maelstrom team (particularly in memory of [Mike Singleton](https://en.wikipedia.org/wiki/Mike_Singleton)), whos incredible games made a little me believe whole worlds could exist in a small home computer!

David Ruck for his superb [TimerMod](https://armclub.org.uk/free/) utility which is available from his site which made profiling many of the routines far easier.

The amazing [Bitshifters](https://bitshifters.github.io/index.html) team for always being generous with their knowledge and pushing the Archimedes and BBC Micro to it's limits with their amazing demos!

[Tara Colin](https://gitlab.com/_targz/archiesdk), and others who have contributed to the ArchieSDK tools which has opened up an exciting new chapter in demoscene development for early RISCOS machines. 

Tom Sneddon for helping me fix the CORS issue which means we can link to Archimedes Live! direct from here. Also check out `b2`, his amazing [BBC Micro Emulator](https://github.com/tom-seddon/b2)! 
