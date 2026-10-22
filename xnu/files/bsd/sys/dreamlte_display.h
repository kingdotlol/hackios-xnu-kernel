/*
 * Samsung Exynos 8895 Display Controller (DECON) and MIPI-DSI driver header.
 */

#ifndef _SYS_DREAMLTE_DISPLAY_H_
#define _SYS_DREAMLTE_DISPLAY_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

/* Initialize display controller */
int dreamlte_display_init(uint64_t fb_base, uint32_t width, uint32_t height, uint32_t stride);

/* Enable/disable display output */
void dreamlte_display_enable(bool enable);

/* Set brightness level (0-255) */
void dreamlte_display_set_brightness(uint32_t level);

/* Update framebuffer address */
void dreamlte_display_set_framebuffer(uint64_t fb_base);

/* Get display info */
void dreamlte_display_get_info(uint64_t *fb_base, uint32_t *width, uint32_t *height, uint32_t *stride);

/* Check if display is initialized */
bool dreamlte_display_is_initialized(void);

__END_DECLS

#endif /* _SYS_DREAMLTE_DISPLAY_H_ */