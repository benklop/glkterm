/* dom_bindings.c: Full DOM binding layer — Node/Element/Document exposed to QuickJS.
 * Uses Lexbor for the underlying DOM tree via lexbor_api.h wrappers.
 */

#include "dom_bindings.h"
#include "dom_events.h"
#include "lexbor_api.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static JSClassID node_class_id;
static dom_document_t current_doc;
static JSValue node_proto = JS_UNDEFINED;
static JSValue js_document_obj = JS_UNDEFINED; /* cached global document object */

/* ---- Node identity cache ----
 * Ensures the same lxb_dom_node_t* always maps to the same JSValue,
 * preserving jQuery expandos and other JS-side properties. */
#define NODE_CACHE_SIZE 4096
typedef struct {
    lxb_dom_node_t *key;
    JSValue val;
} node_cache_entry_t;

static node_cache_entry_t node_cache[NODE_CACHE_SIZE];
static int node_cache_count = 0;
static JSContext *cached_ctx = NULL;

static void node_cache_clear(void) {
    if (cached_ctx) {
        int i;
        for (i = 0; i < node_cache_count; i++) {
            JS_FreeValue(cached_ctx, node_cache[i].val);
            node_cache[i].key = NULL;
        }
    }
    node_cache_count = 0;
    cached_ctx = NULL;
}

static JSValue node_cache_get(lxb_dom_node_t *node) {
    int i;
    for (i = 0; i < node_cache_count; i++) {
        if (node_cache[i].key == node)
            return JS_DupValue(cached_ctx, node_cache[i].val);
    }
    return JS_UNDEFINED;
}

static void node_cache_put(lxb_dom_node_t *node, JSValue val) {
    if (node_cache_count < NODE_CACHE_SIZE) {
        node_cache[node_cache_count].key = node;
        node_cache[node_cache_count].val = JS_DupValue(cached_ctx, val);
        node_cache_count++;
    }
}

/* ---- Helpers ---- */
static JSValue js_return_empty(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    (void)this_val; (void)argc; (void)argv;
    return JS_NewString(ctx, "");
}

static JSValue js_return_null(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_NULL;
}

/* ============================================================
 * Node wrapping: JS object ↔ lxb_dom_node_t*
 * ============================================================ */

static void node_finalizer(JSRuntime *rt, JSValue val) {
    (void)rt; (void)val;
}

static JSValue wrap_node(JSContext *ctx, lxb_dom_node_t *node) {
    if (!node) return JS_NULL;
    /* Return the global document object for document-type nodes so that
     * jQuery event handlers attached to $(document) are found when
     * events bubble up through parentNode. */
    if (lxb_dom_node_type_noi(node) == LXB_DOM_NODE_TYPE_DOCUMENT &&
        !JS_IsUndefined(js_document_obj))
        return JS_DupValue(ctx, js_document_obj);
    JSValue cached = node_cache_get(node);
    if (!JS_IsUndefined(cached)) return cached;
    JSValue obj = JS_NewObjectClass(ctx, node_class_id);
    if (JS_IsException(obj)) return obj;
    JS_SetOpaque(obj, node);
    node_cache_put(node, obj);
    return obj;
}

static lxb_dom_node_t *unwrap_node(JSContext *ctx, JSValueConst val) {
    return (lxb_dom_node_t *)JS_GetOpaque2(ctx, val, node_class_id);
}

/* ============================================================
 * Node prototype methods
 * ============================================================ */

static JSValue js_node_get_nodeType(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node) return JS_UNDEFINED;
    return JS_NewInt32(ctx, (int)lxb_dom_node_type_noi(node));
}

static JSValue js_node_get_nodeName(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node) return JS_UNDEFINED;
    lxb_dom_node_type_t type = lxb_dom_node_type_noi(node);
    if (type == LXB_DOM_NODE_TYPE_TEXT) return JS_NewString(ctx, "#text");
    if (type == LXB_DOM_NODE_TYPE_COMMENT) return JS_NewString(ctx, "#comment");
    if (type == LXB_DOM_NODE_TYPE_DOCUMENT) return JS_NewString(ctx, "#document");
    if (type == LXB_DOM_NODE_TYPE_DOCUMENT_FRAGMENT)
        return JS_NewString(ctx, "#document-fragment");
    const char *tag = glktwin_get_tag_name(node);
    return JS_NewString(ctx, tag);
}

static JSValue js_node_get_tagName(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node || lxb_dom_node_type_noi(node) != LXB_DOM_NODE_TYPE_ELEMENT)
        return JS_UNDEFINED;
    const char *tag = glktwin_get_tag_name(node);
    return JS_NewString(ctx, tag);
}

static JSValue js_node_get_nodeValue(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node) return JS_NULL;
    size_t len = 0;
    const char *data = glktwin_text_node_data(node, &len);
    if (!data) return JS_NULL;
    return JS_NewStringLen(ctx, data, len);
}

static JSValue js_node_set_nodeValue(JSContext *ctx, JSValueConst this_val,
                                      JSValue val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node) return JS_UNDEFINED;
    size_t len;
    const char *str = JS_ToCStringLen(ctx, &len, val);
    if (!str) return JS_UNDEFINED;
    glktwin_text_node_set_data(node, str, len);
    JS_FreeCString(ctx, str);
    return JS_UNDEFINED;
}

static JSValue js_node_get_parentNode(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node) return JS_NULL;
    return wrap_node(ctx, lxb_dom_node_parent_noi(node));
}

static JSValue js_node_get_parentElement(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node) return JS_NULL;
    lxb_dom_node_t *parent = lxb_dom_node_parent_noi(node);
    if (parent && lxb_dom_node_type_noi(parent) == LXB_DOM_NODE_TYPE_ELEMENT)
        return wrap_node(ctx, parent);
    return JS_NULL;
}

static JSValue js_node_get_firstChild(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    return node ? wrap_node(ctx, lxb_dom_node_first_child_noi(node)) : JS_NULL;
}

static JSValue js_node_get_lastChild(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    return node ? wrap_node(ctx, lxb_dom_node_last_child_noi(node)) : JS_NULL;
}

static JSValue js_node_get_nextSibling(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    return node ? wrap_node(ctx, lxb_dom_node_next_noi(node)) : JS_NULL;
}

static JSValue js_node_get_previousSibling(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    return node ? wrap_node(ctx, lxb_dom_node_prev_noi(node)) : JS_NULL;
}

static JSValue js_node_get_nextElementSibling(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node) return JS_NULL;
    for (lxb_dom_node_t *n = lxb_dom_node_next_noi(node); n; n = lxb_dom_node_next_noi(n))
        if (lxb_dom_node_type_noi(n) == LXB_DOM_NODE_TYPE_ELEMENT)
            return wrap_node(ctx, n);
    return JS_NULL;
}

static JSValue js_node_get_previousElementSibling(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node) return JS_NULL;
    for (lxb_dom_node_t *n = lxb_dom_node_prev_noi(node); n; n = lxb_dom_node_prev_noi(n))
        if (lxb_dom_node_type_noi(n) == LXB_DOM_NODE_TYPE_ELEMENT)
            return wrap_node(ctx, n);
    return JS_NULL;
}

static JSValue js_node_get_childNodes(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node) return JS_NewArray(ctx);
    JSValue arr = JS_NewArray(ctx);
    uint32_t idx = 0;
    for (lxb_dom_node_t *c = lxb_dom_node_first_child_noi(node);
         c; c = lxb_dom_node_next_noi(c))
        JS_SetPropertyUint32(ctx, arr, idx++, wrap_node(ctx, c));
    return arr;
}

static JSValue js_node_get_children(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node) return JS_NewArray(ctx);
    JSValue arr = JS_NewArray(ctx);
    uint32_t idx = 0;
    for (lxb_dom_node_t *c = lxb_dom_node_first_child_noi(node);
         c; c = lxb_dom_node_next_noi(c))
        if (lxb_dom_node_type_noi(c) == LXB_DOM_NODE_TYPE_ELEMENT)
            JS_SetPropertyUint32(ctx, arr, idx++, wrap_node(ctx, c));
    return arr;
}

static JSValue js_node_get_firstElementChild(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node) return JS_NULL;
    for (lxb_dom_node_t *c = lxb_dom_node_first_child_noi(node);
         c; c = lxb_dom_node_next_noi(c))
        if (lxb_dom_node_type_noi(c) == LXB_DOM_NODE_TYPE_ELEMENT)
            return wrap_node(ctx, c);
    return JS_NULL;
}

/* --- Node manipulation --- */

static void append_one_node(lxb_dom_node_t *parent, lxb_dom_node_t *child);

static JSValue js_node_appendChild(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    lxb_dom_node_t *parent = unwrap_node(ctx, this_val);
    lxb_dom_node_t *child = argc > 0 ? unwrap_node(ctx, argv[0]) : NULL;
    if (!parent || !child) return JS_UNDEFINED;
    append_one_node(parent, child);
    return JS_DupValue(ctx, argv[0]);
}

static JSValue js_node_removeChild(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    (void)this_val;
    lxb_dom_node_t *child = argc > 0 ? unwrap_node(ctx, argv[0]) : NULL;
    if (!child) return JS_UNDEFINED;
    lxb_dom_node_remove(child);
    return JS_DupValue(ctx, argv[0]);
}

static JSValue js_node_insertBefore(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
    lxb_dom_node_t *parent = unwrap_node(ctx, this_val);
    lxb_dom_node_t *newNode = argc > 0 ? unwrap_node(ctx, argv[0]) : NULL;
    lxb_dom_node_t *refNode = argc > 1 && !JS_IsNull(argv[1]) && !JS_IsUndefined(argv[1])
                              ? unwrap_node(ctx, argv[1]) : NULL;
    if (!parent || !newNode) return JS_UNDEFINED;
    if (lxb_dom_node_type_noi(newNode) == LXB_DOM_NODE_TYPE_DOCUMENT_FRAGMENT) {
        lxb_dom_node_t *cur = lxb_dom_node_first_child_noi(newNode);
        while (cur) {
            lxb_dom_node_t *next = lxb_dom_node_next_noi(cur);
            lxb_dom_node_remove(cur);
            if (refNode)
                lxb_dom_node_insert_before(refNode, cur);
            else
                lxb_dom_node_insert_child(parent, cur);
            cur = next;
        }
    } else {
        lxb_dom_node_remove(newNode);
        if (refNode)
            lxb_dom_node_insert_before(refNode, newNode);
        else
            lxb_dom_node_insert_child(parent, newNode);
    }
    return JS_DupValue(ctx, argv[0]);
}

static JSValue js_node_replaceChild(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
    (void)this_val;
    lxb_dom_node_t *newChild = argc > 0 ? unwrap_node(ctx, argv[0]) : NULL;
    lxb_dom_node_t *oldChild = argc > 1 ? unwrap_node(ctx, argv[1]) : NULL;
    if (!newChild || !oldChild) return JS_UNDEFINED;
    if (lxb_dom_node_type_noi(newChild) == LXB_DOM_NODE_TYPE_DOCUMENT_FRAGMENT) {
        lxb_dom_node_t *cur = lxb_dom_node_first_child_noi(newChild);
        while (cur) {
            lxb_dom_node_t *next = lxb_dom_node_next_noi(cur);
            lxb_dom_node_remove(cur);
            lxb_dom_node_insert_before(oldChild, cur);
            cur = next;
        }
        lxb_dom_node_remove(oldChild);
    } else {
        lxb_dom_node_remove(newChild);
        lxb_dom_node_insert_before(oldChild, newChild);
        lxb_dom_node_remove(oldChild);
    }
    return argc > 1 ? JS_DupValue(ctx, argv[1]) : JS_UNDEFINED;
}

static JSValue js_node_cloneNode(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node) return JS_NULL;
    bool deep = false;
    if (argc > 0) deep = JS_ToBool(ctx, argv[0]);
    lxb_dom_node_t *clone = glktwin_clone_node(node, deep);
    return wrap_node(ctx, clone);
}

static JSValue js_node_contains(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    lxb_dom_node_t *other = argc > 0 ? unwrap_node(ctx, argv[0]) : NULL;
    if (!node || !other) return JS_FALSE;
    for (lxb_dom_node_t *n = other; n; n = lxb_dom_node_parent_noi(n))
        if (n == node) return JS_TRUE;
    return JS_FALSE;
}

static JSValue js_node_hasChildNodes(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    return JS_NewBool(ctx, node && lxb_dom_node_first_child_noi(node) != NULL);
}

static JSValue js_node_compareDocumentPosition(JSContext *ctx, JSValueConst this_val,
                                               int argc, JSValueConst *argv) {
    lxb_dom_node_t *a = unwrap_node(ctx, this_val);
    lxb_dom_node_t *b = argc > 0 ? unwrap_node(ctx, argv[0]) : NULL;
    if (!a || !b) return JS_NewInt32(ctx, 1); /* DISCONNECTED */
    if (a == b) return JS_NewInt32(ctx, 0);

    /* Check if b is a descendant of a */
    for (lxb_dom_node_t *n = b; n; n = lxb_dom_node_parent_noi(n)) {
        if (n == a) return JS_NewInt32(ctx, 0x14); /* CONTAINS | FOLLOWING */
    }
    /* Check if a is a descendant of b */
    for (lxb_dom_node_t *n = a; n; n = lxb_dom_node_parent_noi(n)) {
        if (n == b) return JS_NewInt32(ctx, 0x0A); /* CONTAINED_BY | PRECEDING */
    }
    return JS_NewInt32(ctx, 0x04); /* FOLLOWING */
}

static JSValue js_node_getRootNode(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node) return JS_NULL;
    while (lxb_dom_node_parent_noi(node))
        node = lxb_dom_node_parent_noi(node);
    return wrap_node(ctx, node);
}

static JSValue js_node_isConnected(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node) return JS_FALSE;
    while (lxb_dom_node_parent_noi(node))
        node = lxb_dom_node_parent_noi(node);
    return JS_NewBool(ctx,
        lxb_dom_node_type_noi(node) == LXB_DOM_NODE_TYPE_DOCUMENT);
}

static JSValue js_node_remove_self(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    (void)argc; (void)argv;
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (node) {
        lxb_dom_node_remove(node);
    }
    return JS_UNDEFINED;
}

static void append_one_node(lxb_dom_node_t *parent, lxb_dom_node_t *child) {
    if (!parent || !child) return;
    if (lxb_dom_node_type_noi(child) == LXB_DOM_NODE_TYPE_DOCUMENT_FRAGMENT) {
        lxb_dom_node_t *cur = lxb_dom_node_first_child_noi(child);
        while (cur) {
            lxb_dom_node_t *next = lxb_dom_node_next_noi(cur);
            lxb_dom_node_remove(cur);
            lxb_dom_node_insert_child(parent, cur);
            cur = next;
        }
    } else {
        lxb_dom_node_remove(child);
        lxb_dom_node_insert_child(parent, child);
    }
}

static JSValue js_node_append(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    lxb_dom_node_t *parent = unwrap_node(ctx, this_val);
    if (!parent) return JS_UNDEFINED;
    for (int i = 0; i < argc; i++) {
        if (JS_IsString(argv[i])) {
            const char *text = JS_ToCString(ctx, argv[i]);
            if (text) {
                lxb_dom_document_t *doc = (lxb_dom_document_t *)current_doc;
                lxb_dom_text_t *tn = lxb_dom_document_create_text_node(
                    doc, (const lxb_char_t *)text, strlen(text));
                if (tn) lxb_dom_node_insert_child(parent, lxb_dom_interface_node(tn));
                JS_FreeCString(ctx, text);
            }
        } else {
            lxb_dom_node_t *child = unwrap_node(ctx, argv[i]);
            if (child) append_one_node(parent, child);
        }
    }
    return JS_UNDEFINED;
}

static JSValue js_node_prepend(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    lxb_dom_node_t *parent = unwrap_node(ctx, this_val);
    if (!parent) return JS_UNDEFINED;
    lxb_dom_node_t *first = lxb_dom_node_first_child_noi(parent);
    for (int i = 0; i < argc; i++) {
        lxb_dom_node_t *child = unwrap_node(ctx, argv[i]);
        if (!child) continue;
        if (lxb_dom_node_type_noi(child) == LXB_DOM_NODE_TYPE_DOCUMENT_FRAGMENT) {
            lxb_dom_node_t *cur = lxb_dom_node_first_child_noi(child);
            while (cur) {
                lxb_dom_node_t *next = lxb_dom_node_next_noi(cur);
                lxb_dom_node_remove(cur);
                if (first) lxb_dom_node_insert_before(first, cur);
                else lxb_dom_node_insert_child(parent, cur);
                cur = next;
            }
        } else {
            lxb_dom_node_remove(child);
            if (first)
                lxb_dom_node_insert_before(first, child);
            else
                lxb_dom_node_insert_child(parent, child);
        }
    }
    return JS_UNDEFINED;
}

/* --- Text content & innerHTML --- */

static JSValue js_node_get_textContent(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node) return JS_NULL;
    size_t len = 0;
    char *text = glktwin_get_text_content(node, &len);
    if (!text) return JS_NewString(ctx, "");
    JSValue s = JS_NewStringLen(ctx, text, len);
    free(text);
    return s;
}

static JSValue js_node_set_textContent(JSContext *ctx, JSValueConst this_val, JSValue val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node) return JS_UNDEFINED;
    const char *text = JS_ToCString(ctx, val);
    if (!text) return JS_UNDEFINED;

    /* Remove existing children without destroying them (JS may still reference them).
     * lxb_dom_node_text_content_set would call lxb_dom_node_destroy on children,
     * causing use-after-free when JS later manipulates those nodes. */
    while (lxb_dom_node_first_child_noi(node)) {
        lxb_dom_node_remove(lxb_dom_node_first_child_noi(node));
    }
    size_t tlen = strlen(text);
    if (tlen > 0) {
        lxb_dom_document_t *doc = (lxb_dom_document_t *)current_doc;
        lxb_dom_text_t *tn = lxb_dom_document_create_text_node(
            doc, (const lxb_char_t *)text, tlen);
        if (tn) lxb_dom_node_insert_child(node, lxb_dom_interface_node(tn));
    }

    JS_FreeCString(ctx, text);
    return JS_UNDEFINED;
}

static JSValue js_node_get_innerHTML(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node) return JS_NewString(ctx, "");
    size_t len = 0;
    char *html = glktwin_serialize_inner_html(node, &len);
    if (!html) return JS_NewString(ctx, "");
    JSValue s = JS_NewStringLen(ctx, html, len);
    free(html);
    return s;
}

static JSValue js_node_set_innerHTML(JSContext *ctx, JSValueConst this_val, JSValue val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node || lxb_dom_node_type_noi(node) != LXB_DOM_NODE_TYPE_ELEMENT)
        return JS_UNDEFINED;
    const char *html = JS_ToCString(ctx, val);
    if (!html) return JS_UNDEFINED;
    glktwin_set_inner_html(lxb_dom_interface_element(node), current_doc,
                           html, strlen(html));
    JS_FreeCString(ctx, html);
    return JS_UNDEFINED;
}

static JSValue js_node_get_outerHTML(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node) return JS_NewString(ctx, "");
    /* outerHTML = the node itself serialized (including its tag + children) */
    size_t len = 0;
    char *html = glktwin_serialize_inner_html(lxb_dom_node_parent_noi(node), &len);
    /* Fallback: just serialize the node's inner with wrapping tag */
    if (!html) return JS_NewString(ctx, "");
    /* Actually, we should serialize just this node. Let's use inner_html of parent
       and extract... but that's complex. Use a simpler approach. */
    free(html);

    /* Serialize this single node + its subtree via the inner HTML of a temp wrapper */
    /* For now, return innerHTML wrapped in the tag name */
    const char *tag = glktwin_get_tag_name(node);
    char *inner = glktwin_serialize_inner_html(node, &len);
    if (!inner) inner = strdup("");
    size_t total = strlen(tag) * 2 + len + 10;
    char *buf = malloc(total);
    if (!buf) { free(inner); return JS_NewString(ctx, ""); }
    char lower_tag[64];
    size_t tlen = strlen(tag);
    if (tlen >= sizeof(lower_tag)) tlen = sizeof(lower_tag) - 1;
    for (size_t i = 0; i < tlen; i++)
        lower_tag[i] = (char)tolower((unsigned char)tag[i]);
    lower_tag[tlen] = '\0';
    snprintf(buf, total, "<%s>%s</%s>", lower_tag, inner, lower_tag);
    JSValue s = JS_NewString(ctx, buf);
    free(buf);
    free(inner);
    return s;
}

/* --- Attributes --- */

static JSValue js_node_getAttribute(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node || argc < 1 || lxb_dom_node_type_noi(node) != LXB_DOM_NODE_TYPE_ELEMENT)
        return JS_NULL;
    const char *name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_NULL;
    size_t val_len = 0;
    const lxb_char_t *val = lxb_dom_element_get_attribute(
        lxb_dom_interface_element(node),
        (const lxb_char_t *)name, strlen(name), &val_len);
    JS_FreeCString(ctx, name);
    if (!val) return JS_NULL;
    return JS_NewStringLen(ctx, (const char *)val, val_len);
}

static JSValue js_node_setAttribute(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node || argc < 2 || lxb_dom_node_type_noi(node) != LXB_DOM_NODE_TYPE_ELEMENT)
        return JS_UNDEFINED;
    const char *name = JS_ToCString(ctx, argv[0]);
    const char *value = JS_ToCString(ctx, argv[1]);
    if (name && value)
        lxb_dom_element_set_attribute(lxb_dom_interface_element(node),
            (const lxb_char_t *)name, strlen(name),
            (const lxb_char_t *)value, strlen(value));
    if (name) JS_FreeCString(ctx, name);
    if (value) JS_FreeCString(ctx, value);
    return JS_UNDEFINED;
}

static JSValue js_node_removeAttribute(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node || argc < 1 || lxb_dom_node_type_noi(node) != LXB_DOM_NODE_TYPE_ELEMENT)
        return JS_UNDEFINED;
    const char *name = JS_ToCString(ctx, argv[0]);
    if (name) {
        lxb_dom_element_remove_attribute(lxb_dom_interface_element(node),
            (const lxb_char_t *)name, strlen(name));
        JS_FreeCString(ctx, name);
    }
    return JS_UNDEFINED;
}

static JSValue js_node_hasAttribute(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node || argc < 1 || lxb_dom_node_type_noi(node) != LXB_DOM_NODE_TYPE_ELEMENT)
        return JS_FALSE;
    const char *name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_FALSE;
    bool has = lxb_dom_element_has_attribute(lxb_dom_interface_element(node),
        (const lxb_char_t *)name, strlen(name));
    JS_FreeCString(ctx, name);
    return JS_NewBool(ctx, has);
}

/* --- id / className --- */

static JSValue js_node_get_id(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node || lxb_dom_node_type_noi(node) != LXB_DOM_NODE_TYPE_ELEMENT)
        return JS_NewString(ctx, "");
    size_t len = 0;
    const lxb_char_t *val = lxb_dom_element_get_attribute(
        lxb_dom_interface_element(node), (const lxb_char_t *)"id", 2, &len);
    if (!val) return JS_NewString(ctx, "");
    return JS_NewStringLen(ctx, (const char *)val, len);
}

static JSValue js_node_set_id(JSContext *ctx, JSValueConst this_val, JSValue val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node || lxb_dom_node_type_noi(node) != LXB_DOM_NODE_TYPE_ELEMENT)
        return JS_UNDEFINED;
    const char *s = JS_ToCString(ctx, val);
    if (s) {
        lxb_dom_element_set_attribute(lxb_dom_interface_element(node),
            (const lxb_char_t *)"id", 2,
            (const lxb_char_t *)s, strlen(s));
        JS_FreeCString(ctx, s);
    }
    return JS_UNDEFINED;
}

static JSValue js_node_get_className(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node || lxb_dom_node_type_noi(node) != LXB_DOM_NODE_TYPE_ELEMENT)
        return JS_NewString(ctx, "");
    size_t len = 0;
    const lxb_char_t *val = lxb_dom_element_get_attribute(
        lxb_dom_interface_element(node), (const lxb_char_t *)"class", 5, &len);
    if (!val) return JS_NewString(ctx, "");
    return JS_NewStringLen(ctx, (const char *)val, len);
}

static JSValue js_node_set_className(JSContext *ctx, JSValueConst this_val, JSValue val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node || lxb_dom_node_type_noi(node) != LXB_DOM_NODE_TYPE_ELEMENT)
        return JS_UNDEFINED;
    const char *s = JS_ToCString(ctx, val);
    if (s) {
        lxb_dom_element_set_attribute(lxb_dom_interface_element(node),
            (const lxb_char_t *)"class", 5,
            (const lxb_char_t *)s, strlen(s));
        JS_FreeCString(ctx, s);
    }
    return JS_UNDEFINED;
}

/* --- classList (returns object with add/remove/contains/toggle) --- */

static JSValue js_classList_add(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
    lxb_dom_node_t *node = (lxb_dom_node_t *)JS_GetOpaque(this_val, 0);
    if (!node) {
        JSValue nodeVal = JS_GetPropertyStr(ctx, this_val, "_node");
        node = unwrap_node(ctx, nodeVal);
        JS_FreeValue(ctx, nodeVal);
    }
    if (!node) return JS_UNDEFINED;
    lxb_dom_element_t *el = lxb_dom_interface_element(node);
    size_t cls_len = 0;
    const lxb_char_t *cls = lxb_dom_element_get_attribute(el,
        (const lxb_char_t *)"class", 5, &cls_len);
    char buf[2048];
    if (cls && cls_len > 0 && cls_len < sizeof(buf) - 1) {
        memcpy(buf, cls, cls_len);
        buf[cls_len] = '\0';
    } else {
        buf[0] = '\0';
        cls_len = 0;
    }
    for (int i = 0; i < argc; i++) {
        const char *name = JS_ToCString(ctx, argv[i]);
        if (!name) continue;
        if (!strstr(buf, name)) {
            size_t nlen = strlen(name);
            size_t blen = strlen(buf);
            if (blen + nlen + 2 < sizeof(buf)) {
                if (blen > 0) buf[blen++] = ' ';
                memcpy(buf + blen, name, nlen);
                buf[blen + nlen] = '\0';
            }
        }
        JS_FreeCString(ctx, name);
    }
    lxb_dom_element_set_attribute(el, (const lxb_char_t *)"class", 5,
        (const lxb_char_t *)buf, strlen(buf));
    return JS_UNDEFINED;
}

static JSValue js_classList_remove(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    JSValue nodeVal = JS_GetPropertyStr(ctx, this_val, "_node");
    lxb_dom_node_t *node = unwrap_node(ctx, nodeVal);
    JS_FreeValue(ctx, nodeVal);
    if (!node) return JS_UNDEFINED;
    lxb_dom_element_t *el = lxb_dom_interface_element(node);
    size_t cls_len = 0;
    const lxb_char_t *cls = lxb_dom_element_get_attribute(el,
        (const lxb_char_t *)"class", 5, &cls_len);
    if (!cls) return JS_UNDEFINED;
    char buf[2048];
    if (cls_len >= sizeof(buf)) cls_len = sizeof(buf) - 1;
    memcpy(buf, cls, cls_len);
    buf[cls_len] = '\0';

    for (int i = 0; i < argc; i++) {
        const char *name = JS_ToCString(ctx, argv[i]);
        if (!name) continue;
        size_t nlen = strlen(name);
        char *p = buf;
        char result[2048];
        result[0] = '\0';
        char *tok = strtok(p, " ");
        while (tok) {
            if (strcmp(tok, name) != 0) {
                if (result[0]) strcat(result, " ");
                strcat(result, tok);
            }
            tok = strtok(NULL, " ");
        }
        strcpy(buf, result);
        JS_FreeCString(ctx, name);
    }
    lxb_dom_element_set_attribute(el, (const lxb_char_t *)"class", 5,
        (const lxb_char_t *)buf, strlen(buf));
    return JS_UNDEFINED;
}

static JSValue js_classList_contains(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv) {
    JSValue nodeVal = JS_GetPropertyStr(ctx, this_val, "_node");
    lxb_dom_node_t *node = unwrap_node(ctx, nodeVal);
    JS_FreeValue(ctx, nodeVal);
    if (!node || argc < 1) return JS_FALSE;
    lxb_dom_element_t *el = lxb_dom_interface_element(node);
    size_t cls_len = 0;
    const lxb_char_t *cls = lxb_dom_element_get_attribute(el,
        (const lxb_char_t *)"class", 5, &cls_len);
    if (!cls) return JS_FALSE;
    const char *name = JS_ToCString(ctx, argv[0]);
    if (!name) return JS_FALSE;
    char buf[2048];
    if (cls_len >= sizeof(buf)) cls_len = sizeof(buf) - 1;
    memcpy(buf, cls, cls_len);
    buf[cls_len] = '\0';
    size_t nlen = strlen(name);
    bool found = false;
    char *tok = strtok(buf, " ");
    while (tok) {
        if (strlen(tok) == nlen && strcmp(tok, name) == 0) { found = true; break; }
        tok = strtok(NULL, " ");
    }
    JS_FreeCString(ctx, name);
    return JS_NewBool(ctx, found);
}

static JSValue js_classList_toggle(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    JSValue has = js_classList_contains(ctx, this_val, argc, argv);
    if (JS_ToBool(ctx, has)) {
        js_classList_remove(ctx, this_val, argc, argv);
        return JS_FALSE;
    }
    js_classList_add(ctx, this_val, argc, argv);
    return JS_TRUE;
}

static JSValue js_node_get_classList(JSContext *ctx, JSValueConst this_val) {
    JSValue cl = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, cl, "_node", JS_DupValue(ctx, this_val));
    JS_SetPropertyStr(ctx, cl, "add", JS_NewCFunction(ctx, js_classList_add, "add", 1));
    JS_SetPropertyStr(ctx, cl, "remove", JS_NewCFunction(ctx, js_classList_remove, "remove", 1));
    JS_SetPropertyStr(ctx, cl, "contains", JS_NewCFunction(ctx, js_classList_contains, "contains", 1));
    JS_SetPropertyStr(ctx, cl, "toggle", JS_NewCFunction(ctx, js_classList_toggle, "toggle", 1));
    return cl;
}

/* --- style property: uses JS Proxy to sync back to the element's style
 * attribute on every property set, so that the C renderer can detect
 * display:none, visibility:hidden, etc. via the DOM attribute. --- */

static JSValue js_node_get_style(JSContext *ctx, JSValueConst this_val) {
    /* Return the cached Proxy if it already exists on this node wrapper. */
    JSValue cached = JS_GetPropertyStr(ctx, this_val, "_$s");
    if (!JS_IsUndefined(cached) && !JS_IsNull(cached))
        return cached;
    /* Lazily created by the JS init snippet (see style_proxy_js below);
     * fallback to a plain object if the snippet hasn't run yet. */
    JSValue style = JS_NewObject(ctx);
    return style;
}

/* --- querySelector / querySelectorAll --- */

static JSValue js_node_querySelector(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node || argc < 1) return JS_NULL;
    const char *sel = JS_ToCString(ctx, argv[0]);
    if (!sel) return JS_NULL;
    lxb_dom_node_t *found = glktwin_query_selector(current_doc, node, sel);
    JS_FreeCString(ctx, sel);
    return wrap_node(ctx, found);
}

static JSValue js_node_querySelectorAll(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node || argc < 1) return JS_NewArray(ctx);
    const char *sel = JS_ToCString(ctx, argv[0]);
    if (!sel) return JS_NewArray(ctx);
    size_t count = 0;
    lxb_dom_node_t **results = glktwin_query_selector_all(current_doc, node, sel, &count);
    JS_FreeCString(ctx, sel);
    JSValue arr = JS_NewArray(ctx);
    for (size_t i = 0; i < count; i++)
        JS_SetPropertyUint32(ctx, arr, (uint32_t)i, wrap_node(ctx, results[i]));
    free(results);
    return arr;
}

static JSValue js_node_matches(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node || argc < 1) return JS_FALSE;
    const char *sel = JS_ToCString(ctx, argv[0]);
    if (!sel) return JS_FALSE;
    bool m = glktwin_element_matches(current_doc, node, sel);
    JS_FreeCString(ctx, sel);
    return JS_NewBool(ctx, m);
}

static JSValue js_node_closest(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node || argc < 1) return JS_NULL;
    const char *sel = JS_ToCString(ctx, argv[0]);
    if (!sel) return JS_NULL;
    for (lxb_dom_node_t *n = node; n; n = lxb_dom_node_parent_noi(n)) {
        if (lxb_dom_node_type_noi(n) == LXB_DOM_NODE_TYPE_ELEMENT &&
            glktwin_element_matches(current_doc, n, sel)) {
            JS_FreeCString(ctx, sel);
            return wrap_node(ctx, n);
        }
    }
    JS_FreeCString(ctx, sel);
    return JS_NULL;
}

/* --- Events (addEventListener, removeEventListener, dispatchEvent) --- */

static JSValue js_node_addEventListener(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node || argc < 2) return JS_UNDEFINED;
    const char *type = JS_ToCString(ctx, argv[0]);
    if (!type) return JS_UNDEFINED;
    bool capture = false;
    if (argc > 2) {
        if (JS_IsObject(argv[2])) {
            JSValue cap = JS_GetPropertyStr(ctx, argv[2], "capture");
            capture = JS_ToBool(ctx, cap);
            JS_FreeValue(ctx, cap);
        } else {
            capture = JS_ToBool(ctx, argv[2]);
        }
    }
    dom_add_event_listener(ctx, node, type, argv[1], capture);
    JS_FreeCString(ctx, type);
    return JS_UNDEFINED;
}

static JSValue js_node_removeEventListener(JSContext *ctx, JSValueConst this_val,
                                           int argc, JSValueConst *argv) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node || argc < 2) return JS_UNDEFINED;
    const char *type = JS_ToCString(ctx, argv[0]);
    if (!type) return JS_UNDEFINED;
    bool capture = false;
    if (argc > 2) capture = JS_ToBool(ctx, argv[2]);
    dom_remove_event_listener(ctx, node, type, argv[1], capture);
    JS_FreeCString(ctx, type);
    return JS_UNDEFINED;
}

static JSValue js_node_dispatchEvent(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node || argc < 1) return JS_FALSE;
    JSValue typeVal = JS_GetPropertyStr(ctx, argv[0], "type");
    const char *type = JS_ToCString(ctx, typeVal);
    JS_FreeValue(ctx, typeVal);
    if (!type) return JS_FALSE;
    bool result = dom_dispatch_event(ctx, node, type, argv[0]);
    JS_FreeCString(ctx, type);
    return JS_NewBool(ctx, result);
}

/* --- getBoundingClientRect stub --- */

static JSValue js_node_getBoundingClientRect(JSContext *ctx, JSValueConst this_val,
                                             int argc, JSValueConst *argv) {
    (void)this_val; (void)argc; (void)argv;
    JSValue rect = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, rect, "top", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, rect, "left", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, rect, "bottom", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, rect, "right", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, rect, "width", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, rect, "height", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, rect, "x", JS_NewFloat64(ctx, 0));
    JS_SetPropertyStr(ctx, rect, "y", JS_NewFloat64(ctx, 0));
    return rect;
}

/* --- attributes (NamedNodeMap-like array of {name,value} objects) --- */

static JSValue js_node_get_attributes(JSContext *ctx, JSValueConst this_val) {
    lxb_dom_node_t *node = unwrap_node(ctx, this_val);
    if (!node || lxb_dom_node_type_noi(node) != LXB_DOM_NODE_TYPE_ELEMENT)
        return JS_NewArray(ctx);
    lxb_dom_element_t *el = lxb_dom_interface_element(node);
    JSValue arr = JS_NewArray(ctx);
    uint32_t idx = 0;
    lxb_dom_attr_t *attr = lxb_dom_element_first_attribute_noi(el);
    while (attr) {
        size_t name_len = 0, val_len = 0;
        const lxb_char_t *name = lxb_dom_attr_qualified_name(attr, &name_len);
        const lxb_char_t *val = lxb_dom_attr_value_noi(attr, &val_len);
        JSValue item = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, item, "name",
            JS_NewStringLen(ctx, (const char *)name, name_len));
        JS_SetPropertyStr(ctx, item, "value",
            val ? JS_NewStringLen(ctx, (const char *)val, val_len)
                : JS_NewString(ctx, ""));
        JS_SetPropertyUint32(ctx, arr, idx++, item);
        attr = lxb_dom_element_next_attribute_noi(attr);
    }
    return arr;
}

/* --- ownerDocument --- */

static JSValue js_node_get_ownerDocument(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue doc = JS_GetPropertyStr(ctx, global, "document");
    JS_FreeValue(ctx, global);
    return doc;
}

/* ============================================================
 * Document API
 * ============================================================ */

static JSValue js_doc_getElementById(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv) {
    (void)this_val;
    if (!current_doc || argc < 1) return JS_NULL;
    const char *id = JS_ToCString(ctx, argv[0]);
    if (!id) return JS_NULL;

    /* Use querySelector("#id") which works for both parsed and dynamic elements.
     * lxb_dom_element_by_id uses the element's attr_id shortcut pointer which is
     * only set during HTML parsing, not by setAttribute. */
    char sel[256];
    snprintf(sel, sizeof(sel), "[id=\"%s\"]", id);
    lxb_dom_node_t *root = lxb_dom_document_root((lxb_dom_document_t *)current_doc);
    lxb_dom_node_t *found = glktwin_query_selector(current_doc, root, sel);
    JS_FreeCString(ctx, id);
    return found ? wrap_node(ctx, found) : JS_NULL;
}

static JSValue js_doc_createElement(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
    (void)this_val;
    lxb_dom_document_t *doc = (lxb_dom_document_t *)current_doc;
    if (!doc || argc < 1) return JS_NULL;
    const char *tag = JS_ToCString(ctx, argv[0]);
    if (!tag) return JS_NULL;
    lxb_dom_element_t *el = lxb_dom_document_create_element(doc,
        (const lxb_char_t *)tag, strlen(tag), NULL);
    JS_FreeCString(ctx, tag);
    return el ? wrap_node(ctx, lxb_dom_interface_node(el)) : JS_NULL;
}

static JSValue js_doc_createTextNode(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv) {
    (void)this_val;
    lxb_dom_document_t *doc = (lxb_dom_document_t *)current_doc;
    if (!doc || argc < 1) return JS_NULL;
    const char *text = JS_ToCString(ctx, argv[0]);
    if (!text) return JS_NULL;
    lxb_dom_text_t *tn = lxb_dom_document_create_text_node(doc,
        (const lxb_char_t *)text, strlen(text));
    JS_FreeCString(ctx, text);
    return tn ? wrap_node(ctx, lxb_dom_interface_node(tn)) : JS_NULL;
}

static JSValue js_doc_createDocumentFragment(JSContext *ctx, JSValueConst this_val,
                                             int argc, JSValueConst *argv) {
    (void)this_val; (void)argc; (void)argv;
    lxb_dom_node_t *frag = glktwin_create_document_fragment(current_doc);
    return wrap_node(ctx, frag);
}

static JSValue js_doc_createEvent(JSContext *ctx, JSValueConst this_val,
                                  int argc, JSValueConst *argv) {
    (void)this_val; (void)argc; (void)argv;
    JSValue evt = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, evt, "type", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, evt, "bubbles", JS_TRUE);
    JS_SetPropertyStr(ctx, evt, "cancelable", JS_TRUE);
    JS_SetPropertyStr(ctx, evt, "defaultPrevented", JS_FALSE);
    JS_SetPropertyStr(ctx, evt, "initEvent",
        JS_NewCFunction(ctx, NULL, "initEvent", 0));
    return evt;
}

static JSValue js_doc_querySelector(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
    (void)this_val;
    if (!current_doc || argc < 1) return JS_NULL;
    const char *sel = JS_ToCString(ctx, argv[0]);
    if (!sel) return JS_NULL;
    lxb_dom_node_t *root = lxb_dom_document_root((lxb_dom_document_t *)current_doc);
    lxb_dom_node_t *found = glktwin_query_selector(current_doc, root, sel);
    JS_FreeCString(ctx, sel);
    return wrap_node(ctx, found);
}

static JSValue js_doc_querySelectorAll(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv) {
    (void)this_val;
    if (!current_doc || argc < 1) return JS_NewArray(ctx);
    const char *sel = JS_ToCString(ctx, argv[0]);
    if (!sel) return JS_NewArray(ctx);
    lxb_dom_node_t *root = lxb_dom_document_root((lxb_dom_document_t *)current_doc);
    size_t count = 0;
    lxb_dom_node_t **results = glktwin_query_selector_all(current_doc, root, sel, &count);
    JS_FreeCString(ctx, sel);
    JSValue arr = JS_NewArray(ctx);
    for (size_t i = 0; i < count; i++)
        JS_SetPropertyUint32(ctx, arr, (uint32_t)i, wrap_node(ctx, results[i]));
    free(results);
    return arr;
}

static JSValue js_doc_getElementsByTagName(JSContext *ctx, JSValueConst this_val,
                                           int argc, JSValueConst *argv) {
    (void)this_val;
    if (!current_doc || argc < 1) return JS_NewArray(ctx);
    const char *tag = JS_ToCString(ctx, argv[0]);
    if (!tag) return JS_NewArray(ctx);
    char sel[128];
    snprintf(sel, sizeof(sel), "%s", tag);
    lxb_dom_node_t *root = lxb_dom_document_root((lxb_dom_document_t *)current_doc);
    size_t count = 0;
    lxb_dom_node_t **results = glktwin_query_selector_all(current_doc, root, sel, &count);
    JS_FreeCString(ctx, tag);
    JSValue arr = JS_NewArray(ctx);
    for (size_t i = 0; i < count; i++)
        JS_SetPropertyUint32(ctx, arr, (uint32_t)i, wrap_node(ctx, results[i]));
    free(results);
    return arr;
}

static JSValue js_doc_getElementsByClassName(JSContext *ctx, JSValueConst this_val,
                                              int argc, JSValueConst *argv) {
    (void)this_val;
    if (!current_doc || argc < 1) return JS_NewArray(ctx);
    const char *cls = JS_ToCString(ctx, argv[0]);
    if (!cls) return JS_NewArray(ctx);
    char sel[256];
    snprintf(sel, sizeof(sel), ".%s", cls);
    lxb_dom_node_t *root = lxb_dom_document_root((lxb_dom_document_t *)current_doc);
    size_t count = 0;
    lxb_dom_node_t **results = glktwin_query_selector_all(current_doc, root, sel, &count);
    JS_FreeCString(ctx, cls);
    JSValue arr = JS_NewArray(ctx);
    for (size_t i = 0; i < count; i++)
        JS_SetPropertyUint32(ctx, arr, (uint32_t)i, wrap_node(ctx, results[i]));
    free(results);
    return arr;
}

static JSValue js_doc_get_body(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    lxb_html_document_t *doc = (lxb_html_document_t *)current_doc;
    if (!doc) return JS_NULL;
    lxb_dom_element_t *body = lxb_html_document_body_element_noi(doc);
    return body ? wrap_node(ctx, lxb_dom_interface_node(body)) : JS_NULL;
}

static JSValue js_doc_get_head(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    lxb_html_document_t *doc = (lxb_html_document_t *)current_doc;
    if (!doc) return JS_NULL;
    lxb_dom_element_t *head = lxb_html_document_head_element_noi(doc);
    return head ? wrap_node(ctx, lxb_dom_interface_node(head)) : JS_NULL;
}

static JSValue js_doc_get_documentElement(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    if (!current_doc) return JS_NULL;
    lxb_dom_node_t *root = lxb_dom_document_root((lxb_dom_document_t *)current_doc);
    return wrap_node(ctx, root);
}

/* Event constructor: new Event(type, opts) */
static JSValue js_Event_constructor(JSContext *ctx, JSValueConst new_target,
                                    int argc, JSValueConst *argv) {
    (void)new_target;
    JSValue evt = JS_NewObject(ctx);
    if (argc > 0) {
        JS_SetPropertyStr(ctx, evt, "type", JS_DupValue(ctx, argv[0]));
    } else {
        JS_SetPropertyStr(ctx, evt, "type", JS_NewString(ctx, ""));
    }
    JS_SetPropertyStr(ctx, evt, "bubbles", JS_FALSE);
    JS_SetPropertyStr(ctx, evt, "cancelable", JS_FALSE);
    JS_SetPropertyStr(ctx, evt, "defaultPrevented", JS_FALSE);
    if (argc > 1 && JS_IsObject(argv[1])) {
        JSValue b = JS_GetPropertyStr(ctx, argv[1], "bubbles");
        if (!JS_IsUndefined(b))
            JS_SetPropertyStr(ctx, evt, "bubbles", b);
        else JS_FreeValue(ctx, b);
        JSValue c = JS_GetPropertyStr(ctx, argv[1], "cancelable");
        if (!JS_IsUndefined(c))
            JS_SetPropertyStr(ctx, evt, "cancelable", c);
        else JS_FreeValue(ctx, c);
        JSValue d = JS_GetPropertyStr(ctx, argv[1], "detail");
        if (!JS_IsUndefined(d))
            JS_SetPropertyStr(ctx, evt, "detail", d);
        else JS_FreeValue(ctx, d);
    }
    return evt;
}

/* CustomEvent constructor */
static JSValue js_CustomEvent_constructor(JSContext *ctx, JSValueConst new_target,
                                          int argc, JSValueConst *argv) {
    return js_Event_constructor(ctx, new_target, argc, argv);
}

/* MutationObserver stub */
static JSValue js_MutationObserver_constructor(JSContext *ctx, JSValueConst new_target,
                                               int argc, JSValueConst *argv) {
    (void)new_target;
    JSValue mo = JS_NewObject(ctx);
    if (argc > 0)
        JS_SetPropertyStr(ctx, mo, "_cb", JS_DupValue(ctx, argv[0]));
    JS_SetPropertyStr(ctx, mo, "observe",
        JS_NewCFunction(ctx, (JSCFunction *)NULL, "observe", 0));
    JS_SetPropertyStr(ctx, mo, "disconnect",
        JS_NewCFunction(ctx, (JSCFunction *)NULL, "disconnect", 0));
    return mo;
}

/* --- createHTMLDocument: returns a new minimal HTML document --- */

static JSValue js_impl_createHTMLDocument(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv) {
    (void)this_val;
    /* Create a real new HTML document via Lexbor */
    const char *title = "";
    if (argc > 0) {
        const char *t = JS_ToCString(ctx, argv[0]);
        if (t) { title = t; JS_FreeCString(ctx, t); }
    }
    /* Parse a minimal HTML document */
    const char *html = "<html><head></head><body></body></html>";
    void *new_doc = glktwin_lexbor_parse_html(html, strlen(html));
    if (!new_doc) return JS_NULL;

    /* Create a document-like object with body, head, etc. */
    JSValue doc_obj = JS_NewObject(ctx);

    lxb_html_document_t *hdoc = (lxb_html_document_t *)new_doc;
    lxb_dom_element_t *body = lxb_html_document_body_element_noi(hdoc);
    lxb_dom_element_t *head = lxb_html_document_head_element_noi(hdoc);

    if (body) JS_SetPropertyStr(ctx, doc_obj, "body",
                  wrap_node(ctx, lxb_dom_interface_node(body)));
    if (head) JS_SetPropertyStr(ctx, doc_obj, "head",
                  wrap_node(ctx, lxb_dom_interface_node(head)));

    lxb_dom_node_t *root = lxb_dom_document_root((lxb_dom_document_t *)new_doc);
    if (root) JS_SetPropertyStr(ctx, doc_obj, "documentElement",
                  wrap_node(ctx, root));

    JS_SetPropertyStr(ctx, doc_obj, "createElement",
        JS_NewCFunction(ctx, js_doc_createElement, "createElement", 1));
    JS_SetPropertyStr(ctx, doc_obj, "createTextNode",
        JS_NewCFunction(ctx, js_doc_createTextNode, "createTextNode", 1));
    JS_SetPropertyStr(ctx, doc_obj, "createDocumentFragment",
        JS_NewCFunction(ctx, js_doc_createDocumentFragment, "createDocumentFragment", 0));
    JS_SetPropertyStr(ctx, doc_obj, "querySelector",
        JS_NewCFunction(ctx, js_doc_querySelector, "querySelector", 1));
    JS_SetPropertyStr(ctx, doc_obj, "querySelectorAll",
        JS_NewCFunction(ctx, js_doc_querySelectorAll, "querySelectorAll", 1));
    JS_SetPropertyStr(ctx, doc_obj, "getElementById",
        JS_NewCFunction(ctx, js_doc_getElementById, "getElementById", 1));
    JS_SetPropertyStr(ctx, doc_obj, "nodeType", JS_NewInt32(ctx, 9));

    return doc_obj;
}

/* ============================================================
 * Initialization
 * ============================================================ */

void dom_bindings_init(JSContext *ctx) {
    cached_ctx = ctx;
    JS_NewClassID(&node_class_id);
    JSClassDef def = { "Node", .finalizer = node_finalizer };
    JS_NewClass(JS_GetRuntime(ctx), node_class_id, &def);

    /* Create Node prototype and register it as the class proto */
    node_proto = JS_NewObject(ctx);
    JS_SetClassProto(ctx, node_class_id, JS_DupValue(ctx, node_proto));

    /* Methods */
    JS_SetPropertyStr(ctx, node_proto, "appendChild",
        JS_NewCFunction(ctx, js_node_appendChild, "appendChild", 1));
    JS_SetPropertyStr(ctx, node_proto, "removeChild",
        JS_NewCFunction(ctx, js_node_removeChild, "removeChild", 1));
    JS_SetPropertyStr(ctx, node_proto, "insertBefore",
        JS_NewCFunction(ctx, js_node_insertBefore, "insertBefore", 2));
    JS_SetPropertyStr(ctx, node_proto, "replaceChild",
        JS_NewCFunction(ctx, js_node_replaceChild, "replaceChild", 2));
    JS_SetPropertyStr(ctx, node_proto, "cloneNode",
        JS_NewCFunction(ctx, js_node_cloneNode, "cloneNode", 1));
    JS_SetPropertyStr(ctx, node_proto, "contains",
        JS_NewCFunction(ctx, js_node_contains, "contains", 1));
    JS_SetPropertyStr(ctx, node_proto, "hasChildNodes",
        JS_NewCFunction(ctx, js_node_hasChildNodes, "hasChildNodes", 0));
    JS_SetPropertyStr(ctx, node_proto, "compareDocumentPosition",
        JS_NewCFunction(ctx, js_node_compareDocumentPosition,
                        "compareDocumentPosition", 1));
    JS_SetPropertyStr(ctx, node_proto, "getRootNode",
        JS_NewCFunction(ctx, js_node_getRootNode, "getRootNode", 0));
    JS_SetPropertyStr(ctx, node_proto, "remove",
        JS_NewCFunction(ctx, js_node_remove_self, "remove", 0));
    JS_SetPropertyStr(ctx, node_proto, "append",
        JS_NewCFunction(ctx, js_node_append, "append", 1));
    JS_SetPropertyStr(ctx, node_proto, "prepend",
        JS_NewCFunction(ctx, js_node_prepend, "prepend", 1));

    JS_SetPropertyStr(ctx, node_proto, "getAttribute",
        JS_NewCFunction(ctx, js_node_getAttribute, "getAttribute", 1));
    JS_SetPropertyStr(ctx, node_proto, "setAttribute",
        JS_NewCFunction(ctx, js_node_setAttribute, "setAttribute", 2));
    JS_SetPropertyStr(ctx, node_proto, "removeAttribute",
        JS_NewCFunction(ctx, js_node_removeAttribute, "removeAttribute", 1));
    JS_SetPropertyStr(ctx, node_proto, "hasAttribute",
        JS_NewCFunction(ctx, js_node_hasAttribute, "hasAttribute", 1));

    JS_SetPropertyStr(ctx, node_proto, "querySelector",
        JS_NewCFunction(ctx, js_node_querySelector, "querySelector", 1));
    JS_SetPropertyStr(ctx, node_proto, "querySelectorAll",
        JS_NewCFunction(ctx, js_node_querySelectorAll, "querySelectorAll", 1));
    JS_SetPropertyStr(ctx, node_proto, "matches",
        JS_NewCFunction(ctx, js_node_matches, "matches", 1));
    JS_SetPropertyStr(ctx, node_proto, "closest",
        JS_NewCFunction(ctx, js_node_closest, "closest", 1));

    JS_SetPropertyStr(ctx, node_proto, "addEventListener",
        JS_NewCFunction(ctx, js_node_addEventListener, "addEventListener", 3));
    JS_SetPropertyStr(ctx, node_proto, "removeEventListener",
        JS_NewCFunction(ctx, js_node_removeEventListener, "removeEventListener", 3));
    JS_SetPropertyStr(ctx, node_proto, "dispatchEvent",
        JS_NewCFunction(ctx, js_node_dispatchEvent, "dispatchEvent", 1));

    JS_SetPropertyStr(ctx, node_proto, "getBoundingClientRect",
        JS_NewCFunction(ctx, js_node_getBoundingClientRect, "getBoundingClientRect", 0));
    JS_SetPropertyStr(ctx, node_proto, "canPlayType",
        JS_NewCFunction(ctx, js_return_empty, "canPlayType", 1));
    JS_SetPropertyStr(ctx, node_proto, "play",
        JS_NewCFunction(ctx, js_return_empty, "play", 0));
    JS_SetPropertyStr(ctx, node_proto, "pause",
        JS_NewCFunction(ctx, js_return_empty, "pause", 0));
    JS_SetPropertyStr(ctx, node_proto, "load",
        JS_NewCFunction(ctx, js_return_empty, "load", 0));
    JS_SetPropertyStr(ctx, node_proto, "getContext",
        JS_NewCFunction(ctx, js_return_null, "getContext", 1));
    JS_SetPropertyStr(ctx, node_proto, "focus",
        JS_NewCFunction(ctx, js_return_empty, "focus", 0));
    JS_SetPropertyStr(ctx, node_proto, "blur",
        JS_NewCFunction(ctx, js_return_empty, "blur", 0));
    JS_SetPropertyStr(ctx, node_proto, "click",
        JS_NewCFunction(ctx, js_return_empty, "click", 0));
    JS_SetPropertyStr(ctx, node_proto, "scrollIntoView",
        JS_NewCFunction(ctx, js_return_empty, "scrollIntoView", 0));

    /* namespaceURI: HTML elements use the XHTML namespace */
    JS_SetPropertyStr(ctx, node_proto, "namespaceURI",
        JS_NewString(ctx, "http://www.w3.org/1999/xhtml"));

    /* Getters on prototype (property-like access via defineProperty) */
    JSValue global = JS_GetGlobalObject(ctx);

    /* Use defineProperty for getters (nodeType, tagName, etc.) */
    JSAtom atoms[] = {
        JS_NewAtom(ctx, "nodeType"),
        JS_NewAtom(ctx, "nodeName"),
        JS_NewAtom(ctx, "tagName"),
        JS_NewAtom(ctx, "parentNode"),
        JS_NewAtom(ctx, "parentElement"),
        JS_NewAtom(ctx, "firstChild"),
        JS_NewAtom(ctx, "lastChild"),
        JS_NewAtom(ctx, "nextSibling"),
        JS_NewAtom(ctx, "previousSibling"),
        JS_NewAtom(ctx, "nextElementSibling"),
        JS_NewAtom(ctx, "previousElementSibling"),
        JS_NewAtom(ctx, "childNodes"),
        JS_NewAtom(ctx, "children"),
        JS_NewAtom(ctx, "firstElementChild"),
        JS_NewAtom(ctx, "ownerDocument"),
        JS_NewAtom(ctx, "classList"),
        JS_NewAtom(ctx, "style"),
        JS_NewAtom(ctx, "isConnected"),
        JS_NewAtom(ctx, "attributes"),
    };
    JSCFunction *getters[] = {
        (JSCFunction *)js_node_get_nodeType,
        (JSCFunction *)js_node_get_nodeName,
        (JSCFunction *)js_node_get_tagName,
        (JSCFunction *)js_node_get_parentNode,
        (JSCFunction *)js_node_get_parentElement,
        (JSCFunction *)js_node_get_firstChild,
        (JSCFunction *)js_node_get_lastChild,
        (JSCFunction *)js_node_get_nextSibling,
        (JSCFunction *)js_node_get_previousSibling,
        (JSCFunction *)js_node_get_nextElementSibling,
        (JSCFunction *)js_node_get_previousElementSibling,
        (JSCFunction *)js_node_get_childNodes,
        (JSCFunction *)js_node_get_children,
        (JSCFunction *)js_node_get_firstElementChild,
        (JSCFunction *)js_node_get_ownerDocument,
        (JSCFunction *)js_node_get_classList,
        (JSCFunction *)js_node_get_style,
        (JSCFunction *)js_node_isConnected,
        (JSCFunction *)js_node_get_attributes,
    };
    int num_getters = sizeof(atoms) / sizeof(atoms[0]);
    for (int i = 0; i < num_getters; i++) {
        JSValue getter = JS_NewCFunction2(ctx, getters[i], NULL, 0,
                                          JS_CFUNC_getter, 0);
        JS_DefinePropertyGetSet(ctx, node_proto, atoms[i],
                                getter, JS_UNDEFINED,
                                JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, atoms[i]);
    }

    /* nodeValue: getter + setter */
    {
        JSAtom a = JS_NewAtom(ctx, "nodeValue");
        JSValue getter = JS_NewCFunction2(ctx, (JSCFunction *)js_node_get_nodeValue,
                                          NULL, 0, JS_CFUNC_getter, 0);
        JSValue setter = JS_NewCFunction2(ctx, (JSCFunction *)js_node_set_nodeValue,
                                          NULL, 1, JS_CFUNC_setter, 0);
        JS_DefinePropertyGetSet(ctx, node_proto, a, getter, setter,
                                JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, a);
    }
    /* textContent: getter + setter */
    {
        JSAtom a = JS_NewAtom(ctx, "textContent");
        JSValue getter = JS_NewCFunction2(ctx, (JSCFunction *)js_node_get_textContent,
                                          NULL, 0, JS_CFUNC_getter, 0);
        JSValue setter = JS_NewCFunction2(ctx, (JSCFunction *)js_node_set_textContent,
                                          NULL, 1, JS_CFUNC_setter, 0);
        JS_DefinePropertyGetSet(ctx, node_proto, a, getter, setter,
                                JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, a);
    }
    /* innerHTML: getter + setter */
    {
        JSAtom a = JS_NewAtom(ctx, "innerHTML");
        JSValue getter = JS_NewCFunction2(ctx, (JSCFunction *)js_node_get_innerHTML,
                                          NULL, 0, JS_CFUNC_getter, 0);
        JSValue setter = JS_NewCFunction2(ctx, (JSCFunction *)js_node_set_innerHTML,
                                          NULL, 1, JS_CFUNC_setter, 0);
        JS_DefinePropertyGetSet(ctx, node_proto, a, getter, setter,
                                JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, a);
    }
    /* outerHTML: getter */
    {
        JSAtom a = JS_NewAtom(ctx, "outerHTML");
        JSValue getter = JS_NewCFunction2(ctx, (JSCFunction *)js_node_get_outerHTML,
                                          NULL, 0, JS_CFUNC_getter, 0);
        JS_DefinePropertyGetSet(ctx, node_proto, a, getter, JS_UNDEFINED,
                                JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, a);
    }
    /* id: getter + setter */
    {
        JSAtom a = JS_NewAtom(ctx, "id");
        JSValue getter = JS_NewCFunction2(ctx, (JSCFunction *)js_node_get_id,
                                          NULL, 0, JS_CFUNC_getter, 0);
        JSValue setter = JS_NewCFunction2(ctx, (JSCFunction *)js_node_set_id,
                                          NULL, 1, JS_CFUNC_setter, 0);
        JS_DefinePropertyGetSet(ctx, node_proto, a, getter, setter,
                                JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, a);
    }
    /* className: getter + setter */
    {
        JSAtom a = JS_NewAtom(ctx, "className");
        JSValue getter = JS_NewCFunction2(ctx, (JSCFunction *)js_node_get_className,
                                          NULL, 0, JS_CFUNC_getter, 0);
        JSValue setter = JS_NewCFunction2(ctx, (JSCFunction *)js_node_set_className,
                                          NULL, 1, JS_CFUNC_setter, 0);
        JS_DefinePropertyGetSet(ctx, node_proto, a, getter, setter,
                                JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, a);
    }

    /* ---- document object ---- */
    JSValue document = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, document, "getElementById",
        JS_NewCFunction(ctx, js_doc_getElementById, "getElementById", 1));
    JS_SetPropertyStr(ctx, document, "createElement",
        JS_NewCFunction(ctx, js_doc_createElement, "createElement", 1));
    JS_SetPropertyStr(ctx, document, "createTextNode",
        JS_NewCFunction(ctx, js_doc_createTextNode, "createTextNode", 1));
    JS_SetPropertyStr(ctx, document, "createDocumentFragment",
        JS_NewCFunction(ctx, js_doc_createDocumentFragment, "createDocumentFragment", 0));
    JS_SetPropertyStr(ctx, document, "createEvent",
        JS_NewCFunction(ctx, js_doc_createEvent, "createEvent", 1));
    JS_SetPropertyStr(ctx, document, "querySelector",
        JS_NewCFunction(ctx, js_doc_querySelector, "querySelector", 1));
    JS_SetPropertyStr(ctx, document, "querySelectorAll",
        JS_NewCFunction(ctx, js_doc_querySelectorAll, "querySelectorAll", 1));
    JS_SetPropertyStr(ctx, document, "getElementsByTagName",
        JS_NewCFunction(ctx, js_doc_getElementsByTagName, "getElementsByTagName", 1));
    JS_SetPropertyStr(ctx, document, "getElementsByClassName",
        JS_NewCFunction(ctx, js_doc_getElementsByClassName, "getElementsByClassName", 1));

    /* addEventListener/removeEventListener on document */
    JS_SetPropertyStr(ctx, document, "addEventListener",
        JS_NewCFunction(ctx, js_node_addEventListener, "addEventListener", 3));
    JS_SetPropertyStr(ctx, document, "removeEventListener",
        JS_NewCFunction(ctx, js_node_removeEventListener, "removeEventListener", 3));
    JS_SetPropertyStr(ctx, document, "dispatchEvent",
        JS_NewCFunction(ctx, js_node_dispatchEvent, "dispatchEvent", 1));

    /* document property getters */
    {
        JSAtom a = JS_NewAtom(ctx, "body");
        JSValue getter = JS_NewCFunction2(ctx, (JSCFunction *)js_doc_get_body,
                                          NULL, 0, JS_CFUNC_getter, 0);
        JS_DefinePropertyGetSet(ctx, document, a, getter, JS_UNDEFINED,
                                JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, a);
    }
    {
        JSAtom a = JS_NewAtom(ctx, "head");
        JSValue getter = JS_NewCFunction2(ctx, (JSCFunction *)js_doc_get_head,
                                          NULL, 0, JS_CFUNC_getter, 0);
        JS_DefinePropertyGetSet(ctx, document, a, getter, JS_UNDEFINED,
                                JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, a);
    }
    {
        JSAtom a = JS_NewAtom(ctx, "documentElement");
        JSValue getter = JS_NewCFunction2(ctx, (JSCFunction *)js_doc_get_documentElement,
                                          NULL, 0, JS_CFUNC_getter, 0);
        JS_DefinePropertyGetSet(ctx, document, a, getter, JS_UNDEFINED,
                                JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(ctx, a);
    }

    JS_SetPropertyStr(ctx, document, "readyState", JS_NewString(ctx, "complete"));
    JS_SetPropertyStr(ctx, document, "title", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, document, "nodeType", JS_NewInt32(ctx, 9));
    JS_SetPropertyStr(ctx, document, "nodeName", JS_NewString(ctx, "#document"));
    JS_SetPropertyStr(ctx, document, "compatMode", JS_NewString(ctx, "CSS1Compat"));
    JS_SetPropertyStr(ctx, document, "contentType", JS_NewString(ctx, "text/html"));
    JS_SetPropertyStr(ctx, document, "characterSet", JS_NewString(ctx, "UTF-8"));
    JS_SetPropertyStr(ctx, document, "charset", JS_NewString(ctx, "UTF-8"));

    /* Cache the document JS object for wrap_node to return when
     * wrapping the Lexbor document node (ensures jQuery event handlers
     * attached to $(document) are found during event bubbling). */
    js_document_obj = JS_DupValue(ctx, document);

    JS_SetPropertyStr(ctx, global, "document", document);

    /* Also set on window */
    JSValue window = JS_GetPropertyStr(ctx, global, "window");
    if (!JS_IsUndefined(window)) {
        JS_SetPropertyStr(ctx, window, "document", JS_DupValue(ctx, document));
        JS_FreeValue(ctx, window);
    }

    /* Event / CustomEvent / MutationObserver constructors */
    JS_SetPropertyStr(ctx, global, "Event",
        JS_NewCFunction2(ctx, js_Event_constructor, "Event", 1,
                         JS_CFUNC_constructor, 0));
    JS_SetPropertyStr(ctx, global, "CustomEvent",
        JS_NewCFunction2(ctx, js_CustomEvent_constructor, "CustomEvent", 2,
                         JS_CFUNC_constructor, 0));
    JS_SetPropertyStr(ctx, global, "MutationObserver",
        JS_NewCFunction2(ctx, js_MutationObserver_constructor, "MutationObserver", 1,
                         JS_CFUNC_constructor, 0));

    /* DOMTokenList with proper prototype (classList polyfill checks prototype.toggle) */
    {
        JSValue dtl_proto = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, dtl_proto, "add",
            JS_NewCFunction(ctx, js_classList_add, "add", 1));
        JS_SetPropertyStr(ctx, dtl_proto, "remove",
            JS_NewCFunction(ctx, js_classList_remove, "remove", 1));
        JS_SetPropertyStr(ctx, dtl_proto, "contains",
            JS_NewCFunction(ctx, js_classList_contains, "contains", 1));
        JS_SetPropertyStr(ctx, dtl_proto, "toggle",
            JS_NewCFunction(ctx, js_classList_toggle, "toggle", 1));
        JS_SetPropertyStr(ctx, dtl_proto, "item",
            JS_NewCFunction(ctx, (JSCFunction *)js_Event_constructor, "item", 1));
        JSValue dtl_ctor = JS_NewCFunction2(ctx,
            (JSCFunction *)js_Event_constructor, "DOMTokenList", 0,
            JS_CFUNC_constructor, 0);
        JS_SetPropertyStr(ctx, dtl_ctor, "prototype", dtl_proto);
        JS_SetPropertyStr(ctx, global, "DOMTokenList", dtl_ctor);
    }

    /* Node constructor: must be a real constructor with .prototype = node_proto
     * so that (wrappedNode instanceof Node) works. */
    {
        JSValue nodeCtor = JS_NewCFunction2(ctx,
            (JSCFunction *)js_Event_constructor, "Node", 0,
            JS_CFUNC_constructor, 0);
        JS_SetPropertyStr(ctx, nodeCtor, "prototype",
            JS_DupValue(ctx, node_proto));
        JS_SetPropertyStr(ctx, nodeCtor, "ELEMENT_NODE", JS_NewInt32(ctx, 1));
        JS_SetPropertyStr(ctx, nodeCtor, "TEXT_NODE", JS_NewInt32(ctx, 3));
        JS_SetPropertyStr(ctx, nodeCtor, "COMMENT_NODE", JS_NewInt32(ctx, 8));
        JS_SetPropertyStr(ctx, nodeCtor, "DOCUMENT_NODE", JS_NewInt32(ctx, 9));
        JS_SetPropertyStr(ctx, nodeCtor, "DOCUMENT_FRAGMENT_NODE",
            JS_NewInt32(ctx, 11));
        JS_SetPropertyStr(ctx, global, "Node", nodeCtor);
    }

    /* DOM subtype constructors: prototype inherits from node_proto so
     * (node instanceof Element), (node instanceof HTMLElement) etc. work. */
    {
        static JSCFunction *noop_ctor = (JSCFunction *)js_Event_constructor;
        const char *dom_ctors[] = {
            "Element", "HTMLElement", "HTMLDivElement", "HTMLSpanElement",
            "HTMLAnchorElement", "HTMLButtonElement", "HTMLFormElement",
            "HTMLInputElement", "HTMLSelectElement", "HTMLTextAreaElement",
            "HTMLScriptElement", "HTMLStyleElement", "HTMLLinkElement",
            "HTMLImageElement", "HTMLCanvasElement", "HTMLVideoElement",
            "HTMLAudioElement", "HTMLTemplateElement",
            "HTMLDocument", "DocumentFragment", "Text", "Comment",
            NULL
        };
        for (int i = 0; dom_ctors[i]; i++) {
            JSValue ctor = JS_NewCFunction2(ctx, noop_ctor, dom_ctors[i], 0,
                                             JS_CFUNC_constructor, 0);
            JS_SetPropertyStr(ctx, ctor, "prototype",
                JS_DupValue(ctx, node_proto));
            JS_SetPropertyStr(ctx, global, dom_ctors[i], ctor);
        }
    }

    /* Non-DOM constructors with independent prototypes */
    {
        static JSCFunction *noop_ctor = (JSCFunction *)js_Event_constructor;
        const char *other_ctors[] = {
            "DOMParser", "DOMException",
            "HTMLCollection", "NodeList",
            "CSSStyleDeclaration", "CSSStyleSheet", "StyleSheet",
            "Range", "Selection", "TreeWalker", "NodeIterator",
            "DOMRect", "DOMRectReadOnly",
            "IntersectionObserver", "ResizeObserver",
            "PointerEvent", "MouseEvent", "KeyboardEvent",
            "FocusEvent", "InputEvent", "UIEvent", "WheelEvent",
            "TouchEvent", "AnimationEvent", "TransitionEvent",
            "ClipboardEvent", "DragEvent", "PopStateEvent",
            "HashChangeEvent", "ErrorEvent", "ProgressEvent",
            "StorageEvent",
            NULL
        };
        for (int i = 0; other_ctors[i]; i++) {
            JSValue ctor = JS_NewCFunction2(ctx, noop_ctor, other_ctors[i], 0,
                                             JS_CFUNC_constructor, 0);
            JS_SetPropertyStr(ctx, ctor, "prototype", JS_NewObject(ctx));
            JS_SetPropertyStr(ctx, global, other_ctors[i], ctor);
        }
    }

    /* document.implementation with createHTMLDocument that returns a doc-like object */
    JSValue impl = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, impl, "createHTMLDocument",
        JS_NewCFunction(ctx, (JSCFunction *)js_impl_createHTMLDocument,
                        "createHTMLDocument", 1));
    JS_SetPropertyStr(ctx, document, "implementation", impl);

    /* Override the C 'style' getter with a JS Proxy-based implementation
     * that syncs property writes back to the element's style attribute. */
    {
        static const char style_proxy_js[] =
            "(function(){"
            "var P=Node.prototype;"
            "Object.defineProperty(P,'style',{configurable:true,get:function(){"
            "  if(this._$s)return this._$s;"
            "  var self=this;"
            "  var t={};"
            "  var sa=self.getAttribute('style');"
            "  if(sa){"
            "    sa.split(';').forEach(function(p){"
            "      var kv=p.split(':');"
            "      if(kv.length>=2){"
            "        var k=kv[0].trim().replace(/-([a-z])/g,function(_,c){return c.toUpperCase()});"
            "        t[k]=kv.slice(1).join(':').trim();"
            "      }"
            "    });"
            "  }"
            "  function sync(){"
            "    var parts=[];"
            "    for(var k in t){"
            "      if(typeof t[k]==='string'&&t[k]!==''){"
            "        var dk=k.replace(/[A-Z]/g,function(m){return '-'+m.toLowerCase()});"
            "        parts.push(dk+':'+t[k]);"
            "      }"
            "    }"
            "    try{self.setAttribute('style',parts.join(';'));}catch(e){}"
            "  }"
            "  this._$s=new Proxy(t,{"
            "    set:function(o,p,v){o[p]=v;sync();return true;},"
            "    get:function(o,p){"
            "      if(p==='cssText')return self.getAttribute('style')||'';"
            "      if(p==='setProperty')return function(n,v){"
            "        var ck=n.replace(/-([a-z])/g,function(_,c){return c.toUpperCase()});"
            "        o[ck]=v;sync();};"
            "      if(p==='removeProperty')return function(n){"
            "        var ck=n.replace(/-([a-z])/g,function(_,c){return c.toUpperCase()});"
            "        var old=o[ck]||'';delete o[ck];sync();return old;};"
            "      if(p==='getPropertyValue')return function(n){"
            "        var ck=n.replace(/-([a-z])/g,function(_,c){return c.toUpperCase()});"
            "        return o[ck]||'';};"
            "      if(p==='length')return Object.keys(o).length;"
            "      if(p==='item')return function(i){return Object.keys(o)[i]||null;};"
            "      return o[p];"
            "    },"
            "    deleteProperty:function(o,p){delete o[p];sync();return true;}"
            "  });"
            "  return this._$s;"
            "}});"
            "})();";
        JS_Eval(ctx, style_proxy_js, sizeof(style_proxy_js) - 1,
                "<style-proxy>", JS_EVAL_TYPE_GLOBAL);
    }

    JS_FreeValue(ctx, global);
}

void dom_bindings_shutdown(JSContext *ctx) {
    node_cache_clear();
    if (!JS_IsUndefined(js_document_obj)) {
        JS_FreeValue(ctx, js_document_obj);
        js_document_obj = JS_UNDEFINED;
    }
    if (!JS_IsUndefined(node_proto)) {
        JS_FreeValue(ctx, node_proto);
        node_proto = JS_UNDEFINED;
    }
}

void dom_bindings_set_document(JSContext *ctx, dom_document_t doc) {
    (void)ctx;
    current_doc = doc;
}

dom_document_t dom_bindings_get_document(JSContext *ctx) {
    (void)ctx;
    return current_doc;
}

void *dom_get_root(dom_document_t doc) {
    if (!doc) return NULL;
    return lxb_dom_document_root((lxb_dom_document_t *)doc);
}

void *dom_bindings_node_opaque(JSContext *ctx, JSValueConst node_val) {
    return JS_GetOpaque2(ctx, node_val, node_class_id);
}

JSValue dom_bindings_wrap_node(JSContext *ctx, void *node) {
    return wrap_node(ctx, (lxb_dom_node_t *)node);
}
