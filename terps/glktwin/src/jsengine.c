/* jsengine.c: QuickJS engine layer — runtime, context, window/navigator/location stubs,
 * console, timers, localStorage, getComputedStyle.
 */

#include "jsengine.h"
#include "dom_bindings.h"
#include "dom_events.h"
#include "css_cascade.h"
#include "glk.h"
#include "glkstart.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern strid_t glkunix_stream_open_pathname_gen(char *pathname,
    glui32 writemode, glui32 textmode, glui32 rock);

extern const char glktwin_js_jquery[];
extern const size_t glktwin_js_jquery_len;

#define TIMER_ID_BASE 1000
#define LOCAL_STORAGE_MAX 256

typedef struct Timer {
    int id;
    uint32_t fire_at_ms;   /* absolute engine time to fire */
    uint32_t interval_ms;  /* repeat interval; 0 for one-shot */
    JSValue callback;
    bool is_interval;
} Timer;

typedef struct LocalStorageItem {
    char *key;
    char *value;
} LocalStorageItem;

struct JSEngine {
    JSRuntime *rt;
    JSContext *ctx;
    Timer *timers;
    int num_timers;
    int next_timer_id;
    uint32_t clock_ms;     /* monotonic tick counter */
    LocalStorageItem localStorage[LOCAL_STORAGE_MAX];
    int num_local_storage;
    JSValue raf_callbacks;
};

/* ---- Console ---- */

static JSValue js_console_log(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    (void)this_val;
    for (int i = 0; i < argc; i++) {
        const char *s = JS_ToCString(ctx, argv[i]);
        if (s) {
            glk_put_string((char *)s);
            glk_put_string(i < argc - 1 ? " " : "\n");
            JS_FreeCString(ctx, s);
        }
    }
    return JS_UNDEFINED;
}

static JSValue js_console_warn(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    (void)this_val;
    fprintf(stderr, "[JS console.warn] ");
    for (int i = 0; i < argc; i++) {
        const char *s = JS_ToCString(ctx, argv[i]);
        if (s) { fprintf(stderr, "%s ", s); JS_FreeCString(ctx, s); }
    }
    fprintf(stderr, "\n");
    return JS_UNDEFINED;
}

static JSValue js_console_error(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
    (void)this_val;
    fprintf(stderr, "[JS console.error] ");
    for (int i = 0; i < argc; i++) {
        const char *s = JS_ToCString(ctx, argv[i]);
        if (s) { fprintf(stderr, "%s ", s); JS_FreeCString(ctx, s); }
        /* If argument is an Error object, print its stack */
        if (JS_IsObject(argv[i])) {
            JSValue stack = JS_GetPropertyStr(ctx, argv[i], "stack");
            if (!JS_IsUndefined(stack) && !JS_IsNull(stack)) {
                const char *st = JS_ToCString(ctx, stack);
                if (st) { fprintf(stderr, "\n  Stack: %s", st); JS_FreeCString(ctx, st); }
            }
            JS_FreeValue(ctx, stack);
        }
    }
    fprintf(stderr, "\n");
    return JS_UNDEFINED;
}

/* ---- Window dialogs ---- */

static JSValue js_alert(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc > 0) {
        const char *s = JS_ToCString(ctx, argv[0]);
        if (s) { glk_put_string((char *)s); JS_FreeCString(ctx, s); }
    }
    glk_put_string("\n");
    return JS_UNDEFINED;
}

static JSValue js_confirm(JSContext *ctx, JSValueConst this_val,
                          int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc > 0) {
        const char *s = JS_ToCString(ctx, argv[0]);
        if (s) { glk_put_string((char *)s); JS_FreeCString(ctx, s); }
    }
    glk_put_string(" [y/N]: ");
    event_t ev;
    glk_request_char_event(glk_window_get_root());
    glk_select(&ev);
    if (ev.type == evtype_CharInput && (ev.val1 == 'y' || ev.val1 == 'Y'))
        return JS_TRUE;
    return JS_FALSE;
}

static JSValue js_prompt(JSContext *ctx, JSValueConst this_val,
                         int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc > 0) {
        const char *s = JS_ToCString(ctx, argv[0]);
        if (s) { glk_put_string((char *)s); JS_FreeCString(ctx, s); }
    }
    glk_put_string(" ");
    char buf[512];
    glk_request_line_event(glk_window_get_root(), buf, (glui32)(sizeof(buf) - 1), 0);
    event_t ev;
    glk_select(&ev);
    if (ev.type == evtype_LineInput) {
        buf[ev.val1] = '\0';
        return JS_NewString(ctx, buf);
    }
    return JS_NULL;
}

/* ---- Timers ---- */

static JSValue js_setTimeout(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv) {
    (void)this_val;
    JSEngine *e = JS_GetContextOpaque(ctx);
    if (!e || argc < 1) return JS_UNDEFINED;
    int delay_ms = 0;
    if (argc >= 2) JS_ToInt32(ctx, &delay_ms, argv[1]);
    if (delay_ms < 0) delay_ms = 0;
    int id = e->next_timer_id++;
    Timer *t = realloc(e->timers, (e->num_timers + 1) * sizeof(Timer));
    if (!t) return JS_UNDEFINED;
    e->timers = t;
    t = &e->timers[e->num_timers++];
    t->id = id;
    t->fire_at_ms = e->clock_ms + (uint32_t)delay_ms;
    t->interval_ms = 0;
    t->callback = JS_DupValue(ctx, argv[0]);
    t->is_interval = false;
    return JS_NewInt32(ctx, id);
}

static JSValue js_setInterval(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    (void)this_val;
    JSEngine *e = JS_GetContextOpaque(ctx);
    if (!e || argc < 1) return JS_UNDEFINED;
    int delay_ms = 0;
    if (argc >= 2) JS_ToInt32(ctx, &delay_ms, argv[1]);
    if (delay_ms < 1) delay_ms = 1;
    int id = e->next_timer_id++;
    Timer *t = realloc(e->timers, (e->num_timers + 1) * sizeof(Timer));
    if (!t) return JS_UNDEFINED;
    e->timers = t;
    t = &e->timers[e->num_timers++];
    t->id = id;
    t->fire_at_ms = e->clock_ms + (uint32_t)delay_ms;
    t->interval_ms = (uint32_t)delay_ms;
    t->callback = JS_DupValue(ctx, argv[0]);
    t->is_interval = true;
    return JS_NewInt32(ctx, id);
}

static JSValue js_clearTimeout(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    (void)this_val;
    JSEngine *e = JS_GetContextOpaque(ctx);
    if (!e || argc < 1) return JS_UNDEFINED;
    int id;
    JS_ToInt32(ctx, &id, argv[0]);
    for (int i = 0; i < e->num_timers; i++) {
        if (e->timers[i].id == id) {
            JS_FreeValue(ctx, e->timers[i].callback);
            e->timers[i] = e->timers[--e->num_timers];
            break;
        }
    }
    return JS_UNDEFINED;
}

static JSValue js_requestAnimationFrame(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv) {
    (void)this_val;
    JSEngine *e = JS_GetContextOpaque(ctx);
    if (!e || argc < 1) return JS_UNDEFINED;
    int id = e->next_timer_id++;
    JSValue arr = e->raf_callbacks;
    if (JS_IsUndefined(arr) || JS_IsNull(arr)) {
        arr = JS_NewArray(ctx);
        e->raf_callbacks = arr;
    }
    JSValue lenVal = JS_GetPropertyStr(ctx, arr, "length");
    int len = 0;
    JS_ToInt32(ctx, &len, lenVal);
    JS_FreeValue(ctx, lenVal);
    JS_SetPropertyUint32(ctx, arr, (uint32_t)len, JS_DupValue(ctx, argv[0]));
    return JS_NewInt32(ctx, id);
}

/* ---- getComputedStyle ---- */

static JSValue js_getComputedStyle_stub(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv) {
    (void)this_val;
    void *node = NULL;
    if (argc >= 1) node = dom_bindings_node_opaque(ctx, argv[0]);
    return css_get_computed_style(ctx, node);
}

/* ---- localStorage ---- */

static JSValue js_localStorage_getItem(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv) {
    (void)this_val;
    JSEngine *e = JS_GetContextOpaque(ctx);
    if (!e || argc < 1) return JS_NULL;
    const char *key = JS_ToCString(ctx, argv[0]);
    if (!key) return JS_NULL;
    for (int i = 0; i < e->num_local_storage; i++)
        if (e->localStorage[i].key && strcmp(e->localStorage[i].key, key) == 0) {
            JSValue v = JS_NewString(ctx, e->localStorage[i].value);
            JS_FreeCString(ctx, key);
            return v;
        }
    JS_FreeCString(ctx, key);
    return JS_NULL;
}

static JSValue js_localStorage_setItem(JSContext *ctx, JSValueConst this_val,
                                       int argc, JSValueConst *argv) {
    (void)this_val;
    JSEngine *e = JS_GetContextOpaque(ctx);
    if (!e || argc < 2) return JS_UNDEFINED;
    const char *key = JS_ToCString(ctx, argv[0]);
    const char *val = JS_ToCString(ctx, argv[1]);
    if (!key || !val) {
        JS_FreeCString(ctx, (char *)key);
        JS_FreeCString(ctx, (char *)val);
        return JS_UNDEFINED;
    }
    for (int i = 0; i < e->num_local_storage; i++)
        if (e->localStorage[i].key && strcmp(e->localStorage[i].key, key) == 0) {
            free(e->localStorage[i].value);
            e->localStorage[i].value = strdup(val);
            JS_FreeCString(ctx, key);
            JS_FreeCString(ctx, val);
            return JS_UNDEFINED;
        }
    if (e->num_local_storage >= LOCAL_STORAGE_MAX) {
        JS_FreeCString(ctx, key);
        JS_FreeCString(ctx, val);
        return JS_UNDEFINED;
    }
    e->localStorage[e->num_local_storage].key = strdup(key);
    e->localStorage[e->num_local_storage].value = strdup(val);
    e->num_local_storage++;
    JS_FreeCString(ctx, key);
    JS_FreeCString(ctx, val);
    return JS_UNDEFINED;
}

static JSValue js_localStorage_removeItem(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv) {
    (void)this_val;
    JSEngine *e = JS_GetContextOpaque(ctx);
    if (!e || argc < 1) return JS_UNDEFINED;
    const char *key = JS_ToCString(ctx, argv[0]);
    if (!key) return JS_UNDEFINED;
    for (int i = 0; i < e->num_local_storage; i++)
        if (e->localStorage[i].key && strcmp(e->localStorage[i].key, key) == 0) {
            free(e->localStorage[i].key);
            free(e->localStorage[i].value);
            e->localStorage[i] = e->localStorage[--e->num_local_storage];
            break;
        }
    JS_FreeCString(ctx, key);
    return JS_UNDEFINED;
}

static JSValue js_localStorage_key(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    (void)this_val;
    JSEngine *e = JS_GetContextOpaque(ctx);
    if (!e || argc < 1) return JS_NULL;
    int idx;
    JS_ToInt32(ctx, &idx, argv[0]);
    if (idx < 0 || idx >= e->num_local_storage) return JS_NULL;
    return JS_NewString(ctx, e->localStorage[idx].key);
}

static JSValue js_localStorage_get_length(JSContext *ctx, JSValueConst this_val) {
    (void)this_val;
    JSEngine *e = JS_GetContextOpaque(ctx);
    return JS_NewInt32(ctx, e ? e->num_local_storage : 0);
}

/* ---- No-op stubs ---- */

static JSValue js_noop(JSContext *ctx, JSValueConst this_val,
                       int argc, JSValueConst *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_UNDEFINED;
}

static JSValue js_return_zero(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    (void)this_val; (void)argc; (void)argv;
    return JS_NewInt32(ctx, 0);
}

static JSValue js_return_empty_string(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv) {
    (void)this_val; (void)argc; (void)argv;
    return JS_NewString(ctx, "");
}

/* ---- Helper ---- */

static void add_global_func(JSContext *ctx, JSValue obj, const char *name, JSCFunction *f) {
    JS_SetPropertyStr(ctx, obj, name, JS_NewCFunction(ctx, f, name, 1));
}

/* ============================================================
 * Engine creation
 * ============================================================ */

JSEngine *jsengine_create(void) {
    JSEngine *e = calloc(1, sizeof(JSEngine));
    if (!e) return NULL;
    e->rt = JS_NewRuntime();
    if (!e->rt) { free(e); return NULL; }
    JS_SetMemoryLimit(e->rt, 256 * 1024 * 1024);
    e->ctx = JS_NewContext(e->rt);
    if (!e->ctx) { JS_FreeRuntime(e->rt); free(e); return NULL; }
    JS_SetContextOpaque(e->ctx, e);
    e->raf_callbacks = JS_UNDEFINED;
    e->next_timer_id = TIMER_ID_BASE;

    JSValue global = JS_GetGlobalObject(e->ctx);

    /* console */
    JSValue console = JS_NewObject(e->ctx);
    add_global_func(e->ctx, console, "log", js_console_log);
    add_global_func(e->ctx, console, "warn", js_console_warn);
    add_global_func(e->ctx, console, "error", js_console_error);
    add_global_func(e->ctx, console, "info", js_console_log);
    add_global_func(e->ctx, console, "debug", js_console_log);
    add_global_func(e->ctx, console, "trace", js_noop);
    add_global_func(e->ctx, console, "dir", js_console_log);
    add_global_func(e->ctx, console, "time", js_noop);
    add_global_func(e->ctx, console, "timeEnd", js_noop);
    add_global_func(e->ctx, console, "group", js_noop);
    add_global_func(e->ctx, console, "groupEnd", js_noop);
    JS_SetPropertyStr(e->ctx, global, "console", console);

    /* window = self = globalThis = the global object itself.
     * In a browser, window IS the global scope; setting window.X = v
     * makes X available as a global variable. We replicate this by making
     * window a reference to the global object.
     */
    JS_SetPropertyStr(e->ctx, global, "window", JS_DupValue(e->ctx, global));
    JS_SetPropertyStr(e->ctx, global, "self", JS_DupValue(e->ctx, global));
    JS_SetPropertyStr(e->ctx, global, "globalThis", JS_DupValue(e->ctx, global));

    add_global_func(e->ctx, global, "alert", js_alert);
    add_global_func(e->ctx, global, "confirm", js_confirm);
    add_global_func(e->ctx, global, "prompt", js_prompt);
    add_global_func(e->ctx, global, "setTimeout", js_setTimeout);
    add_global_func(e->ctx, global, "setInterval", js_setInterval);
    add_global_func(e->ctx, global, "clearTimeout", js_clearTimeout);
    add_global_func(e->ctx, global, "clearInterval", js_clearTimeout);
    add_global_func(e->ctx, global, "requestAnimationFrame", js_requestAnimationFrame);
    add_global_func(e->ctx, global, "cancelAnimationFrame", js_noop);
    add_global_func(e->ctx, global, "getComputedStyle", js_getComputedStyle_stub);
    add_global_func(e->ctx, global, "matchMedia", js_noop);
    add_global_func(e->ctx, global, "getSelection", js_noop);
    add_global_func(e->ctx, global, "scrollTo", js_noop);
    add_global_func(e->ctx, global, "scroll", js_noop);
    add_global_func(e->ctx, global, "focus", js_noop);
    add_global_func(e->ctx, global, "blur", js_noop);
    add_global_func(e->ctx, global, "open", js_noop);
    add_global_func(e->ctx, global, "close", js_noop);
    add_global_func(e->ctx, global, "print", js_noop);
    add_global_func(e->ctx, global, "addEventListener", js_noop);
    add_global_func(e->ctx, global, "removeEventListener", js_noop);
    add_global_func(e->ctx, global, "dispatchEvent", js_noop);
    add_global_func(e->ctx, global, "btoa", js_return_empty_string);
    add_global_func(e->ctx, global, "atob", js_return_empty_string);
    add_global_func(e->ctx, global, "queueMicrotask", js_noop);
    add_global_func(e->ctx, global, "fetch", js_noop);

    JS_SetPropertyStr(e->ctx, global, "innerWidth", JS_NewInt32(e->ctx, 80));
    JS_SetPropertyStr(e->ctx, global, "innerHeight", JS_NewInt32(e->ctx, 24));
    JS_SetPropertyStr(e->ctx, global, "outerWidth", JS_NewInt32(e->ctx, 80));
    JS_SetPropertyStr(e->ctx, global, "outerHeight", JS_NewInt32(e->ctx, 24));
    JS_SetPropertyStr(e->ctx, global, "pageXOffset", JS_NewInt32(e->ctx, 0));
    JS_SetPropertyStr(e->ctx, global, "pageYOffset", JS_NewInt32(e->ctx, 0));
    JS_SetPropertyStr(e->ctx, global, "scrollX", JS_NewInt32(e->ctx, 0));
    JS_SetPropertyStr(e->ctx, global, "scrollY", JS_NewInt32(e->ctx, 0));
    JS_SetPropertyStr(e->ctx, global, "devicePixelRatio",
                      JS_NewFloat64(e->ctx, 1.0));

    /* location */
    JSValue location = JS_NewObject(e->ctx);
    JS_SetPropertyStr(e->ctx, location, "href", JS_NewString(e->ctx, "file:///story.html"));
    JS_SetPropertyStr(e->ctx, location, "protocol", JS_NewString(e->ctx, "file:"));
    JS_SetPropertyStr(e->ctx, location, "host", JS_NewString(e->ctx, ""));
    JS_SetPropertyStr(e->ctx, location, "hostname", JS_NewString(e->ctx, ""));
    JS_SetPropertyStr(e->ctx, location, "pathname", JS_NewString(e->ctx, "/story.html"));
    JS_SetPropertyStr(e->ctx, location, "search", JS_NewString(e->ctx, ""));
    JS_SetPropertyStr(e->ctx, location, "hash", JS_NewString(e->ctx, ""));
    JS_SetPropertyStr(e->ctx, location, "origin", JS_NewString(e->ctx, "file://"));
    add_global_func(e->ctx, location, "reload", js_noop);
    add_global_func(e->ctx, location, "replace", js_noop);
    add_global_func(e->ctx, location, "assign", js_noop);
    JS_SetPropertyStr(e->ctx, global, "location", location);

    /* history */
    JSValue history = JS_NewObject(e->ctx);
    JS_SetPropertyStr(e->ctx, history, "length", JS_NewInt32(e->ctx, 1));
    add_global_func(e->ctx, history, "pushState", js_noop);
    add_global_func(e->ctx, history, "replaceState", js_noop);
    add_global_func(e->ctx, history, "back", js_noop);
    add_global_func(e->ctx, history, "forward", js_noop);
    add_global_func(e->ctx, history, "go", js_noop);
    JS_SetPropertyStr(e->ctx, global, "history", history);

    /* navigator */
    JSValue navigator = JS_NewObject(e->ctx);
    JS_SetPropertyStr(e->ctx, navigator, "userAgent",
        JS_NewString(e->ctx, "glktwin/1.0 (TUI; Glk)"));
    JS_SetPropertyStr(e->ctx, navigator, "platform",
        JS_NewString(e->ctx, "Glk"));
    JS_SetPropertyStr(e->ctx, navigator, "language",
        JS_NewString(e->ctx, "en-US"));
    JS_SetPropertyStr(e->ctx, navigator, "cookieEnabled", JS_FALSE);
    JS_SetPropertyStr(e->ctx, navigator, "onLine", JS_TRUE);
    JS_SetPropertyStr(e->ctx, navigator, "maxTouchPoints", JS_NewInt32(e->ctx, 0));
    JS_SetPropertyStr(e->ctx, global, "navigator", navigator);

    /* screen */
    JSValue screen = JS_NewObject(e->ctx);
    JS_SetPropertyStr(e->ctx, screen, "width", JS_NewInt32(e->ctx, 80));
    JS_SetPropertyStr(e->ctx, screen, "height", JS_NewInt32(e->ctx, 24));
    JS_SetPropertyStr(e->ctx, screen, "availWidth", JS_NewInt32(e->ctx, 80));
    JS_SetPropertyStr(e->ctx, screen, "availHeight", JS_NewInt32(e->ctx, 24));
    JS_SetPropertyStr(e->ctx, screen, "colorDepth", JS_NewInt32(e->ctx, 8));
    JS_SetPropertyStr(e->ctx, screen, "pixelDepth", JS_NewInt32(e->ctx, 8));
    JS_SetPropertyStr(e->ctx, global, "screen", screen);

    /* performance */
    JSValue performance = JS_NewObject(e->ctx);
    add_global_func(e->ctx, performance, "now", js_return_zero);
    add_global_func(e->ctx, performance, "mark", js_noop);
    add_global_func(e->ctx, performance, "measure", js_noop);
    JS_SetPropertyStr(e->ctx, global, "performance", performance);

    /* localStorage */
    JSValue ls = JS_NewObject(e->ctx);
    add_global_func(e->ctx, ls, "getItem", js_localStorage_getItem);
    add_global_func(e->ctx, ls, "setItem", js_localStorage_setItem);
    add_global_func(e->ctx, ls, "removeItem", js_localStorage_removeItem);
    add_global_func(e->ctx, ls, "key", js_localStorage_key);
    add_global_func(e->ctx, ls, "clear", js_noop);
    {
        JSAtom a = JS_NewAtom(e->ctx, "length");
        JSValue getter = JS_NewCFunction2(e->ctx,
            (JSCFunction *)js_localStorage_get_length, NULL, 0, JS_CFUNC_getter, 0);
        JS_DefinePropertyGetSet(e->ctx, ls, a, getter, JS_UNDEFINED,
                                JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
        JS_FreeAtom(e->ctx, a);
    }
    JS_SetPropertyStr(e->ctx, global, "localStorage", ls);

    /* sessionStorage (same interface, in-memory only) */
    JSValue ss = JS_NewObject(e->ctx);
    add_global_func(e->ctx, ss, "getItem", js_localStorage_getItem);
    add_global_func(e->ctx, ss, "setItem", js_localStorage_setItem);
    add_global_func(e->ctx, ss, "removeItem", js_localStorage_removeItem);
    add_global_func(e->ctx, ss, "key", js_localStorage_key);
    add_global_func(e->ctx, ss, "clear", js_noop);
    JS_SetPropertyStr(e->ctx, global, "sessionStorage", ss);

    /* XMLHttpRequest stub */
    JSValue xhr_proto = JS_NewObject(e->ctx);
    add_global_func(e->ctx, xhr_proto, "open", js_noop);
    add_global_func(e->ctx, xhr_proto, "send", js_noop);
    add_global_func(e->ctx, xhr_proto, "setRequestHeader", js_noop);
    add_global_func(e->ctx, xhr_proto, "abort", js_noop);
    JS_SetPropertyStr(e->ctx, xhr_proto, "status", JS_NewInt32(e->ctx, 0));
    JS_SetPropertyStr(e->ctx, xhr_proto, "readyState", JS_NewInt32(e->ctx, 0));
    JS_SetPropertyStr(e->ctx, xhr_proto, "responseText", JS_NewString(e->ctx, ""));
    JSValue xhr_ctor = JS_NewCFunction2(e->ctx,
        (JSCFunction *)js_noop, "XMLHttpRequest", 0, JS_CFUNC_constructor, 0);
    JS_SetPropertyStr(e->ctx, xhr_ctor, "prototype", xhr_proto);
    JS_SetPropertyStr(e->ctx, global, "XMLHttpRequest", xhr_ctor);

    /* Image stub */
    JS_SetPropertyStr(e->ctx, global, "Image",
        JS_NewCFunction2(e->ctx, (JSCFunction *)js_noop, "Image", 0,
                         JS_CFUNC_constructor, 0));

    /* Audio stub */
    JS_SetPropertyStr(e->ctx, global, "Audio",
        JS_NewCFunction2(e->ctx, (JSCFunction *)js_noop, "Audio", 0,
                         JS_CFUNC_constructor, 0));

    JS_FreeValue(e->ctx, global);

    return e;
}

void jsengine_destroy(JSEngine *e) {
    if (!e) return;
    for (int i = 0; i < e->num_timers; i++)
        JS_FreeValue(e->ctx, e->timers[i].callback);
    free(e->timers);
    for (int i = 0; i < e->num_local_storage; i++) {
        free(e->localStorage[i].key);
        free(e->localStorage[i].value);
    }
    if (!JS_IsUndefined(e->raf_callbacks))
        JS_FreeValue(e->ctx, e->raf_callbacks);
    dom_events_shutdown(e->ctx);
    dom_bindings_shutdown(e->ctx);
    JS_FreeContext(e->ctx);
    JS_FreeRuntime(e->rt);
    free(e);
}

JSContext *jsengine_context(JSEngine *e) { return e ? e->ctx : NULL; }
JSRuntime *jsengine_runtime(JSEngine *e) { return e ? e->rt : NULL; }

bool jsengine_eval(JSEngine *e, const char *src, size_t len, const char *name) {
    if (!e || !src) return false;
    JSValue r = JS_Eval(e->ctx, src, len, name, JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(r)) {
        JSValue ex = JS_GetException(e->ctx);
        const char *s = JS_ToCString(e->ctx, ex);
        if (s) {
            fprintf(stderr, "[JS error in %s] %s\n", name ? name : "?", s);
            JS_FreeCString(e->ctx, s);
        }
        JSValue stack = JS_GetPropertyStr(e->ctx, ex, "stack");
        if (!JS_IsUndefined(stack)) {
            const char *st = JS_ToCString(e->ctx, stack);
            if (st) {
                fprintf(stderr, "%s\n", st);
                JS_FreeCString(e->ctx, st);
            }
        }
        JS_FreeValue(e->ctx, stack);
        JS_FreeValue(e->ctx, ex);
        JS_FreeValue(e->ctx, r);
        return false;
    }
    JS_FreeValue(e->ctx, r);
    return true;
}

void jsengine_timer_tick(JSEngine *e) {
    jsengine_timer_advance(e, 100);
}

void jsengine_timer_advance(JSEngine *e, uint32_t advance_ms) {
    if (!e) return;
    int i, iter;

    e->clock_ms += advance_ms;

    for (iter = 0; iter < 200; iter++) {
        int fired = 0;
        for (i = 0; i < e->num_timers; i++) {
            if (e->timers[i].fire_at_ms > e->clock_ms)
                continue;
            fired = 1;

            int timer_id = e->timers[i].id;
            int count_before = e->num_timers;

            JSValue ret = JS_Call(e->ctx, e->timers[i].callback,
                                  JS_UNDEFINED, 0, NULL);
            if (JS_IsException(ret)) {
                JSValue ex = JS_GetException(e->ctx);
                JSValue msg = JS_GetPropertyStr(e->ctx, ex, "message");
                const char *str = JS_ToCString(e->ctx, msg);
                fprintf(stderr, "[JS timer error] %s\n",
                        str ? str : "(unknown)");
                if (str) JS_FreeCString(e->ctx, str);
                JS_FreeValue(e->ctx, msg);
                JSValue stack = JS_GetPropertyStr(e->ctx, ex, "stack");
                const char *st = JS_ToCString(e->ctx, stack);
                if (st) {
                    fprintf(stderr, "  Stack: %s\n", st);
                    JS_FreeCString(e->ctx, st);
                }
                JS_FreeValue(e->ctx, stack);
                JS_FreeValue(e->ctx, ex);
            }
            JS_FreeValue(e->ctx, ret);

            /* The callback may have called clearInterval/clearTimeout
             * which modifies the timers array. Re-find our timer by ID
             * to check if it still exists. */
            int found = -1;
            for (int j = 0; j < e->num_timers; j++) {
                if (e->timers[j].id == timer_id) { found = j; break; }
            }
            if (found >= 0) {
                if (e->timers[found].is_interval) {
                    e->timers[found].fire_at_ms += e->timers[found].interval_ms;
                } else {
                    JS_FreeValue(e->ctx, e->timers[found].callback);
                    e->timers[found] = e->timers[--e->num_timers];
                }
            }
            /* Restart from the beginning since the array may have changed */
            i = -1;
        }
        if (!fired) break;
    }
}

int jsengine_has_timers(JSEngine *e) {
    return e ? e->num_timers : 0;
}

void jsengine_raf_flush(JSEngine *e) {
    if (!e || JS_IsUndefined(e->raf_callbacks)) return;
    JSValue lenVal = JS_GetPropertyStr(e->ctx, e->raf_callbacks, "length");
    int len = 0;
    JS_ToInt32(e->ctx, &len, lenVal);
    JS_FreeValue(e->ctx, lenVal);
    for (int i = 0; i < len; i++) {
        JSValue cb = JS_GetPropertyUint32(e->ctx, e->raf_callbacks, (uint32_t)i);
        if (!JS_IsUndefined(cb) && !JS_IsNull(cb)) {
            JSValue ret = JS_Call(e->ctx, cb, JS_UNDEFINED, 0, NULL);
            JS_FreeValue(e->ctx, ret);
        }
        JS_FreeValue(e->ctx, cb);
    }
    JS_FreeValue(e->ctx, e->raf_callbacks);
    e->raf_callbacks = JS_UNDEFINED;
}

/* ---- localStorage persistence ---- */

void jsengine_localStorage_save(JSEngine *e, const char *path) {
    if (!e || !path) return;
    char pathname[1024];
    snprintf(pathname, sizeof(pathname), "%s", path);
    strid_t str = glkunix_stream_open_pathname_gen(pathname, 1, 0, 0);
    if (!str) return;
    glk_put_string_stream(str, "{");
    int first = 1;
    for (int i = 0; i < e->num_local_storage; i++)
        if (e->localStorage[i].key && e->localStorage[i].value) {
            if (!first) glk_put_string_stream(str, ",");
            glk_put_string_stream(str, "\"");
            glk_put_string_stream(str, e->localStorage[i].key);
            glk_put_string_stream(str, "\":\"");
            glk_put_string_stream(str, e->localStorage[i].value);
            glk_put_string_stream(str, "\"");
            first = 0;
        }
    glk_put_string_stream(str, "}");
    glk_stream_close(str, NULL);
}

void jsengine_localStorage_load(JSEngine *e, const char *path) {
    if (!e || !path) return;
    char pathname[1024];
    snprintf(pathname, sizeof(pathname), "%s", path);
    strid_t str = glkunix_stream_open_pathname(pathname, 0, 0);
    if (!str) return;
    char buf[8192];
    glui32 n = glk_get_buffer_stream(str, buf, sizeof(buf) - 1);
    glk_stream_close(str, NULL);
    if (n == 0) return;
    buf[n] = '\0';
    JSValue obj = JS_ParseJSON(e->ctx, buf, (size_t)n, "saves.json");
    if (JS_IsException(obj)) return;
    if (!JS_IsObject(obj)) { JS_FreeValue(e->ctx, obj); return; }
    JSPropertyEnum *props; uint32_t len;
    if (JS_GetOwnPropertyNames(e->ctx, &props, &len, obj, JS_GPN_STRING_MASK) != 0) {
        JS_FreeValue(e->ctx, obj);
        return;
    }
    for (uint32_t i = 0; i < len && e->num_local_storage < LOCAL_STORAGE_MAX; i++) {
        JSValue key = JS_AtomToValue(e->ctx, props[i].atom);
        JSValue val = JS_GetProperty(e->ctx, obj, props[i].atom);
        const char *ks = JS_ToCString(e->ctx, key);
        const char *vs = JS_ToCString(e->ctx, val);
        if (ks && vs) {
            e->localStorage[e->num_local_storage].key = strdup(ks);
            e->localStorage[e->num_local_storage].value = strdup(vs);
            e->num_local_storage++;
        }
        JS_FreeCString(e->ctx, ks);
        JS_FreeCString(e->ctx, vs);
        JS_FreeValue(e->ctx, key);
        JS_FreeValue(e->ctx, val);
    }
    JS_FreePropertyEnum(e->ctx, props, len);
    JS_FreeValue(e->ctx, obj);
}

JSValue jsengine_get_computed_style(JSContext *ctx, void *node_opaque) {
    return css_get_computed_style(ctx, node_opaque);
}

bool jsengine_load_jquery(JSEngine *e) {
    return jsengine_eval(e, glktwin_js_jquery, glktwin_js_jquery_len, "jquery.js");
}
