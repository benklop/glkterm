/* format_adapter.h: detect, init, start, navigate per Twine format. */

#ifndef GLKTWIN_FORMAT_ADAPTER_H
#define GLKTWIN_FORMAT_ADAPTER_H

#include "quickjs.h"
#include <stdbool.h>

typedef struct FormatAdapter {
    const char *name;
    bool (*detect)(void *doc_opaque);
    void (*init)(JSContext *ctx);
    void (*start)(JSContext *ctx, void *doc_opaque);
    void (*navigate)(JSContext *ctx, const char *passage_id);
} FormatAdapter;

void format_register(FormatAdapter *a);
void format_register_adapters(void);
FormatAdapter *format_detect(void *doc_opaque);
void format_start(JSContext *ctx, void *doc_opaque);

#endif
