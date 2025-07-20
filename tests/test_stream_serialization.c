#include "unity/unity.h"
#include "../glkterm/glk.h"
#include "../glkterm/glkterm.h"
#include "../glkterm/glkunix_autosave.h"

/* Mock implementation for the test */
extern void* glkunix_get_streamlist(void);

/* Test that we can determine stream types and properties */
void test_stream_type_properties(void)
{
    /* Test stream type values are as expected */
    TEST_ASSERT_EQUAL_INT(1, strtype_File);
    TEST_ASSERT_EQUAL_INT(2, strtype_Window);
    TEST_ASSERT_EQUAL_INT(3, strtype_Memory);
    TEST_ASSERT_EQUAL_INT(4, strtype_Resource);
    
    /* Verify stream structure fields exist and can be accessed */
    stream_t test_stream;
    test_stream.type = strtype_Window;
    test_stream.rock = 42;
    test_stream.unicode = 1;
    test_stream.readable = 1;
    test_stream.writable = 0;
    test_stream.readcount = 100;
    test_stream.writecount = 50;
    
    TEST_ASSERT_EQUAL_INT(strtype_Window, test_stream.type);
    TEST_ASSERT_EQUAL_INT(42, test_stream.rock);
    TEST_ASSERT_EQUAL_INT(1, test_stream.unicode);
    TEST_ASSERT_EQUAL_INT(1, test_stream.readable);
    TEST_ASSERT_EQUAL_INT(0, test_stream.writable);
    TEST_ASSERT_EQUAL_INT(100, test_stream.readcount);
    TEST_ASSERT_EQUAL_INT(50, test_stream.writecount);
}

void test_memory_stream_properties(void)
{
    stream_t mem_stream;
    mem_stream.type = strtype_Memory;
    mem_stream.buflen = 1024;
    mem_stream.isbinary = 0;
    
    /* Test that memory stream specific fields are accessible */
    TEST_ASSERT_EQUAL_INT(strtype_Memory, mem_stream.type);
    TEST_ASSERT_EQUAL_INT(1024, mem_stream.buflen);
    TEST_ASSERT_EQUAL_INT(0, mem_stream.isbinary);
    
    /* Test pointer offset calculations would work */
    char buffer[1024];
    mem_stream.buf = buffer;
    mem_stream.bufptr = buffer + 100;
    mem_stream.bufend = buffer + 500;
    mem_stream.bufeof = buffer + 400;
    
    /* Calculate offsets like the serialization code does */
    glui32 ptr_offset = mem_stream.bufptr ? (mem_stream.bufptr - mem_stream.buf) : 0;
    glui32 end_offset = mem_stream.bufend ? (mem_stream.bufend - mem_stream.buf) : 0;
    glui32 eof_offset = mem_stream.bufeof ? (mem_stream.bufeof - mem_stream.buf) : 0;
    
    TEST_ASSERT_EQUAL_INT(100, ptr_offset);
    TEST_ASSERT_EQUAL_INT(500, end_offset);
    TEST_ASSERT_EQUAL_INT(400, eof_offset);
}

void test_unicode_memory_stream_properties(void)
{
    stream_t unicode_stream;
    unicode_stream.type = strtype_Memory;
    unicode_stream.unicode = 1;
    unicode_stream.buflen = 512;
    
    /* Test unicode buffer offset calculations */
    glui32 unicode_buffer[512];
    unicode_stream.ubuf = unicode_buffer;
    unicode_stream.ubufptr = unicode_buffer + 50;
    unicode_stream.ubufend = unicode_buffer + 250;
    unicode_stream.ubufeof = unicode_buffer + 200;
    
    /* Calculate unicode offsets like the serialization code does */
    glui32 ptr_offset = unicode_stream.ubufptr ? (unicode_stream.ubufptr - unicode_stream.ubuf) : 0;
    glui32 end_offset = unicode_stream.ubufend ? (unicode_stream.ubufend - unicode_stream.ubuf) : 0;
    glui32 eof_offset = unicode_stream.ubufeof ? (unicode_stream.ubufeof - unicode_stream.ubuf) : 0;
    
    TEST_ASSERT_EQUAL_INT(50, ptr_offset);
    TEST_ASSERT_EQUAL_INT(250, end_offset);
    TEST_ASSERT_EQUAL_INT(200, eof_offset);
}

void test_file_stream_properties(void)
{
    stream_t file_stream;
    file_stream.type = strtype_File;
    file_stream.filename = "test_file.txt";
    file_stream.lastop = 1; /* Some operation flag */
    
    TEST_ASSERT_EQUAL_INT(strtype_File, file_stream.type);
    TEST_ASSERT_EQUAL_STRING("test_file.txt", file_stream.filename);
    TEST_ASSERT_EQUAL_INT(1, file_stream.lastop);
}

void test_window_stream_properties(void)
{
    stream_t window_stream;
    window_t test_window;
    
    window_stream.type = strtype_Window;
    window_stream.win = &test_window;
    
    TEST_ASSERT_EQUAL_INT(strtype_Window, window_stream.type);
    TEST_ASSERT_EQUAL_PTR(&test_window, window_stream.win);
}

void test_resource_stream_properties(void)
{
    stream_t resource_stream;
    resource_stream.type = strtype_Resource;
    resource_stream.isbinary = 1;
    
    TEST_ASSERT_EQUAL_INT(strtype_Resource, resource_stream.type);
    TEST_ASSERT_EQUAL_INT(1, resource_stream.isbinary);
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
    
    RUN_TEST(test_stream_type_properties);
    RUN_TEST(test_memory_stream_properties);
    RUN_TEST(test_unicode_memory_stream_properties);
    RUN_TEST(test_file_stream_properties);
    RUN_TEST(test_window_stream_properties);
    RUN_TEST(test_resource_stream_properties);
    
    return UNITY_END();
}
