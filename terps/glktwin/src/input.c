/* input.c: Main event loop — line input for link selection, timer dispatch. */

#include "glk.h"
#include "glkterm.h"
#include "story_loader.h"
#include "dom_bindings.h"
#include "render.h"
#include "jsengine.h"
#include "lexbor_api.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void navigate_link(JSContext *ctx, void *node) {
    if (!ctx || !node) return;

    lxb_dom_node_t *n = (lxb_dom_node_t *)node;
    if (lxb_dom_node_type_noi(n) != LXB_DOM_NODE_TYPE_ELEMENT) return;

    /* Trigger click via jQuery so that SugarCube link setters
     * (e.g. [[$input to "message-"]]) execute properly. */
    JSValue jsNode = dom_bindings_wrap_node(ctx, node);
    if (!JS_IsNull(jsNode)) {
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue jQuery = JS_GetPropertyStr(ctx, global, "jQuery");
        if (JS_IsFunction(ctx, jQuery)) {
            JSValue $el = JS_Call(ctx, jQuery, JS_UNDEFINED, 1, &jsNode);
            if (!JS_IsException($el)) {
                JSValue triggerFn = JS_GetPropertyStr(ctx, $el, "trigger");
                if (JS_IsFunction(ctx, triggerFn)) {
                    JSValue clickStr = JS_NewString(ctx, "click");
                    JS_Call(ctx, triggerFn, $el, 1, &clickStr);
                    JS_FreeValue(ctx, clickStr);
                }
                JS_FreeValue(ctx, triggerFn);
            }
            JS_FreeValue(ctx, $el);
        }
        JS_FreeValue(ctx, jQuery);
        JS_FreeValue(ctx, global);
    }
    JS_FreeValue(ctx, jsNode);
}

static void submit_text_input(const char *text) {
    char js[2048];
    /* Escape single quotes in user input */
    char escaped[512];
    int j = 0;
    for (int i = 0; text[i] && j < (int)sizeof(escaped) - 2; i++) {
        if (text[i] == '\'' || text[i] == '\\') {
            escaped[j++] = '\\';
        }
        escaped[j++] = text[i];
    }
    escaped[j] = '\0';

    snprintf(js, sizeof(js),
        "try {"
        "  var $inp = jQuery('input.macro-textbox');"
        "  if ($inp.length) {"
        "    var el = $inp[0];"
        "    el.value = '%s';"
        "    $inp.val('%s');"
        "    $inp.trigger('change');"
        "    var evt = jQuery.Event('keypress', {which: 13, keyCode: 13});"
        "    $inp.trigger(evt);"
        "  } else {"
        "    console.warn('No textbox input found');"
        "  }"
        "} catch(e) { console.error('textinput: ' + e.message + '\\n' + e.stack); }",
        escaped, escaped);
    jsengine_eval(glktwin_js_engine(), js, strlen(js), "<textinput>");
}

/* Snapshot the visible text length for change detection. */
static size_t passage_snapshot(void *doc) {
    if (!doc) return 0;
    lxb_dom_node_t *root = lxb_dom_document_root((lxb_dom_document_t *)doc);
    lxb_dom_node_t *passages = glktwin_query_selector(doc, root, "#passages");
    if (!passages) return 0;
    size_t len = 0;
    char *html = glktwin_serialize_inner_html(passages, &len);
    free(html);
    return len;
}

static void show_prompt(void) {
    if (render_link_count() > 0 && render_input_count() > 0)
        glk_put_string("\n(Enter a number or type your answer)\n> ");
    else
        glk_put_string("\n> ");
}

static void do_render(winid_t win, void *doc, int *input_pending,
                      char *linebuf, size_t linebuf_sz) {
    if (*input_pending) {
        event_t cev;
        glk_cancel_line_event(win, &cev);
        *input_pending = 0;
    }
    glk_window_clear(win);
    render_passage(doc);
    if (render_link_count() > 0 || render_input_count() > 0) {
        show_prompt();
        glk_request_line_event(win, linebuf, (glui32)(linebuf_sz - 1), 0);
        *input_pending = 1;
    }
    /* Reflow the text and mark all output as "seen" so that glkterm's
       built-in pager does not intercept the next keypress. Without this,
       content taller than the terminal triggers "more"-style paging and
       the user must press a key to scroll before line input works. */
    gli_windows_update();
    gli_windows_set_paging(TRUE);
}

void glktwin_main_loop(void) {
    event_t ev;
    winid_t mainwin = glktwin_main_window();
    char linebuf[256];
    int input_pending = 0;
    size_t last_snapshot = 0;

    if (!mainwin) return;

    glk_request_timer_events(100);

    last_snapshot = passage_snapshot(glktwin_current_document());

    if (render_link_count() > 0 || render_input_count() > 0) {
        show_prompt();
        glk_request_line_event(mainwin, linebuf, sizeof(linebuf) - 1, 0);
        input_pending = 1;
    }

    while (1) {
        glk_select(&ev);

        if (ev.type == evtype_LineInput && ev.win == mainwin) {
            input_pending = 0;
            linebuf[ev.val1] = '\0';

            char *p = linebuf;
            while (*p == ' ' || *p == '\t') p++;

            if (*p == 'q' || *p == 'Q') break;

            int handled = 0;

            /* Empty Enter with a single link and no text input:
             * auto-select the only available choice. */
            if (*p == '\0' && render_link_count() == 1 &&
                render_input_count() == 0) {
                void *node = render_link_node(1);
                if (node) {
                    JSContext *ctx = jsengine_context(glktwin_js_engine());
                    navigate_link(ctx, node);
                    handled = 1;
                }
            }

            if (!handled && render_link_count() > 0) {
                int choice = atoi(p);
                if (choice >= 1 && choice <= render_link_count()) {
                    void *node = render_link_node(choice);
                    if (node) {
                        JSContext *ctx = jsengine_context(glktwin_js_engine());
                        navigate_link(ctx, node);
                        handled = 1;
                    }
                }
            }

            if (!handled && render_input_count() > 0 && *p) {
                submit_text_input(p);
                handled = 1;
            }

            if (handled) {
                jsengine_timer_advance(glktwin_js_engine(), 50);
                jsengine_raf_flush(glktwin_js_engine());

                glk_window_clear(mainwin);
                if (glktwin_current_document())
                    render_passage(glktwin_current_document());

                last_snapshot = passage_snapshot(glktwin_current_document());

                if (render_link_count() > 0 || render_input_count() > 0) {
                    show_prompt();
                    glk_request_line_event(mainwin, linebuf,
                                           sizeof(linebuf) - 1, 0);
                    input_pending = 1;
                }
                gli_windows_update();
                gli_windows_set_paging(TRUE);
            } else {
                if (*p) {
                    if (render_link_count() > 0) {
                        glk_put_string("Enter a link number (1-");
                        char nbuf[16];
                        snprintf(nbuf, sizeof(nbuf), "%d",
                                 render_link_count());
                        glk_put_string(nbuf);
                        glk_put_string(") or 'q' to quit.\n");
                    } else if (render_input_count() > 0) {
                        glk_put_string("Type your answer or 'q' to quit.\n");
                    }
                }
                /* Always re-request input so the user isn't left
                 * stuck at a dead prompt after an empty Enter. */
                if (render_link_count() > 0 || render_input_count() > 0) {
                    show_prompt();
                    glk_request_line_event(mainwin, linebuf,
                                           sizeof(linebuf) - 1, 0);
                    input_pending = 1;
                }
            }
        }

        if (ev.type == evtype_Timer && glktwin_js_engine()) {
            jsengine_timer_tick(glktwin_js_engine());
            jsengine_raf_flush(glktwin_js_engine());

            if (glktwin_current_document()) {
                size_t snap = passage_snapshot(glktwin_current_document());
                if (snap != last_snapshot) {
                    last_snapshot = snap;
                    do_render(mainwin, glktwin_current_document(),
                              &input_pending, linebuf, sizeof(linebuf));
                    last_snapshot = passage_snapshot(
                        glktwin_current_document());
                }
            }
        }

        if (ev.type == evtype_Arrange && glktwin_current_document()) {
            do_render(mainwin, glktwin_current_document(),
                      &input_pending, linebuf, sizeof(linebuf));
            last_snapshot = passage_snapshot(glktwin_current_document());
        }
    }
}
