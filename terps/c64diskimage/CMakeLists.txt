add_library(c64diskimage STATIC c64diskimage.c)

set_property(TARGET c64diskimage PROPERTY C_STANDARD 11)
if(CMAKE_C_COMPILER_ID MATCHES "GNU$" OR CMAKE_C_COMPILER_ID MATCHES "Clang$")
    target_compile_options(c64diskimage PRIVATE -Wall -Wextra)
endif()

target_include_directories(c64diskimage INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
