#include "unity/unity.h"
#include <string.h>
#include "../glkterm/glk.h"
#include "../glkterm/glkterm.h"
#include "../glkterm/glkunix_autosave.h"

/* Mock implementation for the test */
extern void* glkunix_get_filereflist(void);

/* Test that we can determine fileref properties */
void test_fileref_properties(void)
{
    /* Verify fileref structure fields exist and can be accessed */
    fileref_t test_fileref;
    test_fileref.rock = 123;
    test_fileref.filename = "test_file.dat";
    test_fileref.filetype = fileusage_Data;
    test_fileref.textmode = 0; /* binary mode */
    
    TEST_ASSERT_EQUAL_INT(123, test_fileref.rock);
    TEST_ASSERT_EQUAL_STRING("test_file.dat", test_fileref.filename);
    TEST_ASSERT_EQUAL_INT(fileusage_Data, test_fileref.filetype);
    TEST_ASSERT_EQUAL_INT(0, test_fileref.textmode);
}

void test_fileref_text_mode(void)
{
    fileref_t text_fileref;
    text_fileref.filename = "story.txt";
    text_fileref.filetype = fileusage_Transcript;
    text_fileref.textmode = 1; /* text mode */
    text_fileref.rock = 456;
    
    TEST_ASSERT_EQUAL_STRING("story.txt", text_fileref.filename);
    TEST_ASSERT_EQUAL_INT(fileusage_Transcript, text_fileref.filetype);
    TEST_ASSERT_EQUAL_INT(1, text_fileref.textmode);
    TEST_ASSERT_EQUAL_INT(456, text_fileref.rock);
}

void test_fileref_usage_types(void)
{
    /* Test that file usage constants are accessible */
    TEST_ASSERT_NOT_EQUAL(fileusage_Data, fileusage_SavedGame);
    TEST_ASSERT_NOT_EQUAL(fileusage_Transcript, fileusage_InputRecord);
    
    /* Test specific usage type in a fileref */
    fileref_t save_fileref;
    save_fileref.filetype = fileusage_SavedGame;
    save_fileref.textmode = 0; /* save games are typically binary */
    save_fileref.filename = "save001.glk";
    save_fileref.rock = 789;
    
    TEST_ASSERT_EQUAL_INT(fileusage_SavedGame, save_fileref.filetype);
    TEST_ASSERT_EQUAL_INT(0, save_fileref.textmode);
    TEST_ASSERT_EQUAL_STRING("save001.glk", save_fileref.filename);
    TEST_ASSERT_EQUAL_INT(789, save_fileref.rock);
}

void test_fileref_null_filename(void)
{
    /* Test handling of NULL filename */
    fileref_t null_fileref;
    null_fileref.filename = NULL;
    null_fileref.filetype = fileusage_Data;
    null_fileref.textmode = 1;
    null_fileref.rock = 999;
    
    TEST_ASSERT_NULL(null_fileref.filename);
    TEST_ASSERT_EQUAL_INT(fileusage_Data, null_fileref.filetype);
    TEST_ASSERT_EQUAL_INT(1, null_fileref.textmode);
    TEST_ASSERT_EQUAL_INT(999, null_fileref.rock);
}

void test_fileref_structure_integrity(void)
{
    /* Test that we can create and modify fileref structures */
    fileref_t fref1, fref2;
    
    /* Set up first fileref */
    fref1.rock = 100;
    fref1.filename = "input.log";
    fref1.filetype = fileusage_InputRecord;
    fref1.textmode = 1;
    
    /* Set up second fileref */
    fref2.rock = 200;
    fref2.filename = "game.dat";
    fref2.filetype = fileusage_Data;
    fref2.textmode = 0;
    
    /* Verify they're independent */
    TEST_ASSERT_NOT_EQUAL(fref1.rock, fref2.rock);
    TEST_ASSERT_FALSE(strcmp(fref1.filename, fref2.filename) == 0);
    TEST_ASSERT_NOT_EQUAL(fref1.filetype, fref2.filetype);
    TEST_ASSERT_NOT_EQUAL(fref1.textmode, fref2.textmode);
    
    /* Verify individual values */
    TEST_ASSERT_EQUAL_INT(100, fref1.rock);
    TEST_ASSERT_EQUAL_STRING("input.log", fref1.filename);
    TEST_ASSERT_EQUAL_INT(fileusage_InputRecord, fref1.filetype);
    TEST_ASSERT_EQUAL_INT(1, fref1.textmode);
    
    TEST_ASSERT_EQUAL_INT(200, fref2.rock);
    TEST_ASSERT_EQUAL_STRING("game.dat", fref2.filename);
    TEST_ASSERT_EQUAL_INT(fileusage_Data, fref2.filetype);
    TEST_ASSERT_EQUAL_INT(0, fref2.textmode);
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
    
    RUN_TEST(test_fileref_properties);
    RUN_TEST(test_fileref_text_mode);
    RUN_TEST(test_fileref_usage_types);
    RUN_TEST(test_fileref_null_filename);
    RUN_TEST(test_fileref_structure_integrity);
    
    return UNITY_END();
}
