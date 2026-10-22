/*
 * Samsung Exynos 8895 GICv3 driver.
 * The Exynos 8895 uses ARM GICv3 (GIC-500).
 * This driver provides initialization and basic interrupt handling.
 * Much of the GICv3 handling is done in osfmk/arm64/gic_v3_irq.c
 * This file provides Exynos-specific initialization and quirks.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <arm/cpufunc.h>
#include <arm/pmap.h>

/* GICv3 register offsets (Distributor) */
#define GICD_CTLR         0x0000  /* Control Register */
#define GICD_TYPER        0x0004  /* Interrupt Controller Type Register */
#define GICD_IIDR         0x0008  /* Implementer Identification Register */
#define GICD_STATUSR      0x0010  /* Status Register */
#define GICD_SETSPI_NSR   0x0040  /* Set SPI Register (Non-secure) */
#define GICD_CLRSPI_NSR   0x0048  /* Clear SPI Register (Non-secure) */
#define GICD_SETSPI_SR    0x0050  /* Set SPI Register (Secure) */
#define GICD_CLRSPI_SR    0x0058  /* Clear SPI Register (Secure) */
#define GICD_IGROUPR      0x0080  /* Interrupt Group Registers */
#define GICD_ISENABLER    0x0100  /* Interrupt Set-Enable Registers */
#define GICD_ICENABLER    0x0180  /* Interrupt Clear-Enable Registers */
#define GICD_ISPENDR      0x0200  /* Interrupt Set-Pending Registers */
#define GICD_ICPENDR      0x0280  /* Interrupt Clear-Pending Registers */
#define GICD_ISACTIVER    0x0300  /* Interrupt Set-Active Registers */
#define GICD_ICACTIVER    0x0380  /* Interrupt Clear-Active Registers */
#define GICD_IPRIORITYR   0x0400  /* Interrupt Priority Registers */
#define GICD_ITARGETSR    0x0800  /* Interrupt Targets Registers (GICv2) */
#define GICD_ICFGR        0x0C00  /* Interrupt Configuration Registers */
#define GICD_IGRPMODR     0x0D00  /* Interrupt Group Modifier Registers */
#define GICD_NSACR        0x0E00  /* Non-secure Access Control Registers */
#define GICD_SGIR         0x0F00  /* Software Generated Interrupt Register */
#define GICD_CPENDSGIR    0x0F10  /* SGI Clear-Pending Registers */
#define GICD_SPENDSGIR    0x0F20  /* SGI Set-Pending Registers */

/* GICv3 Redistributor registers (per CPU) */
#define GICR_CTLR         0x0000  /* Redistributor Control Register */
#define GICR_IIDR         0x0004  /* Redistributor IIDR */
#define GICR_TYPER        0x0008  /* Redistributor Type Register */
#define GICR_STATUSR      0x0010  /* Redistributor Status Register */
#define GICR_WAKER        0x0014  /* Redistributor Wake Register */
#define GICR_PROPBASER    0x0070  /* Redistributor Property Table Base Address Register */
#define GICR_PENDBASER    0x0078  /* Redistributor Pending Table Base Address Register */
#define GICR_IGROUPR0     0x1000  /* Redistributor Interrupt Group Register 0 */
#define GICR_ISENABLER0   0x1080  /* Redistributor Interrupt Set-Enable Register 0 */
#define GICR_ICENABLER0   0x1100  /* Redistributor Interrupt Clear-Enable Register 0 */
#define GICR_ISPENDR0     0x1180  /* Redistributor Interrupt Set-Pending Register 0 */
#define GICR_ICPENDR0     0x1200  /* Redistributor Interrupt Clear-Pending Register 0 */
#define GICR_ISACTIVER0   0x1280  /* Redistributor Interrupt Set-Active Register 0 */
#define GICR_ICACTIVER0   0x1300  /* Redistributor Interrupt Clear-Active Register 0 */
#define GICR_IPRIORITYR   0x1400  /* Redistributor Interrupt Priority Registers */
#define GICR_ICFGR0       0x1C00  /* Redistributor Interrupt Configuration Register 0 */
#define GICR_ICFGR1       0x1C04  /* Redistributor Interrupt Configuration Register 1 */

/* GICv3 CPU Interface registers (per CPU) */
#define GICC_CTLR         0x0000  /* CPU Interface Control Register */
#define GICC_PMR          0x0004  /* Priority Mask Register */
#define GICC_BPR          0x0008  /* Binary Point Register */
#define GICC_IAR          0x0010  /* Interrupt Acknowledge Register */
#define GICC_EOIR         0x0018  /* End of Interrupt Register */
#define GICC_RPR          0x0020  /* Running Priority Register */
#define GICC_HPPIR        0x0028  /* Highest Priority Pending Interrupt Register */
#define GICC_ABPR         0x0030  /* Aliased Binary Point Register */
#define GICC_AIAR         0x0038  /* Aliased Interrupt Acknowledge Register */
#define GICC_AEOIR        0x0040  /* Aliased End of Interrupt Register */
#define GICC_APR          0x00D0  /* Active Priority Register */
#define GICC_NSAPR        0x00E0  /* Non-secure Active Priority Register */

/* GICD_CTLR bits */
#define GICD_CTLR_ENABLE_GRP0   (1 << 0)   /* Enable Group 0 */
#define GICD_CTLR_ENABLE_GRP1NS (1 << 1)   /* Enable Group 1 Non-secure */
#define GICD_CTLR_ENABLE_GRP1S  (1 << 2)   /* Enable Group 1 Secure */
#define GICD_CTLR_ARE_NS        (1 << 4)   /* Affinity Routing Enable (Non-secure) */
#define GICD_CTLR_ARE_S         (1 << 5)   /* Affinity Routing Enable (Secure) */
#define GICD_CTLR_DS            (1 << 6)   /* Disable Security */

/* GICR_WAKER bits */
#define GICR_WAKER_PROCESSOR_SLEEP (1 << 1) /* Processor Sleep */
#define GICR_WAKER_CHILDREN_ASLEEP (1 << 2) /* Children Asleep */

/* Default Exynos 8895 GIC addresses */
#define EXYNOS_GIC_DIST_BASE    0x2C000000
#define EXYNOS_GIC_REDIST_BASE  0x2C080000
#define EXYNOS_GIC_REDIST_SIZE  0x00800000  /* 8MB for 8 cores */

static volatile uint32_t *dreamlte_gic_dist_base = NULL;
static volatile uint32_t *dreamlte_gic_redist_base = NULL;
static bool dreamlte_gic_initialized = false;

/*
 * Initialize the GICv3 for Exynos 8895.
 * dist_base: Physical address of GIC distributor
 * redist_base: Physical address of GIC redistributor base
 */
void
dreamlte_gic_init(uint64_t dist_base, uint64_t redist_base)
{
    if (dreamlte_gic_initialized) {
        return;
    }

    dreamlte_gic_dist_base = (volatile uint32_t *)(uintptr_t)dist_base;
    dreamlte_gic_redist_base = (volatile uint32_t *)(uintptr_t)redist_base;

    printf("dreamlte_gic: distributor at 0x%llx, redistributor at 0x%llx\n", dist_base, redist_base);

    /* Read GIC type register to verify */
    uint32_t typer = dreamlte_gic_dist_base[GICD_TYPER / 4];
    uint32_t iidr = dreamlte_gic_dist_base[GICD_IIDR / 4];
    printf("dreamlte_gic: TYPER=0x%08x, IIDR=0x%08x\n", typer, iidr);

    /* Enable GIC distributor for Group 1 (non-secure) */
    uint32_t ctlr = dreamlte_gic_dist_base[GICD_CTLR / 4];
    ctlr |= GICD_CTLR_ENABLE_GRP1NS | GICD_CTLR_ARE_NS;
    dreamlte_gic_dist_base[GICD_CTLR / 4] = ctlr;

    /* Wake up redistributors */
    for (int cpu = 0; cpu < 8; cpu++) {
        volatile uint32_t *redist = (volatile uint32_t *)((uintptr_t)redist_base + cpu * 0x100000);
        uint32_t waker = redist[GICR_WAKER / 4];
        if (waker & GICR_WAKER_PROCESSOR_SLEEP) {
            redist[GICR_WAKER / 4] = waker & ~GICR_WAKER_PROCESSOR_SLEEP;
            /* Wait for children to wake up */
            while (redist[GICR_WAKER / 4] & GICR_WAKER_CHILDREN_ASLEEP) {
                /* Spin */
            }
        }
    }

    /* Set all SPIs to Group 1 (non-secure), level-triggered */
    /* SPIs start at ID 32, 16 per register */
    int num_spi = ((typer & 0x1F) + 1) * 32;  /* Number of SPIs */
    for (int i = 32; i < num_spi; i += 16) {
        int reg = i / 16;
        /* Set to Group 1 */
        dreamlte_gic_dist_base[(GICD_IGROUPR + reg * 4) / 4] = 0xFFFFFFFF;
        /* Set to level-triggered (0 = level, 1 = edge) */
        dreamlte_gic_dist_base[(GICD_ICFGR + reg * 4) / 4] = 0x00000000;
    }

    /* Set default priorities for all interrupts */
    for (int i = 0; i < num_spi; i += 4) {
        int reg = i / 4;
        dreamlte_gic_dist_base[(GICD_IPRIORITYR + reg * 4) / 4] = 0xA0A0A0A0;  /* Medium priority */
    }

    dreamlte_gic_initialized = true;
    printf("dreamlte_gic: initialized with %d SPIs\n", num_spi - 32);
}

/*
 * Enable a specific interrupt (SPI).
 */
void
dreamlte_gic_enable_irq(int irq)
{
    if (!dreamlte_gic_initialized || irq < 32) {
        return;
    }

    int reg = (irq - 32) / 32;
    int bit = (irq - 32) % 32;
    dreamlte_gic_dist_base[(GICD_ISENABLER + reg * 4) / 4] = (1 << bit);
}

/*
 * Disable a specific interrupt (SPI).
 */
void
dreamlte_gic_disable_irq(int irq)
{
    if (!dreamlte_gic_initialized || irq < 32) {
        return;
    }

    int reg = (irq - 32) / 32;
    int bit = (irq - 32) % 32;
    dreamlte_gic_dist_base[(GICD_ICENABLER + reg * 4) / 4] = (1 << bit);
}

/*
 * Set interrupt priority.
 */
void
dreamlte_gic_set_priority(int irq, uint8_t priority)
{
    if (!dreamlte_gic_initialized || irq < 32) {
        return;
    }

    int reg = (irq - 32) / 4;
    int shift = ((irq - 32) % 4) * 8;
    uint32_t val = dreamlte_gic_dist_base[(GICD_IPRIORITYR + reg * 4) / 4];
    val = (val & ~(0xFF << shift)) | ((uint32_t)priority << shift);
    dreamlte_gic_dist_base[(GICD_IPRIORITYR + reg * 4) / 4] = val;
}

/*
 * Set interrupt target CPU (affinity routing).
 */
void
dreamlte_gic_set_target(int irq, int cpu)
{
    if (!dreamlte_gic_initialized || irq < 32 || cpu < 0 || cpu >= 8) {
        return;
    }

    /* For GICv3 with ARE, use GICR_PROPBASER/PENDBASER instead of ITARGETSR */
    /* This is handled by the generic GICv3 code in osfmk/arm64/gic_v3_irq.c */
}

/*
 * Send SGI (Software Generated Interrupt) to target CPUs.
 */
void
dreamlte_gic_send_sgi(int sgi_id, uint16_t target_list, int filter)
{
    if (!dreamlte_gic_initialized || sgi_id >= 16) {
        return;
    }

    uint64_t sgi = ((uint64_t)target_list << 16) | ((uint64_t)filter << 24) | sgi_id;
    dreamlte_gic_dist_base[GICD_SGIR / 4] = (uint32_t)sgi;
    dreamlte_gic_dist_base[GICD_SGIR / 4 + 1] = (uint32_t)(sgi >> 32);
}

/*
 * Get the current CPU's GIC redistributor base.
 */
volatile uint32_t *
dreamlte_gic_get_redist_base(int cpu)
{
    if (!dreamlte_gic_initialized || cpu < 0 || cpu >= 8) {
        return NULL;
    }
    return (volatile uint32_t *)((uintptr_t)dreamlte_gic_redist_base + cpu * 0x100000);
}

/*
 * Check if GIC is initialized.
 */
bool
dreamlte_gic_is_initialized(void)
{
    return dreamlte_gic_initialized;
}