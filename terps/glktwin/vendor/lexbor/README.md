# Lexbor (vendored amalgamation)

glktwin uses Lexbor for HTML5, DOM, CSS, and Selectors. To generate the amalgamation:

1. Clone Lexbor: `git clone https://github.com/lexbor/lexbor.git repo`
2. From the repo directory run: `perl single.pl html css selectors dom > ../lexbor_single.h`
3. The file `lexbor_single.h` should appear in this directory.

Or run from glktwin directory: `./scripts/fetch_deps.sh`

Then build glktwin; one source file (e.g. src/lexbor_include.c) will #include "lexbor_single.h".
