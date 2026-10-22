/*
 * Samsung Galaxy S8 (dreamlte) Touchscreen driver.
 * Supports Goodix GT9xx series touchscreen controllers over I2C.
 * Provides /dev/dreamlte-touch for touch input events.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/tty.h>
#include <machine/bus.h>
#include <arm/cpufunc.h>

/* Goodix GT9xx registers */
#define GT9XX_REG_CONFIG        0x8047
#define GT9XX_REG_COORD_ADDR    0x814E
#define GT9XX_REG_COORD_DATA    0x814F
#define GT9XX_REG_POINT_INFO    0x814E
#define GT9XX_REG_TOUCH_NUM     0x814E
#define GT9XX_REG_BUFFER_STATUS 0x814E
#define GT9XX_REG_FW_VERSION    0x8140
#define GT9XX_REG_VENDOR_ID     0x814A
#define GT9XX_REG_CHIP_ID       0x8140
#define GT9XX_CMD_RESET         0x0200
#define GT9XX_CMD_CALIBRATE     0x0201

/* Touch point structure (GT9xx format) */
struct gt9xx_touch_point {
    uint8_t track_id;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint8_t reserved[3];
} __attribute__((packed));

/* I2C addresses for Goodix (7-bit) */
#define GT9XX_I2C_ADDR_1        0x5D
#define GT9XX_I2C_ADDR_2        0x14

/* Exynos I2C controller registers */
#define EXYNOS_I2C_BASE         0x13860000
#define EXYNOS_I2C_SIZE         0x00040000
#define EXYNOS_I2C_CON          0x0000  /* Control */
#define EXYNOS_I2C_STAT         0x0004  /* Status */
#define EXYNOS_I2C_DS           0x0008  /* Data Shift */
#define EXYNOS_I2C_LC           0x000C  /* Loop Count */
#define EXYNOS_I2C_CNT          0x0010  /* Byte Count */
#define EXYNOS_I2C_PND          0x0014  /* Pending */
#define EXYNOS_I2C_PTF          0x0018  /* Pending Timeout */
#define EXYNOS_I2C_TX           0x001C  /* Transmit */
#define EXYNOS_I2C_RX           0x0020  /* Receive */
#define EXYNOS_I2C_TX_INT       0x0024  /* TX Interrupt */
#define EXYNOS_I2C_RX_INT       0x0028  /* RX Interrupt */
#define EXYNOS_I2C_SDA          0x002C  /* SDA Control */

/* I2C_CON bits */
#define I2C_CON_ACKEN           (1 << 7)
#define I2C_CON_TXRXEN          (1 << 6)
#define I2C_CON_MASTER          (1 << 5)
#define I2C_CON_INTEN           (1 << 4)
#define I2C_CON_IRQ             (1 << 3)
#define I2C_CON_CLKSEL(n)       ((n) & 0x7)

/* I2C_STAT bits */
#define I2C_STAT_ACK            (1 << 7)
#define I2C_STAT_RXACK          (1 << 6)
#define I2C_STAT_ARBLOST        (1 << 5)
#define I2C_STAT_BUSBUSY        (1 << 4)
#define I2C_STAT_SLAVE          (1 << 3)
#define I2C_STAT_IDLE           (1 << 2)
#define I2C_STAT_TXRX           (1 << 1)
#define I2C_STAT_LASTBIT        (1 << 0)

/* Touch event structure for /dev/dreamlte-touch */
struct dreamlte_touch_event {
    uint64_t timestamp;
    uint32_t type;        /* 0=down, 1=up, 2=move */
    uint32_t finger_id;
    int32_t x;
    int32_t y;
    int32_t pressure;
    int32_t width_major;
    int32_t width_minor;
};

#define DREAMLTE_TOUCH_MAX_POINTS 10
#define DREAMLTE_TOUCH_BUFFER_SIZE 256

static volatile uint32_t *dreamlte_i2c_base = NULL;
static uint8_t dreamlte_ts_i2c_addr = GT9XX_I2C_ADDR_1;
static int dreamlte_ts_width = 1440;
static int dreamlte_ts_height = 2960;
static bool dreamlte_ts_initialized = false;

/* Touch event buffer */
static struct dreamlte_touch_event dreamlte_touch_buffer[DREAMLTE_TOUCH_BUFFER_SIZE];
static int dreamlte_touch_head = 0;
static int dreamlte_touch_tail = 0;
static struct mtx dreamlte_touch_lock;

/*
 * I2C helper functions
 */
static int
i2c_wait_idle(void)
{
    for (int i = 0; i < 10000; i++) {
        if (dreamlte_i2c_base[EXYNOS_I2C_STAT / 4] & I2C_STAT_IDLE) {
            return 0;
        }
    }
    return -1;
}

static int
i2c_wait_tx_ready(void)
{
    for (int i = 0; i < 10000; i++) {
        uint32_t stat = dreamlte_i2c_base[EXYNOS_I2C_STAT / 4];
        if (stat & I2C_STAT_IDLE) {
            return 0;
        }
    }
    return -1;
}

static int
i2c_write_byte(uint8_t data)
{
    dreamlte_i2c_base[EXYNOS_I2C_DS / 4] = data;
    dreamlte_i2c_base[EXYNOS_I2C_CON / 4] |= I2C_CON_TXRXEN;
    return i2c_wait_tx_ready();
}

static int
i2c_read_byte(uint8_t *data, int last)
{
    dreamlte_i2c_base[EXYNOS_I2C_CON / 4] &= ~I2C_CON_TXRXEN;
    if (last) {
        dreamlte_i2c_base[EXYNOS_I2C_CON / 4] &= ~I2C_CON_ACKEN;
    } else {
        dreamlte_i2c_base[EXYNOS_I2C_CON / 4] |= I2C_CON_ACKEN;
    }
    dreamlte_i2c_base[EXYNOS_I2C_CON / 4] |= I2C_CON_TXRXEN;

    if (i2c_wait_tx_ready() < 0) {
        return -1;
    }
    *data = dreamlte_i2c_base[EXYNOS_I2C_DS / 4];
    return 0;
}

static int
i2c_start(uint8_t addr, int write)
{
    /* Set slave address */
    dreamlte_i2c_base[EXYNOS_I2C_DS / 4] = (addr << 1) | (write ? 0 : 1);

    /* Generate start condition */
    uint32_t con = dreamlte_i2c_base[EXYNOS_I2C_CON / 4];
    con |= I2C_CON_MASTER | I2C_CON_TXRXEN | I2C_CON_INTEN;
    if (!write) con |= I2C_CON_ACKEN;
    dreamlte_i2c_base[EXYNOS_I2C_CON / 4] = con;

    return i2c_wait_tx_ready();
}

static void
i2c_stop(void)
{
    uint32_t con = dreamlte_i2c_base[EXYNOS_I2C_CON / 4];
    con &= ~I2C_CON_MASTER;
    dreamlte_i2c_base[EXYNOS_I2C_CON / 4] = con;
}

/*
 * Read from Goodix register
 */
static int
gt9xx_read_reg(uint16_t reg, uint8_t *buf, int len)
{
    if (!dreamlte_ts_initialized) return -1;

    mtx_lock(&dreamlte_touch_lock);

    /* Write register address */
    if (i2c_start(dreamlte_ts_i2c_addr, 1) < 0) goto fail;
    if (i2c_write_byte(reg >> 8) < 0) goto fail;
    if (i2c_write_byte(reg & 0xFF) < 0) goto fail;

    /* Repeated start for read */
    if (i2c_start(dreamlte_ts_i2c_addr, 0) < 0) goto fail;

    for (int i = 0; i < len; i++) {
        if (i2c_read_byte(&buf[i], i == len - 1) < 0) goto fail;
    }

    i2c_stop();
    mtx_unlock(&dreamlte_touch_lock);
    return 0;

fail:
    i2c_stop();
    mtx_unlock(&dreamlte_touch_lock);
    return -1;
}

static int
gt9xx_write_reg(uint16_t reg, const uint8_t *buf, int len)
{
    if (!dreamlte_ts_initialized) return -1;

    mtx_lock(&dreamlte_touch_lock);

    if (i2c_start(dreamlte_ts_i2c_addr, 1) < 0) goto fail;
    if (i2c_write_byte(reg >> 8) < 0) goto fail;
    if (i2c_write_byte(reg & 0xFF) < 0) goto fail;

    for (int i = 0; i < len; i++) {
        if (i2c_write_byte(buf[i]) < 0) goto fail;
    }

    i2c_stop();
    mtx_unlock(&dreamlte_touch_lock);
    return 0;

fail:
    i2c_stop();
    mtx_unlock(&dreamlte_touch_lock);
    return -1;
}

/*
 * Read touch points from Goodix
 */
static int
gt9xx_read_touch_points(struct gt9xx_touch_point *points, int max_points, int *num_points)
{
    uint8_t buf[DREAMLTE_TOUCH_MAX_POINTS * sizeof(struct gt9xx_touch_point) + 2];

    /* Read touch data */
    if (gt9xx_read_reg(GT9XX_REG_COORD_ADDR, buf, sizeof(buf)) < 0) {
        return -1;
    }

    *num_points = buf[0] & 0x0F;
    if (*num_points > max_points) *num_points = max_points;

    for (int i = 0; i < *num_points; i++) {
        points[i] = *(struct gt9xx_touch_point *)&buf[1 + i * sizeof(struct gt9xx_touch_point)];
    }

    /* Clear buffer status */
    uint8_t clear = 0;
    gt9xx_write_reg(GT9XX_REG_BUFFER_STATUS, &clear, 1);

    return 0;
}

/*
 * Initialize the touchscreen
 */
int
dreamlte_ts_init(int width, int height)
{
    if (dreamlte_ts_initialized) return 0;

    dreamlte_ts_width = width;
    dreamlte_ts_height = height;

    mtx_init(&dreamlte_touch_lock, "dreamlte_ts", NULL, MTX_DEF);

    /* Map I2C controller */
    dreamlte_i2c_base = (volatile uint32_t *)(uintptr_t)EXYNOS_I2C_BASE;

    /* Configure I2C clock (assuming 100kHz @ 100MHz PCLK) */
    dreamlte_i2c_base[EXYNOS_I2C_CON / 4] = I2C_CON_CLKSEL(4) | I2C_CON_INTEN;
    dreamlte_i2c_base[EXYNOS_I2C_LC / 4] = 0;
    dreamlte_i2c_base[EXYNOS_I2C_CNT / 4] = 0;

    /* Try both I2C addresses */
    uint8_t chip_id[4];
    for (int addr_idx = 0; addr_idx < 2; addr_idx++) {
        dreamlte_ts_i2c_addr = (addr_idx == 0) ? GT9XX_I2C_ADDR_1 : GT9XX_I2C_ADDR_2;

        if (gt9xx_read_reg(GT9XX_REG_CHIP_ID, chip_id, 4) == 0) {
            printf("dreamlte_ts: Found Goodix at 0x%02x, chip ID: %c%c%c%c\n",
                   dreamlte_ts_i2c_addr, chip_id[0], chip_id[1], chip_id[2], chip_id[3]);
            break;
        }
    }

    if (dreamlte_ts_i2c_addr != GT9XX_I2C_ADDR_1 && dreamlte_ts_i2c_addr != GT9XX_I2C_ADDR_2) {
        printf("dreamlte_ts: No Goodix touchscreen found\n");
        mtx_destroy(&dreamlte_touch_lock);
        return -1;
    }

    /* Read firmware version */
    uint8_t fw_ver[4];
    gt9xx_read_reg(GT9XX_REG_FW_VERSION, fw_ver, 4);
    printf("dreamlte_ts: Firmware version %d.%d.%d.%d\n", fw_ver[0], fw_ver[1], fw_ver[2], fw_ver[3]);

    /* Reset the controller */
    uint8_t reset_cmd = 0x02;
    gt9xx_write_reg(GT9XX_CMD_RESET, &reset_cmd, 1);

    /* Wait for reset */
    for (volatile int i = 0; i < 100000; i++) {}

    dreamlte_ts_initialized = true;
    printf("dreamlte_ts: initialized (%dx%d)\n", width, height);

    return 0;
}

/*
 * Poll for touch events (called from interrupt or timer)
 */
void
dreamlte_ts_poll(void)
{
    if (!dreamlte_ts_initialized) return;

    struct gt9xx_touch_point points[DREAMLTE_TOUCH_MAX_POINTS];
    int num_points;

    if (gt9xx_read_touch_points(points, DREAMLTE_TOUCH_MAX_POINTS, &num_points) < 0) {
        return;
    }

    uint64_t timestamp = dreamlte_timer_get_time_ns();

    mtx_lock(&dreamlte_touch_lock);

    for (int i = 0; i < num_points; i++) {
        struct dreamlte_touch_event *evt = &dreamlte_touch_buffer[dreamlte_touch_head];

        evt->timestamp = timestamp;
        evt->type = 2; /* move - we don't track state changes in this simple impl */
        evt->finger_id = points[i].track_id;
        evt->x = points[i].x;
        evt->y = points[i].y;
        evt->pressure = points[i].width; /* approximate */
        evt->width_major = 10;
        evt->width_minor = 10;

        dreamlte_touch_head = (dreamlte_touch_head + 1) % DREAMLTE_TOUCH_BUFFER_SIZE;
        if (dreamlte_touch_head == dreamlte_touch_tail) {
            dreamlte_touch_tail = (dreamlte_touch_tail + 1) % DREAMLTE_TOUCH_BUFFER_SIZE;
        }
    }

    mtx_unlock(&dreamlte_touch_lock);
}

/*
 * Device file operations
 */
static int
dreamlte_ts_open(dev_t dev, int flag, int fmt, struct proc *p)
{
    return 0;
}

static int
dreamlte_ts_close(dev_t dev, int flag, int fmt, struct proc *p)
{
    return 0;
}

static int
dreamlte_ts_read(dev_t dev, struct uio *uio, int flags)
{
    if (!dreamlte_ts_initialized) return ENXIO;

    int error = 0;

    mtx_lock(&dreamlte_touch_lock);

    while (dreamlte_touch_head == dreamlte_touch_tail) {
        /* Buffer empty, wait for data */
        error = msleep(&dreamlte_touch_buffer, &dreamlte_touch_lock, PCATCH, "tsread", 0);
        if (error) {
            mtx_unlock(&dreamlte_touch_lock);
            return error;
        }
    }

    while (dreamlte_touch_head != dreamlte_touch_tail && uio->uio_resid >= sizeof(struct dreamlte_touch_event)) {
        error = uiomove(&dreamlte_touch_buffer[dreamlte_touch_tail],
                       sizeof(struct dreamlte_touch_event), uio);
        if (error) break;

        dreamlte_touch_tail = (dreamlte_touch_tail + 1) % DREAMLTE_TOUCH_BUFFER_SIZE;
    }

    mtx_unlock(&dreamlte_touch_lock);
    return error;
}

static int
dreamlte_ts_ioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
    return ENOTTY;
}

static int
dreamlte_ts_poll(dev_t dev, int events, struct proc *p)
{
    int revents = 0;

    mtx_lock(&dreamlte_touch_lock);

    if (events & (POLLIN | POLLRDNORM)) {
        if (dreamlte_touch_head != dreamlte_touch_tail) {
            revents |= POLLIN | POLLRDNORM;
        }
    }

    mtx_unlock(&dreamlte_touch_lock);
    return revents;
}

static struct cdevsw dreamlte_ts_cdevsw = {
    .d_version = D_VERSION,
    .d_open = dreamlte_ts_open,
    .d_close = dreamlte_ts_close,
    .d_read = dreamlte_ts_read,
    .d_ioctl = dreamlte_ts_ioctl,
    .d_poll = dreamlte_ts_poll,
    .d_name = "dreamlte-touch",
};

static struct cdev *dreamlte_ts_dev = NULL;

/*
 * Initialize the /dev/dreamlte-touch device
 */
void
dreamlte_ts_device_init(void)
{
    if (dreamlte_ts_dev) return;

    dreamlte_ts_dev = make_dev(&dreamlte_ts_cdevsw, 0, UID_ROOT, GID_WHEEL, 0666, "dreamlte-touch");
    if (dreamlte_ts_dev) {
        printf("dreamlte_ts: created /dev/dreamlte-touch\n");
    }
}

/*
 * Check if touchscreen is initialized
 */
bool
dreamlte_ts_is_initialized(void)
{
    return dreamlte_ts_initialized;
}