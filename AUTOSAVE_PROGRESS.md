# GLK Autosave Implementation Progress

## Project Overview
Implementation of autosave/autorestore functionality in glkterm library to support interpreter autosave features like Glulxe's `GLKUNIX_AUTOSAVE_FEATURES`.

**Goal**: Enable automatic save/restore of both GLK library state and interpreter state for seamless game session continuity.

---

## ✅ COMPLETED WORK

### 1. Infrastructure & Build System
- [x] **CMake Integration**: Added autosave sources to build system
- [x] **Feature Flags**: Enabled `GLKUNIX_AUTOSAVE_FEATURES` 
- [x] **Header Structure**: Created comprehensive `glkunix_autosave.h`
- [x] **C99 Upgrade**: Updated build to support modern C features
- [x] **Glulxe Integration**: Included `unixautosave.c` in Glulxe build

### 2. API Framework  
- [x] **Function Signatures**: All required autosave API functions declared
- [x] **Context Types**: Serialization/unserialization context structures
- [x] **Event Tracking**: Last event type tracking for autosave decisions
- [x] **Update Tags**: 64-bit safe object identification system (IMPROVED)
- [x] **Memory Management**: Hash table for object-to-tag mapping
- [x] **Cross-platform**: Endian-safe serialization format

### 3. Basic Functionality
- [x] **Event Integration**: Modified `gtevent.c` to call `gli_set_last_event_type()`
- [x] **Object Tracking**: Get/set update tags for windows, streams, filerefs
- [x] **Primitive Serialization**: uint32, buffer serialization functions
- [x] **Dispatch Rocks**: Set/get dispatch rocks for GLK objects
- [x] **Error Handling**: Memory allocation failures, file I/O errors
- [x] **Cleanup Functions**: Memory deallocation for hash table

### 4. Glulxe Compatibility
- [x] **API Compliance**: All functions Glulxe expects are implemented
- [x] **Command Line Options**: `--autosave`, `--autorestore` available
- [x] **Type Safety**: Fixed parameter type mismatches
- [x] **Build Success**: Glulxe compiles and links with autosave support

### 5. **NEW: Persistent Update Tags** 🆕
- [x] **Deterministic Tags**: Update tags now based on rock+type, not memory addresses
- [x] **Cross-session Stability**: Same objects get same tags across different runs
- [x] **Object Type System**: Defined constants for different GLK object types
- [x] **Backward Compatibility**: Legacy functions preserved for existing code

---

## ⚠️ STUB IMPLEMENTATIONS (Critical Gaps)

### 1. GLK State Serialization - **NOT IMPLEMENTED**
```c
int glkunix_serialize_library_state(glkunix_serialize_context_t context)
{
    /* TODO: Serialize actual GLK library state */
    return glkunix_serialize_uint32(context, "glk_state_version", 1); // STUB!
}
```

**Missing**: 
- Window hierarchy and properties
- Stream states and file positions  
- Fileref states
- Sound channel states
- Style hint configurations
- Input line states

### 2. GLK State Restoration - **NOT IMPLEMENTED**
```c
int glkunix_unserialize_library_state(glkunix_unserialize_context_t context)
{
    /* TODO: Unserialize actual GLK library state */
    glui32 version;
    return glkunix_unserialize_uint32(context, "glk_state_version", &version); // STUB!
}
```

**Missing**:
- Recreating window hierarchy
- Restoring stream positions and contents
- Restoring fileref states
- Reinitializing sound channels
- Applying style configurations

### 3. Object-specific Serialization - **NOT IMPLEMENTED**
```c
int glkunix_unserialize_object(glkunix_unserialize_context_t context, glui32 tag, void *object)
{
    /* TODO: Implement object unserialization */
    return 1; // STUB!
}
```

### 4. Library State Management - **INCOMPLETE**
```c
glkunix_library_state_t glkunix_load_library_state(strid_t file, ...)
{
    // Only allocates empty state structure - doesn't read from file
    glkunix_library_state_struct *state = malloc(sizeof(glkunix_library_state_struct));
    memset(state, 0, sizeof(glkunix_library_state_struct));
    return (glkunix_library_state_t)state; // STUB!
}
```

---

## 🔧 ARCHITECTURAL LIMITATIONS

### 1. API Design Issue
```c
int glkunix_unserialize_object_list_entries(void *array,
    int (*unserialize_obj)(glkunix_unserialize_context_t ctx, void *obj),
    glui32 count, glui32 objsize, void *list)
```
**Problem**: Function needs file context but API doesn't provide it.
**Current**: Uses dummy context with NULL file pointer.

### 2. **UPDATE:** Update Tag Persistence - **PARTIALLY FIXED** ✅
**Current**: Update tags now use deterministic rock+type formula instead of memory addresses.
**Fixed**: Objects get consistent tags across different sessions.
**Remaining**: Need to handle rock collisions and validate tag uniqueness.

---

## 📋 WORK REMAINING

### Phase 1: Core GLK State Serialization (HIGH PRIORITY)

#### 1.1 Window State Serialization
- [ ] Serialize window hierarchy (parent/child relationships)
- [ ] Serialize window types, sizes, positions
- [ ] Serialize window contents (text buffers, grid contents)
- [ ] Serialize window styles and formatting
- [ ] Serialize cursor positions

#### 1.2 Stream State Serialization  
- [ ] Serialize file stream positions
- [ ] Serialize memory stream contents and positions
- [ ] Serialize window stream associations
- [ ] Serialize stream read/write modes

#### 1.3 Fileref State Serialization
- [ ] Serialize file references and paths
- [ ] Serialize file types and usage modes
- [ ] Serialize file existence states

### Phase 2: Object Management (MEDIUM PRIORITY)

#### 2.1 Update Tag Persistence
- [ ] Design persistent tag assignment system
- [ ] Serialize update tag mappings to file
- [ ] Restore tag mappings on load
- [ ] Handle tag conflicts across sessions

#### 2.2 Object Lifecycle Management
- [ ] Track object creation/destruction
- [ ] Handle object recreation during restore
- [ ] Manage object reference consistency

### Phase 3: Advanced Features (LOW PRIORITY)

#### 3.1 Sound Channel Support
- [ ] Serialize sound channel states
- [ ] Handle sound resource references
- [ ] Restore audio playback states

#### 3.2 Style Hint Management
- [ ] Serialize style hint configurations
- [ ] Restore text formatting preferences

---

## 🎯 IMMEDIATE NEXT STEPS

### Step 1: Analyze GLK State Structure
```bash
# Study glkterm internal structures
grep -r "typedef.*struct" glkterm/
grep -r "window_t\|stream_t\|fileref_t" glkterm/
```

### Step 2: Design State Serialization Format
- Define binary format for each GLK object type
- Plan versioning strategy for format evolution
- Design error recovery mechanisms

### Step 3: Implement Window Serialization (Start Here)
- Begin with `glkunix_serialize_library_state()`
- Focus on window hierarchy as most critical component
- Add comprehensive window state serialization

---

## 🚨 CRITICAL ASSESSMENT

**Current Status**: **NOT PRODUCTION READY**

While the infrastructure is solid and Glulxe integration works, the autosave functionality is essentially non-functional because:

1. **No GLK state is actually saved** - only a version number
2. **No GLK state is restored** - library state remains unchanged  
3. **Object restoration is broken** - update tags are not persistent
4. **Interpreter state depends on GLK state** - incomplete GLK restoration breaks VM state

**Recommendation**: Complete at minimum Phase 1 (Core GLK State Serialization) before considering the feature usable.

---

## 📊 COMPLETION ESTIMATE

- **Infrastructure**: 100% ✅
- **API Framework**: 100% ✅  
- **Update Tag System**: 85% ✅ (improved from 30%)
- **GLK State Serialization**: 5% ⚠️
- **Object Management**: 45% ✅ (improved from 30%)
- **Production Readiness**: 25% ⚠️ (improved from 15%)

**Estimated work remaining**: ~30-50 hours for basic functionality, ~60-100 hours for full implementation.
