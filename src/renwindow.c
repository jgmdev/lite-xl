#include <assert.h>
#include <stdio.h>
#include "renwindow.h"

#ifdef LITE_USE_SDL_RENDERER
static void update_surface_scale(RenWindow *ren) {
  int w_pixels, h_pixels;
  int w_points, h_points;
  SDL_GL_GetDrawableSize(ren->window, &w_pixels, &h_pixels);
  SDL_GetWindowSize(ren->window, &w_points, &h_points);
  float scaleX = (float) w_pixels / (float) w_points;
  float scaleY = (float) h_pixels / (float) h_points;
  ren->surface_scale_x = scaleX;
  ren->surface_scale_y = scaleY;
}

static void setup_renderer(RenWindow *ren, int w, int h) {
  /* Note that w and h here should always be in pixels and obtained from
     a call to SDL_GL_GetDrawableSize(). */
  if (ren->renderer) {
    SDL_DestroyTexture(ren->texture);
    SDL_DestroyRenderer(ren->renderer);
  }
  ren->renderer = SDL_CreateRenderer(ren->window, -1, 0);
  ren->texture = SDL_CreateTexture(ren->renderer, SDL_PIXELFORMAT_BGRA32, SDL_TEXTUREACCESS_STREAMING, w, h);
  update_surface_scale(ren);
}
#endif


void renwin_init_surface(UNUSED RenWindow *ren) {
#ifdef LITE_USE_SDL_RENDERER
  if (ren->surface) {
    SDL_FreeSurface(ren->surface);
  }
  int w, h;
  SDL_GL_GetDrawableSize(ren->window, &w, &h);
  ren->surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_BGRA32);
  if (!ren->surface) {
    fprintf(stderr, "Error creating surface: %s", SDL_GetError());
    exit(1);
  }
  setup_renderer(ren, w, h);
#endif
}

void renwin_surface_scale(UNUSED RenWindow *ren, float *scale_x, float *scale_y) {
#ifdef LITE_USE_SDL_RENDERER
  if (scale_x)
    *scale_x = ren->surface_scale_x;
  if (scale_y)
    *scale_y = ren->surface_scale_y;
#else
  if (scale_x)
    *scale_x = 1;
  if (scale_y)
    *scale_y = 1;
#endif
}


static RenRect scaled_rect(const RenRect rect, RenWindow *ren) {
#ifdef LITE_USE_SDL_RENDERER
  float scale_x = ren->surface_scale_x;
  float scale_y = ren->surface_scale_y;
#else
  float scale_x = 1;
  float scale_y = 1;
#endif
  return (RenRect) {
    rect.x * scale_x,
    rect.y * scale_y,
    rect.width * scale_x,
    rect.height * scale_y
  };
}


void renwin_clip_to_surface(RenWindow *ren) {
  SDL_Surface *surface = renwin_get_surface(ren);
  ren->clip = (RenRect) {0, 0, surface->w, surface->h};
}


void renwin_set_clip_rect(RenWindow *ren, RenRect rect) {
  ren->clip = scaled_rect(rect, ren);
}


SDL_Surface *renwin_get_surface(RenWindow *ren) {
#ifdef LITE_USE_SDL_RENDERER
  return ren->surface;
#else
  SDL_Surface *surface = SDL_GetWindowSurface(ren->window);
  if (!surface) {
    fprintf(stderr, "Error getting window surface: %s", SDL_GetError());
    exit(1);
  }
  return surface;
#endif
}

void renwin_resize_surface(UNUSED RenWindow *ren) {
#ifdef LITE_USE_SDL_RENDERER
  int new_w, new_h;
  SDL_GL_GetDrawableSize(ren->window, &new_w, &new_h);
  /* Note that (w, h) may differ from (new_w, new_h) on retina displays. */
  if (new_w != ren->surface->w || new_h != ren->surface->h) {
    renwin_init_surface(ren);
    renwin_clip_to_surface(ren);
    setup_renderer(ren, new_w, new_h);
  }
#endif
}

void renwin_show_window(RenWindow *ren) {
  SDL_ShowWindow(ren->window);
}

void renwin_update_rects(RenWindow *ren, RenRect *rects, int count) {
#ifdef LITE_USE_SDL_RENDERER
  const float scale_x = ren->surface_scale_x;
  const float scale_y = ren->surface_scale_y;
  for (int i = 0; i < count; i++) {
    const RenRect *r = &rects[i];
    const int x = scale_x * r->x, y = scale_y * r->y;
    const int w = scale_x * r->width, h = scale_y * r->height;
    const SDL_Rect sr = {.x = x, .y = y, .w = w, .h = h};
    int32_t *pixels = ((int32_t *) ren->surface->pixels) + x + ren->surface->w * y;
    SDL_UpdateTexture(ren->texture, &sr, pixels, ren->surface->w * 4);
  }
  SDL_RenderCopy(ren->renderer, ren->texture, NULL, NULL);
  SDL_RenderPresent(ren->renderer);
#else
  SDL_UpdateWindowSurfaceRects(ren->window, (SDL_Rect*) rects, count);
#endif
}

void renwin_free(RenWindow *ren) {
#ifdef LITE_USE_SDL_RENDERER
  SDL_DestroyTexture(ren->texture);
  SDL_DestroyRenderer(ren->renderer);
  SDL_FreeSurface(ren->surface);
#endif
  SDL_DestroyWindow(ren->window);
  ren->window = NULL;
}
