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
    int             firstchar;    /* first charcode in bitmap data, 0 if range */
    int             size;         /* font size in glyph */
    Varptr          bits;         /* right-padded bitmap data MSB first */
    Varptr          offset;       /* offsets into bitmap data (see offset_width) */
    uint8_t *       width;        /* glyph widths or 0 if fixed width */
    uint16_t *      range;        /* charcode ranges table (sparse array) */
    int             defaultglyph; /* bitmap index of default glyph */
    uint32_t        bits_size;    /* # words of bits (disk files only) */
    int             bpp;          /* bits per pixel (1=bitmap, 8=alpha channel) */
    int             bits_width;   /* bitmap word/Varptr size (1, 2, 4, 0=2) */
    int             offset_width; /* offset word/Varptr size (1, 2, 4, 0=4) */
    uint8_t         data[];       /* font bitmap data allocated in single malloc */
} Font;
