#ifndef RENDERER_H
#define RENDERER_H

#include <SDL.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __GNUC__
#define UNUSED __attribute__((__unused__))
#else
#define UNUSED
#endif

#ifdef LITE_USE_SDL_RENDERER
#define RECT_TYPE double
#else
#define RECT_TYPE int
#endif

#define FONT_FALLBACK_MAX 10
typedef struct RenFont RenFont;
typedef enum { FONT_HINTING_NONE, FONT_HINTING_SLIGHT, FONT_HINTING_FULL } ERenFontHinting;
typedef enum { FONT_ANTIALIASING_NONE, FONT_ANTIALIASING_GRAYSCALE, FONT_ANTIALIASING_SUBPIXEL } ERenFontAntialiasing;
typedef enum { FONT_STYLE_BOLD = 1, FONT_STYLE_ITALIC = 2, FONT_STYLE_UNDERLINE = 4, FONT_STYLE_SMOOTH = 8, FONT_STYLE_STRIKETHROUGH = 16 } ERenFontStyle;
typedef enum { FONT_FAMILY, FONT_SUBFAMILY, FONT_ID, FONT_FULLNAME, FONT_VERSION, FONT_PSNAME, FONT_TFAMILY, FONT_TSUBFAMILY, FONT_WWSFAMILY, FONT_WWSSUBFAMILY } EFontMetaTag;
typedef struct { uint8_t b, g, r, a; } RenColor;
typedef struct { RECT_TYPE x, y, width, height; } RenRect;
typedef struct { SDL_Surface *surface; double scale_x, scale_y; } RenSurface;
typedef struct { EFontMetaTag tag; char *value; size_t len; } FontMetaData;

struct RenWindow;
typedef struct RenWindow RenWindow;
extern RenWindow window_renderer;

RenFont* ren_font_load(RenWindow *window_renderer, const char *filename, float size, ERenFontAntialiasing antialiasing, ERenFontHinting hinting, unsigned char style);
RenFont* ren_font_copy(RenWindow *window_renderer, RenFont* font, float size, ERenFontAntialiasing antialiasing, ERenFontHinting hinting, int style);
const char* ren_font_get_path(RenFont *font);
void ren_font_free(RenFont *font);
int ren_font_get_metadata(const char *path, FontMetaData **data, int *count, bool *monospaced);
int ren_font_group_get_tab_size(RenFont **font);
int ren_font_group_get_height(RenFont **font);
float ren_font_group_get_size(RenFont **font);
void ren_font_group_set_size(RenWindow *window_renderer, RenFont **font, float size);
void ren_font_group_set_tab_size(RenFont **font, int n);
double ren_font_group_get_width(RenWindow *window_renderer, RenFont **font, const char *text, size_t len);
double ren_draw_text(RenSurface *rs, RenFont **font, const char *text, size_t len, float x, float y, RenColor color);

void ren_draw_rect(RenSurface *rs, RenRect rect, RenColor color);

void ren_init(SDL_Window *win);
void ren_resize_window(RenWindow *window_renderer);
void ren_update_rects(RenWindow *window_renderer, RenRect *rects, int count);
void ren_set_clip_rect(RenWindow *window_renderer, RenRect rect);
void ren_get_size(RenWindow *window_renderer, int *x, int *y); /* Reports the size in points. */
void ren_free_window_resources(RenWindow *window_renderer);
float ren_get_scale_factor(SDL_Window *win);


#endif
