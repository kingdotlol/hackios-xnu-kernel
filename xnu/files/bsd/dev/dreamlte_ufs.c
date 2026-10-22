/*
 * Samsung Exynos 8895 UFS (Universal Flash Storage) host controller driver.
 * Based on the JEDEC UFS 2.1 specification and Exynos UFS host controller.
 * This is a minimal driver for early boot - reads the root filesystem.
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
#include <machine/bus.h>
#include <arm/cpufunc.h>

/* Exynos UFS Host Controller register offsets */
#define EXYNOS_UFS_BASE         0x11C00000
#define EXYNOS_UFS_SIZE         0x00200000

/* UFS Host Controller Registers (per JEDEC UFSHCI) */
#define UFS_HC_CAP              0x0000  /* Controller Capabilities */
#define UFS_HC_VER              0x0008  /* Version */
#define UFS_HC_HCS              0x0030  /* Host Controller Status */
#define UFS_HC_HCE              0x0034  /* Host Controller Enable */
#define UFS_HC_UTRLBA           0x0050  /* UTP Transfer Request List Base Address */
#define UFS_HC_UTRLBAU          0x0054  /* UTP Transfer Request List Base Address Upper */
#define UFS_HC_UTRLDBR          0x0058  /* UTP Transfer Request List Door Bell Register */
#define UFS_HC_UTRLCLR          0x005C  /* UTP Transfer Request List Clear Register */
#define UFS_HC_UTRLRSR          0x0060  /* UTP Transfer Request List Run Stop Register */
#define UFS_HC_UTRLCNR          0x0064  /* UTP Transfer Request List Completion Notification Register */
#define UFS_HC_UTMRLBA          0x0070  /* UTP Task Management Request List Base Address */
#define UFS_HC_UTMRLBAU         0x0074  /* UTP Task Management Request List Base Address Upper */
#define UFS_HC_UTMRLDBR         0x0078  /* UTP Task Management Request List Door Bell Register */
#define UFS_HC_UTMRLCLR         0x007C  /* UTP Task Management Request List Clear Register */
#define UFS_HC_UTMRLRSR         0x0080  /* UTP Task Management Request List Run Stop Register */
#define UFS_HC_UTMRLCNR         0x0084  /* UTP Task Management Request List Completion Notification Register */

/* UTP Transfer Request Descriptor (TRD) */
#define UFS_UTR_DESC_SIZE       0x40
#define UFS_UTM_DESC_SIZE       0x30
#define UFS_MAX_UTRS            32

/* UTRD Command Types */
#define UFS_UTR_CMD_SCSI        0x00
#define UFS_UTR_CMD_UFS         0x01
#define UFS_UTR_CMD_DEV_MGMT    0x02
#define UFS_UTR_CMD_REQ_SENSE   0x03

/* UTRD Flags */
#define UFS_UTR_DIR_IN          (1 << 0)
#define UFS_UTR_DIR_OUT         (1 << 1)
#define UFS_UTR_CDB_SIZE(n)     ((n) << 2)

/* UIC Commands */
#define UIC_CMD_DME_GET         0x01
#define UIC_CMD_DME_SET         0x02
#define UIC_CMD_DME_PEER_GET    0x03
#define UIC_CMD_DME_PEER_SET    0x04
#define UIC_CMD_DME_ENDPOINT_RESET 0x10
#define UIC_CMD_DME_LINK_STARTUP 0x11
#define UIC_CMD_DME_HIBER_ENTER 0x12
#define UIC_CMD_DME_HIBER_EXIT  0x13
#define UIC_CMD_DME_TEST_MODE   0x14

/* UIC Attributes (PA_LOCAL / PA_PEER) */
#define UIC_ATTR_PWR_TX         0x1560
#define UIC_ATTR_PWR_RX         0x1580
#define UIC_ATTR_HS_RATE        0x15A0
#define UIC_ATTR_HS_SERIES      0x15A1
#define UIC_ATTR_PWM_TX         0x15C0
#define UIC_ATTR_PWM_RX         0x15E0

static volatile uint32_t *dreamlte_ufs_base = NULL;
static uint64_t dreamlte_utrl_base = 0;
static uint64_t dreamlte_utmrl_base = 0;
static uint64_t dreamlte_ucd_base = 0;
static uint64_t dreamlte_ucd_phys = 0;
static bool dreamlte_ufs_initialized = false;

/* Simple memory allocator for UFS structures */
static uint8_t ufs_mem_pool[64 * 1024] __attribute__((aligned(4096)));
static size_t ufs_mem_pool_used = 0;

static void *
ufs_alloc_aligned(size_t size, size_t align)
{
    size_t offset = (ufs_mem_pool_used + align - 1) & ~(align - 1);
    if (offset + size > sizeof(ufs_mem_pool)) {
        return NULL;
    }
    void *ptr = &ufs_mem_pool[offset];
    ufs_mem_pool_used = offset + size;
    memset(ptr, 0, size);
    return ptr;
}

/*
 * Initialize the Exynos UFS host controller.
 */
int
dreamlte_ufs_init(void)
{
    if (dreamlte_ufs_initialized) {
        return 0;
    }

    dreamlte_ufs_base = (volatile uint32_t *)(uintptr_t)EXYNOS_UFS_BASE;

    printf("dreamlte_ufs: controller at 0x%p\n", dreamlte_ufs_base);

    /* Read controller version */
    uint32_t ver = dreamlte_ufs_base[UFS_HC_VER / 4];
    printf("dreamlte_ufs: version 0x%08x\n", ver);

    /* Read capabilities */
    uint32_t cap = dreamlte_ufs_base[UFS_HC_CAP / 4];
    printf("dreamlte_ufs: capabilities 0x%08x\n", cap);

    /* Allocate UTRD list (32 entries, 64 bytes each, 128-byte aligned) */
    dreamlte_utrl_base = (uint64_t)(uintptr_t)ufs_alloc_aligned(UFS_MAX_UTRS * UFS_UTR_DESC_SIZE, 128);
    if (!dreamlte_utrl_base) {
        printf("dreamlte_ufs: failed to allocate UTRD list\n");
        return -1;
    }

    /* Allocate UTMRD list (8 entries, 48 bytes each) */
    dreamlte_utmrl_base = (uint64_t)(uintptr_t)ufs_alloc_aligned(8 * UFS_UTM_DESC_SIZE, 128);
    if (!dreamlte_utmrl_base) {
        printf("dreamlte_ufs: failed to allocate UTMRD list\n");
        return -1;
    }

    /* Allocate UCD (command descriptor) area */
    dreamlte_ucd_base = (uint64_t)(uintptr_t)ufs_alloc_aligned(UFS_MAX_UTRS * 0x1000, 4096);
    dreamlte_ucd_phys = dreamlte_ucd_base; /* Assume 1:1 mapping for now */
    if (!dreamlte_ucd_base) {
        printf("dreamlte_ufs: failed to allocate UCD area\n");
        return -1;
    }

    printf("dreamlte_ufs: UTRD=0x%llx, UTMRD=0x%llx, UCD=0x%llx\n",
           dreamlte_utrl_base, dreamlte_utmrl_base, dreamlte_ucd_base);

    /* Configure UTRD base address */
    dreamlte_ufs_base[UFS_HC_UTRLBA / 4] = (uint32_t)dreamlte_utrl_base;
    dreamlte_ufs_base[UFS_HC_UTRLBAU / 4] = (uint32_t)(dreamlte_utrl_base >> 32);

    /* Configure UTMRD base address */
    dreamlte_ufs_base[UFS_HC_UTMRLBA / 4] = (uint32_t)dreamlte_utmrl_base;
    dreamlte_ufs_base[UFS_HC_UTMRLBAU / 4] = (uint32_t)(dreamlte_utmrl_base >> 32);

    /* Enable host controller */
    dreamlte_ufs_base[UFS_HC_HCE / 4] = 1;

    /* Wait for controller to be ready */
    for (int i = 0; i < 10000; i++) {
        if (dreamlte_ufs_base[UFS_HC_HCS / 4] & 1) {
            break;
        }
        __builtin_arm_dsb();
    }

    /* Start UTRD list */
    dreamlte_ufs_base[UFS_HC_UTRLRSR / 4] = 1;
    dreamlte_ufs_base[UFS_HC_UTRLDBR / 4] = 1; /* Ring doorbell for slot 0 */

    dreamlte_ufs_initialized = true;
    printf("dreamlte_ufs: initialized\n");

    return 0;
}

/*
 * Send a UIC command.
 */
int
dreamlte_ufs_uic_cmd(uint8_t cmd, uint32_t attr, uint32_t *val)
{
    if (!dreamlte_ufs_initialized) {
        return -1;
    }

    /* Build UTMRD for UIC command */
    volatile uint32_t *utmrd = (volatile uint32_t *)(uintptr_t)dreamlte_utmrl_base;
    utmrd[0] = (cmd << 16) | attr;  /* Command + Attribute */
    utmrd[1] = 0;                    /* Reserved */
    utmrd[2] = 0;                    /* Argument */
    utmrd[3] = 0;                    /* Reserved */

    /* Ring task management doorbell */
    dreamlte_ufs_base[UFS_HC_UTMRLDBR / 4] = 1;

    /* Wait for completion */
    for (int i = 0; i < 10000; i++) {
        if (dreamlte_ufs_base[UFS_HC_UTMRLCNR / 4] & 1) {
            break;
        }
        __builtin_arm_dsb();
    }

    /* Clear completion */
    dreamlte_ufs_base[UFS_HC_UTMRLCNR / 4] = 1;

    if (val) {
        *val = utmrd[2];
    }

    return 0;
}

/*
 * Send a SCSI command via UTRD.
 */
int
dreamlte_ufs_scsi_cmd(uint8_t lun, const void *cdb, uint8_t cdb_len,
                      void *data, uint32_t data_len, uint8_t dir)
{
    if (!dreamlte_ufs_initialized) {
        return -1;
    }

    /* Build UTRD */
    volatile uint32_t *utrd = (volatile uint32_t *)(uintptr_t)dreamlte_utrl_base;
    volatile uint8_t *ucd = (volatile uint8_t *)(uintptr_t)dreamlte_ucd_base;

    /* Clear UTRD */
    for (int i = 0; i < UFS_UTR_DESC_SIZE / 4; i++) {
        utrd[i] = 0;
    }

    /* Command type and flags */
    utrd[0] = (UFS_UTR_CMD_SCSI << 24) | (dir ? UFS_UTR_DIR_IN : UFS_UTR_DIR_OUT) | (cdb_len << 2);
    utrd[1] = (lun << 24);  /* LUN */
    utrd[2] = data_len;      /* Data length */

    /* PRD (Physical Region Descriptor) table pointer */
    utrd[4] = (uint32_t)dreamlte_ucd_phys;
    utrd[5] = (uint32_t)(dreamlte_ucd_phys >> 32);

    /* Copy CDB to UCD */
    memcpy((void *)ucd, cdb, cdb_len);

    /* If data transfer, set up PRD in UCD */
    if (data && data_len > 0) {
        uint32_t *prd = (uint32_t *)(ucd + 0x40);
        prd[0] = (uint32_t)(uintptr_t)data;  /* Data buffer address (low) */
        prd[1] = (uint32_t)((uintptr_t)data >> 32);  /* Data buffer address (high) */
        prd[2] = data_len;  /* Data length */
        prd[3] = 0;         /* Reserved */
    }

    /* Ring doorbell */
    dreamlte_ufs_base[UFS_HC_UTRLDBR / 4] = 1;

    /* Wait for completion */
    for (int i = 0; i < 100000; i++) {
        if (dreamlte_ufs_base[UFS_HC_UTRLCNR / 4] & 1) {
            break;
        }
        __builtin_arm_dsb();
    }

    /* Clear completion */
    dreamlte_ufs_base[UFS_HC_UTRLCNR / 4] = 1;

    /* Check status */
    uint32_t status = utrd[0] >> 24;
    if (status != 0) {
        printf("dreamlte_ufs: SCSI command failed, status=%d\n", status);
        return -1;
    }

    /* Copy data back if read */
    if (dir == UFS_UTR_DIR_IN && data && data_len > 0) {
        memcpy(data, ucd + 0x40 + 16, data_len);
    }

    return 0;
}

/*
 * Read blocks from UFS device (LUN 0 - boot/rootfs).
 */
int
dreamlte_ufs_read_blocks(uint64_t lba, uint32_t count, void *buffer)
{
    uint8_t cdb[16] = {0};

    /* READ(16) command */
    cdb[0] = 0x88;
    cdb[1] = 0;  /* LUN in bits 7:5 */
    cdb[2] = (lba >> 56) & 0xFF;
    cdb[3] = (lba >> 48) & 0xFF;
    cdb[4] = (lba >> 40) & 0xFF;
    cdb[5] = (lba >> 32) & 0xFF;
    cdb[6] = (lba >> 24) & 0xFF;
    cdb[7] = (lba >> 16) & 0xFF;
    cdb[8] = (lba >> 8) & 0xFF;
    cdb[9] = lba & 0xFF;
    cdb[10] = (count >> 24) & 0xFF;
    cdb[11] = (count >> 16) & 0xFF;
    cdb[12] = (count >> 8) & 0xFF;
    cdb[13] = count & 0xFF;

    return dreamlte_ufs_scsi_cmd(0, cdb, 16, buffer, count * 4096, UFS_UTR_DIR_IN);
}

/*
 * Check if UFS is initialized.
 */
bool
dreamlte_ufs_is_initialized(void)
{
    return dreamlte_ufs_initialized;
}