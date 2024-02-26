#include "coroutine_scheduler.h"

// ALWAYS_INLINE
bool CoroutineScheduler::RDMAReadSync(coro_id_t coro_id, RCQP* qp, char* rd_data, uint64_t remote_offset, size_t size) {
  auto rc = qp->post_send(IBV_WR_RDMA_READ, rd_data, size, remote_offset, IBV_SEND_SIGNALED, coro_id);
  if (rc != SUCC) {
    RDMA_LOG(rdma_loglevel::RDMA_LOG_ERROR) << "client: post read fail. rc=" << rc << ", tid = " << t_id << ", coroid = " << coro_id;
    return false;
  }
  ibv_wc wc{};
  rc = qp->poll_till_completion(wc, no_timeout);
  if (rc != SUCC) {
    RDMA_LOG(rdma_loglevel::RDMA_LOG_ERROR) << "client: poll read fail. rc=" << rc << ", tid = " << t_id << ", coroid = " << coro_id;
    return false;
  }
  return true;
}

// ALWAYS_INLINE
bool CoroutineScheduler::RDMACASSync(coro_id_t coro_id, RCQP* qp, char* local_buf, uint64_t remote_offset, uint64_t compare, uint64_t swap) {
  auto rc = qp->post_cas(local_buf, remote_offset, compare, swap, IBV_SEND_SIGNALED, coro_id);
  if (rc != SUCC) {
    RDMA_LOG(rdma_loglevel::RDMA_LOG_ERROR) << "client: post cas fail. rc=" << rc << ", tid = " << t_id << ", coroid = " << coro_id;
    return false;
  }
  ibv_wc wc{};
  rc = qp->poll_till_completion(wc, no_timeout);
  if(rc != SUCC) {
    RDMA_LOG(rdma_loglevel::RDMA_LOG_ERROR) << "client: poll read fail. rc=" << rc << ", tid = " << t_id << ", coroid = " << coro_id;
    return false;
  }
  return true;
}

// ALWAYS_INLINE
bool CoroutineScheduler::RDMAWriteSync(coro_id_t coro_id, RCQP* qp, char* wt_data, uint64_t remote_offset, size_t size) {
  auto rc = qp->post_send(IBV_WR_RDMA_WRITE, wt_data, size, remote_offset, IBV_SEND_SIGNALED, coro_id);
  if (rc != SUCC) {
    RDMA_LOG(rdma_loglevel::RDMA_LOG_ERROR) << "client: post write fail. rc=" << rc << ", tid = " << t_id << ", coroid = " << coro_id;
    return false;
  }
  ibv_wc wc{};
  rc = qp->poll_till_completion(wc, no_timeout);
  if (rc != SUCC) {
    RDMA_LOG(rdma_loglevel::RDMA_LOG_ERROR) << "client: poll read fail. rc=" << rc << ", tid = " << t_id << ", coroid = " << coro_id;
    return false;
  }
  return true;
}