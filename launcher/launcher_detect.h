#ifndef GLKTERM_LAUNCHER_DETECT_H
#define GLKTERM_LAUNCHER_DETECT_H

#include "launcher.h"

// File analysis functions
game_format_t detect_format_by_header(const char *file_path);
game_format_t detect_format_by_extension(const char *file_path);
game_format_t detect_format_by_blorb(const char *file_path);

// Utility functions for detection
bool file_exists(const char *path);
char *get_file_extension(const char *path);

#endif /* GLKTERM_LAUNCHER_DETECT_H */
