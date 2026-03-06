# QuickJS (vendored)

glktwin uses QuickJS for JavaScript execution. To populate this directory:

1. Clone QuickJS: `git clone https://github.com/bellard/quickjs.git repo`
2. Copy the following files from `repo/` into this directory:
   - quickjs.c, quickjs.h, quickjs-opcode.h, quickjs-atom.h
   - quickjs-libc.c, quickjs-libc.h
   - cutils.c, cutils.h
   - libunicode.c, libunicode.h, libunicode-table.h
   - libregexp.c, libregexp.h, libregexp-opcode.h
   - libbf.c, libbf.h (optional, for big decimal)
   - dtoa.c, dtoa.h (optional, for number conversion)

Minimum required for glktwin: quickjs.c, quickjs-libc.c, cutils.c, libunicode.c, libregexp.c, and all headers.

Or run from glktwin directory: `./scripts/fetch_deps.sh`
