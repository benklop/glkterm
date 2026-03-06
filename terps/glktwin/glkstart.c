/* glkstart.c: Unix startup for glktwin (TUI Twine interpreter).
 * Single positional argument: path to Twine story HTML file.
 */

#include "glk.h"
#include "glkstart.h"

char *glktwin_story_path = NULL;

glkunix_argumentlist_t glkunix_arguments[] = {
    { "", glkunix_arg_ValueFollows, "filename: Twine story HTML file to run." },
    { NULL, glkunix_arg_End, NULL }
};

int glkunix_startup_code(glkunix_startup_t *data)
{
    if (data->argc >= 2)
        glktwin_story_path = data->argv[1];
    return TRUE;
}
