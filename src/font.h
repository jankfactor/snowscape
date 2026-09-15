#ifndef SNOWSCAPE_FONT_H
#define SNOWSCAPE_FONT_H

/* 4x8 glyphs, five-pixel advance, ten-pixel newline advance.
 * Coordinates are pixels from the top left of a 320x256 screen bank.
 * PAL_256 selects Mode 13; otherwise Mode 9. fg is a logical palette index
 * (0..255 or 0..15), not a packed pixel value. Lowercase uses uppercase glyphs;
 * unsupported characters use space. Text must fit on screen; no clipping.
 */
void draw_text(unsigned char *fb, int x, int y, const char *s,
               unsigned char fg);
/* Clear unset glyph pixels to palette index zero; leave spacing untouched. */
void draw_text_opaque(unsigned char *fb, int x, int y, const char *s,
                      unsigned char fg);
/* dst starts at the glyph's top-left byte (an even pixel in Mode 9). */
void draw_text_char_opaque(unsigned char *dst, char c, unsigned char fg);

#endif
