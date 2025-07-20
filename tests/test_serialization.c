/* test_serialization.c: Unit tests for basic serialization functions
   Tests the endian-safe serialization system
*/

#include "../unity/unity.h"
#include "../../glkterm/glkunix_autosave.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static FILE *test_file = NULL;

void setUp(void)
{
    /* Create temporary file for testing */
    test_file = tmpfile();
    if (!test_file) {
        TEST_FAIL_MESSAGE("Could not create temporary test file");
    }
}

void tearDown(void)
{
    if (test_file) {
        fclose(test_file);
        test_file = NULL;
    }
}

void test_serialize_uint32_endianness(void)
{
    glui32 write_count = 0;
    glkunix_serialize_context_t ctx = { .file = test_file, .write_count = &write_count };
    
    /* Test serializing a value with known byte pattern */
    glui32 test_value = 0x12345678;
    int result = glkunix_serialize_uint32(ctx, "test", test_value);
    
    TEST_ASSERT_EQUAL_INT(1, result);
    TEST_ASSERT_EQUAL_UINT32(4, write_count);
    
    /* Check that it's written in big-endian format */
    fseek(test_file, 0, SEEK_SET);
    unsigned char bytes[4];
    size_t read_count = fread(bytes, 1, 4, test_file);
    
    TEST_ASSERT_EQUAL_size_t(4, read_count);
    TEST_ASSERT_EQUAL_HEX8(0x12, bytes[0]);
    TEST_ASSERT_EQUAL_HEX8(0x34, bytes[1]);
    TEST_ASSERT_EQUAL_HEX8(0x56, bytes[2]);
    TEST_ASSERT_EQUAL_HEX8(0x78, bytes[3]);
}

void test_unserialize_uint32_endianness(void)
{
    /* Write known big-endian bytes */
    unsigned char bytes[4] = { 0xAB, 0xCD, 0xEF, 0x01 };
    fwrite(bytes, 1, 4, test_file);
    fseek(test_file, 0, SEEK_SET);
    
    glui32 read_count = 0;
    glkunix_unserialize_context_t ctx = { .file = test_file, .read_count = &read_count };
    
    glui32 value;
    int result = glkunix_unserialize_uint32(ctx, "test", &value);
    
    TEST_ASSERT_EQUAL_INT(1, result);
    TEST_ASSERT_EQUAL_UINT32(4, read_count);
    TEST_ASSERT_EQUAL_HEX32(0xABCDEF01, value);
}

void test_serialize_roundtrip_uint32(void)
{
    glui32 write_count = 0;
    glkunix_serialize_context_t write_ctx = { .file = test_file, .write_count = &write_count };
    
    glui32 original_values[] = { 0, 1, 0xFFFFFFFF, 0x12345678, 0x80000000 };
    int num_values = sizeof(original_values) / sizeof(original_values[0]);
    
    /* Serialize all values */
    for (int i = 0; i < num_values; i++) {
        int result = glkunix_serialize_uint32(write_ctx, "test", original_values[i]);
        TEST_ASSERT_EQUAL_INT(1, result);
    }
    
    /* Reset file position */
    fseek(test_file, 0, SEEK_SET);
    
    /* Unserialize and verify */
    glui32 read_count = 0;
    glkunix_unserialize_context_t read_ctx = { .file = test_file, .read_count = &read_count };
    
    for (int i = 0; i < num_values; i++) {
        glui32 value;
        int result = glkunix_unserialize_uint32(read_ctx, "test", &value);
        TEST_ASSERT_EQUAL_INT(1, result);
        TEST_ASSERT_EQUAL_HEX32(original_values[i], value);
    }
    
    TEST_ASSERT_EQUAL_UINT32(num_values * 4, write_count);
    TEST_ASSERT_EQUAL_UINT32(num_values * 4, read_count);
}

void test_serialize_buffer(void)
{
    glui32 write_count = 0;
    glkunix_serialize_context_t ctx = { .file = test_file, .write_count = &write_count };
    
    char test_data[] = "Hello, GLK Autosave!";
    glui32 data_len = strlen(test_data);
    
    int result = glkunix_serialize_buffer(ctx, "buffer", test_data, data_len);
    TEST_ASSERT_EQUAL_INT(1, result);
    TEST_ASSERT_EQUAL_UINT32(4 + data_len, write_count); /* length + data */
}

void test_unserialize_buffer(void)
{
    char original_data[] = "Test Buffer Data";
    glui32 data_len = strlen(original_data);
    
    /* Manually write length + data */
    glui32 write_count = 0;
    glkunix_serialize_context_t write_ctx = { .file = test_file, .write_count = &write_count };
    glkunix_serialize_buffer(write_ctx, "buffer", original_data, data_len);
    
    /* Reset and read back */
    fseek(test_file, 0, SEEK_SET);
    
    char read_buffer[100];
    memset(read_buffer, 0, sizeof(read_buffer));
    
    glui32 read_count = 0;
    glkunix_unserialize_context_t read_ctx = { .file = test_file, .read_count = &read_count };
    
    int result = glkunix_unserialize_buffer(read_ctx, "buffer", read_buffer, data_len);
    TEST_ASSERT_EQUAL_INT(1, result);
    TEST_ASSERT_EQUAL_STRING(original_data, read_buffer);
}

void test_serialize_buffer_empty(void)
{
    glui32 write_count = 0;
    glkunix_serialize_context_t ctx = { .file = test_file, .write_count = &write_count };
    
    int result = glkunix_serialize_buffer(ctx, "empty", NULL, 0);
    TEST_ASSERT_EQUAL_INT(1, result);
    TEST_ASSERT_EQUAL_UINT32(4, write_count); /* Just the length field */
}

void test_serialize_error_handling(void)
{
    /* Test with null file pointer to test error handling */
    glui32 write_count = 0;
    glkunix_serialize_context_t ctx = { .file = NULL, .write_count = &write_count };
    
    int result = glkunix_serialize_uint32(ctx, "test", 42);
    TEST_ASSERT_EQUAL_INT(0, result); /* Should fail */
    
    /* Note: We can't easily test file write failures without complex mocking,
     * so we just test the null pointer case here */
}

int main(void)
{
    UNITY_BEGIN();
    
    RUN_TEST(test_serialize_uint32_endianness);
    RUN_TEST(test_unserialize_uint32_endianness);
    RUN_TEST(test_serialize_roundtrip_uint32);
    RUN_TEST(test_serialize_buffer);
    RUN_TEST(test_unserialize_buffer);
    RUN_TEST(test_serialize_buffer_empty);
    RUN_TEST(test_serialize_error_handling);
    
    return UNITY_END();
}
