/*
 * Samsung Exynos 8895 UFS driver header.
 */

#ifndef _SYS_DREAMLTE_UFS_H_
#define _SYS_DREAMLTE_UFS_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

/* Initialize UFS host controller */
int dreamlte_ufs_init(void);

/* Read blocks from UFS (LUN 0) */
int dreamlte_ufs_read_blocks(uint64_t lba, uint32_t count, void *buffer);

/* Check if UFS is initialized */
bool dreamlte_ufs_is_initialized(void);

__END_DECLS

#endif /* _SYS_DREAMLTE_UFS_H_ */