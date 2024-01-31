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
class RedoLogFetch {
 public:
  RedoLogFetch() {}

  RedoLogFetch(bool status) : failStatus(status) {}

  void setFailStatus(bool status) { failStatus = status; }

  bool getFailStatus() const { return failStatus; }

  void setRedoLogItem(RedoLogItem *item) { redoLogItem = item; }

  RedoLogItem *getRedoLogItem() const { return redoLogItem; }

  void setRedoLogBufferBuf(unsigned char *buffer) {
    redo_log_buffer_buf = buffer;
  }
  unsigned char *getRedoLogBufferBuf() const { return redo_log_buffer_buf; }

  /**
   * @StateReplicate: 把 redo log buffer 从状态层读回来
   * @return
   */
  bool redo_log_fetch() {
    if (this->getFailStatus()) {
      // 开始读取
      this->setRedoLogItem(nullptr);

      this->setRedoLogBufferBuf(nullptr);
    }
    return true;
  }

  // ford 中关于读取的方法，供参考？
  /*
  bool DTX::CheckCasRW(std::vector<CasRead> &pending_cas_rw,
                       std::list<HashRead> &pending_next_hash_rw,
                       std::list<InsertOffRead> &pending_next_off_rw) {
    for (auto &re : pending_cas_rw) {
#if LOCK_WAIT
      if (*((lock_t *)re.cas_buf) != STATE_CLEAN) {
        // RDMA_LOG(DBG) << std::hex << *((lock_t*)re.cas_buf);
        // Re-read the slot until it becomes unlocked
        // FOR TEST ONLY

        auto remote_data_addr = re.item->item_ptr->remote_offset;
        auto remote_lock_addr =
            re.item->item_ptr->GetRemoteLockAddr(remote_data_addr);

        while (*((lock_t *)re.cas_buf) != STATE_CLEAN) {
          // timing
          Timer timer;
          timer.Start();

          auto rc = re.qp->post_cas(re.cas_buf, remote_lock_addr, STATE_CLEAN,
                                    STATE_LOCKED, IBV_SEND_SIGNALED);
          if (rc != SUCC) {
            TLOG(ERROR, t_id) << "client: post cas fail. rc=" << rc;
            exit(-1);
          }

          ibv_wc wc{};
          rc = re.qp->poll_till_completion(wc, no_timeout);
          if (rc != SUCC) {
            TLOG(ERROR, t_id) << "client: poll cas fail. rc=" << rc;
            exit(-1);
          }

          timer.Stop();
          lock_durations.emplace_back(timer.Duration_us());
        }

        auto rc = re.qp->post_send(IBV_WR_RDMA_READ, re.data_buf, DataItemSize,
                                   remote_data_addr, IBV_SEND_SIGNALED);

        if (rc != SUCC) {
          TLOG(ERROR, t_id) << "client: post cas fail. rc=" << rc;
          exit(-1);
        }
        // Note: Now the coordinator gets the lock. It can read the data

        ibv_wc wc{};
        rc = re.qp->poll_till_completion(wc, no_timeout);
        if (rc != SUCC) {
          TLOG(ERROR, t_id) << "client: poll cas fail. rc=" << rc;
          exit(-1);
        }
      }
#else
      if (*((lock_t *)re.cas_buf) != STATE_CLEAN) {
        return false;
      }
#endif

      auto it = re.item->item_ptr;
      auto *fetched_item = (DataItem *)(re.data_buf);
      if (likely(fetched_item->key == it->key &&
                 fetched_item->table_id == it->table_id)) {
        if (it->user_insert) {
          // insert or update (insert an exsiting key)
          if (it->version < fetched_item->version) return false;
          old_version_for_insert.push_back(
              OldVersionForInsert{.table_id = it->table_id,
                                  .key = it->key,
                                  .version = fetched_item->version});
        } else {
          // Update or deletion
          if (likely(fetched_item->valid)) {
            if (tx_id < fetched_item->version) return false;
            *it = *fetched_item;  // Get old data
          } else {
            // The item is deleted before, then update the local cache
            addr_cache->Insert(re.primary_node_id, it->table_id, it->key,
                               NOT_FOUND);
            return false;
          }
        }
        // The item must be visible because we can lock it
        re.item->is_fetched = true;
      } else {
        // The cached address is stale

        // 1. Release lock
        *((lock_t *)re.cas_buf) = STATE_CLEAN;
        if (!coro_sched->RDMAWrite(coro_id, re.qp, re.cas_buf,
                                   it->GetRemoteLockAddr(), sizeof(lock_t)))
          return false;

        // 2. Read via hash
        const HashMeta &meta =
            global_meta_man->GetPrimaryHashMetaWithTableID(it->table_id);
        uint64_t idx = MurmurHash64A(it->key, 0xdeadbeef) % meta.bucket_num;
        offset_t node_off = idx * meta.node_size + meta.base_off;
        auto *local_hash_node =
            (HashNode *)thread_rdma_buffer_alloc->Alloc(sizeof(HashNode));
        if (it->user_insert) {
          pending_next_off_rw.emplace_back(
              InsertOffRead{.qp = re.qp,
                            .item = re.item,
                            .buf = (char *)local_hash_node,
                            .remote_node = re.primary_node_id,
                            .meta = meta,
                            .node_off = node_off});
        } else {
          pending_next_hash_rw.emplace_back(
              HashRead{.qp = re.qp,
                       .item = re.item,
                       .buf = (char *)local_hash_node,
                       .remote_node = re.primary_node_id,
                       .meta = meta});
        }
        if (!coro_sched->RDMARead(coro_id, re.qp, (char *)local_hash_node,
                                  node_off, sizeof(HashNode)))
          return false;
      }
    }
    return true;
  }
*/

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
  unsigned char *redo_log_buffer_buf = nullptr;

  // 处理读回操作的线程
  THD *thd = create_internal_thd();

  node_id_t primary_node_id = MetaManager::get_instance()->GetPrimaryNodeID();
  RCQP *qp = thd->qp_manager->GetRemoteLogBufQPWithNodeID(primary_node_id);
  MetaManager *meta_mgr = MetaManager::get_instance();
};
