#include <SDL2/SDL.h>

struct sdl_window;

int sdl_init(void);
struct sdl_window *sdl_create_window(Drawable *dp);
int sdl_key(Uint8 state, SDL_Keysym sym);
