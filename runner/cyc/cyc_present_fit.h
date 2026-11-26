/* cyc_present_fit.h - the window's one fixed scale for a game's picture that
 * asks for it (CycHostExtras.sharp_scale), whatever the player's IntegerScale
 * and LinearFilter. A game with a fixed canvas asks so that every player sees
 * the same, tested, picture (Faxanadu DX #107: Seamless's 424x240):
 *
 *   - the largest scale that fits the drawable, keeping the picture's shape,
 *     so there are bars only where the window's shape differs from it;
 *   - at a whole-number scale, nearest-neighbour (424x240 at 720p, 1440p and
 *     4K: 3x, 6x, 9x);
 *   - otherwise sharp: nearest-neighbour up to the next whole multiple, then
 *     a linear scale down to the fit (1080p: 4.5x, 1908x1080, from 5x,
 *     2120x1200). Each picture pixel keeps its exact colour but for one
 *     blended output pixel at most at each edge, where the fit's pixel grid
 *     cuts it, as Shovel Knight scales at 4.5x;
 *   - below 1x (a drawable smaller than the picture), a linear scale down:
 *     the whole picture, smaller.
 *
 * The drawable is in pixels (SDL_GetRendererOutputSize), never the window's
 * points: on a HiDPI display they differ. SDL's logical size, with integer
 * scaling off, letterboxes the same sizes to within a pixel, so the window's
 * overlays and the mouse, which use the logical size, still land on the
 * picture. Pure: present_fit_test.c checks it without SDL.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    CYC_FIT_NEAREST,   /* a whole multiple: nearest-neighbour, straight to the fit */
    CYC_FIT_SHARP,     /* nearest-neighbour to `multiple`, then linear down to the fit */
    CYC_FIT_LINEAR,    /* below 1x: linear down to the fit */
} CycFitFilter;

typedef struct {
    int x, y, w, h;       /* the picture's place in the drawable, centred, in pixels */
    int multiple;         /* the whole multiple drawn nearest-neighbour: the scale
                           * itself (NEAREST), the next one up (SHARP), 1 (LINEAR) */
    CycFitFilter filter;
} CycFit;

/* A pw x ph picture in an ow x oh drawable. The scale is ow/pw or oh/ph,
 * whichever is smaller, kept as that fraction so a whole number is exact. */
static inline CycFit cyc_present_fit(int pw, int ph, int ow, int oh)
{
    CycFit fit = { 0, 0, 0, 0, 1, CYC_FIT_LINEAR };
    if (pw <= 0 || ph <= 0 || ow <= 0 || oh <= 0) return fit;
    bool by_width = (int64_t)ow * ph <= (int64_t)oh * pw;   /* ow/pw <= oh/ph */
    int64_t num = by_width ? ow : oh, den = by_width ? pw : ph;
    fit.w = by_width ? ow : (int)((int64_t)pw * oh / ph);
    fit.h = by_width ? (int)((int64_t)ph * ow / pw) : oh;
    if (fit.w < 1) fit.w = 1;
    if (fit.h < 1) fit.h = 1;
    fit.x = (ow - fit.w) / 2;
    fit.y = (oh - fit.h) / 2;
    if (num < den) {
        fit.filter = CYC_FIT_LINEAR;
        fit.multiple = 1;
    } else if (num % den == 0) {
        fit.filter = CYC_FIT_NEAREST;
        fit.multiple = (int)(num / den);
    } else {
        fit.filter = CYC_FIT_SHARP;
        fit.multiple = (int)(num / den) + 1;
    }
    return fit;
}
