/*
 * Samsung Exynos 8895 Power Management driver.
 * Provides CPU idle states, DVFS (Dynamic Voltage and Frequency Scaling),
 * and thermal management for the Exynos 8895 (4x M2 + 4x Cortex-A53).
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

/* Exynos Power Management Unit (PMU) registers */
#define EXYNOS_PMU_BASE         0x10040000
#define EXYNOS_PMU_SIZE         0x00040000

#define EXYNOS_PMU_PWR_CFG      0x0000  /* Power domain configuration */
#define EXYNOS_PMU_PWR_STS      0x0004  /* Power domain status */
#define EXYNOS_PMU_CPU_CFG      0x0010  /* CPU configuration */
#define EXYNOS_PMU_CPU_STS      0x0014  /* CPU status */
#define EXYNOS_PMU_CLK_GATE     0x0020  /* Clock gating control */
#define EXYNOS_PMU_INT_EN       0x0030  /* Interrupt enable */
#define EXYNOS_PMU_INT_STS      0x0034  /* Interrupt status */
#define EXYNOS_PMU_TIMER        0x0040  /* Power timer */

/* Exynos CPM (Clock and Power Management) registers */
#define EXYNOS_CPM_BASE         0x10050000
#define EXYNOS_CPM_SIZE         0x00100000

#define EXYNOS_CPM_APLL_CON0    0x0100  /* APLL configuration 0 */
#define EXYNOS_CPM_APLL_CON1    0x0104  /* APLL configuration 1 */
#define EXYNOS_CPM_MPLL_CON0    0x0110  /* MPLL configuration 0 */
#define EXYNOS_CPM_DIV_CON      0x0200  /* Divider configuration */
#define EXYNOS_CPM_CLK_GATE     0x0300  /* Clock gating */

/* Exynos MU (Management Unit) for DVFS */
#define EXYNOS_MU_BASE          0x10060000
#define EXYNOS_MU_SIZE          0x00040000

#define EXYNOS_MU_CPU0_CON      0x0000  /* CPU0 (cluster 0) control */
#define EXYNOS_MU_CPU1_CON      0x0010  /* CPU1 (cluster 1) control */
#define EXYNOS_MU_CPU2_CON      0x0020  /* CPU2 (cluster 0) control */
#define EXYNOS_MU_CPU3_CON      0x0030  /* CPU3 (cluster 0) control */
#define EXYNOS_MU_CPU4_CON      0x0040  /* CPU4 (cluster 1) control */
#define EXYNOS_MU_CPU5_CON      0x0050  /* CPU5 (cluster 1) control */
#define EXYNOS_MU_CPU6_CON      0x0060  /* CPU6 (cluster 1) control */
#define EXYNOS_MU_CPU7_CON      0x0070  /* CPU7 (cluster 1) control */

#define EXYNOS_MU_CLUSTER0_CON  0x0100  /* Cluster 0 (A53) control */
#define EXYNOS_MU_CLUSTER1_CON  0x0110  /* Cluster 1 (M2) control */

/* Thermal management registers */
#define EXYNOS_TMU_BASE         0x10070000
#define EXYNOS_TMU_SIZE         0x00010000

#define EXYNOS_TMU_CON          0x0000  /* TMU control */
#define EXYNOS_TMU_TEMP         0x0004  /* Temperature reading */
#define EXYNOS_TMU_INT_EN       0x0010  /* Interrupt enable */
#define EXYNOS_TMU_INT_STS      0x0014  /* Interrupt status */
#define EXYNOS_TMU_TRIM         0x0020  /* Trim values */

static volatile uint32_t *dreamlte_pmu_base = NULL;
static volatile uint32_t *dreamlte_cpm_base = NULL;
static volatile uint32_t *dreamlte_mu_base = NULL;
static volatile uint32_t *dreamlte_tmu_base = NULL;
static bool dreamlte_pm_initialized = false;

/* DVFS frequency tables */
/* Cluster 0 (Cortex-A53): 400MHz - 1.7GHz */
static const uint32_t dreamlte_a53_freqs[] = {
    400000, 600000, 800000, 1000000, 1200000, 1400000, 1500000, 1700000
};

/* Cluster 1 (M2): 600MHz - 2.3GHz */
static const uint32_t dreamlte_m2_freqs[] = {
    600000, 800000, 1000000, 1200000, 1400000, 1600000, 1800000, 2000000, 2100000, 2200000, 2300000
};

static struct mtx dreamlte_pm_lock;

/*
 * Initialize power management
 */
int
dreamlte_pm_init(void)
{
    if (dreamlte_pm_initialized) return 0;

    mtx_init(&dreamlte_pm_lock, "dreamlte_pm", NULL, MTX_DEF);

    /* Map PMU, CPM, MU, TMU registers */
    dreamlte_pmu_base = (volatile uint32_t *)(uintptr_t)EXYNOS_PMU_BASE;
    dreamlte_cpm_base = (volatile uint32_t *)(uintptr_t)EXYNOS_CPM_BASE;
    dreamlte_mu_base = (volatile uint32_t *)(uintptr_t)EXYNOS_MU_BASE;
    dreamlte_tmu_base = (volatile uint32_t *)(uintptr_t)EXYNOS_TMU_BASE;

    printf("dreamlte_pm: PMU=0x%p, CPM=0x%p, MU=0x%p, TMU=0x%p\n",
           dreamlte_pmu_base, dreamlte_cpm_base, dreamlte_mu_base, dreamlte_tmu_base);

    /* Enable all CPU power domains */
    uint32_t pwr_cfg = dreamlte_pmu_base[EXYNOS_PMU_PWR_CFG / 4];
    pwr_cfg |= 0xFF; /* Enable all 8 CPU power domains */
    dreamlte_pmu_base[EXYNOS_PMU_PWR_CFG / 4] = pwr_cfg;

    /* Wait for power domains to be ready */
    for (int i = 0; i < 1000; i++) {
        if ((dreamlte_pmu_base[EXYNOS_PMU_PWR_STS / 4] & 0xFF) == 0xFF) break;
    }

    /* Enable clock gating for idle states */
    dreamlte_pmu_base[EXYNOS_PMU_CLK_GATE / 4] = 0xFFFFFFFF;

    /* Initialize TMU for thermal monitoring */
    dreamlte_tmu_base[EXYNOS_TMU_CON / 4] = 0x1; /* Enable TMU */

    dreamlte_pm_initialized = true;
    printf("dreamlte_pm: initialized\n");

    return 0;
}

/*
 * Set CPU frequency using DVFS
 * cluster: 0 (A53) or 1 (M2)
 * freq_idx: index into frequency table
 */
int
dreamlte_cpu_set_freq(int cluster, int freq_idx)
{
    if (!dreamlte_pm_initialized) return -1;
    if (cluster < 0 || cluster > 1) return -1;

    mtx_lock(&dreamlte_pm_lock);

    const uint32_t *freqs = (cluster == 0) ? dreamlte_a53_freqs : dreamlte_m2_freqs;
    int num_freqs = (cluster == 0) ? 8 : 11;

    if (freq_idx < 0 || freq_idx >= num_freqs) {
        mtx_unlock(&dreamlte_pm_lock);
        return -1;
    }

    uint32_t freq = freqs[freq_idx];
    uint32_t pll_con = (cluster == 0) ? EXYNOS_CPM_APLL_CON0 : EXYNOS_CPM_MPLL_CON0;

    /* Calculate PLL settings for target frequency */
    /* This is simplified - actual PLL calculation is more complex */
    uint32_t div_val = (100000000 * 2) / freq; /* Example calculation */

    /* Update PLL configuration */
    dreamlte_cpm_base[pll_con / 4] = div_val;

    /* Update divider */
    uint32_t div_con = dreamlte_cpm_base[EXYNOS_CPM_DIV_CON / 4];
    if (cluster == 0) {
        div_con = (div_con & ~0xFF) | (div_val & 0xFF);
    } else {
        div_con = (div_con & ~0xFF00) | ((div_val & 0xFF) << 8);
    }
    dreamlte_cpm_base[EXYNOS_CPM_DIV_CON / 4] = div_con;

    mtx_unlock(&dreamlte_pm_lock);

    printf("dreamlte_pm: cluster %d frequency set to %u MHz\n", cluster, freq / 1000000);
    return 0;
}

/*
 * Get current CPU frequency
 */
uint32_t
dreamlte_cpu_get_freq(int cluster)
{
    if (!dreamlte_pm_initialized) return 0;
    if (cluster < 0 || cluster > 1) return 0;

    /* Read current PLL/divider settings and calculate frequency */
    /* Simplified - return max frequency for now */
    return (cluster == 0) ? 1700000000 : 2300000000;
}

/*
 * Enter CPU idle state (WFI with power gating)
 * state: 0 = WFI, 1 = retention, 2 = power down
 */
void
dreamlte_cpu_idle(int state)
{
    if (!dreamlte_pm_initialized) return;

    if (state == 0) {
        /* Simple WFI */
        __asm__ volatile ("wfi");
        return;
    }

    mtx_lock(&dreamlte_pm_lock);

    /* Configure power mode for retention or power down */
    uint32_t cpu_cfg = dreamlte_pmu_base[EXYNOS_PMU_CPU_CFG / 4];

    if (state == 1) {
        /* Retention mode - keep cache, power down logic */
        cpu_cfg |= (1 << 8); /* Retention enable */
    } else if (state == 2) {
        /* Power down - full power off */
        cpu_cfg |= (1 << 16); /* Power down enable */
    }

    dreamlte_pmu_base[EXYNOS_PMU_CPU_CFG / 4] = cpu_cfg;

    mtx_unlock(&dreamlte_pm_lock);

    /* Execute WFI to enter idle state */
    __asm__ volatile ("wfi");

    /* Restore power mode on wake */
    mtx_lock(&dreamlte_pm_lock);
    dreamlte_pmu_base[EXYNOS_PMU_CPU_CFG / 4] = 0;
    mtx_unlock(&dreamlte_pm_lock);
}

/*
 * Read temperature from TMU
 * Returns temperature in millicelsius
 */
int32_t
dreamlte_thermal_get_temp(void)
{
    if (!dreamlte_pm_initialized) return -273000;

    uint32_t temp_raw = dreamlte_tmu_base[EXYNOS_TMU_TEMP / 4];

    /* Convert raw value to millicelsius */
    /* Formula varies by calibration; this is approximate */
    int32_t temp_mc = (int32_t)(temp_raw & 0xFFF) * 1000 / 4 - 40000; /* Approximate */

    return temp_mc;
}

/*
 * Check if thermal throttling is needed
 */
bool
dreamlte_thermal_throttle_needed(void)
{
    int32_t temp = dreamlte_thermal_get_temp();
    return temp > 85000; /* Throttle above 85°C */
}

/*
 * Get thermal zone info
 */
void
dreamlte_thermal_get_info(int *temp_mc, int *throttle_temp_mc)
{
    if (temp_mc) *temp_mc = dreamlte_thermal_get_temp();
    if (throttle_temp_mc) *throttle_temp_mc = 85000;
}

/*
 * Power domain control
 */
int
dreamlte_pm_power_domain(int domain, bool on)
{
    if (!dreamlte_pm_initialized) return -1;
    if (domain < 0 || domain >= 8) return -1;

    mtx_lock(&dreamlte_pm_lock);

    uint32_t pwr_cfg = dreamlte_pmu_base[EXYNOS_PMU_PWR_CFG / 4];

    if (on) {
        pwr_cfg |= (1 << domain);
    } else {
        pwr_cfg &= ~(1 << domain);
    }

    dreamlte_pmu_base[EXYNOS_PMU_PWR_CFG / 4] = pwr_cfg;

    /* Wait for status change */
    uint32_t mask = 1 << domain;
    for (int i = 0; i < 1000; i++) {
        uint32_t sts = dreamlte_pmu_base[EXYNOS_PMU_PWR_STS / 4];
        if (on && (sts & mask)) break;
        if (!on && !(sts & mask)) break;
    }

    mtx_unlock(&dreamlte_pm_lock);
    return 0;
}

/*
 * Check if PM is initialized
 */
bool
dreamlte_pm_is_initialized(void)
{
    return dreamlte_pm_initialized;
}