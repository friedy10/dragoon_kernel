#ifndef DRAGOON_VIRTIO_H
#define DRAGOON_VIRTIO_H

#include "types.h"

/* Virtio MMIO register offsets */
#define VIRTIO_MMIO_MAGIC          0x000
#define VIRTIO_MMIO_VERSION        0x004
#define VIRTIO_MMIO_DEVICE_ID      0x008
#define VIRTIO_MMIO_VENDOR_ID      0x00C
#define VIRTIO_MMIO_DEV_FEATURES   0x010
#define VIRTIO_MMIO_DEV_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRV_FEATURES   0x020
#define VIRTIO_MMIO_DRV_FEATURES_SEL 0x024
#define VIRTIO_MMIO_QUEUE_SEL      0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX  0x034
#define VIRTIO_MMIO_QUEUE_NUM      0x038
#define VIRTIO_MMIO_QUEUE_READY    0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY   0x050
#define VIRTIO_MMIO_INT_STATUS     0x060
#define VIRTIO_MMIO_INT_ACK        0x064
#define VIRTIO_MMIO_STATUS         0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW   0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH  0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW  0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH 0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW   0x0A0
#define VIRTIO_MMIO_QUEUE_USED_HIGH  0x0A4
#define VIRTIO_MMIO_CONFIG         0x100

/* Virtio status bits */
#define VIRTIO_STATUS_ACKNOWLEDGE  0x01
#define VIRTIO_STATUS_DRIVER       0x02
#define VIRTIO_STATUS_DRIVER_OK    0x04
#define VIRTIO_STATUS_FEATURES_OK  0x08
#define VIRTIO_STATUS_FAILED       0x80

/* Virtio magic value */
#define VIRTIO_MAGIC 0x74726976

/* Device IDs */
#define VIRTIO_DEV_INPUT 18

/* Virtqueue descriptor flags */
#define VIRTQ_DESC_F_NEXT     1
#define VIRTQ_DESC_F_WRITE    2

/* Virtqueue size */
#define VIRTQ_SIZE 16

/* Virtqueue descriptor */
struct virtq_desc {
    u64 addr;
    u32 len;
    u16 flags;
    u16 next;
} __packed;

/* Virtqueue available ring */
struct virtq_avail {
    u16 flags;
    u16 idx;
    u16 ring[VIRTQ_SIZE];
} __packed;

/* Virtqueue used element */
struct virtq_used_elem {
    u32 id;
    u32 len;
} __packed;

/* Virtqueue used ring */
struct virtq_used {
    u16 flags;
    u16 idx;
    struct virtq_used_elem ring[VIRTQ_SIZE];
} __packed;

/* Virtqueue */
struct virtq {
    struct virtq_desc  *desc;
    struct virtq_avail *avail;
    struct virtq_used  *used;
    u16 num;
    u16 free_head;
    u16 last_used_idx;
    u64 base;       /* MMIO base of the device this queue belongs to */
    u32 queue_idx;  /* queue index within the device */
};

/* Virtio device */
#define VIRTIO_MAX_QUEUES 2
struct virtio_dev {
    u64 base;           /* MMIO base address */
    u32 device_id;
    u32 vendor_id;
    u32 version;        /* 1=legacy, 2=modern */
    struct virtq queues[VIRTIO_MAX_QUEUES];
    int num_queues;
    int active;
};

/* Max virtio devices we track */
#define VIRTIO_MAX_DEVS 8

/* API */
int  virtio_probe(void);
struct virtio_dev *virtio_find_dev(u32 device_id, int index);
int  virtio_dev_init(struct virtio_dev *dev, int num_queues);
int  virtq_setup(struct virtio_dev *dev, int queue_idx, int size);
void virtq_push_buf(struct virtq *vq, void *buf, u32 len, u16 flags);
int  virtq_has_used(struct virtq *vq);
int  virtq_pop_used(struct virtq *vq, u32 *len);
void virtq_kick(struct virtq *vq);

/* Read/write device config space */
u8   virtio_config_read8(struct virtio_dev *dev, u32 offset);
void virtio_config_write8(struct virtio_dev *dev, u32 offset, u8 val);

#endif /* DRAGOON_VIRTIO_H */
