/*
 * Samsung Exynos 8895 GICv3 driver header.
 */

#ifndef _SYS_DREAMLTE_GIC_H_
#define _SYS_DREAMLTE_GIC_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

/* Initialize GICv3 */
void dreamlte_gic_init(uint64_t dist_base, uint64_t redist_base);

/* Enable interrupt */
void dreamlte_gic_enable_irq(int irq);

/* Disable interrupt */
void dreamlte_gic_disable_irq(int irq);

/* Set interrupt priority */
void dreamlte_gic_set_priority(int irq, uint8_t priority);

/* Set interrupt target CPU */
void dreamlte_gic_set_target(int irq, int cpu);

/* Send SGI */
void dreamlte_gic_send_sgi(int sgi_id, uint16_t target_list, int filter);

/* Get redistributor base for CPU */
volatile uint32_t *dreamlte_gic_get_redist_base(int cpu);

/* Check if GIC is initialized */
bool dreamlte_gic_is_initialized(void);

__END_DECLS

#endif /* _SYS_DREAMLTE_GIC_H_ */