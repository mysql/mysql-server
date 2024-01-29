//
// Created by xuyanshi on 1/27/24.
//

#ifndef MYSQL_STATE_FETCH_H
#define MYSQL_STATE_FETCH_H

#include "state/state_store/redo_log.h"

class StateFetch {
 public:
  StateFetch() {}

 private:
  bool failStatus = false;

  RedoLogItem *redoLogItem = nullptr;

  unsigned char *redo_log_buffer_buf = nullptr;
};

#endif  // MYSQL_STATE_FETCH_H
