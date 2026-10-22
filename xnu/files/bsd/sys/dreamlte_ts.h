/*
 * Samsung Galaxy S8 (dreamlte) Touchscreen driver header.
 */

#ifndef _SYS_DREAMLTE_TS_H_
#define _SYS_DREAMLTE_TS_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

/* Initialize touchscreen */
int dreamlte_ts_init(int width, int height);

/* Poll for touch events (call from timer/interrupt) */
void dreamlte_ts_poll(void);

/* Initialize /dev/dreamlte-touch device */
void dreamlte_ts_device_init(void);

/* Check if touchscreen is initialized */
bool dreamlte_ts_is_initialized(void);

__END_DECLS

#endif /* _SYS_DREAMLTE_TS_H_ */