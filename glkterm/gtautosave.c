/* gtautosave.c: Autosave/autorestore support for GlkTerm
    Implementation of GLK UI state serialization to support
    interpreter autosave functionality like Glulxe's GLKUNIX_AUTOSAVE_FEATURES
*/

#include "glk.h"
#include "glkterm.h"
#include "glkunix_autosave.h"
#include "gtw_buf.h"
#include "gtw_grid.h"
#include "gtw_pair.h"
#include "gtw_blnk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Forward declarations for internal serialization functions */
static int serialize_window_list(glkunix_serialize_context_t context);
static int serialize_stream_list(glkunix_serialize_context_t context);
static int serialize_fileref_list(glkunix_serialize_context_t context);
static int serialize_window(glkunix_serialize_context_t context, window_t *win);
static int serialize_window_data(glkunix_serialize_context_t context, window_t *win);
static int serialize_textbuffer_data(glkunix_serialize_context_t context, window_textbuffer_t *dwin);
static int serialize_textgrid_data(glkunix_serialize_context_t context, window_textgrid_t *dwin);
static int serialize_pair_data(glkunix_serialize_context_t context, window_pair_t *dwin);
static int serialize_blank_data(glkunix_serialize_context_t context, window_blank_t *dwin);
static int serialize_stream(glkunix_serialize_context_t context, stream_t *str);
static int serialize_fileref(glkunix_serialize_context_t context, fileref_t *fref);
static glui32 gli_get_update_tag(void *obj, int objtype);

/* Forward declarations for unserialization functions */
static int unserialize_window_list(glkunix_unserialize_context_t context);
static int unserialize_stream_list(glkunix_unserialize_context_t context);
static int unserialize_fileref_list(glkunix_unserialize_context_t context);
static int unserialize_window(glkunix_unserialize_context_t context);
static int unserialize_stream(glkunix_unserialize_context_t context);
static int unserialize_fileref(glkunix_unserialize_context_t context);
static int unserialize_window_data(glkunix_unserialize_context_t context, window_t *win);
static int restore_window_hierarchy(glkunix_unserialize_context_t context);

/* Accessor for current stream (since it's static in gtstream.c) */
extern stream_t *glk_stream_get_current(void);

/* Accessor functions for static object lists (internal only) */
extern window_t *glkunix_get_windowlist(void);
extern stream_t *glkunix_get_streamlist(void); 
extern fileref_t *glkunix_get_filereflist(void);

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

#define MAX_HIERARCHY_ENTRIES 100

/* Temporary structure for storing window hierarchy during restoration */
struct window_hierarchy_info {
    glui32 window_tag;
    glui32 parent_tag; 
    glui32 child1_tag;  /* For pair windows */
    glui32 child2_tag;  /* For pair windows */
    glui32 key_tag;     /* For pair windows */
    window_t *window;   /* Pointer to the created window */
};

static struct window_hierarchy_info hierarchy_info[MAX_HIERARCHY_ENTRIES];
static glui32 hierarchy_count = 0;

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
    /* Serialize GLK library state in order:
     * 1. Version number
     * 2. Global state (root window, focus window, current stream)
     * 3. Window list and hierarchy
     * 4. Stream list
     * 5. Fileref list
     */
    
    if (!glkunix_serialize_uint32(context, "glk_state_version", 1)) {
        return 0;
    }
    
    /* Global state pointers (as update tags) */
    glui32 root_tag = gli_rootwin ? gli_get_update_tag((void*)gli_rootwin, OBJTYPE_WINDOW) : 0;
    glui32 focus_tag = gli_focuswin ? gli_get_update_tag((void*)gli_focuswin, OBJTYPE_WINDOW) : 0;
    stream_t *current_stream = glk_stream_get_current();
    glui32 current_stream_tag = current_stream ? gli_get_update_tag((void*)current_stream, OBJTYPE_STREAM) : 0;
    
    if (!glkunix_serialize_uint32(context, "root_window_tag", root_tag) ||
        !glkunix_serialize_uint32(context, "focus_window_tag", focus_tag) ||
        !glkunix_serialize_uint32(context, "current_stream_tag", current_stream_tag)) {
        return 0;
    }
    
    /* Serialize object lists */
    if (!serialize_window_list(context) ||
        !serialize_stream_list(context) ||
        !serialize_fileref_list(context)) {
        return 0;
    }
    
    return 1;
}

int glkunix_unserialize_library_state(glkunix_unserialize_context_t context)
{
    /* Read and verify the GLK state version */
    glui32 version;
    if (!glkunix_unserialize_uint32(context, "glk_state_version", &version)) {
        return 0;
    }
    
    /* For now, we only support version 1 */
    if (version != 1) {
        return 0; /* Unsupported version */
    }
    
    /* Restore object lists */
    if (!unserialize_window_list(context) ||
        !unserialize_stream_list(context) ||
        !unserialize_fileref_list(context)) {
        return 0;
    }
    
    return 1;
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

/* Internal dispatcher function for getting update tags by object type */
static glui32 gli_get_update_tag(void *obj, int objtype)
{
    if (!obj) return 0;
    
    switch (objtype) {
        case OBJTYPE_WINDOW:
            return glkunix_window_get_updatetag((winid_t)obj);
        case OBJTYPE_STREAM:
            return glkunix_stream_get_updatetag((strid_t)obj);
        case OBJTYPE_FILEREF:
            return glkunix_fileref_get_updatetag((frefid_t)obj);
        default:
            return 0;
    }
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
    if (!ctx.file) {
        return 0;
    }
    
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
    if (!ctx.file || !value) {
        return 0;
    }
    
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

/* Window list serialization implementation */
static int serialize_window_list(glkunix_serialize_context_t context)
{
    /* Count windows first */
    glui32 count = 0;
    window_t *windowlist = glkunix_get_windowlist();
    window_t *win;
    
    for (win = windowlist; win; win = win->next) {
        count++;
    }
    
    if (!glkunix_serialize_uint32(context, "window_count", count)) {
        return 0;
    }
    
    /* Serialize each window */
    for (win = windowlist; win; win = win->next) {
        if (!serialize_window(context, win)) {
            return 0;
        }
    }
    
    return 1;
}

static int serialize_window(glkunix_serialize_context_t context, window_t *win)
{
    if (!win) {
        return 0;
    }
    
    /* Basic window properties */
    glui32 update_tag = gli_get_update_tag((void*)win, OBJTYPE_WINDOW);
    if (!glkunix_serialize_uint32(context, "window_tag", update_tag) ||
        !glkunix_serialize_uint32(context, "window_type", win->type) ||
        !glkunix_serialize_uint32(context, "window_rock", win->rock)) {
        return 0;
    }
    
    /* Window relationships */
    glui32 parent_tag = win->parent ? gli_get_update_tag((void*)win->parent, OBJTYPE_WINDOW) : 0;
    glui32 stream_tag = win->str ? gli_get_update_tag((void*)win->str, OBJTYPE_STREAM) : 0;
    glui32 echo_stream_tag = win->echostr ? gli_get_update_tag((void*)win->echostr, OBJTYPE_STREAM) : 0;
    
    if (!glkunix_serialize_uint32(context, "parent_tag", parent_tag) ||
        !glkunix_serialize_uint32(context, "stream_tag", stream_tag) ||
        !glkunix_serialize_uint32(context, "echo_stream_tag", echo_stream_tag)) {
        return 0;
    }
    
    /* Bounding box */
    if (!glkunix_serialize_uint32(context, "bbox_left", win->bbox.left) ||
        !glkunix_serialize_uint32(context, "bbox_top", win->bbox.top) ||
        !glkunix_serialize_uint32(context, "bbox_right", win->bbox.right) ||
        !glkunix_serialize_uint32(context, "bbox_bottom", win->bbox.bottom)) {
        return 0;
    }
    
    /* Input request states */
    if (!glkunix_serialize_uint32(context, "line_request", win->line_request) ||
        !glkunix_serialize_uint32(context, "line_request_uni", win->line_request_uni) ||
        !glkunix_serialize_uint32(context, "char_request", win->char_request) ||
        !glkunix_serialize_uint32(context, "char_request_uni", win->char_request_uni)) {
        return 0;
    }
    
    /* Line input settings */
    if (!glkunix_serialize_uint32(context, "echo_line_input", win->echo_line_input) ||
        !glkunix_serialize_uint32(context, "terminate_line_input", win->terminate_line_input)) {
        return 0;
    }
    
    /* Current style */
    if (!glkunix_serialize_uint32(context, "style", win->styleplus.style) ||
        !glkunix_serialize_uint32(context, "inline_fgcolor", (glui32)win->styleplus.inline_fgcolor) ||
        !glkunix_serialize_uint32(context, "inline_bgcolor", (glui32)win->styleplus.inline_bgcolor) ||
        !glkunix_serialize_uint32(context, "inline_reverse", (glui32)win->styleplus.inline_reverse)) {
        return 0;
    }
    
    /* Serialize window-specific data based on type */
    if (!serialize_window_data(context, win)) {
        return 0;
    }
    
    return 1;
}

/* Serialize window-specific data based on window type */
static int serialize_window_data(glkunix_serialize_context_t context, window_t *win)
{
    if (!win || !win->data) {
        /* No window data to serialize */
        if (!glkunix_serialize_uint32(context, "has_data", 0)) {
            return 0;
        }
        return 1;
    }
    
    if (!glkunix_serialize_uint32(context, "has_data", 1)) {
        return 0;
    }
    
    switch (win->type) {
    case wintype_TextBuffer:
        return serialize_textbuffer_data(context, (window_textbuffer_t *)win->data);
        
    case wintype_TextGrid:
        return serialize_textgrid_data(context, (window_textgrid_t *)win->data);
        
    case wintype_Pair:
        return serialize_pair_data(context, (window_pair_t *)win->data);
        
    case wintype_Blank:
        return serialize_blank_data(context, (window_blank_t *)win->data);
        
    case wintype_Graphics:
        /* Graphics windows not fully supported in glkterm */
        return 1;
        
    default:
        /* Unknown window type */
        return 0;
    }
}

/* Serialize text buffer window data */
static int serialize_textbuffer_data(glkunix_serialize_context_t context, window_textbuffer_t *dwin)
{
    if (!dwin) {
        return 0;
    }
    
    /* Serialize buffer dimensions and properties */
    if (!glkunix_serialize_uint32(context, "tb_width", dwin->width) ||
        !glkunix_serialize_uint32(context, "tb_height", dwin->height) ||
        !glkunix_serialize_uint32(context, "tb_numchars", dwin->numchars) ||
        !glkunix_serialize_uint32(context, "tb_dirtybeg", dwin->dirtybeg) ||
        !glkunix_serialize_uint32(context, "tb_dirtyend", dwin->dirtyend) ||
        !glkunix_serialize_uint32(context, "tb_dirtydelta", dwin->dirtydelta)) {
        return 0;
    }
    
    /* Serialize buffer content */
    if (dwin->chars && dwin->numchars > 0) {
        if (!glkunix_serialize_buffer(context, "tb_chars", (unsigned char*)dwin->chars, dwin->numchars)) {
            return 0;
        }
    } else {
        if (!glkunix_serialize_buffer(context, "tb_chars", NULL, 0)) {
            return 0;
        }
    }
    
    return 1;
}

/* Serialize text grid window data */
static int serialize_textgrid_data(glkunix_serialize_context_t context, window_textgrid_t *dwin)
{
    if (!dwin) {
        return 0;
    }
    
    /* Serialize grid dimensions and cursor position */
    if (!glkunix_serialize_uint32(context, "tg_width", dwin->width) ||
        !glkunix_serialize_uint32(context, "tg_height", dwin->height) ||
        !glkunix_serialize_uint32(context, "tg_curx", dwin->curx) ||
        !glkunix_serialize_uint32(context, "tg_cury", dwin->cury) ||
        !glkunix_serialize_uint32(context, "tg_dirtybeg", dwin->dirtybeg) ||
        !glkunix_serialize_uint32(context, "tg_dirtyend", dwin->dirtyend)) {
        return 0;
    }
    
    /* Serialize input state */
    if (!glkunix_serialize_uint32(context, "tg_inunicode", dwin->inunicode) ||
        !glkunix_serialize_uint32(context, "tg_inorgx", dwin->inorgx) ||
        !glkunix_serialize_uint32(context, "tg_inorgy", dwin->inorgy)) {
        return 0;
    }
    
    /* Serialize grid content (lines array) */
    if (dwin->lines && dwin->height > 0) {
        /* Serialize each line */
        for (int line = 0; line < dwin->height; line++) {
            tgline_t *tgline = &dwin->lines[line];
            
            /* Serialize line properties */
            if (!glkunix_serialize_uint32(context, "tg_line_size", tgline->size) ||
                !glkunix_serialize_uint32(context, "tg_line_dirtybeg", tgline->dirtybeg) ||
                !glkunix_serialize_uint32(context, "tg_line_dirtyend", tgline->dirtyend)) {
                return 0;
            }
            
            /* Serialize character data */
            if (tgline->chars && dwin->width > 0) {
                if (!glkunix_serialize_buffer(context, "tg_line_chars", tgline->chars, dwin->width)) {
                    return 0;
                }
            } else {
                if (!glkunix_serialize_buffer(context, "tg_line_chars", NULL, 0)) {
                    return 0;
                }
            }
            
            /* Serialize style data */
            if (tgline->styleplusses && dwin->width > 0) {
                if (!glkunix_serialize_buffer(context, "tg_line_styles", 
                        (unsigned char*)tgline->styleplusses, dwin->width * sizeof(styleplus_t))) {
                    return 0;
                }
            } else {
                if (!glkunix_serialize_buffer(context, "tg_line_styles", NULL, 0)) {
                    return 0;
                }
            }
        }
    }
    
    return 1;
}

/* Serialize pair window data */
static int serialize_pair_data(glkunix_serialize_context_t context, window_pair_t *dwin)
{
    if (!dwin) {
        return 0;
    }
    
    /* Serialize pair window properties */
    glui32 child1_tag = dwin->child1 ? gli_get_update_tag((void*)dwin->child1, OBJTYPE_WINDOW) : 0;
    glui32 child2_tag = dwin->child2 ? gli_get_update_tag((void*)dwin->child2, OBJTYPE_WINDOW) : 0;
    
    if (!glkunix_serialize_uint32(context, "pair_child1_tag", child1_tag) ||
        !glkunix_serialize_uint32(context, "pair_child2_tag", child2_tag) ||
        !glkunix_serialize_uint32(context, "pair_splitpos", dwin->splitpos) ||
        !glkunix_serialize_uint32(context, "pair_splitwidth", dwin->splitwidth)) {
        return 0;
    }
    
    return 1;
}

/* Serialize blank window data */
static int serialize_blank_data(glkunix_serialize_context_t context, window_blank_t *dwin)
{
    /* Blank windows have no data to serialize */
    return 1;
}

static int serialize_stream_list(glkunix_serialize_context_t context)
{
    /* Count streams first */
    glui32 count = 0;
    stream_t *streamlist = glkunix_get_streamlist();
    stream_t *str;
    
    for (str = streamlist; str; str = str->next) {
        count++;
    }
    
    if (!glkunix_serialize_uint32(context, "stream_count", count)) {
        return 0;
    }
    
    /* Serialize each stream */
    for (str = streamlist; str; str = str->next) {
        if (!serialize_stream(context, str)) {
            return 0;
        }
    }
    
    return 1;
}

static int serialize_stream(glkunix_serialize_context_t context, stream_t *str)
{
    if (!str) {
        return 0;
    }
    
    /* Basic stream properties */
    glui32 update_tag = gli_get_update_tag((void*)str, OBJTYPE_STREAM);
    if (!glkunix_serialize_uint32(context, "stream_tag", update_tag) ||
        !glkunix_serialize_uint32(context, "stream_type", str->type) ||
        !glkunix_serialize_uint32(context, "stream_rock", str->rock)) {
        return 0;
    }
    
    /* Stream state */
    if (!glkunix_serialize_uint32(context, "unicode", str->unicode) ||
        !glkunix_serialize_uint32(context, "readable", str->readable) ||
        !glkunix_serialize_uint32(context, "writable", str->writable) ||
        !glkunix_serialize_uint32(context, "readcount", str->readcount) ||
        !glkunix_serialize_uint32(context, "writecount", str->writecount)) {
        return 0;
    }
    
    /* Type-specific data */
    switch (str->type) {
        case strtype_Window:
            /* Window stream - store associated window tag */
            {
                glui32 win_tag = str->win ? gli_get_update_tag((void*)str->win, OBJTYPE_WINDOW) : 0;
                if (!glkunix_serialize_uint32(context, "window_tag", win_tag)) {
                    return 0;
                }
            }
            break;
            
        case strtype_File:
            /* File stream - store filename and position */
            {
                const char *filename = str->filename ? str->filename : "";
                if (!glkunix_serialize_buffer(context, "filename", (void*)filename, strlen(filename)) ||
                    !glkunix_serialize_uint32(context, "lastop", str->lastop)) {
                    return 0;
                }
                /* Note: File position will be automatically restored when file is reopened */
            }
            break;
            
        case strtype_Memory:
            /* Memory stream - we'll need to store buffer contents for read-only streams */
            if (!glkunix_serialize_uint32(context, "buflen", str->buflen) ||
                !glkunix_serialize_uint32(context, "isbinary", str->isbinary)) {
                return 0;
            }
            
            /* Store current buffer position offsets */
            if (str->unicode) {
                glui32 ptr_offset = str->ubufptr ? (str->ubufptr - str->ubuf) : 0;
                glui32 end_offset = str->ubufend ? (str->ubufend - str->ubuf) : 0;
                glui32 eof_offset = str->ubufeof ? (str->ubufeof - str->ubuf) : 0;
                if (!glkunix_serialize_uint32(context, "ubuf_ptr_offset", ptr_offset) ||
                    !glkunix_serialize_uint32(context, "ubuf_end_offset", end_offset) ||
                    !glkunix_serialize_uint32(context, "ubuf_eof_offset", eof_offset)) {
                    return 0;
                }
                
                /* Serialize unicode buffer content */
                if (str->buflen > 0 && str->ubuf) {
                    if (!glkunix_serialize_buffer(context, "ubuf_data", str->ubuf, str->buflen * sizeof(glui32))) {
                        return 0;
                    }
                }
            } else {
                glui32 ptr_offset = str->bufptr ? (str->bufptr - str->buf) : 0;
                glui32 end_offset = str->bufend ? (str->bufend - str->buf) : 0;
                glui32 eof_offset = str->bufeof ? (str->bufeof - str->buf) : 0;
                if (!glkunix_serialize_uint32(context, "buf_ptr_offset", ptr_offset) ||
                    !glkunix_serialize_uint32(context, "buf_end_offset", end_offset) ||
                    !glkunix_serialize_uint32(context, "buf_eof_offset", eof_offset)) {
                    return 0;
                }
                
                /* Serialize binary buffer content */
                if (str->buflen > 0 && str->buf) {
                    if (!glkunix_serialize_buffer(context, "buf_data", str->buf, str->buflen)) {
                        return 0;
                    }
                }
            }
            break;
            
        case strtype_Resource:
            /* Resource stream */
            if (!glkunix_serialize_uint32(context, "isbinary", str->isbinary)) {
                return 0;
            }
            /* Note: Resource streams are read-only and managed by gi_blorb.c */
            break;
    }
    
    return 1;
}

static int serialize_fileref_list(glkunix_serialize_context_t context)
{
    /* Count filerefs first */
    glui32 count = 0;
    fileref_t *filereflist = glkunix_get_filereflist();
    fileref_t *fref;
    
    for (fref = filereflist; fref; fref = fref->next) {
        count++;
    }
    
    if (!glkunix_serialize_uint32(context, "fileref_count", count)) {
        return 0;
    }
    
    /* Serialize each fileref */
    for (fref = filereflist; fref; fref = fref->next) {
        if (!serialize_fileref(context, fref)) {
            return 0;
        }
    }
    
    return 1;
}

static int serialize_fileref(glkunix_serialize_context_t context, fileref_t *fref)
{
    if (!fref) {
        return 0;
    }
    
    /* Basic fileref properties */
    glui32 update_tag = gli_get_update_tag((void*)fref, OBJTYPE_FILEREF);
    if (!glkunix_serialize_uint32(context, "fileref_tag", update_tag) ||
        !glkunix_serialize_uint32(context, "fileref_rock", fref->rock)) {
        return 0;
    }
    
    /* File properties */
    const char *filename = fref->filename ? fref->filename : "";
    if (!glkunix_serialize_buffer(context, "filename", (void*)filename, strlen(filename)) ||
        !glkunix_serialize_uint32(context, "filetype", fref->filetype) ||
        !glkunix_serialize_uint32(context, "textmode", fref->textmode)) {
        return 0;
    }
    
    return 1;
}

/* Unserialization functions for restoring GLK object lists */

static int unserialize_window_list(glkunix_unserialize_context_t context)
{
    /* Read window count */
    glui32 count;
    if (!glkunix_unserialize_uint32(context, "window_count", &count)) {
        return 0;
    }
    
    /* Process each window - first pass creates objects */
    for (glui32 i = 0; i < count; i++) {
        if (!unserialize_window(context)) {
            return 0;
        }
    }
    
    /* Second pass: restore window hierarchy relationships */
    if (!restore_window_hierarchy(context)) {
        return 0;
    }
    
    return 1;
}

static int unserialize_stream_list(glkunix_unserialize_context_t context)
{
    /* Read stream count */
    glui32 count;
    if (!glkunix_unserialize_uint32(context, "stream_count", &count)) {
        return 0;
    }
    
    /* Process each stream */
    for (glui32 i = 0; i < count; i++) {
        if (!unserialize_stream(context)) {
            return 0;
        }
    }
    
    return 1;
}

static int unserialize_fileref_list(glkunix_unserialize_context_t context)
{
    /* Read fileref count */
    glui32 count;
    if (!glkunix_unserialize_uint32(context, "fileref_count", &count)) {
        return 0;
    }
    
    /* Process each fileref */
    for (glui32 i = 0; i < count; i++) {
        if (!unserialize_fileref(context)) {
            return 0;
        }
    }
    
    return 1;
}

static int unserialize_window(glkunix_unserialize_context_t context)
{
    /* Read basic window properties */
    glui32 update_tag, window_type, window_rock;
    glui32 width, height;
    
    if (!glkunix_unserialize_uint32(context, "window_tag", &update_tag) ||
        !glkunix_unserialize_uint32(context, "window_type", &window_type) ||
        !glkunix_unserialize_uint32(context, "window_rock", &window_rock) ||
        !glkunix_unserialize_uint32(context, "width", &width) ||
        !glkunix_unserialize_uint32(context, "height", &height)) {
        return 0;
    }
    
    /* Create or find the window with this rock */
    window_t *win = gli_new_window(window_type, window_rock);
    if (!win) {
        return 0; /* Failed to create window */
    }
    
    /* Set basic properties */
    win->bbox.left = 0; /* Will be set by layout later */
    win->bbox.top = 0;
    win->bbox.right = width;
    win->bbox.bottom = height;
    
    /* Read parent information */
    glui32 parent_tag;
    if (!glkunix_unserialize_uint32(context, "parent_tag", &parent_tag)) {
        return 0;
    }
    
    /* Store hierarchy information for all windows */
    if (hierarchy_count < MAX_HIERARCHY_ENTRIES) {
        struct window_hierarchy_info *info = &hierarchy_info[hierarchy_count];
        info->window_tag = glkunix_window_get_updatetag(win);
        info->parent_tag = parent_tag;
        info->child1_tag = 0;  /* Will be set for pair windows */
        info->child2_tag = 0;  /* Will be set for pair windows */
        info->key_tag = 0;     /* Will be set for pair windows */
        info->window = win;
        hierarchy_count++;
    }
    
    /* Read other window fields we need to skip for now */
    glui32 stream_tag, echo_stream_tag;
    if (!glkunix_unserialize_uint32(context, "stream_tag", &stream_tag) ||
        !glkunix_unserialize_uint32(context, "echo_stream_tag", &echo_stream_tag)) {
        return 0;
    }
    
    /* Read bounding box */
    glui32 bbox_left, bbox_top, bbox_right, bbox_bottom;
    if (!glkunix_unserialize_uint32(context, "bbox_left", &bbox_left) ||
        !glkunix_unserialize_uint32(context, "bbox_top", &bbox_top) ||
        !glkunix_unserialize_uint32(context, "bbox_right", &bbox_right) ||
        !glkunix_unserialize_uint32(context, "bbox_bottom", &bbox_bottom)) {
        return 0;
    }
    
    /* Read input request states */
    glui32 line_request, line_request_uni, char_request, char_request_uni;
    if (!glkunix_unserialize_uint32(context, "line_request", &line_request) ||
        !glkunix_unserialize_uint32(context, "line_request_uni", &line_request_uni) ||
        !glkunix_unserialize_uint32(context, "char_request", &char_request) ||
        !glkunix_unserialize_uint32(context, "char_request_uni", &char_request_uni)) {
        return 0;
    }
    
    /* Read line input settings */
    glui32 echo_line_input, terminate_line_input;
    if (!glkunix_unserialize_uint32(context, "echo_line_input", &echo_line_input) ||
        !glkunix_unserialize_uint32(context, "terminate_line_input", &terminate_line_input)) {
        return 0;
    }
    
    /* Read style information */
    glui32 style, inline_fgcolor, inline_bgcolor, inline_reverse;
    if (!glkunix_unserialize_uint32(context, "style", &style) ||
        !glkunix_unserialize_uint32(context, "inline_fgcolor", &inline_fgcolor) ||
        !glkunix_unserialize_uint32(context, "inline_bgcolor", &inline_bgcolor) ||
        !glkunix_unserialize_uint32(context, "inline_reverse", &inline_reverse)) {
        return 0;
    }
    
    /* Read has_data flag */
    glui32 has_data;
    if (!glkunix_unserialize_uint32(context, "has_data", &has_data)) {
        return 0;
    }
    
    /* Read type-specific data */
    if (has_data && !unserialize_window_data(context, win)) {
        return 0;
    }
    
    return 1;
}

static int unserialize_stream(glkunix_unserialize_context_t context)
{
    /* Read basic stream properties */
    glui32 update_tag, stream_type, stream_rock;
    glui32 unicode, readable, writable, readcount, writecount;
    
    if (!glkunix_unserialize_uint32(context, "stream_tag", &update_tag) ||
        !glkunix_unserialize_uint32(context, "stream_type", &stream_type) ||
        !glkunix_unserialize_uint32(context, "stream_rock", &stream_rock) ||
        !glkunix_unserialize_uint32(context, "unicode", &unicode) ||
        !glkunix_unserialize_uint32(context, "readable", &readable) ||
        !glkunix_unserialize_uint32(context, "writable", &writable) ||
        !glkunix_unserialize_uint32(context, "readcount", &readcount) ||
        !glkunix_unserialize_uint32(context, "writecount", &writecount)) {
        return 0;
    }
    
    /* Create stream based on type */
    stream_t *str = NULL;
    
    switch (stream_type) {
        case strtype_Window:
            {
                glui32 window_tag;
                if (!glkunix_unserialize_uint32(context, "window_tag", &window_tag)) {
                    return 0;
                }
                /* TODO: Find window by tag and create window stream */
                /* For now, create a basic window stream */
                str = gli_new_stream(strtype_Window, readable, writable, stream_rock);
            }
            break;
            
        case strtype_File:
            {
                /* Read filename length and data */
                glui32 filename_len;
                if (!glkunix_unserialize_uint32(context, NULL, &filename_len)) {
                    return 0;
                }
                
                char *filename = NULL;
                if (filename_len > 0) {
                    filename = malloc(filename_len + 1);
                    if (!filename) {
                        return 0;
                    }
                    if (!glkunix_unserialize_buffer(context, "filename", filename, filename_len)) {
                        free(filename);
                        return 0;
                    }
                    filename[filename_len] = '\0';
                }
                
                glui32 lastop;
                if (!glkunix_unserialize_uint32(context, "lastop", &lastop)) {
                    if (filename) free(filename);
                    return 0;
                }
                
                /* Create file stream */
                str = gli_stream_open_pathname(filename, writable, unicode, stream_rock);
                if (str) {
                    str->lastop = lastop;
                }
                
                if (filename) free(filename);
            }
            break;
            
        case strtype_Memory:
            {
                glui32 buflen, isbinary;
                if (!glkunix_unserialize_uint32(context, "buflen", &buflen) ||
                    !glkunix_unserialize_uint32(context, "isbinary", &isbinary)) {
                    return 0;
                }
                
                /* Read buffer position offsets */
                glui32 ptr_offset, end_offset, eof_offset;
                if (unicode) {
                    if (!glkunix_unserialize_uint32(context, "ubuf_ptr_offset", &ptr_offset) ||
                        !glkunix_unserialize_uint32(context, "ubuf_end_offset", &end_offset) ||
                        !glkunix_unserialize_uint32(context, "ubuf_eof_offset", &eof_offset)) {
                        return 0;
                    }
                } else {
                    if (!glkunix_unserialize_uint32(context, "buf_ptr_offset", &ptr_offset) ||
                        !glkunix_unserialize_uint32(context, "buf_end_offset", &end_offset) ||
                        !glkunix_unserialize_uint32(context, "buf_eof_offset", &eof_offset)) {
                        return 0;
                    }
                }
                
                /* TODO: Create memory stream with proper buffer setup */
                /* This is complex because we need to allocate buffers and restore content */
                str = gli_new_stream(strtype_Memory, readable, writable, stream_rock);
                if (str) {
                    str->buflen = buflen;
                    str->isbinary = isbinary;
                    
                    /* Allocate and restore buffer contents */
                    if (buflen > 0) {
                        if (str->isbinary) {
                            /* Binary memory stream - allocate char buffer */
                            str->buf = malloc(buflen);
                            if (str->buf) {
                                /* Restore buffer content from serialized data */
                                if (!glkunix_unserialize_buffer(context, "buf_data", str->buf, buflen)) {
                                    free(str->buf);
                                    str->buf = NULL;
                                    str->buflen = 0;
                                } else {
                                    /* Restore buffer pointers based on saved offsets */
                                    glui32 ptr_offset, end_offset, eof_offset;
                                    if (glkunix_unserialize_uint32(context, "buf_ptr_offset", &ptr_offset) &&
                                        glkunix_unserialize_uint32(context, "buf_end_offset", &end_offset) &&
                                        glkunix_unserialize_uint32(context, "buf_eof_offset", &eof_offset)) {
                                        str->bufptr = str->buf + ((ptr_offset < buflen) ? ptr_offset : buflen);
                                        str->bufend = str->buf + ((end_offset < buflen) ? end_offset : buflen);
                                        str->bufeof = str->buf + ((eof_offset < buflen) ? eof_offset : buflen);
                                    } else {
                                        /* Fallback to safe defaults */
                                        str->bufptr = str->buf;
                                        str->bufend = str->buf + buflen;
                                        str->bufeof = str->buf;
                                    }
                                }
                            } else {
                                str->buflen = 0;
                            }
                        } else {
                            /* Unicode memory stream - allocate glui32 buffer */
                            str->ubuf = malloc(buflen * sizeof(glui32));
                            if (str->ubuf) {
                                /* Restore unicode buffer content */
                                if (!glkunix_unserialize_buffer(context, "ubuf_data", str->ubuf, buflen * sizeof(glui32))) {
                                    free(str->ubuf);
                                    str->ubuf = NULL;
                                    str->buflen = 0;
                                } else {
                                    /* Restore buffer pointers based on saved offsets */
                                    glui32 ptr_offset, end_offset, eof_offset;
                                    if (glkunix_unserialize_uint32(context, "ubuf_ptr_offset", &ptr_offset) &&
                                        glkunix_unserialize_uint32(context, "ubuf_end_offset", &end_offset) &&
                                        glkunix_unserialize_uint32(context, "ubuf_eof_offset", &eof_offset)) {
                                        str->ubufptr = str->ubuf + ((ptr_offset < buflen) ? ptr_offset : buflen);
                                        str->ubufend = str->ubuf + ((end_offset < buflen) ? end_offset : buflen);
                                        str->ubufeof = str->ubuf + ((eof_offset < buflen) ? eof_offset : buflen);
                                    } else {
                                        /* Fallback to safe defaults */
                                        str->ubufptr = str->ubuf;
                                        str->ubufend = str->ubuf + buflen;
                                        str->ubufeof = str->ubuf;
                                    }
                                }
                            } else {
                                str->buflen = 0;
                            }
                        }
                    }
                }
            }
            break;
            
        case strtype_Resource:
            {
                glui32 isbinary;
                if (!glkunix_unserialize_uint32(context, "isbinary", &isbinary)) {
                    return 0;
                }
                
                /* Create resource stream */
                str = gli_new_stream(strtype_Resource, readable, writable, stream_rock);
                if (str) {
                    str->isbinary = isbinary;
                }
            }
            break;
    }
    
    if (!str) {
        return 0; /* Failed to create stream */
    }
    
    /* Restore basic properties */
    str->unicode = unicode;
    str->readcount = readcount;
    str->writecount = writecount;
    
    return 1;
}

static int unserialize_fileref(glkunix_unserialize_context_t context)
{
    /* Read basic fileref properties */
    glui32 update_tag, fileref_rock;
    
    if (!glkunix_unserialize_uint32(context, "fileref_tag", &update_tag) ||
        !glkunix_unserialize_uint32(context, "fileref_rock", &fileref_rock)) {
        return 0;
    }
    
    /* Read filename length and data */
    glui32 filename_len;
    if (!glkunix_unserialize_uint32(context, NULL, &filename_len)) {
        return 0;
    }
    
    char *filename = NULL;
    if (filename_len > 0) {
        filename = malloc(filename_len + 1);
        if (!filename) {
            return 0;
        }
        if (!glkunix_unserialize_buffer(context, "filename", filename, filename_len)) {
            free(filename);
            return 0;
        }
        filename[filename_len] = '\0';
    }
    
    /* Read file properties */
    glui32 filetype, textmode;
    if (!glkunix_unserialize_uint32(context, "filetype", &filetype) ||
        !glkunix_unserialize_uint32(context, "textmode", &textmode)) {
        if (filename) free(filename);
        return 0;
    }
    
    /* Create fileref */
    fileref_t *fref = gli_new_fileref(filename, filetype, fileref_rock);
    if (fref) {
        fref->textmode = textmode;
    }
    
    if (filename) free(filename);
    
    return fref ? 1 : 0;
}

static int unserialize_window_data(glkunix_unserialize_context_t context, window_t *win)
{
    /* Unserialize type-specific window data */
    switch (win->type) {
        case wintype_TextBuffer:
            {
                /* Restore text buffer contents */
                window_textbuffer_t *dwin = win->data;
                if (dwin) {
                    glui32 numchars, charssize, width, height;
                    glui32 dirtybeg, dirtyend, dirtydelta;
                    
                    /* Restore text buffer properties */
                    if (!glkunix_unserialize_uint32(context, "tb_numchars", &numchars) ||
                        !glkunix_unserialize_uint32(context, "tb_charssize", &charssize) ||
                        !glkunix_unserialize_uint32(context, "tb_width", &width) ||
                        !glkunix_unserialize_uint32(context, "tb_height", &height) ||
                        !glkunix_unserialize_uint32(context, "tb_dirtybeg", &dirtybeg) ||
                        !glkunix_unserialize_uint32(context, "tb_dirtyend", &dirtyend) ||
                        !glkunix_unserialize_uint32(context, "tb_dirtydelta", &dirtydelta)) {
                        return 0;
                    }
                    
                    /* Restore text buffer content */
                    if (numchars > 0) {
                        /* Allocate buffer for character data */
                        char *chars = malloc(charssize > numchars ? charssize : numchars);
                        if (chars) {
                            if (glkunix_unserialize_buffer(context, "tb_chars", chars, numchars)) {
                                /* Free existing buffer and set new one */
                                if (dwin->chars) {
                                    free(dwin->chars);
                                }
                                dwin->chars = chars;
                                dwin->numchars = numchars;
                                dwin->charssize = charssize > numchars ? charssize : numchars;
                                dwin->width = width;
                                dwin->height = height;
                                dwin->dirtybeg = dirtybeg;
                                dwin->dirtyend = dirtyend;
                                dwin->dirtydelta = dirtydelta;
                                dwin->drawall = 1; /* Force redraw */
                            } else {
                                free(chars);
                                return 0;
                            }
                        } else {
                            return 0;
                        }
                    } else {
                        /* Empty buffer case */
                        glkunix_unserialize_buffer(context, "tb_chars", NULL, 0); /* Consume the empty buffer data */
                        if (dwin->chars) {
                            free(dwin->chars);
                            dwin->chars = NULL;
                        }
                        dwin->numchars = 0;
                        dwin->charssize = 0;
                    }
                }
            }
            break;
            
        case wintype_TextGrid:
            {
                /* Restore grid contents and cursor position */
                window_textgrid_t *dwin = win->data;
                if (dwin) {
                    glui32 width, height, curx, cury, dirtybeg, dirtyend;
                    glui32 inunicode, inorgx, inorgy;
                    
                    /* Restore text grid properties */
                    if (!glkunix_unserialize_uint32(context, "tg_width", &width) ||
                        !glkunix_unserialize_uint32(context, "tg_height", &height) ||
                        !glkunix_unserialize_uint32(context, "tg_curx", &curx) ||
                        !glkunix_unserialize_uint32(context, "tg_cury", &cury) ||
                        !glkunix_unserialize_uint32(context, "tg_dirtybeg", &dirtybeg) ||
                        !glkunix_unserialize_uint32(context, "tg_dirtyend", &dirtyend)) {
                        return 0;
                    }
                    
                    /* Restore input state */
                    if (!glkunix_unserialize_uint32(context, "tg_inunicode", &inunicode) ||
                        !glkunix_unserialize_uint32(context, "tg_inorgx", &inorgx) ||
                        !glkunix_unserialize_uint32(context, "tg_inorgy", &inorgy)) {
                        return 0;
                    }
                    
                    /* Update grid properties */
                    dwin->width = width;
                    dwin->height = height;
                    dwin->curx = curx;
                    dwin->cury = cury;
                    dwin->dirtybeg = dirtybeg;
                    dwin->dirtyend = dirtyend;
                    dwin->inunicode = inunicode;
                    dwin->inorgx = inorgx;
                    dwin->inorgy = inorgy;
                    
                    /* Restore grid content (lines array) */
                    if (height > 0 && dwin->lines) {
                        /* Restore each line */
                        for (int line = 0; line < height; line++) {
                            if (line < dwin->linessize) {
                                tgline_t *tgline = &dwin->lines[line];
                                
                                /* Restore line properties */
                                glui32 line_size, line_dirtybeg, line_dirtyend;
                                if (!glkunix_unserialize_uint32(context, "tg_line_size", &line_size) ||
                                    !glkunix_unserialize_uint32(context, "tg_line_dirtybeg", &line_dirtybeg) ||
                                    !glkunix_unserialize_uint32(context, "tg_line_dirtyend", &line_dirtyend)) {
                                    return 0;
                                }
                                
                                tgline->size = line_size;
                                tgline->dirtybeg = line_dirtybeg;
                                tgline->dirtyend = line_dirtyend;
                                
                                /* Restore character data */
                                glui32 chars_len;
                                if (!glkunix_unserialize_uint32(context, NULL, &chars_len)) {
                                    return 0;
                                }
                                
                                if (chars_len > 0 && width > 0) {
                                    /* Ensure line has character buffer */
                                    if (!tgline->chars || tgline->size < width) {
                                        if (tgline->chars) free(tgline->chars);
                                        tgline->chars = malloc(width);
                                        if (!tgline->chars) {
                                            return 0;
                                        }
                                        tgline->size = width;
                                    }
                                    
                                    if (fread(tgline->chars, 1, chars_len, context.file) != chars_len) {
                                        return 0;
                                    }
                                    if (context.read_count) {
                                        *(context.read_count) += chars_len;
                                    }
                                } else {
                                    /* Empty line - consume the zero-length data */
                                    /* No actual data to read since chars_len is 0 */
                                }
                                
                                /* Restore style data */
                                glui32 styles_len;
                                if (!glkunix_unserialize_uint32(context, NULL, &styles_len)) {
                                    return 0;
                                }
                                
                                if (styles_len > 0 && width > 0) {
                                    /* Ensure line has style buffer */
                                    if (!tgline->styleplusses) {
                                        tgline->styleplusses = malloc(width * sizeof(styleplus_t));
                                        if (!tgline->styleplusses) {
                                            return 0;
                                        }
                                    }
                                    
                                    if (fread(tgline->styleplusses, 1, styles_len, context.file) != styles_len) {
                                        return 0;
                                    }
                                    if (context.read_count) {
                                        *(context.read_count) += styles_len;
                                    }
                                } else {
                                    /* Empty style data - consume the zero-length data */
                                    /* No actual data to read since styles_len is 0 */
                                }
                            } else {
                                /* Line index beyond allocated space - skip this line's data */
                                glui32 line_size, line_dirtybeg, line_dirtyend;
                                glui32 chars_len, styles_len;
                                
                                if (!glkunix_unserialize_uint32(context, "tg_line_size", &line_size) ||
                                    !glkunix_unserialize_uint32(context, "tg_line_dirtybeg", &line_dirtybeg) ||
                                    !glkunix_unserialize_uint32(context, "tg_line_dirtyend", &line_dirtyend) ||
                                    !glkunix_unserialize_uint32(context, NULL, &chars_len) ||
                                    !glkunix_unserialize_uint32(context, NULL, &styles_len)) {
                                    return 0;
                                }
                                
                                /* Skip character and style data */
                                if (chars_len > 0) {
                                    fseek(context.file, chars_len, SEEK_CUR);
                                    if (context.read_count) {
                                        *(context.read_count) += chars_len;
                                    }
                                }
                                if (styles_len > 0) {
                                    fseek(context.file, styles_len, SEEK_CUR);
                                    if (context.read_count) {
                                        *(context.read_count) += styles_len;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            break;
            
        case wintype_Graphics:
            /* TODO: Restore graphics state */
            break;
            
        case wintype_Pair:
            {
                /* Read pair window properties */
                glui32 child1_tag, child2_tag, splitpos, splitwidth;
                if (!glkunix_unserialize_uint32(context, "pair_child1_tag", &child1_tag) ||
                    !glkunix_unserialize_uint32(context, "pair_child2_tag", &child2_tag) ||
                    !glkunix_unserialize_uint32(context, "pair_splitpos", &splitpos) ||
                    !glkunix_unserialize_uint32(context, "pair_splitwidth", &splitwidth)) {
                    return 0;
                }
                
                /* Update the existing hierarchy info entry with child tags */
                if (hierarchy_count > 0) {
                    struct window_hierarchy_info *info = &hierarchy_info[hierarchy_count - 1];
                    if (info->window == win) {
                        info->child1_tag = child1_tag;
                        info->child2_tag = child2_tag;
                    }
                }
                
                /* Create and initialize the pair data structure */
                window_pair_t *dwin = (window_pair_t *)malloc(sizeof(window_pair_t));
                if (!dwin) {
                    return 0;
                }
                
                dwin->owner = win;
                dwin->child1 = NULL; /* Will be set in hierarchy restoration */
                dwin->child2 = NULL; /* Will be set in hierarchy restoration */
                dwin->splitpos = splitpos;
                dwin->splitwidth = splitwidth;
                
                /* Set default pair properties - these will be updated from serialized data */
                dwin->dir = winmethod_Left;
                dwin->vertical = 0;
                dwin->backward = 0;
                dwin->hasborder = 0;
                dwin->division = winmethod_Proportional;
                dwin->key = NULL;
                dwin->keydamage = 0;
                dwin->size = 50;
                
                win->data = dwin;
            }
            break;
    }
    
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

/* Helper functions for creating serialization contexts from FILE pointers */
glkunix_serialize_context_t glkunix_serialize_start(FILE *file)
{
    glkunix_serialize_context_t ctx;
    ctx.file = file;
    ctx.write_count = NULL; /* Count not tracked in simple version */
    return ctx;
}

void glkunix_serialize_end(glkunix_serialize_context_t ctx)
{
    /* Flush file if needed */
    if (ctx.file) {
        fflush(ctx.file);
    }
}

glkunix_unserialize_context_t glkunix_unserialize_start(FILE *file)
{
    glkunix_unserialize_context_t ctx;
    ctx.file = file;
    ctx.read_count = NULL; /* Count not tracked in simple version */
    return ctx;
}

void glkunix_unserialize_end(glkunix_unserialize_context_t ctx)
{
    /* No cleanup needed for simple version */
    (void)ctx; /* Suppress unused parameter warning */
}

/* Restore window hierarchy relationships after all windows are created */
static int restore_window_hierarchy(glkunix_unserialize_context_t context)
{
    /* Restore parent-child relationships */
    for (glui32 i = 0; i < hierarchy_count; i++) {
        struct window_hierarchy_info *info = &hierarchy_info[i];
        window_t *win = info->window;
        
        /* Set parent relationship */
        if (info->parent_tag != 0) {
            window_t *parent = glkunix_window_find_by_updatetag(info->parent_tag);
            if (parent) {
                win->parent = parent;
            }
        }
        
        /* For pair windows, set child relationships */
        if (win->type == wintype_Pair && win->data) {
            window_pair_t *dwin = (window_pair_t *)win->data;
            
            if (info->child1_tag != 0) {
                window_t *child1 = glkunix_window_find_by_updatetag(info->child1_tag);
                if (child1) {
                    dwin->child1 = child1;
                }
            }
            
            if (info->child2_tag != 0) {
                window_t *child2 = glkunix_window_find_by_updatetag(info->child2_tag);
                if (child2) {
                    dwin->child2 = child2;
                }
            }
        }
    }
    
    /* Clean up hierarchy info */
    hierarchy_count = 0;
    
    return 1;
}
