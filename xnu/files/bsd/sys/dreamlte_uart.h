/*
 * Samsung Exynos 8895 UART driver header.
 */

#ifndef _SYS_DREAMLTE_UART_H_
#define _SYS_DREAMLTE_UART_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

/* Initialize UART for early console output */
void dreamlte_uart_init(uint64_t uart_paddr);

/* Output a single character */
void dreamlte_uart_putc(char c);

/* Output a string */
void dreamlte_uart_puts(const char *str);

/* Read a character (blocking, returns -1 if none available) */
int dreamlte_uart_getc(void);

/* Check if UART is initialized */
bool dreamlte_uart_is_initialized(void);

__END_DECLS

#endif /* _SYS_DREAMLTE_UART_H_ */