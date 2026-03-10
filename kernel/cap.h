#ifndef DRAGOON_CAP_H
#define DRAGOON_CAP_H

#include "types.h"

/* Capability types */
#define CAP_NONE    0
#define CAP_MEMORY  1
#define CAP_IPC     2
#define CAP_IRQ     3
#define CAP_IO      4
#define CAP_TASK    5

/* IPC rights */
#define CAP_IPC_SEND (1 << 0)
#define CAP_IPC_RECV (1 << 1)

#define MAX_CAPS_PER_TASK 16
#define CAP_POOL_SIZE     256

struct capability {
    u32 type;
    u32 refcount;
    union {
        struct { u64 base; u64 pages; }  mem;
        struct { u32 ep_id; u32 rights; } ipc;
        struct { u32 irq_num; }           irq;
        struct { u64 base; u64 size; }    io;
        struct { u32 task_id; }           task;
    };
};

void cap_init(void);
int cap_alloc(struct capability *cap_out);
int cap_destroy(int cap_id);
struct capability *cap_lookup(int cap_id);
int cap_check(int cap_id, u32 expected_type);

/* Convenience constructors */
int cap_create_memory(u64 base, u64 pages);
int cap_create_ipc(u32 ep_id, u32 rights);
int cap_create_irq(u32 irq_num);
int cap_create_io(u64 base, u64 size);
int cap_create_task(u32 task_id);

#endif /* DRAGOON_CAP_H */
