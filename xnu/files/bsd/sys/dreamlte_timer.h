/*
 * Samsung Exynos 8895 Timer driver header.
 */

#ifndef _SYS_DREAMLTE_TIMER_H_
#define _SYS_DREAMLTE_TIMER_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

/* Initialize timer subsystem */
void dreamlte_timer_init(void);

/* Get current time in nanoseconds */
uint64_t dreamlte_timer_get_time_ns(void);

/* Get system counter value */
uint64_t dreamlte_timer_get_counter(void);

/* Get system counter frequency */
uint64_t dreamlte_timer_get_freq(void);

/* Set up one-shot timer interrupt */
void dreamlte_timer_set_oneshot(uint64_t delay_ns);

/* Cancel one-shot timer */
void dreamlte_timer_cancel_oneshot(void);

/* Initialize PWM timer for periodic interrupts */
void dreamlte_pwm_timer_init(int timer_num, uint32_t freq_hz);

/* Check if timer is initialized */
bool dreamlte_timer_is_initialized(void);

__END_DECLS

#endif /* _SYS_DREAMLTE_TIMER_H_ */