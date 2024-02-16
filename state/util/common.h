#pragma once

#include <stdint.h>
#include <stddef.h>
#include "my_compiler.h"

// #define ALWAYS_INLINE inline __attribute__((always_inline))

using mr_id_t = int;            // memory region id type
using tx_id_t = uint64_t;       // transaction id type, is the same as trx_id_t in mysql
using coro_id_t = int;          // Coroutine id type
using t_id_t = uint32_t;        // thread id type
using node_id_t = int;          // Node id type
using offset_t = int64_t;       // offset type, used for remote offset in RDMA
using rwlatch_t = uint64_t;       // latch that has to be modified by RDMA atomic operation, must be 64bit

// memory region ids for various state
const mr_id_t STATE_TXN_LIST_ID = 97;
const mr_id_t STATE_LOG_BUF_ID = 98;
const mr_id_t STATE_LOCK_BUF_ID = 99;
// memory region ids for MasterNode's local_mr
const mr_id_t MASTER_LOCAL_ID = 101;

#define MAX_REMOTE_NODE_NUM 10  // Max of remote node number

#define unlikely(x) __builtin_expect(!!(x), 0)
#define likely(x) __builtin_expect(!!(x), 1)

#define CORO_NUM 1      // number of coroutine

// used for bitmap latch
#define BITMAP_LOCKED 0x1
#define BITMAP_UNLOCKED 0x0

// used for transaction state in TxnItem
#define STATE_TXN_ACTIVE 0x0        // the transaction is in processing (cannot release locks)
#define STATE_TXN_COMMITING 0x1     // the transaction is commtting (cannot request new lock)
#define STATE_TXN_ABORTING 0x2      // the transaction is aborting (cannot request new lock)
