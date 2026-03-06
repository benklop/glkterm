/* render.c: Walk active passage DOM, output via Glk with Unicode support.
 * Simulates web bold/emphasis using Glk styles: normal text = grey, bold = white. */

#include "render.h"
#include "dom_bindings.h"
#include "lexbor_api.h"
#include "glk.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <ctype.h>

#define MAX_LINKS 256
#define MAX_INPUTS 32

static int link_counter;
static lxb_dom_node_t *link_nodes[MAX_LINKS];
static int total_links;   /* pre-counted; used to suppress numbering for single link */

static int input_counter;
static lxb_dom_node_t *input_nodes[MAX_INPUTS];

int render_link_count(void) { return link_counter; }

void *render_link_node(int link_num) {
    if (link_num < 1 || link_num > link_counter) return NULL;
    return link_nodes[link_num - 1];
}

int render_input_count(void) { return input_counter; }

void *render_input_node(int input_num) {
    if (input_num < 1 || input_num > input_counter) return NULL;
    return input_nodes[input_num - 1];
}

/* Convert a UTF-8 string to an array of glui32 codepoints.
 * Returns the number of codepoints written (not including the NUL terminator).
 * out must have room for at least out_cap elements. */
static size_t utf8_to_uni(const char *utf8, size_t byte_len,
                          glui32 *out, size_t out_cap) {
    size_t i = 0, n = 0;
    const unsigned char *s = (const unsigned char *)utf8;
    while (i < byte_len && n < out_cap - 1) {
        unsigned char c = s[i];
        glui32 cp;
        if (c < 0x80) {
            cp = c; i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 >= byte_len) break;
            cp = ((glui32)(c & 0x1F) << 6) | (s[i+1] & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= byte_len) break;
            cp = ((glui32)(c & 0x0F) << 12) | ((glui32)(s[i+1] & 0x3F) << 6) |
                 (s[i+2] & 0x3F);
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= byte_len) break;
            cp = ((glui32)(c & 0x07) << 18) | ((glui32)(s[i+1] & 0x3F) << 12) |
                 ((glui32)(s[i+2] & 0x3F) << 6) | (s[i+3] & 0x3F);
            i += 4;
        } else {
            cp = 0xFFFD; i += 1; /* replacement character */
        }
        out[n++] = cp;
    }
    out[n] = 0;
    return n;
}

static void put_utf8(const char *text, size_t len) {
    if (!text || len == 0) return;
    glui32 uni[4096];
    size_t n = utf8_to_uni(text, len, uni, sizeof(uni) / sizeof(uni[0]));
    if (n > 0)
        glk_put_buffer_uni(uni, (glui32)n);
}

static void put_str(const char *s) {
    if (!s) return;
    put_utf8(s, strlen(s));
}

static bool should_skip(lxb_dom_node_t *node) {
    if (lxb_dom_node_type_noi(node) != LXB_DOM_NODE_TYPE_ELEMENT)
        return false;

    const char *tag = glktwin_get_tag_name(node);
    if (!tag) return false;

    if (strcmp(tag, "SCRIPT") == 0 || strcmp(tag, "STYLE") == 0 ||
        strcmp(tag, "NOSCRIPT") == 0 || strcmp(tag, "TEMPLATE") == 0)
        return true;

    lxb_dom_element_t *el = lxb_dom_interface_element(node);
    size_t len = 0;
    const lxb_char_t *hidden = lxb_dom_element_get_attribute(
        el, (const lxb_char_t *)"hidden", 6, &len);
    if (hidden) return true;

    const lxb_char_t *style = lxb_dom_element_get_attribute(
        el, (const lxb_char_t *)"style", 5, &len);
    if (style && len > 0) {
        const char *s = (const char *)style;
        if (strstr(s, "display:none") || strstr(s, "display: none"))
            return true;
    }

    return false;
}

static bool is_block_element(const char *tag) {
    return (strcmp(tag, "DIV") == 0 || strcmp(tag, "P") == 0 ||
            strcmp(tag, "H1") == 0 || strcmp(tag, "H2") == 0 ||
            strcmp(tag, "H3") == 0 || strcmp(tag, "H4") == 0 ||
            strcmp(tag, "BR") == 0 || strcmp(tag, "LI") == 0 ||
            strcmp(tag, "UL") == 0 || strcmp(tag, "OL") == 0 ||
            strcmp(tag, "BLOCKQUOTE") == 0 || strcmp(tag, "HR") == 0 ||
            strcmp(tag, "HEADER") == 0 || strcmp(tag, "FOOTER") == 0 ||
            strcmp(tag, "NAV") == 0 || strcmp(tag, "SECTION") == 0 ||
            strcmp(tag, "ARTICLE") == 0 || strcmp(tag, "PRE") == 0 ||
            strcmp(tag, "TABLE") == 0 || strcmp(tag, "TR") == 0);
}

/* True if element is <b>, <strong>, or has inline style font-weight: bold/700. */
static bool element_has_bold(lxb_dom_node_t *node) {
    const char *tag = glktwin_get_tag_name(node);
    if (tag && (strcmp(tag, "B") == 0 || strcmp(tag, "STRONG") == 0))
        return true;
    lxb_dom_element_t *el = lxb_dom_interface_element(node);
    size_t len = 0;
    const lxb_char_t *style = lxb_dom_element_get_attribute(
        el, (const lxb_char_t *)"style", 5, &len);
    if (!style || len == 0) return false;
    const char *s = (const char *)style;
    const char *fw = strstr(s, "font-weight");
    if (!fw) return false;
    /* Skip to value after colon. */
    const char *v = strchr(fw, ':');
    if (!v) return false;
    v++;
    while (*v == ' ' || *v == '\t') v++;
    if (strncasecmp(v, "bold", 4) == 0) return true;
    if (*v == '7' && v[1] == '0' && v[2] == '0' && (!v[3] || !isalnum((unsigned char)v[3]))) return true;
    return false;
}

typedef struct {
    int last_was_newline;
    int bold_depth;  /* > 0 when inside one or more bold elements */
} render_state_t;

static void walk_node(lxb_dom_node_t *node, render_state_t *st) {
    if (!node) return;

    if (lxb_dom_node_type_noi(node) == LXB_DOM_NODE_TYPE_TEXT) {
        char buf[4096];
        size_t n = glktwin_lexbor_text_content(node, buf, sizeof(buf));
        if (n > 0) {
            /* Collapse whitespace (tabs, multiple spaces, \r) like a browser.
             * Newlines are preserved to maintain <br>-like breaks. */
            size_t i, j = 0;
            int in_ws = st->last_was_newline;
            for (i = 0; i < n; i++) {
                if (buf[i] == '\t' || buf[i] == '\r' ||
                    (buf[i] == ' ' && in_ws)) {
                    continue;
                }
                if (buf[i] == ' ') {
                    in_ws = 1;
                } else {
                    in_ws = 0;
                }
                buf[j++] = buf[i];
            }
            if (j > 0) {
                glk_set_style(st->bold_depth > 0 ? style_Emphasized : style_Normal);
                put_utf8(buf, j);
                st->last_was_newline = (buf[j - 1] == '\n');
            }
        }
        return;
    }

    if (lxb_dom_node_type_noi(node) != LXB_DOM_NODE_TYPE_ELEMENT)
        return;

    if (should_skip(node))
        return;

    const char *tag = glktwin_get_tag_name(node);
    if (!tag) return;

    bool is_block = is_block_element(tag);
    bool is_link = (strcmp(tag, "A") == 0);
    bool bold = element_has_bold(node);
    if (bold) st->bold_depth++;

    if (is_block && !st->last_was_newline) {
        put_str("\n");
        st->last_was_newline = 1;
    }

    if (strcmp(tag, "HR") == 0) {
        put_str("--------------------\n");
        st->last_was_newline = 1;
        if (bold) st->bold_depth--;
        return;
    }

    if (strcmp(tag, "BR") == 0) {
        put_str("\n");
        st->last_was_newline = 1;
        if (bold) st->bold_depth--;
        return;
    }

    if (strcmp(tag, "INPUT") == 0) {
        if (input_counter < MAX_INPUTS)
            input_nodes[input_counter] = node;
        input_counter++;
        if (bold) st->bold_depth--;
        return;
    }

    if (is_link) {
        if (link_counter < MAX_LINKS)
            link_nodes[link_counter] = node;
        link_counter++;
        if (total_links > 1)
            put_str("[");
    }

    if (strcmp(tag, "LI") == 0) {
        put_str("  * ");
    }

    for (lxb_dom_node_t *child = lxb_dom_node_first_child_noi(node); child;
         child = lxb_dom_node_next_noi(child)) {
        walk_node(child, st);
    }

    if (bold) st->bold_depth--;

    if (is_link && total_links > 1) {
        char numbuf[32];
        snprintf(numbuf, sizeof(numbuf), " (%d)]", link_counter);
        put_str(numbuf);
    }

    if (is_block && !st->last_was_newline) {
        put_str("\n");
        st->last_was_newline = 1;
    }
}

/* Quick recursive count of visible <a> tags for numbering decisions. */
static void count_links(lxb_dom_node_t *node) {
    if (!node) return;
    if (lxb_dom_node_type_noi(node) != LXB_DOM_NODE_TYPE_ELEMENT) return;
    if (should_skip(node)) return;
    const char *tag = glktwin_get_tag_name(node);
    if (tag && strcmp(tag, "A") == 0) total_links++;
    for (lxb_dom_node_t *c = lxb_dom_node_first_child_noi(node); c;
         c = lxb_dom_node_next_noi(c))
        count_links(c);
}

void render_passage(dom_document_t doc) {
    if (!doc) return;

    link_counter = 0;
    input_counter = 0;
    total_links = 0;

    lxb_dom_node_t *root = lxb_dom_document_root((lxb_dom_document_t *)doc);
    lxb_dom_node_t *passages = glktwin_query_selector(doc, root, "#passages");

    lxb_dom_node_t *render_root = passages;
    if (!render_root) {
        lxb_dom_element_t *body = lxb_html_document_body_element_noi(
            (lxb_html_document_t *)doc);
        render_root = (lxb_dom_node_t *)body;
    }
    if (render_root) {
        count_links(render_root);
        render_state_t st = { .last_was_newline = 1, .bold_depth = 0 };
        walk_node(render_root, &st);
        if (!st.last_was_newline) put_str("\n");
    }
}
