/* css_cascade.c: getComputedStyle stub returning an object with getPropertyValue. */

#include "css_cascade.h"
#include <stdlib.h>
#include <string.h>

static JSValue js_getPropertyValue(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
    (void)this_val;
    if (argc < 1) return JS_NewString(ctx, "");
    const char *prop = JS_ToCString(ctx, argv[0]);
    if (!prop) return JS_NewString(ctx, "");

    /* Return sensible defaults for commonly queried properties */
    const char *val = "";
    if (strcmp(prop, "display") == 0) val = "block";
    else if (strcmp(prop, "visibility") == 0) val = "visible";
    else if (strcmp(prop, "position") == 0) val = "static";
    else if (strcmp(prop, "overflow") == 0) val = "visible";

    JS_FreeCString(ctx, prop);
    return JS_NewString(ctx, val);
}

static JSValue js_removeProperty(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv) {
    (void)this_val; (void)argc; (void)argv;
    return JS_NewString(ctx, "");
}

static JSValue js_setProperty(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    (void)this_val; (void)argc; (void)argv;
    return JS_UNDEFINED;
}

void css_cascade_init(JSContext *ctx) {
    (void)ctx;
}

JSValue css_get_computed_style(JSContext *ctx, void *node_opaque) {
    (void)node_opaque;
    JSValue style = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, style, "getPropertyValue",
        JS_NewCFunction(ctx, js_getPropertyValue, "getPropertyValue", 1));
    JS_SetPropertyStr(ctx, style, "removeProperty",
        JS_NewCFunction(ctx, js_removeProperty, "removeProperty", 1));
    JS_SetPropertyStr(ctx, style, "setProperty",
        JS_NewCFunction(ctx, js_setProperty, "setProperty", 3));

    /* Common CSS properties as direct string properties for jQuery */
    JS_SetPropertyStr(ctx, style, "display", JS_NewString(ctx, "block"));
    JS_SetPropertyStr(ctx, style, "visibility", JS_NewString(ctx, "visible"));
    JS_SetPropertyStr(ctx, style, "position", JS_NewString(ctx, "static"));
    JS_SetPropertyStr(ctx, style, "overflow", JS_NewString(ctx, "visible"));
    JS_SetPropertyStr(ctx, style, "width", JS_NewString(ctx, "auto"));
    JS_SetPropertyStr(ctx, style, "height", JS_NewString(ctx, "auto"));
    JS_SetPropertyStr(ctx, style, "opacity", JS_NewString(ctx, "1"));
    JS_SetPropertyStr(ctx, style, "color", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, style, "backgroundColor", JS_NewString(ctx, ""));
    JS_SetPropertyStr(ctx, style, "margin", JS_NewString(ctx, "0px"));
    JS_SetPropertyStr(ctx, style, "padding", JS_NewString(ctx, "0px"));
    JS_SetPropertyStr(ctx, style, "fontSize", JS_NewString(ctx, "16px"));
    JS_SetPropertyStr(ctx, style, "fontFamily", JS_NewString(ctx, "monospace"));
    JS_SetPropertyStr(ctx, style, "lineHeight", JS_NewString(ctx, "normal"));
    JS_SetPropertyStr(ctx, style, "length", JS_NewInt32(ctx, 0));

    return style;
}
