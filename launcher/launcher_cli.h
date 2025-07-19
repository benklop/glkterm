#ifndef GLKTERM_LAUNCHER_CLI_H
#define GLKTERM_LAUNCHER_CLI_H

#include <stdbool.h>
#include "launcher.h"

// CLI option structure for argument parsing
typedef struct {
    bool show_help;
    bool show_format_only;
    bool verbose;
    bool no_save;
    bool list_saves;
    char *save_file;
    char *game_file;
} cli_options_t;

// CLI utility functions
void cli_print_usage(const char *program_name);
void cli_print_supported_formats(void);
int cli_parse_args(int argc, char *argv[], cli_options_t *options);
void cli_cleanup_options(cli_options_t *options);

// Output formatting functions
void cli_print_game_info(const char *game_file, game_format_t format, bool verbose);
void cli_print_error(const char *format, ...);
void cli_print_info(const char *format, ...);

#endif /* GLKTERM_LAUNCHER_CLI_H */
