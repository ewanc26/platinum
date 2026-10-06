#ifndef PLATINUM_IMAGE_PIX_H
#define PLATINUM_IMAGE_PIX_H

#include "image_blob.h"

#include <MacTypes.h>
#include <Quickdraw.h>
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A parsed image as a QuickDraw PixMap: a copy of the pixel bytes, a colour
 * table built from the palette, and a PixMap record pointing at both, drawn with
 * CopyBits. The PixMap field values follow Apple's description of an indexed
 * chunky PixMap (pixelType 0, one component, rowBytes with the high bit set,
 * 72 dpi); none of this has been run on a Mac.
 */
typedef struct platinum_pixmap {
    PixMap pix;
    Ptr pixels;
    CTabHandle table;
    short width;
    short height;
    int valid;
} platinum_pixmap;

void platinum_pixmap_init(platinum_pixmap *pm);
/* Copy `image` into a PixMap. memFullErr if either allocation fails, with
 * nothing left allocated. The blob `image` points into may be freed afterwards. */
OSErr platinum_pixmap_make(platinum_pixmap *pm, const platinum_image *image);
/* Draw into `window` at `dest`, which is scaled to if it differs in size. */
void platinum_pixmap_draw(const platinum_pixmap *pm, WindowPtr window,
                          const Rect *dest);
void platinum_pixmap_dispose(platinum_pixmap *pm);

#ifdef __cplusplus
}
#endif

#endif
