#ifndef __WINE_OHOS_SURFACE_LUT_H
#define __WINE_OHOS_SURFACE_LUT_H

#include "windef.h"

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

/*
 * Alpha Blend Lookup Table (LUT)
 *
 * Thread safety analysis:
 * - alpha_blend_lut[][] is a static global (64KB)
 * - Once initialized, data is immutable (read-only for lifetime)
 * - Initialization happens in ohos_init() at DLL_PROCESS_ATTACH
 * - DllMain with DisableThreadLibraryCalls guarantees single-threaded init
 * - No mutex needed for reads after init completes
 * - No race condition possible: init before any surface flush calls
 */

static DWORD alpha_blend_lut[256][256];

static inline void init_alpha_blend_lut(void)
{
    for (int a = 0; a < 256; a++)
        for (int c = 0; c < 256; c++)
            alpha_blend_lut[a][c] = (c * a) >> 8;
}

static inline DWORD alpha_blend_lut_pixel(DWORD src_pixel, BYTE alpha)
{
    DWORD b = src_pixel & 0xff;
    DWORD g = (src_pixel >> 8) & 0xff;
    DWORD r = (src_pixel >> 16) & 0xff;

    return ((alpha << 24) |
            (alpha_blend_lut[alpha][r] << 16) |
            (alpha_blend_lut[alpha][g] << 8) |
            alpha_blend_lut[alpha][b]);
}

#ifdef __ARM_NEON

/*
 * SIMD NEON Alpha Blend (ARM64)
 *
 * Thread safety: NEON intrinsics operate on local registers only.
 * No shared state, no global variables, fully thread-safe.
 * Each call uses stack-local NEON vectors, no inter-thread interference.
 */

static inline void alpha_blend_neon_batch(DWORD *dst, DWORD *src, BYTE alpha, int width)
{
    int x = 0;
    uint8x8_t alpha_u8 = vdup_n_u8(alpha);
    uint32x4_t alpha_shift = vdupq_n_u32(alpha << 24);

    for (; x + 4 <= width; x += 4)
    {
        uint32x4_t s = vld1q_u32(src + x);
        uint8x8_t b = vreinterpret_u8_u32(vmovn_u32(s & vdupq_n_u32(0xFF)));
        uint8x8_t g = vreinterpret_u8_u32(vshrn_n_u32(s, 8));
        uint8x8_t r = vreinterpret_u8_u32(vshrn_n_u32(s, 16));

        uint8x8_t b_blend = vshrn_n_u16(vmull_u8(b, alpha_u8), 8);
        uint8x8_t g_blend = vshrn_n_u16(vmull_u8(g, alpha_u8), 8);
        uint8x8_t r_blend = vshrn_n_u16(vmull_u8(r, alpha_u8), 8);

        vst1q_u32(dst + x, alpha_shift |
                  vshll_n_u16(vreinterpret_u16_u8(r_blend), 16) |
                  vshll_n_u16(vreinterpret_u16_u8(g_blend), 8) |
                  vshll_n_u16(vreinterpret_u16_u8(b_blend), 0));
    }

    for (; x < width; x++)
    {
        DWORD p = src[x];
        DWORD b = p & 0xff;
        DWORD g = (p >> 8) & 0xff;
        DWORD r = (p >> 16) & 0xff;
        dst[x] = ((alpha << 24) |
                  ((r * alpha) >> 8) << 16 |
                  ((g * alpha) >> 8) << 8 |
                  ((b * alpha) >> 8));
    }
}

#endif /* __ARM_NEON */

#endif /* __WINE_OHOS_SURFACE_LUT_H */