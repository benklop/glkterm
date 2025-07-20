# GLK Autosave/Autorestore Design Document

## Overview

This document outlines the design for implementing GLK interpreter-managed autosave/autorestore functionality in GlkTerm, compatible with the GLK specification and Glulxe's `GLKUNIX_AUTOSAVE_FEATURES`. This implementation will be completely separate from and will not modify any existing player-managed save functionality in games or interprete## Screen Interface Pattern for GlkTerm

### Key Insight from Fizmo Implementation

The most elegant and proven approach for autosave integration is the **screen interface hook pattern** used by fizmo-iosglk:

```c
// In screen_interface.h - core Fizmo
struct z_screen_interface {
    // ... standard interface functions ...
    
    // Autosave hooks (optional function pointers)
    int (*do_autosave)();                     // Called before input operations
    int (*restore_autosave)(z_file *savefile); // Called during interpreter startup
};
```

### Application to GlkTerm

GlkTerm should implement the same pattern in its GLK library interface:

```c
// glkterm.h - add to interface struct
struct glkterm_interface {
    // ... existing GLK functions ...
    
    // Optional autosave hooks for interpreters that support it
    int (*do_autosave)(void);                 // Save GLK + interpreter state
    int (*restore_autosave)(const char *path); // Restore from autosave file
};
```

**Benefits of this approach**:
1. **Non-intrusive**: Existing interpreters continue unchanged (hooks are NULL)
2. **Opt-in**: Only interpreters with autosave support set the function pointers
3. **Proven**: Exact same pattern used successfully in production iOS apps
4. **Flexible**: Allows different autosave strategies per interpreter type

### Integration Points

**For Glulxe (immediate target)**:
```c
// In Glulxe startup code
if (glkterm_interface.do_autosave) {
    set_library_select_hook(glkterm_interface.do_autosave);
}
if (glkterm_interface.restore_autosave) {
    set_library_autorestore_hook(glkterm_interface.restore_autosave);
}
```

**For other interpreters (future)**:
- Interpreters that want autosave would implement their own hooks
- Function signature allows interpreter-specific autosave strategies
- GlkTerm provides GLK state serialization primitives as a library service

### Refined Implementation Strategy

Based on comprehensive analysis of iOSGlulxe, IosFrotz, and iOSFizmo implementations, the optimal approach for GlkTerm is:

## References

- [GLK Specification 0.7.6](https://www.eblong.com/zarf/glk/Glk-Spec-076.html) - Core GLK API specification
- [Interpreter-Managed Saves in Glulx](https://www.eblong.com/zarf/glk/terp-saving-notes.html) - Primary autosave specification
- [iOSGlk Implementation](https://github.com/erkyrath/iosglk) - Reference implementation (deprecated but functional)
- [iOSGlulxe Project](https://github.com/erkyrath/iosglulxe) - Working example with autosave integration
- Glulxe autosave code (`unixautosave.c`, `unixstrt.c`) - Consumer of the API

## Key Concepts

### Two Types of Save Files

1. **Player-managed saves**: Traditional SAVE/RESTORE commands from the game
   - Occur at `@save` or `@saveundo` opcodes
   - Do not include I/O (GLK) state
   - Game must call `GGRecoverObjects()` after restore
   - Portable between interpreters
   - **Note**: We will make NO changes to existing player-managed save functionality in any interpreter or game

2. **Interpreter-managed saves**: Automatic background saves
   - Occur at `@glk` opcode (during `glk_select()` calls)
   - Include both VM state AND I/O (GLK) state
   - Restore happens before game execution resumes
   - Non-portable, interpreter-specific

### When Autosave Occurs

- During `glk_select()` calls when the game is awaiting input
- NOT during computation or when the game is busy
- Triggered by the GLK library, not by the game

### Stack Frame Differences

- **Player saves**: Stack setup for returning a result code after restore
- **Interpreter saves**: Stack setup for re-executing the `@glk` opcode

## Architecture

### GLK Library Components

#### 1. Context Types
```c
typedef struct glkunix_serialize_context_struct {
    FILE *file;
    glui32 *write_count;  /* Pointer allows modification when passed by value */
} glkunix_serialize_context_t;

typedef struct glkunix_unserialize_context_struct {
    FILE *file;
    glui32 *read_count;   /* Pointer allows modification when passed by value */
} glkunix_unserialize_context_t;
```

#### 2. Core Serialization Functions
```c
/* Basic serialization primitives */
int glkunix_serialize_uint32(glkunix_serialize_context_t ctx, char *id, glui32 value);
int glkunix_unserialize_uint32(glkunix_unserialize_context_t ctx, char *id, glui32 *value);
int glkunix_serialize_buffer(glkunix_serialize_context_t ctx, char *id, void *buffer, glui32 length);
int glkunix_unserialize_buffer(glkunix_unserialize_context_t ctx, char *id, void *buffer, glui32 length);

/* Object list serialization */
int glkunix_serialize_object_list(glkunix_serialize_context_t ctx, char *id,
    int (*serialize_obj)(glkunix_serialize_context_t ctx, void *obj),
    glui32 count, glui32 objsize, void *list);
int glkunix_unserialize_list(glkunix_unserialize_context_t ctx, char *id, void **array, glui32 *count);
int glkunix_unserialize_object_list_entries(void *array,
    int (*unserialize_obj)(glkunix_unserialize_context_t ctx, void *obj),
    glui32 count, glui32 objsize, void *list);
```

#### 3. High-Level Library State Functions
```c
/* Library state management */
typedef void *glkunix_library_state_t;

glkunix_library_state_t glkunix_load_library_state(strid_t file,
    int (*unserialize_extra)(glkunix_unserialize_context_t ctx, void *rock), 
    void *rock);
int glkunix_save_library_state(strid_t jsavefile, strid_t jresultfile,
    int (*serialize_extra)(glkunix_serialize_context_t ctx, void *rock),
    void *rock);
int glkunix_update_from_library_state(glkunix_library_state_t library_state);
void glkunix_library_state_free(glkunix_library_state_t library_state);
```

#### 4. Object Update Tag System
```c
/* Update tags for tracking object identity across save/restore */
glui32 glkunix_stream_get_updatetag(strid_t str);
glui32 glkunix_window_get_updatetag(winid_t win);
glui32 glkunix_fileref_get_updatetag(frefid_t fref);

winid_t glkunix_window_find_by_updatetag(glui32 tag);
strid_t glkunix_stream_find_by_updatetag(glui32 tag);
frefid_t glkunix_fileref_find_by_updatetag(glui32 tag);

void glkunix_window_set_dispatch_rock(winid_t win, gidispatch_rock_t rock);
void glkunix_stream_set_dispatch_rock(strid_t str, gidispatch_rock_t rock);
void glkunix_fileref_set_dispatch_rock(frefid_t fref, gidispatch_rock_t rock);
```

### Glulxe Integration Points

#### 1. Hook Functions
Glulxe expects these hooks to be set during startup:
```c
set_library_start_hook(func)      /* Called at beginning of glk_main() */
set_library_autorestore_hook(func) /* Called after VM setup, before game starts */
set_library_select_hook(func)     /* Called during glk_select() for autosave */
```

#### 2. Expected Functions
```c
glui32 glkunix_get_last_event_type(void);  /* For autosave decisions */
void glkunix_set_autosave_signature(unsigned char *buffer, glui32 length);
void glkunix_do_autosave(glui32 selector, glui32 arg0, glui32 arg1, glui32 arg2);
```

## Interpreter Requirements

### For Existing Interpreters (Non-Glulxe)

**No changes required** - All existing interpreters will continue to work exactly as before:
- All GLK functions remain unchanged
- No new dependencies
- No behavioral changes
- Traditional save/restore functionality unaffected

### For Autosave Support

To gain autosave functionality, an interpreter would need:

1. **Hook system integration**:
   - Call `set_library_start_hook()`, `set_library_autorestore_hook()`, `set_library_select_hook()`
   - Implement callbacks for these hooks

2. **VM state serialization**:
   - Ability to save/restore VM state at GLK call boundaries
   - Modified stack setup for autorestore (re-execute GLK call vs. return result)

3. **Extra state handling**:
   - Serialize interpreter-specific state not in standard save files
   - Handle object identity mapping between GLK and interpreter dispatch

4. **Autosave cleanup**:
   - Call `iosglk_clear_autosave()` when the VM exits (as seen in iOSGlulxe `TerpGlkDelegate.m`)

**Glulxe already has all of this** - it just needs the GLK library functions to be implemented.

### Available Examples from iOSGlulxe

The iOSGlulxe project shows a working autosave implementation with:

- **Automatic session restoration**: "when you launch the app you'll be where you left off" (from website)
- **Autosave cleanup**: `iosglk_clear_autosave()` called in `vmHasExited` 
- **Save file detection**: `checkGlkSaveFileFormat` method to validate Glulx save files
- **Integration with GLK UI**: File management through the app's file sharing interface

This demonstrates that autosave works properly with Glulxe when the GLK library provides the necessary functions.

### IosFrotz: Comprehensive Multi-Interpreter Autosave Reference

The [IosFrotz project](https://github.com/ifrotz/iosfrotz) provides the most comprehensive reference implementation for GLK autosave, supporting multiple interpreter types with a unified approach:

**Supported Interpreters with Autosave**:
- **Glulxe**: Full autosave integration using standard GLK calls
- **Git**: Autosave hooks during execution loop (`terp.c`, `glkop.c`)
- **Frotz**: Z-machine autosave integration (`fastmem.c`)
- **TADS**: Hugo, Agility, and other interpreters with varying levels of autosave support

**Key Architectural Insights from `glkios/glkautosave.c`**:

1. **Centralized GLK Object Serialization**:
   ```c
   struct glk_object_save {
       glui32 type;  // Window, Stream, Fileref, etc.
       glui32 id;    // Persistent object identifier
       union { /* All GLK object types */ } obj;
       int iscurrent;  // Current stream marker
   };
   ```

2. **Universal Trigger Pattern**:
   - Global `do_autosave` flag set by UI
   - Checked during interpreter execution loops
   - Triggered from GLK calls (`op_glk` in Glulx/Git)
   - Uses `AUTOSAVE_FILE` for consistent naming

3. **Comprehensive State Capture**:
   - **Windows**: Layout, input state, content buffers, style hints
   - **Streams**: Buffer pointers (adjusted for memory layout), read/write counts
   - **Files**: Relative paths, usage flags, text mode
   - **Graphics**: Dimensions, colors, image cache state

4. **Version Management & Compatibility**:
   - Multiple format versions (`FrotzGlkClassChunkVersionNumV1`-`V4`)
   - Backward compatibility during restore
   - Graceful degradation for unknown versions

5. **Production-Tested Reliability**:
   - Atomic file operations prevent corruption
   - Memory pointer adjustment for different interpreter layouts
   - Comprehensive error handling and recovery
   - Non-intrusive design (no interference with player saves)

**Lessons for GlkTerm**:
- Same GLK autosave infrastructure can support multiple interpreter types
- Object ID management system enables reliable serialization/restoration
- Interpreter-specific hooks are minimal and non-intrusive
- Real-world validation across diverse game types and interpreter engines

### iOSFizmo (fizmo-iosglk): Complete Z-machine Autosave Implementation

**Repository**: https://github.com/erkyrath/fizmo (fizmo-iosglk subdirectory)

This provides the most detailed reference implementation for GLK autosave integration with a complete Z-machine interpreter.

#### Key Architecture Components

**1. Screen Interface Hook System**
```c
// Function pointers in z_screen_interface struct
int (*do_autosave)();              // Called before @read operations  
int (*restore_autosave)(z_file *); // Called during fizmo_start()
```

**2. Dual-State Management Pattern**
- **Game State**: Standard Fizmo save file (`autosave.glksave`) 
- **GLK Library State**: iOS plist file (`autosave.plist`)
- **Atomic Operations**: Write to temp files, then rename
- **Save Location**: Documents directory with predictable names

**3. Core Implementation Functions**
```c
// ios-autosave.h interface
int iosglk_do_autosave(void);                    // Main autosave function
void iosglk_queue_autosave(void *pathname);      // Queue save file for restore
z_file *iosglk_find_autosave(void);             // Locate autosave on startup
int iosglk_restore_autosave(z_file *savefile);   // Restore from autosave
void iosglk_clear_autosave(void);               // Clean up autosave files
```

**4. GLK State Coordination**
```c
// glk_screen_if.c functions
void glkint_stash_library_state(library_state_data *dat);    // Capture GLK state
void glkint_recover_library_state(library_state_data *dat);  // Restore GLK state
```

#### Implementation Details

**Autosave Process** (`iosglk_do_autosave()`):
1. **VM State Preservation**: Save current PC location
2. **Game State Save**: Use Fizmo's `save_game_to_stream()` to temp file
3. **GLK State Capture**: Call `glkint_stash_library_state()` 
4. **Library Serialization**: Use NSKeyedArchiver with custom hooks
5. **Atomic Commit**: Move temp files to final locations

**Autorestore Process** (`iosglk_restore_autosave()`):
1. **Game State Restore**: Use Fizmo's `restore_game_from_stream()`
2. **Library State Restore**: NSKeyedUnarchiver with custom hooks
3. **GLK State Recovery**: Call `glkint_recover_library_state()`
4. **Object Remapping**: Update GLK library with restored state

**Integration Pattern**:
```c
// fizmo-iosglk.m hook installation
glkint_screen_interface.do_autosave = iosglk_do_autosave;
glkint_screen_interface.restore_autosave = iosglk_restore_autosave;
```

**Key Features**:
- **Non-intrusive**: No changes to player-managed saves
- **Triggered automatically**: Called during `@read` operations via screen interface
- **Error handling**: Graceful fallback if autosave/restore fails
- **File validation**: Distinguishes normal saves from autosaves via file implementation type
- **Process restart**: Clean interpreter restart with autorestore

#### Lessons for GlkTerm

1. **Screen Interface Pattern**: Use function pointers in interface struct for clean hook integration
2. **Dual State Management**: Separate interpreter state from GLK state for modularity  
3. **Library State Helpers**: Provide `stash`/`recover` functions for GLK-specific data
4. **Atomic File Operations**: Prevent corruption with temp files and atomic moves
5. **Predictable Locations**: Use standard save directories with consistent naming

## Data to Serialize

### GLK Library State

#### Top-level data:
- Current window size
- All windows, streams, filerefs, and schannels
- Timer event interval
- Identity of root window
- Identity of current selected output stream

#### Window data:
- Window type, rock
- Parent window
- Input request state (character/line, byte/unicode)
- Line input buffer contents
- Echo settings
- Line input terminator keys
- Associated streams (output, echo)
- Output text style
- Window-type-specific data:
  - Buffer windows: displayed text, including scrollback
  - Grid windows: size, cursor position, displayed text
  - Pair windows: direction, division size, border flag, key window

#### Stream data:
- Stream type, rock
- Unicode flag
- Readable/writable flags
- Read/write character counts
- Type-specific data:
  - Window streams: associated window identity
  - Memory streams: buffer length, position, contents
  - File streams: filename, mode, text/binary flag, position

#### Fileref data:
- Rock
- File mode
- Text/binary flag
- Filename on disk

#### Sound channel data:
- (Not implemented in GlkTerm currently)

### Extra Glulxe State

Data not in normal Glulx save files but needed for autosave:
- Memory protection range (start/end addresses)
- Current I/O system mode and rock
- Current address of string table
- Table associating GLK object tags with gi_dispa IDs
- Identity of open game file stream
- Random number generator state (if deterministic mode)
- Currently accelerated functions and parameter table

## Implementation Plan

### Phase 1: Core Infrastructure
1. **Update tag system**: Add unique, persistent IDs to all GLK objects
2. **Basic serialization**: Implement context types and primitive serialization functions
3. **Object serialization**: Implement window, stream, fileref serialization
4. **Hook integration**: Add the hook functions that Glulxe expects
5. **Isolation verification**: Ensure no interference with existing save/restore functionality

### Phase 2: Library State Management
1. **State capture**: Implement `glkunix_save_library_state()`
2. **State restoration**: Implement `glkunix_load_library_state()` and `glkunix_update_from_library_state()`
3. **File management**: Handle autosave file creation and cleanup (separate from player saves)

### Phase 3: Glulxe Integration
1. **Event tracking**: Implement `glkunix_get_last_event_type()`
2. **Autosave triggering**: Implement the select hook for automatic saving
3. **Autorestore**: Implement the autorestore hook for startup restoration
4. **Testing**: Validate with Glulxe games, ensuring player saves remain unaffected

### Phase 4: Advanced Features
1. **Memory stream handling**: Special handling for memory streams with app-provided buffers
2. **File stream recovery**: Handle cases where files are deleted/moved between saves
3. **Error recovery**: Graceful handling of corrupted autosave files
4. **Performance optimization**: Minimize autosave impact on gameplay
5. **Coexistence testing**: Verify autosave works alongside traditional save/restore

### Phase 5: Multi-Interpreter Support (Optional)
1. **Interpreter detection**: Identify which interpreters support autosave
2. **Graceful degradation**: Ensure non-autosave interpreters continue working normally
3. **Documentation**: Document how other interpreters could add autosave support
4. **Example integration**: Provide example code for adding autosave to other interpreters

## File Format Design

### Two-file approach (like iOSGlk):
1. **`autosave.glksave`**: Standard Glulx save file with modified stack setup
2. **`autosave.glkstate`**: GLK library state in custom binary format

**Note**: These autosave files are completely separate from any player-managed save files created by traditional SAVE commands.

### GLK State File Format:
```
Header: "GLKTERM_AUTOSAVE_V1"
Library metadata (version, timestamp, etc.)
Object count table
Object serialization data:
  - Windows (including content)
  - Streams (including buffers)
  - Filerefs
Update tag mapping table
```

## Refined Implementation Strategy

Based on comprehensive analysis of iOSGlulxe, IosFrotz, and iOSFizmo implementations, the optimal approach for GlkTerm is:

### Primary Implementation Focus

**Start with iOSFizmo-style API** for immediate compatibility:
```c
// Core functions following iOSFizmo patterns
void iosglk_queue_autosave(const char *pathname);
void iosglk_clear_autosave(void);
int iosglk_can_restart_cleanly(void);
void iosglk_shut_down_process(void);
```

**Benefits of this approach**:
1. **Immediate Glulxe compatibility** - Proven to work with existing Glulxe autosave hooks
2. **Minimal implementation** - Focus on core functionality first
3. **Real-world validation** - Same patterns used in production iOS apps
4. **Clear separation** - Autosave completely isolated from player saves

### Implementation Phases

**Phase 1**: Core GLK autosave infrastructure using iOSFizmo patterns
**Phase 2**: Glulxe integration and testing with the existing hook system
**Phase 3**: GlkTerm launcher integration for seamless autorestore
**Phase 4**: Enhanced multi-interpreter support using IosFrotz comprehensive approach

This approach provides the fastest path to working autosave while maintaining compatibility with proven implementations.

## Error Handling

### Autosave failures:
- Log error but continue game execution
- Don't overwrite existing good autosave with corrupted data
- Use atomic file operations (write to temp, then rename)

### Autorestore failures:
- Fall back to normal game startup
- Log error for debugging
- Clean up corrupted autosave files

## Compatibility

### With existing GLK code:
- All existing GLK functions continue to work unchanged
- Autosave features are optional (behind `GLKUNIX_AUTOSAVE_FEATURES`)
- No impact on games that don't use autosave
- **No modifications to existing player-managed save/restore commands or functionality**

### With other interpreters:
- Autosave files are interpreter-specific (not portable)
- Standard player-managed save files remain fully portable and unchanged
- API follows same patterns as iOSGlk for consistency
- **Interpreter compatibility**:
  - **Glulxe**: Full autosave support (already has the necessary hooks and code)
  - **Other interpreters**: Will continue to work unchanged, but without autosave features
  - **No modifications required**: Existing interpreters don't need any changes to continue functioning
  - **Opt-in autosave**: Interpreters would need modifications to gain autosave support, but this is optional

## Testing Strategy

### Unit tests:
- Serialization/unserialization of individual objects
- Update tag generation and lookup
- Context handling

### Integration tests:
- Save/restore complete GLK state
- File stream reopening after restore
- Memory stream buffer restoration

### End-to-end tests:
- Glulxe autosave/autorestore cycles
- Interruption and recovery scenarios
- Large game state handling
- **Coexistence testing**: Verify autosave doesn't interfere with player-managed saves
- **Isolation testing**: Ensure traditional SAVE/RESTORE commands work unchanged

## Performance Considerations

### Autosave frequency:
- Only during `glk_select()` calls
- Skip if last event was arrange/timer (configurable)
- Throttle to prevent excessive I/O

### Memory usage:
- Stream buffer handling
- Large scrollback buffer management
- Temporary object creation during serialization

### File I/O:
- Use buffered I/O for large serializations
- Compress state data if beneficial
- Atomic file operations to prevent corruption

## Future Extensions

### Cross-session features:
- Save window layouts between sessions
- Restore game position on restart
- Session history and bookmarks

### Cloud save integration:
- Upload autosave to cloud storage
- Sync across devices
- Conflict resolution

### Enhanced debugging:
- Autosave integrity checking
- State diff visualization
- Performance profiling tools

## Upstream and Related Project References

- [GLK Specification](https://www.eblong.com/zarf/glk/Glk-Spec-076.html) — Core API spec
- [Interpreter-Managed Saves in Glulx](https://www.eblong.com/zarf/glk/terp-saving-notes.html) — Autosave spec
- [iOSGlk](https://github.com/erkyrath/iosglk) — Reference iOS GLK implementation
- [iOSGlulxe](https://github.com/erkyrath/iosglulxe) — iOS Glulxe with autosave
- [IosFrotz](https://github.com/ifrotz/iosfrotz) — Multi-interpreter iOS GLK with autosave
- [Fizmo](https://github.com/erkyrath/fizmo) — Z-machine interpreter with GLK and autosave (see `fizmo-iosglk`)
- [iOSFizmo](https://github.com/erkyrath/fizmo/tree/main/fizmo-iosglk) — iOS GLK glue for Fizmo, autosave implementation
- [Glulxe autosave code](https://github.com/erkyrath/glulxe) — See `unixautosave.c`, `unixstrt.c`

## Glossary

- **GLK**: The Glk API/library for interactive fiction I/O
- **Autosave/Autorestore**: Interpreter-managed, background save/restore of both VM and UI state
- **Player-managed save**: Traditional SAVE/RESTORE commands, only VM state
- **Screen interface**: Struct of function pointers for UI/GLK operations, including autosave hooks
- **VM**: Virtual Machine (e.g., Glulx, Z-machine)
- **iOSGlk**: Objective-C implementation of Glk for iOS
- **Fizmo**: Z-machine interpreter with modular screen interface

## Interpreter Autosave Support Matrix

| Interpreter         | Autosave Support | Integration Pattern / Notes                                 |
|---------------------|------------------|-------------------------------------------------------------|
| Glulxe              | Yes              | Built-in, just needs GLK hooks                              |
| Fizmo               | Yes (iOS)        | Screen interface hooks (`do_autosave`, `restore_autosave`)  |
| Git                 | Partial (iOS)    | Needs custom hooks, see IosFrotz                            |
| Frotz               | Partial (iOS)    | Needs custom hooks, see IosFrotz                            |
| Hugo, Agility, etc. | Varies           | See IosFrotz for patterns                                   |

## Implementation & Testing Checklist

- [ ] Implement screen interface autosave hooks
- [ ] Serialize/deserialize all GLK object types
- [ ] Integrate with Glulxe autosave hooks
- [ ] Validate with Glulxe games (autosave/restore)
- [ ] Validate player-managed saves remain unaffected
- [ ] Add error handling and atomic file operations
- [ ] Document interpreter-specific integration steps
- [ ] Add multi-interpreter support (optional, see IosFrotz)

## Key Source Files and Functions (Upstream)

- **iOSGlulxe**: `iosglulxe_autosave.m`, `glkunix_do_autosave()`
- **IosFrotz**: `glkios/glkautosave.c`, interpreter main loops
- **Fizmo**: `fizmo-iosglk/ios-autosave.m`, `screen_interface.h`, `fizmo-iosglk.m`
- **Glulxe**: `unixautosave.c`, `unixstrt.c`

## Open Questions / TODOs / Future Research

- [ ] Should we support autosave for interpreters other than Glulxe and Fizmo? If so, which ones and how?
- [ ] What is the best way to handle GLK state versioning for future compatibility?
- [ ] Should we provide a migration path for existing player-managed saves if autosave is enabled?
- [ ] Are there edge cases in file/stream restoration that need more robust handling?
- [ ] What are the best practices for cloud save or cross-device autosave in the future?

---
