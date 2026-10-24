/*
 * Exynos 8895 Mali-G71 integration boundary.
 *
 * XNU does not ship macOS IOKit classes/KEXT loader in this tree. This file is
 * intentionally not registered in bsd/conf/files: it is the C++ adapter seam
 * for a future XNU-native driver, not a runnable IOKit kext. Registering it as
 * an IOKit driver would falsely imply framework and DMA/MMU support.
 */
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/dreamlte_gpu.h>

extern "C" int exynos_mali_probe(uint32_t *gpu_id)
{
    if (gpu_id == 0) return EINVAL;
    *gpu_id = 0;
    /* Register mapping and probe remain owned by dreamlte_gpu.c. */
    return ENOTSUP;
}
