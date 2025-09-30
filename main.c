/* GFX demo app */
#include <unistd.h>
#include <sys/select.h>
#include "draw.h"
#include "sdl.h"

extern int open_pty(void);
static int term_fd;

extern int angle;       /* in console.c */

static void clear_screen(Drawable *dp)
{
    draw_clear(dp);
    if (dp->font) {
        draw_font_string(dp, dp->font, "Use '{' or '}' to rotate text", 20, 20,
            0, 0, dp->fgcolor, dp->bgcolor, 1, 0);
    }
}

static void sendhost(const char *str)
{
    write(term_fd, str, strlen(str));
}

void tmt_callback(tmt_msg_t m, TMT *vt, const void *a, void *p)
{
    if (m == TMT_MSG_ANSWER)
        sendhost(a);
}

static int sdl_nextevent(struct console *con, struct console *con2)
{
    int c;
    SDL_Event event;
    static int w = 20;
    static int h = 10;

    //while (SDL_WaitEvent(&event)) {
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                return 1;

            case SDL_KEYDOWN:
                c = sdl_key(event.key.state, event.key.keysym);
                switch (c) {
                case '\0':  return 0;
                case SDLK_UP:
                    sendhost(TMT_KEY_UP);
                    return 0;
                case SDLK_DOWN:
                    sendhost(TMT_KEY_DOWN);
                    return 0;
                case SDLK_RIGHT:
                    sendhost(TMT_KEY_RIGHT);
                    return 0;
                case SDLK_LEFT:
                    sendhost(TMT_KEY_LEFT);
                    return 0;
                /* test cases follow */
                case '~':   return 1;
                case '_':   console_resize(con, --w, --h); return 0;
                case '+':   console_resize(con, ++w, ++h); return 0;
                case '{':   angle--;
                same:
                            clear_screen(con->dp);
                            console_dirty(con, 0, 0, con->cols, con->lines);
                            return 0;
                case '}':   angle++; goto same;
                }
                char c2 = c;
                write(term_fd, &c2, 1);
        }
    }
    fd_set fdset;
    struct timeval tv;
    int ret;
    char buf[256];

    FD_ZERO(&fdset);
    FD_SET(term_fd, &fdset);
    tv.tv_sec = 0;
    tv.tv_usec = 30000;
    ret = select(term_fd + 1, &fdset, NULL, NULL, &tv);
    if (ret > 0) {
        if (FD_ISSET(term_fd, &fdset)) {
            int n = read(term_fd, buf, sizeof(buf));
            if (n > 0)
                console_write(con, buf, n);
        }
    }

    return 0;
}

int main(int ac, char **av)
{
    Drawable *dp;
    struct sdl_window *sdl;

    if (!sdl_init()) exit(1);
    //if (!(dp = create_drawable(MWPF_DEFAULT, 640, 400))) exit(2);
    if (!(dp = create_drawable(MWPF_DEFAULT, 1024, 800))) exit(2);
    if (!(sdl = sdl_create_window(dp))) exit(3);

#if 1
    term_fd = open_pty();
    struct console *con;
    struct console *con2 = NULL;
    dp->font = font_load_font("times_30x37_8");
    //dp->font = font_load_font("mssans_11x13_8");
    if (!(con = create_console(80, 24))) exit(4);
    //console_load_font(con, "unifont_8x16_1");
    //console_load_font(con, "cour_11x19_8");
    //console_load_font(con, "cour_21x37_8");
    //console_load_font(con, "cour_20x37_1");
    //console_load_font(con, "mssans_11x13_8");
    //console_load_font(con, "rom_8x16_1");
    //console_load_font(con, "DOSJ-437.F19");
    //console_load_font(con, "VGA-ROM.F16");
    //console_load_font(con, "COMPAQP3.F16");
    //console_load_font(con, "DOSV-437.F16");
    //console_load_font(con, "CGA.F08");
#if !OLDWAY
    if (con->font->range == NULL)      /* not likely a unicode font */
        tmt_unicode_to_acs(con->vt, true);
#endif

    //if (!(con2 = create_console(14, 8))) exit(4);
    //con2->dp = dp;
    //console_load_font(con2, "cour_20x37_1");
    //console_load_font(con2, "cour_21x37_8");
    //console_load_font(con2, "DOSJ-437.F19");
    clear_screen(dp);
    draw_flush(dp, 0, 0, 0, 0);

#if 2
    /* test invalid UTF-8 */
    //console_putchar(con, 0xc0);
    //console_putchar(con2, 0xc1);
    /* test undefined glyph */
    //console_putchar(con, 127);
    //console_putchar(con2, 127);
    /* test unicode output */
    for (int i=0x00A1; i<=0x00AF; i++) {
        char buf[32];
        int n = xwctomb(buf, i);
        if (n > 0)
            console_write(con, buf, n);
    }
#endif

    write(term_fd, "TERM=ansi\n", 10);
    for (;;) {
        //Rect update = con->update;          /* save update rect for dup console */
        int flush = angle? 2: 0;
        draw_console(con, dp, 3*8, 5*15, flush);
        //con->update = update;
        //draw_console(con2, dp, 42*8, 5*15, flush);
        draw_flush(dp, 0, 0, 0, 0);
        if (sdl_nextevent(con, con2))
            break;
        //continue;
        //int x1 = random() % 640;
        //int x2 = random() % 640;
        //int y1 = random() % 400;
        //int y2 = random() % 400;
        //int r = random() % 40;
        //draw_line(dp, x1, y1, x2, y2);
        //draw_thick_line(dp, x1, y1, x2, y2, 6);
        //draw_circle(dp, x1, y1, r);
        //draw_flood_fill(dp, x1, y1);
        //draw_rect(dp, x1, y1, x2, y2);
        //draw_fill_rect(dp, x1, y1, x2, y2);
    }
    free(con);
#endif

    SDL_Quit();
    free(sdl);
    free(dp);
}
