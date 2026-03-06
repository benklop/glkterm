/* lexbor_include.c: Single translation unit for Lexbor amalgamation.
 * All Lexbor-internal access goes through wrapper functions defined here.
 */
#include "lexbor_single.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int validate_tree(lxb_dom_node_t *node, lxb_dom_node_t *expected_parent, int depth);

/* ---- Parse / destroy ---- */

void *glktwin_lexbor_parse_html(const char *html, size_t len) {
    lxb_html_document_t *doc = lxb_html_document_create();
    if (!doc) return NULL;
    lxb_status_t status = lxb_html_document_parse(doc, (const lxb_char_t *)html, len);
    if (status != LXB_STATUS_OK) {
        lxb_html_document_clean(doc);
        lxb_html_document_destroy(doc);
        return NULL;
    }
    return doc;
}

void glktwin_lexbor_document_destroy(void *doc) {
    if (doc) {
        lxb_html_document_clean((lxb_html_document_t *)doc);
        lxb_html_document_destroy((lxb_html_document_t *)doc);
    }
}

/* ---- Single text node content ---- */

size_t glktwin_lexbor_text_content(lxb_dom_node_t *node, char *buf, size_t cap) {
    if (!node || lxb_dom_node_type_noi(node) != LXB_DOM_NODE_TYPE_TEXT || !buf || cap == 0)
        return 0;
    lxb_dom_character_data_t *ch = lxb_dom_interface_character_data(node);
    size_t len = ch->data.length;
    if (len >= cap) len = cap - 1;
    if (len > 0 && ch->data.data)
        memcpy(buf, ch->data.data, len);
    buf[len] = '\0';
    return len;
}

/* Get the character data pointer and length for a text/comment node.
 * Returns the raw data pointer (not a copy), or NULL if not a character data node. */
const char *glktwin_text_node_data(lxb_dom_node_t *node, size_t *out_len) {
    if (!node) { if (out_len) *out_len = 0; return NULL; }
    lxb_dom_node_type_t t = lxb_dom_node_type_noi(node);
    if (t != LXB_DOM_NODE_TYPE_TEXT && t != LXB_DOM_NODE_TYPE_COMMENT &&
        t != LXB_DOM_NODE_TYPE_CDATA_SECTION) {
        if (out_len) *out_len = 0;
        return NULL;
    }
    lxb_dom_character_data_t *ch = lxb_dom_interface_character_data(node);
    if (out_len) *out_len = ch->data.length;
    return (const char *)(ch->data.data ? ch->data.data : (const lxb_char_t *)"");
}

/* Set the character data for a text/comment node. */
bool glktwin_text_node_set_data(lxb_dom_node_t *node, const char *str, size_t len) {
    if (!node) return false;
    lxb_dom_node_type_t t = lxb_dom_node_type_noi(node);
    if (t != LXB_DOM_NODE_TYPE_TEXT && t != LXB_DOM_NODE_TYPE_COMMENT &&
        t != LXB_DOM_NODE_TYPE_CDATA_SECTION)
        return false;
    lxb_status_t status = lxb_dom_character_data_replace(
        lxb_dom_interface_character_data(node),
        (const lxb_char_t *)str, len, 0, 0);
    return status == LXB_STATUS_OK;
}

/* ---- Recursive text content ---- */

char *glktwin_get_text_content(lxb_dom_node_t *node, size_t *out_len) {
    if (!node) { if (out_len) *out_len = 0; return NULL; }
    size_t len = 0;
    lxb_char_t *text = lxb_dom_node_text_content(node, &len);
    if (!text) { if (out_len) *out_len = 0; return NULL; }
    char *result = malloc(len + 1);
    if (result) {
        memcpy(result, text, len);
        result[len] = '\0';
    }
    if (out_len) *out_len = len;
    return result;
}

/* ---- Tag name ---- */

const char *glktwin_get_tag_name(lxb_dom_node_t *node) {
    static char buf[64];
    if (!node || lxb_dom_node_type_noi(node) != LXB_DOM_NODE_TYPE_ELEMENT) {
        buf[0] = '\0';
        return buf;
    }
    lxb_dom_element_t *el = lxb_dom_interface_element(node);
    size_t len = 0;
    const lxb_char_t *name = lxb_dom_element_qualified_name(el, &len);
    if (!name || len == 0) { buf[0] = '\0'; return buf; }
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    for (size_t i = 0; i < len; i++)
        buf[i] = (char)((name[i] >= 'a' && name[i] <= 'z') ? name[i] - 32 : name[i]);
    buf[len] = '\0';
    return buf;
}

/* ---- innerHTML get (serialize children as HTML) ---- */

static lxb_status_t serialize_cb(const lxb_char_t *data, size_t len, void *ctx) {
    typedef struct { char *buf; size_t len; size_t cap; } SerBuf;
    SerBuf *sb = (SerBuf *)ctx;
    if (sb->len + len >= sb->cap) {
        size_t new_cap = (sb->cap + len) * 2;
        char *p = realloc(sb->buf, new_cap);
        if (!p) return LXB_STATUS_ERROR;
        sb->buf = p;
        sb->cap = new_cap;
    }
    memcpy(sb->buf + sb->len, data, len);
    sb->len += len;
    return LXB_STATUS_OK;
}

char *glktwin_serialize_inner_html(lxb_dom_node_t *node, size_t *out_len) {
    if (!node) { if (out_len) *out_len = 0; return NULL; }
    struct { char *buf; size_t len; size_t cap; } sb = { malloc(1024), 0, 1024 };
    if (!sb.buf) { if (out_len) *out_len = 0; return NULL; }

    for (lxb_dom_node_t *child = lxb_dom_node_first_child_noi(node);
         child; child = lxb_dom_node_next_noi(child)) {
        lxb_html_serialize_tree_cb(child, serialize_cb, &sb);
    }

    sb.buf[sb.len] = '\0';
    if (out_len) *out_len = sb.len;
    return sb.buf;
}

/* ---- innerHTML set (parse fragment + replace children) ---- */

bool glktwin_set_inner_html(lxb_dom_element_t *element, void *doc,
                            const char *html, size_t len) {
    if (!element || !doc) return false;
    lxb_html_document_t *hdoc = (lxb_html_document_t *)doc;

    /* Remove all existing children */
    lxb_dom_node_t *node = lxb_dom_interface_node(element);
    while (lxb_dom_node_first_child_noi(node)) {
        lxb_dom_node_t *child = lxb_dom_node_first_child_noi(node);
        lxb_dom_node_remove(child);
    }

    if (!html || len == 0) return true;

    lxb_dom_node_t *root = lxb_dom_document_root(&hdoc->dom_document);

    /* Parse as fragment */
    lxb_dom_node_t *frag = lxb_html_document_parse_fragment(
        hdoc, element, (const lxb_char_t *)html, len);
    if (!frag) return false;

    /* Move fragment children into element */
    while (lxb_dom_node_first_child_noi(frag)) {
        lxb_dom_node_t *child = lxb_dom_node_first_child_noi(frag);
        lxb_dom_node_remove(child);
        lxb_dom_node_insert_child(node, child);
    }

    return true;
}

/* ---- querySelector ---- */

struct qs_ctx {
    lxb_dom_node_t **results;
    size_t count;
    size_t cap;
    bool first_only;
};

static lxb_status_t qs_cb(lxb_dom_node_t *node,
                           lxb_css_selector_specificity_t spec, void *ctx) {
    (void)spec;
    struct qs_ctx *qc = (struct qs_ctx *)ctx;
    if (qc->first_only && qc->count > 0)
        return LXB_STATUS_OK;
    if (qc->count >= qc->cap) {
        size_t new_cap = qc->cap ? qc->cap * 2 : 16;
        lxb_dom_node_t **p = realloc(qc->results, new_cap * sizeof(*p));
        if (!p) return LXB_STATUS_ERROR;
        qc->results = p;
        qc->cap = new_cap;
    }
    qc->results[qc->count++] = node;
    return LXB_STATUS_OK;
}

static const char *node_tag_name(lxb_dom_node_t *n) {
    if (!n) return "NULL";
    if (n->type == LXB_DOM_NODE_TYPE_ELEMENT) {
        size_t len;
        const lxb_char_t *name = lxb_dom_element_local_name(lxb_dom_interface_element(n), &len);
        return name ? (const char *)name : "?elem";
    }
    if (n->type == LXB_DOM_NODE_TYPE_TEXT) return "#text";
    if (n->type == LXB_DOM_NODE_TYPE_DOCUMENT) return "#document";
    if (n->type == LXB_DOM_NODE_TYPE_DOCUMENT_FRAGMENT) return "#fragment";
    if (n->type == LXB_DOM_NODE_TYPE_COMMENT) return "#comment";
    return "?";
}

static int validate_tree(lxb_dom_node_t *node, lxb_dom_node_t *expected_parent, int depth) {
    if (!node) return 0;
    if (depth > 500) {
        fprintf(stderr, "[TREE] depth > 500, likely cycle\n");
        return -1;
    }
    if (node->parent != expected_parent) {
        fprintf(stderr, "[TREE] node %p <%s> parent mismatch: expected %p <%s>, got %p <%s>\n",
                (void*)node, node_tag_name(node),
                (void*)expected_parent, node_tag_name(expected_parent),
                (void*)node->parent, node_tag_name(node->parent));
        return -1;
    }
    lxb_dom_node_t *prev = NULL;
    int child_idx = 0;
    for (lxb_dom_node_t *c = node->first_child; c; c = c->next, child_idx++) {
        if (c->prev != prev) {
            fprintf(stderr, "[TREE] child #%d %p <%s> of <%s> prev mismatch: expected %p <%s>, got %p <%s>\n",
                    child_idx, (void*)c, node_tag_name(c),
                    node_tag_name(node),
                    (void*)prev, node_tag_name(prev),
                    (void*)c->prev, node_tag_name(c->prev));
            return -1;
        }
        if (validate_tree(c, node, depth + 1) < 0) return -1;
        prev = c;
    }
    if (node->last_child != prev) {
        fprintf(stderr, "[TREE] node %p <%s> last_child mismatch: expected %p <%s>, got %p <%s>\n",
                (void*)node, node_tag_name(node),
                (void*)prev, node_tag_name(prev),
                (void*)node->last_child, node_tag_name(node->last_child));
        return -1;
    }
    return 0;
}

static bool run_selector(void *doc, lxb_dom_node_t *root,
                          const char *selector, struct qs_ctx *qc) {
    lxb_html_document_t *hdoc = (lxb_html_document_t *)doc;
    if (!hdoc || !root || !selector) return false;

    lxb_css_parser_t *parser = lxb_css_parser_create();
    if (!parser) return false;
    if (lxb_css_parser_init(parser, NULL) != LXB_STATUS_OK) {
        lxb_css_parser_destroy(parser, true);
        return false;
    }

    lxb_css_selector_list_t *list = lxb_css_selectors_parse(
        parser, (const lxb_char_t *)selector, strlen(selector));
    if (!list) {
        lxb_css_parser_destroy(parser, true);
        return false;
    }

    lxb_selectors_t *selectors = lxb_selectors_create();
    if (!selectors) {
        lxb_css_parser_destroy(parser, true);
        return false;
    }
    lxb_selectors_init(selectors);

    if (validate_tree(root, root->parent, 0) < 0) {
        fprintf(stderr, "[TREE] Corrupt tree detected before selector: %s\n", selector);
    }

    lxb_selectors_find(selectors, root, list, qs_cb, qc);

    lxb_selectors_destroy(selectors, true);
    lxb_css_parser_destroy(parser, true);
    return true;
}

lxb_dom_node_t *glktwin_query_selector(void *doc, lxb_dom_node_t *root,
                                        const char *selector) {
    struct qs_ctx qc = { NULL, 0, 0, true };
    if (!run_selector(doc, root, selector, &qc) || qc.count == 0) {
        free(qc.results);
        return NULL;
    }
    lxb_dom_node_t *result = qc.results[0];
    free(qc.results);
    return result;
}

lxb_dom_node_t **glktwin_query_selector_all(void *doc, lxb_dom_node_t *root,
                                             const char *selector,
                                             size_t *out_count) {
    struct qs_ctx qc = { NULL, 0, 0, false };
    if (!run_selector(doc, root, selector, &qc)) {
        if (out_count) *out_count = 0;
        return NULL;
    }
    if (out_count) *out_count = qc.count;
    return qc.results;
}

/* ---- Element matches selector ---- */

bool glktwin_element_matches(void *doc, lxb_dom_node_t *node,
                              const char *selector) {
    if (!node || !selector || !doc) return false;
    lxb_dom_node_t *parent = lxb_dom_node_parent_noi(node);
    if (!parent) parent = node;

    struct qs_ctx qc = { NULL, 0, 0, false };
    run_selector(doc, parent, selector, &qc);
    bool found = false;
    for (size_t i = 0; i < qc.count; i++) {
        if (qc.results[i] == node) { found = true; break; }
    }
    free(qc.results);
    return found;
}

/* ---- Owner document ---- */

void *glktwin_node_owner_document(lxb_dom_node_t *node) {
    if (!node) return NULL;
    return node->owner_document;
}

/* ---- Clone node ---- */

static void clone_children(lxb_dom_document_t *doc, lxb_dom_node_t *dst, lxb_dom_node_t *src);

static lxb_dom_node_t *clone_single(lxb_dom_document_t *doc, lxb_dom_node_t *node) {
    if (!node) return NULL;
    lxb_dom_node_type_t type = lxb_dom_node_type_noi(node);

    if (type == LXB_DOM_NODE_TYPE_ELEMENT) {
        lxb_dom_element_t *el = lxb_dom_interface_element(node);
        size_t name_len = 0;
        const lxb_char_t *name = lxb_dom_element_qualified_name(el, &name_len);
        if (!name) return NULL;
        lxb_dom_element_t *clone = lxb_dom_document_create_element(doc, name, name_len, NULL);
        if (!clone) return NULL;

        /* Copy attributes */
        lxb_dom_attr_t *attr = el->first_attr;
        while (attr) {
            size_t aname_len = 0;
            const lxb_char_t *aname = lxb_dom_attr_local_name_noi(attr, &aname_len);
            if (aname) {
                size_t aval_len = 0;
                const lxb_char_t *aval = lxb_dom_element_get_attribute(el, aname, aname_len, &aval_len);
                if (aval)
                    lxb_dom_element_set_attribute(clone, aname, aname_len, aval, aval_len);
            }
            attr = attr->next;
        }
        return lxb_dom_interface_node(clone);
    }
    if (type == LXB_DOM_NODE_TYPE_TEXT) {
        lxb_dom_character_data_t *ch = lxb_dom_interface_character_data(node);
        lxb_dom_text_t *txt = lxb_dom_document_create_text_node(
            doc, ch->data.data, ch->data.length);
        return txt ? lxb_dom_interface_node(txt) : NULL;
    }
    return NULL;
}

static void clone_children(lxb_dom_document_t *doc, lxb_dom_node_t *dst, lxb_dom_node_t *src) {
    for (lxb_dom_node_t *child = lxb_dom_node_first_child_noi(src);
         child; child = lxb_dom_node_next_noi(child)) {
        lxb_dom_node_t *cc = clone_single(doc, child);
        if (cc) {
            lxb_dom_node_insert_child(dst, cc);
            clone_children(doc, cc, child);
        }
    }
}

lxb_dom_node_t *glktwin_create_document_fragment(void *doc) {
    if (!doc) return NULL;
    lxb_dom_document_fragment_t *frag =
        lxb_dom_document_create_document_fragment((lxb_dom_document_t *)doc);
    return frag ? lxb_dom_interface_node(frag) : NULL;
}

lxb_dom_node_t *glktwin_clone_node(lxb_dom_node_t *node, bool deep) {
    if (!node || !node->owner_document) return NULL;
    lxb_dom_node_t *clone = clone_single(node->owner_document, node);
    if (clone && deep)
        clone_children(node->owner_document, clone, node);
    return clone;
}

int glktwin_validate_tree(void *doc) {
    if (!doc) return 0;
    lxb_html_document_t *hdoc = (lxb_html_document_t *)doc;
    lxb_dom_node_t *root = lxb_dom_document_root(&hdoc->dom_document);
    if (!root) return 0;
    return validate_tree(root, root->parent, 0);
}
