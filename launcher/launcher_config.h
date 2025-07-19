#ifndef GLKTERM_LAUNCHER_CONFIG_H
#define GLKTERM_LAUNCHER_CONFIG_H

#include <stddef.h>
#include "launcher.h"

// Array size constants
#define INTERPRETER_COUNT (GAME_FORMAT_ADVSYS + 1)
#define FORMAT_NAME_COUNT (GAME_FORMAT_ADVSYS + 1)

// Interpreter mappings - based on what's built in terps/
extern const interpreter_info_t interpreters[INTERPRETER_COUNT];

// Format names for user display
extern const char * const format_names[FORMAT_NAME_COUNT];

// Magic byte patterns for format detection
typedef struct {
    const char *pattern;
    int pattern_length;
    game_format_t format;
} magic_pattern_t;

extern const magic_pattern_t magic_patterns[];
extern const size_t magic_patterns_count;

// File extension mappings
typedef struct {
    const char *extension;
    game_format_t format;
} extension_mapping_t;

extern const extension_mapping_t extension_mappings[];

#endif /* GLKTERM_LAUNCHER_CONFIG_H */
