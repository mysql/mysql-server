#pragma once

#include "meta_manager.h"

/** This QPManager builds qp connections (compute node <-> memory node) 
 * for each txn thread in MasterNode
*/
class QPManager {
 public:
  QPManager(t_id_t global_tid) : global_tid(global_tid) {}

  void BuildQPConnection(MetaManager* meta_man);

  ALWAYS_INLINE
  RCQP* GetRemoteTxnListQPWithNodeID(const node_id_t node_id) const {
    return txn_list_qps[node_id];
  }

  ALWAYS_INLINE
  RCQP* GetRemoteLockBufQPWithNodeID(const node_id_t node_id) const {
    return lock_buf_qps[node_id];
  }

  ALWAYS_INLINE
  RCQP* GetRemoteLogBufQPWithNodeID(const node_id_t node_id) const {
    return log_buf_qps[node_id];
  }

  ALWAYS_INLINE
  void GetRemoteTxnListQPsWithNodeIDs(const std::vector<node_id_t>* node_ids, std::vector<RCQP*>& qps) {
    for (node_id_t node_id : *node_ids) {
      RCQP* qp = txn_list_qps[node_id];
      if (qp) {
        qps.push_back(qp);
      }
    }
  }

  ALWAYS_INLINE
  void GetRemoteLockBufQPsWithNodeIDs(const std::vector<node_id_t>* node_ids, std::vector<RCQP*>& qps) {
    for (node_id_t node_id : *node_ids) {
      RCQP* qp = lock_buf_qps[node_id];
      if (qp) {
        qps.push_back(qp);
      }
    }
  }

  ALWAYS_INLINE
  void GetRemoteLogBufQPsWithNodeIDs(const std::vector<node_id_t>* node_ids, std::vector<RCQP*>& qps) {
    for (node_id_t node_id : *node_ids) {
      RCQP* qp = log_buf_qps[node_id];
      if (qp) {
        qps.push_back(qp);
      }
    }
  }

 private:
  RCQP* txn_list_qps[MAX_REMOTE_NODE_NUM]{nullptr};

  RCQP* lock_buf_qps[MAX_REMOTE_NODE_NUM]{nullptr};

  RCQP* log_buf_qps[MAX_REMOTE_NODE_NUM]{nullptr};
  
  t_id_t global_tid;
};
