/*
 * Dragoon Microkernel - Framebuffer Driver (ramfb via fw_cfg)
 *
 * Sets up a 640x480 XRGB8888 framebuffer using QEMU's ramfb device.
 * The ramfb device is configured through the fw_cfg DMA interface.
 */
#include "fb.h"
#include "mm.h"
#include "printf.h"

extern int memcmp(const void *s1, const void *s2, u64 n);

/* fw_cfg MMIO addresses on QEMU virt */
#define FW_CFG_BASE  0x09020000ULL
#define FW_CFG_DATA  (FW_CFG_BASE + 0x00)
#define FW_CFG_SEL   (FW_CFG_BASE + 0x08)
#define FW_CFG_DMA   (FW_CFG_BASE + 0x10)

/* fw_cfg selectors */
#define FW_CFG_SIGNATURE  0x0000
#define FW_CFG_FILE_DIR   0x0019

/* DRM format for XRGB8888 */
#define DRM_FORMAT_XRGB8888 0x34325258

/* Byte-swap helpers (ARM64 is little-endian, fw_cfg is big-endian) */
static inline u16 bswap16(u16 v)
{
    return (v >> 8) | (v << 8);
}

static inline u32 bswap32(u32 v)
{
    return ((v & 0xFF000000U) >> 24) | ((v & 0x00FF0000U) >> 8) |
           ((v & 0x0000FF00U) << 8)  | ((v & 0x000000FFU) << 24);
}

static inline u64 bswap64(u64 v)
{
    return ((u64)bswap32((u32)v) << 32) | (u64)bswap32((u32)(v >> 32));
}

/* fw_cfg legacy interface */
static void fw_cfg_select(u16 selector)
{
    *(volatile u16 *)FW_CFG_SEL = bswap16(selector);
    dsb();
}

static u8 fw_cfg_read_byte(void)
{
    return *(volatile u8 *)FW_CFG_DATA;
}

/* Read n bytes from currently selected fw_cfg entry */
static void fw_cfg_read(void *buf, u32 n)
{
    u8 *p = (u8 *)buf;
    for (u32 i = 0; i < n; i++)
        p[i] = fw_cfg_read_byte();
}

/* fw_cfg DMA structures (all fields big-endian on wire) */
struct fw_cfg_dma_access {
    u32 control;
    u32 length;
    u64 address;
} __packed __aligned(16);

struct fw_cfg_file {
    u32 size;
    u16 select;
    u16 reserved;
    char name[56];
} __packed;

struct ramfb_cfg {
    u64 addr;
    u32 fourcc;
    u32 flags;
    u32 width;
    u32 height;
    u32 stride;
} __packed;

static u32 *fb_buffer;
static u64 fb_phys_addr;

/* Perform a DMA write to fw_cfg */
static int fw_cfg_dma_write(u16 selector, void *data, u32 len)
{
    static struct fw_cfg_dma_access dma __aligned(16);

    dma.control = bswap32(((u32)selector << 16) | (1 << 4) | (1 << 3)); /* SELECT + WRITE */

    /* Actually, bit 4 = WRITE, bit 3 = SELECT... let me recheck */
    /* DMA control bits: bit0=error, bit1=read, bit2=skip, bit3=select, bit4=write */
    /* Wait, actually for fw_cfg: SELECT=bit3, WRITE=bit4... but some sources say
       bit1=read, bit2=write, bit3=skip, bit4=select. Let me use the QEMU source convention:
       FW_CFG_DMA_CTL_ERROR  = 0x01
       FW_CFG_DMA_CTL_READ   = 0x02
       FW_CFG_DMA_CTL_SKIP   = 0x04
       FW_CFG_DMA_CTL_SELECT = 0x08
       FW_CFG_DMA_CTL_WRITE  = 0x10
    */
    dma.control = bswap32(((u32)selector << 16) | 0x08 | 0x10); /* SELECT | WRITE */
    dma.length = bswap32(len);
    dma.address = bswap64((u64)data);

    dsb();

    /* Write DMA address to doorbell: high 32 bits first, then low 32 bits */
    u64 dma_addr = (u64)&dma;
    *(volatile u32 *)(FW_CFG_DMA + 0) = bswap32((u32)(dma_addr >> 32));
    *(volatile u32 *)(FW_CFG_DMA + 4) = bswap32((u32)(dma_addr & 0xFFFFFFFF));

    dsb();

    /* Wait for completion (control becomes 0) */
    volatile u32 *ctrl = &dma.control;
    int timeout = 1000000;
    while (*ctrl != 0 && --timeout > 0)
        ;

    if (*ctrl != 0) {
        kprintf("[fb] DMA timeout\n");
        return -1;
    }

    return 0;
}

/* Find a fw_cfg file by name, return its selector or -1 */
static int fw_cfg_find_file(const char *name)
{
    fw_cfg_select(FW_CFG_FILE_DIR);

    u32 count_be;
    fw_cfg_read(&count_be, 4);
    u32 count = bswap32(count_be);

    for (u32 i = 0; i < count; i++) {
        struct fw_cfg_file entry;
        fw_cfg_read(&entry, sizeof(entry));

        /* Compare names */
        int match = 1;
        for (int j = 0; name[j]; j++) {
            if (entry.name[j] != name[j]) {
                match = 0;
                break;
            }
        }

        if (match) {
            u16 sel = bswap16(entry.select);
            kprintf("[fb] found '%s' at selector 0x%x\n", name, sel);
            return (int)sel;
        }
    }
    return -1;
}

int fb_init(void)
{
    /* Verify fw_cfg is present by reading signature */
    fw_cfg_select(FW_CFG_SIGNATURE);
    u8 sig[4];
    fw_cfg_read(sig, 4);
    if (sig[0] != 'Q' || sig[3] != 'U') {
        kprintf("[fb] fw_cfg not found\n");
        return -1;
    }
    kprintf("[fb] fw_cfg detected\n");

    /* Find the ramfb config file */
    int ramfb_sel = fw_cfg_find_file("etc/ramfb");
    if (ramfb_sel < 0) {
        kprintf("[fb] ramfb not found (add -device ramfb to QEMU)\n");
        return -1;
    }

    /* Allocate framebuffer memory (contiguous pages) */
    u64 fb_pages = (FB_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
    fb_buffer = (u32 *)pages_alloc(fb_pages);
    if (!fb_buffer) {
        kprintf("[fb] failed to allocate framebuffer (%llu pages)\n", fb_pages);
        return -1;
    }
    fb_phys_addr = (u64)fb_buffer;
    kprintf("[fb] framebuffer at %p (%llu pages)\n", fb_buffer, fb_pages);

    /* Prepare ramfb config (all fields big-endian) */
    static struct ramfb_cfg cfg __aligned(16);
    cfg.addr   = bswap64(fb_phys_addr);
    cfg.fourcc = bswap32(DRM_FORMAT_XRGB8888);
    cfg.flags  = 0;
    cfg.width  = bswap32(FB_WIDTH);
    cfg.height = bswap32(FB_HEIGHT);
    cfg.stride = bswap32(FB_STRIDE);

    /* Write config via DMA */
    if (fw_cfg_dma_write((u16)ramfb_sel, &cfg, sizeof(cfg)) < 0) {
        kprintf("[fb] failed to configure ramfb\n");
        return -1;
    }

    kprintf("[fb] ramfb configured: %dx%d XRGB8888\n", FB_WIDTH, FB_HEIGHT);
    return 0;
}

u32 *fb_get_buffer(void)
{
    return fb_buffer;
}
