#include <SDL.h>
#include "renderer.h"

struct RenWindow {
  SDL_Window *window;
  RenRect clip; /* Clipping rect in pixel coordinates. */
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  SDL_Surface *surface;
  float surface_scale_x;
  float surface_scale_y;
};
typedef struct RenWindow RenWindow;

void renwin_init_surface(RenWindow *ren);
void renwin_surface_scale(RenWindow *ren, float *scale_x, float *scale_y);
void renwin_clip_to_surface(RenWindow *ren);
void renwin_set_clip_rect(RenWindow *ren, RenRect rect);
void renwin_resize_surface(RenWindow *ren);
void renwin_show_window(RenWindow *ren);
void renwin_update_rects(RenWindow *ren, RenRect *rects, int count);
void renwin_free(RenWindow *ren);
SDL_Surface *renwin_get_surface(RenWindow *ren);
