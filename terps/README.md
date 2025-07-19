# Interactive Fiction Interpreters for GlkTerm

This directory contains a collection of interactive fiction interpreters that have been adapted to work with the glkterm library. These interpreters allow you to play various types of text-based adventure games and interactive fiction.

## Overview

All interpreters are built using CMake and can be individually enabled or disabled during configuration. Each interpreter is compiled as a standalone executable that links against the glkterm library for terminal-based input/output.

## Supported Interpreters

### AdvSys - `advsys`
**Version:** 1.2 + ANSI + NewParser + parts of glkize  
**Game Format:** AdvSys games  
**File Extensions:** `.dat`  
**License:** BSD-style license  
**Description:** Interpreter for games created with the AdvSys adventure development system.

### Agility - `agility` 
**Version:** 1.1.2 with Glk  
**Game Format:** AGT (Adventure Game Toolkit) games  
**File Extensions:** `.agx`, `.agt`  
**License:** GPL v2  
**Description:** Runs games created with Adventure Game Toolkit. Note: Uses some platform-specific code for file handling on Unix systems.

### Alan 2 - `alan2`
**Version:** 2.8.6  
**Game Format:** Alan v2 games  
**File Extensions:** `.acd`  
**License:** Artistic License  
**Description:** Interpreter for Alan version 2 adventure games. Several bugs have been fixed from the original version.

### Alan 3 - `alan3`
**Version:** 3.0beta8  
**Game Format:** Alan v3 games  
**File Extensions:** `.a3c`  
**License:** Artistic License  
**Description:** Interpreter for the newer Alan version 3 adventure games.

### Bocfel - `bocfel`
**Version:** 2.2.4  
**Game Format:** Z-machine games (Infocom format)  
**File Extensions:** `.z3`, `.z4`, `.z5`, `.z8`, `.zblorb`, `.zlb`  
**License:** MIT License  
**Description:** Modern Z-machine interpreter that can run classic Infocom games and modern Inform-compiled games. Supports Blorb resources and has extensive compatibility features.

### Git - `git`
**Version:** 1.3.8  
**Game Format:** Glulx games  
**File Extensions:** `.ulx`, `.gblorb`, `.glb`  
**License:** MIT License  
**Description:** Fast Glulx virtual machine interpreter. Startup code has been reworked to redirect errors to Glk windows.

### Glulxe - `glulxe`
**Version:** 0.6.1  
**Game Format:** Glulx games  
**File Extensions:** `.ulx`, `.gblorb`, `.glb`  
**License:** MIT License  
**Description:** Reference implementation of the Glulx virtual machine. Supports floating-point operations and extended features.

### Hugo - `hugo`
**Version:** git-df0ce19  
**Game Format:** Hugo games  
**File Extensions:** `.hex`  
**License:** BSD-style license  
**Description:** Interpreter for games written in the Hugo programming language.

### JACL - `jacl`
**Version:** git-ce6517f  
**Game Format:** JACL games  
**File Extensions:** `.jacl`, `.j2`  
**License:** GPL v2  
**Description:** Interpreter for games written in the JACL (Just Another Classic Language) adventure language.

### Level9 - `level9`
**Version:** 5.1  
**Game Format:** Level 9 Computing games  
**File Extensions:** `.sna`, `.l9`  
**License:** GPL v2  
**Description:** Plays classic games from Level 9 Computing. Includes graphics support and has disabled the static version string status bar.

### Magnetic - `magnetic`
**Version:** 2.3.1  
**Game Format:** Magnetic Scrolls games  
**File Extensions:** `.mag`  
**License:** GPL v2  
**Description:** Interpreter for Magnetic Scrolls adventures. Features optimized drawing and delayed status window opening.

### Plus - `plus`
**Version:** 1.0  
**Game Format:** Scott Adams Graphic Adventures Plus games  
**File Extensions:** Various disk image formats  
**License:** GPL v2  
**Description:** Specialized interpreter for Scott Adams Plus graphic adventures, supporting various 8-bit computer disk image formats.

### SCARE - `scare`
**Version:** 1.3.10  
**Game Format:** Adrift 4 games  
**File Extensions:** `.taf`  
**License:** GPL v2  
**Description:** Interpreter for games created with Adrift version 4. Requires zlib for decompression support.

### ScottFree - `scott`
**Version:** 1.14  
**Game Format:** Scott Adams games  
**File Extensions:** `.dat`, `.ti99`, various disk formats  
**License:** GPL v2  
**Description:** Comprehensive interpreter for classic Scott Adams adventure games, supporting multiple platforms and disk image formats including TI-99/4A, Commodore 64, Apple II, and Atari 8-bit.

### TADS - `tadsr`
**Game Format:** TADS 2 & 3 games  
**File Extensions:** `.gam` (TADS 2), `.t3` (TADS 3)  
**License:** GPL v2  
**Description:** Combined interpreter for both TADS 2 and TADS 3 games. Supports the full feature set of both TADS systems.

### TaylorMade - `taylor`
**Version:** 0.4  
**Game Format:** Later Adventure Soft UK games  
**File Extensions:** Various disk and tape image formats  
**License:** GPL v2  
**Description:** Interpreter for later Adventure Soft UK games including Temple of Terror, Kayleth, and Questprobe series.

### FrankenDrift - `frankendrift` (Disabled by Default)
**Game Format:** Various (experimental)  
**License:** Various (.NET dependencies)  
**Description:** .NET-based interpreter originally designed for Garglk. Currently disabled by default as it requires adaptation to work properly with glkterm and needs .NET runtime support.

## Building Interpreters

All interpreters are built automatically when you build the glkterm project using CMake. You can control which interpreters are built using CMake options:

```bash
# Build with all default interpreters (FrankenDrift excluded)
cmake ..

# Disable specific interpreters
cmake .. -DWITH_SCOTT=OFF -DWITH_TADS=OFF

# Enable FrankenDrift (requires .NET)
cmake .. -DWITH_FRANKENDRIFT=ON

# Build only specific interpreters
cmake .. -DWITH_ADVSYS=OFF -DWITH_AGILITY=OFF -DWITH_ALAN2=OFF \
         -DWITH_ALAN3=OFF -DWITH_BOCFEL=ON -DWITH_GIT=ON \
         -DWITH_GLULXE=ON # ... etc
```

## Usage Examples

After building, interpreters are available in the `build/terps/` directory:

```bash
# Z-machine games (Infocom-style)
./build/terps/bocfel zork1.z5

# Glulx games 
./build/terps/git adventure.ulx
./build/terps/glulxe story.gblorb

# Scott Adams games
./build/terps/scott adventure1.dat

# TADS games
./build/terps/tadsr game.gam

# Hugo games
./build/terps/hugo game.hex

# With glkterm options
./build/terps/bocfel -width 132 -height 40 game.z8
```

## Command-Line Options

Each interpreter inherits the standard glkterm command-line options:

- `-width NUM`, `-height NUM`: Set screen dimensions
- `-ml BOOL`: Use message line (default yes)
- `-revgrid BOOL`: Reverse text in status windows
- `-historylen NUM`: Command history length
- `-border BOOL`: Force window borders
- `-precise BOOL`: More precise timing
- `-sound BOOL`: Enable sound
- `-color BOOL`: Enable color
- `-version`: Show version information
- `-help`: Show help text

Many interpreters also support their own specific options. Use `--help` or similar to see interpreter-specific options.

## Dependencies

Most interpreters have minimal dependencies beyond the glkterm library:

- **Standard:** C compiler, glkterm library
- **Some interpreters:** C++ compiler (bocfel, git, tads, etc.)
- **SCARE:** zlib development libraries
- **FrankenDrift:** .NET runtime (when enabled)

## File Format Support

| Interpreter | Primary Extensions | Additional Formats |
|-------------|-------------------|-------------------|
| advsys | .dat | |
| agility | .agx, .agt | |
| alan2 | .acd | |
| alan3 | .a3c | |
| bocfel | .z3-.z8, .zblorb | .zlb |
| git | .ulx, .gblorb | .glb |
| glulxe | .ulx, .gblorb | .glb |
| hugo | .hex | |
| jacl | .jacl, .j2 | |
| level9 | .sna, .l9 | |
| magnetic | .mag | |
| plus | (disk images) | .dsk, .d64 |
| scare | .taf | |
| scott | .dat | .ti99, disk images |
| tadsr | .gam, .t3 | |
| taylor | (various) | disk/tape images |

## Graphics and Media Support

Several interpreters support graphics and multimedia:

- **bocfel:** Blorb graphics and sound
- **git/glulxe:** Blorb graphics and sound
- **level9:** Graphics support enabled
- **magnetic:** Graphics with optimized drawing
- **plus:** Graphics for Scott Adams Plus games
- **scott:** Graphics for various Scott Adams variants
- **taylor:** Graphics for Adventure Soft UK games

## Troubleshooting

### Common Issues

1. **Library not found:** Ensure glkterm is built and installed
2. **Game won't load:** Check file format compatibility
3. **Graphics not working:** Verify Blorb file integrity
4. **Sound issues:** Check SDL2_mixer installation

### Debug Options

Many interpreters support debug modes:
```bash
./build/terps/scott -d game.dat  # Scott Adams debug mode
./build/terps/agility -d game.agx  # Agility debug info
```

## License Information

Each interpreter maintains its original license:

- Most interpreters: Various open source licenses (see individual directories)
- GPL2-licensed interpreters: agility, jacl, plus, scare, tads, taylor
- Check individual interpreter directories for specific license information

## Contributing

When adding new interpreters:

1. Add the interpreter source to a new subdirectory
2. Create a CMakeLists.txt entry in the main terps/CMakeLists.txt
3. Add appropriate CMake option (WITH_INTERPRETER)
4. Update this README with interpreter details
5. Test building and basic functionality

For interpreter-specific issues, refer to the original interpreter documentation in each subdirectory.
