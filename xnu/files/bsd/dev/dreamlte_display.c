/*
 * Samsung Exynos 8895 Display Controller (DECON) and MIPI-DSI driver.
 * The Galaxy S8 uses a dual-display setup: internal AMOLED (MIPI-DSI)
 * and optional external display (HDMI/DP).
 * This driver provides control over the display controller beyond the
 * basic UEFI GOP framebuffer.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <machine/bus.h>
#include <arm/cpufunc.h>
#include <arm/pmap.h>

/* Exynos DECON (Display Controller) register offsets */
#define EXYNOS_DECON_BASE       0x1C000000
#define EXYNOS_DECON_SIZE       0x00400000

#define EXYNOS_DECON_VIDCON0    0x0000  /* Video control 0 */
#define EXYNOS_DECON_VIDCON1    0x0004  /* Video control 1 */
#define EXYNOS_DECON_VIDTCON0   0x0008  /* Video timing control 0 */
#define EXYNOS_DECON_VIDTCON1   0x000C  /* Video timing control 1 */
#define EXYNOS_DECON_VIDTCON2   0x0010  /* Video timing control 2 */
#define EXYNOS_DECON_WINCON0    0x0020  /* Window 0 control */
#define EXYNOS_DECON_WINCON1    0x0024  /* Window 1 control */
#define EXYNOS_DECON_WINCON2    0x0028  /* Window 2 control */
#define EXYNOS_DECON_WINCON3    0x002C  /* Window 3 control */
#define EXYNOS_DECON_WINCON4    0x0030  /* Window 4 control */
#define EXYNOS_DECON_WINMAP     0x0040  /* Window map */
#define EXYNOS_DECON_VIDOSD0A   0x0050  /* Window 0 position A */
#define EXYNOS_DECON_VIDOSD0B   0x0054  /* Window 0 position B */
#define EXYNOS_DECON_VIDOSD0C   0x0058  /* Window 0 size */
#define EXYNOS_DECON_VIDOSD1A   0x005C  /* Window 1 position A */
#define EXYNOS_DECON_VIDOSD1B   0x0060  /* Window 1 position B */
#define EXYNOS_DECON_VIDOSD1C   0x0064  /* Window 1 size */
#define EXYNOS_DECON_VIDOSD2A   0x0068  /* Window 2 position A */
#define EXYNOS_DECON_VIDOSD2B   0x006C  /* Window 2 position B */
#define EXYNOS_DECON_VIDOSD2C   0x0070  /* Window 2 size */
#define EXYNOS_DECON_VIDOSD3A   0x0074  /* Window 3 position A */
#define EXYNOS_DECON_VIDOSD3B   0x0078  /* Window 3 position B */
#define EXYNOS_DECON_VIDOSD3C   0x007C  /* Window 3 size */
#define EXYNOS_DECON_VIDOSD4A   0x0080  /* Window 4 position A */
#define EXYNOS_DECON_VIDOSD4B   0x0084  /* Window 4 position B */
#define EXYNOS_DECON_VIDOSD4C   0x0088  /* Window 4 size */
#define EXYNOS_DECON_VIDW00ADD0 0x00A0  /* Window 0 buffer 0 address */
#define EXYNOS_DECON_VIDW00ADD1 0x00A4  /* Window 0 buffer 1 address */
#define EXYNOS_DECON_VIDW00ADD2 0x00A8  /* Window 0 buffer 2 address */
#define EXYNOS_DECON_VIDW01ADD0 0x00B0  /* Window 1 buffer 0 address */
#define EXYNOS_DECON_VIDW01ADD1 0x00B4  /* Window 1 buffer 1 address */
#define EXYNOS_DECON_VIDW01ADD2 0x00B8  /* Window 1 buffer 2 address */
#define EXYNOS_DECON_VIDW02ADD0 0x00C0  /* Window 2 buffer 0 address */
#define EXYNOS_DECON_VIDW02ADD1 0x00C4  /* Window 2 buffer 1 address */
#define EXYNOS_DECON_VIDW02ADD2 0x00C8  /* Window 2 buffer 2 address */
#define EXYNOS_DECON_VIDW03ADD0 0x00D0  /* Window 3 buffer 0 address */
#define EXYNOS_DECON_VIDW03ADD1 0x00D4  /* Window 3 buffer 1 address */
#define EXYNOS_DECON_VIDW03ADD2 0x00D8  /* Window 3 buffer 2 address */
#define EXYNOS_DECON_VIDW04ADD0 0x00E0  /* Window 4 buffer 0 address */
#define EXYNOS_DECON_VIDW04ADD1 0x00E4  /* Window 4 buffer 1 address */
#define EXYNOS_DECON_VIDW04ADD2 0x00E8  /* Window 4 buffer 2 address */
#define EXYNOS_DECON_VIDINTCON0 0x0100  /* Interrupt control 0 */
#define EXYNOS_DECON_VIDINTCON1 0x0104  /* Interrupt control 1 */
#define EXYNOS_DECON_W0KEYCON0  0x0110  /* Window 0 key color control 0 */
#define EXYNOS_DECON_W0KEYCON1  0x0114  /* Window 0 key color control 1 */
#define EXYNOS_DECON_W1KEYCON0  0x0118  /* Window 1 key color control 0 */
#define EXYNOS_DECON_W1KEYCON1  0x011C  /* Window 1 key color control 1 */
#define EXYNOS_DECON_W2KEYCON0  0x0120  /* Window 2 key color control 0 */
#define EXYNOS_DECON_W2KEYCON1  0x0124  /* Window 2 key color control 1 */
#define EXYNOS_DECON_W3KEYCON0  0x0128  /* Window 3 key color control 0 */
#define EXYNOS_DECON_W3KEYCON1  0x012C  /* Window 3 key color control 1 */
#define EXYNOS_DECON_W4KEYCON0  0x0130  /* Window 4 key color control 0 */
#define EXYNOS_DECON_W4KEYCON1  0x0134  /* Window 4 key color control 1 */
#define EXYNOS_DECON_VIDTCON3   0x0138  /* Video timing control 3 */

/* VIDCON0 bits */
#define VIDCON0_ENVID         (1 << 0)   /* Video output enable */
#define VIDCON0_ENVID_F       (1 << 1)   /* Video output enable (FIFO) */
#define VIDCON0_PNRMODE_RGB   (0 << 2)   /* RGB parallel */
#define VIDCON0_PNRMODE_CPU   (1 << 2)   /* CPU interface */
#define VIDCON0_PNRMODE_RGB666 (2 << 2)  /* RGB666 */
#define VIDCON0_PNRMODE_RGB565 (3 << 2)  /* RGB565 */
#define VIDCON0_CLKVAL(n)     ((n) << 6) /* Clock divider */
#define VIDCON0_CLKDIR        (1 << 17)  /* Clock direction */
#define VIDCON0_CLKSEL_F      (1 << 18)  /* Clock source */
#define VIDCON0_ENCLK         (1 << 19)  /* Clock enable */

/* WINCON bits */
#define WINCON_ENWIN          (1 << 0)   /* Window enable */
#define WINCON_BPP_24BIT      (0 << 2)   /* 24 bpp */
#define WINCON_BPP_16BIT_565  (3 << 2)   /* 16 bpp 565 */
#define WINCON_BPP_15BIT      (4 << 2)   /* 15 bpp */
#define WINCON_BPP_12BIT      (5 << 2)   /* 12 bpp */
#define WINCON_BPP_8BIT       (6 << 2)   /* 8 bpp */
#define WINCON_BPP_1BIT       (7 << 2)   /* 1 bpp */
#define WINCON_FORMAT_RGB     (0 << 6)   /* RGB */
#define WINCON_FORMAT_BGR     (1 << 6)   /* BGR */
#define WINCON_BUFSEL         (1 << 8)   /* Buffer select */
#define WINCON_BUFAUTO        (1 << 9)   /* Buffer auto flip */
#define WINCON_HAWSWP         (1 << 13)  /* Half-word swap */
#define WINCON_BURSTLEN_4     (0 << 14)  /* Burst length 4 */
#define WINCON_BURSTLEN_8     (1 << 14)  /* Burst length 8 */
#define WINCON_BURSTLEN_16    (2 << 14)  /* Burst length 16 */

/* MIPI-DSI register offsets (within DECON space or separate) */
#define EXYNOS_DSI_BASE       0x1C100000
#define EXYNOS_DSI_SIZE       0x00100000

#define EXYNOS_DSI_DSIM_CON   0x0000  /* DSI control */
#define EXYNOS_DSI_DSIM_CLK   0x0004  /* DSI clock control */
#define EXYNOS_DSI_DSIM_TIMING 0x0008 /* DSI timing */
#define EXYNOS_DSI_DSIM_ESCMODE 0x000C /* Escape mode */
#define EXYNOS_DSI_DSIM_STATUS 0x0010 /* DSI status */
#define EXYNOS_DSI_DSIM_INTC  0x0014  /* DSI interrupt control */
#define EXYNOS_DSI_DSIM_INTS  0x0018  /* DSI interrupt status */
#define EXYNOS_DSI_DSIM_PKTHDR 0x0020 /* Packet header */
#define EXYNOS_DSI_DSIM_PAYLOAD 0x0024 /* Payload */
#define EXYNOS_DSI_DSIM_MULTI  0x0030 /* Multi packet */
#define EXYNOS_DSI_DSIM_PHYCON 0x0040 /* PHY control */
#define EXYNOS_DSI_DSIM_PLL    0x0050 /* PLL control */

/* Galaxy S8 display timing (1440x2960 @ 60Hz) */
#define S8_H_ACTIVE           1440
#define S8_H_FP               48
#define S8_H_SYNC             32
#define S8_H_BP               80

#define S8_V_ACTIVE           2960
#define S8_V_FP               16
#define S8_V_SYNC             8
#define S8_V_BP               24

#define S8_PIXEL_CLK          285000000  /* ~285 MHz */

static volatile uint32_t *dreamlte_decon_base = NULL;
static volatile uint32_t *dreamlte_dsi_base = NULL;
static bool dreamlte_display_initialized = false;
static uint32_t dreamlte_display_width = S8_H_ACTIVE;
static uint32_t dreamlte_display_height = S8_V_ACTIVE;
static uint32_t dreamlte_display_stride = 0;
static uint64_t dreamlte_display_fb_base = 0;
static struct mtx dreamlte_display_lock;

/*
 * Initialize the display controller
 */
int
dreamlte_display_init(uint64_t fb_base, uint32_t width, uint32_t height, uint32_t stride)
{
    if (dreamlte_display_initialized) return 0;

    mtx_init(&dreamlte_display_lock, "dreamlte_display", NULL, MTX_DEF);

    dreamlte_display_fb_base = fb_base;
    dreamlte_display_width = width;
    dreamlte_display_height = height;
    dreamlte_display_stride = stride;

    /* Map DECON and DSI registers */
    dreamlte_decon_base = (volatile uint32_t *)(uintptr_t)EXYNOS_DECON_BASE;
    dreamlte_dsi_base = (volatile uint32_t *)(uintptr_t)EXYNOS_DSI_BASE;

    printf("dreamlte_display: DECON=0x%p, DSI=0x%p, fb=0x%llx, %ux%u stride=%u\n",
           dreamlte_decon_base, dreamlte_dsi_base, fb_base, width, height, stride);

    /* Configure video timing */
    dreamlte_display_config_timing();

    /* Configure window 0 for primary framebuffer */
    dreamlte_display_config_window(0, fb_base, width, height, stride);

    /* Configure MIPI-DSI */
    dreamlte_dsi_init();

    /* Enable display */
    dreamlte_display_enable(true);

    dreamlte_display_initialized = true;
    printf("dreamlte_display: initialized\n");

    return 0;
}

/*
 * Configure video timing for Galaxy S8 panel
 */
static void
dreamlte_display_config_timing(void)
{
    mtx_lock(&dreamlte_display_lock);

    /* VIDTCON0: Vertical timing */
    uint32_t vtcon0 = ((S8_V_SYNC - 1) << 20) |
                      ((S8_V_BP - 1) << 10) |
                      (S8_V_FP - 1);
    dreamlte_decon_base[EXYNOS_DECON_VIDTCON0 / 4] = vtcon0;

    /* VIDTCON1: Horizontal timing */
    uint32_t vtcon1 = ((S8_H_SYNC - 1) << 20) |
                      ((S8_H_BP - 1) << 10) |
                      (S8_H_FP - 1);
    dreamlte_decon_base[EXYNOS_DECON_VIDTCON1 / 4] = vtcon1;

    /* VIDTCON2: Active resolution */
    uint32_t vtcon2 = ((S8_V_ACTIVE - 1) << 11) |
                      (S8_H_ACTIVE - 1);
    dreamlte_decon_base[EXYNOS_DECON_VIDTCON2 / 4] = vtcon2;

    /* VIDCON0: Clock and format */
    /* CLKVAL = (PCLK / PixelClock) - 1, assuming 200MHz PCLK */
    uint32_t clkval = (200000000 / (S8_PIXEL_CLK / 1000000)) - 1;
    uint32_t vidcon0 = VIDCON0_CLKVAL(clkval) | VIDCON0_CLKDIR | VIDCON0_CLKSEL_F | VIDCON0_ENCLK;
    vidcon0 |= VIDCON0_PNRMODE_RGB; /* RGB parallel */
    dreamlte_decon_base[EXYNOS_DECON_VIDCON0 / 4] = vidcon0;

    mtx_unlock(&dreamlte_display_lock);
}

/*
 * Configure a display window
 */
static void
dreamlte_display_config_window(int win, uint64_t fb_base, uint32_t width, uint32_t height, uint32_t stride)
{
    mtx_lock(&dreamlte_display_lock);

    /* Window position (full screen) */
    uint32_t osda = 0;
    uint32_t osdb = ((height - 1) << 11) | (width - 1);
    uint32_t osdc = (stride / 4); /* Words per line */

    switch (win) {
        case 0:
            dreamlte_decon_base[EXYNOS_DECON_VIDOSD0A / 4] = osda;
            dreamlte_decon_base[EXYNOS_DECON_VIDOSD0B / 4] = osdb;
            dreamlte_decon_base[EXYNOS_DECON_VIDOSD0C / 4] = osdc;
            dreamlte_decon_base[EXYNOS_DECON_VIDW00ADD0 / 4] = (uint32_t)fb_base;
            dreamlte_decon_base[EXYNOS_DECON_VIDW00ADD1 / 4] = (uint32_t)fb_base;
            dreamlte_decon_base[EXYNOS_DECON_VIDW00ADD2 / 4] = (uint32_t)fb_base;
            dreamlte_decon_base[EXYNOS_DECON_WINCON0 / 4] = WINCON_ENWIN | WINCON_BPP_24BIT | WINCON_FORMAT_RGB | WINCON_BUFAUTO | WINCON_BURSTLEN_16;
            break;
        case 1:
            dreamlte_decon_base[EXYNOS_DECON_VIDOSD1A / 4] = osda;
            dreamlte_decon_base[EXYNOS_DECON_VIDOSD1B / 4] = osdb;
            dreamlte_decon_base[EXYNOS_DECON_VIDOSD1C / 4] = osdc;
            dreamlte_decon_base[EXYNOS_DECON_VIDW01ADD0 / 4] = (uint32_t)fb_base;
            dreamlte_decon_base[EXYNOS_DECON_WINCON1 / 4] = WINCON_ENWIN | WINCON_BPP_24BIT | WINCON_FORMAT_RGB | WINCON_BUFAUTO | WINCON_BURSTLEN_16;
            break;
        default:
            break;
    }

    mtx_unlock(&dreamlte_display_lock);
}

/*
 * Initialize MIPI-DSI for Galaxy S8 panel
 */
static void
dreamlte_dsi_init(void)
{
    mtx_lock(&dreamlte_display_lock);

    /* Reset DSI */
    dreamlte_dsi_base[EXYNOS_DSI_DSIM_CON / 4] = 0;

    /* Configure DSI PLL for ~285 MHz pixel clock */
    /* This is panel-specific and requires the panel's command set */
    /* For Galaxy S8, the panel is likely Samsung S6E3FA3 or similar */

    /* Basic DSI configuration */
    uint32_t dsi_con = (1 << 0) | /* Enable DSI */
                       (3 << 1) | /* 4 lanes */
                       (1 << 4) | /* Video mode */
                       (0 << 8);  /* Non-burst mode */
    dreamlte_dsi_base[EXYNOS_DSI_DSIM_CON / 4] = dsi_con;

    /* PHY configuration */
    dreamlte_dsi_base[EXYNOS_DSI_DSIM_PHYCON / 4] = (1 << 0) | (1 << 1); /* Enable PHY */

    /* PLL configuration for target frequency */
    /* Simplified - actual PLL config requires panel-specific values */
    dreamlte_dsi_base[EXYNOS_DSI_DSIM_PLL / 4] = 0x000A0000; /* Example */

    /* Send panel initialization commands */
    dreamlte_dsi_send_cmd(0x11, NULL, 0); /* Sleep out */
    for (volatile int i = 0; i < 120000; i++) {} /* Wait 120ms */
    dreamlte_dsi_send_cmd(0x29, NULL, 0); /* Display on */

    mtx_unlock(&dreamlte_display_lock);
}

/*
 * Send a DSI command to the panel
 */
static void
dreamlte_dsi_send_cmd(uint8_t cmd, const uint8_t *params, int param_count)
{
    /* Build packet header */
    uint32_t header = (cmd << 8) | 0x00; /* Long packet, 1 param */
    if (param_count > 0) {
        header |= (param_count + 1) << 16; /* Param count + 1 for command byte */
    }
    dreamlte_dsi_base[EXYNOS_DSI_DSIM_PKTHDR / 4] = header;

    /* Build payload */
    uint32_t payload = cmd;
    if (param_count > 0) {
        payload |= (uint32_t)params[0] << 8;
        if (param_count > 1) payload |= (uint32_t)params[1] << 16;
        if (param_count > 2) payload |= (uint32_t)params[2] << 24;
    }
    dreamlte_dsi_base[EXYNOS_DSI_DSIM_PAYLOAD / 4] = payload;

    /* Trigger transfer */
    dreamlte_dsi_base[EXYNOS_DSI_DSIM_MULTI / 4] = 1;

    /* Wait for completion */
    for (int i = 0; i < 10000; i++) {
        if (dreamlte_dsi_base[EXYNOS_DSI_DSIM_STATUS / 4] & 1) break;
    }
}

/*
 * Enable/disable display output
 */
void
dreamlte_display_enable(bool enable)
{
    if (!dreamlte_display_initialized) return;

    mtx_lock(&dreamlte_display_lock);

    uint32_t vidcon0 = dreamlte_decon_base[EXYNOS_DECON_VIDCON0 / 4];
    if (enable) {
        vidcon0 |= VIDCON0_ENVID | VIDCON0_ENVID_F;
    } else {
        vidcon0 &= ~(VIDCON0_ENVID | VIDCON0_ENVID_F);
    }
    dreamlte_decon_base[EXYNOS_DECON_VIDCON0 / 4] = vidcon0;

    mtx_unlock(&dreamlte_display_lock);
}

/*
 * Set brightness (via DSI command or PWM)
 */
void
dreamlte_display_set_brightness(uint32_t level)
{
    if (!dreamlte_display_initialized) return;

    /* Clamp level */
    if (level > 255) level = 255;

    /* Send brightness command via DSI (CABC or direct) */
    uint8_t params[1] = { (uint8_t)level };
    dreamlte_dsi_send_cmd(0x51, params, 1); /* Write display brightness */
}

/*
 * Update framebuffer address (for double/triple buffering)
 */
void
dreamlte_display_set_framebuffer(uint64_t fb_base)
{
    if (!dreamlte_display_initialized) return;

    mtx_lock(&dreamlte_display_lock);
    dreamlte_display_fb_base = fb_base;
    dreamlte_decon_base[EXYNOS_DECON_VIDW00ADD0 / 4] = (uint32_t)fb_base;
    dreamlte_decon_base[EXYNOS_DECON_VIDW00ADD1 / 4] = (uint32_t)fb_base;
    mtx_unlock(&dreamlte_display_lock);
}

/*
 * Get display info
 */
void
dreamlte_display_get_info(uint64_t *fb_base, uint32_t *width, uint32_t *height, uint32_t *stride)
{
    if (fb_base) *fb_base = dreamlte_display_fb_base;
    if (width) *width = dreamlte_display_width;
    if (height) *height = dreamlte_display_height;
    if (stride) *stride = dreamlte_display_stride;
}

/*
 * Check if display is initialized
 */
bool
dreamlte_display_is_initialized(void)
{
    return dreamlte_display_initialized;
}