#include "unity/unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../glkterm/glk.h"
#include "../glkterm/glkterm.h"
#include "../glkterm/glkunix_autosave.h"
#include "../glkterm/gtw_pair.h"

/* Test window hierarchy round-trip serialization and restoration */

void setUp(void) {
    /* Reset any global state before each test */
}

void tearDown(void) {
    /* Clean up after each test */
    glkunix_autosave_cleanup();
}

/* Test creating a simple hierarchy and verifying round-trip */
void test_simple_hierarchy_roundtrip(void)
{
    /* This test verifies that we can serialize and restore a simple window hierarchy */
    
    /* Create a simple window structure */
    window_t root_window;
    memset(&root_window, 0, sizeof(root_window));
    root_window.rock = 1000;
    root_window.type = wintype_Pair;
    
    window_t child1;
    memset(&child1, 0, sizeof(child1));
    child1.rock = 1001;
    child1.type = wintype_TextBuffer;
    child1.parent = &root_window;
    
    window_t child2;
    memset(&child2, 0, sizeof(child2));
    child2.rock = 1002;
    child2.type = wintype_TextGrid;
    child2.parent = &root_window;
    
    /* Create pair data for root window */
    window_pair_t pair_data;
    memset(&pair_data, 0, sizeof(pair_data));
    pair_data.owner = &root_window;
    pair_data.child1 = &child1;
    pair_data.child2 = &child2;
    pair_data.dir = winmethod_Left;
    pair_data.division = winmethod_Proportional;
    pair_data.size = 50;
    pair_data.hasborder = 1;
    pair_data.vertical = 0;
    pair_data.backward = 0;
    pair_data.splitpos = 40;
    pair_data.splitwidth = 1;
    
    root_window.data = &pair_data;
    
    /* Verify the hierarchy is set up correctly */
    TEST_ASSERT_EQUAL_PTR(&root_window, child1.parent);
    TEST_ASSERT_EQUAL_PTR(&root_window, child2.parent);
    TEST_ASSERT_EQUAL_PTR(&child1, pair_data.child1);
    TEST_ASSERT_EQUAL_PTR(&child2, pair_data.child2);
    
    /* Get update tags for verification */
    glui32 root_tag = glkunix_window_get_updatetag(&root_window);
    glui32 child1_tag = glkunix_window_get_updatetag(&child1);
    glui32 child2_tag = glkunix_window_get_updatetag(&child2);
    
    /* Verify tags are different */
    TEST_ASSERT_NOT_EQUAL(root_tag, child1_tag);
    TEST_ASSERT_NOT_EQUAL(root_tag, child2_tag);
    TEST_ASSERT_NOT_EQUAL(child1_tag, child2_tag);
    
    /* Verify we can find windows by tag */
    TEST_ASSERT_EQUAL_PTR(&root_window, glkunix_window_find_by_updatetag(root_tag));
    TEST_ASSERT_EQUAL_PTR(&child1, glkunix_window_find_by_updatetag(child1_tag));
    TEST_ASSERT_EQUAL_PTR(&child2, glkunix_window_find_by_updatetag(child2_tag));
}

/* Test that parent relationships are maintained during restoration */
void test_parent_relationship_restoration(void)
{
    /* Create windows with parent-child relationships */
    window_t parent;
    window_t child;
    
    memset(&parent, 0, sizeof(parent));
    memset(&child, 0, sizeof(child));
    
    parent.rock = 2000;
    parent.type = wintype_TextBuffer;
    
    child.rock = 2001;
    child.type = wintype_TextGrid;
    child.parent = &parent;
    
    /* Verify initial relationship */
    TEST_ASSERT_EQUAL_PTR(&parent, child.parent);
    
    /* Get tags */
    glui32 parent_tag = glkunix_window_get_updatetag(&parent);
    glui32 child_tag = glkunix_window_get_updatetag(&child);
    
    /* Verify tag lookup works both ways */
    TEST_ASSERT_EQUAL_PTR(&parent, glkunix_window_find_by_updatetag(parent_tag));
    TEST_ASSERT_EQUAL_PTR(&child, glkunix_window_find_by_updatetag(child_tag));
}

/* Test pair window child pointer restoration */
void test_pair_window_child_restoration(void)
{
    /* Create a pair window with children */
    window_t pair_win;
    window_t left_child;
    window_t right_child;
    
    memset(&pair_win, 0, sizeof(pair_win));
    memset(&left_child, 0, sizeof(left_child));
    memset(&right_child, 0, sizeof(right_child));
    
    pair_win.rock = 3000;
    pair_win.type = wintype_Pair;
    
    left_child.rock = 3001;
    left_child.type = wintype_TextBuffer;
    left_child.parent = &pair_win;
    
    right_child.rock = 3002;
    right_child.type = wintype_TextGrid;
    right_child.parent = &pair_win;
    
    /* Create pair data */
    window_pair_t pair_data;
    memset(&pair_data, 0, sizeof(pair_data));
    pair_data.owner = &pair_win;
    pair_data.child1 = &left_child;
    pair_data.child2 = &right_child;
    pair_data.dir = winmethod_Above;
    pair_data.division = winmethod_Fixed;
    pair_data.size = 200;
    
    pair_win.data = &pair_data;
    
    /* Verify pair window structure */
    TEST_ASSERT_EQUAL_PTR(&pair_win, pair_data.owner);
    TEST_ASSERT_EQUAL_PTR(&left_child, pair_data.child1);
    TEST_ASSERT_EQUAL_PTR(&right_child, pair_data.child2);
    TEST_ASSERT_EQUAL_INT(winmethod_Above, pair_data.dir);
    TEST_ASSERT_EQUAL_INT(winmethod_Fixed, pair_data.division);
    TEST_ASSERT_EQUAL_INT(200, pair_data.size);
    
    /* Verify children have correct parent */
    TEST_ASSERT_EQUAL_PTR(&pair_win, left_child.parent);
    TEST_ASSERT_EQUAL_PTR(&pair_win, right_child.parent);
}

/* Test complex three-level hierarchy */
void test_complex_hierarchy_structure(void)
{
    /* Create a three-level hierarchy:
     *   root_pair
     *   ├── left_leaf  
     *   └── right_pair
     *       ├── right_left_leaf
     *       └── right_right_leaf
     */
    
    window_t root_pair, left_leaf, right_pair, right_left_leaf, right_right_leaf;
    window_pair_t root_data, right_data;
    
    /* Initialize all windows */
    memset(&root_pair, 0, sizeof(root_pair));
    memset(&left_leaf, 0, sizeof(left_leaf));
    memset(&right_pair, 0, sizeof(right_pair));
    memset(&right_left_leaf, 0, sizeof(right_left_leaf));
    memset(&right_right_leaf, 0, sizeof(right_right_leaf));
    
    memset(&root_data, 0, sizeof(root_data));
    memset(&right_data, 0, sizeof(right_data));
    
    /* Set up window properties */
    root_pair.rock = 4000;
    root_pair.type = wintype_Pair;
    root_pair.data = &root_data;
    
    left_leaf.rock = 4001;
    left_leaf.type = wintype_TextBuffer;
    left_leaf.parent = &root_pair;
    
    right_pair.rock = 4002;
    right_pair.type = wintype_Pair;
    right_pair.parent = &root_pair;
    right_pair.data = &right_data;
    
    right_left_leaf.rock = 4003;
    right_left_leaf.type = wintype_TextGrid;
    right_left_leaf.parent = &right_pair;
    
    right_right_leaf.rock = 4004;
    right_right_leaf.type = wintype_TextBuffer;
    right_right_leaf.parent = &right_pair;
    
    /* Set up pair data */
    root_data.owner = &root_pair;
    root_data.child1 = &left_leaf;
    root_data.child2 = &right_pair;
    root_data.dir = winmethod_Left;
    
    right_data.owner = &right_pair;
    right_data.child1 = &right_left_leaf;
    right_data.child2 = &right_right_leaf;
    right_data.dir = winmethod_Above;
    
    /* Verify the complex hierarchy */
    TEST_ASSERT_EQUAL_PTR(&root_pair, left_leaf.parent);
    TEST_ASSERT_EQUAL_PTR(&root_pair, right_pair.parent);
    TEST_ASSERT_EQUAL_PTR(&right_pair, right_left_leaf.parent);
    TEST_ASSERT_EQUAL_PTR(&right_pair, right_right_leaf.parent);
    
    TEST_ASSERT_EQUAL_PTR(&left_leaf, root_data.child1);
    TEST_ASSERT_EQUAL_PTR(&right_pair, root_data.child2);
    TEST_ASSERT_EQUAL_PTR(&right_left_leaf, right_data.child1);
    TEST_ASSERT_EQUAL_PTR(&right_right_leaf, right_data.child2);
    
    /* Verify each window can be found by its tag */
    glui32 tags[5];
    tags[0] = glkunix_window_get_updatetag(&root_pair);
    tags[1] = glkunix_window_get_updatetag(&left_leaf);
    tags[2] = glkunix_window_get_updatetag(&right_pair);
    tags[3] = glkunix_window_get_updatetag(&right_left_leaf);
    tags[4] = glkunix_window_get_updatetag(&right_right_leaf);
    
    /* All tags should be different */
    for (int i = 0; i < 5; i++) {
        for (int j = i + 1; j < 5; j++) {
            TEST_ASSERT_NOT_EQUAL(tags[i], tags[j]);
        }
    }
    
    /* All windows should be findable by tag */
    TEST_ASSERT_EQUAL_PTR(&root_pair, glkunix_window_find_by_updatetag(tags[0]));
    TEST_ASSERT_EQUAL_PTR(&left_leaf, glkunix_window_find_by_updatetag(tags[1]));
    TEST_ASSERT_EQUAL_PTR(&right_pair, glkunix_window_find_by_updatetag(tags[2]));
    TEST_ASSERT_EQUAL_PTR(&right_left_leaf, glkunix_window_find_by_updatetag(tags[3]));
    TEST_ASSERT_EQUAL_PTR(&right_right_leaf, glkunix_window_find_by_updatetag(tags[4]));
}

/* Main test runner function */
int main(void)
{
    UNITY_BEGIN();
    
    RUN_TEST(test_simple_hierarchy_roundtrip);
    RUN_TEST(test_parent_relationship_restoration);
    RUN_TEST(test_pair_window_child_restoration);
    RUN_TEST(test_complex_hierarchy_structure);
    
    return UNITY_END();
}
