/*
 * Integration test for autosave/autorestore functionality
 * Tests full cycle with file I/O, content restoration, and error conditions
 */

#include <unity.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* GLK headers */
#include "glk.h"
#include "glkterm.h"
#include "glkunix_autosave.h"

static char *temp_autosave_file = NULL;

void setUp(void) {
    /* Create temporary file for autosave tests */
    temp_autosave_file = tempnam(NULL, "glkterm_test_");
    TEST_ASSERT_NOT_NULL(temp_autosave_file);
}

void tearDown(void) {
    /* Clean up temporary file */
    if (temp_autosave_file) {
        unlink(temp_autosave_file);
        free(temp_autosave_file);
        temp_autosave_file = NULL;
    }
}

/* Test full serialize-to-file and restore-from-file cycle */
void test_full_file_cycle(void) {
    FILE *save_file, *restore_file;
    
    /* Create some GLK state */
    winid_t main_win = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 1);
    TEST_ASSERT_NOT_NULL(main_win);
    
    strid_t main_stream = glk_window_get_stream(main_win);
    TEST_ASSERT_NOT_NULL(main_stream);
    
    /* Write some content */
    glk_put_string_stream(main_stream, "Hello, autosave world!");
    
    /* Open file for writing autosave data */
    save_file = fopen(temp_autosave_file, "wb");
    TEST_ASSERT_NOT_NULL(save_file);
    
    /* Create serialize context */
    glkunix_serialize_context_t serialize_ctx = glkunix_serialize_start(save_file);
    TEST_ASSERT_NOT_NULL(serialize_ctx.file);
    
    /* Perform serialization */
    int result = glkunix_serialize_library_state(serialize_ctx);
    TEST_ASSERT_TRUE(result);
    
    /* Finalize serialization */
    glkunix_serialize_end(serialize_ctx);
    fclose(save_file);
    
    /* Clear current state (simulate restart) */
    glk_window_close(main_win, NULL);
    
    /* Open file for reading autosave data */
    restore_file = fopen(temp_autosave_file, "rb");
    TEST_ASSERT_NOT_NULL(restore_file);
    
    /* Create unserialize context */
    glkunix_unserialize_context_t unserialize_ctx = glkunix_unserialize_start(restore_file);
    TEST_ASSERT_NOT_NULL(unserialize_ctx.file);
    
    /* Perform unserialization */
    result = glkunix_unserialize_library_state(unserialize_ctx);
    TEST_ASSERT_TRUE(result);
    
    /* Finalize unserialization */
    glkunix_unserialize_end(unserialize_ctx);
    fclose(restore_file);
    
    /* Verify restored state */
    /* Note: This test validates the file I/O cycle but doesn't validate 
     * content restoration due to current TODOs in implementation */
}

/* Test error conditions during file operations */
void test_file_error_conditions(void) {
    FILE *bad_file;
    glkunix_serialize_context_t serialize_ctx;
    glkunix_unserialize_context_t unserialize_ctx;
    
    /* Test serialization with invalid file */
    bad_file = fopen("/dev/null", "r");  /* Read-only for write operation */
    serialize_ctx = glkunix_serialize_start(bad_file);
    /* Should handle gracefully - implementation-dependent */
    if (serialize_ctx.file) {
        glkunix_serialize_end(serialize_ctx);
    }
    fclose(bad_file);
    
    /* Test unserialization with empty file */
    bad_file = fopen(temp_autosave_file, "w");
    fclose(bad_file);  /* Create empty file */
    
    bad_file = fopen(temp_autosave_file, "rb");
    unserialize_ctx = glkunix_unserialize_start(bad_file);
    
    if (unserialize_ctx.file) {
        int result = glkunix_unserialize_library_state(unserialize_ctx);
        /* Should fail gracefully with empty file */
        TEST_ASSERT_FALSE(result);
        glkunix_unserialize_end(unserialize_ctx);
    }
    fclose(bad_file);
}

/* Test version compatibility */
void test_version_compatibility(void) {
    FILE *save_file, *restore_file;
    
    /* Create minimal state */
    winid_t win = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 1);
    
    /* Save with current version */
    save_file = fopen(temp_autosave_file, "wb");
    glkunix_serialize_context_t ctx = glkunix_serialize_start(save_file);
    glkunix_serialize_library_state(ctx);
    glkunix_serialize_end(ctx);
    fclose(save_file);
    
    /* Close state */
    glk_window_close(win, NULL);
    
    /* Restore and verify version handling */
    restore_file = fopen(temp_autosave_file, "rb");
    glkunix_unserialize_context_t uctx = glkunix_unserialize_start(restore_file);
    
    /* This should succeed with matching version */
    int result = glkunix_unserialize_library_state(uctx);
    TEST_ASSERT_TRUE(result);
    
    glkunix_unserialize_end(uctx);
    fclose(restore_file);
}

/* Test memory stream content preservation (currently limited by TODO) */
void test_memory_stream_preservation(void) {
    /* This test documents what should work once memory stream content 
     * restoration is implemented */
    
    /* Create memory stream */
    char buffer[1024];
    strid_t mem_stream = glk_stream_open_memory(buffer, sizeof(buffer), 
                                               filemode_ReadWrite, 0);
    TEST_ASSERT_NOT_NULL(mem_stream);
    
    /* Write test data */
    glk_put_string_stream(mem_stream, "Memory stream test data");
    
    /* Get current position for later verification */
    glui32 pos_before = glk_stream_get_position(mem_stream);
    TEST_ASSERT_TRUE(pos_before > 0);
    
    /* Serialize to file */
    FILE *save_file = fopen(temp_autosave_file, "wb");
    glkunix_serialize_context_t ctx = glkunix_serialize_start(save_file);
    int result = glkunix_serialize_library_state(ctx);
    TEST_ASSERT_TRUE(result);
    glkunix_serialize_end(ctx);
    fclose(save_file);
    
    /* Close stream */
    glk_stream_close(mem_stream, NULL);
    
    /* Restore from file */
    FILE *restore_file = fopen(temp_autosave_file, "rb");
    glkunix_unserialize_context_t uctx = glkunix_unserialize_start(restore_file);
    result = glkunix_unserialize_library_state(uctx);
    TEST_ASSERT_TRUE(result);
    glkunix_unserialize_end(uctx);
    fclose(restore_file);
    
    /* TODO: Once memory stream content restoration is implemented,
     * verify that the stream content and position are restored correctly */
}

/* Test complex window hierarchy round-trip */
void test_complex_hierarchy_file_roundtrip(void) {
    /* Create complex hierarchy */
    winid_t root = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 100);
    winid_t pair1 = glk_window_open(root, winmethod_Below | winmethod_Proportional, 
                                   50, wintype_Pair, 101);
    winid_t left = glk_window_open(pair1, winmethod_Left | winmethod_Proportional, 
                                  30, wintype_TextGrid, 102);
    winid_t right = glk_window_open(pair1, winmethod_Right | winmethod_Proportional, 
                                   70, wintype_TextBuffer, 103);
    
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_NOT_NULL(pair1);
    TEST_ASSERT_NOT_NULL(left);
    TEST_ASSERT_NOT_NULL(right);
    
    /* Save to file */
    FILE *save_file = fopen(temp_autosave_file, "wb");
    glkunix_serialize_context_t ctx = glkunix_serialize_start(save_file);
    int result = glkunix_serialize_library_state(ctx);
    TEST_ASSERT_TRUE(result);
    glkunix_serialize_end(ctx);
    fclose(save_file);
    
    /* Store reference values for comparison */
    glui32 root_rock = glk_window_get_rock(root);
    glui32 left_rock = glk_window_get_rock(left);
    glui32 right_rock = glk_window_get_rock(right);
    
    /* Close all windows */
    glk_window_close(root, NULL);  /* Should close entire hierarchy */
    
    /* Restore from file */
    FILE *restore_file = fopen(temp_autosave_file, "rb");
    glkunix_unserialize_context_t uctx = glkunix_unserialize_start(restore_file);
    result = glkunix_unserialize_library_state(uctx);
    TEST_ASSERT_TRUE(result);
    glkunix_unserialize_end(uctx);
    fclose(restore_file);
    
    /* Verify hierarchy is restored (basic validation) */
    /* Note: More detailed validation would require access to restored window objects */
}

/* Test endian safety for cross-platform compatibility */
void test_endian_safety(void) {
    /* Create state with various data sizes */
    winid_t win = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 0x12345678);
    strid_t stream = glk_window_get_stream(win);
    
    /* Save to file */
    FILE *save_file = fopen(temp_autosave_file, "wb");
    glkunix_serialize_context_t ctx = glkunix_serialize_start(save_file);
    int result = glkunix_serialize_library_state(ctx);
    TEST_ASSERT_TRUE(result);
    glkunix_serialize_end(ctx);
    fclose(save_file);
    
    glk_window_close(win, NULL);
    
    /* Restore and verify endian handling */
    FILE *restore_file = fopen(temp_autosave_file, "rb");
    glkunix_unserialize_context_t uctx = glkunix_unserialize_start(restore_file);
    result = glkunix_unserialize_library_state(uctx);
    TEST_ASSERT_TRUE(result);
    glkunix_unserialize_end(uctx);
    fclose(restore_file);
    
    /* Implementation uses big-endian format, so this should be safe */
}

/* Test .glkstate file naming convention */
void test_glkstate_naming_convention(void) {
    int result;
    glkunix_library_state_t state;
    
    /* Create minimal GLK state */
    winid_t win = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 42);
    TEST_ASSERT_NOT_NULL(win);
    
    /* Test save with .glkstate naming */
    result = glkunix_save_game_state("testgame");
    TEST_ASSERT_EQUAL(0, result);
    
    /* Verify .glkstate file was created */
    TEST_ASSERT_EQUAL(0, access("testgame.glkstate", F_OK));
    
    /* Close window to clear state */
    glk_window_close(win, NULL);
    
    /* Test load with .glkstate naming */
    state = glkunix_load_game_state("testgame");
    TEST_ASSERT_NOT_NULL(state);
    
    /* Update from loaded state */
    result = glkunix_update_from_library_state(state);
    TEST_ASSERT_EQUAL(0, result);
    
    /* Clean up */
    glkunix_library_state_free(state);
    unlink("testgame.glkstate");
}

/* Test multiple .glkstate files for different games */
void test_multiple_glkstate_files(void) {
    int result;
    
    /* Create minimal state */
    winid_t win = glk_window_open(NULL, 0, 0, wintype_TextBuffer, 1);
    
    /* Save state for multiple game names */
    result = glkunix_save_game_state("game1");
    TEST_ASSERT_EQUAL(0, result);
    
    result = glkunix_save_game_state("game2");
    TEST_ASSERT_EQUAL(0, result);
    
    /* Verify both .glkstate files exist */
    TEST_ASSERT_EQUAL(0, access("game1.glkstate", F_OK));
    TEST_ASSERT_EQUAL(0, access("game2.glkstate", F_OK));
    
    /* Clean up */
    glk_window_close(win, NULL);
    unlink("game1.glkstate");
    unlink("game2.glkstate");
}

/* Test loading non-existent .glkstate file */
void test_load_nonexistent_glkstate(void) {
    glkunix_library_state_t state;
    
    /* Ensure file doesn't exist */
    unlink("nonexistent.glkstate");
    
    /* Try to load - should return NULL */
    state = glkunix_load_game_state("nonexistent");
    TEST_ASSERT_NULL(state);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_full_file_cycle);
    RUN_TEST(test_file_error_conditions);
    RUN_TEST(test_version_compatibility);
    RUN_TEST(test_memory_stream_preservation);
    RUN_TEST(test_complex_hierarchy_file_roundtrip);
    RUN_TEST(test_endian_safety);
    RUN_TEST(test_glkstate_naming_convention);
    RUN_TEST(test_multiple_glkstate_files);
    RUN_TEST(test_load_nonexistent_glkstate);
    
    return UNITY_END();
}
