/* glkunix_autosave.h: Header for GLK Unix autosave functionality
    Required for GLKUNIX_AUTOSAVE_FEATURES support
*/

#ifndef GLKUNIX_AUTOSAVE_H
#define GLKUNIX_AUTOSAVE_H

#include <stdio.h>
#include "glk.h"
#include "gi_dispa.h"

/* Serialization context types used by Glulxe autosave */
typedef struct glkunix_serialize_context_struct {
    FILE *file;
    glui32 *write_count;  /* Pointer to allow modification when passed by value */
} glkunix_serialize_context_t;

typedef struct glkunix_unserialize_context_struct {
    FILE *file;
    glui32 *read_count;   /* Pointer to allow modification when passed by value */
} glkunix_unserialize_context_t;

/* Library state type */
typedef void *glkunix_library_state_t;

/* Function prototypes for autosave infrastructure */
extern glkunix_library_state_t glkunix_load_library_state(strid_t file,
    int (*unserialize_extra)(glkunix_unserialize_context_t ctx, void *rock), 
    void *rock);
extern int glkunix_save_library_state(strid_t jsavefile, strid_t jresultfile,
    int (*serialize_extra)(glkunix_serialize_context_t ctx, void *rock),
    void *rock);
extern int glkunix_update_from_library_state(glkunix_library_state_t library_state);
extern void glkunix_library_state_free(glkunix_library_state_t library_state);

/* Context serialization functions */
extern int glkunix_serialize_uint32(glkunix_serialize_context_t ctx, char *id, glui32 value);
extern int glkunix_unserialize_uint32(glkunix_unserialize_context_t ctx, char *id, glui32 *value);
extern int glkunix_serialize_buffer(glkunix_serialize_context_t ctx, char *id, void *buffer, glui32 length);
extern int glkunix_unserialize_buffer(glkunix_unserialize_context_t ctx, char *id, void *buffer, glui32 length);
extern int glkunix_serialize_object_list(glkunix_serialize_context_t ctx, char *id,
    int (*serialize_obj)(glkunix_serialize_context_t ctx, void *obj),
    glui32 count, glui32 objsize, void *list);
extern int glkunix_unserialize_list(glkunix_unserialize_context_t ctx, char *id, void **array, glui32 *count);
extern int glkunix_unserialize_object_list_entries(void *array,
    int (*unserialize_obj)(glkunix_unserialize_context_t ctx, void *obj),
    glui32 count, glui32 objsize, void *list);

/* Update tag functions for GLK objects */
extern glui32 glkunix_stream_get_updatetag(strid_t str);
extern glui32 glkunix_window_get_updatetag(winid_t win);
extern glui32 glkunix_fileref_get_updatetag(frefid_t fref);

extern winid_t glkunix_window_find_by_updatetag(glui32 tag);
extern strid_t glkunix_stream_find_by_updatetag(glui32 tag);
extern frefid_t glkunix_fileref_find_by_updatetag(glui32 tag);

extern void glkunix_window_set_dispatch_rock(winid_t win, gidispatch_rock_t rock);
extern void glkunix_stream_set_dispatch_rock(strid_t str, gidispatch_rock_t rock);
extern void glkunix_fileref_set_dispatch_rock(frefid_t fref, gidispatch_rock_t rock);

/* Required functions for Glulxe autosave integration */
extern glui32 glkunix_get_last_event_type(void);
extern void gli_set_last_event_type(glui32 event_type);
extern glui32 glkunix_get_autosave_tag(void);
extern void glkunix_set_autosave_tag(glui32 tag);
extern int glkunix_serialize_library_state(glkunix_serialize_context_t context);
extern int glkunix_unserialize_library_state(glkunix_unserialize_context_t context);
extern glui32 glkunix_serialize_object_tag(glkunix_serialize_context_t context, void *object);

/* Other required functions */
extern int giblorb_unset_resource_map(void);

/* Cleanup function */
extern void glkunix_autosave_cleanup(void);

/* Helper functions for creating contexts from FILE pointers */
extern glkunix_serialize_context_t glkunix_serialize_start(FILE *file);
extern void glkunix_serialize_end(glkunix_serialize_context_t ctx);
extern glkunix_unserialize_context_t glkunix_unserialize_start(FILE *file);
extern void glkunix_unserialize_end(glkunix_unserialize_context_t ctx);

/* Convenience functions for .glkstate file handling */
extern int glkunix_save_game_state(const char *gamename);
extern glkunix_library_state_t glkunix_load_game_state(const char *gamename);

#endif /* GLKUNIX_AUTOSAVE_H */
