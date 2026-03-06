#!/usr/bin/env bash
# Fetch and vendor QuickJS and Lexbor for glktwin.
# Run from terps/glktwin (repository root of glktwin).

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GLKTWIN_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
VENDOR="$GLKTWIN_DIR/vendor"

# QuickJS
QJS_URL="${QJS_URL:-https://github.com/bellard/quickjs.git}"
QJS_DIR="$VENDOR/quickjs/repo"
mkdir -p "$VENDOR/quickjs"
if [ ! -d "$QJS_DIR/.git" ]; then
  git clone --depth 1 "$QJS_URL" "$QJS_DIR"
fi
for f in quickjs.c quickjs.h quickjs-opcode.h quickjs-atom.h quickjs-libc.c quickjs-libc.h cutils.c cutils.h libunicode.c libunicode.h libunicode-table.h libregexp.c libregexp.h libregexp-opcode.h list.h; do
  [ -f "$QJS_DIR/$f" ] && cp "$QJS_DIR/$f" "$VENDOR/quickjs/"
done
# Optional but commonly needed
for f in libbf.c libbf.h dtoa.c dtoa.h; do
  [ -f "$QJS_DIR/$f" ] && cp "$QJS_DIR/$f" "$VENDOR/quickjs/" || true
done

# Lexbor amalgamation
LEXBOR_URL="${LEXBOR_URL:-https://github.com/lexbor/lexbor.git}"
LEXBOR_DIR="$VENDOR/lexbor/repo"
mkdir -p "$VENDOR/lexbor"
if [ ! -d "$LEXBOR_DIR/.git" ]; then
  git clone --depth 1 "$LEXBOR_URL" "$LEXBOR_DIR"
fi
if command -v perl >/dev/null 2>&1; then
  (cd "$LEXBOR_DIR" && perl single.pl html css selectors dom > "$VENDOR/lexbor/lexbor_single.h")
  echo "Created $VENDOR/lexbor/lexbor_single.h"
else
  echo "Perl not found; skip lexbor. Install Perl and re-run to generate lexbor_single.h."
fi
