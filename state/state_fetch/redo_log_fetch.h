//
// Created by xuyanshi on 1/27/24.
//

#pragma once

#include "state_fetch.h"

StateFetch *stateFetch;

/**
 * @StateReplicate: 把 redo log buffer 从状态层读回来
 * @return
 */
bool redo_log_fetch() {
  if (stateFetch->getFailStatus()) {
    stateFetch->setRedoLogItem(nullptr);
    stateFetch->setRedoLogBufferBuf(nullptr);
  }
  return true;
}

/**
 * TODO: 回放 buffer 中存储的 log，实现状态恢复
 * @return
 */
bool redo_log_replay() { return true; }