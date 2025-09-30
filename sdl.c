/* SDL backend for GFX library */
#include "draw.h"
#include <SDL2/SDL.h>

struct sdl_window {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    float zoom;
};

int sdl_init(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        printf("Can't initialize SDL\n");
        return 0;
    }
    return 1;
}

struct sdl_window *sdl_create_window(Drawable *dp)
{
    struct sdl_window *sdl;
    int pixelformat;

    sdl = malloc(sizeof(struct sdl_window));
    if (!sdl) return 0;

    sdl->zoom = 1.0;
    sdl->window = SDL_CreateWindow("Graphics Console",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        dp->width*sdl->zoom, dp->height*sdl->zoom,
        SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    if (!sdl->window) {
        printf("SDL: Can't create window\n");
        return 0;
    }

    sdl->renderer = SDL_CreateRenderer(sdl->window, -1, 0);
    if (!sdl->renderer) {
        printf("SDL: Can't create renderer\n");
        return 0;
    }

    /* 
     * Set the SDL texture pixel format to match the framebuffer format
     * to eliminate pixel conversions.
     */
    switch (dp->pixtype) {
    case MWPF_TRUECOLORARGB:
        pixelformat = SDL_PIXELFORMAT_ARGB8888;
        break;
    case MWPF_TRUECOLORABGR:
        pixelformat = SDL_PIXELFORMAT_ABGR8888;
        break;
    default:
        printf("SDL: Unsupported pixel format %d\n", dp->pixtype);
        return 0;
    }

    sdl->texture = SDL_CreateTexture(sdl->renderer, pixelformat,
        SDL_TEXTUREACCESS_STREAMING, dp->width, dp->height);
    if (!sdl->texture) {
        printf("SDL: Can't create texture\n");
        return 0;
    }
    dp->window = sdl;

    SDL_RenderSetLogicalSize(sdl->renderer, dp->width, dp->height);
    SDL_RenderSetScale(sdl->renderer, sdl->zoom, sdl->zoom);
    SDL_SetRenderDrawColor(sdl->renderer, 0x00, 0x00, 0x00, 0x00);
    //SDL_ShowCursor(SDL_DISABLE);    /* hide SDL cursor */
    SDL_PumpEvents();

    return sdl;
}

/* convert keycode to shift value */
static SDL_Keycode key_shift(SDL_Keycode kc)
{
    if (kc >= 'a' && kc < 'z')
        return (kc ^ 0x20);  // upper case

    switch (kc) {
        case '`':  return '~';
        case '1':  return '!';
        case '2':  return '@';
        case '3':  return '#';
        case '4':  return '$';
        case '5':  return '%';
        case '6':  return '^';
        case '7':  return '&';
        case '8':  return '*';
        case '9':  return '(';
        case '0':  return ')';
        case '-':  return '_';
        case '=':  return '+';
        case '[':  return '{';
        case ']':  return '}';
        case '\\': return '|';
        case ';':  return ':';
        case '\'': return '"';
        case ',':  return '<';
        case '.':  return '>';
        case '/':  return '?';

        default: break;
    }

    return kc;
}

int sdl_key(Uint8 state, SDL_Keysym sym)
{
    SDL_Scancode sc = sym.scancode;
    SDL_Keycode kc = sym.sym;
    Uint16 mod = sym.mod;

    switch (sc) {
    case SDL_SCANCODE_MINUS:  kc = '-'; break;
    case SDL_SCANCODE_PERIOD: kc = '.'; break;
    case SDL_SCANCODE_SLASH:  kc = '/'; break;
    default: break;
    }

    if (kc == SDLK_LSHIFT || kc == SDLK_RSHIFT ||
        kc == SDLK_LCTRL || kc == SDLK_RCTRL)
        return 0;

    if (kc < 256 && (mod & (KMOD_SHIFT | KMOD_CAPS)))
        kc = key_shift(kc);

    if (kc < 256 && (mod & KMOD_CTRL))
        kc &= 0x1f;  // convert to control char

    if (kc == 0x007F) kc = '\b';  // convert DEL to BS

    return kc;
}

/* update SDL from drawable */
void draw_flush(Drawable *dp, int x, int y, int width, int height)
{
    struct sdl_window *sdl = (struct sdl_window *)dp->window;
    SDL_Rect r;

    r.x = x;
    r.y = y;
    r.w = width? width: dp->width;
    r.h = height? height: dp->height;
    unsigned char *pixels = dp->pixels + y * dp->pitch + x * dp->bytespp;
    SDL_UpdateTexture(sdl->texture, &r, pixels, dp->pitch);

    /* copy texture to display*/
    SDL_RenderClear(sdl->renderer);
    SDL_RenderCopy(sdl->renderer, sdl->texture, NULL, NULL);
    SDL_RenderPresent(sdl->renderer);
}
