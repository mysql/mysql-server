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
  /** Event used for locking sn */
  os_event_t sn_lock_event;

#ifdef UNIV_DEBUG
  /** The rw_lock instance only for the debug info list */
  /* NOTE: Just "rw_lock_t sn_lock_inst;" and direct minimum initialization
  seem to hit the bug of Sun Studio of Solaris. */
  rw_lock_t *sn_lock_inst;
#endif /* UNIV_DEBUG */

  /** Current sn value. Used to reserve space in the redo log,
  and used to acquire an exclusive access to the log buffer.
  Represents number of data bytes that have ever been reserved.
  Bytes of headers and footers of log blocks are not included.
  Its highest bit is used for locking the access to the log buffer. */
  alignas(ut::INNODB_CACHE_LINE_SIZE) atomic_sn_t sn;

  /** Intended sn value while x-locked. */
  atomic_sn_t sn_locked;

  /** Mutex which can be used for x-lock sn value */
  mutable ib_mutex_t sn_x_lock_mutex;
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

#ifdef UNIV_PFS_RWLOCK
  /** The instrumentation hook.
  @remarks This field is rarely modified, so can not be the cause of
  frequent cache line invalidations. However, user threads read it only during
  mtr.commit(), which in some scenarios happens rarely enough, that the cache
  line containing pfs_psi is evicted between mtr.commit()s causing a cache miss,
  a stall and in consequence MACHINE_CLEARS during mtr.commit(). As this miss
  seems inevitable, we at least want to make it really worth it. So, we put the
  pfs_psi in the same cache line which contains buf, buf_size_sn and buf_size,
  which are also needed during mtr.commit(). This way instead of two separate
  cache misses, we have just one.
  TBD: We could additionally use `lfence` to limit MACHINE_CLEARS.*/
  struct PSI_rwlock *pfs_psi;
#endif /* UNIV_PFS_RWLOCK */

  /** The recent written buffer.
  Protected by: locking sn not to add. */
  alignas(ut::INNODB_CACHE_LINE_SIZE) Link_buf<lsn_t> recent_written;

  /** Used for pausing the log writer threads.
  When paused, each user thread should write log as in the former version. */
  std::atomic_bool writer_threads_paused;

  /** Some threads waiting for the ready for write lsn by closer_event. */
  lsn_t current_ready_waiting_lsn;

  /** current_ready_waiting_lsn is waited using this sig_count. */
  int64_t current_ready_waiting_sig_count;

  /** The recent closed buffer.
  Protected by: locking sn not to add. */
  alignas(ut::INNODB_CACHE_LINE_SIZE) Link_buf<lsn_t> recent_closed;
};
