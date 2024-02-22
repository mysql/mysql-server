#include "qp_manager.h"
#include <fstream>
/**
 * 
*/

QPManager* QPManager::global_qp_mgr_[MAX_THREAD_NUM] = {nullptr};
int QPManager::qp_mgr_num_ = 32;
int QPManager::next_qp_mgr_idx_ = 0;

void QPManager::BuildALLQPConnection(MetaManager* meta_man) {
  for(int i = 0; i < qp_mgr_num_; ++i ) {
    global_qp_mgr_[i]->BuildQPConnection(meta_man);
  }
  std::cout << "finish build all qp_connections\n";
}

void QPManager::BuildQPConnection(MetaManager* meta_man) {
  for (const auto& remote_node : meta_man->remote_nodes) {
    // Note that each remote machine has one MemStore mr and one Log mr
    // MemoryAttr remote_hash_mr = meta_man->GetRemoteHashMR(remote_node.node_id);
    // MemoryAttr remote_log_mr = meta_man->GetRemoteLogMR(remote_node.node_id);

    MemoryAttr remote_log_buf_mr = meta_man->GetRemoteLogBufMR(remote_node.node_id);
    MemoryAttr remote_lock_buf_mr = meta_man->GetRemoteLockBufMR(remote_node.node_id);
    MemoryAttr remote_txn_list_mr = meta_man->GetRemoteTxnListMR(remote_node.node_id);

    // Build QPs with one remote machine (this machine can be a primary or a backup)
    // Create the thread local queue pair
    MemoryAttr local_mr = meta_man->global_rdma_ctrl->get_local_mr(MASTER_LOCAL_ID);
    // RCQP* txn_list_qp = meta_man->global_rdma_ctrl->create_rc_qp(create_rc_idx(remote_node.node_id, (int)global_tid * 3),
    //                                                          meta_man->opened_rnic,
    //                                                          &local_mr);
    assert(meta_man->opened_rnic != nullptr);
    RCQP* txn_list_qp = meta_man->global_rdma_ctrl->create_rc_qp(create_rc_idx(remote_node.node_id, (int)global_tid * 3),
                                                             meta_man->opened_rnic,
                                                             &local_mr);
    assert(txn_list_qp != nullptr);
    RCQP* lock_buf_qp = meta_man->global_rdma_ctrl->create_rc_qp(create_rc_idx(remote_node.node_id, (int)global_tid * 3 + 1),
                                                            meta_man->opened_rnic,
                                                            &local_mr);
    assert(lock_buf_qp != nullptr);
    RCQP* log_buf_qp = meta_man->global_rdma_ctrl->create_rc_qp(create_rc_idx(remote_node.node_id, (int)global_tid*3 + 2),
                                                            meta_man->opened_rnic,
                                                            &local_mr);
    assert(log_buf_qp != nullptr);
    std::fstream f;
    f.open("/usr/local/mysql/myerror.log", std::ios::out|std::ios::app);
    f << "finish build qps\n";
    f.close();
    // Queue pair connection, exchange queue pair info via TCP
    ConnStatus rc;
    do {
      rc = txn_list_qp->connect(remote_node.ip, remote_node.port);
      if (rc == SUCC) {
        txn_list_qp->bind_remote_mr(remote_txn_list_mr);  // Bind the hash mr as the default remote mr for convenient parameter passing
        txn_list_qps[remote_node.node_id] = txn_list_qp;
        RDMA_LOG(RDMA_LOG_INFO) << "Thread " << global_tid << ": Data QP connected! with remote node: " << remote_node.node_id << " ip: " << remote_node.ip;
      }
      usleep(2000);
    } while (rc != SUCC);

    do {
      rc = lock_buf_qp->connect(remote_node.ip, remote_node.port);
      if (rc == SUCC) {
        lock_buf_qp->bind_remote_mr(remote_lock_buf_mr);  // Bind the hash mr as the default remote mr for convenient parameter passing
        lock_buf_qps[remote_node.node_id] = lock_buf_qp;
        RDMA_LOG(RDMA_LOG_INFO) << "Thread " << global_tid << ": Data QP connected! with remote node: " << remote_node.node_id << " ip: " << remote_node.ip;
      }
      usleep(2000);
    } while (rc != SUCC);

    do {
      rc = log_buf_qp->connect(remote_node.ip, remote_node.port);
      if (rc == SUCC) {
        log_buf_qp->bind_remote_mr(remote_log_buf_mr);  // Bind the log mr as the default remote mr for convenient parameter passing
        log_buf_qps[remote_node.node_id] = log_buf_qp;
        RDMA_LOG(RDMA_LOG_INFO) << "Thread " << global_tid << ": Log QP connected! with remote node: " << remote_node.node_id << " ip: " << remote_node.ip;
      }
      usleep(2000);
    } while (rc != SUCC);
  }
}

bool QPManager::create_instance(int qp_mgr_num) {
  qp_mgr_num_ = qp_mgr_num;
  bool res = false;
  for(int i = 0; i < qp_mgr_num; ++i) {
    if(global_qp_mgr_[i] == nullptr) 
      global_qp_mgr_[i] = new (std::nothrow) QPManager(i);
    if(global_qp_mgr_[i] == nullptr) res = true;
  }
  next_qp_mgr_idx_ = 0;
  std::cout << "create instance success: next_qp_mgr_idx: " << next_qp_mgr_idx_ << "\n";
  return res;
  // if(global_qp_mgr_ == nullptr) {
  //   global_qp_mgr_ = new (std::nothrow) QPManager(0);
  // }
  // return (global_qp_mgr_ == nullptr);
}

void QPManager::destroy_instance() {
  // delete global_qp_mgr_;
  // global_qp_mgr_ = nullptr;
  for(int i = 0; i < qp_mgr_num_; ++i) {
    delete global_qp_mgr_[i];
    global_qp_mgr_[i] = nullptr;
  }
}