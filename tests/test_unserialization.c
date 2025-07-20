#include "unity/unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../glkterm/glk.h"
#include "../glkterm/glkterm.h"
#include "../glkterm/glkunix_autosave.h"

/* Mock file context for testing unserialization functions */

void test_unserialize_library_state_version_check(void)
{
    /* Create a temporary file for testing */
    FILE *temp_file = tmpfile();
    TEST_ASSERT_NOT_NULL(temp_file);
    
    /* Write a version number in big-endian format like serialization does */
    glui32 version = 1;
    unsigned char version_bytes[4];
    version_bytes[0] = (version >> 24) & 0xFF;
    version_bytes[1] = (version >> 16) & 0xFF;
    version_bytes[2] = (version >> 8) & 0xFF;
    version_bytes[3] = version & 0xFF;
    fwrite(version_bytes, 4, 1, temp_file);
    
    /* Write empty counts for objects in big-endian format */
    glui32 zero_count = 0;
    unsigned char zero_bytes[4] = {0, 0, 0, 0};
    fwrite(zero_bytes, 4, 1, temp_file); /* window count */
    fwrite(zero_bytes, 4, 1, temp_file); /* stream count */
    fwrite(zero_bytes, 4, 1, temp_file); /* fileref count */
    
    /* Rewind to beginning */
    rewind(temp_file);
    
    /* Create unserialization context */
    glkunix_unserialize_context_t context;
    context.file = temp_file;
    context.read_count = NULL;
    
    /* Test unserialization */
    int result = glkunix_unserialize_library_state(context);
    TEST_ASSERT_EQUAL_INT(1, result);
    
    fclose(temp_file);
}

void test_unserialize_library_state_invalid_version(void)
{
    /* Create a temporary file with invalid version */
    FILE *temp_file = tmpfile();
    TEST_ASSERT_NOT_NULL(temp_file);
    
    /* Write an invalid version number in big-endian format */
    glui32 version = 99; /* Unsupported version */
    unsigned char version_bytes[4];
    version_bytes[0] = (version >> 24) & 0xFF;
    version_bytes[1] = (version >> 16) & 0xFF;
    version_bytes[2] = (version >> 8) & 0xFF;
    version_bytes[3] = version & 0xFF;
    fwrite(version_bytes, 4, 1, temp_file);
    
    /* Rewind to beginning */
    rewind(temp_file);
    
    /* Create unserialization context */
    glkunix_unserialize_context_t context;
    context.file = temp_file;
    context.read_count = NULL;
    
    /* Test unserialization - should fail */
    int result = glkunix_unserialize_library_state(context);
    TEST_ASSERT_EQUAL_INT(0, result);
    
    fclose(temp_file);
}

void test_unserialize_uint32_basic(void)
{
    /* Create a temporary file for testing */
    FILE *temp_file = tmpfile();
    TEST_ASSERT_NOT_NULL(temp_file);
    
    /* Write test data in big-endian format */
    glui32 test_value = 0x12345678;
    unsigned char bytes[4];
    bytes[0] = (test_value >> 24) & 0xFF;
    bytes[1] = (test_value >> 16) & 0xFF;
    bytes[2] = (test_value >> 8) & 0xFF;
    bytes[3] = test_value & 0xFF;
    fwrite(bytes, 4, 1, temp_file);
    
    /* Rewind to beginning */
    rewind(temp_file);
    
    /* Create context */
    glkunix_unserialize_context_t context;
    context.file = temp_file;
    context.read_count = NULL;
    
    /* Read back the value */
    glui32 read_value;
    int result = glkunix_unserialize_uint32(context, "test_value", &read_value);
    
    TEST_ASSERT_EQUAL_INT(1, result);
    TEST_ASSERT_EQUAL_HEX32(test_value, read_value);
    
    fclose(temp_file);
}

void test_unserialize_buffer_basic(void)
{
    /* Create a temporary file for testing */
    FILE *temp_file = tmpfile();
    TEST_ASSERT_NOT_NULL(temp_file);
    
    /* Write test buffer in the format that serialization uses */
    const char *test_string = "Hello, World!";
    glui32 length = strlen(test_string);
    
    /* Write length in big-endian format */
    unsigned char length_bytes[4];
    length_bytes[0] = (length >> 24) & 0xFF;
    length_bytes[1] = (length >> 16) & 0xFF;
    length_bytes[2] = (length >> 8) & 0xFF;
    length_bytes[3] = length & 0xFF;
    fwrite(length_bytes, 4, 1, temp_file);
    
    /* Write buffer data */
    fwrite(test_string, 1, length, temp_file);
    
    /* Rewind to beginning */
    rewind(temp_file);
    
    /* Create context */
    glkunix_unserialize_context_t context;
    context.file = temp_file;
    context.read_count = NULL;
    
    /* Read back the buffer */
    char read_buffer[64];
    int result = glkunix_unserialize_buffer(context, "test_buffer", read_buffer, length);
    
    TEST_ASSERT_EQUAL_INT(1, result);
    TEST_ASSERT_EQUAL_MEMORY(test_string, read_buffer, length);
    
    fclose(temp_file);
}

void test_unserialize_buffer_length_mismatch(void)
{
    /* Create a temporary file for testing */
    FILE *temp_file = tmpfile();
    TEST_ASSERT_NOT_NULL(temp_file);
    
    /* Write buffer with different length than expected */
    glui32 stored_length = 10;
    const char *test_data = "1234567890";
    
    /* Write length in big-endian format */
    unsigned char length_bytes[4];
    length_bytes[0] = (stored_length >> 24) & 0xFF;
    length_bytes[1] = (stored_length >> 16) & 0xFF;
    length_bytes[2] = (stored_length >> 8) & 0xFF;
    length_bytes[3] = stored_length & 0xFF;
    fwrite(length_bytes, 4, 1, temp_file);
    
    fwrite(test_data, 1, stored_length, temp_file);
    
    /* Rewind to beginning */
    rewind(temp_file);
    
    /* Create context */
    glkunix_unserialize_context_t context;
    context.file = temp_file;
    context.read_count = NULL;
    
    /* Try to read with wrong expected length */
    char read_buffer[64];
    int result = glkunix_unserialize_buffer(context, "test_buffer", read_buffer, 5); /* Wrong length */
    
    TEST_ASSERT_EQUAL_INT(0, result); /* Should fail */
    
    fclose(temp_file);
}

void test_unserialize_empty_object_lists(void)
{
    /* Create a temporary file with empty object lists */
    FILE *temp_file = tmpfile();
    TEST_ASSERT_NOT_NULL(temp_file);
    
    /* Write version and empty counts in big-endian format */
    glui32 version = 1;
    glui32 zero_count = 0;
    
    /* Helper function to write uint32 in big-endian */
    auto void write_uint32(glui32 value) {
        unsigned char bytes[4];
        bytes[0] = (value >> 24) & 0xFF;
        bytes[1] = (value >> 16) & 0xFF;
        bytes[2] = (value >> 8) & 0xFF;
        bytes[3] = value & 0xFF;
        fwrite(bytes, 4, 1, temp_file);
    }
    
    write_uint32(version);      /* version */
    write_uint32(zero_count);   /* window count */
    write_uint32(zero_count);   /* stream count */
    write_uint32(zero_count);   /* fileref count */
    
    /* Rewind to beginning */
    rewind(temp_file);
    
    /* Create context */
    glkunix_unserialize_context_t context;
    context.file = temp_file;
    context.read_count = NULL;
    
    /* Test library state unserialization */
    int result = glkunix_unserialize_library_state(context);
    TEST_ASSERT_EQUAL_INT(1, result);
    
    fclose(temp_file);
}

void setUp(void)
{
    /* Setup function called before each test */
}

void tearDown(void)
{
    /* Teardown function called after each test */
}

int main(void)
{
    UNITY_BEGIN();
    
    RUN_TEST(test_unserialize_library_state_version_check);
    RUN_TEST(test_unserialize_library_state_invalid_version);
    RUN_TEST(test_unserialize_uint32_basic);
    RUN_TEST(test_unserialize_buffer_basic);
    RUN_TEST(test_unserialize_buffer_length_mismatch);
    RUN_TEST(test_unserialize_empty_object_lists);
    
    return UNITY_END();
}
