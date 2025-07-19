#ifndef GLKTERM_LAUNCHER_H
#define GLKTERM_LAUNCHER_H

#include <stdbool.h>

// Game format enumeration
typedef enum {
    GAME_FORMAT_UNKNOWN = 0,
    GAME_FORMAT_ZCODE,
    GAME_FORMAT_GLULX,
    GAME_FORMAT_TADS,
    GAME_FORMAT_HUGO,
    GAME_FORMAT_AGT,
    GAME_FORMAT_JACL,
    GAME_FORMAT_LEVEL9,
    GAME_FORMAT_MAGNETIC,
    GAME_FORMAT_ALAN2,
    GAME_FORMAT_ALAN3,
    GAME_FORMAT_ADRIFT,
    GAME_FORMAT_ADRIFT5,
    GAME_FORMAT_SCOTT,
    GAME_FORMAT_PLUS,
    GAME_FORMAT_TAYLOR,
    GAME_FORMAT_ADVSYS
} game_format_t;

// Interpreter information structure
typedef struct {
    const char *executable;
    const char **flags;
    int flag_count;
} interpreter_info_t;

// Main launcher functions
bool launcher_detect_and_run(const char *game_path);
game_format_t launcher_detect_format(const char *game_path);
const char *launcher_format_name(game_format_t format);
bool launcher_run_game(const char *game_path, game_format_t format);

// Utility functions
bool launcher_check_interpreter(const char *interpreter_name);
char *launcher_find_interpreter_path(const char *interpreter_name);
void launcher_print_error(const char *message);
void launcher_print_info(const char *message);

#endif /* GLKTERM_LAUNCHER_H */
