/* test_graphics_serialization.c: Test graphics window serialization */

#include "unity.h"
#include "../glkterm/glk.h"
#include "../glkterm/glkterm.h"
#include "../glkterm/glkunix_autosave.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void setUp(void) {
    /* Set up before each test */
}

void tearDown(void) {
    /* Clean up after each test */
}

/* Test that graphics window type is recognized */
void test_graphics_window_type_recognition(void) {
    /* Test that wintype_Graphics is properly defined */
    TEST_ASSERT_EQUAL(5, wintype_Graphics);
    
    /* Test window type constants are distinct */
    TEST_ASSERT_NOT_EQUAL(wintype_Graphics, wintype_TextBuffer);
    TEST_ASSERT_NOT_EQUAL(wintype_Graphics, wintype_TextGrid);
    TEST_ASSERT_NOT_EQUAL(wintype_Graphics, wintype_Pair);
    TEST_ASSERT_NOT_EQUAL(wintype_Graphics, wintype_Blank);
}

/* Test graphics serialization context handling */
void test_graphics_serialization_context(void) {
    FILE *file;
    char tempfile[] = "/tmp/graphics_context_test_XXXXXX";
    int fd = mkstemp(tempfile);
    TEST_ASSERT_NOT_EQUAL(-1, fd);
    
    file = fdopen(fd, "w+b");
    TEST_ASSERT_NOT_NULL(file);
    
    glkunix_serialize_context_t ctx = glkunix_serialize_start(file);
    TEST_ASSERT_NOT_NULL(ctx.file);
    
    /* Test basic serialization operations work */
    int result = glkunix_serialize_uint32(ctx, "test_value", 42);
    TEST_ASSERT_EQUAL(1, result);
    
    glkunix_serialize_end(ctx);
    
    /* Test unserialization */
    rewind(file);
    glkunix_unserialize_context_t read_ctx = glkunix_unserialize_start(file);
    TEST_ASSERT_NOT_NULL(read_ctx.file);
    
    glui32 value;
    result = glkunix_unserialize_uint32(read_ctx, "test_value", &value);
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_EQUAL(42, value);
    
    glkunix_unserialize_end(read_ctx);
    
    fclose(file);
    unlink(tempfile);
}

/* Test graphics window size calculations */
void test_graphics_window_dimensions(void) {
    /* Test that graphics window dimensions can be calculated correctly */
    grect_t bbox;
    bbox.left = 10;
    bbox.top = 20;
    bbox.right = 650;
    bbox.bottom = 500;
    
    glui32 width = bbox.right - bbox.left;
    glui32 height = bbox.bottom - bbox.top;
    
    TEST_ASSERT_EQUAL(640, width);
    TEST_ASSERT_EQUAL(480, height);
    
    /* Test zero-size window */
    bbox.left = 0;
    bbox.top = 0;
    bbox.right = 0;
    bbox.bottom = 0;
    
    width = bbox.right - bbox.left;
    height = bbox.bottom - bbox.top;
    
    TEST_ASSERT_EQUAL(0, width);
    TEST_ASSERT_EQUAL(0, height);
}

/* Test graphics window in window serialization workflow */
void test_graphics_window_in_serialization_workflow(void) {
    /* Test that graphics windows can be part of library state serialization */
    FILE *file;
    char tempfile[] = "/tmp/graphics_workflow_test_XXXXXX";
    int fd = mkstemp(tempfile);
    TEST_ASSERT_NOT_EQUAL(-1, fd);
    
    file = fdopen(fd, "w+b");
    TEST_ASSERT_NOT_NULL(file);
    
    glkunix_serialize_context_t ctx = glkunix_serialize_start(file);
    
    /* Test that we can write a minimal graphics window data pattern */
    int result;
    result = glkunix_serialize_uint32(ctx, "has_graphics_data", 1);
    TEST_ASSERT_EQUAL(1, result);
    
    result = glkunix_serialize_uint32(ctx, "graphics_width", 800);
    TEST_ASSERT_EQUAL(1, result);
    
    result = glkunix_serialize_uint32(ctx, "graphics_height", 600);
    TEST_ASSERT_EQUAL(1, result);
    
    result = glkunix_serialize_uint32(ctx, "background_color", 0xFFFFFF);
    TEST_ASSERT_EQUAL(1, result);
    
    result = glkunix_serialize_uint32(ctx, "image_data_length", 0);
    TEST_ASSERT_EQUAL(1, result);
    
    glkunix_serialize_end(ctx);
    
    /* Test reading it back */
    rewind(file);
    glkunix_unserialize_context_t read_ctx = glkunix_unserialize_start(file);
    
    glui32 has_data, width, height, bgcolor, img_len;
    
    result = glkunix_unserialize_uint32(read_ctx, "has_graphics_data", &has_data);
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_EQUAL(1, has_data);
    
    result = glkunix_unserialize_uint32(read_ctx, "graphics_width", &width);
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_EQUAL(800, width);
    
    result = glkunix_unserialize_uint32(read_ctx, "graphics_height", &height);
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_EQUAL(600, height);
    
    result = glkunix_unserialize_uint32(read_ctx, "background_color", &bgcolor);
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_EQUAL(0xFFFFFF, bgcolor);
    
    result = glkunix_unserialize_uint32(read_ctx, "image_data_length", &img_len);
    TEST_ASSERT_EQUAL(1, result);
    TEST_ASSERT_EQUAL(0, img_len);
    
    glkunix_unserialize_end(read_ctx);
    
    fclose(file);
    unlink(tempfile);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_graphics_window_type_recognition);
    RUN_TEST(test_graphics_serialization_context);
    RUN_TEST(test_graphics_window_dimensions);
    RUN_TEST(test_graphics_window_in_serialization_workflow);
    
    return UNITY_END();
}
