#include "launcher_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

void cli_print_usage(const char *program_name) {
    printf("Usage: %s [options] <game_file>\n", program_name);
    printf("\nOptions:\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -f, --format        Show detected game format without running\n");
    printf("  -v, --verbose       Show additional information\n");
    printf("  --no-save          Launch without loading save file (future)\n");
    printf("  --list-saves       List available save files (future)\n");
    printf("  --save=<file>      Load specific save file (future)\n");
    printf("\nSupported game formats:\n");
    cli_print_supported_formats();
}

void cli_print_supported_formats(void) {
    printf("  Z-code (.z1-.z8, .dat)\n");
    printf("  Glulx (.ulx)\n");
    printf("  TADS (.gam, .t3)\n");
    printf("  Hugo (.hex)\n");
    printf("  AGT (.agx, .d$$)\n");
    printf("  JACL (.jacl, .j2)\n");
    printf("  Level 9 (.l9, .sna)\n");
    printf("  Magnetic Scrolls (.mag)\n");
    printf("  Alan (.acd, .a3c)\n");
    printf("  Adrift (.taf)\n");
    printf("  Scott Adams (.saga)\n");
    printf("  Plus (.plus)\n");
    printf("  Taylor (.tay)\n");
}

int cli_parse_args(int argc, char *argv[], cli_options_t *options) {
    int i;
    
    // Initialize options
    memset(options, 0, sizeof(*options));
    
    if (argc < 2) {
        return -1; // Need at least one argument
    }
    
    // Parse command line arguments
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            options->show_help = true;
            return 0;
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--format") == 0) {
            options->show_format_only = true;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            options->verbose = true;
        } else if (strcmp(argv[i], "--no-save") == 0) {
            options->no_save = true;
            if (options->verbose) printf("Note: --no-save option not yet implemented\n");
        } else if (strcmp(argv[i], "--list-saves") == 0) {
            options->list_saves = true;
            printf("Error: --list-saves option not yet implemented\n");
            return -1;
        } else if (strncmp(argv[i], "--save=", 7) == 0) {
            options->save_file = strdup(argv[i] + 7);
            if (!options->save_file) {
                return -1; // Memory allocation failed
            }
            printf("Error: --save option not yet implemented\n");
            return -1;
        } else if (argv[i][0] == '-') {
            printf("Error: Unknown option: %s\n", argv[i]);
            return -1;
        } else {
            if (options->game_file != NULL) {
                printf("Error: Multiple game files specified\n");
                return -1;
            }
            options->game_file = strdup(argv[i]);
            if (!options->game_file) {
                return -1; // Memory allocation failed
            }
        }
    }
    
    if (options->game_file == NULL && !options->show_help) {
        printf("Error: No game file specified\n");
        return -1;
    }
    
    return 0;
}

void cli_cleanup_options(cli_options_t *options) {
    if (options->save_file) {
        free(options->save_file);
        options->save_file = NULL;
    }
    if (options->game_file) {
        free(options->game_file);
        options->game_file = NULL;
    }
}

void cli_print_game_info(const char *game_file, game_format_t format, bool verbose) {
    if (verbose) {
        printf("glkcli - glkterm command-line launcher\n");
        printf("Game file: %s\n", game_file);
    }
}

void cli_print_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void cli_print_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("Info: ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}
