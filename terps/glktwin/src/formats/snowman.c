/* snowman.c: Snowman format adapter (stub). */

#include "format_adapter.h"
#include <stdbool.h>

static bool snowman_detect(void *doc_opaque) {
    (void)doc_opaque;
    return false;
}

static void snowman_init(JSContext *ctx) {
    (void)ctx;
}

static void snowman_start(JSContext *ctx, void *doc_opaque) {
    (void)ctx;
    (void)doc_opaque;
}

static void snowman_navigate(JSContext *ctx, const char *passage_id) {
    (void)ctx;
    (void)passage_id;
}

static FormatAdapter snowman_adapter = {
    "snowman",
    snowman_detect,
    snowman_init,
    snowman_start,
    snowman_navigate,
};

void format_register_snowman(void) {
    format_register(&snowman_adapter);
}
