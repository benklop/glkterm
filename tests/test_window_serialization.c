/* test_window_serialization.c: Unit tests for window data serialization */

#include "unity/unity.h"
#include "../glkterm/glk.h"
#include "../glkterm/glkterm.h"
#include "../glkterm/glkunix_autosave.h"
#include "../glkterm/gtw_buf.h"
#include "../glkterm/gtw_grid.h"
#include "../glkterm/gtw_pair.h"
#include "../glkterm/gtw_blnk.h"
#include <stdlib.h>
#include <string.h>

/* Test setup and teardown */
void setUp(void) {
    /* Initialize any needed state */
}

void tearDown(void) {
    /* Clean up any state */
}

/* Test window structure sizes and alignment */
void test_window_structure_sizes(void) {
    /* Verify structure sizes are reasonable */
    TEST_ASSERT_GREATER_THAN(0, sizeof(window_t));
    TEST_ASSERT_GREATER_THAN(0, sizeof(window_textbuffer_t));
    TEST_ASSERT_GREATER_THAN(0, sizeof(window_textgrid_t));
    TEST_ASSERT_GREATER_THAN(0, sizeof(window_pair_t));
    TEST_ASSERT_GREATER_THAN(0, sizeof(window_blank_t));
}

/* Test window type constants */
void test_window_type_constants(void) {
    TEST_ASSERT_EQUAL(1, wintype_Pair);
    TEST_ASSERT_EQUAL(2, wintype_Blank);
    TEST_ASSERT_EQUAL(3, wintype_TextBuffer);
    TEST_ASSERT_EQUAL(4, wintype_TextGrid);
    TEST_ASSERT_EQUAL(5, wintype_Graphics);
}

/* Test text buffer window structure initialization */
void test_textbuffer_initialization(void) {
    window_textbuffer_t tbdata;
    memset(&tbdata, 0, sizeof(tbdata));
    
    tbdata.width = 80;
    tbdata.height = 24;
    tbdata.numchars = 12;
    tbdata.chars = malloc(tbdata.numchars + 1);
    strcpy(tbdata.chars, "Hello World!");
    tbdata.dirtybeg = 0;
    tbdata.dirtyend = 12;
    
    TEST_ASSERT_EQUAL(80, tbdata.width);
    TEST_ASSERT_EQUAL(24, tbdata.height);
    TEST_ASSERT_EQUAL(12, tbdata.numchars);
    TEST_ASSERT_NOT_NULL(tbdata.chars);
    TEST_ASSERT_EQUAL_STRING("Hello World!", tbdata.chars);
    TEST_ASSERT_EQUAL(0, tbdata.dirtybeg);
    TEST_ASSERT_EQUAL(12, tbdata.dirtyend);
    
    free(tbdata.chars);
}

/* Test text grid window structure initialization */
void test_textgrid_initialization(void) {
    window_textgrid_t tgdata;
    memset(&tgdata, 0, sizeof(tgdata));
    
    tgdata.width = 80;
    tgdata.height = 24;
    tgdata.curx = 5;
    tgdata.cury = 10;
    tgdata.dirtybeg = 0;
    tgdata.dirtyend = 24;
    tgdata.inunicode = 0;
    
    TEST_ASSERT_EQUAL(80, tgdata.width);
    TEST_ASSERT_EQUAL(24, tgdata.height);
    TEST_ASSERT_EQUAL(5, tgdata.curx);
    TEST_ASSERT_EQUAL(10, tgdata.cury);
    TEST_ASSERT_EQUAL(0, tgdata.dirtybeg);
    TEST_ASSERT_EQUAL(24, tgdata.dirtyend);
    TEST_ASSERT_EQUAL(0, tgdata.inunicode);
}

/* Test pair window structure initialization */
void test_pair_initialization(void) {
    window_pair_t pairdata;
    memset(&pairdata, 0, sizeof(pairdata));
    
    pairdata.child1 = NULL;
    pairdata.child2 = NULL;
    pairdata.splitpos = 40;
    pairdata.splitwidth = 1;
    
    TEST_ASSERT_NULL(pairdata.child1);
    TEST_ASSERT_NULL(pairdata.child2);
    TEST_ASSERT_EQUAL(40, pairdata.splitpos);
    TEST_ASSERT_EQUAL(1, pairdata.splitwidth);
}

/* Test window structure basic fields */
void test_window_basic_fields(void) {
    window_t win;
    memset(&win, 0, sizeof(win));
    
    win.magicnum = 0x57696E57; /* 'WinW' */
    win.rock = 12345;
    win.type = wintype_TextBuffer;
    win.bbox.left = 0;
    win.bbox.top = 0;
    win.bbox.right = 80;
    win.bbox.bottom = 24;
    win.line_request = 0;
    win.char_request = 1;
    win.styleplus.style = style_Normal;
    
    TEST_ASSERT_EQUAL(0x57696E57, win.magicnum);
    TEST_ASSERT_EQUAL(12345, win.rock);
    TEST_ASSERT_EQUAL(wintype_TextBuffer, win.type);
    TEST_ASSERT_EQUAL(0, win.bbox.left);
    TEST_ASSERT_EQUAL(0, win.bbox.top);
    TEST_ASSERT_EQUAL(80, win.bbox.right);
    TEST_ASSERT_EQUAL(24, win.bbox.bottom);
    TEST_ASSERT_EQUAL(0, win.line_request);
    TEST_ASSERT_EQUAL(1, win.char_request);
    TEST_ASSERT_EQUAL(style_Normal, win.styleplus.style);
}

/* Test null pointer handling */
void test_null_pointer_handling(void) {
    window_textbuffer_t *null_tb = NULL;
    window_textgrid_t *null_tg = NULL;
    window_pair_t *null_pair = NULL;
    window_blank_t *null_blank = NULL;
    
    TEST_ASSERT_NULL(null_tb);
    TEST_ASSERT_NULL(null_tg);
    TEST_ASSERT_NULL(null_pair);
    TEST_ASSERT_NULL(null_blank);
}

/* Test data structure relationships */
void test_data_structure_relationships(void) {
    window_t win;
    window_textbuffer_t tbdata;
    
    memset(&win, 0, sizeof(win));
    memset(&tbdata, 0, sizeof(tbdata));
    
    tbdata.owner = &win;
    win.data = &tbdata;
    win.type = wintype_TextBuffer;
    
    TEST_ASSERT_EQUAL(&win, tbdata.owner);
    TEST_ASSERT_EQUAL(&tbdata, win.data);
    TEST_ASSERT_EQUAL(wintype_TextBuffer, win.type);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_window_structure_sizes);
    RUN_TEST(test_window_type_constants);
    RUN_TEST(test_textbuffer_initialization);
    RUN_TEST(test_textgrid_initialization);
    RUN_TEST(test_pair_initialization);
    RUN_TEST(test_window_basic_fields);
    RUN_TEST(test_null_pointer_handling);
    RUN_TEST(test_data_structure_relationships);
    
    return UNITY_END();
}
