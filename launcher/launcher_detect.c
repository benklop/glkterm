#include "launcher_detect.h"
#include "launcher_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

char *get_file_extension(const char *path) {
    char *ext = strrchr(path, '.');
    if (ext == NULL) return NULL;
    
    // Convert to lowercase
    char *lower_ext = malloc(strlen(ext + 1) + 1);
    if (lower_ext == NULL) return NULL;
    
    int i;
    for (i = 0; ext[i + 1]; i++) {
        lower_ext[i] = tolower(ext[i + 1]);
    }
    lower_ext[i] = '\0';
    
    return lower_ext;
}

game_format_t detect_format_by_header(const char *file_path) {
    FILE *f = fopen(file_path, "rb");
    if (!f) return GAME_FORMAT_UNKNOWN;
    
    unsigned char header[32];
    size_t bytes_read = fread(header, 1, sizeof(header), f);
    fclose(f);
    
    if (bytes_read < 4) return GAME_FORMAT_UNKNOWN;
    
    // Check for Blorb format first
    if (bytes_read >= 12 && 
        memcmp(header, "FORM", 4) == 0 &&
        memcmp(header + 8, "IFRS", 4) == 0) {
        return detect_format_by_blorb(file_path);
    }
    
    // Check magic patterns
    {
        size_t i;
        for (i = 0; i < magic_patterns_count; i++) {
            if (bytes_read >= (size_t)magic_patterns[i].pattern_length) {
                if (memcmp(header, magic_patterns[i].pattern, magic_patterns[i].pattern_length) == 0) {
                    // Special handling for Z-code which needs additional validation
                    if (magic_patterns[i].format == GAME_FORMAT_ZCODE) {
                        // Z-code files have version in first byte, then specific structure
                        if (header[0] >= 1 && header[0] <= 8 && bytes_read >= 26) {
                            // Additional Z-code validation could go here
                            return GAME_FORMAT_ZCODE;
                        }
                    } else if (magic_patterns[i].format == GAME_FORMAT_HUGO) {
                        // Hugo files have version byte followed by date pattern
                        if (bytes_read >= 7 && header[3] == '-' && header[6] == '-') {
                            return GAME_FORMAT_HUGO;
                        }
                    } else {
                        return magic_patterns[i].format;
                    }
                }
            }
        }
    }
    
    return GAME_FORMAT_UNKNOWN;
}

game_format_t detect_format_by_extension(const char *file_path) {
    char *ext = get_file_extension(file_path);
    if (!ext) return GAME_FORMAT_UNKNOWN;
    
    {
        size_t i;
        for (i = 0; extension_mappings[i].extension; i++) {
            if (strcmp(ext, extension_mappings[i].extension) == 0) {
                free(ext);
                return extension_mappings[i].format;
            }
        }
    }
    
    free(ext);
    return GAME_FORMAT_UNKNOWN;
}

game_format_t detect_format_by_blorb(const char *file_path) {
    FILE *f = fopen(file_path, "rb");
    if (!f) return GAME_FORMAT_UNKNOWN;
    
    // Skip FORM header and look for RIdx
    fseek(f, 12, SEEK_SET);
    char ridx[4];
    if (fread(ridx, 1, 4, f) != 4 || memcmp(ridx, "RIdx", 4) != 0) {
        fclose(f);
        return GAME_FORMAT_UNKNOWN;
    }
    
    // Read RIdx size
    unsigned char size_bytes[4];
    if (fread(size_bytes, 1, 4, f) != 4) {
        fclose(f);
        return GAME_FORMAT_UNKNOWN;
    }
    
    long ridx_size = (size_bytes[0] << 24) | (size_bytes[1] << 16) | 
                     (size_bytes[2] << 8) | size_bytes[3];
    
    // Skip resource count (4 bytes) and read first resource entry
    fseek(f, 4, SEEK_CUR);
    
    // Read first resource type (4 bytes)
    char resource_type[4];
    if (fread(resource_type, 1, 4, f) == 4) {
        if (memcmp(resource_type, "Exec", 4) == 0) {
            // Skip resource number (4 bytes), read offset (4 bytes)
            fseek(f, 4, SEEK_CUR);
            if (fread(size_bytes, 1, 4, f) == 4) {
                long offset = (size_bytes[0] << 24) | (size_bytes[1] << 16) | 
                             (size_bytes[2] << 8) | size_bytes[3];
                
                // Jump to the executable data and check its format
                fseek(f, offset, SEEK_SET); // offset already points to data
                char exec_header[4];
                if (fread(exec_header, 1, 4, f) == 4) {
                    if (memcmp(exec_header, "Glul", 4) == 0 || 
                        memcmp(exec_header, "GLUL", 4) == 0) {
                        fclose(f);
                        return GAME_FORMAT_GLULX;
                    } else if (exec_header[0] >= 1 && exec_header[0] <= 8) {
                        // Z-code version
                        fclose(f);
                        return GAME_FORMAT_ZCODE;
                    }
                }
            }
        }
    }
    
    // Fallback: assume Z-code as it's most common in Blorb files
    fclose(f);
    return GAME_FORMAT_ZCODE;
}
