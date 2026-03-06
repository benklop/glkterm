/* sugarcube.c: SugarCube format adapter.
 * Detects SugarCube stories by looking for the format name in <tw-storydata>.
 * The actual SugarCube runtime JS is embedded in the story HTML <script> tags
 * and executed by the story loader; this adapter handles detection and any
 * additional TUI-specific setup.
 */

#include "format_adapter.h"
#include "../lexbor_api.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static bool sugarcube_detect(void *doc_opaque) {
    if (!doc_opaque) return false;

    /* Look for SugarCube format identifier in the HTML */
    lxb_dom_node_t *root = lxb_dom_document_root((lxb_dom_document_t *)doc_opaque);
    if (!root) return false;

    /* Check <tw-storydata format="SugarCube"> */
    lxb_dom_node_t *sd = glktwin_query_selector(doc_opaque, root, "tw-storydata");
    if (sd) {
        size_t fmt_len = 0;
        const lxb_char_t *fmt = lxb_dom_element_get_attribute(
            lxb_dom_interface_element(sd),
            (const lxb_char_t *)"format", 6, &fmt_len);
        if (fmt && fmt_len >= 9) {
            char buf[64];
            if (fmt_len >= sizeof(buf)) fmt_len = sizeof(buf) - 1;
            memcpy(buf, fmt, fmt_len);
            buf[fmt_len] = '\0';
            /* Case-insensitive prefix match */
            for (size_t i = 0; i < fmt_len; i++)
                if (buf[i] >= 'A' && buf[i] <= 'Z') buf[i] += 32;
            if (strstr(buf, "sugarcube"))
                return true;
        }
    }

    /* Fallback: look for SugarCube's characteristic script id */
    lxb_dom_node_t *sc = glktwin_query_selector(doc_opaque, root,
        "script[id=\"script-sugarcube\"]");
    if (sc) return true;

    /* Fallback: search for "SugarCube" text in any script tag */
    size_t count = 0;
    lxb_dom_node_t **scripts = glktwin_query_selector_all(doc_opaque, root,
        "script", &count);
    if (scripts) {
        for (size_t i = 0; i < count; i++) {
            size_t tlen = 0;
            char *text = glktwin_get_text_content(scripts[i], &tlen);
            if (text) {
                if (strstr(text, "SugarCube")) {
                    free(text);
                    free(scripts);
                    return true;
                }
                free(text);
            }
        }
        free(scripts);
    }

    return false;
}

static void sugarcube_init(JSContext *ctx) {
    (void)ctx;
}

static void sugarcube_start(JSContext *ctx, void *doc_opaque) {
    (void)ctx;
    (void)doc_opaque;
}

static void sugarcube_navigate(JSContext *ctx, const char *passage_id) {
    (void)ctx;
    (void)passage_id;
}

static FormatAdapter sugarcube_adapter = {
    "sugarcube",
    sugarcube_detect,
    sugarcube_init,
    sugarcube_start,
    sugarcube_navigate,
};

void format_register_sugarcube(void) {
    format_register(&sugarcube_adapter);
}
