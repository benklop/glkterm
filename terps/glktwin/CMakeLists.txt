# glktwin: TUI Twine interpreter for glkterm
# Dependencies: glkterm (via terp()), vendored lexbor, vendored QuickJS.

set(GLKTWIN_SRCS
    glkstart.c
    main.c
    src/input.c
    src/story_loader.c
    src/jsengine.c
    src/dom_bindings.c
    src/dom_events.c
    src/css_cascade.c
    src/render.c
    src/formats/format_adapter.c
    src/formats/snowman.c
    src/formats/sugarcube.c
)

# QuickJS (vendor/quickjs/)
set(QJS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/quickjs)
set(QJS_SRCS
    ${QJS_DIR}/quickjs.c
    ${QJS_DIR}/quickjs-libc.c
    ${QJS_DIR}/cutils.c
    ${QJS_DIR}/libunicode.c
    ${QJS_DIR}/libregexp.c
)
if(EXISTS ${QJS_DIR}/libbf.c)
    list(APPEND QJS_SRCS ${QJS_DIR}/libbf.c)
endif()
if(EXISTS ${QJS_DIR}/dtoa.c)
    list(APPEND QJS_SRCS ${QJS_DIR}/dtoa.c)
endif()

if(EXISTS ${QJS_DIR}/quickjs.c)
    list(APPEND GLKTWIN_SRCS ${QJS_SRCS})
endif()

# Lexbor amalgamation (vendor/lexbor/lexbor_single.h)
set(LEXBOR_H ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lexbor/lexbor_single.h)
if(EXISTS ${LEXBOR_H})
    list(APPEND GLKTWIN_SRCS src/lexbor_include.c)
endif()

# Embed JS at build time
set(JS_EMBEDDED_C ${CMAKE_CURRENT_BINARY_DIR}/js_embedded.c)
add_custom_command(
    OUTPUT ${JS_EMBEDDED_C}
    COMMAND perl ${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed_js.pl
            ${CMAKE_CURRENT_SOURCE_DIR}/js ${JS_EMBEDDED_C}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed_js.pl
    COMMENT "Embedding JS into C strings"
)
list(APPEND GLKTWIN_SRCS ${JS_EMBEDDED_C})

terp(glktwin
    SRCS ${GLKTWIN_SRCS}
    MACROS GLK _GNU_SOURCE
    POSIX
    MATH
)

set_property(TARGET glktwin PROPERTY C_STANDARD 11)

target_include_directories(glktwin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/quickjs
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lexbor
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/glkterm
)

# QuickJS expects CONFIG_VERSION
if(EXISTS ${QJS_DIR}/quickjs.c)
    target_compile_definitions(glktwin PRIVATE CONFIG_VERSION=\"2025-09-13\")
endif()
