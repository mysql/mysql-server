/*****************************************************************************

Copyright (c) 1995, 2023, Oracle and/or its affiliates.
Copyright (c) 2012, Facebook Inc.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/mtr0mtr.h
 Mini-transaction buffer

 Created 11/26/1995 Heikki Tuuri
 *******************************************************/

#ifndef mtr0mtr_h
#define mtr0mtr_h

#include <stddef.h>

#include "univ.i"

#include "buf0types.h"
#include "dyn0buf.h"
#include "fil0fil.h"
#include "mtr0types.h"
#include "srv0srv.h"
#include "trx0types.h"

/** Start a mini-transaction. */
#define mtr_start(m) (m)->start()

/** Start a synchronous mini-transaction */
#define mtr_start_sync(m) (m)->start(true)

/** Start an asynchronous read-only mini-transaction */
#define mtr_start_ro(m) (m)->start(true, true)

/** Commit a mini-transaction. */
#define mtr_commit(m) (m)->commit()

/** Set and return a savepoint in mtr.
@return savepoint */
#define mtr_set_savepoint(m) (m)->get_savepoint()

/** Release the (index tree) s-latch stored in an mtr memo after a
savepoint. */
#define mtr_release_s_latch_at_savepoint(m, s, l) \
  (m)->release_s_latch_at_savepoint((s), (l))

/** Get the logging mode of a mini-transaction.
@return logging mode: MTR_LOG_NONE, ... */
#define mtr_get_log_mode(m) (m)->get_log_mode()

/** Change the logging mode of a mini-transaction.
@return old mode */
#define mtr_set_log_mode(m, d) (m)->set_log_mode((d))

/** Get the flush observer of a mini-transaction.
@return flush observer object */
#define mtr_get_flush_observer(m) (m)->get_flush_observer()

/** Set the flush observer of a mini-transaction. */
#define mtr_set_flush_observer(m, d) (m)->set_flush_observer((d))

/** Read 1 - 4 bytes from a file page buffered in the buffer pool.
@return value read */
#define mtr_read_ulint(p, t, m) (m)->read_ulint((p), (t))

/** Release an object in the memo stack.
@return true if released */
#define mtr_memo_release(m, o, t) (m)->memo_release((o), (t))

#ifdef UNIV_DEBUG

/** Check if memo contains the given item ignore if table is intrinsic
@return true if contains or table is intrinsic. */
#define mtr_is_block_fix(m, o, t, table) \
  (mtr_memo_contains(m, o, t) || table->is_intrinsic())

/** Check if memo contains the given page ignore if table is intrinsic
@return true if contains or table is intrinsic. */
#define mtr_is_page_fix(m, p, t, table) \
  (mtr_memo_contains_page(m, p, t) || table->is_intrinsic())

/** Check if memo contains the given item.
@return true if contains */
#define mtr_memo_contains(m, o, t) (m)->memo_contains((m)->get_memo(), (o), (t))

/** Check if memo contains the given page.
@return true if contains */
#define mtr_memo_contains_page(m, p, t) \
  (m)->memo_contains_page_flagged((p), (t))
#endif /* UNIV_DEBUG */

/** Print info of an mtr handle. */
#define mtr_print(m) (m)->print()

/** Return the log object of a mini-transaction buffer.
@return log */
#define mtr_get_log(m) (m)->get_log()

/** Push an object to an mtr memo stack. */
#define mtr_memo_push(m, o, t) (m)->memo_push(o, t)

/** Lock an rw-lock in s-mode. */
#define mtr_s_lock(l, m, loc) (m)->s_lock((l), loc)

/** Lock an rw-lock in x-mode. */
#define mtr_x_lock(l, m, loc) (m)->x_lock((l), loc)

/** Lock a tablespace in x-mode. */
#define mtr_x_lock_space(s, m) (m)->x_lock_space((s), UT_LOCATION_HERE)

/** Lock an rw-lock in sx-mode. */
#define mtr_sx_lock(l, m, loc) (m)->sx_lock((l), loc)

#define mtr_memo_contains_flagged(m, p, l) (m)->memo_contains_flagged((p), (l))

#define mtr_memo_contains_page_flagged(m, p, l) \
  (m)->memo_contains_page_flagged((p), (l))

#define mtr_release_block_at_savepoint(m, s, b) \
  (m)->release_block_at_savepoint((s), (b))

#define mtr_block_sx_latch_at_savepoint(m, s, b) \
  (m)->sx_latch_at_savepoint((s), (b))

#define mtr_block_x_latch_at_savepoint(m, s, b) \
  (m)->x_latch_at_savepoint((s), (b))

/** Forward declaration of a tablespace object */
struct fil_space_t;

/** Mini-transaction memo stack slot. */
struct mtr_memo_slot_t {
  /** Pointer to the object - either buf_block_t or rw_lock_t */
  void *object;

  /** type of the stored object (MTR_MEMO_S_LOCK, ...) */
  ulint type;

  /** Check if the object stored in this slot is a lock (rw_lock_t).
  @return true if it is a lock object, false otherwise. */
  bool is_lock() const {
    return type == MTR_MEMO_S_LOCK || type == MTR_MEMO_X_LOCK ||
           type == MTR_MEMO_SX_LOCK;
  }

  std::ostream &print(std::ostream &out) const;
};

inline std::ostream &operator<<(std::ostream &out, const mtr_memo_slot_t &obj) {
  return obj.print(out);
}

/** Mini-transaction handle and buffer */
struct mtr_t {
  /** State variables of the mtr */
  struct Impl {
    /** memo stack for locks etc. */
    mtr_buf_t m_memo;

    /** mini-transaction log */
    mtr_buf_t m_log;

    /** true if inside ibuf changes */
    bool m_inside_ibuf;

    /** true if the mini-transaction might have modified buffer pool pages */
    bool m_modifications;

    /** true if mtr is forced to NO_LOG mode because redo logging is
    disabled globally. In this case, mtr increments the global counter
    at ::start and must decrement it back at ::commit. */
    bool m_marked_nolog;

    /** Shard index used for incrementing global counter at ::start. We need
    to use the same shard while decrementing counter at ::commit. */
    size_t m_shard_index;

    /** Count of how many page initial log records have been
    written to the mtr log */
    uint32_t m_n_log_recs;

    /** specifies which operations should be logged; default
    value MTR_LOG_ALL */
    mtr_log_t m_log_mode;

    /** State of the transaction */
    mtr_state_t m_state;

    /** Flush Observer */
    Flush_observer *m_flush_observer;

#ifdef UNIV_DEBUG
    /** For checking corruption. */
    ulint m_magic_n;

#endif /* UNIV_DEBUG */

    /** Owning mini-transaction */
    mtr_t *m_mtr;
  };

#ifndef UNIV_HOTBACKUP
  /** mtr global logging */
  class Logging {
   public:
    /** mtr global redo logging state.
    Enable Logging  :
    [ENABLED] -> [ENABLED_RESTRICT] -> [DISABLED]

    Disable Logging :
    [DISABLED] -> [ENABLED_RESTRICT] -> [ENABLED_DBLWR] -> [ENABLED] */

    enum State : uint32_t {
      /* Redo Logging is enabled. Server is crash safe. */
      ENABLED,
      /* Redo logging is enabled. All non-logging mtr are finished with the
      pages flushed to disk. Double write is enabled. Some pages could be
      still getting written to disk without double-write. Not safe to crash. */
      ENABLED_DBLWR,
      /* Redo logging is enabled but there could be some mtrs still running
      in no logging mode. Redo archiving and clone are not allowed to start.
      No double-write */
      ENABLED_RESTRICT,
      /* Redo logging is disabled and all new mtrs would not generate any redo.
      Redo archiving and clone are not allowed. */
      DISABLED
    };

    /** Initialize logging state at server start up. */
    void init() {
      m_state.store(ENABLED);
      /* We use sharded counter and force sequentially consistent counting
      which is the general default for c++ atomic operation. If we try to
      optimize it further specific to current operations, we could use
      Release-Acquire ordering i.e. std::memory_order_release during counting
      and std::memory_order_acquire while checking for the count. However,
      sharding looks to be good enough for now and we should go for non default
      memory ordering only with some visible proof for improvement. */
      m_count_nologging_mtr.set_order(std::memory_order_seq_cst);
      Counter::clear(m_count_nologging_mtr);
    }

    /** Disable mtr redo logging. Server is crash unsafe without logging.
    @param[in]  thd     server connection THD
    @return mysql error code. */
    int disable(THD *thd);

    /** Enable mtr redo logging. Ensure that the server is crash safe
    before returning.
    @param[in]  thd     server connection THD
    @return mysql error code. */
    int enable(THD *thd);

    /** Mark a no-logging mtr to indicate that it would not generate redo log
    and system is crash unsafe.
    @return true iff logging is disabled and mtr is marked. */
    bool mark_mtr(size_t index) {
      /* Have initial check to avoid incrementing global counter for regular
      case when redo logging is enabled. */
      if (is_disabled()) {
        /* Increment counter to restrict state change DISABLED to ENABLED. */
        Counter::inc(m_count_nologging_mtr, index);

        /* Check if the no-logging is still disabled. At this point, if we
        find the state disabled, it is no longer possible for the state move
        back to enabled till the mtr finishes and we unmark the mtr. */
        if (is_disabled()) {
          return (true);
        }
        Counter::dec(m_count_nologging_mtr, index);
      }
      return (false);
    }

    /** unmark a no logging mtr. */
    void unmark_mtr(size_t index) {
      ut_ad(!is_enabled());
      ut_ad(Counter::total(m_count_nologging_mtr) > 0);
      Counter::dec(m_count_nologging_mtr, index);
    }

    /* @return flush loop count for faster response when logging is disabled. */
    uint32_t get_nolog_flush_loop() const { return (NOLOG_MAX_FLUSH_LOOP); }

    /** @return true iff redo logging is enabled and server is crash safe. */
    bool is_enabled() const { return (m_state.load() == ENABLED); }

    /** @return true iff redo logging is disabled and new mtrs are not going
    to generate redo log. */
    bool is_disabled() const { return (m_state.load() == DISABLED); }

    /** @return true iff we can skip data page double write. */
    bool dblwr_disabled() const {
      auto state = m_state.load();
      return (state == DISABLED || state == ENABLED_RESTRICT);
    }

    /* Force faster flush loop for quicker adaptive flush response when logging
    is disabled. When redo logging is disabled the system operates faster with
    dirty pages generated at much faster rate. */
    static constexpr uint32_t NOLOG_MAX_FLUSH_LOOP = 5;

   private:
    /** Wait till all no-logging mtrs are finished.
    @return mysql error code. */
    int wait_no_log_mtr(THD *thd);

   private:
    /** Global redo logging state. */
    std::atomic<State> m_state;

    using Shards = Counter::Shards<128>;

    /** Number of no logging mtrs currently running. */
    Shards m_count_nologging_mtr;
  };

  /** Check if redo logging is disabled globally and mark
  the global counter till mtr ends. */
  void check_nolog_and_mark();

  /** Check if the mtr has marked the global no log counter and
  unmark it. */
  void check_nolog_and_unmark();
#endif /* !UNIV_HOTBACKUP */

  mtr_t() {
    m_impl.m_state = MTR_STATE_INIT;
    m_impl.m_marked_nolog = false;
    m_impl.m_shard_index = 0;
  }

  ~mtr_t() {
#ifdef UNIV_DEBUG
    switch (m_impl.m_state) {
      case MTR_STATE_ACTIVE:
        ut_ad(m_impl.m_memo.size() == 0);
        ut_d(remove_from_debug_list());
        break;
      case MTR_STATE_INIT:
      case MTR_STATE_COMMITTED:
        break;
      case MTR_STATE_COMMITTING:
        ut_error;
    }
#endif /* UNIV_DEBUG */
#ifndef UNIV_HOTBACKUP
    /* Safety check in case mtr is not committed. */
    if (m_impl.m_state != MTR_STATE_INIT) {
      check_nolog_and_unmark();
    }
#endif /* !UNIV_HOTBACKUP */
  }

#ifdef UNIV_DEBUG
  /** Removed the MTR from the s_my_thread_active_mtrs list. */
  void remove_from_debug_list() const;

  /** Assure that there are no slots that are latching any resources. Only
  buffer fixing a page is allowed. */
  void check_is_not_latching() const;
#endif /* UNIV_DEBUG */

  /** Start a mini-transaction.
  @param sync           true if it is a synchronous mini-transaction */
  void start(bool sync = true);

  /** @return whether this is an asynchronous mini-transaction. */
  bool is_async() const { return (!m_sync); }

  /** Request a future commit to be synchronous. */
  void set_sync() { m_sync = true; }

  /** Commit the mini-transaction. */
  void commit();

  /** Return current size of the buffer.
  @return       savepoint */
  [[nodiscard]] ulint get_savepoint() const {
    ut_ad(is_active());
    ut_ad(m_impl.m_magic_n == MTR_MAGIC_N);

    return (m_impl.m_memo.size());
  }

  /** Release the (index tree) s-latch stored in an mtr memo after a
  savepoint.
  @param savepoint      value returned by @see set_savepoint.
  @param lock           latch to release */
  inline void release_s_latch_at_savepoint(ulint savepoint, rw_lock_t *lock);

  /** Release the block in an mtr memo after a savepoint. */
  inline void release_block_at_savepoint(ulint savepoint, buf_block_t *block);

  /** SX-latch a not yet latched block after a savepoint. */
  inline void sx_latch_at_savepoint(ulint savepoint, buf_block_t *block);

  /** X-latch a not yet latched block after a savepoint. */
  inline void x_latch_at_savepoint(ulint savepoint, buf_block_t *block);

  /** Get the logging mode.
  @return       logging mode */
  [[nodiscard]] inline mtr_log_t get_log_mode() const;

  /** Change the logging mode.
  @param mode    logging mode
  @return       old mode */
  mtr_log_t set_log_mode(mtr_log_t mode);

  /** Read 1 - 4 bytes from a file page buffered in the buffer pool.
  @param ptr    pointer from where to read
  @param type   MLOG_1BYTE, MLOG_2BYTES, MLOG_4BYTES
  @return       value read */
  [[nodiscard]] inline uint32_t read_ulint(const byte *ptr,
                                           mlog_id_t type) const;

  /** Locks a rw-latch in S mode.
  NOTE: use mtr_s_lock().
  @param lock   rw-lock
  @param location location from where called */
  inline void s_lock(rw_lock_t *lock, ut::Location location);

  /** Locks a rw-latch in X mode.
  NOTE: use mtr_x_lock().
  @param lock   rw-lock
  @param location location where name from where called */
  inline void x_lock(rw_lock_t *lock, ut::Location location);

  /** Locks a rw-latch in X mode.
  NOTE: use mtr_sx_lock().
  @param lock   rw-lock
  @param location location from where called */
  inline void sx_lock(rw_lock_t *lock, ut::Location location);

  /** Acquire a tablespace X-latch.
  NOTE: use mtr_x_lock_space().
  @param[in]    space           tablespace instance
  @param[in]    location        location from where called */
  void x_lock_space(fil_space_t *space, ut::Location location);

  /** Release an object in the memo stack.
  @param object object
  @param type   object type: MTR_MEMO_S_LOCK, ... */
  void memo_release(const void *object, ulint type);

  /** Release a page latch.
  @param[in]    ptr     pointer to within a page frame
  @param[in]    type    object type: MTR_MEMO_PAGE_X_FIX, ... */
  void release_page(const void *ptr, mtr_memo_type_t type);

  /** Note that the mini-transaction might have modified a buffer pool page.
  As it's called from mlog_open(), which is called from fil_op_write_log() and
  perhaps other places which do not modify any page, this can be a false
  positive. */
  void set_modified() { m_impl.m_modifications = true; }

  /** Checks if this mtr has modified any buffer pool page.
  It errs on the safe side: may return true even if it didn't modify any page.
  This is used in MTR_LOG_NO_REDO mode to detect that pages should be added to
  flush lists during commit() even though no redo log will be produced.
  @return true if the mini-transaction might have modified buffer pool pages. */
  [[nodiscard]] bool has_modifications() const {
    return m_impl.m_modifications;
  }

  /** Get the LSN of commit().
  @return the commit LSN
  @retval 0 if the transaction only modified temporary tablespaces or logging
  is disabled globally. */
  [[nodiscard]] lsn_t commit_lsn() const {
    ut_ad(has_committed());
    ut_ad(m_impl.m_log_mode == MTR_LOG_ALL);
    return (m_commit_lsn);
  }

  /** Note that we are inside the change buffer code. */
  void enter_ibuf() { m_impl.m_inside_ibuf = true; }

  /** Note that we have exited from the change buffer code. */
  void exit_ibuf() { m_impl.m_inside_ibuf = false; }

  /** @return true if we are inside the change buffer code */
  [[nodiscard]] bool is_inside_ibuf() const { return (m_impl.m_inside_ibuf); }

  /*
  @return true if the mini-transaction is active */
  [[nodiscard]] bool is_active() const {
    return (m_impl.m_state == MTR_STATE_ACTIVE);
  }

  /** Get flush observer
  @return flush observer */
  [[nodiscard]] Flush_observer *get_flush_observer() const {
    return (m_impl.m_flush_observer);
  }

  /** Set flush observer
  @param[in]    observer        flush observer */
  void set_flush_observer(Flush_observer *observer) {
    ut_ad(observer == nullptr || m_impl.m_log_mode == MTR_LOG_NO_REDO);

    m_impl.m_flush_observer = observer;
  }

  /** Print the memo objects (mtr_memo_slot_t) of mtr_t to the given output
  stream.
  @param[in]   out   the output stream for printing
  @return the output stream.  */
  std::ostream &print_memos(std::ostream &out) const;

#ifdef UNIV_DEBUG
  /** Check if memo contains the given item.
  @param memo   memo stack
  @param object object to search
  @param type   type of object
  @return       true if contains */
  [[nodiscard]] static bool memo_contains(const mtr_buf_t *memo,
                                          const void *object, ulint type);

  /** Check if memo contains the given item.
  @param ptr            object to search
  @param flags          specify types of object (can be ORred) of
                          MTR_MEMO_PAGE_S_FIX ... values
  @return true if contains */
  [[nodiscard]] bool memo_contains_flagged(const void *ptr, ulint flags) const;

  /** Check if memo contains the given page.
  @param[in]    ptr     pointer to within buffer frame
  @param[in]    flags   specify types of object with OR of
                          MTR_MEMO_PAGE_S_FIX... values
  @return       the block
  @retval       NULL    if not found */
  [[nodiscard]] buf_block_t *memo_contains_page_flagged(const byte *ptr,
                                                        ulint flags) const;

  /** Mark the given latched page as modified.
  @param[in]    ptr     pointer to within buffer frame */
  void memo_modify_page(const byte *ptr);

  /** Print info of an mtr handle. */
  void print() const;

  /** @return true if the mini-transaction has committed */
  [[nodiscard]] bool has_committed() const {
    return (m_impl.m_state == MTR_STATE_COMMITTED);
  }

  /** @return true if the mini-transaction is committing */
  bool is_committing() const {
    return (m_impl.m_state == MTR_STATE_COMMITTING);
  }

  /** Check if the changes done in this mtr conflicts with changes done
  in the given mtr.  Two mtrs are said to conflict with each other, if
  they modify the same buffer block.
  @param[in]   mtr2  the given mtr.
  @return true if there is conflict, false otherwise. */
  [[nodiscard]] bool conflicts_with(const mtr_t *mtr2) const;

  /** @return the memo stack */
  [[nodiscard]] const mtr_buf_t *get_memo() const { return (&m_impl.m_memo); }

  /** @return the memo stack */
  [[nodiscard]] mtr_buf_t *get_memo() { return (&m_impl.m_memo); }

  /** Computes the number of bytes that would be written to the redo
  log if mtr was committed right now (excluding headers of log blocks).
  @return  number of bytes of the collected log records increased
           by 1 if MLOG_MULTI_REC_END would already be required */
  size_t get_expected_log_size() const {
    return (m_impl.m_log.size() + (m_impl.m_n_log_recs > 1 ? 1 : 0));
  }

  void wait_for_flush();
#endif /* UNIV_DEBUG */

  /** Note that a record has been added to the log */
  void added_rec() { ++m_impl.m_n_log_recs; }

  /** Checks if this mtr has generated any redo log records which should be
  written to the redo log during commit().
  Note: If redo logging is disabled by set_log_mode(MTR_LOG_NONE) or
  set_log_mode(MTR_LOG_NO_REDO) or globally by s_logging.disable(..), then it
  will return false, even if set_modified() was called.
  Note: Redo log records can be generated for things other than page
  modifications, for example for tablespace rename, or other metadata updates.
  Note: Redo log records can be generated for modifications of pages which were
  already marked as dirty in BP.
  @return true iff there is at least one redo log record generated */
  bool has_any_log_record() { return 0 < m_impl.m_n_log_recs; }

  /** Get the buffered redo log of this mini-transaction.
  @return       redo log */
  [[nodiscard]] const mtr_buf_t *get_log() const {
    ut_ad(m_impl.m_magic_n == MTR_MAGIC_N);

    return (&m_impl.m_log);
  }

  /** Get the buffered redo log of this mini-transaction.
  @return       redo log */
  [[nodiscard]] mtr_buf_t *get_log() {
    ut_ad(m_impl.m_magic_n == MTR_MAGIC_N);

    return (&m_impl.m_log);
  }

  /** Push an object to an mtr memo stack.
  @param object object
  @param type   object type: MTR_MEMO_S_LOCK, ... */
  inline void memo_push(void *object, mtr_memo_type_t type);

#ifdef UNIV_DEBUG
  /** Iterate all MTRs created in this thread to assure they are not latching
  any resources. Violating this could lead to deadlocks under
  log_free_check(). */
  static void check_my_thread_mtrs_are_not_latching() {
    for (auto &it : s_my_thread_active_mtrs) {
      it->check_is_not_latching();
    }
  }
#endif

  /** Matrix to check if a mode update request should be ignored. */
  static bool s_mode_update[MTR_LOG_MODE_MAX][MTR_LOG_MODE_MAX];

#ifdef UNIV_DEBUG
  /** For checking invalid mode update requests. */
  static bool s_mode_update_valid[MTR_LOG_MODE_MAX][MTR_LOG_MODE_MAX];

  /** Count the number of times the same mtr object has been committed and
  restarted. */
  size_t m_restart_count{};
#endif /* UNIV_DEBUG */

#ifndef UNIV_HOTBACKUP
  /** Instance level logging information for all mtrs. */
  static Logging s_logging;
#endif /* !UNIV_HOTBACKUP */

 private:
  Impl m_impl;

  /** LSN at commit time */
  lsn_t m_commit_lsn;

  /** true if it is synchronous mini-transaction */
  bool m_sync;

#ifdef UNIV_DEBUG
  /** List of all non-committed MTR instances created in this thread. Used for
  debug purposes in the log_free_check(). */
  static thread_local ut::unordered_set<const mtr_t *> s_my_thread_active_mtrs;
#endif

  class Command;

  friend class Command;
};

#ifndef UNIV_HOTBACKUP
#ifdef UNIV_DEBUG

/** Reserves space in the log buffer and writes a single MLOG_TEST.
@param[in]      payload  number of extra bytes within the record,
                         not greater than 1024
@return end_lsn pointing to the first byte after the written record */
lsn_t mtr_commit_mlog_test(size_t payload = 0);

/** Reserves space in the log buffer and writes a single MLOG_TEST.
Adjusts size of the payload in the record, in order to fill the current
block up to its boundary. If nothing else is happening in parallel,
we could expect to see afterwards:
(cur_lsn + space_left) % OS_FILE_LOG_BLOCK_SIZE == LOG_BLOCK_HDR_SIZE,
where cur_lsn = log_get_lsn(log).
@param[in,out]  log         redo log
@param[in]      space_left  extra bytes left to the boundary of block,
                            must be not greater than 496 */
void mtr_commit_mlog_test_filling_block(log_t &log, size_t space_left = 0);

#endif /* UNIV_DEBUG */
#endif /* !UNIV_HOTBACKUP */

#include "mtr0mtr.ic"

#endif /* mtr0mtr_h */
