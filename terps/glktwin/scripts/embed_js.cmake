# Embed JS files as C string literals. Generates js_embedded.c in the build dir.
# Usage: include this from glktwin CMakeLists, then add the generated source to glktwin.
# Input: GLKTWIN_JS_FILES list of .js file paths (relative to CMAKE_CURRENT_SOURCE_DIR).

function(glktwin_embed_js out_c_file)
    set(JS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/js")
    set(OUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/${out_c_file}")
    set(EMBED_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed_js.pl")
    if(NOT EXISTS "${EMBED_SCRIPT}")
        # Fallback: generate a minimal C file with empty strings if no .js files
        set(MINIMAL "#include <stddef.h>\nconst char *glktwin_js_jquery = \"\";\nsize_t glktwin_js_jquery_len = 0;\n")
        file(WRITE "${OUT_PATH}" "/* Auto-generated; no embed script. */\n${MINIMAL}\n")
        return()
    endif()
    add_custom_command(
        OUTPUT "${OUT_PATH}"
        COMMAND perl "${EMBED_SCRIPT}" "${JS_DIR}" "${OUT_PATH}"
        DEPENDS "${EMBED_SCRIPT}"
        COMMENT "Embedding JS files into C strings"
    )
endfunction()
