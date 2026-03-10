/*
 * Dragoon Microkernel - Virtio MMIO Transport Driver
 *
 * Probes QEMU virt machine's virtio MMIO slots at 0x0A000000.
 * Supports both legacy (v1) and modern (v2) virtio-mmio.
 * Poll-based (no IRQ).
 */
#include "virtio.h"
#include "mm.h"
#include "printf.h"

/* QEMU virt: 32 virtio MMIO slots, 0x200 apart */
#define VIRTIO_MMIO_BASE  0x0A000000ULL
#define VIRTIO_MMIO_STEP  0x200
#define VIRTIO_MMIO_SLOTS 32

/* Legacy-only registers */
#define VIRTIO_MMIO_GUEST_PAGE_SIZE 0x028
#define VIRTIO_MMIO_QUEUE_ALIGN     0x03C
#define VIRTIO_MMIO_QUEUE_PFN       0x040

static struct virtio_dev devices[VIRTIO_MAX_DEVS];
static int num_devices;

static inline void vio_write(u64 base, u32 off, u32 val)
{
    *(volatile u32 *)(base + off) = val;
}

static inline u32 vio_read(u64 base, u32 off)
{
    return *(volatile u32 *)(base + off);
}

static inline void vio_write8(u64 base, u32 off, u8 val)
{
    *(volatile u8 *)(base + off) = val;
}

static inline u8 vio_read8(u64 base, u32 off)
{
    return *(volatile u8 *)(base + off);
}

int virtio_probe(void)
{
    num_devices = 0;

    for (int i = 0; i < VIRTIO_MMIO_SLOTS && num_devices < VIRTIO_MAX_DEVS; i++) {
        u64 base = VIRTIO_MMIO_BASE + (u64)i * VIRTIO_MMIO_STEP;

        u32 magic = vio_read(base, VIRTIO_MMIO_MAGIC);
        if (magic != VIRTIO_MAGIC)
            continue;

        u32 version = vio_read(base, VIRTIO_MMIO_VERSION);
        u32 dev_id = vio_read(base, VIRTIO_MMIO_DEVICE_ID);

        if (dev_id == 0)
            continue; /* empty slot */

        kprintf("[virtio] slot %d: v%u devid=%u base=0x%llx\n",
                i, version, dev_id, base);

        struct virtio_dev *dev = &devices[num_devices];
        dev->base = base;
        dev->device_id = dev_id;
        dev->vendor_id = vio_read(base, VIRTIO_MMIO_VENDOR_ID);
        dev->version = version;
        dev->num_queues = 0;
        dev->active = 0;
        num_devices++;
    }

    kprintf("[virtio] found %d devices\n", num_devices);
    return num_devices;
}

struct virtio_dev *virtio_find_dev(u32 device_id, int index)
{
    int count = 0;
    for (int i = 0; i < num_devices; i++) {
        if (devices[i].device_id == device_id) {
            if (count == index)
                return &devices[i];
            count++;
        }
    }
    return NULL;
}

/* Legacy (v1) queue setup: uses QueuePFN and page-aligned layout */
static int virtq_setup_legacy(struct virtio_dev *dev, int queue_idx, int size)
{
    u64 base = dev->base;

    vio_write(base, VIRTIO_MMIO_QUEUE_SEL, (u32)queue_idx);
    dsb();

    u32 max_size = vio_read(base, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max_size == 0) {
        kprintf("[virtio] queue %d not available\n", queue_idx);
        return -1;
    }
    if ((u32)size > max_size)
        size = (int)max_size;

    /* Tell device our page size */
    vio_write(base, VIRTIO_MMIO_GUEST_PAGE_SIZE, PAGE_SIZE);

    /*
     * Legacy queue layout (must be contiguous, page-aligned):
     *   Descriptors: size * 16 bytes
     *   Available ring: 6 + 2 * size bytes
     *   --- pad to PAGE_SIZE alignment ---
     *   Used ring: 6 + 8 * size bytes
     */
    u64 desc_size = (u64)size * 16;
    u64 avail_size = 6 + (u64)size * 2;
    u64 avail_end = desc_size + avail_size;
    u64 used_offset = ALIGN_UP(avail_end, PAGE_SIZE);
    u64 used_size = 6 + (u64)size * 8;
    u64 total_size = used_offset + used_size;
    u64 total_pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;

    void *mem = pages_alloc(total_pages);
    if (!mem) {
        kprintf("[virtio] queue alloc failed\n");
        return -1;
    }

    u64 addr = (u64)mem;
    struct virtq_desc  *desc  = (struct virtq_desc *)addr;
    struct virtq_avail *avail = (struct virtq_avail *)(addr + desc_size);
    struct virtq_used  *used  = (struct virtq_used *)(addr + used_offset);

    /* Initialize descriptors as free list */
    for (int i = 0; i < size; i++) {
        desc[i].addr = 0;
        desc[i].len = 0;
        desc[i].flags = 0;
        desc[i].next = (u16)(i + 1);
    }
    desc[size - 1].next = 0xFFFF;
    avail->flags = 0;
    avail->idx = 0;
    used->flags = 0;
    used->idx = 0;

    struct virtq *vq = &dev->queues[queue_idx];
    vq->desc = desc;
    vq->avail = avail;
    vq->used = used;
    vq->num = (u16)size;
    vq->free_head = 0;
    vq->last_used_idx = 0;
    vq->base = base;
    vq->queue_idx = (u32)queue_idx;

    /* Tell device: queue size and physical page frame number */
    vio_write(base, VIRTIO_MMIO_QUEUE_NUM, (u32)size);
    vio_write(base, VIRTIO_MMIO_QUEUE_ALIGN, PAGE_SIZE);
    vio_write(base, VIRTIO_MMIO_QUEUE_PFN, (u32)(addr / PAGE_SIZE));
    dsb();

    kprintf("[virtio] legacy queue %d: size=%d pfn=0x%x\n",
            queue_idx, size, (u32)(addr / PAGE_SIZE));
    return 0;
}

/* Modern (v2) queue setup: uses separate desc/avail/used addresses */
static int virtq_setup_modern(struct virtio_dev *dev, int queue_idx, int size)
{
    u64 base = dev->base;

    vio_write(base, VIRTIO_MMIO_QUEUE_SEL, (u32)queue_idx);
    dsb();

    u32 max_size = vio_read(base, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max_size == 0) {
        kprintf("[virtio] queue %d not available\n", queue_idx);
        return -1;
    }
    if ((u32)size > max_size)
        size = (int)max_size;

    void *mem = pages_alloc(1);
    if (!mem) {
        kprintf("[virtio] queue alloc failed\n");
        return -1;
    }

    u64 addr = (u64)mem;
    struct virtq_desc  *desc  = (struct virtq_desc *)addr;
    struct virtq_avail *avail = (struct virtq_avail *)(addr + (u64)size * 16);
    u64 used_off = ALIGN_UP((u64)avail + 4 + (u64)size * 2, 4);
    struct virtq_used  *used  = (struct virtq_used *)used_off;

    for (int i = 0; i < size; i++) {
        desc[i].addr = 0;
        desc[i].len = 0;
        desc[i].flags = 0;
        desc[i].next = (u16)(i + 1);
    }
    desc[size - 1].next = 0xFFFF;
    avail->flags = 0;
    avail->idx = 0;
    used->flags = 0;
    used->idx = 0;

    struct virtq *vq = &dev->queues[queue_idx];
    vq->desc = desc;
    vq->avail = avail;
    vq->used = used;
    vq->num = (u16)size;
    vq->free_head = 0;
    vq->last_used_idx = 0;
    vq->base = base;
    vq->queue_idx = (u32)queue_idx;

    vio_write(base, VIRTIO_MMIO_QUEUE_NUM, (u32)size);
    vio_write(base, VIRTIO_MMIO_QUEUE_DESC_LOW,  (u32)((u64)desc & 0xFFFFFFFF));
    vio_write(base, VIRTIO_MMIO_QUEUE_DESC_HIGH, (u32)((u64)desc >> 32));
    vio_write(base, VIRTIO_MMIO_QUEUE_AVAIL_LOW,  (u32)((u64)avail & 0xFFFFFFFF));
    vio_write(base, VIRTIO_MMIO_QUEUE_AVAIL_HIGH, (u32)((u64)avail >> 32));
    vio_write(base, VIRTIO_MMIO_QUEUE_USED_LOW,  (u32)((u64)used & 0xFFFFFFFF));
    vio_write(base, VIRTIO_MMIO_QUEUE_USED_HIGH, (u32)((u64)used >> 32));
    dsb();
    vio_write(base, VIRTIO_MMIO_QUEUE_READY, 1);
    dsb();

    kprintf("[virtio] modern queue %d: size=%d\n", queue_idx, size);
    return 0;
}

int virtq_setup(struct virtio_dev *dev, int queue_idx, int size)
{
    if (dev->version == 1)
        return virtq_setup_legacy(dev, queue_idx, size);
    else
        return virtq_setup_modern(dev, queue_idx, size);
}

int virtio_dev_init(struct virtio_dev *dev, int num_queues)
{
    u64 base = dev->base;
    u32 ver = dev->version;

    kprintf("[virtio] init dev at 0x%llx (v%u, devid=%u)\n",
            base, ver, dev->device_id);

    /* Reset */
    vio_write(base, VIRTIO_MMIO_STATUS, 0);
    dsb();

    /* Acknowledge */
    vio_write(base, VIRTIO_MMIO_STATUS,
              vio_read(base, VIRTIO_MMIO_STATUS) | VIRTIO_STATUS_ACKNOWLEDGE);

    /* Driver */
    vio_write(base, VIRTIO_MMIO_STATUS,
              vio_read(base, VIRTIO_MMIO_STATUS) | VIRTIO_STATUS_DRIVER);

    /* Read device features (page 0) */
    vio_write(base, VIRTIO_MMIO_DEV_FEATURES_SEL, 0);
    dsb();
    u32 features = vio_read(base, VIRTIO_MMIO_DEV_FEATURES);
    kprintf("[virtio] features=0x%x\n", features);

    /* Accept all features */
    vio_write(base, VIRTIO_MMIO_DRV_FEATURES_SEL, 0);
    vio_write(base, VIRTIO_MMIO_DRV_FEATURES, features);

    /* Modern: need FEATURES_OK. Legacy: skip this step. */
    if (ver >= 2) {
        vio_write(base, VIRTIO_MMIO_STATUS,
                  vio_read(base, VIRTIO_MMIO_STATUS) | VIRTIO_STATUS_FEATURES_OK);
        dsb();

        u32 status = vio_read(base, VIRTIO_MMIO_STATUS);
        if (!(status & VIRTIO_STATUS_FEATURES_OK)) {
            kprintf("[virtio] features negotiation failed\n");
            vio_write(base, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
            return -1;
        }
    }

    /* Setup queues */
    for (int q = 0; q < num_queues; q++) {
        if (virtq_setup(dev, q, VIRTQ_SIZE) < 0)
            return -1;
    }
    dev->num_queues = num_queues;

    /* Driver OK */
    vio_write(base, VIRTIO_MMIO_STATUS,
              vio_read(base, VIRTIO_MMIO_STATUS) | VIRTIO_STATUS_DRIVER_OK);
    dsb();

    dev->active = 1;
    kprintf("[virtio] device ready (v%u, %d queues)\n", ver, num_queues);
    return 0;
}

void virtq_push_buf(struct virtq *vq, void *buf, u32 len, u16 flags)
{
    u16 head = vq->free_head;
    if (head == 0xFFFF)
        return;

    struct virtq_desc *d = &vq->desc[head];
    vq->free_head = d->next;

    d->addr = (u64)buf;
    d->len = len;
    d->flags = flags;
    d->next = 0;

    u16 avail_idx = vq->avail->idx;
    vq->avail->ring[avail_idx % vq->num] = head;
    dmb();
    vq->avail->idx = avail_idx + 1;
    dmb();
}

void virtq_kick(struct virtq *vq)
{
    dsb();
    vio_write(vq->base, VIRTIO_MMIO_QUEUE_NOTIFY, vq->queue_idx);
}

int virtq_has_used(struct virtq *vq)
{
    dmb();
    return vq->used->idx != vq->last_used_idx;
}

int virtq_pop_used(struct virtq *vq, u32 *len)
{
    if (!virtq_has_used(vq))
        return -1;

    u16 idx = vq->last_used_idx % vq->num;
    struct virtq_used_elem *elem = &vq->used->ring[idx];

    u32 desc_id = elem->id;
    if (len)
        *len = elem->len;

    vq->last_used_idx++;

    /* Return descriptor to free list */
    vq->desc[desc_id].next = vq->free_head;
    vq->free_head = (u16)desc_id;

    return (int)desc_id;
}

u8 virtio_config_read8(struct virtio_dev *dev, u32 offset)
{
    return vio_read8(dev->base, VIRTIO_MMIO_CONFIG + offset);
}

void virtio_config_write8(struct virtio_dev *dev, u32 offset, u8 val)
{
    vio_write8(dev->base, VIRTIO_MMIO_CONFIG + offset, val);
    dsb();
}
