/* quick X11 to GFX conversions */
#include <stdint.h>

#define Display     Drawable    /* references GFX Drawable, can't use X11 Drawable */
typedef int         Bool;
typedef Drawable *  Window;
typedef int         Colormap;

#define True                    1
#define False                   0
#define DefaultScreen(d)        0
#define RootWindow(d,s)         d
#define XDefaultColormap(d,s)   0
#define WhitePixel(d,s)         RGB(255,255,255)
#define BlackPixel(d,s)         RGB(0,0,0)
#define DisplayWidth(d,s)       d->width
#define DisplayHeight(d,s)      d->height
#define GCForeground            1
#define GCbackground            2
#define GCLineWidth             4
#define GCFunction              8
#define DoRed                   0
#define DoGreen                 0
#define DoBlue                  0

typedef struct xcolor {
    int red, green, blue;
    int flags;
    uint32_t pixel;
} XColor;

typedef struct gc {
    Pixel   fg;
    Pixel   bg;
} *GC;

typedef struct xgcvalues {
    int foreground;
    int background;
    int line_width;
    int function;
} XGCValues;

typedef struct xsegment {
    int x1, x2;
    int y1, y2;
} XSegment;

Display *XOpenDisplay(char *display);
#define XDestroyWindow(d,s)
int XSync(Display *dpy, Bool discard);

GC XCreateGC(Display *dpy, Window d, unsigned long valuemask, XGCValues *values);
int XCopyGC(Display *dpy, GC src, unsigned long valuemask, GC dest);
int XFreeGC(Display *display, GC gc);
int XSetForeground(Display *dpy, GC gc, unsigned long foreground);
int XSetBackground(Display *dpy, GC gc, unsigned long background);
int XAllocColor(Display *display, Colormap colormap, XColor *screen_in_out);

int XDrawSegments(Display *dpy, Window d, GC gc, XSegment *segments, int nsegments);
int XDrawLine (Display *dpy, Window d, GC gc, int x1, int y1, int x2, int y2);
int XFillRectangle(Display *dpy, Window d, GC gc, int x, int y, int width, int height);
int XCopyArea(Display *dpy, Window src, Window dest, GC gc,
    int src_x, int src_y, int width, int height, int dest_x, int dest_y);
