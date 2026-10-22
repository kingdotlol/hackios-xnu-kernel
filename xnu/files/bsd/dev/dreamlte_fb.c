/*
 * Samsung Exynos 8895 Framebuffer driver.
 * Provides access to the display controller for early boot graphics.
 *
 * The Exynos display controller (decon/MIPI-DSI) is complex.
 * For early boot, we use the framebuffer set up by UEFI/GOP.
 * The loader passes the framebuffer info via boot_args.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/mman.h>
#include <sys/malloc.h>
#include <machine/bus.h>
#include <arm/cpufunc.h>
#include <arm/pmap.h>
#include <pexpert/pexpert.h>

/* Exynos Display Controller (DECON) register offsets */
#define EXYNOS_DECON_BASE       0x1C000000
#define EXYNOS_DECON_SIZE       0x00400000

#define EXYNOS_DECON_VIDCON0    0x0000  /* Video control 0 */
#define EXYNOS_DECON_VIDCON1    0x0004  /* Video control 1 */
#define EXYNOS_DECON_VIDTCON0   0x0008  /* Video timing control 0 */
#define EXYNOS_DECON_VIDTCON1   0x000C  /* Video timing control 1 */
#define EXYNOS_DECON_VIDTCON2   0x0010  /* Video timing control 2 */
#define EXYNOS_DECON_WINCON0    0x0020  /* Window 0 control */
#define EXYNOS_DECON_WINMAP     0x0040  /* Window map */
#define EXYNOS_DECON_WIN0MAP    0x0044  /* Window 0 map */
#define EXYNOS_DECON_VIDOSD0A   0x0050  /* Window 0 position */
#define EXYNOS_DECON_VIDOSD0B   0x0054  /* Window 0 size */
#define EXYNOS_DECON_VIDOSD0C   0x0058  /* Window 0 control */
#define EXYNOS_DECON_VIDW00ADD0B0 0x00A0  /* Window 0 buffer address */
#define EXYNOS_DECON_VIDW00ADD1B0 0x00D0  /* Window 0 buffer address 1 */
#define EXYNOS_DECON_VIDW00ADD2   0x00E0  /* Window 0 buffer address 2 */

/* VIDCON0 bits */
#define EXYNOS_DECON_VIDCON0_ENVID      (1 << 0)   /* Video output enable */
#define EXYNOS_DECON_VIDCON0_ENVID_F    (1 << 1)   /* Video output enable (FIFO) */
#define EXYNOS_DECON_VIDCON0_PNRMODE_RGB (0 << 2)  /* RGB parallel */
#define EXYNOS_DECON_VIDCON0_PNRMODE_CPU (1 << 2)  /* CPU interface */
#define EXYNOS_DECON_VIDCON0_PNRMODE_RGB666 (2 << 2) /* RGB666 */
#define EXYNOS_DECON_VIDCON0_PNRMODE_RGB565 (3 << 2) /* RGB565 */
#define EXYNOS_DECON_VIDCON0_CLKVAL(n)  ((n) << 6) /* Clock divider */
#define EXYNOS_DECON_VIDCON0_CLKDIR     (1 << 17)  /* Clock direction */
#define EXYNOS_DECON_VIDCON0_CLKSEL_F   (1 << 18)  /* Clock source */

/* Window control bits */
#define EXYNOS_DECON_WINCON0_ENWIN      (1 << 0)   /* Window enable */
#define EXYNOS_DECON_WINCON0_BPP_24BIT  (0 << 2)   /* 24 bpp */
#define EXYNOS_DECON_WINCON0_BPP_16BIT  (1 << 2)   /* 16 bpp */
#define EXYNOS_DECON_WINCON0_BPP_18BIT  (2 << 2)   /* 18 bpp */
#define EXYNOS_DECON_WINCON0_BPP_16BIT_565 (3 << 2) /* 16 bpp 565 */
#define EXYNOS_DECON_WINCON0_BPP_15BIT  (4 << 2)   /* 15 bpp */
#define EXYNOS_DECON_WINCON0_BPP_12BIT  (5 << 2)   /* 12 bpp */
#define EXYNOS_DECON_WINCON0_BPP_8BIT   (6 << 2)   /* 8 bpp */
#define EXYNOS_DECON_WINCON0_BPP_1BIT   (7 << 2)   /* 1 bpp */
#define EXYNOS_DECON_WINCON0_FORMAT_RGB (0 << 6)   /* RGB */
#define EXYNOS_DECON_WINCON0_FORMAT_BGR (1 << 6)   /* BGR */
#define EXYNOS_DECON_WINCON0_BUFSEL     (1 << 8)   /* Buffer select */
#define EXYNOS_DECON_WINCON0_BUFAUTO    (1 << 9)   /* Buffer auto flip */
#define EXYNOS_DECON_WINCON0_HAWSWP     (1 << 13)  /* Half-word swap */
#define EXYNOS_DECON_WINCON0_BURSTLEN_4 (0 << 14)  /* Burst length 4 */
#define EXYNOS_DECON_WINCON0_BURSTLEN_8 (1 << 14)  /* Burst length 8 */
#define EXYNOS_DECON_WINCON0_BURSTLEN_16 (2 << 14) /* Burst length 16 */

static volatile uint32_t *dreamlte_decon_base = NULL;
static uint64_t dreamlte_fb_base = 0;
static uint32_t dreamlte_fb_width = 0;
static uint32_t dreamlte_fb_height = 0;
static uint32_t dreamlte_fb_stride = 0;
static uint32_t dreamlte_fb_depth = 0;
static uint32_t dreamlte_fb_size = 0;
static bool dreamlte_fb_initialized = false;

/*
 * Initialize the framebuffer using the GOP info from UEFI.
 * The loader passes framebuffer info in boot_args.
 */
void
dreamlte_fb_init(uint64_t fb_base, uint32_t width, uint32_t height, uint32_t stride, uint32_t depth)
{
    if (dreamlte_fb_initialized) {
        return;
    }

    dreamlte_fb_base = fb_base;
    dreamlte_fb_width = width;
    dreamlte_fb_height = height;
    dreamlte_fb_stride = stride;
    dreamlte_fb_depth = depth;
    dreamlte_fb_size = height * stride;

    printf("dreamlte_fb: framebuffer at 0x%llx, %ux%u, stride %u, depth %u\n",
           fb_base, width, height, stride, depth);

    /* Map DECON registers if needed for advanced control */
    /* For now, we just use the framebuffer directly */

    dreamlte_fb_initialized = true;
}

/*
 * Get framebuffer information.
 */
void
dreamlte_fb_get_info(uint64_t *base, uint32_t *width, uint32_t *height, uint32_t *stride, uint32_t *depth, uint32_t *size)
{
    if (base) *base = dreamlte_fb_base;
    if (width) *width = dreamlte_fb_width;
    if (height) *height = dreamlte_fb_height;
    if (stride) *stride = dreamlte_fb_stride;
    if (depth) *depth = dreamlte_fb_depth;
    if (size) *size = dreamlte_fb_size;
}

/*
 * Fill the framebuffer with a color (for testing).
 */
void
dreamlte_fb_fill(uint32_t color)
{
    if (!dreamlte_fb_initialized || dreamlte_fb_base == 0) {
        return;
    }

    volatile uint32_t *fb = (volatile uint32_t *)(uintptr_t)dreamlte_fb_base;
    uint32_t pixels = dreamlte_fb_size / 4;

    for (uint32_t i = 0; i < pixels; i++) {
        fb[i] = color;
    }
}

/*
 * Draw a pixel at (x, y).
 */
void
dreamlte_fb_draw_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (!dreamlte_fb_initialized || dreamlte_fb_base == 0) {
        return;
    }

    if (x >= dreamlte_fb_width || y >= dreamlte_fb_height) {
        return;
    }

    volatile uint32_t *fb = (volatile uint32_t *)(uintptr_t)dreamlte_fb_base;
    fb[y * (dreamlte_fb_stride / 4) + x] = color;
}

/*
 * Simple test pattern.
 */
void
dreamlte_fb_test_pattern(void)
{
    if (!dreamlte_fb_initialized) {
        return;
    }

    uint32_t w = dreamlte_fb_width;
    uint32_t h = dreamlte_fb_height;

    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            uint32_t r = (x * 255) / w;
            uint32_t g = (y * 255) / h;
            uint32_t b = ((x + y) * 255) / (w + h);
            uint32_t color = (r << 16) | (g << 8) | b;
            dreamlte_fb_draw_pixel(x, y, color);
        }
    }
}

/*
 * Flush CPU cache for framebuffer region.
 */
void
dreamlte_fb_flush(void)
{
    if (!dreamlte_fb_initialized || dreamlte_fb_base == 0) {
        return;
    }

    /* Clean data cache for framebuffer region */
    uint64_t start = dreamlte_fb_base;
    uint64_t end = dreamlte_fb_base + dreamlte_fb_size;

    /* Use ARM cache maintenance operations */
    __builtin___clear_cache((void *)(uintptr_t)start, (void *)(uintptr_t)end);
}

/*
 * Check if framebuffer is initialized.
 */
bool
dreamlte_fb_is_initialized(void)
{
    return dreamlte_fb_initialized;
}