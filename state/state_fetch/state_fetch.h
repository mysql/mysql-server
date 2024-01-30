//
// Created by xuyanshi on 1/27/24.
//

#pragma once

#include "state/state_store/redo_log.h"
/**
 * @StateReplicate: 定义了状态取回
 */
class StateFetch {
 public:
  StateFetch() {}

  void setFailStatus(bool status) { failStatus = status; }

  bool getFailStatus() const { return failStatus; }

  void setRedoLogItem(RedoLogItem *item) { redoLogItem = item; }

  RedoLogItem *getRedoLogItem() const { return redoLogItem; }
  
  void setRedoLogBufferBuf(unsigned char *buffer) {
    redo_log_buffer_buf = buffer;
  }
  unsigned char *getRedoLogBufferBuf() const { return redo_log_buffer_buf; }

 private:
  bool failStatus = false;

  // redo log buffer 的元信息
  RedoLogItem *redoLogItem = nullptr;

  // redo log buffer 的实际数据
  unsigned char *redo_log_buffer_buf = nullptr;
};
