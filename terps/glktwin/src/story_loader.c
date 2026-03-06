/* story_loader.c: Load Twine story HTML, parse with Lexbor, extract and
 * execute <script> tags, init JS/DOM, render.
 */

#include "glk.h"
#include "glkstart.h"
#include "story_loader.h"
#include "dom_bindings.h"
#include "dom_events.h"
#include "css_cascade.h"
#include "jsengine.h"
#include "render.h"
#include "lexbor_api.h"
#include "formats/format_adapter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SOURCEFILE 1

static winid_t mainwin;
static strid_t mainstr;
static void *current_doc;
static JSEngine *js_engine;

static char *read_file_into_buffer(strid_t str, size_t *out_len) {
    size_t cap = 65536, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    while (1) {
        glui32 n = glk_get_buffer_stream(str, buf + len, (glui32)(cap - len));
        if (n == 0) break;
        len += n;
        if (cap - len < 4096) {
            cap *= 2;
            char *p = realloc(buf, cap);
            if (!p) { free(buf); return NULL; }
            buf = p;
        }
    }
    *out_len = len;
    return buf;
}

/* Collect all <script> elements, extract their text content, execute in order. */
static void execute_scripts(JSEngine *engine, void *doc) {
    if (!engine || !doc) return;

    lxb_dom_node_t *root = lxb_dom_document_root((lxb_dom_document_t *)doc);
    if (!root) return;

    size_t count = 0;
    lxb_dom_node_t **scripts = glktwin_query_selector_all(doc, root, "script", &count);
    if (!scripts || count == 0) {
        free(scripts);
        return;
    }

    for (size_t i = 0; i < count; i++) {
        lxb_dom_node_t *script_node = scripts[i];

        /* Skip scripts with a src attribute (external scripts not supported) */
        if (lxb_dom_node_type_noi(script_node) != LXB_DOM_NODE_TYPE_ELEMENT)
            continue;
        size_t src_len = 0;
        const lxb_char_t *src = lxb_dom_element_get_attribute(
            lxb_dom_interface_element(script_node),
            (const lxb_char_t *)"src", 3, &src_len);
        if (src && src_len > 0)
            continue;

        /* Check type attribute: skip non-JS types, but allow tw-passagedata
         * script types used by some Twine formats */
        size_t type_len = 0;
        const lxb_char_t *type_attr = lxb_dom_element_get_attribute(
            lxb_dom_interface_element(script_node),
            (const lxb_char_t *)"type", 4, &type_len);
        if (type_attr && type_len > 0) {
            const char *t = (const char *)type_attr;
            /* Skip non-JS MIME types that aren't Twine data containers */
            if (strstr(t, "text/twine") || strstr(t, "tw-") ||
                strstr(t, "json") || strstr(t, "template"))
                continue;
        }

        /* Extract script text content */
        size_t text_len = 0;
        char *text = glktwin_get_text_content(script_node, &text_len);
        if (!text || text_len == 0) {
            free(text);
            continue;
        }

        /* Generate a filename for error reporting */
        size_t id_len = 0;
        const lxb_char_t *id_attr = lxb_dom_element_get_attribute(
            lxb_dom_interface_element(script_node),
            (const lxb_char_t *)"id", 2, &id_len);
        char name[128];
        if (id_attr && id_len > 0 && id_len < 100)
            snprintf(name, sizeof(name), "<script#%.*s>", (int)id_len, id_attr);
        else
            snprintf(name, sizeof(name), "<script[%zu]>", i);

        jsengine_eval(engine, text, text_len, name);
        free(text);
    }

    free(scripts);
}

/* Fire DOMContentLoaded and load events after scripts have run. */
static void fire_ready_events(JSEngine *engine) {
    if (!engine) return;
    static const char ready_js[] =
        "if(typeof document !== 'undefined' && document.dispatchEvent) {"
        "  try { document.dispatchEvent(new Event('DOMContentLoaded', {bubbles:true})); } catch(e) {}"
        "}"
        "if(typeof window !== 'undefined' && window.dispatchEvent) {"
        "  try { window.dispatchEvent(new Event('load')); } catch(e) {}"
        "}";
    jsengine_eval(engine, ready_js, sizeof(ready_js) - 1, "<ready>");
}

/* Grey and white for main buffer: normal text = grey, bold/emphasis = white
 * (simulates web appearance where body text is white with bold passages). */
#define GLKTWIN_COLOR_NORMAL_TEXT  0xA0A0A0  /* light grey */
#define GLKTWIN_COLOR_NORMAL_DIM  0x909090   /* slightly dimmer grey */
#define GLKTWIN_COLOR_EMPHASIS     0xFFFFFF  /* white */

static void glktwin_set_text_buffer_style_hints(void) {
    /* Body-like styles: grey text */
    glk_stylehint_set(wintype_TextBuffer, style_Normal, stylehint_TextColor,
                      GLKTWIN_COLOR_NORMAL_TEXT);
    glk_stylehint_set(wintype_TextBuffer, style_Preformatted, stylehint_TextColor,
                      GLKTWIN_COLOR_NORMAL_TEXT);
    glk_stylehint_set(wintype_TextBuffer, style_Note, stylehint_TextColor,
                      GLKTWIN_COLOR_NORMAL_DIM);
    glk_stylehint_set(wintype_TextBuffer, style_BlockQuote, stylehint_TextColor,
                      GLKTWIN_COLOR_NORMAL_TEXT);
    glk_stylehint_set(wintype_TextBuffer, style_User1, stylehint_TextColor,
                      GLKTWIN_COLOR_NORMAL_TEXT);
    glk_stylehint_set(wintype_TextBuffer, style_User2, stylehint_TextColor,
                      GLKTWIN_COLOR_NORMAL_TEXT);

    /* Bold/emphasis: white; use Weight=1 and Oblique=0 so it shows as bold, not italic */
    glk_stylehint_set(wintype_TextBuffer, style_Emphasized, stylehint_TextColor,
                      GLKTWIN_COLOR_EMPHASIS);
    glk_stylehint_set(wintype_TextBuffer, style_Emphasized, stylehint_Weight, 1);
    glk_stylehint_set(wintype_TextBuffer, style_Emphasized, stylehint_Oblique, 0);

    /* Headings / alert: white, bold */
    glk_stylehint_set(wintype_TextBuffer, style_Header, stylehint_TextColor,
                      GLKTWIN_COLOR_EMPHASIS);
    glk_stylehint_set(wintype_TextBuffer, style_Subheader, stylehint_TextColor,
                      GLKTWIN_COLOR_EMPHASIS);
    glk_stylehint_set(wintype_TextBuffer, style_Alert, stylehint_TextColor,
                      GLKTWIN_COLOR_EMPHASIS);

    /* Input line: white so typed text is visible */
    glk_stylehint_set(wintype_TextBuffer, style_Input, stylehint_TextColor,
                      GLKTWIN_COLOR_EMPHASIS);
}

int glktwin_story_load(const char *path) {
    glktwin_set_text_buffer_style_hints();
    mainwin = glk_window_open(0, 0, 0, wintype_TextBuffer, 0);
    if (!mainwin) return 0;
    mainstr = glk_window_get_stream(mainwin);
    glk_stream_set_current(mainstr);

    strid_t file = glkunix_stream_open_pathname((char *)path, 0, SOURCEFILE);
    if (!file) {
        glk_put_string("glktwin: Cannot open file.\n");
        return 0;
    }
    size_t html_len;
    char *html = read_file_into_buffer(file, &html_len);
    glk_stream_close(file, NULL);
    if (!html || html_len == 0) {
        glk_put_string("glktwin: Empty or unreadable file.\n");
        free(html);
        return 0;
    }

    current_doc = glktwin_lexbor_parse_html(html, html_len);
    free(html);
    if (!current_doc) {
        glk_put_string("glktwin: Failed to parse HTML.\n");
        return 0;
    }
    /* Create JS engine */
    js_engine = jsengine_create();
    if (!js_engine) {
        glktwin_lexbor_document_destroy(current_doc);
        current_doc = NULL;
        glk_put_string("glktwin: Out of memory (JS engine).\n");
        return 0;
    }

    /* Initialize DOM bindings, events, CSS cascade */
    JSContext *ctx = jsengine_context(js_engine);
    dom_bindings_set_document(ctx, current_doc);
    dom_bindings_init(ctx);
    dom_events_init(ctx);
    css_cascade_init(ctx);

    /* Load localStorage from previous session */
    jsengine_localStorage_load(js_engine, "saves.json");

    /* jQuery: SugarCube/Twine stories bundle their own jQuery in <script> tags.
     * Only load our vendored copy if the story doesn't include one.
     * We detect this after executing scripts. */

    /* Register format adapters and detect story format */
    format_register_adapters();

    /* Execute inline <script> tags from the HTML */
    execute_scripts(js_engine, current_doc);

    /* If jQuery wasn't loaded by any script, load our vendored copy */
    {
        static const char check[] = "typeof jQuery === 'undefined'";
        JSValue r = JS_Eval(ctx, check, sizeof(check) - 1, "<check>", JS_EVAL_TYPE_GLOBAL);
        if (JS_ToBool(ctx, r))
            jsengine_load_jquery(js_engine);
        JS_FreeValue(ctx, r);
    }

    /* Fire ready events */
    fire_ready_events(js_engine);

    /* Drain short-delay initialization timers (jQuery.ready fires
     * via setTimeout(fn, 0)). Advance 500ms to catch init callbacks
     * without triggering long-delay timers like <<timed 11.5s>>. */
    jsengine_timer_advance(js_engine, 500);
    jsengine_raf_flush(js_engine);

    /* Render current state */
    render_passage(current_doc);

    return 1;
}

winid_t glktwin_main_window(void) { return mainwin; }
strid_t glktwin_main_stream(void) { return mainstr; }

void *glktwin_current_document(void) { return current_doc; }
JSEngine *glktwin_js_engine(void) { return js_engine; }

void glktwin_shutdown(void) {
    if (js_engine) {
        jsengine_localStorage_save(js_engine, "saves.json");
        jsengine_destroy(js_engine);
        js_engine = NULL;
    }
    if (current_doc) {
        glktwin_lexbor_document_destroy(current_doc);
        current_doc = NULL;
    }
}
