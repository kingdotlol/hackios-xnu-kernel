/*
 * Samsung Exynos 8895 Timer driver.
 * Provides access to the generic ARMv8 system counter (CNTVCT_EL0) and
 * Exynos-specific timers for kernel timekeeping.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/cpufunc.h>
#include <arm/cpufunc.h>
#include <arm/pmap.h>

/* ARM Generic Timer registers (system counter) */
#define CNTVCT_EL0        /* System counter - read via MRS */
#define CNTFRQ_EL0        /* Counter frequency - read via MRS */
#define CNTP_CTL_EL0      /* Physical timer control */
#define CNTP_CVAL_EL0     /* Physical timer compare value */
#define CNTP_TVAL_EL0     /* Physical timer value */

/* Exynos-specific timer registers (PWM timers) */
#define EXYNOS_TIMER_BASE     0x13940000
#define EXYNOS_TIMER_SIZE     0x00040000

#define EXYNOS_TIMER_TCFG0    0x0000  /* Timer configuration 0 */
#define EXYNOS_TIMER_TCFG1    0x0004  /* Timer configuration 1 */
#define EXYNOS_TIMER_TCON     0x0008  /* Timer control */
#define EXYNOS_TIMER_TCNTB0   0x000C  /* Timer 0 count buffer */
#define EXYNOS_TIMER_TCMPB0   0x0010  /* Timer 0 compare buffer */
#define EXYNOS_TIMER_TCNTO0   0x0014  /* Timer 0 count observation */
#define EXYNOS_TIMER_TCNTB1   0x0018  /* Timer 1 count buffer */
#define EXYNOS_TIMER_TCMPB1   0x001C  /* Timer 1 compare buffer */
#define EXYNOS_TIMER_TCNTO1   0x0020  /* Timer 1 count observation */
#define EXYNOS_TIMER_TCNTB2   0x0024  /* Timer 2 count buffer */
#define EXYNOS_TIMER_TCMPB2   0x0028  /* Timer 2 compare buffer */
#define EXYNOS_TIMER_TCNTO2   0x002C  /* Timer 2 count observation */
#define EXYNOS_TIMER_TCNTB3   0x0030  /* Timer 3 count buffer */
#define EXYNOS_TIMER_TCMPB3   0x0034  /* Timer 3 compare buffer */
#define EXYNOS_TIMER_TCNTO3   0x0038  /* Timer 3 count observation */
#define EXYNOS_TIMER_TCNTB4   0x003C  /* Timer 4 count buffer */
#define EXYNOS_TIMER_TCMPB4   0x0040  /* Timer 4 compare buffer */
#define EXYNOS_TIMER_TCNTO4   0x0044  /* Timer 4 count observation */

/* TCFG0 bits */
#define EXYNOS_TCFG0_PRESCALER0(n)  ((n) & 0xFF)       /* Prescaler 0 (timer 0/1) */
#define EXYNOS_TCFG0_PRESCALER1(n)  (((n) >> 8) & 0xFF) /* Prescaler 1 (timer 2/3/4) */

/* TCFG1 bits */
#define EXYNOS_TCFG1_MUX0(n)        ((n) & 0xF)        /* Mux for timer 0 */
#define EXYNOS_TCFG1_MUX1(n)        (((n) >> 4) & 0xF) /* Mux for timer 1 */
#define EXYNOS_TCFG1_MUX2(n)        (((n) >> 8) & 0xF) /* Mux for timer 2 */
#define EXYNOS_TCFG1_MUX3(n)        (((n) >> 12) & 0xF) /* Mux for timer 3 */
#define EXYNOS_TCFG1_MUX4(n)        (((n) >> 16) & 0xF) /* Mux for timer 4 */

/* TCON bits */
#define EXYNOS_TCON_T0_START        (1 << 0)   /* Timer 0 start */
#define EXYNOS_TCON_T0_MANUAL       (1 << 1)   /* Timer 0 manual update */
#define EXYNOS_TCON_T0_INVERT       (1 << 2)   /* Timer 0 output inverter */
#define EXYNOS_TCON_T0_AUTO_RELOAD  (1 << 3)   /* Timer 0 auto reload */
#define EXYNOS_TCON_T1_START        (1 << 8)   /* Timer 1 start */
#define EXYNOS_TCON_T1_MANUAL       (1 << 9)   /* Timer 1 manual update */
#define EXYNOS_TCON_T1_INVERT       (1 << 10)  /* Timer 1 output inverter */
#define EXYNOS_TCON_T1_AUTO_RELOAD  (1 << 11)  /* Timer 1 auto reload */
#define EXYNOS_TCON_T2_START        (1 << 12)  /* Timer 2 start */
#define EXYNOS_TCON_T2_MANUAL       (1 << 13)  /* Timer 2 manual update */
#define EXYNOS_TCON_T2_INVERT       (1 << 14)  /* Timer 2 output inverter */
#define EXYNOS_TCON_T2_AUTO_RELOAD  (1 << 15)  /* Timer 2 auto reload */
#define EXYNOS_TCON_T3_START        (1 << 16)  /* Timer 3 start */
#define EXYNOS_TCON_T3_MANUAL       (1 << 17)  /* Timer 3 manual update */
#define EXYNOS_TCON_T3_INVERT       (1 << 18)  /* Timer 3 output inverter */
#define EXYNOS_TCON_T3_AUTO_RELOAD  (1 << 19)  /* Timer 3 auto reload */
#define EXYNOS_TCON_T4_START        (1 << 20)  /* Timer 4 start */
#define EXYNOS_TCON_T4_MANUAL       (1 << 21)  /* Timer 4 manual update */
#define EXYNOS_TCON_T4_INVERT       (1 << 22)  /* Timer 4 output inverter */
#define EXYNOS_TCON_T4_AUTO_RELOAD  (1 << 23)  /* Timer 4 auto reload */

static volatile uint32_t *dreamlte_timer_base = NULL;
static uint64_t dreamlte_timer_freq = 0;
static bool dreamlte_timer_initialized = false;

/*
 * Initialize the timer subsystem.
 * Uses the ARM generic system counter (CNTVCT_EL0) as primary time source.
 */
void
dreamlte_timer_init(void)
{
    if (dreamlte_timer_initialized) {
        return;
    }

    /* Read the system counter frequency */
    uint64_t freq;
    __asm__ volatile ("mrs %0, cntfrq_el0" : "=r" (freq));
    dreamlte_timer_freq = freq;

    /* Map Exynos timer registers if needed */
    dreamlte_timer_base = (volatile uint32_t *)(uintptr_t)EXYNOS_TIMER_BASE;

    printf("dreamlte_timer: system counter frequency %llu Hz\n", dreamlte_timer_freq);

    dreamlte_timer_initialized = true;
}

/*
 * Get current time in nanoseconds using system counter.
 */
uint64_t
dreamlte_timer_get_time_ns(void)
{
    uint64_t ctr;
    __asm__ volatile ("mrs %0, cntvct_el0" : "=r" (ctr));

    if (dreamlte_timer_freq == 0) {
        return ctr;  /* Fallback: return raw counter */
    }

    /* Convert to nanoseconds: ns = (ctr * 1e9) / freq */
    return (ctr * 1000000000ULL) / dreamlte_timer_freq;
}

/*
 * Get current system counter value.
 */
uint64_t
dreamlte_timer_get_counter(void)
{
    uint64_t ctr;
    __asm__ volatile ("mrs %0, cntvct_el0" : "=r" (ctr));
    return ctr;
}

/*
 * Get system counter frequency.
 */
uint64_t
dreamlte_timer_get_freq(void)
{
    return dreamlte_timer_freq;
}

/*
 * Set up a one-shot timer interrupt using the ARM physical timer.
 * delay_ns: delay in nanoseconds
 */
void
dreamlte_timer_set_oneshot(uint64_t delay_ns)
{
    if (dreamlte_timer_freq == 0) {
        return;
    }

    uint64_t ctr;
    __asm__ volatile ("mrs %0, cntvct_el0" : "=r" (ctr));

    uint64_t ticks = (delay_ns * dreamlte_timer_freq) / 1000000000ULL;
    uint64_t cval = ctr + ticks;

    __asm__ volatile ("msr cntp_cval_el0, %0" :: "r" (cval));
    __asm__ volatile ("msr cntp_ctl_el0, %0" :: "r" (1ULL));  /* Enable timer */
}

/*
 * Cancel the one-shot timer.
 */
void
dreamlte_timer_cancel_oneshot(void)
{
    __asm__ volatile ("msr cntp_ctl_el0, %0" :: "r" (0ULL));  /* Disable timer */
}

/*
 * Initialize an Exynos PWM timer for periodic interrupts.
 * timer_num: 0-4
 * freq_hz: desired frequency
 */
void
dreamlte_pwm_timer_init(int timer_num, uint32_t freq_hz)
{
    if (!dreamlte_timer_initialized || dreamlte_timer_base == NULL) {
        return;
    }

    if (timer_num < 0 || timer_num > 4) {
        return;
    }

    /* PCLK is typically 100MHz on Exynos */
    const uint32_t pclk = 100000000;

    /* Configure prescaler and mux */
    uint32_t tcfg0 = dreamlte_timer_base[EXYNOS_TIMER_TCFG0 / 4];
    uint32_t tcfg1 = dreamlte_timer_base[EXYNOS_TIMER_TCFG1 / 4];

    /* Set prescaler to divide PCLK to 1MHz (prescaler = 99) */
    if (timer_num <= 1) {
        tcfg0 = (tcfg0 & ~0xFF) | 99;
    } else {
        tcfg0 = (tcfg0 & ~0xFF00) | (99 << 8);
    }

    /* Set mux to 1/16 (for 1MHz -> 62.5KHz base) */
    uint32_t mux_val = 3;  /* 1/16 */
    switch (timer_num) {
        case 0: tcfg1 = (tcfg1 & ~0xF) | mux_val; break;
        case 1: tcfg1 = (tcfg1 & ~0xF0) | (mux_val << 4); break;
        case 2: tcfg1 = (tcfg1 & ~0xF00) | (mux_val << 8); break;
        case 3: tcfg1 = (tcfg1 & ~0xF000) | (mux_val << 12); break;
        case 4: tcfg1 = (tcfg1 & ~0xF0000) | (mux_val << 16); break;
    }

    dreamlte_timer_base[EXYNOS_TIMER_TCFG0 / 4] = tcfg0;
    dreamlte_timer_base[EXYNOS_TIMER_TCFG1 / 4] = tcfg1;

    /* Calculate count value for desired frequency */
    uint32_t base_freq = pclk / 100 / 16;  /* 62.5KHz after prescaler and mux */
    uint32_t count = base_freq / freq_hz;

    /* Set count buffer */
    switch (timer_num) {
        case 0: dreamlte_timer_base[EXYNOS_TIMER_TCNTB0 / 4] = count; break;
        case 1: dreamlte_timer_base[EXYNOS_TIMER_TCNTB1 / 4] = count; break;
        case 2: dreamlte_timer_base[EXYNOS_TIMER_TCNTB2 / 4] = count; break;
        case 3: dreamlte_timer_base[EXYNOS_TIMER_TCNTB3 / 4] = count; break;
        case 4: dreamlte_timer_base[EXYNOS_TIMER_TCNTB4 / 4] = count; break;
    }

    /* Enable auto-reload and start */
    uint32_t tcon = dreamlte_timer_base[EXYNOS_TIMER_TCON / 4];
    uint32_t start_bit, auto_bit, manual_bit;
    switch (timer_num) {
        case 0: start_bit = EXYNOS_TCON_T0_START; auto_bit = EXYNOS_TCON_T0_AUTO_RELOAD; manual_bit = EXYNOS_TCON_T0_MANUAL; break;
        case 1: start_bit = EXYNOS_TCON_T1_START; auto_bit = EXYNOS_TCON_T1_AUTO_RELOAD; manual_bit = EXYNOS_TCON_T1_MANUAL; break;
        case 2: start_bit = EXYNOS_TCON_T2_START; auto_bit = EXYNOS_TCON_T2_AUTO_RELOAD; manual_bit = EXYNOS_TCON_T2_MANUAL; break;
        case 3: start_bit = EXYNOS_TCON_T3_START; auto_bit = EXYNOS_TCON_T3_AUTO_RELOAD; manual_bit = EXYNOS_TCON_T3_MANUAL; break;
        case 4: start_bit = EXYNOS_TCON_T4_START; auto_bit = EXYNOS_TCON_T4_AUTO_RELOAD; manual_bit = EXYNOS_TCON_T4_MANUAL; break;
    }

    /* Manual update first */
    tcon |= manual_bit;
    dreamlte_timer_base[EXYNOS_TIMER_TCON / 4] = tcon;

    /* Then enable auto-reload and start */
    tcon &= ~manual_bit;
    tcon |= auto_bit | start_bit;
    dreamlte_timer_base[EXYNOS_TIMER_TCON / 4] = tcon;
}

/*
 * Check if timer is initialized.
 */
bool
dreamlte_timer_is_initialized(void)
{
    return dreamlte_timer_initialized;
}