/* render.h: DOM -> Glk text/styles/colors/links. */

#ifndef GLKTWIN_RENDER_H
#define GLKTWIN_RENDER_H

typedef void *dom_document_t;

void render_passage(dom_document_t doc);

/* After render_passage(), returns the number of clickable links found. */
int render_link_count(void);

/* Look up the DOM node for link number (1-based). Returns NULL if invalid. */
void *render_link_node(int link_num);

/* After render_passage(), returns number of <input> text fields found. */
int render_input_count(void);

/* Look up the DOM node for input number (1-based). */
void *render_input_node(int input_num);

#endif
