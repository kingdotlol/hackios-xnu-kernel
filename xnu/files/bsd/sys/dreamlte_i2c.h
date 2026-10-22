/*
 * Samsung Exynos 8895 I2C controller driver header.
 */

#ifndef _SYS_DREAMLTE_I2C_H_
#define _SYS_DREAMLTE_I2C_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

/* Initialize an I2C channel */
int dreamlte_i2c_init_channel(int channel, int speed_khz);

/* I2C master write */
int dreamlte_i2c_write(int channel, uint8_t addr, const uint8_t *data, int len, bool stop);

/* I2C master read */
int dreamlte_i2c_read(int channel, uint8_t addr, uint8_t *data, int len);

/* I2C write then read (combined transaction) */
int dreamlte_i2c_write_read(int channel, uint8_t addr,
                            const uint8_t *write_data, int write_len,
                            uint8_t *read_data, int read_len);

/* Check if I2C channel is initialized */
bool dreamlte_i2c_is_initialized(int channel);

__END_DECLS

#endif /* _SYS_DREAMLTE_I2C_H_ */