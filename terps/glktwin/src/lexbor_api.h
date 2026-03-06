/* lexbor_api.h: Lexbor API declarations for use outside lexbor_include.c.
 * Only lexbor_include.c includes the full lexbor_single.h amalgamation.
 * All other translation units include this header for type/function access.
 */

#ifndef GLKTWIN_LEXBOR_API_H
#define GLKTWIN_LEXBOR_API_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef unsigned char lxb_char_t;
typedef unsigned int  lxb_status_t;
typedef uintptr_t     lxb_tag_id_t;

#define LXB_STATUS_OK 0x0000

typedef struct lxb_dom_document       lxb_dom_document_t;
typedef struct lxb_dom_node           lxb_dom_node_t;
typedef struct lxb_dom_element        lxb_dom_element_t;
typedef struct lxb_dom_character_data lxb_dom_character_data_t;
typedef struct lxb_dom_text           lxb_dom_text_t;
typedef struct lxb_dom_attr           lxb_dom_attr_t;
typedef struct lxb_html_document      lxb_html_document_t;

typedef unsigned int lxb_dom_node_type_t;
#define LXB_DOM_NODE_TYPE_ELEMENT                  0x01
#define LXB_DOM_NODE_TYPE_ATTRIBUTE                0x02
#define LXB_DOM_NODE_TYPE_TEXT                      0x03
#define LXB_DOM_NODE_TYPE_CDATA_SECTION            0x04
#define LXB_DOM_NODE_TYPE_PROCESSING_INSTRUCTION   0x07
#define LXB_DOM_NODE_TYPE_COMMENT                  0x08
#define LXB_DOM_NODE_TYPE_DOCUMENT                 0x09
#define LXB_DOM_NODE_TYPE_DOCUMENT_TYPE            0x0A
#define LXB_DOM_NODE_TYPE_DOCUMENT_FRAGMENT        0x0B

#define lxb_dom_interface_element(obj) ((lxb_dom_element_t *)(obj))
#define lxb_dom_interface_node(obj) ((lxb_dom_node_t *)(obj))

/* --- Node tree traversal (_noi = non-inline exports) --- */
extern lxb_dom_node_t      *lxb_dom_node_first_child_noi(lxb_dom_node_t *node);
extern lxb_dom_node_t      *lxb_dom_node_last_child_noi(lxb_dom_node_t *node);
extern lxb_dom_node_t      *lxb_dom_node_next_noi(lxb_dom_node_t *node);
extern lxb_dom_node_t      *lxb_dom_node_prev_noi(lxb_dom_node_t *node);
extern lxb_dom_node_t      *lxb_dom_node_parent_noi(lxb_dom_node_t *node);
extern lxb_dom_node_type_t  lxb_dom_node_type_noi(lxb_dom_node_t *node);
extern lxb_tag_id_t         lxb_dom_node_tag_id_noi(lxb_dom_node_t *node);

/* --- Document root & element lookup --- */
extern lxb_dom_node_t    *lxb_dom_document_root(lxb_dom_document_t *document);
extern lxb_dom_element_t *lxb_dom_element_by_id(lxb_dom_element_t *root,
                              const lxb_char_t *qualified_name, size_t len);

/* --- Document element creation --- */
extern lxb_dom_element_t *lxb_dom_document_create_element(lxb_dom_document_t *document,
                              const lxb_char_t *local_name, size_t lname_len,
                              void *reserved_for_opt);
extern void               lxb_dom_document_destroy_element(lxb_dom_element_t *element);
extern lxb_dom_text_t    *lxb_dom_document_create_text_node(lxb_dom_document_t *document,
                              const lxb_char_t *data, size_t len);
extern void               lxb_dom_character_data_interface_destroy(
                              lxb_dom_character_data_t *character_data);

/* --- HTML document body/head --- */
extern lxb_dom_element_t *lxb_html_document_body_element_noi(lxb_html_document_t *document);
extern lxb_dom_element_t *lxb_html_document_head_element_noi(lxb_html_document_t *document);

/* --- Node tree manipulation --- */
extern void lxb_dom_node_insert_child(lxb_dom_node_t *to, lxb_dom_node_t *node);
extern void lxb_dom_node_insert_before(lxb_dom_node_t *to, lxb_dom_node_t *node);
extern void lxb_dom_node_insert_after(lxb_dom_node_t *to, lxb_dom_node_t *node);
extern void lxb_dom_node_remove(lxb_dom_node_t *node);

/* --- Element attributes --- */
extern lxb_dom_attr_t    *lxb_dom_element_set_attribute(lxb_dom_element_t *element,
                              const lxb_char_t *qualified_name, size_t qn_len,
                              const lxb_char_t *value, size_t value_len);
extern const lxb_char_t  *lxb_dom_element_get_attribute(lxb_dom_element_t *element,
                              const lxb_char_t *qualified_name, size_t qn_len,
                              size_t *value_len);
extern lxb_status_t       lxb_dom_element_remove_attribute(lxb_dom_element_t *element,
                              const lxb_char_t *qualified_name, size_t qn_len);
extern bool               lxb_dom_element_has_attribute(lxb_dom_element_t *element,
                              const lxb_char_t *qualified_name, size_t qn_len);
extern lxb_dom_attr_t    *lxb_dom_element_first_attribute_noi(lxb_dom_element_t *element);
extern const lxb_char_t  *lxb_dom_attr_local_name_noi(lxb_dom_attr_t *attr, size_t *len);
extern const lxb_char_t  *lxb_dom_attr_value_noi(lxb_dom_attr_t *attr, size_t *len);
extern const lxb_char_t  *lxb_dom_attr_qualified_name(lxb_dom_attr_t *attr, size_t *len);
extern lxb_dom_attr_t    *lxb_dom_element_next_attribute_noi(lxb_dom_attr_t *attr);

/* --- Element name --- */
extern const lxb_char_t  *lxb_dom_element_qualified_name(lxb_dom_element_t *element,
                              size_t *len);
extern const lxb_char_t  *lxb_dom_element_local_name(lxb_dom_element_t *element,
                              size_t *len);

/* --- Node text content (Lexbor built-in, recursive) --- */
extern lxb_char_t    *lxb_dom_node_text_content(lxb_dom_node_t *node, size_t *len);
extern lxb_status_t   lxb_dom_node_text_content_set(lxb_dom_node_t *node,
                          const lxb_char_t *content, size_t len);

/* --- Character data wrappers (text/comment node value access) --- */
extern const char *glktwin_text_node_data(lxb_dom_node_t *node, size_t *out_len);
extern bool        glktwin_text_node_set_data(lxb_dom_node_t *node,
                       const char *str, size_t len);

/* ========== glktwin wrapper functions (implemented in lexbor_include.c) ========== */

/* Parse full HTML string into new document, or NULL on failure. */
extern void  *glktwin_lexbor_parse_html(const char *html, size_t len);
extern void   glktwin_lexbor_document_destroy(void *doc);

/* Copy a single text node's character data into buf; returns bytes written. */
extern size_t glktwin_lexbor_text_content(lxb_dom_node_t *node, char *buf, size_t cap);

/* Serialize a node's children as HTML (innerHTML getter). Caller must free(). */
extern char  *glktwin_serialize_inner_html(lxb_dom_node_t *node, size_t *out_len);

/* Parse HTML fragment and replace all children of `element`. */
extern bool   glktwin_set_inner_html(lxb_dom_element_t *element, void *doc,
                  const char *html, size_t len);

/* Recursive text content extraction (all descendant text nodes). Caller must free(). */
extern char  *glktwin_get_text_content(lxb_dom_node_t *node, size_t *out_len);

/* Get element tag name as uppercase C string (e.g. "DIV"). Static buffer, do not free. */
extern const char *glktwin_get_tag_name(lxb_dom_node_t *node);

/* querySelector / querySelectorAll wrappers. Results array caller must free(). */
extern lxb_dom_node_t  *glktwin_query_selector(void *doc, lxb_dom_node_t *root,
                            const char *selector);
extern lxb_dom_node_t **glktwin_query_selector_all(void *doc, lxb_dom_node_t *root,
                            const char *selector, size_t *out_count);

/* Check if node matches a CSS selector. */
extern bool glktwin_element_matches(void *doc, lxb_dom_node_t *node,
                const char *selector);

/* Get the owner document for a node. */
extern void *glktwin_node_owner_document(lxb_dom_node_t *node);

/* Clone a node (deep or shallow). */
extern lxb_dom_node_t *glktwin_clone_node(lxb_dom_node_t *node, bool deep);

/* Create a DocumentFragment. */
extern lxb_dom_node_t *glktwin_create_document_fragment(void *doc);

/* Validate DOM tree integrity from root. Returns 0 if valid, -1 if corrupt. */
extern int glktwin_validate_tree(void *doc);

#endif
