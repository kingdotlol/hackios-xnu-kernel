/*
 * Samsung Exynos 8895 Mali-G71 GPU driver header.
 * Provides kernel interfaces for GPU initialization and management.
 *
 * Register definitions from ARM Mali Midgard kernel driver
 * (mali_bifrost_gpu_kernel_drivers/driver/product/kernel/drivers/gpu/arm/midgard/mali_midg_regmap.h)
 * and mali_midg_regmap_jm.h.
 */

#ifndef _SYS_DREAMLTE_GPU_H_
#define _SYS_DREAMLTE_GPU_H_

#include <sys/cdefs.h>
#include <stdint.h>
#include <stdbool.h>

__BEGIN_DECLS

/* Exynos 8895 Mali-G71 GPU register base (from device tree / UEFI PCD) */
#define EXYNOS_MALI_BASE        0x18000000
#define EXYNOS_MALI_SIZE        0x01000000  /* 16MB */

/* Mali Midgard register block base offsets (relative to GPU base) */
#define MALI_GPU_CONTROL_BASE   0x0000
#define MALI_JOB_CONTROL_BASE   0x1000
#define MALI_MEMORY_MANAGEMENT_BASE 0x2000

/* --- GPU control registers (offset from MALI_GPU_CONTROL_BASE) --- */
#define MALI_REG_GPU_ID                  0x000   /* (RO) GPU and revision identifier */
#define MALI_REG_L2_FEATURES             0x004   /* (RO) Level 2 cache features */
#define MALI_REG_CORE_FEATURES           0x008   /* (RO) Shader Core Features */
#define MALI_REG_TILER_FEATURES          0x00C   /* (RO) Tiler Features */
#define MALI_REG_MEM_FEATURES            0x010   /* (RO) Memory system features */
#define MALI_REG_MMU_FEATURES            0x014   /* (RO) MMU features */
#define MALI_REG_AS_PRESENT              0x018   /* (RO) Address space slots present */
#define MALI_REG_JS_PRESENT              0x01C   /* (RO) Job slots present */
#define MALI_REG_GPU_IRQ_RAWSTAT         0x020   /* (RW) Raw interrupt status */
#define MALI_REG_GPU_IRQ_CLEAR           0x024   /* (WO) Interrupt clear */
#define MALI_REG_GPU_IRQ_MASK            0x028   /* (RW) Interrupt mask */
#define MALI_REG_GPU_IRQ_STATUS          0x02C   /* (RO) Interrupt status */
#define MALI_REG_GPU_COMMAND             0x030   /* (WO) GPU command */
#define MALI_REG_GPU_STATUS              0x034   /* (RO) GPU status */
#define MALI_REG_LATEST_FLUSH            0x038   /* (RO) Flush ID of latest clean-and-invalidate */
#define MALI_REG_GPU_FAULTSTATUS         0x03C   /* (RO) GPU exception type and fault status */
#define MALI_REG_GPU_FAULTADDRESS_LO     0x040   /* (RO) Fault address low */
#define MALI_REG_GPU_FAULTADDRESS_HI     0x044   /* (RO) Fault address high */
#define MALI_REG_PWR_KEY                 0x050   /* (WO) Power manager key register */
#define MALI_REG_PWR_OVERRIDE0           0x054   /* (RW) Power manager override */
#define MALI_REG_PWR_OVERRIDE1           0x058   /* (RW) Power manager override */
#define MALI_REG_PRFCNT_BASE_LO          0x060   /* (RW) Perf counter memory base low */
#define MALI_REG_PRFCNT_BASE_HI          0x064   /* (RW) Perf counter memory base high */
#define MALI_REG_PRFCNT_CONFIG           0x068   /* (RW) Perf counter config */
#define MALI_REG_PRFCNT_JM_EN            0x06C   /* (RW) Perf counter enable for Job Manager */
#define MALI_REG_PRFCNT_SHADER_EN        0x070   /* (RW) Perf counter enable for shaders */
#define MALI_REG_PRFCNT_TILER_EN         0x074   /* (RW) Perf counter enable for tiler */
#define MALI_REG_PRFCNT_MMU_L2_EN        0x07C   /* (RW) Perf counter enable for MMU/L2 */
#define MALI_REG_CYCLE_COUNT_LO          0x090   /* (RO) Cycle counter low */
#define MALI_REG_CYCLE_COUNT_HI          0x094   /* (RO) Cycle counter high */
#define MALI_REG_THREAD_MAX_THREADS      0x0A0   /* (RO) Maximum threads per core */
#define MALI_REG_THREAD_MAX_WORKGROUP_SIZE 0x0A4 /* (RO) Maximum workgroup size */
#define MALI_REG_THREAD_MAX_BARRIER_SIZE 0x0A8   /* (RO) Max threads at barrier */
#define MALI_REG_TIMESTAMP_LO            0x098   /* (RO) Timestamp counter low */
#define MALI_REG_TIMESTAMP_HI            0x09C   /* (RO) Timestamp counter high */
#define MALI_REG_SHADER_PRESENT_LO       0x100   /* (RO) Shader core present bitmap low */
#define MALI_REG_SHADER_PRESENT_HI       0x104   /* (RO) Shader core present bitmap high */
#define MALI_REG_TILER_PRESENT_LO        0x110   /* (RO) Tiler core present bitmap low */
#define MALI_REG_TILER_PRESENT_HI        0x114   /* (RO) Tiler core present bitmap high */
#define MALI_REG_L2_PRESENT_LO           0x120   /* (RO) L2 cache present bitmap low */
#define MALI_REG_L2_PRESENT_HI           0x124   /* (RO) L2 cache present bitmap high */
#define MALI_REG_SHADER_READY_LO         0x140   /* (RO) Shader core ready bitmap low */
#define MALI_REG_SHADER_READY_HI         0x144   /* (RO) Shader core ready bitmap high */
#define MALI_REG_TILER_READY_LO          0x150   /* (RO) Tiler core ready bitmap low */
#define MALI_REG_TILER_READY_HI          0x154   /* (RO) Tiler core ready bitmap high */
#define MALI_REG_L2_READY_LO             0x160   /* (RO) L2 ready bitmap low */
#define MALI_REG_L2_READY_HI             0x164   /* (RO) L2 ready bitmap high */
#define MALI_REG_SHADER_PWRON_LO         0x180   /* (WO) Shader core power on bitmap low */
#define MALI_REG_SHADER_PWRON_HI         0x184   /* (WO) Shader core power on bitmap high */
#define MALI_REG_TILER_PWRON_LO          0x190   /* (WO) Tiler core power on bitmap low */
#define MALI_REG_TILER_PWRON_HI          0x194   /* (WO) Tiler core power on bitmap high */
#define MALI_REG_L2_PWRON_LO             0x1A0   /* (WO) L2 cache power on bitmap low */
#define MALI_REG_L2_PWRON_HI             0x1A4   /* (WO) L2 cache power on bitmap high */
#define MALI_REG_SHADER_PWROFF_LO        0x1C0   /* (WO) Shader core power off bitmap low */
#define MALI_REG_SHADER_PWROFF_HI        0x1C4   /* (WO) Shader core power off bitmap high */
#define MALI_REG_TILER_PWROFF_LO         0x1D0   /* (WO) Tiler core power off bitmap low */
#define MALI_REG_TILER_PWROFF_HI         0x1D4   /* (WO) Tiler core power off bitmap high */
#define MALI_REG_L2_PWROFF_LO            0x1E0   /* (WO) L2 cache power off bitmap low */
#define MALI_REG_L2_PWROFF_HI            0x1E4   /* (WO) L2 cache power off bitmap high */
#define MALI_REG_SHADER_PWRTRANS_LO      0x200   /* (RO) Shader core power transition low */
#define MALI_REG_SHADER_PWRTRANS_HI      0x204   /* (RO) Shader core power transition high */
#define MALI_REG_TILER_PWRTRANS_LO       0x210   /* (RO) Tiler power transition low */
#define MALI_REG_TILER_PWRTRANS_HI       0x214   /* (RO) Tiler power transition high */
#define MALI_REG_L2_PWRTRANS_LO          0x220   /* (RO) L2 power transition low */
#define MALI_REG_L2_PWRTRANS_HI          0x224   /* (RO) L2 power transition high */
#define MALI_REG_STACK_PRESENT_LO        0xE00   /* (RO) Core stack present bitmap low */
#define MALI_REG_STACK_PRESENT_HI        0xE04   /* (RO) Core stack present bitmap high */
#define MALI_REG_STACK_READY_LO          0xE10   /* (RO) Core stack ready bitmap low */
#define MALI_REG_STACK_READY_HI          0xE14   /* (RO) Core stack ready bitmap high */
#define MALI_REG_STACK_PWRON_LO          0xE20   /* (WO) Core stack power on bitmap low */
#define MALI_REG_STACK_PWRON_HI          0xE24   /* (WO) Core stack power on bitmap high */
#define MALI_REG_STACK_PWROFF_LO         0xE30   /* (WO) Core stack power off bitmap low */
#define MALI_REG_STACK_PWROFF_HI         0xE34   /* (WO) Core stack power off bitmap high */
#define MALI_REG_STACK_PWRTRANS_LO       0xE40   /* (RO) Core stack power transition low */
#define MALI_REG_STACK_PWRTRANS_HI       0xE44   /* (RO) Core stack power transition high */
#define MALI_REG_SHADER_PWRACTIVE_LO     0x240   /* (RO) Shader core active bitmap low */
#define MALI_REG_SHADER_PWRACTIVE_HI     0x244   /* (RO) Shader core active bitmap high */
#define MALI_REG_TILER_PWRACTIVE_LO      0x250   /* (RO) Tiler active bitmap low */
#define MALI_REG_TILER_PWRACTIVE_HI      0x254   /* (RO) Tiler active bitmap high */
#define MALI_REG_L2_PWRACTIVE_LO         0x260   /* (RO) L2 active bitmap low */
#define MALI_REG_L2_PWRACTIVE_HI         0x264   /* (RO) L2 active bitmap high */
#define MALI_REG_COHERENCY_FEATURES      0x300   /* (RO) Coherency features */
#define MALI_REG_COHERENCY_ENABLE        0x304   /* (RW) Coherency enable */
#define MALI_REG_SHADER_CONFIG           0xF04   /* (RW) Shader core configuration */
#define MALI_REG_TILER_CONFIG            0xF08   /* (RW) Tiler core configuration */
#define MALI_REG_L2_MMU_CONFIG           0xF0C   /* (RW) L2 cache and MMU configuration */

/* --- Job control registers (offset from MALI_JOB_CONTROL_BASE) --- */
#define MALI_REG_JOB_IRQ_RAWSTAT         0x000   /* (RW) Raw interrupt status */
#define MALI_REG_JOB_IRQ_CLEAR           0x004   /* (WO) Interrupt clear */
#define MALI_REG_JOB_IRQ_MASK            0x008   /* (RW) Interrupt mask */
#define MALI_REG_JOB_IRQ_STATUS          0x00C   /* (RO) Interrupt status */
#define MALI_REG_JOB_IRQ_JS_STATE        0x010   /* JS state snapshot */
#define MALI_REG_JOB_IRQ_THROTTLE        0x014   /* Interrupt throttle cycles */

/* Job slot n base: MALI_JOB_CONTROL_BASE + JOB_SLOT0 + (n * 0x80) */
#define MALI_REG_JOB_SLOT0               0x800   /* Job slot 0 base */
#define MALI_REG_JOB_SLOT(n)             (MALI_REG_JOB_SLOT0 + ((n) << 7))
#define MALI_REG_JS_HEAD_LO              0x00    /* (RO) Job queue head low */
#define MALI_REG_JS_HEAD_HI              0x04    /* (RO) Job queue head high */
#define MALI_REG_JS_FLUSH_ID_NEXT        0x70    /* (RW) Next flush ID */
#define MALI_REG_JS_TAIL_LO              0x08    /* (RO) Job queue tail low */
#define MALI_REG_JS_TAIL_HI              0x0C    /* (RO) Job queue tail high */
#define MALI_REG_JS_AFFINITY_LO          0x10    /* (RO) Core affinity mask low */
#define MALI_REG_JS_AFFINITY_HI          0x14    /* (RO) Core affinity mask high */
#define MALI_REG_JS_CONFIG               0x18    /* (RO) Configuration settings */
#define MALI_REG_JS_XAFFINITY            0x1C    /* (RO) Extended affinity mask */
#define MALI_REG_JS_COMMAND              0x20    /* (WO) Command register */
#define MALI_REG_JS_STATUS               0x24    /* (RO) Status register */
#define MALI_REG_JS_HEAD_NEXT_LO         0x40    /* (RW) Next head pointer low */
#define MALI_REG_JS_HEAD_NEXT_HI         0x44    /* (RW) Next head pointer high */
#define MALI_REG_JS_AFFINITY_NEXT_LO     0x50    /* (RW) Next affinity mask low */
#define MALI_REG_JS_AFFINITY_NEXT_HI     0x54    /* (RW) Next affinity mask high */
#define MALI_REG_JS_CONFIG_NEXT          0x58    /* (RW) Next configuration */
#define MALI_REG_JS_XAFFINITY_NEXT       0x5C    /* (RW) Next extended affinity */
#define MALI_REG_JS_COMMAND_NEXT         0x60    /* (RW) Next command register */

/* JS_FEATURES_REG(n): MALI_GPU_CONTROL_BASE + JS0_FEATURES + (n * 4) */
#define MALI_REG_JS0_FEATURES            0x0C0   /* (RO) Features of job slot 0 */
#define MALI_REG_JS_FEATURES(n)          (MALI_REG_JS0_FEATURES + ((n) << 2))

/* --- MMU control registers (offset from MALI_MEMORY_MANAGEMENT_BASE) --- */
#define MALI_REG_MMU_IRQ_RAWSTAT         0x000   /* (RW) Raw interrupt status */
#define MALI_REG_MMU_IRQ_CLEAR           0x004   /* (WO) Interrupt clear */
#define MALI_REG_MMU_IRQ_MASK            0x008   /* (RW) Interrupt mask */
#define MALI_REG_MMU_IRQ_STATUS          0x00C   /* (RO) Interrupt status */

/* Address space n base: MALI_MEMORY_MANAGEMENT_BASE + MMU_AS0 + (n * 0x40) */
#define MALI_REG_MMU_AS0                 0x400   /* Address space 0 base */
#define MALI_REG_MMU_AS(n)               (MALI_REG_MMU_AS0 + ((n) << 6))
#define MALI_REG_AS_TRANSTAB_LO          0x00    /* (RW) Translation table base low */
#define MALI_REG_AS_TRANSTAB_HI          0x04    /* (RW) Translation table base high */
#define MALI_REG_AS_MEMATTR_LO           0x08    /* (RW) Memory attributes low */
#define MALI_REG_AS_MEMATTR_HI           0x0C    /* (RW) Memory attributes high */
#define MALI_REG_AS_LOCKADDR_LO          0x10    /* (RW) Lock region address low */
#define MALI_REG_AS_LOCKADDR_HI          0x14    /* (RW) Lock region address high */
#define MALI_REG_AS_COMMAND              0x18    /* (WO) MMU command */
#define MALI_REG_AS_FAULTSTATUS          0x1C    /* (RO) MMU fault status */
#define MALI_REG_AS_FAULTADDRESS_LO      0x20    /* (RO) Fault address low */
#define MALI_REG_AS_FAULTADDRESS_HI      0x24    /* (RO) Fault address high */
#define MALI_REG_AS_STATUS               0x28    /* (RO) Status flags */
#define MALI_REG_AS_TRANSCFG_LO          0x30    /* (RW) Translation config low */
#define MALI_REG_AS_TRANSCFG_HI          0x34    /* (RW) Translation config high */
#define MALI_REG_AS_FAULTEXTRA_LO        0x38    /* (RO) Secondary fault address low */
#define MALI_REG_AS_FAULTEXTRA_HI        0x3C    /* (RO) Secondary fault address high */

/* --- IRQ flag definitions --- */
#define MALI_IRQ_GPU_FAULT               (1 << 0)    /* A GPU Fault has occurred */
#define MALI_IRQ_MULTIPLE_GPU_FAULTS     (1 << 7)    /* More than one GPU Fault */
#define MALI_IRQ_RESET_COMPLETED         (1 << 8)    /* Reset completed */
#define MALI_IRQ_POWER_CHANGED_SINGLE    (1 << 9)    /* Single core power up/down */
#define MALI_IRQ_POWER_CHANGED_ALL       (1 << 10)   /* All cores power up/down */
#define MALI_IRQ_PRFCNT_SAMPLE_COMPLETED (1 << 16)   /* Perf counter sample complete */
#define MALI_IRQ_CLEAN_CACHES_COMPLETED  (1 << 17)   /* Cache clean complete */

#define MALI_IRQ_GPU_REG_ALL (MALI_IRQ_GPU_FAULT | MALI_IRQ_MULTIPLE_GPU_FAULTS | \
        MALI_IRQ_RESET_COMPLETED | MALI_IRQ_POWER_CHANGED_ALL | \
        MALI_IRQ_PRFCNT_SAMPLE_COMPLETED)

/* MMU page fault flags */
#define MALI_MMU_PAGE_FAULT(n)   (1UL << (n))
#define MALI_MMU_BUS_ERROR(n)    (1UL << ((n) + 16))

/* --- GPU_COMMAND values --- */
#define MALI_GPU_CMD_NOP                0x00
#define MALI_GPU_CMD_SOFT_RESET         0x01   /* Stop all bus interfaces, reset GPU */
#define MALI_GPU_CMD_HARD_RESET         0x02   /* Immediate full reset */
#define MALI_GPU_CMD_PRFCNT_CLEAR       0x03
#define MALI_GPU_CMD_PRFCNT_SAMPLE      0x04
#define MALI_GPU_CMD_CYCLE_COUNT_START  0x05
#define MALI_GPU_CMD_CYCLE_COUNT_STOP   0x06
#define MALI_GPU_CMD_CLEAN_CACHES       0x07   /* Clean all caches */
#define MALI_GPU_CMD_CLEAN_INV_CACHES   0x08   /* Clean and invalidate caches */
#define MALI_GPU_CMD_SET_PROTECTED_MODE 0x09

/* --- GPU_STATUS values --- */
#define MALI_GPU_STATUS_PRFCNT_ACTIVE        (1 << 2)
#define MALI_GPU_STATUS_PROTECTED_MODE_ACTIVE (1 << 7)

/* --- JS_COMMAND values --- */
#define MALI_JS_CMD_NOP         0x00
#define MALI_JS_CMD_START       0x01   /* Start processing a job chain */
#define MALI_JS_CMD_SOFT_STOP   0x02   /* Gently stop processing */
#define MALI_JS_CMD_HARD_STOP   0x03   /* Rudely stop processing */
#define MALI_JS_CMD_SOFT_STOP_0 0x04   /* SOFT_STOP if JOB_CHAIN_FLAG is 0 */
#define MALI_JS_CMD_HARD_STOP_0 0x05   /* HARD_STOP if JOB_CHAIN_FLAG is 0 */
#define MALI_JS_CMD_SOFT_STOP_1 0x06   /* SOFT_STOP if JOB_CHAIN_FLAG is 1 */
#define MALI_JS_CMD_HARD_STOP_1 0x07   /* HARD_STOP if JOB_CHAIN_FLAG is 1 */
#define MALI_JS_CMD_MASK        0x07

/* JS_CONFIG bits */
#define MALI_JS_CONFIG_START_FLUSH_NO_ACTION        (0u << 0)
#define MALI_JS_CONFIG_START_FLUSH_CLEAN            (1u << 8)
#define MALI_JS_CONFIG_START_FLUSH_CLEAN_INVALIDATE (3u << 8)
#define MALI_JS_CONFIG_START_MMU                    (1u << 10)
#define MALI_JS_CONFIG_JOB_CHAIN_FLAG               (1u << 11)
#define MALI_JS_CONFIG_END_FLUSH_CLEAN              (1u << 12)
#define MALI_JS_CONFIG_END_FLUSH_CLEAN_INVALIDATE   (3u << 12)
#define MALI_JS_CONFIG_ENABLE_FLUSH_REDUCTION       (1u << 14)
#define MALI_JS_CONFIG_DISABLE_DESCRIPTOR_WR_BK     (1u << 15)
#define MALI_JS_CONFIG_THREAD_PRI(n)                ((n) << 16)

/* JS_XAFFINITY bits */
#define MALI_JS_XAFFINITY_XAFFINITY_ENABLE (1u << 0)
#define MALI_JS_XAFFINITY_TILER_ENABLE     (1u << 8)
#define MALI_JS_XAFFINITY_CACHE_ENABLE     (1u << 16)

/* JS_STATUS values */
#define MALI_JS_STATUS_NO_EXCEPTION_BASE   0x00
#define MALI_JS_STATUS_INTERRUPTED         0x02
#define MALI_JS_STATUS_STOPPED             0x03
#define MALI_JS_STATUS_TERMINATED          0x04
#define MALI_JS_STATUS_FAULT_BASE          0x40
#define MALI_JS_STATUS_CONFIG_FAULT        0x40
#define MALI_JS_STATUS_POWER_FAULT         0x41
#define MALI_JS_STATUS_READ_FAULT          0x42
#define MALI_JS_STATUS_WRITE_FAULT         0x43
#define MALI_JS_STATUS_AFFINITY_FAULT      0x44
#define MALI_JS_STATUS_BUS_FAULT           0x48

/* JS_FEATURE bits */
#define MALI_JS_FEATURE_NULL_JOB           (1u << 1)
#define MALI_JS_FEATURE_SET_VALUE_JOB      (1u << 2)
#define MALI_JS_FEATURE_CACHE_FLUSH_JOB    (1u << 3)
#define MALI_JS_FEATURE_COMPUTE_JOB        (1u << 4)
#define MALI_JS_FEATURE_VERTEX_JOB         (1u << 5)
#define MALI_JS_FEATURE_GEOMETRY_JOB       (1u << 6)
#define MALI_JS_FEATURE_TILER_JOB          (1u << 7)
#define MALI_JS_FEATURE_FUSED_JOB          (1u << 8)
#define MALI_JS_FEATURE_FRAGMENT_JOB       (1u << 9)

/* --- MMU AS_COMMAND values --- */
#define MALI_AS_CMD_NOP         0x00
#define MALI_AS_CMD_UPDATE      0x01   /* Broadcast AS_TRANSTAB and AS_MEMATTR */
#define MALI_AS_CMD_LOCK        0x02
#define MALI_AS_CMD_UNLOCK      0x03
#define MALI_AS_CMD_FLUSH       0x04   /* Flush L2 + flush region (deprecated T60x) */
#define MALI_AS_CMD_FLUSH_PT    0x04   /* Flush L2 + flush region */
#define MALI_AS_CMD_FLUSH_MEM            0x05   /* Wait + flush L1s + flush L2 + flush region */

/* --- MMU AS_TRANSCFG address mode values --- */
#define MALI_AS_TRANSCFG_ADRMODE_LEGACY      0
#define MALI_AS_TRANSCFG_ADRMODE_UNMAPPED    1
#define MALI_AS_TRANSCFG_ADRMODE_IDENTITY    2
#define MALI_AS_TRANSCFG_ADRMODE_AARCH64_4K  6
#define MALI_AS_TRANSCFG_ADRMODE_AARCH64_64K 8

/* --- MMU AS_TRANSTAB address mode values --- */
#define MALI_AS_TRANSTAB_ADRMODE_UNMAPPED  (0u << 0)
#define MALI_AS_TRANSTAB_ADRMODE_IDENTITY  (1u << 1)
#define MALI_AS_TRANSTAB_ADRMODE_TABLE     (3u << 0)

/* --- AS_MEMATTR values (AARCH64) --- */
#define MALI_AS_MEMATTR_IMPL_DEF_CACHE_POLICY 0x88ull
#define MALI_AS_MEMATTR_FORCE_TO_CACHE_ALL    0x8Full
#define MALI_AS_MEMATTR_WRITE_ALLOC           0x8Dull
#define MALI_AS_MEMATTR_AARCH64_OUTER_IMPL_DEF 0x88ull
#define MALI_AS_MEMATTR_AARCH64_OUTER_WA       0x8Dull
#define MALI_AS_MEMATTR_AARCH64_NON_CACHEABLE  0x4Cull

/* --- AS_TRANSCFG PTW memory attribute --- */
#define MALI_AS_TRANSCFG_PTW_MEMATTR_MASK      (3ull << 24)
#define MALI_AS_TRANSCFG_PTW_MEMATTR_NON_CACHEABLE (1ull << 24)
#define MALI_AS_TRANSCFG_PTW_MEMATTR_WRITE_BACK (2ull << 24)

/* --- AS_TRANSCFG PTW shareability --- */
#define MALI_AS_TRANSCFG_PTW_SH_MASK     ((3ull << 28))
#define MALI_AS_TRANSCFG_PTW_SH_OS       (2ull << 28)
#define MALI_AS_TRANSCFG_PTW_SH_IS       (3ull << 28)
#define MALI_AS_TRANSCFG_PTW_SH_IS       (3ull << 28)
#define MALI_AS_TRANSCFG_R_ALLOCATE      (1ull << 30)

/* --- Power domain IDs --- */
#define MALI_POWER_DOMAIN_SHADER  0
#define MALI_POWER_DOMAIN_TILER   1
#define MALI_POWER_DOMAIN_L2      2
#define MALI_POWER_DOMAIN_CORE    3

/* --- BO flags --- */
#define MALI_BO_FLAG_READ_ONLY     (1u << 0)
#define MALI_BO_FLAG_CACHED        (1u << 1)
#define MALI_BO_FLAG_CONTIGUOUS    (1u << 2)
#define MALI_BO_FLAG_SHARED        (1u << 3)
#define MALI_BO_FLAG_IOSURFACE     (1u << 4)
#define MALI_BO_FLAG_EXTERNAL      (1u << 5)

/* --- Job types --- */
#define MALI_JOB_TYPE_VERTEX    0
#define MALI_JOB_TYPE_FRAGMENT  1
#define MALI_JOB_TYPE_COMPUTE   2
#define MALI_JOB_TYPE_TILER     3

/* --- Driver limits --- */
#define MALI_MAX_ADDRESS_SPACES  8
#define MALI_MAX_JOB_SLOTS       8
#define MALI_MAX_JOB_CHAIN       256
#define MALI_FIRMWARE_PATH       "/vendor/firmware/mali_g71.bin"

/* --- Core initialization --- */
void dreamlte_gpu_init(void);
void dreamlte_gpu_shutdown(void);
bool dreamlte_gpu_is_initialized(void);
uint32_t dreamlte_gpu_get_id(void);
int dreamlte_gpu_power_on(void);
int dreamlte_gpu_power_off(void);
int dreamlte_gpu_reset(void);
void dreamlte_gpu_dump_regs(void);

/* Memory management / MMU */
int mali_as_init(uint32_t as_id);
int mali_as_configure(uint32_t as_id, uint64_t ttbr0, uint32_t transcfg, uint32_t memattr);
int mali_as_map(uint32_t as_id, uint64_t gpu_addr, uint64_t phys_addr, uint64_t size, uint32_t flags);
int mali_as_unmap(uint32_t as_id, uint64_t gpu_addr, uint64_t size);
int mali_as_flush_pt(uint32_t as_id);
void mali_as_fini(uint32_t as_id);

/* Buffer object management */
void *mali_bo_create(uint64_t size, uint32_t flags);
void *mali_bo_create_contiguous(uint64_t size, uint32_t flags);
void *mali_bo_create_from_iosurface(void *iosurface);
void mali_bo_ref(void *bo);
void mali_bo_unref(void *bo);
int mali_bo_map(void *bo, uint32_t as_id, uint64_t gpu_addr, uint64_t phys_addr);
int mali_bo_unmap(void *bo);
void *mali_bo_kmap(void *bo);
void mali_bo_kunmap(void *bo);
int mali_bo_pin(void *bo);
int mali_bo_unpin(void *bo);

/* Cache coherency */
int mali_l2_flush(void);
int mali_l2_invalidate(void);
int mali_l2_clean_invalidate(uint64_t addr, uint64_t size);
int mali_bo_flush(void *bo, uint64_t offset, uint64_t size);
int mali_bo_invalidate(void *bo, uint64_t offset, uint64_t size);

/* Job submission */
void *mali_job_create(uint32_t type, uint32_t priority);
int mali_job_submit(void *job);
int mali_job_wait(void *job, uint64_t timeout_ns);
void mali_job_finish(void *job);
void mali_job_free(void *job);
int mali_job_descriptor_build(void *job, uint64_t *out_desc, uint32_t *out_size);

/* Ring buffer management */
int mali_ring_init(uint32_t js);
int mali_ring_submit(uint32_t js, void *job);
bool mali_ring_has_space(uint32_t js, uint32_t needed);
void mali_ring_advance_head(uint32_t js);

/* Synchronization / Fences */
void *mali_fence_create(uint32_t context);
int mali_fence_signal(void *fence);
int mali_fence_wait(void *fence, uint64_t timeout_ns);
int mali_fence_wait_any(void **fences, uint32_t count, uint64_t timeout_ns);
bool mali_fence_is_signaled(void *fence);
void mali_fence_ref(void *fence);
void mali_fence_unref(void *fence);

/* Timeline semaphores */
void *mali_timeline_create(uint32_t context);
int mali_timeline_signal(void *tl, uint32_t seqno);
int mali_timeline_wait(void *tl, uint32_t seqno, uint64_t timeout_ns);
void mali_timeline_destroy(void *tl);

/* Power management */
int mali_power_domain_on(uint32_t domain);
int mali_power_domain_off(uint32_t domain);
int mali_dvfs_set_freq(uint32_t freq);
uint32_t mali_dvfs_get_freq(void);
int mali_dvfs_init(void);
void mali_idle_detect(void);

/* Metal / Compute support */
int mali_compute_job_submit(uint64_t shader_addr, uint32_t shader_size,
                            uint64_t *uniforms, uint32_t num_uniforms,
                            uint64_t *textures, uint32_t num_textures,
                            uint64_t *samplers, uint32_t num_samplers,
                            uint32_t workgroup_x, uint32_t workgroup_y, uint32_t workgroup_z,
                            void **out_fence);
int mali_shader_upload(const void *bytecode, uint32_t size, uint64_t *out_addr);
int mali_uniform_buffer_create(uint64_t size, uint64_t *out_addr);
int mali_descriptor_set_create(uint32_t num_bindings, uint64_t *out_addr);

/* Debug / Profiling */
int mali_perf_counter_enable(uint32_t counter, uint32_t src);
int mali_perf_counter_disable(uint32_t counter);
uint64_t mali_perf_counter_read(uint32_t counter);
void mali_perf_counter_dump(void);
int mali_hang_detect_enable(uint32_t threshold_ms);
void mali_hang_recovery(void);
boolean_t mali_check_hang(void);
void mali_activity_update(void);
void mali_trace_enable(bool enable);
void mali_trace_record(uint32_t event, uint64_t data);
void mali_trace_dump(void);

/* Interrupt handling */
int mali_irq_install(int irq);
void mali_irq_remove(void);

/* Context management */
void *mali_context_create(uint32_t pid);
void mali_context_destroy(void *ctx);
int mali_context_submit(void *ctx, void *job);

/* Utility functions */
uint64_t mali_get_gpu_time(void);
void mali_settle(void);
void mali_to_memory(void *start, uint64_t size);

__END_DECLS

#endif /* _SYS_DREAMLTE_GPU_H_ */


/* =========================================================================
 * Compatibility aliases for short register names used by the driver.
 * These map the short MALI_* names to the MALI_REG_* definitions above.
 * ========================================================================= */

/* GPU control register short names */
#define MALI_AS_PRESENT           MALI_REG_AS_PRESENT
#define MALI_CORE_FEATURES        MALI_REG_CORE_FEATURES
#define MALI_GPU_FEATURES         MALI_REG_CORE_FEATURES
#define MALI_GPU_ID               MALI_REG_GPU_ID
#define MALI_IRQ_BASE             MALI_GPU_CONTROL_BASE
#define MALI_IRQ_CLEAR            MALI_REG_GPU_IRQ_CLEAR
#define MALI_IRQ_MASK             MALI_REG_GPU_IRQ_MASK
#define MALI_IRQ_RAWSTAT          MALI_REG_GPU_IRQ_RAWSTAT
#define MALI_IRQ_STATUS           MALI_REG_GPU_IRQ_STATUS
#define MALI_L2_FEATURES          MALI_REG_L2_FEATURES
#define MALI_L2_PRESENT           MALI_REG_L2_PRESENT
#define MALI_MMU_FEATURES         MALI_REG_MMU_FEATURES
#define MALI_TILER_FEATURES       MALI_REG_TILER_FEATURES
#define MALI_TILER_PRESENT        MALI_REG_TILER_PRESENT

/* Job slot register short names */
#define MALI_JS_BASE              MALI_JOB_CONTROL_BASE
#define MALI_JS_CONFIG            MALI_REG_JS_CONFIG
#define MALI_JS_CTL               MALI_REG_JS_COMMAND
#define MALI_JS_CTL_STOP          MALI_JS_CMD_HARD_STOP
#define MALI_JS_FEATURES          MALI_REG_JS_FEATURES(0)
#define MALI_JS_HEAD_HI           MALI_REG_JS_HEAD_HI
#define MALI_JS_HEAD_LO           MALI_REG_JS_HEAD_LO
#define MALI_JS_PRESENT           MALI_REG_JS_PRESENT
#define MALI_JS_SIZE              0x80
#define MALI_JS_STATUS            MALI_REG_JS_STATUS
#define MALI_JS_TAIL_HI           MALI_REG_JS_TAIL_HI
#define MALI_JS_TAIL_LO           MALI_REG_JS_TAIL_LO
#define MALI_NUM_JOB_SLOTS        MALI_MAX_JOB_SLOTS

/* Power management short names */
#define MALI_PM_BASE              MALI_GPU_CONTROL_BASE
#define MALI_PM_L2_PWR_OFF        MALI_REG_L2_PWROFF_LO
#define MALI_PM_L2_PWR_ON         MALI_REG_L2_PWRON_LO
#define MALI_PM_POWER_CONTROL     MALI_REG_PWR_KEY
#define MALI_PM_POWER_STATUS      MALI_REG_PWR_OVERRIDE0
#define MALI_PM_SHADER_PWR_OFF    MALI_REG_SHADER_PWROFF_LO
#define MALI_PM_SHADER_PWR_ON     MALI_REG_SHADER_PWRON_LO
#define MALI_PM_STATUS_L2_ON      0x00000001
#define MALI_PM_STATUS_SHADER_ON  0x00000001
#define MALI_PM_STATUS_TILER_ON   0x00000001
#define MALI_PM_TILER_PWR_OFF     MALI_REG_TILER_PWROFF_LO
#define MALI_PM_TILER_PWR_ON      MALI_REG_TILER_PWRON_LO

/* =========================================================================
 * GPU ID version extraction macros (Midgard register layout)
 * ========================================================================= */
#define MALI_MAJOR_VERSION(id)    (((id) >> 28) & 0xF)
#define MALI_MINOR_VERSION(id)    (((id) >> 24) & 0xF)
#define MALI_MIDGARD_VERSION(id)  (((id) >> 16) & 0xFF)

/* =========================================================================
 * Forward declaration for delay helper (implemented in dreamlte_gpu.c)
 * ========================================================================= */
void delay_for_interval(uint32_t interval, uint32_t unit);

/* =========================================================================
 * Mali GPU ID register bitfield definitions (from mali_kbase_gpu_id.h)
 * ========================================================================= */
#define GPU_ID_VERSION_PRODUCT_ID        0x0000FFFF
#define GPU_ID_VERSION_PRODUCT_ID_SHIFT  0
#define GPU_ID_PI_NEW_FORMAT_START       0x00002000
#define GPU_ID_PI_T60X                   0x00002000
#define GPU_ID2_PRODUCT_MODEL            0x0000FFFF
#define GPU_ID_VERSION_MAJOR             0x0000000F
#define GPU_ID_VERSION_MAJOR_SHIFT       28

