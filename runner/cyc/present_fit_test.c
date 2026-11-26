/* The window's fixed scale for a game's picture (cyc_present_fit.h): the fit,
 * its filter and its whole multiple at the displays a 424x240 canvas meets
 * (Faxanadu DX #107's cases), every drawable size up to 4K, and what the
 * sharp scale does to a picture's pixels along one axis, the way a GPU's (and
 * SDL's software) linear filter samples: centred on each output pixel. */
#include "cyc_present_fit.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static const char *name(CycFitFilter f)
{
    return f == CYC_FIT_NEAREST ? "nearest" : f == CYC_FIT_SHARP ? "sharp" : "linear";
}

/* One drawable: the fit expected, and what it was. */
static void expect(int pw, int ph, int ow, int oh, int x, int y, int w, int h, CycFitFilter filter, int multiple)
{
    CycFit f = cyc_present_fit(pw, ph, ow, oh);
    bool ok = f.x == x && f.y == y && f.w == w && f.h == h && f.filter == filter && f.multiple == multiple;
    printf("%s %dx%d in %dx%d: %dx%d at (%d,%d), %s, multiple %d\n", ok ? "ok  " : "FAIL", pw, ph, ow, oh,
           f.w, f.h, f.x, f.y, name(f.filter), f.multiple);
    if (!ok) {
        fprintf(stderr, "  expected %dx%d at (%d,%d), %s, multiple %d\n", w, h, x, y, name(filter), multiple);
        ++failures;
    }
}

/* Along one axis, n picture pixels drawn nearest-neighbour at k each (n*k
 * texels), then linearly to `out` pixels. Output pixel i samples texel
 * position u = (i + 0.5) * n*k / out - 0.5 and mixes texels floor(u) and
 * floor(u) + 1. It is exact when both are one picture pixel's (or the
 * second's weight is 0), and blended otherwise. Returns how many were
 * blended; *worst is the most at any one edge between two picture pixels,
 * *bare the picture pixels left with no exact output pixel at all. */
static int seams(int n, int k, int out, int *worst, int *bare)
{
    int texels = n * k, blended = 0, last_edge = -1, run = 0;
    int *exact = calloc((size_t)n, sizeof *exact);
    *worst = 0;
    for (int i = 0; i < out; ++i) {
        double u = (i + 0.5) * texels / out - 0.5;
        if (u < 0) u = 0;
        int t0 = (int)floor(u), t1 = t0 + 1 < texels ? t0 + 1 : t0;
        double weight = u - t0;
        if (t0 / k == t1 / k || weight == 0) {
            exact[t0 / k]++;
            continue;
        }
        ++blended;
        int edge = t1 / k;                       /* between picture pixels edge-1 and edge */
        run = edge == last_edge ? run + 1 : 1;
        last_edge = edge;
        if (run > *worst) *worst = run;
    }
    *bare = 0;
    for (int p = 0; p < n; ++p) *bare += exact[p] == 0;
    free(exact);
    return blended;
}

int main(void)
{
    /* #107's cases, for Seamless's 424x240. */
    expect(424, 240, 1920, 1080, 6, 0, 1908, 1080, CYC_FIT_SHARP, 5);      /* 1080p: 4.5x, from 5x (2120x1200) */
    expect(424, 240, 1280, 720, 4, 0, 1272, 720, CYC_FIT_NEAREST, 3);      /* 720p */
    expect(424, 240, 2560, 1440, 8, 0, 2544, 1440, CYC_FIT_NEAREST, 6);    /* 1440p */
    expect(424, 240, 3840, 2160, 12, 0, 3816, 2160, CYC_FIT_NEAREST, 9);   /* 4K: 12-pixel bars */
    expect(424, 240, 1080, 1920, 0, 654, 1080, 611, CYC_FIT_SHARP, 3);     /* a tall window: letterboxed */
    expect(424, 240, 300, 600, 0, 215, 300, 169, CYC_FIT_LINEAR, 1);       /* narrow and small: scaled down */
    expect(424, 240, 212, 120, 0, 0, 212, 120, CYC_FIT_LINEAR, 1);         /* tiny: the whole view, at half */
    expect(424, 240, 64, 60, 0, 12, 64, 36, CYC_FIT_LINEAR, 1);
    expect(424, 240, 424, 240, 0, 0, 424, 240, CYC_FIT_NEAREST, 1);        /* its own size */
    expect(424, 240, 848, 480, 0, 0, 848, 480, CYC_FIT_NEAREST, 2);        /* exactly its shape */
    expect(424, 240, 3440, 1440, 448, 0, 2544, 1440, CYC_FIT_NEAREST, 6);  /* ultrawide: pillarboxed */
    expect(424, 240, 2560, 1600, 0, 75, 2560, 1449, CYC_FIT_SHARP, 7);     /* 16:10: 6.04x */
    expect(424, 240, 3024, 1964, 0, 126, 3024, 1711, CYC_FIT_SHARP, 8);    /* a 14" MacBook's pixels: 7.13x */
    expect(424, 240, 1, 1, 0, 0, 1, 1, CYC_FIT_LINEAR, 1);
    expect(424, 240, 0, 1080, 0, 0, 0, 0, CYC_FIT_LINEAR, 1);              /* no drawable: nothing */
    expect(256, 240, 1920, 1080, 384, 0, 1152, 1080, CYC_FIT_SHARP, 5);    /* any canvas: 256x240 too */

    /* Every drawable to 4K: inside it, centred, touching two sides, its
     * shape kept to a pixel, and the filter the scale calls for. */
    long sizes = 0;
    for (int ow = 1; ow <= 3840; ow += ow < 600 ? 1 : 7)
        for (int oh = 1; oh <= 2160; oh += oh < 400 ? 1 : 5) {
            CycFit f = cyc_present_fit(424, 240, ow, oh);
            ++sizes;
            CHECK(f.x >= 0 && f.y >= 0 && f.x + f.w <= ow && f.y + f.h <= oh);
            CHECK(f.x == (ow - f.w) / 2 && f.y == (oh - f.h) / 2);
            CHECK(f.w == ow || f.h == oh || f.w == 1 || f.h == 1);
            CHECK(labs((long)f.w * 240 - (long)f.h * 424) <= 424);
            if (f.filter == CYC_FIT_NEAREST) CHECK(f.w == 424 * f.multiple && f.h == 240 * f.multiple);
            else if (f.filter == CYC_FIT_SHARP)
                CHECK(f.multiple >= 2 && f.w > 424 * (f.multiple - 1) && f.w < 424 * f.multiple &&
                      (f.h < 240 * f.multiple));
            else CHECK(f.multiple == 1 && f.w <= 424 && f.h <= 240 && (f.w < 424 || f.h < 240));
        }
    printf("%ld drawables from 1x1 to 3840x2160 fit inside, centred, in shape\n", sizes);

    /* The sharp scale keeps pixels crisp: at most one blended output pixel
     * at each edge between two picture pixels, and from 2x up every picture
     * pixel keeps at least one exact output pixel. Across both axes of every
     * sharp fit of the sweep's widths (and 1080p's 4.5x: 1908 of 424). */
    int worst_seam = 0, sharp_fits = 0;
    for (int ow = 425; ow <= 3840; ++ow) {
        CycFit f = cyc_present_fit(424, 240, ow, 2160 * 4);   /* fitted by width */
        if (f.filter != CYC_FIT_SHARP) continue;
        int worst, bare;
        seams(424, f.multiple, f.w, &worst, &bare);
        ++sharp_fits;
        if (worst > worst_seam) worst_seam = worst;
        CHECK(worst <= 1);
        if (f.w >= 2 * 424) CHECK(bare == 0);
    }
    for (int oh = 241; oh <= 2160; ++oh) {
        CycFit f = cyc_present_fit(424, 240, 3840 * 4, oh);  /* fitted by height */
        if (f.filter != CYC_FIT_SHARP) continue;
        int worst, bare;
        seams(240, f.multiple, f.h, &worst, &bare);
        ++sharp_fits;
        if (worst > worst_seam) worst_seam = worst;
        CHECK(worst <= 1);
        if (f.h >= 2 * 240) CHECK(bare == 0);
    }
    printf("%d sharp fits: at most %d blended pixel at an edge\n", sharp_fits, worst_seam);
    int worst, bare, blended = seams(424, 5, 1908, &worst, &bare);
    printf("1080p across: 424 pixels at 5x down to 1908: %d of 1908 blended, at most %d at an edge, %d pixels without an exact one\n",
           blended, worst, bare);
    CHECK(worst == 1 && bare == 0 && blended > 0 && blended < 424);
    /* Linear straight from the picture (no whole multiple first) blurs
     * every edge over several pixels: what the sharp scale avoids. */
    int plain_worst;
    seams(424, 1, 1908, &plain_worst, &bare);
    printf("1080p across, linear from 1x: at most %d blended pixels at an edge\n", plain_worst);
    CHECK(plain_worst > 1);

    printf("present_fit_test: %u failures\n", failures);
    return failures != 0;
}
