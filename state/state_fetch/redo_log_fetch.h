//
// Created by xuyanshi on 1/27/24.
//

#pragma once

#include "state/state_store/redo_log.h"

/**
 * @StateReplicate: 定义了 redo log 的状态取回
 */
class RedoLogFetch {
 public:
  RedoLogFetch() {}

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
      this->setRedoLogItem(nullptr);
      this->setRedoLogBufferBuf(nullptr);
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
  unsigned char *redo_log_buffer_buf = nullptr;
};
