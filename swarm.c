/*
** XSwarm
** ======
**
** Purpose:     Display a swarm of bees chasing a wasp.
**
** Features:    uses only integer math
**              can be put in the root window
**              uses non-blocking delays to decrease cpu usage
**              has no redeeming social value
**
** Comments:    I used psychoII (I assume it's a derivative of ico.) as an
**              an example of how to open up a window and draw some lines.
**              Most of this program is now totally rewritten by me, but the
**              initial window manager hints stuff is left over from the older
**              program.
**
**              The program meltdown was handy for learning how to take over
**              the screen.
**
**              Any comments, enhances, or fixes are welcome!
**
** Created:     by Jeff Butterworth on 7/11/90
**              butterwo@cs.unc.edu
**
** Updated:     by Jeff Butterworth on 1/4/91
**
*/

/* These are needed for the nap function. */
#include <sys/time.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* X Includes */
//#include <X11/Xlib.h>
//#include <X11/Xatom.h>
//#include <X11/Xutil.h>

/* This makes it work with virtual root window managers. */
//#include "vroot.h"

/* Standard Includes */
#include <stdio.h>

/* For those of you who want to know what version you are using. */
#include "patchlevel.h"

/* Includes for this project. */
#include "swarm.h"

void Bounce_Against_Windows();
void HandleEvent();

int main(argc, argv)
int argc;
char **argv;
{
    char        *geometry = NULL;


    Initialize();

    Parse_Arguments(argc, argv, &geometry);

    /* The program acts as a screen saver if timeout is non-zero. */
    //if (!timeout)
    {
        Create_Window(geometry);
        Animate();
    }
    //else
    //{
        //Change_Screen_Saver(False);
        //while(TRUE)
        //{
            //Wait_For_Idleness();
            //Create_Big_Window();
            //Animate();
        //}
    //}
}


void
Quit(signal)
int                     signal;
{
    if (timeout)
    {
        fprintf(stderr, "Restoring screen saver state.\n");
        //Change_Screen_Saver(True);
    }

    fprintf(stderr, "Terminating because of signal %d. ", signal);
    switch (signal)
    {
        case SIGHUP:    fprintf(stderr, "(Hangup)\n"); break;
        case SIGINT:    fprintf(stderr, "(Interrupt)\n"); break;
        case SIGQUIT:   fprintf(stderr, "(Quit)\n"); break;
        case SIGILL:    fprintf(stderr, "(Illegal Instruction)\n"); break;
        case SIGFPE:    fprintf(stderr, "(Floating Point Exception)\n"); break;
        case SIGKILL:   fprintf(stderr, "(Kill)\n"); break;
#ifdef SIGBUS
        case SIGBUS:    fprintf(stderr, "(Bus Error)\n"); break;
#endif
        case SIGSEGV:   fprintf(stderr, "(Segmentation Violation)\n"); break;
#ifdef SIGSYS
        case SIGSYS:    fprintf(stderr, "(Bad Argument to Sys Call)\n"); break;
#endif
        case SIGTERM:   fprintf(stderr, "(Termination from Kill)\n"); break;
        case SIGSTOP:   fprintf(stderr, "(Stop not from tty)\n"); break;
        case SIGTSTP:   fprintf(stderr, "(Stop from tty)\n"); break;
#ifdef SIGXCPU
        case SIGXCPU:   fprintf(stderr, "(Exceeded CPU Time Limit)\n"); break;
#endif
        default:        fprintf(stderr, "(Unexpected Signal)\n");
    }

    exit(-signal);
}


void
Initialize()
{
    signal(SIGHUP, Quit);
    signal(SIGINT, Quit);
    signal(SIGQUIT, Quit);
    signal(SIGILL, Quit);
    signal(SIGFPE, Quit);
    signal(SIGKILL, Quit);
#ifdef SIGBUS
    signal(SIGBUS, Quit);
#endif
    signal(SIGSEGV, Quit);
#ifdef SIGSYS
    signal(SIGSYS, Quit);
#endif
    signal(SIGTERM, Quit);
    signal(SIGSTOP, Quit);
    if (timeout) signal(SIGTSTP, Quit);
#ifdef SIGXCPU
    signal(SIGXCPU, Quit);
#endif
}


/* Parse_Arguments()                                                    */
/* Analyze command line arguments and set variables accordingly.        */

void
Parse_Arguments(argc, argv, geometry)
int     argc;
char    **argv;
char    **geometry;
{
    /* These variables are used to analyze the command line parameters. */
    int         option;
    extern int  optind;
    extern char *optarg;

    int                 bits;           /* Used to set window geometry. */


    /* Check the command line. */
    while ((option = getopt(argc,argv,"hq?vprVxt:d:g:D:b:a:s:A:S:B:w:c:C:"))
            != EOF)
    {
        switch (option)
        {
            case 'v':
                verbose = TRUE;
                break;
            case 'p':
                pointer = TRUE;
                break;
            case 'V':
                stay_in_front = TRUE;
                break;
            case 'r':
                root = TRUE;
                break;
            case 'x':
                xor = TRUE;
                break;
            case 't':
                timeout = atoi(optarg);
                break;
            case 'g':
                *geometry = optarg;
                break;
            case 'd':
                display.dname = optarg;
                break;
            case 'b':
                bees = atoi(optarg);
                break;
            case 'a':
                bee_acc = atoi(optarg);
                break;
            case 's':
                bee_vel = atoi(optarg);
                break;
            case 'A':
                wasp_acc = atoi(optarg);
                break;
            case 'S':
                wasp_vel = atoi(optarg);
                break;
            case 'D':
                delay = atoi(optarg) * 1000; /* convert to microseconds */
                break;
            case 'B':
                border = atoi(optarg);
                break;
            case 'w':
                wasp_color = optarg;
                break;
            case 'c':
                bee_color = optarg;
                break;
            case 'C':
                bg_color = optarg;
                break;
            case 'h':
            case 'q':
            case '?':
                //Usage(*argv);
                //HandleError("The command line parameters were incorrect.", FATAL);
                break;
        }
    }

    /* Display the arguments. */
    if (verbose)
    {
        printf("Here are the current settings...\n");
        if (xor)
            printf("The logical raster operation GXxor is being used.\n");
        else
            printf("The logical raster operation GXcopy is being used.\n");
        if (stay_in_front)
            printf("The bees will try to stay in the clear area of the screen.\n");
        printf("There are %d bees.\n", bees);
        printf("Bee acceleration is %d pixels per frame per frame.\n",bee_acc);
        printf("Bee speed limit is %d pixels per frame.\n", bee_vel);
        printf("Wasp acceleration is %d pixels per frame per frame.\n",
            wasp_acc);
        printf("Wasp speed limit is %d pixels per frame.\n", wasp_vel);
        printf("The non-blocking delay is %d milliseconds per frame.\n",
            delay / 1000);
        if (timeout) printf("Screen saved after %d seconds.\n", timeout);
        printf("The wasp can't go within %d pixels of the window edge.\n",
            border);
        printf("The wasp color is \"%s\"\n", wasp_color);
        printf("The bee color is \"%s\"\n", bee_color);
        printf("The background color is \"%s\"\n", bg_color);
    }

    /* Open the display. */
    if (!(display.dpy = XOpenDisplay(display.dname)))
    {
        //HandleError("Cannot open display.\n");
        exit(-1);
    }

    /* Record the screen number and root window. */
    display.screen = DefaultScreen(display.dpy);
    display.root = RootWindow(display.dpy, display.screen);

    /* Set the colors. */
    display.cmap = XDefaultColormap(display.dpy, display.screen);
    //if (!display.cmap) HandleError("There was no default colormap!", FATAL);

    if (wasp_color == NULL)
        display.wasp = WhitePixel(display.dpy, display.screen);
        //display.wasp = RGB(0, 0, 255);
    else
        display.wasp = GetColor(&display, wasp_color, NULL);

    if (bee_color == NULL)
        display.bee =WhitePixel(display.dpy, display.screen);
    else
        display.bee = GetColor(&display, bee_color, NULL);

    if (bg_color == NULL)
        display.bg = BlackPixel(display.dpy, display.screen);
    else
        display.bg = GetColor(&display, bg_color, &(display.bg_xcolor));

    /* Set up window size. */
    if (!root && !timeout)
    {
        winW = WINWIDTH;
        winH = WINHEIGHT;
        winX = (DisplayWidth(display.dpy, display.screen)
                - winW) / 2;
        winY = (DisplayHeight(display.dpy, display.screen)
                - winH) / 2;
        //if (*geometry != NULL) 
            //XParseGeometry(*geometry, &winX, &winY, &winW, &winH);
    }
    else
    {
        winW = DisplayWidth(display.dpy, display.screen);
        winH = DisplayHeight(display.dpy, display.screen);
        winX = 0;
        winY = 0;
    }

    /* If screen saving is on, then watch for events everywhere. */
    /* Traverse the window tree. */
    //if (timeout) Traverse_Tree(display.dpy, display.root);
}


/* Change_Screen_Saver()                                                        */
/* Turn the server's screen saver on or off.                            */
/* This routine should be called with on=False before it is called      */
/* with on=True.                                                        */

#if 0
void
Change_Screen_Saver(on)
int     on;     /* True or False */
{
    static int  timeout, interval, blanking, exposures;
    static int  set_yet = FALSE;

    if (on)
    {
        if (set_yet)
        {
            /* Restore the old settings. */
            XSetScreenSaver(display.dpy, timeout, interval, blanking,exposures);
            XActivateScreenSaver(display.dpy);
            XResetScreenSaver(display.dpy);
            XSync(display.dpy, False);
        }
    }
    else
    {
        /* Save the old settings and turn off the server's screen saver. */
        XGetScreenSaver(display.dpy, &timeout, &interval, &blanking,&exposures);
        XSetScreenSaver(display.dpy, 0, 0, DefaultBlanking, DefaultExposures);
        XResetScreenSaver(display.dpy);
        set_yet = TRUE;
    }
}

/* Traverse_Tree()                                                      */
/* Select some events from every single window that is a decendent      */
/* of "current".                                                        */

void
Traverse_Tree(display, current)
Display *display;
Window  current;
{
    Window              my_root, parent, *children;
    unsigned int        num_children;
    int         i;


    /* Watch for signs of life from the user in this window. */
    XSelectInput(display, current, ALIVE_MASK);

    /* Who are my kids? */
    XQueryTree(display, current, &my_root, &parent, &children,
        &num_children);

    /* Examine all of the children too. */
    for (i = 0 ; i < num_children ; i++)
        Traverse_Tree( display, children[i] );

    /* Let's not waste any memory. */
    XFree( (char *) children);
}


/* Wait_For_Idleness()                                                  */
/* Wait for "timeout" seconds of user inactivity.                       */

void
Wait_For_Idleness()
{
    int         watching = TRUE;
    int         found;
    int         timer = 0;
    XEvent      event;

    while( watching )
    {
        if (timer >= timeout) return;

        sleep(1);
        timer++;

        found = XCheckIfEvent(display.dpy, &event, Dummy_Predicate, NULL);
        if (found) timer = 0;

        /* Flush events. */
        while (found)
        {
            switch (event.type)
            {
                /* Watch for events in new windows too. */
                case CreateNotify:
                    {
                        XCreateWindowEvent *ev = (XCreateWindowEvent *) &event;

                        XSelectInput(display.dpy, ev->window, ALIVE_MASK);
                        Traverse_Tree(display.dpy, ev->window);
                    }
                    break;
                default:
                    break;
            }
            /* Check for the existence of more events. */
            found = XCheckIfEvent(display.dpy, &event, Dummy_Predicate, NULL);
        }
    }
}


Bool
Dummy_Predicate(display, event, arg)
Display *display;
XEvent  *event;
char    *arg;
{
    return(True);
}


void
Create_Big_Window()
{
    /* Window Attributes */
    unsigned long               valuemask;
    XSetWindowAttributes        xswa;

    XSizeHints          sizehint;
    XWMHints            wmhints;
    unsigned char       wname[32];      /* Window Name */

    /* First time. */
    static int          first = TRUE;

    /* Cursor Stuff */
    static Cursor       cursor;


    /* Turn the cursor the color of the background. (Invisible) */
    if (first)
    {
        /* I don't care which cursor I get. */
        cursor = XCreateFontCursor(display.dpy, 0);

        XRecolorCursor(display.dpy, cursor, &(display.bg_xcolor),
                        &(display.bg_xcolor));
        first = FALSE;
    }

    /* Create a screen sized window. */
    xswa.cursor = cursor;
    xswa.background_pixel = display.bg;
    xswa.override_redirect = True;
    xswa.do_not_propagate_mask = KeyPressMask | KeyReleaseMask |
        ButtonPressMask | ButtonReleaseMask;
    valuemask = CWCursor | CWBackPixel | CWOverrideRedirect | CWDontPropagate;
    display.win = XCreateWindow(display.dpy, display.root, 
        0, 0, 
        winW, winH, 0, 
        DefaultDepth(display.dpy, display.screen), 
        InputOutput, DefaultVisual(display.dpy, display.screen),
        valuemask, &xswa);

    XMapWindow(display.dpy, display.win);

    /* Event Mask */
    XSelectInput(display.dpy, display.win, KeyPressMask | PointerMotionMask);

    /* Set up the bees' graphics context. */ 
    display.bee_gc = XCreateGC(display.dpy, display.win, 0, NULL); 
    XSetForeground(display.dpy, display.bee_gc, display.bee); 
    XSetBackground(display.dpy, display.bee_gc, display.bg);

    /* Set up the wasp's graphics context. */ 
    display.wasp_gc = XCreateGC(display.dpy, display.win, 0, NULL); 
    XSetForeground(display.dpy, display.wasp_gc, display.wasp); 
    XSetBackground(display.dpy, display.wasp_gc, display.bg);

    /* Set up an erasing graphics context. */
    display.erase_gc = XCreateGC(display.dpy, display.win, 0, NULL); 
    XCopyGC(display.dpy, display.bee_gc, 0xffffffff, display.erase_gc); 
    XSetForeground(display.dpy, display.erase_gc, display.bg);

    /* Set up an xor wasp graphics context. */
    display.wasp_xor_gc = XCreateGC(display.dpy, display.win, 0, NULL); 
    XCopyGC(display.dpy, display.wasp_gc, 0xffffffff, display.wasp_xor_gc); 
    XSetFunction(display.dpy, display.wasp_xor_gc, GXxor);
    XSetForeground(display.dpy, display.wasp_xor_gc, display.bg); 

    /* Set up an xor bee graphics context. */
    display.bee_xor_gc = XCreateGC(display.dpy, display.win, 0, NULL); 
    XCopyGC(display.dpy, display.bee_gc, 0xffffffff, display.bee_xor_gc); 
    XSetFunction(display.dpy, display.bee_xor_gc, GXxor);
    XSetForeground(display.dpy, display.bee_xor_gc, display.bg); 

    /* Clear the background. */
    XFillRectangle(display.dpy, display.win, display.erase_gc,
            0,0, winW, winH);
}
#endif


/* Create_Window()                                                      */
/* Create the window, making sure to set it up correctly.               */

void
Create_Window(geometry)
char    *geometry;
{
#if 0
    XSetWindowAttributes xswa;
    XSizeHints          sizehint;
    XWMHints            wmhints;
    unsigned char       wname[256];     /* Window Name */


    if (!root)
    {
        xswa.event_mask = 0;
        xswa.background_pixel = display.bg;
        display.win = XCreateWindow(display.dpy, display.root, 
            winX, winY, 
            winW, winH, 0, 
            DefaultDepth(display.dpy, display.screen), 
            InputOutput, DefaultVisual(display.dpy, display.screen),
            CWEventMask | CWBackPixel , &xswa);

        sizehint.x = winX;
        sizehint.y = winY;
        sizehint.width = winW;
        sizehint.height = winH;
        sizehint.min_width = 2*border + 20;
        sizehint.min_height = 2*border + 20;
        if (geometry != NULL)
            sizehint.flags = USPosition | USSize | PMinSize;
        else
            sizehint.flags = PPosition | PSize | PMinSize;
        XSetNormalHints(display.dpy, display.win, &sizehint);
            
        display.protocol_atom = XInternAtom(display.dpy, "WM_PROTOCOLS", False);
        display.kill_atom = XInternAtom(display.dpy, "WM_DELETE_WINDOW", False);
#ifdef X11R4
        XSetWMProtocols(display.dpy, display.win, &display.kill_atom, 1);
#endif

        /* Title */
        sprintf( (char *) wname, "XSwarm, Version %s, by Jeff Butterworth",
            VERSION);
        XChangeProperty(display.dpy, display.win,
            XA_WM_NAME, XA_STRING, 8, PropModeReplace, wname, 
            strlen((char *)wname));

        /* Window Manager Hints (This is supposed to make input work.) */
        wmhints.flags = InputHint;
        wmhints.input = True;
        XSetWMHints(display.dpy, display.win, &wmhints);

        XMapWindow(display.dpy, display.win);
    }
    else
    {
        display.win = display.root;
        xswa.backing_store = Always;
        XChangeWindowAttributes(display.dpy, display.win,
            CWBackingStore, &xswa);
    }

    /* Event Mask */
    if (root)
        XSelectInput(display.dpy, display.win,
                        KeyPressMask | StructureNotifyMask);
    else
        XSelectInput(display.dpy, display.win,
                        KeyPressMask | ButtonPressMask | StructureNotifyMask);

#endif

    display.win = display.root;

    /* Set up the bees' graphics context. */ 
    display.bee_gc = XCreateGC(display.dpy, display.win, 0, NULL); 
    XSetForeground(display.dpy, display.bee_gc, display.bee); 
    XSetBackground(display.dpy, display.bee_gc, display.bg);

    /* Set up the wasp's graphics context. */ 
    display.wasp_gc = XCreateGC(display.dpy, display.win, 0, NULL); 
    XSetForeground(display.dpy, display.wasp_gc, display.wasp); 
    XSetBackground(display.dpy, display.wasp_gc, display.bg);

    /* Set up an erasing graphics context. */
    display.erase_gc = XCreateGC(display.dpy, display.win, 0, NULL); 
    XCopyGC(display.dpy, display.bee_gc, 0xffffffff, display.erase_gc); 
    XSetForeground(display.dpy, display.erase_gc, display.bg);

    /* Set up an xor wasp graphics context. */
    display.wasp_xor_gc = XCreateGC(display.dpy, display.win, 0, NULL); 
    XCopyGC(display.dpy, display.wasp_gc, 0xffffffff, display.wasp_xor_gc); 
    //XSetFunction(display.dpy, display.wasp_xor_gc, GXxor);
    XSetForeground(display.dpy, display.wasp_xor_gc, display.bg); 

    /* Set up an xor bee graphics context. */
    display.bee_xor_gc = XCreateGC(display.dpy, display.win, 0, NULL); 
    XCopyGC(display.dpy, display.bee_gc, 0xffffffff, display.bee_xor_gc); 
    //XSetFunction(display.dpy, display.bee_xor_gc, GXxor);
    XSetForeground(display.dpy, display.bee_xor_gc, display.bg); 

    /* Clear the background. */
    if (!xor)
    {
        //XSetWindowBackground(display.dpy, display.win, display.bg);
        XFillRectangle(display.dpy, display.win, display.erase_gc,
            0,0, winW, winH);
    }
}


/* Animate()                                                            */
/* Move the swarm around.                                               */

void
Animate()
{
    register int        b;              /* bee index */
    XSegment            *segs;          /* bee lines */
    XSegment            *old_segs;      /* old bee lines */
    int                 *x, *y;         /* bee positions x[time][bee#] */
    int                 *xv, *yv;       /* bee velocities xv[bee#] */
    int                 wx[3], wy[3];   /* wasp positions */
    int                 wxv, wyv;       /* wasp velocity */
    //XEvent              xev;
    int                 dx,dy,distance;
    int                 init = TRUE;

    /* These variables are related to bouncing off of other windows. */
    int                 testx, testy;
    int                 BehindWindow;   /* TRUE if wasp is behind a window. */
    Window              child;

    /* These variables are related to mouse control of the wasp. */
    int                 garbage_int; /* for XQueryPointer */
    Window              garbage_win; /* ...likewise */



    /* Get the random number generator ready. */
    srandom(getpid());

    /* Allocate memory. */
    segs = (XSegment *) malloc(sizeof(XSegment) * bees);
    old_segs = (XSegment *) malloc(sizeof(XSegment) * bees);
    x = (int *) malloc(sizeof(int) * bees * times);
    y = (int *) malloc(sizeof(int) * bees * times);
    xv = (int *) malloc(sizeof(int) * bees);
    yv = (int *) malloc(sizeof(int) * bees);

    /* Initialize point positions, velocities, etc. */

    /* wasp */
    //if (pointer)
    //{
        //XQueryPointer(display.dpy, display.win, &garbage_win,
                        //&garbage_win, &garbage_int, &garbage_int,
                        //&(wx[0]), &(wy[0]), (unsigned int *) &garbage_int);
    //}
    //else
    {
        wx[0] = BORDER + random() % (winW - 2*BORDER);
        wy[0] = BORDER + random() % (winH - 2*BORDER);
    }
    wx[1] = wx[0];
    wy[1] = wy[0];
    wxv = 0;
    wyv = 0;

    /* bees */
    for (b = 0 ; b < bees ; b++)
    {
        X(0,b) = random() % winW;
        X(1,b) = X(0,b);
        Y(0,b) = random() % winH;
        Y(1,b) = Y(0,b);
        xv[b] = RAND(7);
        yv[b] = RAND(7);
    }

    /* This checks to see if we've gone behind a window. */
    //if (stay_in_front)
    //{
        //XTranslateCoordinates(display.dpy, display.win, display.win,
            //wx[0], wy[0], &testx, &testy, &child);
        BehindWindow = (child != 0);
    //}

    /* Seemingly endless loop. */
    while (TRUE)
    {
        /* <=- Wasp -=> */
        /* Age the position arrays. */
        wx[2] = wx[1];
        wx[1] = wx[0];
        wy[2] = wy[1];
        wy[1] = wy[0];

        //if (pointer)
        //{
            //XQueryPointer(display.dpy, display.win, &garbage_win,
                            //&garbage_win, &garbage_int, &garbage_int,
                            //&wx[0], &wy[0], (unsigned int *) &garbage_int);
        //}
        //else
        {
            /* Accelerate */
            wxv += RAND(wasp_acc);
            wyv += RAND(wasp_acc);

            /* Speed Limit Checks */
            if (wxv > wasp_vel) wxv = wasp_vel;
            if (wxv < -wasp_vel) wxv = -wasp_vel;
            if (wyv > wasp_vel) wyv = wasp_vel;
            if (wyv < -wasp_vel) wyv = -wasp_vel;

            /* Move */
            wx[0] = wx[1] + wxv;
            wy[0] = wy[1] + wyv;

            Bounce_Against_Windows(wx, wy, &wxv, &wyv, BehindWindow);

            /* Bounce Checks */
            if ((wx[0] < border) || (wx[0] > winW-border-1))
            {
                wxv = -wxv;
                wx[0] += wxv<<1;
            }
            if ((wy[0] < border) || (wy[0] > winH-border-1))
            {
                wyv = -wyv;
                wy[0] += wyv<<1;
            }
        }

        /* Don't let things settle down. */
        xv[random() % bees] += RAND(3);
        yv[random() % bees] += RAND(3);

        /* <=- Bees -=> */
        for (b = 0 ; b < bees ; b++)
        {
            /* Age the arrays. */
            X(2,b) = X(1,b);
            X(1,b) = X(0,b);
            Y(2,b) = Y(1,b);
            Y(1,b) = Y(0,b);

            /* Accelerate */
            dx = wx[1] - X(1,b);
            dy = wy[1] - Y(1,b);
            distance = abs(dx)+abs(dy); /* approximation */
            if (distance == 0) distance = 1;
            xv[b] += (dx*bee_acc)/distance;
            yv[b] += (dy*bee_acc)/distance;

            /* Speed Limit Checks */
            if (xv[b] > bee_vel) xv[b] = bee_vel;
            if (xv[b] < -bee_vel) xv[b] = -bee_vel;
            if (yv[b] > bee_vel) yv[b] = bee_vel;
            if (yv[b] < -bee_vel) yv[b] = -bee_vel;

            /* Move */
            X(0,b) = X(1,b) + xv[b];
            Y(0,b) = Y(1,b) + yv[b];

            /* Fill the segment lists. */
            segs[b].x1 = X(0,b);
            segs[b].y1 = Y(0,b);
            segs[b].x2 = X(1,b);
            segs[b].y2 = Y(1,b);
            old_segs[b].x1 = X(1,b);
            old_segs[b].y1 = Y(1,b);
            old_segs[b].x2 = X(2,b);
            old_segs[b].y2 = Y(2,b);
        }

        /* Erase previous, draw current, sync for smoothness. */

        /* Again check to see if the wasp is behind a window. */
        //if (stay_in_front)
        //{
            //XTranslateCoordinates(display.dpy, display.win, display.win,
                //wx[0], wy[0],
                //&testx, &testy, &child);
            //BehindWindow = (child != 0);
        //}

        /* Wasp */
        if (xor)
        {
            if (init == FALSE)
                XDrawLine(display.dpy, display.win, display.wasp_xor_gc,
                    wx[1], wy[1], wx[2], wy[2]);
            XDrawLine(display.dpy, display.win, display.wasp_xor_gc,
                wx[0], wy[0], wx[1], wy[1]);
        }
        else
        {
            if (init == FALSE)
                XDrawLine(display.dpy, display.win, display.erase_gc,
                    wx[1], wy[1], wx[2], wy[2]);
            XDrawLine(display.dpy, display.win, display.wasp_gc,
                wx[0], wy[0], wx[1], wy[1]);
        }

        /* Bees */
        if (xor)
        {
            if (init == FALSE)
                XDrawSegments(display.dpy, display.win, display.bee_xor_gc,
                    old_segs, bees);
            else
                init = FALSE;
            XDrawSegments(display.dpy, display.win, display.bee_xor_gc,
                segs, bees);
        }
        else
        {
            if (init == FALSE)
                XDrawSegments(display.dpy, display.win, display.erase_gc,
                    old_segs, bees);
            else
                init = FALSE;
            XDrawSegments(display.dpy, display.win, display.bee_gc, segs, bees);
        }

        XSync(display.dpy, False);

        /* Check for events. */
        //if (XPending(display.dpy))
        {
            //XNextEvent(display.dpy, &xev);
            //HandleEvent(&xev);

            /* Make sure the wasp is within the window. */
            if (wx[0] < border) wx[0] = border;
            if (wx[0] > winW-border-1) wx[0] = winW-border-1;
            if (wy[0] < border) wy[0] = border;
            if (wy[0] > winH-border-1) wy[0] = winH-border-1;
        }

        /* Clean up and shut down. */
        if (stop)
        {
            stop = FALSE; /* reset the "stop" variable */

            if (xor)
            {
                XDrawSegments(display.dpy, display.win, display.bee_xor_gc,
                    segs, bees);
                XDrawLine(display.dpy, display.win, display.wasp_xor_gc,
                    wx[0], wy[0], wx[1], wy[1]);
            }
            else
                XFillRectangle(display.dpy, display.win, display.erase_gc,
                    0,0, winW, winH);
            XSync(display.dpy, 0);

            if (!root)
            {
                int             xc, yc;
                unsigned int    width, height, border_width, depth;
                Window          root_window;

                /* This is for debugging. */
                /* I was finding that when using xswarm as a screen     */
                /* saver, I would come back after an hour, and it       */
                /* would be in a little shriveled window in the         */
                /* corner.                                              */
#ifdef DEBUG
                XGetGeometry(display.dpy, display.win, &root_window,
                    &xc, &yc, &width, &height, &border_width, &depth);
                printf("Window info: x,y,width,height,border,depth\n");
                printf("%d,%d  %d,%d  %d  %d\n",xc,yc,width,height,
                    border_width,depth);
                printf("Wasp x,y xv,yv: %d,%d %d,%d\n",wx[0],wy[0],wxv,wyv);
                printf("Bee 1 x,y xv,yv: %d %d %d %d\n",
                    X(0,1),Y(0,1),
                    *(xv+1),yv[1]);
#endif

                XDestroyWindow(display.dpy, display.win);
            }

            return;
        }

        /* Delay so we don't use all of the cpu time. */
#ifdef HP_UX
        if (delay != 0) usleep(delay);
#else
        if (delay != 0) usleep(delay);
#endif
    }
}


void Bounce_Against_Windows(wx, wy, wxv, wyv, BehindWindow)
int     wx[3], wy[3];
int     *wxv, *wyv;
int     BehindWindow;
{
    int         testx, testy;
    Window      child;


    /* This makes the wasp avoid moving behind windows. */
    if (root && stay_in_front && !BehindWindow)
    {
        //XTranslateCoordinates(display.dpy, display.win, display.win,
            //wx[0], wy[0], &testx, &testy, &child);
        if (child != 0)
        {
            //XTranslateCoordinates(display.dpy, display.win, display.win,
                //wx[0] - (*wxv), wy[0], &testx, &testy, &child);
            if (child == 0) /* vertical surface */
            {
                wx[0] = wx[1] - (*wxv); /* undo the jump into the window */
                *wxv = -(*wxv);        /* bounce in x direction */
            }
            else
            {
                //XTranslateCoordinates(display.dpy, display.win,display.win,
                    //wx[0], wy[0] - (*wyv), &testx, &testy, &child);
                if (child == 0) /* horizontal surface */
                {
                    wy[0] = wy[1] - (*wyv); /* undo the jump into the window */
                    *wyv = - (*wyv);        /* bounce in y direction */
                }
                else
                { /* concave corner */
                    wx[0] = wx[1] - (*wxv); /* undo the jump into the corner */
                    wy[0] = wy[1] - (*wyv);
                    *wxv = - (*wxv);        /* reverse direction */
                    *wyv = - (*wyv);
                }
            }
        }
    }
}


#if 0
/*
** HandleEvent()
**
** process X events
*/

void HandleEvent(event)
XEvent  *event;
{
    /* If the screen saver is on, then watch for signs of activity. */
    if (((event->type == KeyPress) || (event->type == MotionNotify)) &&
        (timeout)) stop = TRUE;

    switch (event->type)
    {
        case ClientMessage: /* sent by f.delete from twm */
            {
                XClientMessageEvent     *ev = (XClientMessageEvent *) event;

                printf("Client message received.\n");
                if (ev->message_type == display.protocol_atom &&
                    ev->data.l[0] == display.kill_atom)
                    stop = TRUE;
            }
            break;
        case ConfigureNotify:
            {
                XConfigureEvent *ev = (XConfigureEvent *) event;
                winW = ev->width;
                winH = ev->height;
                winX = ev->x;
                winY = ev->y;
            };
            break;
        case KeyPress:
            {
                XKeyEvent *key_event = (XKeyEvent *) event;
                char buf[128];
                KeySym ks;
                XComposeStatus status;

                XLookupString(key_event,buf,128,&ks,&status);
                if (buf[0]=='q' || buf[0]=='Q')
                    stop = TRUE;
            }
            break;
        case ButtonPress:
            pointer = !pointer;
            break;
        default:
            break;
    }
}


#ifndef HP_UX

/*  Put the process to sleep for a while.  */
void nap(sec,usec)
long sec, usec;
{
    static struct timeval tv;

    tv.tv_sec = sec;
    tv.tv_usec = usec;
    select(0, 0, 0, 0, &tv);
}

#else

static void alarmhandler() 
{
}

sleepms(msec)
int msec;
{
    struct itimerval value,ovalue;
    struct sigvec vec;
    long savemask, sigblock(), sigpause();

    vec.sv_handler = alarmhandler;
    vec.sv_mask = 0x0;
    vec.sv_flags = 0;
    sigvector(SIGALRM, &vec, &vec); /* Set up alarmhandler for SIGALRM */
    savemask = sigblock((long)(1L << (SIGALRM - 1)));

    value.it_interval.tv_sec = 0;
    value.it_interval.tv_usec = 0;
    value.it_value.tv_sec = msec/1000;
    value.it_value.tv_usec = (msec%1000)*1000;
    setitimer(ITIMER_REAL,&value,&ovalue);

    (void)sigpause(0L);
    (void)sigsetmask(savemask);

    sigvector(SIGALRM, &vec, NULL); /* Restore previous signal handler */
}

usleep(us)
long us;
{
    sleepms(us / 1000);
}

#endif


void
Usage(program)
char *program;
{
    printf("%s [options]  where options are listed below\n", program);
    printf("-h|q|?              display this message\n");
    printf("-v          give verbose list of current settings\n");
    printf("-p          use mouse (pointer) button to control wasp\n");
    printf("-r          use root window\n");
    printf("-V          stay in clear area of the screen (only with -r)\n");
    printf("-x          use the logical xor raster operation (saves the root bitmap)\n");
    printf("-g geometry         window geometry\n");
    printf("-d host:display     X server to connect to\n");
    printf("-t timeout  screen saved after 'timeout' seconds\n");
    printf("-D delay    non-blocking delay between updates (milliseconds)\n");
    printf("-b bees             number of bees\n");
    printf("-a bee_acc  bee acceleration in pixels per frame per frame\n");
    printf("-s bee_vel  bee speed limit in pixels per frame\n");
    printf("-A wasp_acc wasp max acceleration in pixels per frame per frame\n");
    printf("-S wasp_vel wasp speed limit in pixels per frame\n");
    printf("-B width    border width that wasp can't cross\n");
    printf("-w wasp_clr wasp color\n");
    printf("-c bee_clr  bee color\n");
    printf("-C bg_color background color\n");
    printf("\nPress any mouse button in the window to grab/release the wasp.\n");
    printf("\nPress q in the window to stop the insanity.\n\n");
}


void
HandleError(description, degree)
char    *description;
int     degree;
{
    fprintf(stderr, "An error has occurred.  The description is below...\n");
    fprintf(stderr, "%s\n", description);

    if (degree == FATAL)
    {
        fprintf(stderr, "Program aborting...\n");
        exit(-1);
    }
}
#endif

long
GetColor(display, color, final_color)
disp            *display;
char            *color;
XColor          *final_color;
{
    return RGB(255,255,255);
#if 0
    XColor      cdef;
    char        error_str[STD_STR];

    if (!XParseColor(display->dpy, display->cmap, color, &cdef) ||
        !XAllocColor(display->dpy, display->cmap, &cdef))
    {
        sprintf(error_str, "Color \"%s\" wasn't found.", color);
        //HandleError(error_str, FATAL);
    }

    /* Copy the final color. */
    if (final_color != NULL) *final_color = cdef;

    return(cdef.pixel);
#endif
}
