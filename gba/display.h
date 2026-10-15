#ifndef TUNA_DISPLAY_H
#define TUNA_DISPLAY_H
void display_init(void);
void display_clear(void);
void display_text(unsigned row, const char *text);
/* Inverted (white background / black glyph) row for header and footer bars. */
void display_text_inv(unsigned row, const char *text);
/* Solid progress bar: 'filled' white cells followed by empty cells. */
void display_bar(unsigned row, unsigned width, unsigned filled);
#endif
