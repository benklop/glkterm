# GLK State Serialization Implementation Plan

## Analysis of GLK Object Structures

Based on examination of `glkterm.h`, the core GLK objects we need to serialize are:

### 1. Window Structure (`struct glk_window_struct`)
```c
struct glk_window_struct {
    glui32 magicnum;           // Skip - runtime only
    glui32 rock;               // ✓ Serialize
    glui32 type;               // ✓ Serialize
    grect_t bbox;              // ✓ Serialize (can be recalculated but preserve)
    window_t *parent;          // ✓ Serialize (as update tag)
    void *data;                // ✓ Serialize (type-specific data)
    stream_t *str;             // ✓ Serialize (as update tag)
    stream_t *echostr;         // ✓ Serialize (as update tag)
    
    // Input states
    int line_request;          // ✓ Serialize
    int line_request_uni;      // ✓ Serialize  
    int char_request;          // ✓ Serialize
    int char_request_uni;      // ✓ Serialize
    int echo_line_input;       // ✓ Serialize
    glui32 terminate_line_input; // ✓ Serialize
    
    styleplus_t styleplus;     // ✓ Serialize
    struct stylehint_struct *stylehints; // ✓ Serialize
    
    gidispatch_rock_t disprock; // ✓ Serialize
    window_t *next, *prev;     // Skip - reconstructed
};
```

### 2. Stream Structure (`struct glk_stream_struct`)
```c
struct glk_stream_struct {
    glui32 magicnum;          // Skip - runtime only
    glui32 rock;              // ✓ Serialize
    int type;                 // ✓ Serialize
    int unicode;              // ✓ Serialize
    glui32 readcount, writecount; // ✓ Serialize
    int readable, writable;   // ✓ Serialize
    
    // Type-specific data
    window_t *win;            // ✓ Serialize (as update tag, for window streams)
    FILE *file;               // Skip - will reopen
    char *filename;           // ✓ Serialize (for file streams)
    glui32 lastop;            // ✓ Serialize
    
    // Memory stream data
    unsigned char *buf;       // ✓ Serialize (contents)
    glui32 buflen;            // ✓ Serialize
    // ... positions and pointers -> ✓ Serialize as offsets
    
    gidispatch_rock_t disprock; // ✓ Serialize
    stream_t *next, *prev;    // Skip - reconstructed
};
```

### 3. Fileref Structure (`struct glk_fileref_struct`)
```c
struct glk_fileref_struct {
    glui32 magicnum;          // Skip - runtime only
    glui32 rock;              // ✓ Serialize
    char *filename;           // ✓ Serialize
    int filetype;             // ✓ Serialize
    int textmode;             // ✓ Serialize
    gidispatch_rock_t disprock; // ✓ Serialize
    fileref_t *next, *prev;   // Skip - reconstructed
};
```

### 4. Sound Channel Structure (`struct glk_schannel_struct`)
```c
struct glk_schannel_struct {
    glui32 magicnum;          // Skip - runtime only
    glui32 rock;              // ✓ Serialize
    int paused;               // ✓ Serialize
    glui32 volume_current;    // ✓ Serialize
    // ... other sound state
    gidispatch_rock_t disprock; // ✓ Serialize
};
```

---

## Implementation Strategy

### Phase 1: Foundation (IMMEDIATE - ~8 hours)

#### 1.1 Update Tag Persistence
- [ ] **Modify update tag system** to be deterministic
- [ ] **Use object rock values + type** as stable identifiers
- [ ] **Create tag-to-object mapping table** that survives sessions

```c
// New approach: use rock + type as stable ID
glui32 create_stable_updatetag(glui32 rock, glui32 type) {
    return (type << 24) | (rock & 0x00FFFFFF);
}
```

#### 1.2 Serialization Framework Enhancement
- [ ] **Add type-specific serialization dispatch**
- [ ] **Create versioned format headers**
- [ ] **Add error recovery and validation**

### Phase 2: Core Object Serialization (~16 hours)

#### 2.1 Window Serialization (Priority 1)
```c
int serialize_window(glkunix_serialize_context_t ctx, window_t *win) {
    // Basic properties
    glkunix_serialize_uint32(ctx, "rock", win->rock);
    glkunix_serialize_uint32(ctx, "type", win->type);
    
    // Relationships (as update tags)
    glui32 parent_tag = win->parent ? create_stable_updatetag(win->parent->rock, win->parent->type) : 0;
    glkunix_serialize_uint32(ctx, "parent_tag", parent_tag);
    
    // Type-specific data
    switch (win->type) {
        case wintype_TextBuffer:
            return serialize_textbuffer_data(ctx, (window_textbuffer_t*)win->data);
        case wintype_TextGrid:
            return serialize_textgrid_data(ctx, (window_textgrid_t*)win->data);
        case wintype_Pair:
            return serialize_pair_data(ctx, (window_pair_t*)win->data);
        case wintype_Blank:
            return 1; // No additional data
    }
}
```

#### 2.2 Stream Serialization
```c
int serialize_stream(glkunix_serialize_context_t ctx, stream_t *str) {
    // Basic properties
    glkunix_serialize_uint32(ctx, "rock", str->rock);
    glkunix_serialize_uint32(ctx, "type", str->type);
    glkunix_serialize_uint32(ctx, "readcount", str->readcount);
    glkunix_serialize_uint32(ctx, "writecount", str->writecount);
    
    // Type-specific data
    switch (str->type) {
        case strtype_File:
            glkunix_serialize_buffer(ctx, "filename", str->filename, strlen(str->filename));
            glkunix_serialize_uint32(ctx, "lastop", str->lastop);
            // TODO: serialize file position
            break;
        case strtype_Memory:
            glkunix_serialize_buffer(ctx, "buffer", str->buf, str->buflen);
            // TODO: serialize positions as offsets
            break;
        case strtype_Window:
            // Serialize associated window tag
            glui32 win_tag = create_stable_updatetag(str->win->rock, str->win->type);
            glkunix_serialize_uint32(ctx, "window_tag", win_tag);
            break;
    }
}
```

#### 2.3 Fileref Serialization
```c
int serialize_fileref(glkunix_serialize_context_t ctx, fileref_t *fref) {
    glkunix_serialize_uint32(ctx, "rock", fref->rock);
    glkunix_serialize_buffer(ctx, "filename", fref->filename, strlen(fref->filename));
    glkunix_serialize_uint32(ctx, "filetype", fref->filetype);
    glkunix_serialize_uint32(ctx, "textmode", fref->textmode);
    return 1;
}
```

### Phase 3: Window Type-Specific Data (~12 hours)

#### 3.1 Text Buffer Window Data
- [ ] **Serialize text content** (all lines)
- [ ] **Serialize formatting** (style runs)
- [ ] **Serialize cursor position**
- [ ] **Serialize scroll position**

#### 3.2 Text Grid Window Data  
- [ ] **Serialize grid contents** (characters + styles)
- [ ] **Serialize grid dimensions**
- [ ] **Serialize cursor position**

#### 3.3 Pair Window Data
- [ ] **Serialize split configuration** (direction, method, size)
- [ ] **Serialize child window references**

### Phase 4: Restoration Implementation (~16 hours)

#### 4.1 Object Recreation
- [ ] **Recreate GLK objects** in correct order
- [ ] **Restore object relationships** (parent/child, stream associations)
- [ ] **Restore object properties** and content

#### 4.2 State Restoration
- [ ] **Restore window hierarchy** and layout
- [ ] **Restore text content** and formatting
- [ ] **Restore input states** and cursor positions
- [ ] **Restore file streams** (reopen files, seek to positions)

---

## Implementation Files Plan

### New Files to Create
1. `glkterm/gtautosave_windows.c` - Window serialization
2. `glkterm/gtautosave_streams.c` - Stream serialization  
3. `glkterm/gtautosave_restore.c` - Restoration logic
4. `glkterm/gtautosave_format.h` - File format definitions

### Files to Modify
1. `glkterm/gtautosave.c` - Enhance main serialization functions
2. `glkterm/glkunix_autosave.h` - Add new function declarations
3. `CMakeLists.txt` - Add new source files

---

## Testing Strategy

### Phase 1 Testing
- [ ] **Unit tests** for serialization functions
- [ ] **Mock GLK objects** for testing
- [ ] **Format validation** tests

### Phase 2 Testing  
- [ ] **Simple window creation/save/restore** cycle
- [ ] **Text buffer content** preservation
- [ ] **Multiple window** configurations

### Phase 3 Testing
- [ ] **Complex window hierarchies**
- [ ] **File stream** save/restore
- [ ] **Memory stream** content preservation  
- [ ] **Real Glulx games** autosave testing

---

## Success Criteria

✅ **Minimal Viable Product**:
- Single text buffer window save/restore works
- Basic window properties preserved
- File streams can be reopened

✅ **Production Ready**:
- Complex window hierarchies work
- All window types supported  
- Memory streams preserve content
- File streams restore positions
- No memory leaks or corruption

✅ **Full Feature Complete**:
- Sound channels supported
- Style hints preserved  
- Performance optimized
- Comprehensive error handling

---

## Estimated Timeline

- **Phase 1**: 1-2 days (foundation)
- **Phase 2**: 2-3 days (core serialization)  
- **Phase 3**: 2-3 days (window-specific data)
- **Phase 4**: 2-3 days (restoration)
- **Testing/Polish**: 1-2 days

**Total**: ~10-15 days for production-ready implementation
