/*
 * Samsung Exynos 8895 Power Management driver header.
 */

#ifndef _SYS_DREAMLTE_PM_H_
#define _SYS_DREAMLTE_PM_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

/* Initialize power management */
int dreamlte_pm_init(void);

/* Set CPU frequency via DVFS */
int dreamlte_cpu_set_freq(int cluster, int freq_idx);

/* Get current CPU frequency */
uint32_t dreamlte_cpu_get_freq(int cluster);

/* Enter CPU idle state */
void dreamlte_cpu_idle(int state);

/* Get temperature in millicelsius */
int32_t dreamlte_thermal_get_temp(void);

/* Check if thermal throttling needed */
bool dreamlte_thermal_throttle_needed(void);

/* Get thermal zone info */
void dreamlte_thermal_get_info(int *temp_mc, int *throttle_temp_mc);

/* Power domain control */
int dreamlte_pm_power_domain(int domain, bool on);

/* Check if PM is initialized */
bool dreamlte_pm_is_initialized(void);

__END_DECLS

#endif /* _SYS_DREAMLTE_PM_H_ */