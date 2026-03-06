/* dom_events.h: DOM event system — addEventListener, dispatchEvent, Glk event mapping. */

#ifndef GLKTWIN_DOM_EVENTS_H
#define GLKTWIN_DOM_EVENTS_H

#include "quickjs.h"
#include <stdbool.h>

void dom_events_init(JSContext *ctx);
void dom_events_shutdown(JSContext *ctx);

/* Dispatch a click event on the node registered for `link_val`. */
void dom_dispatch_hyperlink_click(JSContext *ctx, unsigned int link_val);

/* Dispatch an input/submit event for line input. */
void dom_dispatch_line_input(JSContext *ctx, const char *text);

/* Hyperlink registration for Glk hyperlink events. */
unsigned int dom_register_hyperlink(void *node_opaque);
void        *dom_lookup_hyperlink(unsigned int link_val);

/* Add/remove/fire event listeners on a node (called from dom_bindings.c). */
void dom_add_event_listener(JSContext *ctx, void *node,
                            const char *type, JSValue callback, bool capture);
void dom_remove_event_listener(JSContext *ctx, void *node,
                               const char *type, JSValue callback, bool capture);
bool dom_dispatch_event(JSContext *ctx, void *node, const char *type, JSValue event_obj);

/* Dispatch a named event on a DOM node, creating a simple Event object. */
void dom_fire_simple_event(JSContext *ctx, void *node, const char *type);

#endif
