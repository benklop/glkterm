/* Test stubs for functions that are only available in the main executable */

#include "../glkterm/glk.h"
#include "../glkterm/glkterm.h"
#include "../glkterm/gtoption.h"

/* Preference stubs */
int pref_messageline = 1;
int pref_screenwidth = 80;
int pref_screenheight = 24;
int pref_historylen = 20;
int pref_color = 1;
int pref_fgcolor = 37;
int pref_bgcolor = 40;
int pref_reverse_textgrids = 0;
int pref_emph_underline = 0;
int pref_stylehint = 1;
int pref_precise_timing = 0;
int pref_override_window_borders = 0;
int pref_window_borders = 1;
int pref_prompt_defaults = 1;

/* Sound stubs */
void gli_shutdown_sound(void) {
    /* No-op for tests */
}

void gli_store_sound_events(void) {
    /* No-op for tests */
}

schanid_t glk_schannel_iterate(schanid_t chan, glui32 *rockptr) {
    /* No-op for tests - return NULL */
    if (rockptr) *rockptr = 0;
    return NULL;
}
