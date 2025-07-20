/* test_updatetags.c: Unit tests for the update tag system
   Tests the deterministic update tag functionality
*/

#include "../unity/unity.h"
#include "../../glkterm/glkunix_autosave.h"
#include "../../glkterm/glkterm.h"
#include <stdlib.h>
#include <string.h>

/* Mock GLK objects for testing */
typedef struct mock_window_struct {
    glui32 magicnum;
    glui32 rock;
    glui32 type;
    gidispatch_rock_t disprock;
} mock_window_t;

typedef struct mock_stream_struct {
    glui32 magicnum;
    glui32 rock;
    int type;
    gidispatch_rock_t disprock;
} mock_stream_t;

typedef struct mock_fileref_struct {
    glui32 magicnum;
    glui32 rock;
    gidispatch_rock_t disprock;
} mock_fileref_t;

void setUp(void)
{
    /* Clean up any existing update tags */
    glkunix_autosave_cleanup();
}

void tearDown(void)
{
    /* Clean up after each test */
    glkunix_autosave_cleanup();
}

void test_window_updatetag_deterministic(void)
{
    mock_window_t win1 = { .rock = 42, .type = 1 };
    mock_window_t win2 = { .rock = 42, .type = 1 };  /* Same rock and type */
    mock_window_t win3 = { .rock = 43, .type = 1 };  /* Different rock */
    
    /* Same rock+type should produce same tag */
    glui32 tag1 = glkunix_window_get_updatetag((winid_t)&win1);
    glui32 tag2 = glkunix_window_get_updatetag((winid_t)&win2);
    glui32 tag3 = glkunix_window_get_updatetag((winid_t)&win3);
    
    TEST_ASSERT_EQUAL_UINT32(tag1, tag2);
    TEST_ASSERT_NOT_EQUAL_UINT32(tag1, tag3);
    TEST_ASSERT_NOT_EQUAL_UINT32(0, tag1);
}

void test_stream_updatetag_deterministic(void)
{
    mock_stream_t str1 = { .rock = 100, .type = 2 };
    mock_stream_t str2 = { .rock = 100, .type = 2 };  /* Same rock and type */
    mock_stream_t str3 = { .rock = 101, .type = 2 };  /* Different rock */
    
    glui32 tag1 = glkunix_stream_get_updatetag((strid_t)&str1);
    glui32 tag2 = glkunix_stream_get_updatetag((strid_t)&str2);
    glui32 tag3 = glkunix_stream_get_updatetag((strid_t)&str3);
    
    TEST_ASSERT_EQUAL_UINT32(tag1, tag2);
    TEST_ASSERT_NOT_EQUAL_UINT32(tag1, tag3);
    TEST_ASSERT_NOT_EQUAL_UINT32(0, tag1);
}

void test_fileref_updatetag_deterministic(void)
{
    mock_fileref_t fref1 = { .rock = 200 };
    mock_fileref_t fref2 = { .rock = 200 };  /* Same rock */
    mock_fileref_t fref3 = { .rock = 201 };  /* Different rock */
    
    glui32 tag1 = glkunix_fileref_get_updatetag((frefid_t)&fref1);
    glui32 tag2 = glkunix_fileref_get_updatetag((frefid_t)&fref2);
    glui32 tag3 = glkunix_fileref_get_updatetag((frefid_t)&fref3);
    
    TEST_ASSERT_EQUAL_UINT32(tag1, tag2);
    TEST_ASSERT_NOT_EQUAL_UINT32(tag1, tag3);
    TEST_ASSERT_NOT_EQUAL_UINT32(0, tag1);
}

void test_updatetag_find_by_tag(void)
{
    mock_window_t win = { .rock = 500, .type = 1 };
    
    glui32 tag = glkunix_window_get_updatetag((winid_t)&win);
    winid_t found_win = glkunix_window_find_by_updatetag(tag);
    
    TEST_ASSERT_EQUAL_PTR(&win, found_win);
}

void test_updatetag_null_objects(void)
{
    /* NULL objects should return 0 tags */
    glui32 tag1 = glkunix_window_get_updatetag(NULL);
    glui32 tag2 = glkunix_stream_get_updatetag(NULL);
    glui32 tag3 = glkunix_fileref_get_updatetag(NULL);
    
    TEST_ASSERT_EQUAL_UINT32(0, tag1);
    TEST_ASSERT_EQUAL_UINT32(0, tag2);
    TEST_ASSERT_EQUAL_UINT32(0, tag3);
    
    /* Finding by tag 0 should return NULL */
    TEST_ASSERT_NULL(glkunix_window_find_by_updatetag(0));
    TEST_ASSERT_NULL(glkunix_stream_find_by_updatetag(0));
    TEST_ASSERT_NULL(glkunix_fileref_find_by_updatetag(0));
}

void test_updatetag_cross_session_consistency(void)
{
    mock_window_t win = { .rock = 999, .type = 1 };
    
    /* Get tag in first "session" */
    glui32 tag1 = glkunix_window_get_updatetag((winid_t)&win);
    
    /* Simulate session restart by cleaning up */
    glkunix_autosave_cleanup();
    
    /* Get tag in second "session" - should be same */
    glui32 tag2 = glkunix_window_get_updatetag((winid_t)&win);
    
    TEST_ASSERT_EQUAL_UINT32(tag1, tag2);
}

int main(void)
{
    UNITY_BEGIN();
    
    RUN_TEST(test_window_updatetag_deterministic);
    RUN_TEST(test_stream_updatetag_deterministic);
    RUN_TEST(test_fileref_updatetag_deterministic);
    RUN_TEST(test_updatetag_find_by_tag);
    RUN_TEST(test_updatetag_null_objects);
    RUN_TEST(test_updatetag_cross_session_consistency);
    
    return UNITY_END();
}
