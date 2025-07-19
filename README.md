# GlkTerm: Curses.h Implementation of the Glk API

**GlkTerm Library:** version 1.0.5  
**Glk API:** version 0.7.5  
**Designed by:** Andrew Plotkin <erkyrath@eblong.com>  
**Homepage:** http://eblong.com/zarf/glk/

## Overview

This is source code for an implementation of the Glk library which runs in a terminal window, using the curses.h library for screen control. Curses.h (no relation to the Meldrews) should be available on all Unix systems.

This source code is not directly applicable to any other display system. Curses library calls are scattered all through the code; I haven't tried to abstract them out. If you want to create a Glk library for a different display system, you'd best start over. Use GlkTerm for a starting point for terminal-window-style display systems, and MacGlk or XGlk for graphical/windowed display systems.

## Building with CMake

This project uses CMake as the build system, providing better cross-platform support and dependency management.

### Requirements

- CMake 3.16 or higher
- A C compiler (GCC recommended)
- ncurses development libraries
- SDL2 and SDL2_mixer development libraries (optional, for sound support)

### Building

1. **Create a build directory:**
   ```bash
   mkdir build
   cd build
   ```

2. **Configure with CMake:**
   ```bash
   cmake ..
   ```

3. **Build the library:**
   ```bash
   make
   ```

### Configuration Options

- **Enable/disable sound support:**
  ```bash
  cmake .. -DENABLE_SOUND=OFF  # Disable sound
  cmake .. -DENABLE_SOUND=ON   # Enable sound (default)
  ```

- **Set build type:**
  ```bash
  cmake .. -DCMAKE_BUILD_TYPE=Debug    # Debug build
  cmake .. -DCMAKE_BUILD_TYPE=Release  # Release build (default)
  ```

### Installation

After building, you can install the library system-wide:

```bash
sudo make install
```

This will install:
- `libglkterm.a` to the library directory
- Header files to the include directory  
- `Make.glkterm` to the include directory (for legacy Makefile compatibility)

## Using the Library

### With CMake

```cmake
find_package(glkterm REQUIRED)
target_link_libraries(your_target glkterm)
```

### With Traditional Makefiles

Include the generated `Make.glkterm` file in your Makefile:
```makefile
include Make.glkterm

your_program: your_program.o
	$(CC) -o your_program your_program.o $(GLKLIB) $(LINKLIBS)
```

## Command-line Arguments

GlkTerm can accept command-line arguments both for itself and on behalf of the underlying program. These are the arguments the library accepts itself:

- **`-width NUM`, `-height NUM`:** Set the screen width and height manually. Normally GlkTerm determines the screen size itself by asking the curses library. If this doesn't work, you can set a fixed size using these options.

- **`-ml BOOL`:** Use message line (default "yes"). Normally GlkTerm reserves the bottom line of the screen for special messages. By setting this to "no", you can free that space for game text. Note that some operations will grab the bottom line temporarily anyway.

- **`-revgrid BOOL`:** Reverse text in grid (status) windows (default "no"). Set this to "yes" to display all textgrid windows (status windows) in reverse text.

- **`-historylen NUM`:** The number of commands to keep in the command history of each window (default 20).

- **`-border BOOL`:** Force one-character borders between windows. The default is "yes", but some games switch these off. Set "yes" to force them on, or "no" to force them off, ignoring the game's request. These are lines of '-' and '|' characters. Without the borders, there's a little more room for game text, but it may be hard to distinguish windows. The `-revgrid` option may help.

- **`-precise BOOL`:** More precise timing for timed input (default "no"). The curses.h library only provides timed input in increments of a tenth of a second. So Glk timer events will only be checked ten times a second, even on fast machines. If this isn't good enough, you can try setting this option to "yes"; then timer events will be checked constantly. This busy-spins the CPU, probably slowing down everything else on the machine, so use it only when necessary. For that matter, it may not even work on all OSes. (If GlkTerm is compiled without support for timed input, this option will be removed.)

- **`-version`:** Display Glk library version.

- **`-help`:** Display list of command-line options.

**NUM** values can be any number. **BOOL** values can be "yes" or "no", or no value to toggle.

## Programming with GlkTerm

When you compile a Glk program and link it with GlkTerm, you must supply one more file: you must define a function called `glkunix_startup_code()`, and an array `glkunix_arguments[]`. These set up various Unix-specific options used by the Glk library. There is a sample "glkstart.c" file included in this package; you should modify it to your needs.

### Command-line Argument Processing

The `glkunix_arguments[]` array is a list of command-line arguments that your program can accept. The library will sort these out of the command line and pass them on to your code. The array structure looks like this:

```c
typedef struct glkunix_argumentlist_struct {
    char *name;
    int argtype;
    char *desc;
} glkunix_argumentlist_t;

extern glkunix_argumentlist_t glkunix_arguments[];
```

In each entry:
- **`name`** is the option as it would appear on the command line (including the leading dash, if any)
- **`desc`** is a description of the argument; this is used when the library is printing a list of options
- **`argtype`** is one of the following constants:
  - `glkunix_arg_NoValue`: The argument appears by itself
  - `glkunix_arg_ValueFollows`: The argument must be followed by another argument (the value)
  - `glkunix_arg_ValueCanFollow`: The argument may be followed by a value, optionally
  - `glkunix_arg_NumberValue`: The argument must be followed by a number
  - `glkunix_arg_End`: The array must be terminated with an entry containing this value

#### Example Arguments Array

```c
glkunix_argumentlist_t glkunix_arguments[] = {
    { "", glkunix_arg_ValueFollows, "filename: The game file to load." },
    { "-hum", glkunix_arg_ValueFollows, "-hum NUM: Hum some NUM." },
    { "-bom", glkunix_arg_ValueCanFollow, "-bom [ NUM ]: Do a bom (on the NUM, if given)." },
    { "-goo", glkunix_arg_NoValue, "-goo: Find goo." },
    { "-wob", glkunix_arg_NumberValue, "-wob NUM: Wob NUM times." },
    { NULL, glkunix_arg_End, NULL }
};
```

### Startup Code

After the library parses the command line, it calls `glkunix_startup_code()`:

```c
int glkunix_startup_code(glkunix_startup_t *data);
```

This should return `TRUE` if everything initializes properly. If it returns `FALSE`, the library will shut down without ever calling your `glk_main()` function.

The data structure contains:
```c
typedef struct glkunix_startup_struct {
    int argc;
    char **argv;
} glkunix_startup_t;
```

### Utility Functions

These Unix Glk library functions are available during startup:

```c
strid_t glkunix_stream_open_pathname(char *pathname, glui32 textmode, glui32 rock);
```
Opens an arbitrary file in read-only mode. **Only available during `glkunix_startup_code()`.**

```c
void glkunix_set_base_file(char *filename);
```
Sets the library's idea of the "current directory" for the executing program.

## Operating System Compatibility

The library requires ncurses. You may have to change `#include <curses.h>` to `#include <ncurses.h>` on some systems.

### Tested Systems

- **SunOS:** May need special include/library paths and `NO_MEMMOVE` definition
- **Solaris:** Compiles as-is (older versions may need `memmove()` adjustments)
- **IRIX:** Compiles as-is
- **HPUX:** May need to disable `OPT_TIMED_INPUT` and remove `KEY_END`/`KEY_HELP` references
- **AIX:** Similar to HPUX requirements
- **FreeBSD:** Compiles as-is
- **Unixware:** May need to use `cc` instead of `gcc`

## Source Code Organization

- **`glk_*` functions:** Glk API functions declared in `glk.h`
- **`gli_*` functions:** Internal GlkTerm library implementation functions declared in `glkterm.h`

The code catches every error that can be caught and prints visible warnings. It should be portable to any C environment with ANSI stdio and curses.h libraries.

## Known Issues and Limitations

- Window resizing may not work properly on some Unix systems (SIGWINCH handling)
- During `glk_exit()`, the "hit any key to exit" prompt doesn't do paging
- Could accept more style hints (indentation, centering, bold, underline/italics)
- No visible indication of paging in windows that need to page
- Window border artifacts may remain when closing windows

## Version History

### 1.0.5
- Added sound and style support
- Fixed struct initialization bug in `gli_date_to_tm()`
- Replaced `tmpnam()` with `mkstemp()`

### 1.0.4
- Updated Blorb-resource functions for FORM chunks (Glk 0.7.4)
- Added autosave/autorestore hooks stub

### 1.0.3
- Added Blorb-resource functions (Glk 0.7.4)
- Updated external filename handling

### 1.0.2
- Fixed `glk_put_char_uni()` output bug
- Fixed file read/write positioning bug
- Fixed stream memory allocation issues
- Added improved sound function stubs (Glk 0.7.3)

### 1.0.1
- Added date-time functions (Glk 0.7.2)
- Fixed Unicode normalization and case-changing bugs

### 1.0.0
- Support for Glk 0.7.1 features: window borders, line input terminators, echo control, Unicode normalization
- Added `glkunix_stream_open_pathname_gen()`

## License

The GlkTerm, GiDispa, and GiBlorb libraries, as well as the `glk.h` header file, are copyright 1998-2016 by Andrew Plotkin. All are distributed under the MIT license; see the "LICENSE" file.

## Migration from Original Makefile

The CMake build system preserves all functionality of the original Makefile:

- Builds the same static library (`libglkterm.a`)
- Generates the same `Make.glkterm` file for compatibility
- Handles the same dependencies (ncurses, SDL2/SDL2_mixer)
- Uses the same compilation flags

The main improvements:
- Better dependency detection using pkg-config
- Cross-platform support
- Option to disable sound support at configure time
- Modern installation and packaging support
