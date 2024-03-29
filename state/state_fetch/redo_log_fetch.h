//
// Created by xuyanshi on 1/27/24.
//

#pragma once

#include "state/state_store/redo_log.h"

// #include "sql_class.h"

// #include "sql/sql_thd_internal_api.h"

// 包括了QP，MetaManager等
#include "storage/innobase/include/log0sys.h"

/**
 * @StateReplicate: 定义了 redo log 的状态取回
 */
class RedoLogFetch {
 public:
  RedoLogFetch() = default;

  ~RedoLogFetch() = default;

  RedoLogFetch(bool status) : failStatus(status) {}

  void setFailStatus(bool status) { failStatus = status; }

  bool getFailStatus() const { return failStatus; }

  void setRedoLogItem(RedoLogItem *item) { redoLogItem = item; }

  RedoLogItem *getRedoLogItem() const { return redoLogItem; }

  void setRedoLogBufferBuf(unsigned char *buffer) { log_buf_data = buffer; }
  unsigned char *getRedoLogBufferBuf() const { return log_buf_data; }

  /**
   * @StateReplicate: 把 redo log buffer 从状态层读回来
   * @return
   */
  bool redo_log_fetch(log_t &log) {
    // this->log = log;
    primary_node_id = MetaManager::get_instance()->GetPrimaryNodeID();
    qp = log.qp_manager->GetRemoteLogBufQPWithNodeID(primary_node_id);
    meta_mgr = MetaManager::get_instance();
    
    // 取回 redo log buffer 的元数据
    size_t redo_log_buf_size = sizeof(RedoLogItem);
    redoLogItem =
        (RedoLogItem *)log.rdma_buffer_allocator->Alloc(redo_log_buf_size);
    if (!log.coro_sched->RDMAReadSync(0, qp, (char *)redoLogItem,
                                      meta_mgr->GetRedoLogCurrAddr(),
                                      redo_log_buf_size)) {
      // Fail
      std::cout << "failed to read redo_log_remote_buf\n";
      assert(0);
      return false;
    }

    // 取回 redo log buffer 的实际数据
    // TODO:这里的size还不确定，需要与storage/innobase/log/log0buf.cc:1133保持一致
    size_t log_buf_data_size = ut::INNODB_CACHE_LINE_SIZE; // log.buf_size;
    log_buf_data = (byte *)log.rdma_buffer_allocator->Alloc(log_buf_data_size);
    if (!log.coro_sched->RDMAReadSync(
            0, qp, (char *)log_buf_data,
            meta_mgr->GetRedoLogCurrAddr() + sizeof(RedoLogItem),
            log_buf_data_size)) {
      // Fail
      std::cout << "failed to read log_buf_data\n";
      assert(0);
      return false;
    }

    return true;
  }

  /**
   * @StateReplicate: TODO: 回放 buffer 中存储的 log，实现状态恢复
   * @return
   */
  bool redo_log_replay() { return true; }

 private:
  // failStatus 为真，则说明需要进行故障恢复，继续之后的逻辑
  bool failStatus = false;

  // redo log buffer 的元信息，即原来的 log
  RedoLogItem *redoLogItem = nullptr;

  // redo log buffer 的实际数据，即原来的 log.buf
  unsigned char *log_buf_data = nullptr;

  node_id_t primary_node_id =
      0;  // MetaManager::get_instance()->GetPrimaryNodeID();
  RCQP *qp =
      nullptr;  // thd->qp_manager->GetRemoteLogBufQPWithNodeID(primary_node_id);
  MetaManager *meta_mgr = nullptr;  // MetaManager::get_instance();
};