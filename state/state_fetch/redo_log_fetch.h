//
// Created by xuyanshi on 1/27/24.
//

#pragma once

#include "state/state_store/redo_log.h"

#include "sql_class.h"

#include "sql/sql_thd_internal_api.h"

/**
 * @StateReplicate: 定义了 redo log 的状态取回
 */
// class RedoLogFetch {
//  public:
//   RedoLogFetch() {}

//   ~RedoLogFetch() { destroy_internal_thd(thd); }

//   RedoLogFetch(bool status) : failStatus(status) {}

//   void setFailStatus(bool status) { failStatus = status; }

//   bool getFailStatus() const { return failStatus; }

//   void setRedoLogItem(RedoLogItem *item) { redoLogItem = item; }

//   RedoLogItem *getRedoLogItem() const { return redoLogItem; }

//   void setRedoLogBufferBuf(unsigned char *buffer) {
//     redo_log_buffer_buf = buffer;
//   }
//   unsigned char *getRedoLogBufferBuf() const { return redo_log_buffer_buf; }

//   /**
//    * @StateReplicate: 把 redo log buffer 从状态层读回来
//    * @return
//    */
//   bool redo_log_fetch() {
//     if (this->getFailStatus()) {
//       // 取回 redo log buffer 的元数据
//       size_t redo_log_buf_size = sizeof(RedoLogItem);
//       redoLogItem =
//           (RedoLogItem *)thd->rdma_buffer_allocator->Alloc(redo_log_buf_size);
//       if (!thd->coro_sched->RDMAReadSync(0, qp, (char *)redoLogItem,
//                                          meta_mgr->GetRedoLogCurrAddr(),
//                                          redo_log_buf_size)) {
//         return false;
//       }
//       // this->setRedoLogItem(redoLogItem);

//       // 取回 redo log buffer 的实际数据
//       size_t redo_log_buffer_buf_size = sizeof(redoLogItem->buf_size);
//       redo_log_buffer_buf = (unsigned char *)thd->rdma_buffer_allocator->Alloc(
//           redo_log_buffer_buf_size);
//       if (!thd->coro_sched->RDMAReadSync(
//               0, qp, (char *)redo_log_buffer_buf,
//               meta_mgr->GetRedoLogCurrAddr() + sizeof(*redoLogItem),
//               redo_log_buffer_buf_size)) {
//         return false;
//       }
//       // this->setRedoLogBufferBuf(nullptr);
//     }
//     return true;
//   }

//   /**
//    * @StateReplicate: TODO: 回放 buffer 中存储的 log，实现状态恢复
//    * @return
//    */
//   bool redo_log_replay() { return true; }

//  private:
//   // failStatus 为真，则说明需要进行故障恢复，继续之后的逻辑
//   bool failStatus = false;

//   // redo log buffer 的元信息，即原来的 log
//   RedoLogItem *redoLogItem = nullptr;

//   // redo log buffer 的实际数据，即原来的 log.buf
//   unsigned char *redo_log_buffer_buf = nullptr;

//   // 处理读回操作的线程
//   THD *thd = create_internal_thd();

//   node_id_t primary_node_id = MetaManager::get_instance()->GetPrimaryNodeID();
//   RCQP *qp = thd->qp_manager->GetRemoteLogBufQPWithNodeID(primary_node_id);
//   MetaManager *meta_mgr = MetaManager::get_instance();
// };