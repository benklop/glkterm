# JS patches for glktwin

Patches are applied to vendored JS before embedding. Keep as diff files for re-application on upstream updates.

## jQuery (js/patches/jquery.patch)

- Stub `.css()` setters for visual-only properties (no-op).
- `.animate()`: call completion callback immediately.
- `.offset()` / `.position()` / `.width()` / `.height()`: return 0 (or sensible defaults).
- `$.ajax` / `$.get`: reject gracefully (no network in TUI).

## Format runtimes (js/patches/formats/)

- Strip or stub audio/image/canvas APIs.
- SugarCube: UI dialogs → Glk equivalents.
- Harlowe: transition macros → resolve immediately.

When adding real format runtimes (Snowman, Chapbook, Harlowe, SugarCube) under `js/formats/`, add corresponding patches here.
