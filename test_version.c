#include <ncurses.h>
#include <stdio.h>
int main() { 
    printf("NCURSES_VERSION: %s\n", NCURSES_VERSION);
    printf("NCURSES_VERSION_MAJOR: %d\n", NCURSES_VERSION_MAJOR);
    printf("NCURSES_VERSION_MINOR: %d\n", NCURSES_VERSION_MINOR);
    printf("NCURSES_VERSION_PATCH: %d\n", NCURSES_VERSION_PATCH);
    return 0; 
}
