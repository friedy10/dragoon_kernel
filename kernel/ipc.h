#ifndef DRAGOON_IPC_H
#define DRAGOON_IPC_H

#include "types.h"

#define MAX_ENDPOINTS 32

struct ipc_message {
    u64 data[4];
    int cap_transfer;  /* capability ID to transfer, -1 if none */
};

void ipc_init(void);
int ipc_endpoint_create(void);
void ipc_endpoint_destroy(int ep_id);
int ipc_send(int ep_id, struct ipc_message *msg);
int ipc_recv(int ep_id, struct ipc_message *msg);

#endif /* DRAGOON_IPC_H */
