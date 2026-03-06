/* dom_bindings.h: Lexbor DOM exposed to QuickJS (document, nodes). */

#ifndef GLKTWIN_DOM_BINDINGS_H
#define GLKTWIN_DOM_BINDINGS_H

#include "quickjs.h"

/* Opaque: lxb_html_document_t* from lexbor. */
typedef void *dom_document_t;

void dom_bindings_init(JSContext *ctx);
void dom_bindings_shutdown(JSContext *ctx);
void dom_bindings_set_document(JSContext *ctx, dom_document_t doc);
dom_document_t dom_bindings_get_document(JSContext *ctx);

/* Get the current document root element for rendering. */
void *dom_get_root(dom_document_t doc);

/* Get lexbor node pointer from a JS node object (for getComputedStyle). */
void *dom_bindings_node_opaque(JSContext *ctx, JSValueConst node_val);

/* Wrap a lexbor node pointer as a JS node object. */
JSValue dom_bindings_wrap_node(JSContext *ctx, void *node);

#endif
