#ifndef DRAGOON_FONT_H
#define DRAGOON_FONT_H

#include "types.h"

#define FONT_WIDTH  8
#define FONT_HEIGHT 16

/* Returns pointer to 16 bytes of bitmap data for ASCII char c */
const u8 *font_get_glyph(char c);

#endif /* DRAGOON_FONT_H */
