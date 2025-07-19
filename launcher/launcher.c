#include "launcher.h"
#include "launcher_config.h"
#include "launcher_detect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/wait.h>

// Main launcher implementation using modular components

game_format_t launcher_detect_format(const char *game_path) {
    if (!file_exists(game_path)) {
        return GAME_FORMAT_UNKNOWN;
    }
    
    // Try header detection first (most reliable)
    game_format_t format = detect_format_by_header(game_path);
    if (format != GAME_FORMAT_UNKNOWN) {
        return format;
    }
    
    // Fall back to extension detection
    return detect_format_by_extension(game_path);
}

const char *launcher_format_name(game_format_t format) {
    if (format >= 0 && format < FORMAT_NAME_COUNT) {
        return format_names[format];
    }
    return "Unknown";
}

char *launcher_find_interpreter_path(const char *interpreter_name) {
    // First check if it exists in the current build directory (for development)
    char *build_path = malloc(512);
    if (build_path) {
        snprintf(build_path, 512, "./build/terps/%s", interpreter_name);
        if (file_exists(build_path)) {
            return build_path;
        }
        
        // Try without build prefix (if we're in the build directory)
        snprintf(build_path, 512, "./terps/%s", interpreter_name);
        if (file_exists(build_path)) {
            return build_path;
        }
        
        // Try relative to the glkterm binary location
        snprintf(build_path, 512, "../terps/%s", interpreter_name);
        if (file_exists(build_path)) {
            return build_path;
        }
        
        free(build_path);
    }
    
    // Check PATH
    char *path_copy = strdup(getenv("PATH") ?: "");
    if (!path_copy) return NULL;
    
    char *dir = strtok(path_copy, ":");
    while (dir) {
        char *full_path = malloc(strlen(dir) + strlen(interpreter_name) + 2);
        if (full_path) {
            sprintf(full_path, "%s/%s", dir, interpreter_name);
            if (file_exists(full_path)) {
                free(path_copy);
                return full_path;
            }
            free(full_path);
        }
        dir = strtok(NULL, ":");
    }
    
    free(path_copy);
    return NULL;
}

bool launcher_check_interpreter(const char *interpreter_name) {
    char *path = launcher_find_interpreter_path(interpreter_name);
    if (path) {
        free(path);
        return true;
    }
    return false;
}

bool launcher_run_game(const char *game_path, game_format_t format) {
    if (format == GAME_FORMAT_UNKNOWN || format >= INTERPRETER_COUNT) {
        launcher_print_error("Unknown or unsupported game format");
        return false;
    }
    
    const interpreter_info_t *interp = &interpreters[format];
    if (!interp->executable) {
        launcher_print_error("No interpreter configured for this game format");
        return false;
    }
    
    char *interpreter_path = launcher_find_interpreter_path(interp->executable);
    if (!interpreter_path) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), 
                "Interpreter '%s' not found", interp->executable);
        launcher_print_error(error_msg);
        return false;
    }
    
    // Build argument list
    {
        int argc = 2 + interp->flag_count; // program name + flags + game file
        char **argv = malloc((argc + 1) * sizeof(char*));
        int i;
        
        if (!argv) {
            free(interpreter_path);
            launcher_print_error("Memory allocation failed");
            return false;
        }
        
        argv[0] = interpreter_path;
        for (i = 0; i < interp->flag_count; i++) {
            argv[i + 1] = (char*)interp->flags[i];
        }
        argv[argc - 1] = (char*)game_path;
        argv[argc] = NULL;
        // Execute the interpreter
        {
            pid_t pid = fork();
            if (pid == 0) {
                // Child process
                execv(interpreter_path, argv);
                // If we get here, exec failed
                perror("exec failed");
                exit(1);
            } else if (pid > 0) {
                // Parent process - wait for child
                int status;
                waitpid(pid, &status, 0);
                
                free(argv);
                free(interpreter_path);
                
                return WIFEXITED(status) && WEXITSTATUS(status) == 0;
            } else {
                // Fork failed
                free(argv);
                free(interpreter_path);
                launcher_print_error("Failed to launch interpreter");
                return false;
            }
        }
    }
}

bool launcher_detect_and_run(const char *game_path) {
    launcher_print_info("Detecting game format...");
    
    game_format_t format = launcher_detect_format(game_path);
    if (format == GAME_FORMAT_UNKNOWN) {
        launcher_print_error("Unable to detect game format");
        return false;
    }
    
    char info_msg[256];
    snprintf(info_msg, sizeof(info_msg), 
            "Detected format: %s", launcher_format_name(format));
    launcher_print_info(info_msg);
    
    return launcher_run_game(game_path, format);
}

void launcher_print_error(const char *message) {
    fprintf(stderr, "Error: %s\n", message);
}

void launcher_print_info(const char *message) {
    printf("Info: %s\n", message);
}
