/* css_cascade.h: getComputedStyle, CSSOM from lexbor. */

#ifndef GLKTWIN_CSS_CASCADE_H
#define GLKTWIN_CSS_CASCADE_H

#include "quickjs.h"

void css_cascade_init(JSContext *ctx);

/* Return computed style object for node (opaque = lxb_dom_node_t*). */
JSValue css_get_computed_style(JSContext *ctx, void *node_opaque);

#endif
