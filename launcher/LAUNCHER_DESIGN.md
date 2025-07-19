# glkterm Launcher Design Document

## Overview
This document outlines the design and implementation plan for the glkterm launcher system, which will provide automatic interpreter detection and enhanced save file management for interactive fiction games.

## Goals
- Automatic game format detection and interpreter selection
- Enhanced save file management with game association
- Two complementary interfaces: CLI and TUI
- Terminal-native experience that leverages glkterm's strengths

## Architecture

### Two Binary Approach
- **`glkcli`** - Minimal CLI launcher for terminal power users
- **`glkterm`** - Full-featured TUI launcher for interactive use

### Shared Core Components
- `glkterm/launcher.c/.h` - Core game detection and interpreter launching
- `glkterm/config.c/.h` - Configuration file handling
- `glkterm/savefiles.c/.h` - Save file management and association

## Implementation Phases

### Phase 1: Core Launcher Logic
**Files:** `glkterm/launcher.c`, `glkterm/launcher.h`

**Features:**
- Game format detection by file header magic bytes
- File extension fallback detection
- Blorb file analysis for embedded games
- Interpreter mapping and execution
- Basic error handling and user feedback

**Game Format Support:**
Based on garglk's detection patterns, support for:
- Z-code (.z1-.z8, .dat)
- Glulx (.ulx)
- TADS (.gam, .t3)
- Hugo (.hex)
- AGT (.agx, .d$$)
- JACL (.jacl, .j2)
- Level 9 (.l9, .sna)
- Magnetic Scrolls (.mag)
- Alan (.acd, .a3c)
- Adrift (.taf)
- Scott Adams (.saga)
- Plus (.plus)
- Taylor (.tay)

### Phase 2: Basic CLI Launcher
**Files:** `glkcli/main.c`

**Features:**
- Command-line interface: `glkcli [options] <gamefile>`
- Uses Phase 1 for detection and launching
- Basic options:
  - `--no-save` - Launch without loading save file
  - `--list-saves` - Show available save files for game
  - `--save=<file>` - Load specific save file
  - `--help` - Show usage information

**Usage Examples:**
```bash
glkcli game.z5                    # Auto-detect and launch with most recent save
glkcli --no-save game.z5          # Launch without save file
glkcli --save=mysave.sav game.z5  # Launch with specific save
glkcli --list-saves game.z5       # List available saves
```

### Phase 3: Save File Management
**Files:** `glkterm/config.c/.h`, `glkterm/savefiles.c/.h`

**Features:**
- Configuration file handling (`~/.config/glkterm/config`)
- Track most recent save file per game
- Automatic save file association
- Save file discovery and management
- Game-to-save mapping persistence

**Configuration File Format:**
```ini
[games]
games_directory = ~/Games/IF

[saves]
auto_load_saves = true
save_directory = ~/.local/share/glkterm/saves

[game_saves]
/path/to/game.z5 = /path/to/recent_save.sav
```

### Phase 4: Full TUI Launcher
**Files:** `glkterm/tui.c/.h`, enhanced `glkterm/main.c`

**Features:**
- ncurses-based terminal user interface
- Game browser from configured games directory
- Save file selection dialog
- Recent games list
- File picker for browsing other games
- Configuration management interface

**TUI Flow:**
1. Main menu: Recent Games / Browse Games / Settings / Quit
2. Game selection with save file preview
3. Save file selection dialog (if multiple saves exist)
4. Launch game with selected interpreter

## Technical Details

### Game Detection Priority
1. Custom interpreter mapping from config file (highest priority)
2. File header magic byte detection
3. Blorb file analysis for embedded games
4. File extension mapping (lowest priority)

### Error Handling
- Clear error messages for unsupported formats
- Graceful fallback when interpreters are missing
- User-friendly feedback for file access issues

### Integration with Existing Build System
- Add launcher components to CMakeLists.txt
- Build both `glkcli` and `glkterm` binaries
- Link against existing glkterm library for GLK functionality

## Dependencies
- Existing glkterm library
- ncurses (for TUI - Phase 4)
- Standard C library functions for file I/O
- POSIX functions for directory operations

## Configuration Locations
- Config file: `~/.config/glkterm/config`
- Save files: `~/.local/share/glkterm/saves/` (or user-configured)
- Recent games: Stored in main config file

## Future Enhancements
- Game metadata caching (title, author, etc.)
- Save file thumbnails/descriptions
- Game organization by series or author
- Integration with online game databases
- Automated game file discovery
