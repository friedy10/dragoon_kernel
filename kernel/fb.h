#ifndef DRAGOON_FB_H
#define DRAGOON_FB_H

#include "types.h"

#define FB_WIDTH   640
#define FB_HEIGHT  480
#define FB_BPP     4
#define FB_STRIDE  (FB_WIDTH * FB_BPP)
#define FB_SIZE    (FB_WIDTH * FB_HEIGHT * FB_BPP)

int fb_init(void);
u32 *fb_get_buffer(void);

#endif /* DRAGOON_FB_H */
