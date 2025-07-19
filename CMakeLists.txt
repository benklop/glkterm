cmake_minimum_required(VERSION 3.16)
project(glkterm C CXX)

# Set C standard
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler options
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O")

# Option for sound support
option(ENABLE_SOUND "Enable sound support using SDL2_mixer" ON)

# Find required packages
find_package(PkgConfig)

# Find ncurses (prefer wide character version)
if(PKG_CONFIG_FOUND)
    # Try ncursesw first for extended functions
    pkg_check_modules(NCURSES ncursesw)
    if(NOT NCURSES_FOUND)
        # Fall back to regular ncurses
        pkg_check_modules(NCURSES ncurses)
    endif()
endif()

if(NOT NCURSES_FOUND)
    # Fallback to manual linking if pkg-config doesn't work
    message(WARNING "pkg-config for ncurses not found, using manual linking")
    # Try ncursesw first
    find_library(NCURSESW_LIBRARY ncursesw)
    if(NCURSESW_LIBRARY)
        set(NCURSES_LIBRARIES ncursesw)
    else()
        set(NCURSES_LIBRARIES ncurses)
    endif()
endif()

# Test for extended ncurses functions
include(CheckSymbolExists)
set(CMAKE_REQUIRED_LIBRARIES ${NCURSES_LIBRARIES})
if(NCURSES_FOUND)
    set(CMAKE_REQUIRED_FLAGS ${NCURSES_CFLAGS})
endif()
check_symbol_exists(alloc_pair "ncurses.h" HAVE_ALLOC_PAIR)
check_symbol_exists(init_extended_pair "ncurses.h" HAVE_INIT_EXTENDED_PAIR)
unset(CMAKE_REQUIRED_LIBRARIES)
unset(CMAKE_REQUIRED_FLAGS)

# Find SDL2 if sound is enabled
if(ENABLE_SOUND)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(SDL2 sdl2 SDL2_mixer)
    endif()
    
    if(NOT SDL2_FOUND)
        message(WARNING "pkg-config for SDL2/SDL2_mixer not found, sound support disabled")
        set(ENABLE_SOUND OFF)
    endif()
endif()

# Define the library source files
set(GLKTERM_SOURCES
    main.c
    gtevent.c
    gtfref.c
    gtgestal.c
    gtinput.c
    gtmessag.c
    gtmessin.c
    gtmisc.c
    gtstream.c
    gtstyle.c
    gtw_blnk.c
    gtw_buf.c
    gtw_grid.c
    gtw_pair.c
    gtwindow.c
    gtblorb.c
    cgunicod.c
    cgdate.c
    gi_dispa.c
    gi_blorb.c
)

# Conditionally add sound-related source files
if(ENABLE_SOUND AND SDL2_FOUND)
    list(APPEND GLKTERM_SOURCES gtschan.c)
endif()

# Define the library headers
set(GLKTERM_HEADERS
    glk.h
    glkstart.h
    glkterm.h
    gtoption.h
    gtw_blnk.h
    gtw_buf.h
    gtw_grid.h
    gtw_pair.h
    gi_dispa.h
    gi_blorb.h
    cssrgb.h
    tailq.h
)

# Create the static library
add_library(glkterm STATIC ${GLKTERM_SOURCES})

# Set target properties
target_include_directories(glkterm PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Define sound support - only add definitions if not already defined in headers
if(NOT ENABLE_SOUND OR NOT SDL2_FOUND)
    target_compile_definitions(glkterm PRIVATE GLK_NO_SOUND)
endif()

# Define extended ncurses function availability
if(HAVE_ALLOC_PAIR)
    target_compile_definitions(glkterm PRIVATE HAVE_ALLOC_PAIR)
endif()
if(HAVE_INIT_EXTENDED_PAIR)
    target_compile_definitions(glkterm PRIVATE HAVE_INIT_EXTENDED_PAIR)
endif()

# Link libraries
target_link_libraries(glkterm PUBLIC ${NCURSES_LIBRARIES})

if(NCURSES_FOUND)
    target_compile_options(glkterm PRIVATE ${NCURSES_CFLAGS})
endif()

if(ENABLE_SOUND AND SDL2_FOUND)
    target_link_libraries(glkterm PUBLIC ${SDL2_LIBRARIES})
    target_compile_options(glkterm PRIVATE ${SDL2_CFLAGS})
endif()

# Handle special compilation for cgunicod.c (depends on cgunigen.c)
set_source_files_properties(cgunicod.c PROPERTIES
    OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cgunigen.c
)

# Installation
include(GNUInstallDirs)

# Option to build interpreters
option(BUILD_INTERPRETERS "Build interpreters (terps)" ON)

if(BUILD_INTERPRETERS)
    # Add interpreters subdirectory
    add_subdirectory(terps)
endif()

install(TARGETS glkterm
    EXPORT glkterm-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${GLKTERM_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Create equivalent of Make.glkterm
# Build library flags string
set(LINK_FLAGS "")

# Add ncurses library directories and flags
if(NCURSES_FOUND)
    # Get library directories
    if(NCURSES_LIBRARY_DIRS)
        foreach(dir ${NCURSES_LIBRARY_DIRS})
            set(LINK_FLAGS "${LINK_FLAGS} -L${dir}")
        endforeach()
    endif()
    # Get library names
    if(NCURSES_LIBRARIES)
        foreach(lib ${NCURSES_LIBRARIES})
            set(LINK_FLAGS "${LINK_FLAGS} -l${lib}")
        endforeach()
    endif()
else()
    set(LINK_FLAGS "${LINK_FLAGS} -lncurses")
endif()

# Add SDL2 library directories and flags if enabled
if(ENABLE_SOUND AND SDL2_FOUND)
    # Get library directories
    if(SDL2_LIBRARY_DIRS)
        foreach(dir ${SDL2_LIBRARY_DIRS})
            set(LINK_FLAGS "${LINK_FLAGS} -L${dir}")
        endforeach()
    endif()
    # Get library names
    if(SDL2_LIBRARIES)
        foreach(lib ${SDL2_LIBRARIES})
            set(LINK_FLAGS "${LINK_FLAGS} -l${lib}")
        endforeach()
    endif()
endif()

# Generate Make.glkterm file
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/Make.glkterm
    "# Make.glkterm - Generated by CMake\n"
    "# This file provides linking information for applications using glkterm\n\n"
    "LINKLIBS =${LINK_FLAGS}\n"
    "GLKLIB = -lglkterm\n"
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Make.glkterm
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export targets
install(EXPORT glkterm-targets
    FILE glkterm-config.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/glkterm
)

# Create a config file for find_package
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/glkterm-config-version.cmake
    VERSION 1.0.5
    COMPATIBILITY SameMajorVersion
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/glkterm-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/glkterm
)
