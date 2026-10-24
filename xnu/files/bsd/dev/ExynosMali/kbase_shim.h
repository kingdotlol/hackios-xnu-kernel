#ifndef _EXYNOS_MALI_KBASE_SHIM_H_
#define _EXYNOS_MALI_KBASE_SHIM_H_
/*
 * Deliberately narrow compatibility contract; this is NOT ARM kbase and does
 * not emulate kbase's proprietary ioctl, memory, scheduler, or firmware ABI.
 * Callers must supply device-specific pinned DMA memory and validated MMU ops.
 */
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/dreamlte_gpu.h>

#define EXYNOS_MALI_KBASE_SHIM_ABI 1
struct exynos_mali_kbase_ops {
    int (*probe)(uint32_t *gpu_id);
    int (*power)(int enable);
    int (*reset)(void);
};
int exynos_mali_probe(uint32_t *gpu_id);
#endif
