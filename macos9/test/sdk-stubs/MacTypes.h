/*
 * sdk-stubs/MacTypes.h -- see the top of this directory's README for why these
 * stubs exist. Declaration-only: no sizes, no layout, no semantics.
 *
 * A green compile against these stubs means the Mac sources are dialect-clean
 * and internally type-consistent. It does NOT mean they build with
 * CodeWarrior, link against the real QuickDraw, or run on Classic Mac OS 9.
 */
#ifndef PLATINUM_STUB_MACTYPES_H
#define PLATINUM_STUB_MACTYPES_H

typedef unsigned char Boolean;
typedef signed char SInt8;
typedef unsigned char UInt8;
typedef signed short SInt16;
typedef unsigned short UInt16;
typedef signed long SInt32;
typedef unsigned long UInt32;
typedef long Size;
typedef unsigned long OSType; /* a four-character code: 32 bits */
typedef short ScriptCode;
typedef UInt32 KeyMap[4];

typedef char *Ptr;
typedef Ptr *Handle;
typedef unsigned char *StringPtr;
typedef const unsigned char *ConstStringPtrParam;
typedef unsigned char *Str255;
typedef const unsigned char *ConstStr255Param;
typedef unsigned char *Str63;
typedef unsigned char *Str31;
typedef unsigned char *Str27;
typedef unsigned char *Str15;

typedef unsigned char TextByte;
typedef signed char SignedByte;

typedef long Fixed;
typedef SInt16 OSErr; /* 16 bits on the classic Toolbox */
typedef SInt32 OSStatus;
typedef UInt32 FourCharCode;

typedef struct StubPoint { short v; short h; } Point;
typedef struct StubRect { short top; short left; short bottom; short right; } Rect;
typedef struct StubRGBColor {
    unsigned short red;
    unsigned short green;
    unsigned short blue;
} RGBColor;
typedef struct StubPattern { unsigned char pat[8]; } Pattern;
/* BitMap, PixMap and the colour table: member names, types and order as the
 * Multiversal Interfaces define them (BitMap 14 bytes, PixMap 50). */
typedef struct StubBitMap {
    Ptr baseAddr;
    short rowBytes;
    Rect bounds;
} BitMap;
typedef struct StubColorSpec {
    short value;
    RGBColor rgb;
} ColorSpec;
typedef struct StubColorTable {
    long ctSeed;
    unsigned short ctFlags;
    short ctSize;
    ColorSpec ctTable[1];
} ColorTable;
typedef ColorTable *CTabPtr;
typedef CTabPtr *CTabHandle;
typedef struct StubPixMap {
    Ptr baseAddr;
    short rowBytes;
    Rect bounds;
    short pmVersion;
    short packType;
    long packSize;
    Fixed hRes;
    Fixed vRes;
    short pixelType;
    short pixelSize;
    short cmpCount;
    short cmpSize;
    long planeBytes;
    CTabHandle pmTable;
    long pmReserved;
} PixMap;
typedef struct StubRgn {
    short boundingTop;
    short boundingLeft;
    short boundingBottom;
    short boundingRight;
} Region;
typedef struct StubRectPtr { Rect stub; long seed; } RectPtr;

typedef RectPtr *RgnHandle;
typedef BitMap *BitMapPtr;
typedef PixMap *PixMapPtr;
typedef void *GrafPtr;
typedef struct StubWindow *WindowPtr;

typedef unsigned char FontFamily;
typedef unsigned char FontStyle;
typedef struct StubFontMetrics {
    short ascent;
    short descent;
    short leading;
    short width;
} FontMetrics;

typedef struct StubProc { long stub; } ProcStruct;
typedef ProcStruct *ProcPtr;

typedef struct StubParamStructRec { long stub; } ParamStructRec;

#define true 1
#define false 0
#define nil 0L

#define noErr 0
/* FindWindow part codes, as in the Window Manager chapter of Inside Macintosh. */
#define inDesk 0
#define inMenuBar 1
#define inSysWindow 2
#define inContent 3
#define inDrag 4
#define inGrow 5
#define inGoAway 6
#define paramErr -50
#define fnfErr -43
#define ioErr -36
#define memFullErr -108
#define dirFErr -64
#define eofErr -39
#define overrunErr -27

#define systemFont 0
#define smSystemScript (-1)
#define smRoman 0

#endif
