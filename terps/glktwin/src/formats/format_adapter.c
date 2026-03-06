/* format_adapter.c: Format registry and dispatch. */

#include "format_adapter.h"
#include <stdlib.h>
#include <string.h>

#define MAX_ADAPTERS 8
static FormatAdapter *adapters[MAX_ADAPTERS];
static int num_adapters;

extern void format_register_snowman(void);
extern void format_register_sugarcube(void);

void format_register_adapters(void) {
    num_adapters = 0;
    format_register_sugarcube();
    format_register_snowman();
}

void format_register(FormatAdapter *a) {
    if (num_adapters < MAX_ADAPTERS)
        adapters[num_adapters++] = a;
}

FormatAdapter *format_detect(void *doc_opaque) {
    (void)doc_opaque;
    for (int i = 0; i < num_adapters; i++)
        if (adapters[i]->detect(doc_opaque))
            return adapters[i];
    return NULL;
}

void format_start(JSContext *ctx, void *doc_opaque) {
    FormatAdapter *a = format_detect(doc_opaque);
    if (a) {
        a->init(ctx);
        a->start(ctx, doc_opaque);
    }
}
