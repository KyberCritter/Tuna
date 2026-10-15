#include <gba.h>
#include "display.h"
/* Original compact 5x7 glyphs: digits followed by uppercase letters. */
static const unsigned char font[36][7] = {
    {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14},{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},
    {14,17,16,16,16,17,14},{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},
    {31,16,16,30,16,16,16},{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},
    {14,4,4,4,4,4,14},{7,2,2,2,2,18,12},{17,18,20,24,20,18,17},
    {16,16,16,16,16,16,31},{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},
    {14,17,17,17,17,17,14},{30,17,17,30,16,16,16},{14,17,17,17,21,18,13},
    {30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},
    {17,17,10,4,10,17,17},{17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
};
static volatile unsigned short *const map = (volatile unsigned short *)0x0600f800;
void display_init(void) {
    volatile unsigned short *tiles = (volatile unsigned short *)0x06000000;
    unsigned c, y, x;
    REG_DISPCNT = 0x0100; /* Mode 0, BG0, 4bpp tiles. */
    REG_BG0CNT = 31 << 8;
    BG_PALETTE[0] = RGB5(0,0,0); BG_PALETTE[1] = RGB5(31,31,31);
    for (c = 0; c < 96; ++c) for (y = 0; y < 8; ++y) {
        unsigned pattern = 0, ascii = c + 32;
        if (y < 7 && ascii >= '0' && ascii <= '9') pattern = font[ascii - '0'][y];
        if (y < 7 && ascii >= 'A' && ascii <= 'Z') pattern = font[10 + ascii - 'A'][y];
        if (y < 7 && ascii >= 'a' && ascii <= 'z') pattern = font[10 + ascii - 'a'][y];
        if (ascii == ':' && (y == 2 || y == 5)) pattern = 4;
        if (ascii == '-' && y == 3) pattern = 14;
        if (ascii == '/' && y < 7) pattern = 1u << (y * 4 / 7);
        if (ascii == '.' && y == 6) pattern = 4;
        if (ascii == '?' && y < 7) { static const unsigned char q[7] = {14,17,1,2,4,0,4}; pattern = q[y]; }
        if (ascii == '|' && y < 7) pattern = 4;
        if (ascii == '=' && (y == 2 || y == 4)) pattern = 31;
        if (ascii == '>' && y < 7) { static const unsigned char g[7] = {16,8,4,2,4,8,16}; pattern = g[y]; }
        if (ascii == '<' && y < 7) { static const unsigned char g[7] = {2,4,8,16,8,4,2}; pattern = g[y]; }
        if (ascii == '[' && y < 7) pattern = (y == 0 || y == 6) ? 28 : 16;
        if (ascii == ']' && y < 7) pattern = (y == 0 || y == 6) ? 7 : 1;
        for (x = 0; x < 8; x += 4) {
            unsigned j, value = 0;
            for (j = 0; j < 4; ++j) if (x + j < 5 && (pattern & (16u >> (x + j)))) value |= 1u << (j * 4);
            tiles[c * 16 + y * 2 + x / 4] = (unsigned short)value;
            /* Inverted copy (index 96..191): the surrounding cell is index 1
               (white) and the glyph strokes become index 0 (black backdrop). */
            tiles[(96 + c) * 16 + y * 2 + x / 4] = (unsigned short)(value ^ 0x1111u);
        }
    }
    /* Tile 192 is the progress-bar trough: a two-pixel baseline so an empty bar
       is still visible in the two-colour palette. */
    for (y = 0; y < 8; ++y) for (x = 0; x < 8; x += 4)
        tiles[192 * 16 + y * 2 + x / 4] = (unsigned short)(y >= 6 ? 0x1111u : 0);
    display_clear();
}
void display_clear(void) { unsigned i; for (i = 0; i < 1024; ++i) map[i] = 0; }
void display_text(unsigned row, const char *text) {
    unsigned col;
    if (row >= 20) return;
    for (col = 0; col < 30; ++col) {
        unsigned c = *text ? (unsigned char)*text++ : ' ';
        if (c < 32 || c > 126) c = '?';
        map[row * 32 + col] = (unsigned short)(c - 32);
    }
}
void display_text_inv(unsigned row, const char *text) {
    unsigned col;
    if (row >= 20) return;
    for (col = 0; col < 30; ++col) {
        unsigned c = *text ? (unsigned char)*text++ : ' ';
        if (c < 32 || c > 126) c = '?';
        map[row * 32 + col] = (unsigned short)(96 + c - 32);
    }
}
void display_bar(unsigned row, unsigned width, unsigned filled) {
    unsigned col;
    if (row >= 20) return;
    if (width > 30) width = 30;
    if (filled > width) filled = width;
    for (col = 0; col < 30; ++col) map[row * 32 + col] = (unsigned short)(col < width ? (col < filled ? 96 : 192) : 0);
}
