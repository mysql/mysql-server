//
// Created by xuyanshi on 1/25/24.
//

#pragma once

#include "util/common.h"

// log_t 并不是 redo log 本身，而是 redo log buffer 及其相关的信息
// redo log buffer 本质上只是一个 byte 数组，但是为了维护这个 buffer
// 还需要设置很多其他的 meta data，这些 meta data 全部封装在 log_t 结构体中
// https://dev.mysql.com/doc/dev/mysql-server/latest/structlog__t.html#details
// storage/innobase/include/log0sys.h::log_t
struct RedoLogItem {
  /** Aligned log buffer. Committing mini-transactions write there
  redo records, and the log_writer thread writes the log buffer to
  disk in background.
  Protected by: locking sn not to add. */
  alignas(ut::INNODB_CACHE_LINE_SIZE)
      ut::aligned_array_pointer<byte, LOG_BUFFER_ALIGNMENT> buf;

  /** Size of the log buffer expressed in number of data bytes,
  that is excluding bytes for headers and footers of log blocks. */
  atomic_sn_t buf_size_sn;

  /** Size of the log buffer expressed in number of total bytes,
  that is including bytes for headers and footers of log blocks. */
  size_t buf_size;
};
