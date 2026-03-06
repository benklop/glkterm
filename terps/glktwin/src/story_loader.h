#ifndef GLKTWIN_STORY_LOADER_H
#define GLKTWIN_STORY_LOADER_H

#include "glk.h"
#include "jsengine.h"

int glktwin_story_load(const char *path);
winid_t glktwin_main_window(void);
strid_t glktwin_main_stream(void);
void *glktwin_current_document(void);
JSEngine *glktwin_js_engine(void);
void glktwin_shutdown(void);

#endif
