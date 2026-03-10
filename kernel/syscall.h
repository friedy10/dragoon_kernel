#ifndef DRAGOON_SYSCALL_H
#define DRAGOON_SYSCALL_H

#include "types.h"

/* Syscall numbers */
#define SYS_YIELD       0
#define SYS_SEND        1
#define SYS_RECV        2
#define SYS_CAP_CREATE  3
#define SYS_CAP_DESTROY 4
#define SYS_MAP_PAGE    5
#define SYS_DEBUG_PRINT 6
#define SYS_EXIT        7

void syscall_init(void);
u64 syscall_handler(u64 num, u64 a0, u64 a1, u64 a2, u64 a3);

#endif /* DRAGOON_SYSCALL_H */
