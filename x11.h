/* quick X11 to GFX conversions */
#include <stdint.h>

#define Display     Drawable    /* references GFX Drawable, can't use X11 Drawable */
typedef int         Bool;
typedef int         Window;
typedef int         Colormap;
typedef uint32_t    XColor;
typedef int *       XGCValues;

#define True                    1
#define False                   0
#define DefaultScreen(d)        0
#define RootWindow(d,s)         0
#define XDefaultColormap(d,s)   0
#define WhitePixel(d,s)         RGB(255,255,255)
#define BlackPixel(d,s)         RGB(0,0,0)
#define DisplayWidth(d,s)       d->width
#define DisplayHeight(d,s)      d->height

typedef struct xsegment {
    int x1, x2;
    int y1, y2;
} XSegment;

typedef struct gc {
    Pixel   fg;
    Pixel   bg;
} *GC;

Display *XOpenDisplay(char *display);
#define XDestroyWindow(d,s)
int XSync(Display *dpy, Bool discard);

GC XCreateGC(Display *dpy, int d, unsigned long valuemask, XGCValues *values);
int XCopyGC(Display *dpy, GC src, unsigned long valuemask, GC dest);
int XSetForeground(Display *dpy, GC gc, unsigned long foreground);
int XSetBackground(Display *dpy, GC gc, unsigned long background);

int XDrawSegments(Display *dpy, int d, GC gc, XSegment *segments, int nsegments);
int XDrawLine (Display *dpy, int d, GC gc, int x1, int y1, int x2, int y2);
int XFillRectangle(Display *xdpy, int d, GC gc, int x, int y, int width, int height);
