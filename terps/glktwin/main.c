/* main.c: glktwin entry point. Implements glk_main() for the TUI Twine interpreter.
 * The glkterm library calls glkunix_startup_code() then glk_main().
 */

#include "glk.h"
#include "glkstart.h"
#include <stdio.h>

extern char *glktwin_story_path;

/* Forward declarations for glktwin modules (implemented in later steps). */
extern int glktwin_story_load(const char *path);
extern void glktwin_main_loop(void);
extern void glktwin_shutdown(void);

void glk_main(void)
{
    if (!glktwin_story_path) {
        fprintf(stderr, "glktwin: No story file given.\n"
                        "Usage: glktwin <story.html>\n");
        return;
    }

    if (!glktwin_story_load(glktwin_story_path))
        return;

    glktwin_main_loop();
    glktwin_shutdown();
}
