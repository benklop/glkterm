/* gtautosave.c: Autosave/autorestore support for GlkTerm
    Implementation of GLK UI state serialization to support
    interpreter autosave functionality like Glulxe's GLKUNIX_AUTOSAVE_FEATURES
*/

#include "glk.h"
#include "glkterm.h"
#include "glkunix_autosave.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Global state variables 
 * NOTE: These are not thread-safe by design, consistent with the rest of glkterm.
 * The library assumes single-threaded usage except for audio callbacks.
 */
static glui32 gli_last_event_type = 0xFFFFFFFF; /* No event yet */
static glui32 gli_autosave_tag = 0; /* Current autosave tag */

/* Update tag mapping - use a deterministic approach for cross-session persistence
 * Instead of using memory addresses, use rock+type for stable identification
 * No synchronization needed - follows glkterm's single-threaded design
 */
#define UPDATETAG_HASH_SIZE 1024

/* Object type constants for stable tagging */
#define OBJTYPE_WINDOW 1
#define OBJTYPE_STREAM 2  
#define OBJTYPE_FILEREF 3
#define OBJTYPE_SCHANNEL 4

static struct updatetag_entry {
    void *object;
    glui32 tag;
    struct updatetag_entry *next;
} *updatetag_table[UPDATETAG_HASH_SIZE];

/* Create deterministic update tag from rock and object type */
static glui32 create_stable_updatetag(glui32 rock, glui32 objtype)
{
    /* Use high byte for object type, low 24 bits for rock value
     * This ensures the same object always gets the same tag across sessions */
    return (objtype << 24) | (rock & 0x00FFFFFF);
}

/* Internal autosave registry functions */
static long (*gli_locate_arr_fn)(void *array, glui32 len, char *typecode,
    gidispatch_rock_t objrock, int *elemsizeref) = NULL;
static gidispatch_rock_t (*gli_restore_arr_fn)(long bufkey, glui32 len,
    char *typecode, void **arrayref) = NULL;

/* Hash function for update tag table */
static unsigned int updatetag_hash(void *object)
{
    uintptr_t addr = (uintptr_t)object;
    return (unsigned int)((addr >> 3) % UPDATETAG_HASH_SIZE);
}

/* Get or create an update tag for an object using its rock and type */
static glui32 get_or_create_updatetag_for_object(void *object, glui32 rock, glui32 objtype)
{
    if (!object) return 0;
    
    /* Create stable tag from rock and type */
    glui32 stable_tag = create_stable_updatetag(rock, objtype);
    
    unsigned int hash = updatetag_hash(object);
    struct updatetag_entry *entry = updatetag_table[hash];
    
    /* Search for existing entry */
    while (entry) {
        if (entry->object == object) {
            return entry->tag;
        }
        entry = entry->next;
    }
    
    /* Create new entry with stable tag */
    entry = malloc(sizeof(struct updatetag_entry));
    if (!entry) {
        /* Memory allocation failed - return 0 to indicate error */
        return 0;
    }
    
    entry->object = object;
    entry->tag = stable_tag;
    entry->next = updatetag_table[hash];
    updatetag_table[hash] = entry;
    
    return stable_tag;
}

/* Legacy function - kept for compatibility but should be avoided */
static glui32 get_or_create_updatetag(void *object)
{
    /* This function can't create stable tags without knowing object type
     * Return a hash-based tag for now - not persistent across sessions */
    if (!object) return 0;
    
    unsigned int hash = updatetag_hash(object);
    struct updatetag_entry *entry = updatetag_table[hash];
    
    /* Search for existing entry */
    while (entry) {
        if (entry->object == object) {
            return entry->tag;
        }
        entry = entry->next;
    }
    
    /* Create entry with hash-based tag (not persistent) */
    entry = malloc(sizeof(struct updatetag_entry));
    if (!entry) return 0;
    
    entry->object = object;
    entry->tag = (glui32)((uintptr_t)object & 0xFFFFFFFF); /* Fallback hash */
    entry->next = updatetag_table[hash];
    updatetag_table[hash] = entry;
    
    return entry->tag;
}

/* Find object by update tag */
static void *find_object_by_updatetag(glui32 tag)
{
    if (tag == 0) return NULL;
    
    for (int i = 0; i < UPDATETAG_HASH_SIZE; i++) {
        struct updatetag_entry *entry = updatetag_table[i];
        while (entry) {
            if (entry->tag == tag) {
                return entry->object;
            }
            entry = entry->next;
        }
    }
    return NULL;
}

void gidispatch_set_autorestore_registry_impl(
    long (*locatearr)(void *array, glui32 len, char *typecode,
        gidispatch_rock_t objrock, int *elemsizeref),
    gidispatch_rock_t (*restorearr)(long bufkey, glui32 len,
        char *typecode, void **arrayref))
{
    gli_locate_arr_fn = locatearr;
    gli_restore_arr_fn = restorearr;
}

/* Get the last event type for autosave decision making */
glui32 glkunix_get_last_event_type(void)
{
    return gli_last_event_type;
}

/* Set the last event type (called from event handling code) */
void gli_set_last_event_type(glui32 event_type)
{
    gli_last_event_type = event_type;
}

/* Autosave tag functions */
glui32 glkunix_get_autosave_tag(void)
{
    return gli_autosave_tag;
}

void glkunix_set_autosave_tag(glui32 tag)
{
    gli_autosave_tag = tag;
}

/* Library state serialization functions */
int glkunix_serialize_library_state(glkunix_serialize_context_t context)
{
    /* TODO: Serialize actual GLK library state */
    /* For now, just write a placeholder */
    return glkunix_serialize_uint32(context, "glk_state_version", 1);
}

int glkunix_unserialize_library_state(glkunix_unserialize_context_t context)
{
    /* TODO: Unserialize actual GLK library state */
    glui32 version;
    return glkunix_unserialize_uint32(context, "glk_state_version", &version);
}

/* Object serialization functions */
glui32 glkunix_serialize_object_tag(glkunix_serialize_context_t context, void *object)
{
    /* Get the update tag for this object and serialize it */
    glui32 tag = get_or_create_updatetag(object);
    if (!glkunix_serialize_uint32(context, "object_tag", tag)) {
        return 0;
    }
    return tag;
}

int glkunix_unserialize_object(glkunix_unserialize_context_t context, glui32 tag, void *object)
{
    /* TODO: Implement object unserialization */
    /* For now, just return success */
    return 1;
}

/* Update tag functions for tracking GLK objects across saves/restores */
glui32 glkunix_stream_get_updatetag(strid_t str)
{
    if (!str) return 0;
    return get_or_create_updatetag_for_object(str, str->rock, OBJTYPE_STREAM);
}

glui32 glkunix_window_get_updatetag(winid_t win)
{
    if (!win) return 0;
    return get_or_create_updatetag_for_object(win, win->rock, OBJTYPE_WINDOW);
}

glui32 glkunix_fileref_get_updatetag(frefid_t fref)
{
    if (!fref) return 0;
    return get_or_create_updatetag_for_object(fref, fref->rock, OBJTYPE_FILEREF);
}

winid_t glkunix_window_find_by_updatetag(glui32 tag)
{
    return (winid_t)find_object_by_updatetag(tag);
}

strid_t glkunix_stream_find_by_updatetag(glui32 tag)
{
    return (strid_t)find_object_by_updatetag(tag);
}

frefid_t glkunix_fileref_find_by_updatetag(glui32 tag)
{
    return (frefid_t)find_object_by_updatetag(tag);
}

void glkunix_window_set_dispatch_rock(winid_t win, gidispatch_rock_t rock)
{
    if (win) {
        win->disprock = rock;
    }
}

void glkunix_stream_set_dispatch_rock(strid_t str, gidispatch_rock_t rock)
{
    if (str) {
        str->disprock = rock;
    }
}

void glkunix_fileref_set_dispatch_rock(frefid_t fref, gidispatch_rock_t rock)
{
    if (fref) {
        fref->disprock = rock;
    }
}

/* Context serialization functions */
int glkunix_serialize_uint32(glkunix_serialize_context_t ctx, char *id, glui32 value)
{
    /* Write in big-endian format for cross-platform compatibility */
    unsigned char bytes[4];
    bytes[0] = (value >> 24) & 0xFF;
    bytes[1] = (value >> 16) & 0xFF;
    bytes[2] = (value >> 8) & 0xFF;
    bytes[3] = value & 0xFF;
    
    if (fwrite(bytes, 4, 1, ctx.file) != 1) {
        return 0;
    }
    if (ctx.write_count) {
        *(ctx.write_count) += 4;
    }
    return 1;
}

int glkunix_unserialize_uint32(glkunix_unserialize_context_t ctx, char *id, glui32 *value)
{
    unsigned char bytes[4];
    if (fread(bytes, 4, 1, ctx.file) != 1) {
        return 0;
    }
    
    /* Read from big-endian format */
    *value = ((glui32)bytes[0] << 24) | 
             ((glui32)bytes[1] << 16) | 
             ((glui32)bytes[2] << 8) | 
             (glui32)bytes[3];
    
    if (ctx.read_count) {
        *(ctx.read_count) += 4;
    }
    return 1;
}

int glkunix_serialize_buffer(glkunix_serialize_context_t ctx, char *id, void *buffer, glui32 length)
{
    if (!glkunix_serialize_uint32(ctx, NULL, length)) {
        return 0;
    }
    if (length > 0) {
        if (fwrite(buffer, 1, length, ctx.file) != length) {
            return 0;
        }
        if (ctx.write_count) {
            *(ctx.write_count) += length;
        }
    }
    return 1;
}

int glkunix_unserialize_buffer(glkunix_unserialize_context_t ctx, char *id, void *buffer, glui32 length)
{
    glui32 stored_length;
    if (!glkunix_unserialize_uint32(ctx, NULL, &stored_length)) {
        return 0;
    }
    if (stored_length != length) {
        return 0; /* Length mismatch */
    }
    if (length > 0) {
        if (fread(buffer, 1, length, ctx.file) != length) {
            return 0;
        }
        if (ctx.read_count) {
            *(ctx.read_count) += length;
        }
    }
    return 1;
}

int glkunix_serialize_object_list(glkunix_serialize_context_t ctx, char *id,
    int (*serialize_obj)(glkunix_serialize_context_t ctx, void *obj),
    glui32 count, glui32 objsize, void *list)
{
    if (!glkunix_serialize_uint32(ctx, NULL, count)) {
        return 0;
    }
    
    char *objptr = (char *)list;
    for (glui32 i = 0; i < count; i++) {
        if (!serialize_obj(ctx, objptr)) {
            return 0;
        }
        objptr += objsize;
    }
    return 1;
}

int glkunix_unserialize_list(glkunix_unserialize_context_t ctx, char *id, void **array, glui32 *count)
{
    /* Read the count first */
    if (!glkunix_unserialize_uint32(ctx, NULL, count)) {
        return 0;
    }
    
    /* If count is 0, set array to NULL */
    if (*count == 0) {
        *array = NULL;
        return 1;
    }
    
    /* For now, we just read the count and let the caller handle allocation.
     * In Glulxe's usage, this function is used to get the count, then
     * glkunix_unserialize_object_list_entries is called to populate the objects.
     * We'll allocate a minimal placeholder that can be used by the caller. */
    *array = malloc(sizeof(void*));
    if (!*array) {
        return 0;
    }
    
    return 1;
}

int glkunix_unserialize_object_list_entries(void *array,
    int (*unserialize_obj)(glkunix_unserialize_context_t ctx, void *obj),
    glui32 count, glui32 objsize, void *list)
{
    /* This function should iterate through objects and call unserialize_obj for each.
     * Since we don't have a proper context here, we'll create a stub context.
     * In a full implementation, this would need the actual file context. */
    
    if (count == 0 || !list || !unserialize_obj) {
        return 1; /* Nothing to do */
    }
    
    /* For now, create a dummy context. This is not ideal but allows compilation.
     * A proper implementation would require restructuring the API to pass context. */
    glkunix_unserialize_context_t dummy_ctx = { .file = NULL, .read_count = NULL };
    
    char *objptr = (char *)list;
    for (glui32 i = 0; i < count; i++) {
        if (!unserialize_obj(dummy_ctx, objptr)) {
            return 0;
        }
        objptr += objsize;
    }
    
    return 1;
}

/* Library state management */
typedef struct glkunix_library_state_struct {
    glui32 window_count;
    glui32 stream_count;
    glui32 fileref_count;
} glkunix_library_state_struct;

glkunix_library_state_t glkunix_load_library_state(strid_t file,
    int (*unserialize_extra)(glkunix_unserialize_context_t ctx, void *rock), 
    void *rock)
{
    glkunix_library_state_struct *state = malloc(sizeof(glkunix_library_state_struct));
    if (!state) return NULL;
    
    memset(state, 0, sizeof(glkunix_library_state_struct));
    
    /* TODO: Read actual GLK state from file */
    
    return (glkunix_library_state_t)state;
}

int glkunix_save_library_state(strid_t jsavefile, strid_t jresultfile,
    int (*serialize_extra)(glkunix_serialize_context_t ctx, void *rock),
    void *rock)
{
    FILE *file = (jsavefile && jsavefile->type == strtype_File) ? jsavefile->file : NULL;
    if (!file) return 0;
    
    glui32 write_count = 0;
    glkunix_serialize_context_t ctx = { .file = file, .write_count = &write_count };
    
    const char *header = "GLKTERM_STATE_V1";
    if (fwrite(header, 1, strlen(header), file) != strlen(header)) {
        return 0;
    }
    
    /* Call the extra serialization function if provided */
    if (serialize_extra && !serialize_extra(ctx, rock)) {
        return 0;
    }
    
    return 1;
}

int glkunix_update_from_library_state(glkunix_library_state_t library_state)
{
    if (!library_state) return 0;
    
    /* TODO: Update current GLK state from library_state */
    
    return 1;
}

void glkunix_library_state_free(glkunix_library_state_t library_state)
{
    if (library_state) {
        free(library_state);
    }
}

/* Required by some autosave implementations */
int giblorb_unset_resource_map(void) {
    return 1;
}

/* Cleanup function to free update tag table */
void glkunix_autosave_cleanup(void)
{
    for (int i = 0; i < UPDATETAG_HASH_SIZE; i++) {
        struct updatetag_entry *entry = updatetag_table[i];
        while (entry) {
            struct updatetag_entry *next = entry->next;
            free(entry);
            entry = next;
        }
        updatetag_table[i] = NULL;
    }
    /* Note: No need to reset next_update_tag since we now use deterministic tags */
}
