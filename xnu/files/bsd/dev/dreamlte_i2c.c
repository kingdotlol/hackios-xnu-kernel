/*
 * Samsung Exynos 8895 I2C controller driver.
 * Provides a generic I2C bus interface for peripheral drivers
 * (touchscreen, sensors, PMIC, etc.).
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

/* Exynos I2C controller register offsets */
#define EXYNOS_I2C_BASE         0x13860000
#define EXYNOS_I2C_SIZE         0x00040000
#define EXYNOS_I2C_CHANNELS     12  /* Exynos 8895 has 12 I2C channels */

#define EXYNOS_I2C_CON          0x0000  /* Control */
#define EXYNOS_I2C_STAT         0x0004  /* Status */
#define EXYNOS_I2C_DS           0x0008  /* Data Shift */
#define EXYNOS_I2C_LC           0x000C  /* Loop Count (byte count) */
#define EXYNOS_I2C_CNT          0x0010  /* Byte Count */
#define EXYNOS_I2C_PND          0x0014  /* Pending */
#define EXYNOS_I2C_PTF          0x0018  /* Pending Timeout */
#define EXYNOS_I2C_TX           0x001C  /* Transmit */
#define EXYNOS_I2C_RX           0x0020  /* Receive */
#define EXYNOS_I2C_TX_INT       0x0024  /* TX Interrupt */
#define EXYNOS_I2C_RX_INT       0x0028  /* RX Interrupt */
#define EXYNOS_I2C_SDA          0x002C  /* SDA Control */

/* CON bits */
#define I2C_CON_ACKEN           (1 << 7)   /* Acknowledge enable */
#define I2C_CON_TXRXEN          (1 << 6)   /* Transmit/Receive enable */
#define I2C_CON_MASTER          (1 << 5)   /* Master mode */
#define I2C_CON_INTEN           (1 << 4)   /* Interrupt enable */
#define I2C_CON_IRQ             (1 << 3)   /* Interrupt pending */
#define I2C_CON_CLKSEL(n)       ((n) & 0x7) /* Clock select */

/* STAT bits */
#define I2C_STAT_ACK            (1 << 7)   /* Acknowledge received */
#define I2C_STAT_RXACK          (1 << 6)   /* Receive acknowledge */
#define I2C_STAT_ARBLOST        (1 << 5)   /* Arbitration lost */
#define I2C_STAT_BUSBUSY        (1 << 4)   /* Bus busy */
#define I2C_STAT_SLAVE          (1 << 3)   /* Slave mode */
#define I2C_STAT_IDLE           (1 << 2)   /* Bus idle */
#define I2C_STAT_TXRX           (1 << 1)   /* Transmit/Receive */
#define I2C_STAT_LASTBIT        (1 << 0)   /* Last bit */

/* Channel offset */
#define I2C_CH_OFFSET(ch)       ((ch) * 0x100)

static volatile uint32_t *dreamlte_i2c_bases[EXYNOS_I2C_CHANNELS];
static struct mtx dreamlte_i2c_locks[EXYNOS_I2C_CHANNELS];
static bool dreamlte_i2c_initialized[EXYNOS_I2C_CHANNELS];

/*
 * Initialize an I2C channel
 * channel: 0-11
 * speed_khz: I2C speed in kHz (100 or 400 typically)
 */
int
dreamlte_i2c_init_channel(int channel, int speed_khz)
{
    if (channel < 0 || channel >= EXYNOS_I2C_CHANNELS) {
        return -1;
    }

    if (dreamlte_i2c_initialized[channel]) {
        return 0;
    }

    mtx_init(&dreamlte_i2c_locks[channel], "dreamlte_i2c", NULL, MTX_DEF);

    /* Map I2C channel registers */
    dreamlte_i2c_bases[channel] = (volatile uint32_t *)(uintptr_t)(EXYNOS_I2C_BASE + I2C_CH_OFFSET(channel));

    volatile uint32_t *base = dreamlte_i2c_bases[channel];

    /* Reset controller */
    base[EXYNOS_I2C_CON / 4] = 0;

    /* Calculate clock divider for target speed */
    /* PCLK = 100MHz, I2C clock = PCLK / (clk_sel + 1) / 16 / (prescaler + 1) */
    /* For 100kHz: clk_sel=4, prescaler=99 -> 100MHz / 5 / 16 / 100 = 125kHz */
    /* For 400kHz: clk_sel=1, prescaler=99 -> 100MHz / 2 / 16 / 100 = 312.5kHz */
    uint32_t clk_sel = (speed_khz <= 100) ? 4 : 1;
    uint32_t prescaler = 99;

    base[EXYNOS_I2C_CON / 4] = I2C_CON_CLKSEL(clk_sel) | I2C_CON_INTEN;
    base[EXYNOS_I2C_LC / 4] = 0;
    base[EXYNOS_I2C_CNT / 4] = 0;

    dreamlte_i2c_initialized[channel] = true;
    printf("dreamlte_i2c: channel %d initialized at %p (%d kHz)\n",
           channel, base, speed_khz);

    return 0;
}

/*
 * Wait for I2C bus to be idle
 */
static int
i2c_wait_idle(volatile uint32_t *base)
{
    for (int i = 0; i < 100000; i++) {
        if (base[EXYNOS_I2C_STAT / 4] & I2C_STAT_IDLE) {
            return 0;
        }
    }
    return -1; /* Timeout */
}

/*
 * Wait for TX ready
 */
static int
i2c_wait_tx(volatile uint32_t *base)
{
    for (int i = 0; i < 100000; i++) {
        uint32_t stat = base[EXYNOS_I2C_STAT / 4];
        if (stat & I2C_STAT_IDLE) {
            return 0;
        }
    }
    return -1; /* Timeout */
}

/*
 * I2C master write
 * channel: I2C channel
 * addr: 7-bit slave address
 * data: data to write
 * len: length of data
 * stop: generate stop condition
 */
int
dreamlte_i2c_write(int channel, uint8_t addr, const uint8_t *data, int len, bool stop)
{
    if (channel < 0 || channel >= EXYNOS_I2C_CHANNELS || !dreamlte_i2c_initialized[channel]) {
        return -1;
    }

    mtx_lock(&dreamlte_i2c_locks[channel]);
    volatile uint32_t *base = dreamlte_i2c_bases[channel];

    /* Wait for bus idle */
    if (i2c_wait_idle(base) < 0) {
        mtx_unlock(&dreamlte_i2c_locks[channel]);
        return -1;
    }

    /* Set slave address (write) */
    base[EXYNOS_I2C_DS / 4] = (addr << 1) | 0;

    /* Start condition */
    base[EXYNOS_I2C_CON / 4] |= I2C_CON_MASTER | I2C_CON_TXRXEN | I2C_CON_INTEN;

    if (i2c_wait_tx(base) < 0) {
        mtx_unlock(&dreamlte_i2c_locks[channel]);
        return -1;
    }

    /* Check ACK */
    if (!(base[EXYNOS_I2C_STAT / 4] & I2C_STAT_ACK)) {
        base[EXYNOS_I2C_CON / 4] &= ~I2C_CON_MASTER;
        mtx_unlock(&dreamlte_i2c_locks[channel]);
        return -1; /* NACK */
    }

    /* Write data bytes */
    for (int i = 0; i < len; i++) {
        base[EXYNOS_I2C_DS / 4] = data[i];
        base[EXYNOS_I2C_CON / 4] |= I2C_CON_TXRXEN;

        if (i2c_wait_tx(base) < 0) {
            mtx_unlock(&dreamlte_i2c_locks[channel]);
            return -1;
        }

        if (!(base[EXYNOS_I2C_STAT / 4] & I2C_STAT_ACK)) {
            base[EXYNOS_I2C_CON / 4] &= ~I2C_CON_MASTER;
            mtx_unlock(&dreamlte_i2c_locks[channel]);
            return -1; /* NACK */
        }
    }

    if (stop) {
        base[EXYNOS_I2C_CON / 4] &= ~I2C_CON_MASTER;
    }

    mtx_unlock(&dreamlte_i2c_locks[channel]);
    return 0;
}

/*
 * I2C master read
 * channel: I2C channel
 * addr: 7-bit slave address
 * data: buffer to read into
 * len: number of bytes to read
 */
int
dreamlte_i2c_read(int channel, uint8_t addr, uint8_t *data, int len)
{
    if (channel < 0 || channel >= EXYNOS_I2C_CHANNELS || !dreamlte_i2c_initialized[channel]) {
        return -1;
    }

    mtx_lock(&dreamlte_i2c_locks[channel]);
    volatile uint32_t *base = dreamlte_i2c_bases[channel];

    /* Wait for bus idle */
    if (i2c_wait_idle(base) < 0) {
        mtx_unlock(&dreamlte_i2c_locks[channel]);
        return -1;
    }

    /* Set slave address (read) */
    base[EXYNOS_I2C_DS / 4] = (addr << 1) | 1;

    /* Start condition */
    base[EXYNOS_I2C_CON / 4] |= I2C_CON_MASTER | I2C_CON_TXRXEN | I2C_CON_INTEN | I2C_CON_ACKEN;

    if (i2c_wait_tx(base) < 0) {
        mtx_unlock(&dreamlte_i2c_locks[channel]);
        return -1;
    }

    if (!(base[EXYNOS_I2C_STAT / 4] & I2C_STAT_ACK)) {
        base[EXYNOS_I2C_CON / 4] &= ~I2C_CON_MASTER;
        mtx_unlock(&dreamlte_i2c_locks[channel]);
        return -1; /* NACK */
    }

    /* Read data bytes */
    for (int i = 0; i < len; i++) {
        /* Last byte: don't send ACK */
        if (i == len - 1) {
            base[EXYNOS_I2C_CON / 4] &= ~I2C_CON_ACKEN;
        } else {
            base[EXYNOS_I2C_CON / 4] |= I2C_CON_ACKEN;
        }

        base[EXYNOS_I2C_CON / 4] &= ~I2C_CON_TXRXEN; /* Receive mode */
        base[EXYNOS_I2C_CON / 4] |= I2C_CON_TXRXEN;  /* Trigger read */

        if (i2c_wait_tx(base) < 0) {
            mtx_unlock(&dreamlte_i2c_locks[channel]);
            return -1;
        }

        data[i] = base[EXYNOS_I2C_DS / 4] & 0xFF;
    }

    base[EXYNOS_I2C_CON / 4] &= ~I2C_CON_MASTER; /* Stop condition */

    mtx_unlock(&dreamlte_i2c_locks[channel]);
    return 0;
}

/*
 * I2C write then read (combined transaction)
 * channel: I2C channel
 * addr: 7-bit slave address
 * write_data: data to write
 * write_len: length of write data
 * read_data: buffer to read into
 * read_len: number of bytes to read
 */
int
dreamlte_i2c_write_read(int channel, uint8_t addr,
                        const uint8_t *write_data, int write_len,
                        uint8_t *read_data, int read_len)
{
    if (channel < 0 || channel >= EXYNOS_I2C_CHANNELS || !dreamlte_i2c_initialized[channel]) {
        return -1;
    }

    mtx_lock(&dreamlte_i2c_locks[channel]);
    volatile uint32_t *base = dreamlte_i2c_bases[channel];

    /* Wait for bus idle */
    if (i2c_wait_idle(base) < 0) {
        mtx_unlock(&dreamlte_i2c_locks[channel]);
        return -1;
    }

    /* Write phase */
    base[EXYNOS_I2C_DS / 4] = (addr << 1) | 0;
    base[EXYNOS_I2C_CON / 4] |= I2C_CON_MASTER | I2C_CON_TXRXEN | I2C_CON_INTEN;

    if (i2c_wait_tx(base) < 0) {
        mtx_unlock(&dreamlte_i2c_locks[channel]);
        return -1;
    }

    if (!(base[EXYNOS_I2C_STAT / 4] & I2C_STAT_ACK)) {
        base[EXYNOS_I2C_CON / 4] &= ~I2C_CON_MASTER;
        mtx_unlock(&dreamlte_i2c_locks[channel]);
        return -1;
    }

    for (int i = 0; i < write_len; i++) {
        base[EXYNOS_I2C_DS / 4] = write_data[i];
        base[EXYNOS_I2C_CON / 4] |= I2C_CON_TXRXEN;

        if (i2c_wait_tx(base) < 0) {
            mtx_unlock(&dreamlte_i2c_locks[channel]);
            return -1;
        }

        if (!(base[EXYNOS_I2C_STAT / 4] & I2C_STAT_ACK)) {
            base[EXYNOS_I2C_CON / 4] &= ~I2C_CON_MASTER;
            mtx_unlock(&dreamlte_i2c_locks[channel]);
            return -1;
        }
    }

    /* Repeated start for read phase */
    base[EXYNOS_I2C_DS / 4] = (addr << 1) | 1;
    base[EXYNOS_I2C_CON / 4] |= I2C_CON_TXRXEN;

    if (i2c_wait_tx(base) < 0) {
        mtx_unlock(&dreamlte_i2c_locks[channel]);
        return -1;
    }

    if (!(base[EXYNOS_I2C_STAT / 4] & I2C_STAT_ACK)) {
        base[EXYNOS_I2C_CON / 4] &= ~I2C_CON_MASTER;
        mtx_unlock(&dreamlte_i2c_locks[channel]);
        return -1;
    }

    /* Read phase */
    for (int i = 0; i < read_len; i++) {
        if (i == read_len - 1) {
            base[EXYNOS_I2C_CON / 4] &= ~I2C_CON_ACKEN;
        } else {
            base[EXYNOS_I2C_CON / 4] |= I2C_CON_ACKEN;
        }

        base[EXYNOS_I2C_CON / 4] &= ~I2C_CON_TXRXEN;
        base[EXYNOS_I2C_CON / 4] |= I2C_CON_TXRXEN;

        if (i2c_wait_tx(base) < 0) {
            mtx_unlock(&dreamlte_i2c_locks[channel]);
            return -1;
        }

        read_data[i] = base[EXYNOS_I2C_DS / 4] & 0xFF;
    }

    base[EXYNOS_I2C_CON / 4] &= ~I2C_CON_MASTER;

    mtx_unlock(&dreamlte_i2c_locks[channel]);
    return 0;
}

/*
 * Check if I2C channel is initialized
 */
bool
dreamlte_i2c_is_initialized(int channel)
{
    if (channel < 0 || channel >= EXYNOS_I2C_CHANNELS) {
        return false;
    }
    return dreamlte_i2c_initialized[channel];
}