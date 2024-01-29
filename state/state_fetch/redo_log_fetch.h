//
// Created by xuyanshi on 1/27/24.
//

#pragma once

#include "state_fetch.h"

RedoLogItem *redoLogItem;

/**
 * @StateReplicate: 把 redo log buffer 从状态层读回来
 * @return
 */
bool redo_log_fetch() {
  redoLogItem = nullptr;

  return true;
}

/**
 * TODO: 回放 buffer 中存储的 log，实现状态恢复
 * @return
 */
bool redo_log_replay() { return true; }