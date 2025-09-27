/* GFX library compiled-in font descriptor */
#include <stdint.h>

typedef union varptr {            /* used for accessing font bits and offsets */
    uint8_t *       ptr8;
    uint16_t *      ptr16;
    uint32_t *      ptr32;
} Varptr;

/* builtin C-based proportional/fixed font structure*/
typedef struct font {
    char *          name;         /* font name */
    int             maxwidth;     /* max width in pixels */
    unsigned int    height;       /* height in pixels */
    int             ascent;       /* ascent (baseline) height */
    int             firstchar;    /* first character in bitmap */
    int             size;         /* font size in characters */
    Varptr          bits;         /* possibly right-padded bitmap data MSB first */
    Varptr          offset;       /* offsets into bitmap data (see offset_width) */
    uint8_t *       width;        /* character widths or 0 if fixed width */
    uint16_t *      range;        /* sparse array glyph ranges table */
    int             defaultglyph; /* default glyph index */
    uint32_t        bits_size;    /* # words of bits (disk files only) */
    int             bpp;          /* bits per pixel (1=bitmap, 8=alpha channel) */
    int             bits_width;   /* bitmap word/Varptr size (1, 2, 4, 0=2) */
    int             offset_width; /* offset word/Varptr size (1, 2, 4, 0=4) */
    uint8_t         data[];       /* font bits allocated in single malloc */
} Font;
