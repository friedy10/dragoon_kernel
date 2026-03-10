/*
 * Dragoon Microkernel - Syscall Handler
 *
 * Dispatches SVC calls from tasks.
 * Syscall number in x8, args in x0-x3.
 */
#include "syscall.h"
#include "printf.h"
#include "task.h"
#include "sched.h"
#include "ipc.h"
#include "cap.h"
#include "mm.h"

void syscall_init(void)
{
    kprintf("[syscall] syscall interface initialized\n");
}

u64 syscall_handler(u64 num, u64 a0, u64 a1, u64 a2, u64 a3)
{
    switch (num) {
    case SYS_YIELD:
        sched_yield();
        return 0;

    case SYS_SEND: {
        struct ipc_message msg;
        msg.data[0] = a1;
        msg.data[1] = a2;
        msg.data[2] = a3;
        msg.data[3] = 0;
        msg.cap_transfer = -1;
        return (u64)ipc_send((int)a0, &msg);
    }

    case SYS_RECV: {
        struct ipc_message msg;
        int ret = ipc_recv((int)a0, &msg);
        /* Return first data word; caller can retrieve rest via shared memory */
        if (ret == 0)
            return msg.data[0];
        return (u64)-1;
    }

    case SYS_CAP_CREATE: {
        /* a0 = type, a1/a2 = type-specific args */
        struct capability cap;
        cap.type = (u32)a0;
        switch (cap.type) {
        case CAP_MEMORY:
            cap.mem.base = a1;
            cap.mem.pages = a2;
            break;
        case CAP_IO:
            cap.io.base = a1;
            cap.io.size = a2;
            break;
        default:
            return (u64)-1;
        }
        return (u64)cap_alloc(&cap);
    }

    case SYS_CAP_DESTROY:
        return (u64)cap_destroy((int)a0);

    case SYS_MAP_PAGE: {
        void *p = page_alloc();
        return (u64)p;
    }

    case SYS_DEBUG_PRINT: {
        const char *msg = (const char *)a0;
        kprintf("%s", msg);
        return 0;
    }

    case SYS_EXIT: {
        int cur = task_current_id();
        struct task *t = task_get(cur);
        if (t) {
            t->state = TASK_DEAD;
            schedule();
        }
        return 0;
    }

    default:
        kprintf("[syscall] unknown syscall %llu\n", num);
        return (u64)-1;
    }
}
