# glkterm Launcher

This directory contains the modular launcher system for glkterm, designed to automatically detect and launch Interactive Fiction games with the appropriate interpreters.

## Directory Structure

```
launcher/
├── LAUNCHER_DESIGN.md      # Design document and roadmap
├── launcher.h              # Main launcher API
├── launcher.c              # Core launcher implementation
├── launcher_config.h       # Configuration and static data declarations
├── launcher_config.c       # Interpreter mappings, format definitions
├── launcher_detect.h       # Game format detection API
├── launcher_detect.c       # Format detection logic (header, extension, blorb)
├── launcher_cli.h          # CLI utilities API
├── launcher_cli.c          # Command-line argument parsing and formatting
└── glkcli/
    └── main.c              # CLI launcher executable
```

## Modular Design

The launcher has been refactored into several focused modules:

### Core Modules

- **launcher.c/h** - Main API for game detection and launching
- **launcher_config.c/h** - Static configuration data (interpreters, format names, magic patterns)
- **launcher_detect.c/h** - Game format detection logic separated by method

### Interface Modules

- **launcher_cli.c/h** - Reusable CLI utilities for argument parsing and output formatting
- **glkcli/** - Command-line interface implementation

## Key Features

### Format Detection
- **Header-based**: Detects formats by examining file magic bytes
- **Extension-based**: Fallback detection using file extensions  
- **Blorb support**: Properly parses Blorb files to detect embedded Z-code or Glulx games

### Supported Formats
- Z-code (.z1-.z8, .dat) → bocfel
- Glulx (.ulx, .gblorb) → git
- TADS (.gam, .t3) → tadsr
- Hugo (.hex) → hugo
- AGT (.agx, .d$$) → agility
- JACL (.jacl, .j2) → jacl
- Level 9 (.l9, .sna) → level9
- Magnetic Scrolls (.mag) → magnetic
- Alan 2/3 (.acd, .a3c) → alan2/alan3
- Adrift (.taf) → scare
- Scott Adams (.saga) → scott
- Plus (.plus) → plus
- Taylor (.tay) → taylor

### CLI Interface
- Format detection only (`-f`)
- Verbose output (`-v`)
- Future save file management hooks
- Comprehensive help and format listing

## Usage

```bash
# Detect game format
./glkcli -f game.z5

# Launch game with verbose output
./glkcli -v game.z5

# Show help
./glkcli -h
```

## Architecture Benefits

1. **Modularity**: Each component has a single responsibility
2. **Reusability**: CLI utilities can be shared with future TUI implementation
3. **Maintainability**: Configuration data is centralized and easy to modify
4. **Extensibility**: New formats can be added by updating configuration tables
5. **Testing**: Individual modules can be tested in isolation

## Future Development

This modular structure prepares for:
- Phase 3: Save file management and configuration
- Phase 4: Text-based UI (TUI) implementation
- Additional interpreter support
- Enhanced game metadata detection

The clean separation of concerns allows each component to be developed and tested independently while maintaining a cohesive API.
