/*
 * Dragoon Microkernel - IPC (Inter-Process Communication)
 *
 * Synchronous message passing with capability transfer.
 * Sender blocks until receiver is ready and vice versa.
 */
#include "ipc.h"
#include "task.h"
#include "spinlock.h"
#include "printf.h"

struct ipc_endpoint {
    int active;
    int sender_task;     /* task blocked waiting to send, -1 if none */
    int receiver_task;   /* task blocked waiting to receive, -1 if none */
    struct ipc_message pending_msg;
    int has_pending;
};

static struct ipc_endpoint endpoints[MAX_ENDPOINTS];
static struct spinlock ipc_lock = SPINLOCK_INIT;

void ipc_init(void)
{
    for (int i = 0; i < MAX_ENDPOINTS; i++) {
        endpoints[i].active = 0;
        endpoints[i].sender_task = -1;
        endpoints[i].receiver_task = -1;
        endpoints[i].has_pending = 0;
    }
    kprintf("[ipc] IPC subsystem initialized, max endpoints: %d\n", MAX_ENDPOINTS);
}

int ipc_endpoint_create(void)
{
    spin_lock(&ipc_lock);
    for (int i = 0; i < MAX_ENDPOINTS; i++) {
        if (!endpoints[i].active) {
            endpoints[i].active = 1;
            endpoints[i].sender_task = -1;
            endpoints[i].receiver_task = -1;
            endpoints[i].has_pending = 0;
            spin_unlock(&ipc_lock);
            return i;
        }
    }
    spin_unlock(&ipc_lock);
    return -1;
}

void ipc_endpoint_destroy(int ep_id)
{
    if (ep_id < 0 || ep_id >= MAX_ENDPOINTS)
        return;
    endpoints[ep_id].active = 0;
}

int ipc_send(int ep_id, struct ipc_message *msg)
{
    if (ep_id < 0 || ep_id >= MAX_ENDPOINTS || !endpoints[ep_id].active)
        return -1;

    struct ipc_endpoint *ep = &endpoints[ep_id];

    /* If a receiver is waiting, deliver directly */
    if (ep->receiver_task >= 0) {
        struct task *recv = task_get(ep->receiver_task);
        if (recv) {
            /* Copy message data into endpoint for receiver to pick up */
            ep->pending_msg = *msg;
            ep->has_pending = 1;
            recv->state = TASK_READY;
            ep->receiver_task = -1;
            return 0;
        }
    }

    /* No receiver ready - block sender */
    int cur = task_current_id();
    ep->pending_msg = *msg;
    ep->has_pending = 1;
    ep->sender_task = cur;

    struct task *t = task_get(cur);
    if (t)
        t->state = TASK_BLOCKED;

    /* Yield to scheduler */
    extern void schedule(void);
    schedule();

    return 0;
}

int ipc_recv(int ep_id, struct ipc_message *msg)
{
    if (ep_id < 0 || ep_id >= MAX_ENDPOINTS || !endpoints[ep_id].active)
        return -1;

    struct ipc_endpoint *ep = &endpoints[ep_id];

    /* If a message is pending, receive it */
    if (ep->has_pending) {
        *msg = ep->pending_msg;
        ep->has_pending = 0;

        /* Unblock sender if any */
        if (ep->sender_task >= 0) {
            struct task *sender = task_get(ep->sender_task);
            if (sender)
                sender->state = TASK_READY;
            ep->sender_task = -1;
        }
        return 0;
    }

    /* No message - block receiver */
    int cur = task_current_id();
    ep->receiver_task = cur;

    struct task *t = task_get(cur);
    if (t)
        t->state = TASK_BLOCKED;

    extern void schedule(void);
    schedule();

    /* After waking up, message should be pending */
    if (ep->has_pending) {
        *msg = ep->pending_msg;
        ep->has_pending = 0;
        return 0;
    }

    return -1;
}
