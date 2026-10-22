/*
 * Samsung Exynos 8895 Framebuffer driver header.
 */

#ifndef _SYS_DREAMLTE_FB_H_
#define _SYS_DREAMLTE_FB_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

/* Initialize framebuffer from UEFI GOP info */
void dreamlte_fb_init(uint64_t fb_base, uint32_t width, uint32_t height, uint32_t stride, uint32_t depth);

/* Get framebuffer information */
void dreamlte_fb_get_info(uint64_t *base, uint32_t *width, uint32_t *height, uint32_t *stride, uint32_t *depth, uint32_t *size);

/* Fill framebuffer with color */
void dreamlte_fb_fill(uint32_t color);

/* Draw a pixel */
void dreamlte_fb_draw_pixel(uint32_t x, uint32_t y, uint32_t color);

/* Test pattern */
void dreamlte_fb_test_pattern(void);

/* Flush cache for framebuffer */
void dreamlte_fb_flush(void);

/* Check if framebuffer is initialized */
bool dreamlte_fb_is_initialized(void);

__END_DECLS

#endif /* _SYS_DREAMLTE_FB_H_ */