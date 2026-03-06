/* dom_events.c: addEventListener/removeEventListener/dispatchEvent with bubbling.
 * Stores listeners per node in a global table keyed by node pointer.
 */

#include "dom_events.h"
#include "dom_bindings.h"
#include "lexbor_api.h"
#include <stdlib.h>
#include <string.h>

/* --- Hyperlink table --- */
static unsigned int next_hyperlink_id = 1;
#define MAX_LINKS 256
static struct { unsigned int id; void *node_opaque; } link_table[MAX_LINKS];
static int num_links;

/* --- Event listener storage --- */
typedef struct Listener {
    char *type;
    JSValue callback;
    bool capture;
} Listener;

typedef struct NodeListeners {
    void *node;
    Listener *listeners;
    int count;
    int cap;
} NodeListeners;

#define MAX_NODE_LISTENERS 1024
static NodeListeners nl_table[MAX_NODE_LISTENERS];
static int nl_count;

static NodeListeners *find_or_create_nl(void *node) {
    for (int i = 0; i < nl_count; i++)
        if (nl_table[i].node == node)
            return &nl_table[i];
    if (nl_count >= MAX_NODE_LISTENERS) return NULL;
    NodeListeners *nl = &nl_table[nl_count++];
    nl->node = node;
    nl->listeners = NULL;
    nl->count = 0;
    nl->cap = 0;
    return nl;
}

static NodeListeners *find_nl(void *node) {
    for (int i = 0; i < nl_count; i++)
        if (nl_table[i].node == node)
            return &nl_table[i];
    return NULL;
}

/* --- Init --- */
void dom_events_init(JSContext *ctx) {
    (void)ctx;
    num_links = 0;
    nl_count = 0;
}

void dom_events_shutdown(JSContext *ctx) {
    for (int i = 0; i < nl_count; i++) {
        NodeListeners *nl = &nl_table[i];
        for (int j = 0; j < nl->count; j++) {
            JS_FreeValue(ctx, nl->listeners[j].callback);
            free(nl->listeners[j].type);
        }
        free(nl->listeners);
        nl->listeners = NULL;
        nl->count = 0;
        nl->cap = 0;
    }
    nl_count = 0;
}

/* --- Listener management --- */

void dom_add_event_listener(JSContext *ctx, void *node,
                            const char *type, JSValue callback, bool capture) {
    NodeListeners *nl = find_or_create_nl(node);
    if (!nl) return;

    /* Duplicate check skipped: QuickJS JSValue is a struct, identity comparison
     * is not trivial. Registering the same handler twice is mostly harmless. */

    if (nl->count >= nl->cap) {
        int new_cap = nl->cap ? nl->cap * 2 : 4;
        Listener *p = realloc(nl->listeners, new_cap * sizeof(Listener));
        if (!p) return;
        nl->listeners = p;
        nl->cap = new_cap;
    }
    Listener *l = &nl->listeners[nl->count++];
    l->type = strdup(type);
    l->callback = JS_DupValue(ctx, callback);
    l->capture = capture;
}

void dom_remove_event_listener(JSContext *ctx, void *node,
                               const char *type, JSValue callback, bool capture) {
    NodeListeners *nl = find_nl(node);
    if (!nl) return;
    for (int i = 0; i < nl->count; i++) {
        Listener *l = &nl->listeners[i];
        if (l->capture == capture && strcmp(l->type, type) == 0) {
            JS_FreeValue(ctx, l->callback);
            free(l->type);
            nl->listeners[i] = nl->listeners[--nl->count];
            return;
        }
    }
}

/* --- Event dispatch with bubbling --- */

bool dom_dispatch_event(JSContext *ctx, void *node, const char *type, JSValue event_obj) {
    if (!node || !type) return true;

    /* Build ancestor chain for bubbling */
    lxb_dom_node_t *chain[128];
    int depth = 0;
    for (lxb_dom_node_t *n = (lxb_dom_node_t *)node;
         n && depth < 128; n = lxb_dom_node_parent_noi(n)) {
        chain[depth++] = n;
    }

    /* Set event properties */
    JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, type));
    JS_SetPropertyStr(ctx, event_obj, "bubbles", JS_TRUE);
    JS_SetPropertyStr(ctx, event_obj, "cancelable", JS_TRUE);

    bool cancelled = false;
    int stop = 0;

    /* Capture phase (ancestors down to target) */
    for (int i = depth - 1; i > 0 && !stop; i--) {
        NodeListeners *nl = find_nl(chain[i]);
        if (!nl) continue;
        for (int j = 0; j < nl->count && !stop; j++) {
            Listener *l = &nl->listeners[j];
            if (l->capture && strcmp(l->type, type) == 0) {
                JS_SetPropertyStr(ctx, event_obj, "currentTarget",
                    JS_DupValue(ctx, JS_UNDEFINED));
                JSValue ret = JS_Call(ctx, l->callback, JS_UNDEFINED, 1, &event_obj);
                if (JS_IsException(ret)) {
                    JSValue ex = JS_GetException(ctx);
                    JS_FreeValue(ctx, ex);
                }
                JS_FreeValue(ctx, ret);
            }
        }
    }

    /* Target phase + bubble phase */
    for (int i = 0; i < depth && !stop; i++) {
        NodeListeners *nl = find_nl(chain[i]);
        if (!nl) continue;
        bool is_target = (i == 0);
        for (int j = 0; j < nl->count && !stop; j++) {
            Listener *l = &nl->listeners[j];
            if (strcmp(l->type, type) != 0) continue;
            if (!is_target && l->capture) continue;
            if (is_target || !l->capture) {
                JSValue ret = JS_Call(ctx, l->callback, JS_UNDEFINED, 1, &event_obj);
                if (JS_IsException(ret)) {
                    JSValue ex = JS_GetException(ctx);
                    JS_FreeValue(ctx, ex);
                }
                JS_FreeValue(ctx, ret);
            }
        }
    }

    /* Check if preventDefault was called */
    JSValue pd = JS_GetPropertyStr(ctx, event_obj, "defaultPrevented");
    if (JS_ToBool(ctx, pd))
        cancelled = true;
    JS_FreeValue(ctx, pd);

    return !cancelled;
}

/* --- Simple event fire --- */

void dom_fire_simple_event(JSContext *ctx, void *node, const char *type) {
    JSValue evt = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, evt, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, evt, "preventDefault",
        JS_NewCFunction(ctx, (JSCFunction *)NULL, "preventDefault", 0));
    dom_dispatch_event(ctx, node, type, evt);
    JS_FreeValue(ctx, evt);
}

/* --- Hyperlink management --- */

unsigned int dom_register_hyperlink(void *node_opaque) {
    if (num_links >= MAX_LINKS) return 0;
    unsigned int id = next_hyperlink_id++;
    link_table[num_links].id = id;
    link_table[num_links].node_opaque = node_opaque;
    num_links++;
    return id;
}

void *dom_lookup_hyperlink(unsigned int link_val) {
    for (int i = 0; i < num_links; i++)
        if (link_table[i].id == link_val)
            return link_table[i].node_opaque;
    return NULL;
}

void dom_dispatch_hyperlink_click(JSContext *ctx, unsigned int link_val) {
    void *node = dom_lookup_hyperlink(link_val);
    if (node)
        dom_fire_simple_event(ctx, node, "click");
}

void dom_dispatch_line_input(JSContext *ctx, const char *text) {
    (void)ctx;
    (void)text;
}
