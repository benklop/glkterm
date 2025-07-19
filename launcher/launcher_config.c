#include "launcher_config.h"
#include "launcher.h"

// Interpreter mappings - based on what's built in terps/
const interpreter_info_t interpreters[INTERPRETER_COUNT] = {
    [GAME_FORMAT_ZCODE] = {"bocfel", NULL, 0},
    [GAME_FORMAT_GLULX] = {"git", NULL, 0},
    [GAME_FORMAT_TADS] = {"tadsr", NULL, 0},
    [GAME_FORMAT_HUGO] = {"hugo", NULL, 0},
    [GAME_FORMAT_AGT] = {"agility", (const char*[]){"-gl"}, 1},
    [GAME_FORMAT_JACL] = {"jacl", NULL, 0},
    [GAME_FORMAT_LEVEL9] = {"level9", NULL, 0},
    [GAME_FORMAT_MAGNETIC] = {"magnetic", NULL, 0},
    [GAME_FORMAT_ALAN2] = {"alan2", NULL, 0},
    [GAME_FORMAT_ALAN3] = {"alan3", NULL, 0},
    [GAME_FORMAT_ADRIFT] = {"scare", NULL, 0},
    [GAME_FORMAT_SCOTT] = {"scott", NULL, 0},
    [GAME_FORMAT_PLUS] = {"plus", NULL, 0},
    [GAME_FORMAT_TAYLOR] = {"taylor", NULL, 0},
    [GAME_FORMAT_ADVSYS] = {"advsys", NULL, 0}
};

// Format names for user display
const char * const format_names[FORMAT_NAME_COUNT] = {
    [GAME_FORMAT_UNKNOWN] = "Unknown",
    [GAME_FORMAT_ZCODE] = "Z-code",
    [GAME_FORMAT_GLULX] = "Glulx",
    [GAME_FORMAT_TADS] = "TADS",
    [GAME_FORMAT_HUGO] = "Hugo",
    [GAME_FORMAT_AGT] = "AGT",
    [GAME_FORMAT_JACL] = "JACL",
    [GAME_FORMAT_LEVEL9] = "Level 9",
    [GAME_FORMAT_MAGNETIC] = "Magnetic Scrolls",
    [GAME_FORMAT_ALAN2] = "Alan 2",
    [GAME_FORMAT_ALAN3] = "Alan 3",
    [GAME_FORMAT_ADRIFT] = "Adrift",
    [GAME_FORMAT_ADRIFT5] = "Adrift 5",
    [GAME_FORMAT_SCOTT] = "Scott Adams",
    [GAME_FORMAT_PLUS] = "Plus",
    [GAME_FORMAT_TAYLOR] = "Taylor",
    [GAME_FORMAT_ADVSYS] = "AdvSys"
};

// Magic byte patterns for format detection
const magic_pattern_t magic_patterns[] = {
    // Z-code: version byte 1-8, then 17 bytes, then 6-digit release date
    {"\x01", 1, GAME_FORMAT_ZCODE}, // We'll do more detailed checking in the function
    {"\x02", 1, GAME_FORMAT_ZCODE},
    {"\x03", 1, GAME_FORMAT_ZCODE},
    {"\x04", 1, GAME_FORMAT_ZCODE},
    {"\x05", 1, GAME_FORMAT_ZCODE},
    {"\x06", 1, GAME_FORMAT_ZCODE},
    {"\x07", 1, GAME_FORMAT_ZCODE},
    {"\x08", 1, GAME_FORMAT_ZCODE},
    
    // TADS
    {"TADS2 bin\x0a\x0d\x1a", 11, GAME_FORMAT_TADS},
    {"T3-image\x0d\x0a\x1a", 11, GAME_FORMAT_TADS},
    
    // Glulx
    {"Glul", 4, GAME_FORMAT_GLULX},
    
    // Hugo
    {"\x16", 1, GAME_FORMAT_HUGO}, // Hugo uses various version bytes
    {"\x18", 1, GAME_FORMAT_HUGO},
    {"\x19", 1, GAME_FORMAT_HUGO},
    {"\x1e", 1, GAME_FORMAT_HUGO},
    {"\x1f", 1, GAME_FORMAT_HUGO},
    
    // AGT
    {"\x58\xc7\xc1\x51", 4, GAME_FORMAT_AGT},
    
    // Alan 2
    {"\x02\x07\x05", 3, GAME_FORMAT_ALAN2},
    {"\x02\x08\x01", 3, GAME_FORMAT_ALAN2},
    {"\x02\x08\x02", 3, GAME_FORMAT_ALAN2},
    {"\x02\x08\x03", 3, GAME_FORMAT_ALAN2},
    {"\x02\x08\x07", 3, GAME_FORMAT_ALAN2},
    
    // Alan 3
    {"ALAN\x03", 5, GAME_FORMAT_ALAN3},
};

const size_t magic_patterns_count = sizeof(magic_patterns) / sizeof(magic_patterns[0]);

// File extension mappings
const extension_mapping_t extension_mappings[] = {
    {"z1", GAME_FORMAT_ZCODE},
    {"z2", GAME_FORMAT_ZCODE},
    {"z3", GAME_FORMAT_ZCODE},
    {"z4", GAME_FORMAT_ZCODE},
    {"z5", GAME_FORMAT_ZCODE},
    {"z6", GAME_FORMAT_ZCODE},
    {"z7", GAME_FORMAT_ZCODE},
    {"z8", GAME_FORMAT_ZCODE},
    {"dat", GAME_FORMAT_ZCODE},  // Also used by AdvSys, but Z-code is more common
    {"ulx", GAME_FORMAT_GLULX},
    {"gam", GAME_FORMAT_TADS},
    {"t3", GAME_FORMAT_TADS},
    {"hex", GAME_FORMAT_HUGO},
    {"agx", GAME_FORMAT_AGT},
    {"d$$", GAME_FORMAT_AGT},
    {"jacl", GAME_FORMAT_JACL},
    {"j2", GAME_FORMAT_JACL},
    {"l9", GAME_FORMAT_LEVEL9},
    {"sna", GAME_FORMAT_LEVEL9},
    {"mag", GAME_FORMAT_MAGNETIC},
    {"acd", GAME_FORMAT_ALAN2},
    {"a3c", GAME_FORMAT_ALAN3},
    {"taf", GAME_FORMAT_ADRIFT},
    {"saga", GAME_FORMAT_SCOTT},
    {"plus", GAME_FORMAT_PLUS},
    {"tay", GAME_FORMAT_TAYLOR},
    {NULL, GAME_FORMAT_UNKNOWN}
};
