#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "launcher.h"
#include "launcher_cli.h"

int main(int argc, char *argv[]) {
    cli_options_t options;
    int parse_result = cli_parse_args(argc, argv, &options);
    
    if (parse_result < 0) {
        cli_print_usage(argv[0]);
        return 1;
    }
    
    if (options.show_help) {
        cli_print_usage(argv[0]);
        cli_cleanup_options(&options);
        return 0;
    }
    
    if (options.show_format_only) {
        game_format_t format = launcher_detect_format(options.game_file);
        printf("Detected format: %s\n", launcher_format_name(format));
        cli_cleanup_options(&options);
        if (format == GAME_FORMAT_UNKNOWN) {
            return 1;
        }
        return 0;
    }
    
    cli_print_game_info(options.game_file, GAME_FORMAT_UNKNOWN, options.verbose);
    
    // Detect and run the game
    bool success = launcher_detect_and_run(options.game_file);
    cli_cleanup_options(&options);
    return success ? 0 : 1;
}
