#pragma once

#include <list>
#include <cassert>

#include "util/debug.h"

#include "util/common.h"
#include "rlib/rdma_ctrl.hpp"
// #include "coroutine.h"

using namespace rdmaio;

// Scheduling coroutines. Each txn thread only has ONE scheduler
class CoroutineScheduler {
 public:
  // The coro_num includes all the coroutines
  CoroutineScheduler(t_id_t thread_id, coro_id_t coro_num) { t_id = thread_id; }
  ~CoroutineScheduler() {}

  bool RDMAWriteSync(coro_id_t coro_id, RCQP* qp, char* wt_data, uint64_t remote_offset, size_t size);

  bool RDMAReadSync(coro_id_t coro_id, RCQP* qp, char* rd_data, uint64_t remote_offset, size_t size);

  bool RDMACASSync(coro_id_t coro_id, RCQP* qp, char* local_buf, uint64_t remote_offset, uint64_t compare, uint64_t swap);

 private:
  t_id_t t_id;
};