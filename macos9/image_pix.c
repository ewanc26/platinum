#include "image_pix.h"

#include <Memory.h>
#include <string.h>

void platinum_pixmap_init(platinum_pixmap *pm)
{
    if (pm != NULL)
        memset(pm, 0, sizeof(*pm));
}

OSErr platinum_pixmap_make(platinum_pixmap *pm, const platinum_image *image)
{
    long pixel_bytes;
    long table_bytes;
    long i;
    ColorTable *ct;

    if (pm == NULL || image == NULL || image->pixels == NULL ||
        image->palette == NULL || image->colours < 1)
        return paramErr;
    platinum_pixmap_init(pm);

    pixel_bytes = (long)image->height * (long)image->row_bytes;
    pm->pixels = NewPtr(pixel_bytes);
    if (pm->pixels == NULL)
        return memFullErr;
    memcpy(pm->pixels, image->pixels, (size_t)pixel_bytes);

    table_bytes = (long)sizeof(ColorTable) +
                  (long)(image->colours - 1) * (long)sizeof(ColorSpec);
    pm->table = (CTabHandle)NewHandle(table_bytes);
    if (pm->table == NULL) {
        DisposePtr(pm->pixels);
        pm->pixels = NULL;
        return memFullErr;
    }

    HLock((Handle)pm->table);
    ct = *pm->table;
    ct->ctSeed = GetCTSeed();
    ct->ctFlags = 0;
    ct->ctSize = (short)(image->colours - 1);
    for (i = 0; i < image->colours; ++i) {
        const unsigned char *rgb = image->palette + i * 3;

        ct->ctTable[i].value = (short)i;
        /* 8-bit channels widened to 16 bits by repeating the byte. */
        ct->ctTable[i].rgb.red = (unsigned short)((rgb[0] << 8) | rgb[0]);
        ct->ctTable[i].rgb.green = (unsigned short)((rgb[1] << 8) | rgb[1]);
        ct->ctTable[i].rgb.blue = (unsigned short)((rgb[2] << 8) | rgb[2]);
    }
    HUnlock((Handle)pm->table);

    pm->width = image->width;
    pm->height = image->height;
    pm->pix.baseAddr = pm->pixels;
    /* The high bit of rowBytes marks a PixMap rather than a BitMap. */
    pm->pix.rowBytes = (short)((long)image->row_bytes - 32768L);
    SetRect(&pm->pix.bounds, 0, 0, image->width, image->height);
    pm->pix.pmVersion = 0;
    pm->pix.packType = 0;
    pm->pix.packSize = 0;
    pm->pix.hRes = 0x00480000L;
    pm->pix.vRes = 0x00480000L;
    pm->pix.pixelType = 0;
    pm->pix.pixelSize = (short)image->depth;
    pm->pix.cmpCount = 1;
    pm->pix.cmpSize = (short)image->depth;
    pm->pix.planeBytes = 0;
    pm->pix.pmTable = pm->table;
    pm->pix.pmReserved = 0;
    pm->valid = 1;
    return noErr;
}

void platinum_pixmap_draw(const platinum_pixmap *pm, WindowPtr window,
                          const Rect *dest)
{
    GrafPtr old_port;

    if (pm == NULL || !pm->valid || window == NULL || dest == NULL)
        return;
    GetPort(&old_port);
    SetPort((GrafPtr)window);
    /* In a colour window portBits aliases portPixMap and portVersion, which is
     * how CopyBits is told the destination is a colour port. */
    CopyBits((BitMap *)&pm->pix, &window->portBits, &pm->pix.bounds, dest,
             srcCopy, NULL);
    SetPort(old_port);
}

void platinum_pixmap_dispose(platinum_pixmap *pm)
{
    if (pm == NULL)
        return;
    if (pm->table != NULL)
        DisposeHandle((Handle)pm->table);
    if (pm->pixels != NULL)
        DisposePtr(pm->pixels);
    platinum_pixmap_init(pm);
}
