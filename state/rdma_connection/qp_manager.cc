#include "qp_manager.h"

/**
 *
 */
void QPManager::BuildQPConnection(MetaManager *meta_man) {
  for (const auto &remote_node : meta_man->remote_nodes) {
    // Note that each remote machine has one MemStore mr and one Log mr
    // MemoryAttr remote_hash_mr =
    // meta_man->GetRemoteHashMR(remote_node.node_id); MemoryAttr remote_log_mr
    // = meta_man->GetRemoteLogMR(remote_node.node_id);

    MemoryAttr remote_log_buf_mr =
        meta_man->GetRemoteLogBufMR(remote_node.node_id);
    MemoryAttr remote_lock_buf_mr =
        meta_man->GetRemoteLockBufMR(remote_node.node_id);
    MemoryAttr remote_txn_list_mr =
        meta_man->GetRemoteTxnListMR(remote_node.node_id);

    // Build QPs with one remote machine (this machine can be a primary or a
    // backup) Create the thread local queue pair
    MemoryAttr local_mr =
        meta_man->global_rdma_ctrl->get_local_mr(MASTER_LOCAL_ID);
    RCQP *txn_list_qp = meta_man->global_rdma_ctrl->create_rc_qp(
        create_rc_idx(remote_node.node_id, (int)global_tid * 2),
        meta_man->opened_rnic, &local_mr);
    RCQP *lock_buf_qp = meta_man->global_rdma_ctrl->create_rc_qp(
        create_rc_idx(remote_node.node_id, (int)global_tid * 2 + 1),
        meta_man->opened_rnic, &local_mr);
    RCQP *log_buf_qp = meta_man->global_rdma_ctrl->create_rc_qp(
        create_rc_idx(remote_node.node_id, (int)global_tid * 2 + 2),
        meta_man->opened_rnic, &local_mr);
    
    // Queue pair connection, exchange queue pair info via TCP
    ConnStatus rc;
    do {
      rc = txn_list_qp->connect(remote_node.ip, remote_node.port);
      if (rc == SUCC) {
        txn_list_qp->bind_remote_mr(
            remote_txn_list_mr);  // Bind the hash mr as the default remote mr
                                  // for convenient parameter passing
        txn_list_qps[remote_node.node_id] = txn_list_qp;
        // RDMA_LOG(INFO) << "Thread " << global_tid << ": Data QP connected!
        // with remote node: " << remote_node.node_id << " ip: " <<
        // remote_node.ip;
      }
      usleep(2000);
    } while (rc != SUCC);

    do {
      rc = lock_buf_qp->connect(remote_node.ip, remote_node.port);
      if (rc == SUCC) {
        lock_buf_qp->bind_remote_mr(
            remote_lock_buf_mr);  // Bind the hash mr as the default remote mr
                                  // for convenient parameter passing
        lock_buf_qps[remote_node.node_id] = lock_buf_qp;
        // RDMA_LOG(INFO) << "Thread " << global_tid << ": Data QP connected!
        // with remote node: " << remote_node.node_id << " ip: " <<
        // remote_node.ip;
      }
      usleep(2000);
    } while (rc != SUCC);

    do {
      rc = log_buf_qp->connect(remote_node.ip, remote_node.port);
      if (rc == SUCC) {
        log_buf_qp->bind_remote_mr(
            remote_log_buf_mr);  // Bind the log mr as the default remote mr for
                                 // convenient parameter passing
        log_buf_qps[remote_node.node_id] = log_buf_qp;
        // RDMA_LOG(INFO) << "Thread " << global_tid << ": Log QP connected!
        // with remote node: " << remote_node.node_id << " ip: " <<
        // remote_node.ip;
      }
      usleep(2000);
    } while (rc != SUCC);
  }
}