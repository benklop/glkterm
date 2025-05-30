/* glkterm.h: Private header file
        for GlkTerm, curses.h implementation of the Glk API.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html
*/

#ifndef GLKTERM_H
#define GLKTERM_H

#include <stdio.h>
#include "gi_dispa.h"
#include "tailq.h"

/* We define our own TRUE and FALSE and NULL, because ANSI
    is a strange world. */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif

/* This macro is called whenever the library code catches an error
    or illegal operation from the game program. */

#define gli_strict_warning(msg)   \
    (gli_msgline_warning(msg)) 

/* Some useful type declarations. */

typedef struct grect_struct {
    int left, top;
    int right, bottom;
} grect_t;

typedef struct glk_window_struct window_t;
typedef struct glk_stream_struct stream_t;
typedef struct glk_fileref_struct fileref_t;
typedef struct glk_schannel_struct schannel_t;

#define MAGIC_WINDOW_NUM (9826)
#define MAGIC_STREAM_NUM (8269)
#define MAGIC_FILEREF_NUM (6982)
#define MAGIC_SCHANNEL_NUM (2698)

/* Holds the current style plus any overrides. Members should be considered
    internal to gtstyle.c and not be accessed directly by other code. */
typedef struct styleplus_struct {
    glui32 style;
    glsi32 inline_fgcolor;
    glsi32 inline_bgcolor;
    glsi32 inline_reverse;
    int inline_fgi;
    int inline_bgi;
} styleplus_t;

struct glk_window_struct {
    glui32 magicnum;
    glui32 rock;
    glui32 type;
    
    grect_t bbox; /* content rectangle, excluding borders */
    window_t *parent; /* pair window which contains this one */
    void *data; /* one of the window_*_t structures */
    
    stream_t *str; /* the window stream. */
    stream_t *echostr; /* the window's echo stream, if any. */
    
    int line_request;
    int line_request_uni;
    int char_request;
    int char_request_uni;

    int echo_line_input; /* applies to future line inputs, not the current */
    glui32 terminate_line_input; /* ditto; this is a bitmask of flags */

    styleplus_t styleplus; /* current style plus inline settings */
    struct stylehint_struct *stylehints; /* current window hints */

    gidispatch_rock_t disprock;
    window_t *next, *prev; /* in the big linked list of windows */
};

#define strtype_File (1)
#define strtype_Window (2)
#define strtype_Memory (3)
#define strtype_Resource (4)

struct glk_stream_struct {
    glui32 magicnum;
    glui32 rock;

    int type; /* file, window, or memory stream */
    int unicode; /* one-byte or four-byte chars? Not meaningful for windows */
    
    glui32 readcount, writecount;
    int readable, writable;
    
    /* for strtype_Window */
    window_t *win;
    
    /* for strtype_File */
    FILE *file; 
    char *filename;
    glui32 lastop; /* 0, filemode_Write, or filemode_Read */
    
    /* for strtype_Resource */
    int isbinary;

    /* for strtype_Memory and strtype_Resource. Separate pointers for 
       one-byte and four-byte streams */
    unsigned char *buf;
    unsigned char *bufptr;
    unsigned char *bufend;
    unsigned char *bufeof;
    glui32 *ubuf;
    glui32 *ubufptr;
    glui32 *ubufend;
    glui32 *ubufeof;
    glui32 buflen;
    gidispatch_rock_t arrayrock;

    gidispatch_rock_t disprock;
    stream_t *next, *prev; /* in the big linked list of streams */
};

struct glk_fileref_struct {
    glui32 magicnum;
    glui32 rock;

    char *filename;
    int filetype;
    int textmode;

    gidispatch_rock_t disprock;
    fileref_t *next, *prev; /* in the big linked list of filerefs */
};

typedef struct glk_schannel_event_struct schannel_event_t;
struct glk_schannel_event_struct {
    event_t event;
    schannel_t *chan;
    TAILQ_ENTRY(glk_schannel_event_struct) entries;
};

struct glk_schannel_struct {
    glui32 magicnum;
    glui32 rock;

    /* the finished event to store, if any */
    schannel_event_t *finished_event_data;
    /* the Mix channel used or -1 */
    int mix_channel;
    /* the Mix chunk loaded or NULL */
    struct Mix_Chunk *mix_chunk;
    /* 1 if the channel is in a paused state, even when stopped, or 0 */
    int paused;
    /* the starting volume when changing over time */
    glui32 volume_begin;
    /* the current (expected) volume */
    glui32 volume_current;
    /* the eventual end volume when changing over time */
    glui32 volume_end;
    /* the volume event to store, if any */
    schannel_event_t *volume_event_data;
    /* the starting time when changing over time */
    unsigned int volume_ticks_begin;
    /* the total time when changing over time */
    unsigned int volume_ticks_duration;
    /* the timer ID when changing over time or 0 */
    int volume_sdl_timerid;

    gidispatch_rock_t disprock;
    TAILQ_ENTRY(glk_schannel_struct) entries;
};

/* Arguments to keybindings */

#define gcmd_Left (1)
#define gcmd_Right (2)
#define gcmd_Up (3)
#define gcmd_Down (4)
#define gcmd_LeftEnd (5)
#define gcmd_RightEnd (6)
#define gcmd_UpEnd (7)
#define gcmd_DownEnd (8)
#define gcmd_UpPage (9)
#define gcmd_DownPage (10)
#define gcmd_Delete (11)
#define gcmd_DeleteNext (12)
#define gcmd_KillInput (13)
#define gcmd_KillLine (14)

/* A few global variables */

extern window_t *gli_rootwin;
extern window_t *gli_focuswin;
extern grect_t content_box;
extern void (*gli_interrupt_handler)(void);

/* The following typedefs are copied from cheapglk.h. They support the
   tables declared in cgunigen.c. */

typedef glui32 gli_case_block_t[2]; /* upper, lower */
/* If both are 0xFFFFFFFF, you have to look at the special-case table */

typedef glui32 gli_case_special_t[3]; /* upper, lower, title */
/* Each of these points to a subarray of the unigen_special_array
   (in cgunicode.c). In that subarray, element zero is the length,
   and that's followed by length unicode values. */

typedef glui32 gli_decomp_block_t[2]; /* count, position */
/* The position points to a subarray of the unigen_decomp_array.
   If the count is zero, there is no decomposition. */


#ifdef OPT_USE_SIGNALS
    extern int just_resumed;
    extern int just_killed;
#ifdef OPT_WINCHANGED_SIGNAL
        extern int screen_size_changed;
#endif /* OPT_WINCHANGED_SIGNAL */
#endif /* OPT_USE_SIGNALS */

extern char gli_workingdir[256];
extern unsigned char char_printable_table[256];
extern unsigned char char_typable_table[256];
#ifndef OPT_NATIVE_LATIN_1
extern unsigned char char_from_native_table[256];
extern unsigned char char_to_native_table[256];
#endif /* OPT_NATIVE_LATIN_1 */

extern gidispatch_rock_t (*gli_register_obj)(void *obj, glui32 objclass);
extern void (*gli_unregister_obj)(void *obj, glui32 objclass, gidispatch_rock_t objrock);
extern gidispatch_rock_t (*gli_register_arr)(void *array, glui32 len, char *typecode);
extern void (*gli_unregister_arr)(void *array, glui32 len, char *typecode, 
    gidispatch_rock_t objrock);

extern int pref_printversion;
extern int pref_screenwidth;
extern int pref_screenheight;
extern int pref_messageline;
extern int pref_reverse_textgrids;
extern int pref_override_window_borders;
extern int pref_window_borders;
extern int pref_precise_timing;
extern int pref_historylen;
extern int pref_prompt_defaults;
extern int pref_sound;
extern int pref_color;
extern int pref_fgcolor;
extern int pref_bgcolor;
extern int pref_stylehint;
extern int pref_emph_underline;

/* Declarations of library internal functions. */

extern void gli_initialize_misc(void);
extern char *gli_ascii_equivalent(unsigned char ch);

extern void gli_msgline_warning(char *msg);
extern void gli_msgline(char *msg);
extern void gli_msgline_redraw(void);

extern int gli_msgin_getline(char *prompt, char *buf, int maxlen, int *length);
extern int gli_msgin_getchar(char *prompt, int hilite);

extern void gli_initialize_events(void);
extern void gli_event_store(glui32 type, window_t *win, glui32 val1, glui32 val2);
extern void gli_set_halfdelay(void);
extern void gli_shutdown_events(void);

extern void gli_input_handle_key(int key);
extern void gli_input_guess_focus(void);
extern glui32 gli_input_from_native(int key);

extern void gli_initialize_windows(void);
extern void gli_setup_curses(void);
extern void gli_fast_exit(void);
extern window_t *gli_new_window(glui32 type, glui32 rock);
extern void gli_delete_window(window_t *win);
extern window_t *gli_window_iterate_treeorder(window_t *win);
extern void gli_window_rearrange(window_t *win, grect_t *box);
extern void gli_window_redraw(window_t *win);
extern void gli_windows_redraw(void);
extern void gli_windows_update(void);
extern void gli_windows_size_change(void);
extern void gli_windows_place_cursor(void);
extern void gli_windows_set_paging(int forcetoend);
extern void gli_windows_trim_buffers(void);
extern void gli_window_put_char(window_t *win, char ch);
extern void gli_windows_unechostream(stream_t *str);
extern void gli_print_spaces(int len);

extern void gcmd_win_change_focus(window_t *win, glui32 arg);
extern void gcmd_win_refresh(window_t *win, glui32 arg);
extern void gcmd_win_resize(window_t *win, glui32 arg);

extern glui32 gli_sound_gestalt(glui32 id);
extern void gli_initialize_sound(void);
extern void gli_store_sound_events(void);
extern void gli_shutdown_sound(void);

extern void gli_initialize_styles(void);
extern void gli_initialize_window_styles(window_t *win);
extern int gli_compare_styles(const styleplus_t *styleplus1, const styleplus_t *styleplus2);
extern int gli_get_color_for_name(const char *name, glsi32 *color);
extern void gli_reset_styleplus(styleplus_t *styleplus, glui32 style);
extern int gli_set_window_style(window_t *win, const styleplus_t *styleplus);
extern void gli_set_inline_colors(stream_t *str, glui32 fg, glui32 bg);
extern void gli_set_inline_reverse(stream_t *str, glui32 reverse);
extern void gli_destroy_window_styles(window_t *win);
extern void gli_shutdown_styles(void);

extern stream_t *gli_new_stream(int type, int readable, int writable, 
    glui32 rock);
extern void gli_delete_stream(stream_t *str);
extern stream_t *gli_stream_open_window(window_t *win);
extern strid_t gli_stream_open_pathname(char *pathname, int writemode, 
    int textmode, glui32 rock);
extern void gli_stream_set_current(stream_t *str);
extern void gli_stream_fill_result(stream_t *str, 
    stream_result_t *result);
extern void gli_stream_echo_line(stream_t *str, char *buf, glui32 len);
extern void gli_stream_echo_line_uni(stream_t *str, glui32 *buf, glui32 len);
extern void gli_streams_close_all(void);

extern fileref_t *gli_new_fileref(char *filename, glui32 usage, 
    glui32 rock);
extern void gli_delete_fileref(fileref_t *fref);

/* A macro that I can't think of anywhere else to put it. */

#define gli_event_clearevent(evp)  \
    ((evp)->type = evtype_None,    \
    (evp)->win = NULL,    \
    (evp)->val1 = 0,   \
    (evp)->val2 = 0)

#ifdef NO_MEMMOVE
    extern void *memmove(void *dest, void *src, int n);
#endif /* NO_MEMMOVE */

#endif /* GLKTERM_H */
