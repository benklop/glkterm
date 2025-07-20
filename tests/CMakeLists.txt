# tests/CMakeLists.txt
# Unit testing for glkterm autosave functionality

cmake_minimum_required(VERSION 3.10)

# Enable testing
enable_testing()

# Include the parent project's headers
include_directories(${CMAKE_SOURCE_DIR})

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
    ../glkterm/gtautosave.c
)
target_link_libraries(test_updatetags unity)
target_include_directories(test_updatetags PRIVATE ../glkterm)

add_executable(test_serialization
    test_serialization.c
    ../glkterm/gtautosave.c
)
target_link_libraries(test_serialization unity)
target_include_directories(test_serialization PRIVATE ../glkterm)

# Test targets
add_custom_target(check
    COMMAND test_updatetags
    COMMAND test_serialization
    DEPENDS test_updatetags test_serialization
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
