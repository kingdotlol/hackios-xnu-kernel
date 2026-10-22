/*
 * Samsung Exynos 8895 UART driver for early boot debugging.
 * Exynos UART is similar to PL011 but with Samsung-specific register layout.
 *
 * This provides early console output before the loader's log mechanism is fully set up.
 * The kernel primarily uses loader_log_putc() for output, but this UART driver
 * can be used for very early boot debugging (before loader is available).
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/tty.h>
#include <sys/cons.h>
#include <sys/consio.h>
#include <sys/kern_control.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <arm/cpufunc.h>
#include <arm/pmap.h>

/* Exynos 8895 UART register offsets (based on PL011 with Samsung extensions) */
#define EXYNOS_UART_DR          0x000   /* Data Register */
#define EXYNOS_UART_RSR         0x004   /* Receive Status Register / Error Clear Register */
#define EXYNOS_UART_FR          0x018   /* Flag Register */
#define EXYNOS_UART_ILPR        0x020   /* IrDA Low-Power Register */
#define EXYNOS_UART_IBRD        0x024   /* Integer Baud Rate Divisor */
#define EXYNOS_UART_FBRD        0x028   /* Fractional Baud Rate Divisor */
#define EXYNOS_UART_LCR_H       0x02C   /* Line Control Register */
#define EXYNOS_UART_CR          0x030   /* Control Register */
#define EXYNOS_UART_IFLS        0x034   /* Interrupt FIFO Level Select Register */
#define EXYNOS_UART_IMSC        0x038   /* Interrupt Mask Set/Clear Register */
#define EXYNOS_UART_RIS         0x03C   /* Raw Interrupt Status Register */
#define EXYNOS_UART_MIS         0x040   /* Masked Interrupt Status Register */
#define EXYNOS_UART_ICR         0x044   /* Interrupt Clear Register */
#define EXYNOS_UART_DMACR       0x048   /* DMA Control Register */
#define EXYNOS_UART_TCR         0x080   /* Transmission Control Register (Samsung extension) */

/* Flag Register bits */
#define EXYNOS_UART_FR_TXFE     (1 << 7)  /* Transmit FIFO Empty */
#define EXYNOS_UART_FR_RXFF     (1 << 6)  /* Receive FIFO Full */
#define EXYNOS_UART_FR_TXFF     (1 << 5)  /* Transmit FIFO Full */
#define EXYNOS_UART_FR_RXFE     (1 << 4)  /* Receive FIFO Empty */
#define EXYNOS_UART_FR_BUSY     (1 << 3)  /* UART Busy */
#define EXYNOS_UART_FR_DCD      (1 << 2)  /* Data Carrier Detect */
#define EXYNOS_UART_FR_DSR      (1 << 1)  /* Data Set Ready */
#define EXYNOS_UART_FR_CTS      (1 << 0)  /* Clear To Send */

/* Control Register bits */
#define EXYNOS_UART_CR_CTSEN    (1 << 15) /* CTS Hardware Flow Control Enable */
#define EXYNOS_UART_CR_RTSEN    (1 << 14) /* RTS Hardware Flow Control Enable */
#define EXYNOS_UART_CR_OUT2     (1 << 13) /* Out2 complement */
#define EXYNOS_UART_CR_OUT1     (1 << 12) /* Out1 complement */
#define EXYNOS_UART_CR_RTS      (1 << 11) /* Request To Send */
#define EXYNOS_UART_CR_DTR      (1 << 10) /* Data Terminal Ready */
#define EXYNOS_UART_CR_RXE      (1 << 9)  /* Receive Enable */
#define EXYNOS_UART_CR_TXE      (1 << 8)  /* Transmit Enable */
#define EXYNOS_UART_CR_LBE      (1 << 7)  /* Loop Back Enable */
#define EXYNOS_UART_CR_UARTEN   (1 << 0)  /* UART Enable */

/* Line Control Register bits */
#define EXYNOS_UART_LCR_H_SPS   (1 << 7)  /* Stick Parity Select */
#define EXYNOS_UART_LCR_H_WLEN_8 (3 << 5) /* 8-bit word length */
#define EXYNOS_UART_LCR_H_WLEN_7 (2 << 5) /* 7-bit word length */
#define EXYNOS_UART_LCR_H_WLEN_6 (1 << 5) /* 6-bit word length */
#define EXYNOS_UART_LCR_H_WLEN_5 (0 << 5) /* 5-bit word length */
#define EXYNOS_UART_LCR_H_FEN   (1 << 4)  /* Enable FIFOs */
#define EXYNOS_UART_LCR_H_STP2  (1 << 3)  /* Two Stop Bits */
#define EXYNOS_UART_LCR_H_EPS   (1 << 2)  /* Even Parity Select */
#define EXYNOS_UART_LCR_H_PEN   (1 << 1)  /* Parity Enable */
#define EXYNOS_UART_LCR_H_BRK   (1 << 0)  /* Send Break */

/* Samsung-specific UART clock/baud rate registers */
#define EXYNOS_UART_CLK_DIV     0x100   /* Clock divider (Samsung extension) */
#define EXYNOS_UART_FLOW_CTRL   0x104   /* Flow control (Samsung extension) */

/* Default UART base addresses for Exynos 8895 */
#define EXYNOS_UART0_BASE       0x13800000
#define EXYNOS_UART1_BASE       0x13810000
#define EXYNOS_UART2_BASE       0x13820000
#define EXYNOS_UART3_BASE       0x13830000

/* UART clock frequency (typically 100MHz or derived from system clock) */
#define EXYNOS_UART_CLK_FREQ    100000000
#define EXYNOS_UART_BAUD_RATE   115200

static volatile uint32_t *dreamlte_uart_base = NULL;
static bool dreamlte_uart_initialized = false;

/*
 * Initialize the Exynos UART for early console output.
 * Called very early during boot, before VM is fully set up.
 */
void
dreamlte_uart_init(uint64_t uart_paddr)
{
    if (dreamlte_uart_initialized) {
        return;
    }

    /* Map UART registers - assume 1:1 mapping for early boot */
    dreamlte_uart_base = (volatile uint32_t *)(uintptr_t)uart_paddr;

    /* Disable UART */
    dreamlte_uart_base[EXYNOS_UART_CR / 4] = 0;

    /* Clear all interrupts */
    dreamlte_uart_base[EXYNOS_UART_ICR / 4] = 0xFFFFFFFF;

    /* Set baud rate: IBRD = UART_CLK / (16 * BAUD_RATE) */
    uint32_t ibrd = EXYNOS_UART_CLK_FREQ / (16 * EXYNOS_UART_BAUD_RATE);
    uint32_t fbrd = ((EXYNOS_UART_CLK_FREQ % (16 * EXYNOS_UART_BAUD_RATE)) * 64 + (16 * EXYNOS_UART_BAUD_RATE) / 2) / (16 * EXYNOS_UART_BAUD_RATE);

    dreamlte_uart_base[EXYNOS_UART_IBRD / 4] = ibrd;
    dreamlte_uart_base[EXYNOS_UART_FBRD / 4] = fbrd;

    /* Line control: 8 bits, no parity, 1 stop bit, FIFOs enabled */
    dreamlte_uart_base[EXYNOS_UART_LCR_H / 4] = EXYNOS_UART_LCR_H_WLEN_8 | EXYNOS_UART_LCR_H_FEN;

    /* Enable UART, TX, RX */
    dreamlte_uart_base[EXYNOS_UART_CR / 4] = EXYNOS_UART_CR_UARTEN | EXYNOS_UART_CR_TXE | EXYNOS_UART_CR_RXE;

    dreamlte_uart_initialized = true;
}

/*
 * Output a single character to the UART.
 * Polls until there's space in the TX FIFO.
 */
void
dreamlte_uart_putc(char c)
{
    if (!dreamlte_uart_initialized || dreamlte_uart_base == NULL) {
        return;
    }

    /* Wait for TX FIFO not full */
    while (dreamlte_uart_base[EXYNOS_UART_FR / 4] & EXYNOS_UART_FR_TXFF) {
        /* Spin */
    }

    /* Write character */
    dreamlte_uart_base[EXYNOS_UART_DR / 4] = (uint32_t)c;

    /* If newline, also send carriage return */
    if (c == '\n') {
        while (dreamlte_uart_base[EXYNOS_UART_FR / 4] & EXYNOS_UART_FR_TXFF) {
            /* Spin */
        }
        dreamlte_uart_base[EXYNOS_UART_DR / 4] = '\r';
    }
}

/*
 * Output a string to the UART.
 */
void
dreamlte_uart_puts(const char *str)
{
    while (*str) {
        dreamlte_uart_putc(*str++);
    }
}

/*
 * Read a character from the UART (blocking).
 * Returns -1 if no character available.
 */
int
dreamlte_uart_getc(void)
{
    if (!dreamlte_uart_initialized || dreamlte_uart_base == NULL) {
        return -1;
    }

    /* Check if data available */
    if (dreamlte_uart_base[EXYNOS_UART_FR / 4] & EXYNOS_UART_FR_RXFE) {
        return -1;
    }

    return (int)(dreamlte_uart_base[EXYNOS_UART_DR / 4] & 0xFF);
}

/*
 * Check if UART is initialized.
 */
bool
dreamlte_uart_is_initialized(void)
{
    return dreamlte_uart_initialized;
}