/*
 * compiled font descriptor
 */

#include <stdint.h>

typedef union varptr {
    uint8_t *       ptr8;
    uint16_t *      ptr16;
    uint32_t *      ptr32;
} Varptr;

/* builtin C-based proportional/fixed font structure*/
typedef struct font {
    char *          name;       /* font name*/
    int             maxwidth;   /* max width in pixels*/
    unsigned int    height;     /* height in pixels*/
    int             ascent;     /* ascent (baseline) height*/
    int             firstchar;  /* first character in bitmap*/
    int             size;       /* font size in characters*/
    Varptr          bits;       /* possibly right-padded bitmap data*/
    Varptr          offset;     /* offsets into bitmap data*/
    uint8_t *       width;      /* character widths or 0 if fixed*/
    int             defaultchar;/* default char (not glyph index)*/
    int32_t         bits_size;  /* # words of bits (unused) */
    int             bits_width; /* size of bits Varptr (affects padding) */
    int             offset_width; /* size of offset Varptr (1, 2 or 4) */
    uint8_t         data[];     /* font bits allocated in single malloc */
} Font;
