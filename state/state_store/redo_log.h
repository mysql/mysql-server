//
// Created by xuyanshi on 1/25/24.
//

#pragma once

#include "util/common.h"

struct RedoLogItem {
  // TODO: log_t 并不是 redo log 本身，而是 redo log buffer 及其相关的信息！！
  // redo log buffer 本质上只是一个 byte 数组，但是为了维护这个 buffer
  // 还需要设置很多其他的 meta data，这些 meta data 全部封装在 log_t 结构体中

  // https://dev.mysql.com/doc/dev/mysql-server/latest/structlog__t.html#details
  // storage/innobase/include/log0sys.h::log_t
};
