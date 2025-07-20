# tests/CMakeLists.txt
# Unit testing for glkterm autosave functionality

cmake_minimum_required(VERSION 3.10)

# Enable testing
enable_testing()

# Include the parent project's headers
include_directories(${CMAKE_SOURCE_DIR})

# Create a test version of glkterm library without main.c and sound
set(GLKTERM_TEST_SOURCES
    ../glkterm/gtevent.c
    ../glkterm/gtfref.c
    ../glkterm/gtgestal.c
    ../glkterm/gtinput.c
    ../glkterm/gtmessag.c
    ../glkterm/gtmessin.c
    ../glkterm/gtmisc.c
    ../glkterm/gtstream.c
    ../glkterm/gtstyle.c
    ../glkterm/gtw_blnk.c
    ../glkterm/gtw_buf.c
    ../glkterm/gtw_grid.c
    ../glkterm/gtw_pair.c
    ../glkterm/gtwindow.c
    ../glkterm/gtblorb.c
    ../glkterm/cgunicod.c
    ../glkterm/cgdate.c
    ../glkterm/gi_dispa.c
    ../glkterm/gi_blorb.c
    ../glkterm/gtautosave.c
    test_stubs.c
)

add_library(glkterm_test STATIC ${GLKTERM_TEST_SOURCES})
target_include_directories(glkterm_test PUBLIC ../glkterm)
target_link_libraries(glkterm_test ${NCURSES_LIBRARIES})
target_compile_definitions(glkterm_test PRIVATE GLK_NO_SOUND)

# Unity framework
set(UNITY_SOURCES
    unity/unity.c
    unity/unity.h
    unity/unity_internals.h
)

# Build Unity as a static library
add_library(unity STATIC ${UNITY_SOURCES})
target_include_directories(unity PUBLIC unity)

# Test executables
add_executable(test_updatetags
    test_updatetags.c
)
target_link_libraries(test_updatetags unity glkterm_test)
target_include_directories(test_updatetags PRIVATE ../glkterm)

add_executable(test_serialization
    test_serialization.c
)
target_link_libraries(test_serialization unity glkterm_test)
target_include_directories(test_serialization PRIVATE ../glkterm)

# Window serialization tests
add_executable(test_window_serialization
    test_window_serialization.c
)
target_link_libraries(test_window_serialization unity glkterm_test)
target_include_directories(test_window_serialization PRIVATE ../glkterm)

# Stream serialization tests
add_executable(test_stream_serialization
    test_stream_serialization.c
)
target_link_libraries(test_stream_serialization unity glkterm_test)
target_include_directories(test_stream_serialization PRIVATE ../glkterm)

# Fileref serialization tests
add_executable(test_fileref_serialization
    test_fileref_serialization.c
)
target_link_libraries(test_fileref_serialization unity glkterm_test)
target_include_directories(test_fileref_serialization PRIVATE ../glkterm)

# Unserialization tests
add_executable(test_unserialization
    test_unserialization.c
)
target_link_libraries(test_unserialization unity glkterm_test)
target_include_directories(test_unserialization PRIVATE ../glkterm)

# Window hierarchy tests
add_executable(test_window_hierarchy
    test_window_hierarchy.c
)
target_link_libraries(test_window_hierarchy unity glkterm_test)
target_include_directories(test_window_hierarchy PRIVATE ../glkterm)

# Round-trip hierarchy tests
add_executable(test_hierarchy_roundtrip
    test_hierarchy_roundtrip.c
)
target_link_libraries(test_hierarchy_roundtrip unity glkterm_test)
target_include_directories(test_hierarchy_roundtrip PRIVATE ../glkterm)

# Integration tests (file I/O, error conditions, full cycle)
add_executable(test_integration
    test_integration.c
)
target_link_libraries(test_integration unity glkterm_test)
target_include_directories(test_integration PRIVATE ../glkterm)

# Content restoration tests (detailed content preservation)
add_executable(test_content_restoration
    test_content_restoration.c
)
target_link_libraries(test_content_restoration unity glkterm_test)
target_include_directories(test_content_restoration PRIVATE ../glkterm)

# Test targets
add_custom_target(check
    COMMAND test_updatetags
    COMMAND test_serialization
    COMMAND test_window_serialization
    COMMAND test_stream_serialization
    COMMAND test_fileref_serialization
    COMMAND test_unserialization
    COMMAND test_window_hierarchy
    COMMAND test_hierarchy_roundtrip
    DEPENDS test_updatetags test_serialization test_window_serialization test_stream_serialization test_fileref_serialization test_unserialization test_window_hierarchy test_hierarchy_roundtrip
    COMMENT "Running unit tests"
)

# Individual test targets
add_custom_target(test-updatetags
    COMMAND test_updatetags
    DEPENDS test_updatetags
    COMMENT "Running update tag tests"
)

add_custom_target(test-serialization
    COMMAND test_serialization
    DEPENDS test_serialization
    COMMENT "Running serialization tests"
)

# Test registration
add_test(NAME UpdateTags COMMAND test_updatetags)
add_test(NAME Serialization COMMAND test_serialization)
add_test(NAME WindowSerialization COMMAND test_window_serialization)
add_test(NAME StreamSerialization COMMAND test_stream_serialization)
add_test(NAME FilerefSerialization COMMAND test_fileref_serialization)
add_test(NAME Unserialization COMMAND test_unserialization)
add_test(NAME WindowHierarchy COMMAND test_window_hierarchy)
add_test(NAME HierarchyRoundtrip COMMAND test_hierarchy_roundtrip)
