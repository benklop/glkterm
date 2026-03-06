/* jsengine.h: QuickJS engine and Glk bindings for glktwin. */

#ifndef GLKTWIN_JSENGINE_H
#define GLKTWIN_JSENGINE_H

#include "quickjs.h"
#include <stddef.h>
#include <stdbool.h>

typedef struct JSEngine JSEngine;

JSEngine *jsengine_create(void);
void jsengine_destroy(JSEngine *e);

JSContext *jsengine_context(JSEngine *e);
JSRuntime *jsengine_runtime(JSEngine *e);

/* Load script (jQuery, format runtime, or story script). */
bool jsengine_eval(JSEngine *e, const char *src, size_t len, const char *name);
bool jsengine_load_jquery(JSEngine *e);

/* Called from main loop when evtype_Timer fires (advances clock by 100ms). */
void jsengine_timer_tick(JSEngine *e);

/* Advance timer clock by a specific amount and fire ready timers. */
void jsengine_timer_advance(JSEngine *e, uint32_t advance_ms);

/* Returns nonzero if there are pending timers. */
int jsengine_has_timers(JSEngine *e);

/* requestAnimationFrame queue: run all pending callbacks (call from main loop after render). */
void jsengine_raf_flush(JSEngine *e);

/* localStorage: persist to / load from Glk fileref. */
void jsengine_localStorage_save(JSEngine *e, const char *path);
void jsengine_localStorage_load(JSEngine *e, const char *path);

/* Stub: getComputedStyle(node) — delegate to CSS layer; returns JS object with TUI-relevant props. */
JSValue jsengine_get_computed_style(JSContext *ctx, void *node_opaque);

#endif
