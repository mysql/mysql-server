/*****************************************************************************

Copyright (c) 1995, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file mtr/mtr0mtr.cc
 Mini-transaction buffer

 Created 11/26/1995 Heikki Tuuri
 *******************************************************/

#include "mtr0mtr.h"

#include "buf0buf.h"
#include "buf0flu.h"
#include "clone0api.h"
#include "fsp0sysspace.h"
#include "log0meb.h"
#ifndef UNIV_HOTBACKUP
#include "clone0clone.h"
#include "log0buf.h"
#include "log0chkp.h"
#include "log0recv.h"
#include "log0test.h"
#include "log0write.h"
#include "mtr0log.h"
#endif /* !UNIV_HOTBACKUP */
#include "my_dbug.h"
#ifndef UNIV_HOTBACKUP
#include "page0types.h"
#include "trx0purge.h"
#endif /* !UNIV_HOTBACKUP */

static_assert(static_cast<int>(MTR_MEMO_PAGE_S_FIX) ==
                  static_cast<int>(RW_S_LATCH),
              "");

static_assert(static_cast<int>(MTR_MEMO_PAGE_X_FIX) ==
                  static_cast<int>(RW_X_LATCH),
              "");

static_assert(static_cast<int>(MTR_MEMO_PAGE_SX_FIX) ==
                  static_cast<int>(RW_SX_LATCH),
              "");

/** Iterate over a memo block in reverse. */
template <typename Functor>
struct Iterate {
  /** Release specific object */
  explicit Iterate(Functor &functor) : m_functor(functor) { /* Do nothing */
  }

  /** @return false if the functor returns false. */
  bool operator()(mtr_buf_t::block_t *block) {
    const mtr_memo_slot_t *start =
        reinterpret_cast<const mtr_memo_slot_t *>(block->begin());

    mtr_memo_slot_t *slot = reinterpret_cast<mtr_memo_slot_t *>(block->end());

    ut_ad(!(block->used() % sizeof(*slot)));

    while (slot-- != start) {
      if (!m_functor(slot)) {
        return false;
      }
    }

    return true;
  }

  Functor &m_functor;
};

/** Find specific object */
struct Find {
  /** Constructor */
  Find(const void *object, ulint type)
      : m_slot(), m_type(type), m_object(object) {
    ut_a(object != nullptr);
  }

  /** @return false if the object was found. */
  bool operator()(mtr_memo_slot_t *slot) {
    if (m_object == slot->object && m_type == slot->type) {
      m_slot = slot;
      return false;
    }

    return true;
  }

  /** Slot if found */
  mtr_memo_slot_t *m_slot;

  /** Type of the object to look for */
  ulint m_type;

  /** The object instance to look for */
  const void *m_object;
};

/** Find a page frame */
struct Find_page {
  /** Constructor
  @param[in]    ptr     pointer to within a page frame
  @param[in]    flags   MTR_MEMO flags to look for */
  Find_page(const void *ptr, ulint flags)
      : m_ptr(ptr), m_flags(flags), m_slot(nullptr) {
    /* We can only look for page-related flags. */
    ut_ad(!(flags &
            ~(MTR_MEMO_PAGE_S_FIX | MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX |
              MTR_MEMO_BUF_FIX | MTR_MEMO_MODIFY)));
  }

  /** Visit a memo entry.
  @param[in]    slot    memo entry to visit
  @retval       false   if a page was found
  @retval       true    if the iteration should continue */
  bool operator()(mtr_memo_slot_t *slot) {
    ut_ad(m_slot == nullptr);

    if (!(m_flags & slot->type) || slot->object == nullptr) {
      return true;
    }

    buf_block_t *block = reinterpret_cast<buf_block_t *>(slot->object);

    if (m_ptr < block->frame ||
        m_ptr >= block->frame + block->page.size.logical()) {
      return true;
    }

    m_slot = slot;
    return false;
  }

  /** @return the slot that was found */
  mtr_memo_slot_t *get_slot() const {
    ut_ad(m_slot != nullptr);
    return m_slot;
  }
  /** @return the block that was found */
  buf_block_t *get_block() const {
    return reinterpret_cast<buf_block_t *>(get_slot()->object);
  }

 private:
  /** Pointer inside a page frame to look for */
  const void *const m_ptr;
  /** MTR_MEMO flags to look for */
  const ulint m_flags;
  /** The slot corresponding to m_ptr */
  mtr_memo_slot_t *m_slot;
};

struct Mtr_memo_print {
  Mtr_memo_print(std::ostream &out) : m_out(out) {}

  bool operator()(mtr_memo_slot_t *slot) {
    slot->print(m_out);
    return true;
  }

 private:
  std::ostream &m_out;
};

std::ostream &mtr_t::print_memos(std::ostream &out) const {
  Mtr_memo_print printer(out);
  Iterate<Mtr_memo_print> iterator(printer);
  out << "[mtr_t: this=" << (void *)this << ", ";
  m_impl.m_memo.for_each_block_in_reverse(iterator);
  out << "]" << std::endl;
  return out;
}

#ifndef UNIV_HOTBACKUP
#ifdef UNIV_DEBUG
struct Mtr_memo_contains {
  Mtr_memo_contains(const mtr_t *mtr, mtr_memo_type_t type)
      : m_mtr(mtr), m_type(type) {}

  /** Check if the object in the given slot is of the correct type
  and then check if it is contained in the mtr.
  @retval true if the object in the slot is not of required type.
  os is of the required type, but is not contained in the mtr.
  @retval false if the object in the slot is of the required type
                and it is contained in the mtr. */
  bool operator()(mtr_memo_slot_t *slot) {
    if (slot->type != m_type) {
      return true;
    }
    return !mtr_memo_contains(m_mtr, slot->object, m_type);
  }

 private:
  const mtr_t *m_mtr;
  mtr_memo_type_t m_type;
};

bool mtr_t::conflicts_with(const mtr_t *mtr2) const {
  Mtr_memo_contains check(mtr2, MTR_MEMO_MODIFY);
  Iterate<Mtr_memo_contains> iterator(check);

  bool conflict = !m_impl.m_memo.for_each_block_in_reverse(iterator);
  if (conflict) {
    print_memos(std::cout);
    mtr2->print_memos(std::cout);
  }
  return conflict;
}
#endif /* UNIV_DEBUG */
#endif /* !UNIV_HOTBACKUP */

/** Release latches and decrement the buffer fix count.
@param[in]      slot    memo slot */
static void memo_slot_release(mtr_memo_slot_t *slot) {
  switch (slot->type) {
#ifndef UNIV_HOTBACKUP
    buf_block_t *block;
#endif /* !UNIV_HOTBACKUP */

    case MTR_MEMO_BUF_FIX:
    case MTR_MEMO_PAGE_S_FIX:
    case MTR_MEMO_PAGE_SX_FIX:
    case MTR_MEMO_PAGE_X_FIX:
#ifndef UNIV_HOTBACKUP
      block = reinterpret_cast<buf_block_t *>(slot->object);

      buf_page_release_latch(block, slot->type);
      /* The buf_page_release_latch(block,..) call was last action dereferencing
      the `block`, so we can unfix the `block` now, but not sooner.*/
      buf_block_unfix(block);
#endif /* !UNIV_HOTBACKUP */
      break;

    case MTR_MEMO_S_LOCK:
      rw_lock_s_unlock(reinterpret_cast<rw_lock_t *>(slot->object));
      break;

    case MTR_MEMO_SX_LOCK:
      rw_lock_sx_unlock(reinterpret_cast<rw_lock_t *>(slot->object));
      break;

    case MTR_MEMO_X_LOCK:
      rw_lock_x_unlock(reinterpret_cast<rw_lock_t *>(slot->object));
      break;

#ifdef UNIV_DEBUG
    default:
      ut_ad(slot->type == MTR_MEMO_MODIFY);
#endif /* UNIV_DEBUG */
  }

  slot->object = nullptr;
}

/** Release the latches and blocks acquired by the mini-transaction. */
struct Release_all {
  /** @return true always. */
  bool operator()(mtr_memo_slot_t *slot) const {
    if (slot->object != nullptr) {
      memo_slot_release(slot);
    }

    return true;
  }
};

/** Check that all slots have been handled. */
struct Debug_check {
  /** @return true always. */
  bool operator()(const mtr_memo_slot_t *slot) const {
    ut_a(slot->object == nullptr);
    return true;
  }
};

#ifdef UNIV_DEBUG
/** Assure that there are no slots that are latching any resources. Only buffer
 fixing a page is allowed. */
struct Debug_check_no_latching {
  /** @return true always. */
  bool operator()(const mtr_memo_slot_t *slot) const {
    switch (slot->type) {
      case MTR_MEMO_BUF_FIX:
        break;
      default:
        ib::fatal(UT_LOCATION_HERE, ER_MTR_MSG_1, (int)slot->type);
    }
    return true;
  }
};
#endif /* UNIV_DEBUG */

/** Add blocks modified by the mini-transaction to the flush list. */
struct Add_dirty_blocks_to_flush_list {
  /** Constructor.
  @param[in]    start_lsn       LSN of the first entry that was
                                  added to REDO by the MTR
  @param[in]    end_lsn         LSN after the last entry was
                                  added to REDO by the MTR
  @param[in,out]        observer        flush observer */
  Add_dirty_blocks_to_flush_list(lsn_t start_lsn, lsn_t end_lsn,
                                 Flush_observer *observer);

  /** Add the modified page to the buffer flush list. */
  void add_dirty_page_to_flush_list(mtr_memo_slot_t *slot) const {
    ut_ad(m_end_lsn > m_start_lsn || (m_end_lsn == 0 && m_start_lsn == 0));

#ifndef UNIV_HOTBACKUP
    buf_block_t *block = reinterpret_cast<buf_block_t *>(slot->object);
    buf_flush_note_modification(block, m_start_lsn, m_end_lsn,
                                m_flush_observer);
#endif /* !UNIV_HOTBACKUP */
  }

  /** @return true always. */
  bool operator()(mtr_memo_slot_t *slot) const {
    if (slot->object != nullptr) {
      if (slot->type == MTR_MEMO_PAGE_X_FIX ||
          slot->type == MTR_MEMO_PAGE_SX_FIX) {
        add_dirty_page_to_flush_list(slot);

      } else if (slot->type == MTR_MEMO_BUF_FIX) {
        buf_block_t *block;
        block = reinterpret_cast<buf_block_t *>(slot->object);
        if (block->made_dirty_with_no_latch) {
          add_dirty_page_to_flush_list(slot);
          block->made_dirty_with_no_latch = false;
        }
      }
    }

    return true;
  }

  /** Mini-transaction REDO end LSN */
  const lsn_t m_end_lsn;

  /** Mini-transaction REDO start LSN */
  const lsn_t m_start_lsn;

  /** Flush observer */
  Flush_observer *const m_flush_observer;
};

/** Constructor.
@param[in]      start_lsn       LSN of the first entry that was added
                                to REDO by the MTR
@param[in]      end_lsn         LSN after the last entry was added
                                to REDO by the MTR
@param[in,out]  observer        flush observer */
Add_dirty_blocks_to_flush_list::Add_dirty_blocks_to_flush_list(
    lsn_t start_lsn, lsn_t end_lsn, Flush_observer *observer)
    : m_end_lsn(end_lsn), m_start_lsn(start_lsn), m_flush_observer(observer) {
  /* Do nothing */
}

class mtr_t::Command {
 public:
  /** Constructor.
  Takes ownership of the mtr->m_impl, is responsible for deleting it.
  @param[in,out]        mtr     Mini-transaction */
  explicit Command(mtr_t *mtr) : m_locks_released() { init(mtr); }

  void init(mtr_t *mtr) {
    m_impl = &mtr->m_impl;
    m_sync = mtr->m_sync;
  }

  /** Destructor */
  ~Command() { ut_ad(m_impl == nullptr); }

  /** Write the redo log record, add dirty pages to the flush list and
  release the resources. */
  void execute();

  /** Add blocks modified in this mini-transaction to the flush list. */
  void add_dirty_blocks_to_flush_list(lsn_t start_lsn, lsn_t end_lsn);

  /** Release both the latches and blocks used in the mini-transaction. */
  void release_all();

  /** Release the resources */
  void release_resources();

 private:
#ifndef UNIV_HOTBACKUP
  /** Prepare to write the mini-transaction log to the redo log buffer.
  @return number of bytes to write in finish_write() */
  ulint prepare_write();
#endif /* !UNIV_HOTBACKUP */

  /** true if it is a sync mini-transaction. */
  bool m_sync;

  /** The mini-transaction state. */
  mtr_t::Impl *m_impl;

  /** Set to 1 after the user thread releases the latches. The log
  writer thread must wait for this to be set to 1. */
  volatile ulint m_locks_released;
};

/* Mode update matrix. The array is indexed as [old mode][new mode].
All new modes for a specific old mode are in one horizontal line.
true : update to new mode
false: ignore new mode
   A  - MTR_LOG_ALL
   N  - MTR_LOG_NONE
   NR - MTR_LOG_NO_REDO
   S  - MTR_LOG_SHORT_INSERTS */
bool mtr_t::s_mode_update[MTR_LOG_MODE_MAX][MTR_LOG_MODE_MAX] = {
    /*      |  A      N    NR     S  */
    /* A */ {false, true, true, true},   /* A is default and we allow to switch
                                            to all other modes. */
    /* N */ {true, false, true, false},  /* For both A & NR, we can shortly
                                             switch to N and return back*/
    /* NR*/ {false, true, false, false}, /* Default is NR when global redo is
                                            disabled. Allow to move to N */
    /* S */ {true, false, false, false}  /* Only allow return back to A after
                                            short switch from A to S */
};
#ifdef UNIV_DEBUG
/* Mode update validity matrix. The array is indexed as [old mode][new mode]. */
bool mtr_t::s_mode_update_valid[MTR_LOG_MODE_MAX][MTR_LOG_MODE_MAX] = {
    /*      | A      N    NR    S  */
    /* A */ {true, true, true, true}, /* No assert case. */

    /* N */ {true, true, true, true},

    /* NR*/ {true, true, true, true}, /* We generally never return back from
                                         NR to A but need to allow for LOB
                                         restarting B-tree mtr. */

    /* S */ {true, false, false, true} /* Short Insert state is set transiently
                                          and we don't expect N or NR switch. */
};
#endif /* UNIV_DEBUG */

#ifndef UNIV_HOTBACKUP
mtr_t::Logging mtr_t::s_logging;
#endif /* !UNIV_HOTBACKUP */

mtr_log_t mtr_t::set_log_mode(mtr_log_t mode) {
  ut_ad(mode < MTR_LOG_MODE_MAX);

  const auto old_mode = m_impl.m_log_mode;
  ut_ad(s_mode_update_valid[old_mode][mode]);

#ifdef UNIV_DEBUG
  if (mode == MTR_LOG_NO_REDO && old_mode == MTR_LOG_ALL) {
    /* Should change to no redo mode before generating any redo. */
    ut_ad(!has_any_log_record());
  }
#endif /* UNIV_DEBUG */

  if (s_mode_update[old_mode][mode]) {
    m_impl.m_log_mode = mode;
  }

#ifndef UNIV_HOTBACKUP
  /* If we are explicitly setting no logging, this mtr doesn't need
  logging and we can safely unmark it. */
  if (mode == MTR_LOG_NO_REDO && mode == old_mode) {
    check_nolog_and_unmark();
    m_impl.m_log_mode = mode;
  }
#endif /* !UNIV_HOTBACKUP */

  return old_mode;
}

#ifndef UNIV_HOTBACKUP
/** Write the block contents to the REDO log */
struct mtr_write_log_t {
  /** Append a block to the redo log buffer.
  @return whether the appending should continue */
  bool operator()(const mtr_buf_t::block_t *block) {
    lsn_t start_lsn;
    lsn_t end_lsn;
    bool is_last_block = false;

    ut_ad(block != nullptr);

    if (block->used() == 0) {
      return true;
    }

    start_lsn = m_lsn;

    end_lsn =
        log_buffer_write(*log_sys, block->begin(), block->used(), start_lsn);

    ut_a(end_lsn % OS_FILE_LOG_BLOCK_SIZE <
         OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE);

    m_left_to_write -= block->used();

    if (m_left_to_write == 0) {
      is_last_block = true;
      /* This write was up to the end of record group,
      the last record in group has been written.

      Therefore next group of records starts at m_lsn.
      We need to find out, if the next group is the first group,
      that starts in this log block.

      In such case we need to set first_rec_group.

      Now, we could have two cases:
      1. This group of log records has started in previous block
         to block containing m_lsn.
      2. This group of log records has started in the same block
         as block containing m_lsn.

      Only in case 1), the next group of records is the first group
      of log records in block containing m_lsn. */
      if (m_handle.start_lsn / OS_FILE_LOG_BLOCK_SIZE !=
          end_lsn / OS_FILE_LOG_BLOCK_SIZE) {
        log_buffer_set_first_record_group(*log_sys, end_lsn);
      }
    }

    log_buffer_write_completed(*log_sys, start_lsn, end_lsn, is_last_block);

    m_lsn = end_lsn;

    return true;
  }

  Log_handle m_handle;
  lsn_t m_lsn;
  ulint m_left_to_write;
};
#endif /* !UNIV_HOTBACKUP */

#ifdef UNIV_DEBUG
thread_local ut::unordered_set<const mtr_t *> mtr_t::s_my_thread_active_mtrs;
#endif

void mtr_t::start(bool sync) {
  ut_ad(m_impl.m_state == MTR_STATE_INIT ||
        m_impl.m_state == MTR_STATE_COMMITTED);

  UNIV_MEM_INVALID(this, sizeof(*this));
  IF_DEBUG(UNIV_MEM_VALID(&m_restart_count, sizeof(m_restart_count)););

  UNIV_MEM_INVALID(&m_impl, sizeof(m_impl));

  m_sync = sync;

  m_commit_lsn = 0;

  new (&m_impl.m_log) mtr_buf_t();
  new (&m_impl.m_memo) mtr_buf_t();

  m_impl.m_mtr = this;
  m_impl.m_log_mode = MTR_LOG_ALL;
  m_impl.m_inside_ibuf = false;
  m_impl.m_modifications = false;
  m_impl.m_n_log_recs = 0;
  m_impl.m_state = MTR_STATE_ACTIVE;
  m_impl.m_flush_observer = nullptr;
  m_impl.m_marked_nolog = false;

#ifndef UNIV_HOTBACKUP
  check_nolog_and_mark();
#endif /* !UNIV_HOTBACKUP */
  ut_d(m_impl.m_magic_n = MTR_MAGIC_N);

#ifdef UNIV_DEBUG
  auto res = s_my_thread_active_mtrs.insert(this);
  /* Assert there are no collisions in thread local context - it would mean
  reusing MTR without committing or destructing it. */
  ut_a(res.second);
  m_restart_count++;
#endif /* UNIV_DEBUG */
}

#ifndef UNIV_HOTBACKUP
void mtr_t::check_nolog_and_mark() {
  /* Safe check to make this call idempotent. */
  if (m_impl.m_marked_nolog) {
    return;
  }

  size_t shard_index = default_indexer_t<>::get_rnd_index();
  m_impl.m_marked_nolog = s_logging.mark_mtr(shard_index);

  /* Disable redo logging by this mtr if logging is globally off. */
  if (m_impl.m_marked_nolog) {
    ut_ad(m_impl.m_log_mode == MTR_LOG_ALL);
    m_impl.m_log_mode = MTR_LOG_NO_REDO;
    m_impl.m_shard_index = shard_index;
  }
}

void mtr_t::check_nolog_and_unmark() {
  if (m_impl.m_marked_nolog) {
    s_logging.unmark_mtr(m_impl.m_shard_index);

    m_impl.m_marked_nolog = false;
    m_impl.m_shard_index = 0;

    if (m_impl.m_log_mode == MTR_LOG_NO_REDO) {
      /* Reset back to default mode. */
      m_impl.m_log_mode = MTR_LOG_ALL;
    }
  }
}
#endif /* !UNIV_HOTBACKUP */

/** Release the resources */
void mtr_t::Command::release_resources() {
  ut_ad(m_impl->m_magic_n == MTR_MAGIC_N);

  /* Currently only used in commit */
  ut_ad(m_impl->m_state == MTR_STATE_COMMITTING);

#ifdef UNIV_DEBUG
  Debug_check release;
  Iterate<Debug_check> iterator(release);

  m_impl->m_memo.for_each_block_in_reverse(iterator);
#endif /* UNIV_DEBUG */

  /* Reset the mtr buffers */
  m_impl->m_log.erase();

  m_impl->m_memo.erase();

  m_impl->m_state = MTR_STATE_COMMITTED;

  m_impl = nullptr;
}

/** Commit a mini-transaction. */
void mtr_t::commit() {
  ut_ad(is_active());
  ut_ad(!is_inside_ibuf());
  ut_ad(m_impl.m_magic_n == MTR_MAGIC_N);
  m_impl.m_state = MTR_STATE_COMMITTING;

  DBUG_EXECUTE_IF("mtr_commit_crash", DBUG_SUICIDE(););

  Command cmd(this);

  if (has_any_log_record() ||
      (has_modifications() && m_impl.m_log_mode == MTR_LOG_NO_REDO)) {
    ut_ad(!srv_read_only_mode || m_impl.m_log_mode == MTR_LOG_NO_REDO);

    cmd.execute();
  } else {
    cmd.release_all();
    cmd.release_resources();
  }
#ifndef UNIV_HOTBACKUP
  check_nolog_and_unmark();
#endif /* !UNIV_HOTBACKUP */

  ut_d(remove_from_debug_list());
}

#ifdef UNIV_DEBUG
void mtr_t::remove_from_debug_list() const {
  auto it = s_my_thread_active_mtrs.find(this);
  /* We have to find the MTR that is about to be committed in local context. We
  are not sharing MTRs between threads. */
  ut_a(it != s_my_thread_active_mtrs.end());
  s_my_thread_active_mtrs.erase(it);
}

void mtr_t::check_is_not_latching() const {
  Debug_check_no_latching release;
  Iterate<Debug_check_no_latching> iterator(release);

  m_impl.m_memo.for_each_block_in_reverse(iterator);
}
#endif /* UNIV_DEBUG */

#ifndef UNIV_HOTBACKUP

/** Acquire a tablespace X-latch.
NOTE: use mtr_x_lock_space().
@param[in]      space           tablespace instance
@param[in]      location        location from where called */
void mtr_t::x_lock_space(fil_space_t *space, ut::Location location) {
  ut_ad(m_impl.m_magic_n == MTR_MAGIC_N);
  ut_ad(is_active());

  x_lock(&space->latch, location);
}

/** Release an object in the memo stack. */
void mtr_t::memo_release(const void *object, ulint type) {
  ut_ad(m_impl.m_magic_n == MTR_MAGIC_N);
  ut_ad(is_active());

  /* We cannot release a page that has been written to in the
  middle of a mini-transaction. */
  ut_ad(!has_modifications() || type != MTR_MEMO_PAGE_X_FIX);

  Find find(object, type);
  Iterate<Find> iterator(find);

  if (!m_impl.m_memo.for_each_block_in_reverse(iterator)) {
    memo_slot_release(find.m_slot);
  }
}

/** Release a page latch.
@param[in]      ptr     pointer to within a page frame
@param[in]      type    object type: MTR_MEMO_PAGE_X_FIX, ... */
void mtr_t::release_page(const void *ptr, mtr_memo_type_t type) {
  ut_ad(m_impl.m_magic_n == MTR_MAGIC_N);
  ut_ad(is_active());

  /* We cannot release a page that has been written to in the
  middle of a mini-transaction. */
  ut_ad(!has_modifications() || type != MTR_MEMO_PAGE_X_FIX);

  Find_page find(ptr, type);
  Iterate<Find_page> iterator(find);

  if (!m_impl.m_memo.for_each_block_in_reverse(iterator)) {
    memo_slot_release(find.get_slot());
    return;
  }

  /* The page was not found! */
  ut_d(ut_error);
}

/** Prepare to write the mini-transaction log to the redo log buffer.
@return number of bytes to write in finish_write() */
ulint mtr_t::Command::prepare_write() {
  switch (m_impl->m_log_mode) {
    case MTR_LOG_SHORT_INSERTS:
      ut_d(ut_error);
      /* fall through (write no redo log) */
      [[fallthrough]];
    case MTR_LOG_NO_REDO:
    case MTR_LOG_NONE:
      ut_ad(m_impl->m_log.size() == 0);
      return 0;
    case MTR_LOG_ALL:
      break;
    default:
      ut_d(ut_error);
      ut_o(return 0);
  }

  /* An ibuf merge could happen when loading page to apply log
  records during recovery. During the ibuf merge mtr is used. */

  ut_a(!recv_recovery_is_on() || !recv_no_ibuf_operations);

  ulint len = m_impl->m_log.size();
  ut_ad(len > 0);

  const auto n_recs = m_impl->m_n_log_recs;
  ut_ad(n_recs > 0);

  ut_ad(log_sys != nullptr);

  /* This was not the first time of dirtying a
  tablespace since the latest checkpoint. */

  if (n_recs <= 1) {
    ut_ad(n_recs == 1);

    /* Flag the single log record as the
    only record in this mini-transaction. */

    *m_impl->m_log.front()->begin() |= MLOG_SINGLE_REC_FLAG;

  } else {
    /* Because this mini-transaction comprises
    multiple log records, append MLOG_MULTI_REC_END
    at the end. */

    mlog_catenate_ulint(&m_impl->m_log, MLOG_MULTI_REC_END, MLOG_1BYTE);
    ++len;
  }

  ut_ad(m_impl->m_log_mode == MTR_LOG_ALL);
  ut_ad(m_impl->m_log.size() == len);
  ut_ad(len > 0);

  return len;
}
#endif /* !UNIV_HOTBACKUP */

/** Release the latches and blocks acquired by this mini-transaction */
void mtr_t::Command::release_all() {
  Release_all release;
  Iterate<Release_all> iterator(release);

  m_impl->m_memo.for_each_block_in_reverse(iterator);

  /* Note that we have released the latches. */
  m_locks_released = 1;
}

/** Add blocks modified in this mini-transaction to the flush list. */
void mtr_t::Command::add_dirty_blocks_to_flush_list(lsn_t start_lsn,
                                                    lsn_t end_lsn) {
  Add_dirty_blocks_to_flush_list add_to_flush(start_lsn, end_lsn,
                                              m_impl->m_flush_observer);

  Iterate<Add_dirty_blocks_to_flush_list> iterator(add_to_flush);

  m_impl->m_memo.for_each_block_in_reverse(iterator);
}

/** Write the redo log record, add dirty pages to the flush list and release
the resources. */
void mtr_t::Command::execute() {
  ut_ad(m_impl->m_log_mode != MTR_LOG_NONE);

#ifndef UNIV_HOTBACKUP
  ulint len = prepare_write();

  if (len > 0) {
    mtr_write_log_t write_log;

    write_log.m_left_to_write = len;

    auto handle = log_buffer_reserve(*log_sys, len);

    write_log.m_handle = handle;
    write_log.m_lsn = handle.start_lsn;

    m_impl->m_log.for_each_block(write_log);

    ut_ad(write_log.m_left_to_write == 0);
    ut_ad(write_log.m_lsn == handle.end_lsn);

    buf_flush_list_added->wait_to_add(handle.start_lsn);

    DEBUG_SYNC_C("mtr_redo_before_add_dirty_blocks");

    add_dirty_blocks_to_flush_list(handle.start_lsn, handle.end_lsn);

    buf_flush_list_added->report_added(handle.start_lsn, handle.end_lsn);

    m_impl->m_mtr->m_commit_lsn = handle.end_lsn;

  } else {
    DEBUG_SYNC_C("mtr_noredo_before_add_dirty_blocks");

    add_dirty_blocks_to_flush_list(0, 0);
  }
#endif /* !UNIV_HOTBACKUP */

  release_all();
  release_resources();
}

std::ostream &mtr_memo_slot_t::print(std::ostream &out) const {
  buf_block_t *block = nullptr;
  if (!is_lock()) {
    block = reinterpret_cast<buf_block_t *>(object);
  }
  out << "[mtr_memo_slot_t: object=" << object << ", type=" << type << " ("
      << mtr_memo_type(type) << "), page_id="
      << ((block == nullptr) ? page_id_t(FIL_NULL, FIL_NULL)
                             : block->get_page_id())
      << "]" << std::endl;
  return out;
}

#ifndef UNIV_HOTBACKUP
int mtr_t::Logging::enable(THD *thd) {
  if (is_enabled()) {
    return 0;
  }
  /* Allow mtrs to generate redo log. Concurrent clone and redo
  log archiving is still restricted till we reach a recoverable state. */
  ut_ad(m_state.load() == DISABLED);
  m_state.store(ENABLED_RESTRICT);

  /* 1. Wait for all no-log mtrs to finish and add dirty pages to disk.*/
  auto err = wait_no_log_mtr(thd);
  if (err != 0) {
    m_state.store(DISABLED);
    return err;
  }

  /* 2. Wait for dirty pages to flush by forcing checkpoint at current LSN.
  All no-logging page modification are done with the LSN when we stopped
  redo logging. We need to have one write mini-transaction after enabling redo
  to progress the system LSN and take a checkpoint. An easy way is to flush
  the max transaction ID which is generally done at TRX_SYS_TRX_ID_WRITE_MARGIN
  interval but safe to do any time. */
  trx_sys_mutex_enter();
  trx_sys_write_max_trx_id();
  trx_sys_mutex_exit();

  /* It would ensure that the modified page in previous mtr and all other
  pages modified before are flushed to disk. Since there could be large
  number of left over pages from LAD operation, we still don't enable
  double-write at this stage. */
  log_make_latest_checkpoint(*log_sys);
  m_state.store(ENABLED_DBLWR);

  /* 3. Take another checkpoint after enabling double write to ensure any page
  being written without double write are already synced to disk. */
  log_make_latest_checkpoint(*log_sys);

  /* 4. Mark that it is safe to recover from crash. */
  log_persist_enable(*log_sys);

  ib::warn(ER_IB_WRN_REDO_ENABLED);
  m_state.store(ENABLED);

  return 0;
}

int mtr_t::Logging::disable(THD *) {
  if (is_disabled()) {
    return 0;
  }

  /* Disallow archiving to start. */
  ut_ad(m_state.load() == ENABLED);
  m_state.store(ENABLED_RESTRICT);

  /* Check if redo log archiving is active. */
  if (meb::redo_log_archive_is_active()) {
    m_state.store(ENABLED);
    my_error(ER_INNODB_REDO_ARCHIVING_ENABLED, MYF(0));
    return ER_INNODB_REDO_ARCHIVING_ENABLED;
  }

  /* Concurrent clone operation is not supported. */
  Clone_notify notifier(Clone_notify::Type::SYSTEM_REDO_DISABLE,
                        dict_sys_t::s_invalid_space_id, false);
  if (notifier.failed()) {
    m_state.store(ENABLED);
    return notifier.get_error();
  }

  /* Mark that it is unsafe to crash going forward. */
  if (!srv_read_only_mode) {
    /* We need to check for read-only mode, because InnoDB might be
    restarted in read-only mode on log files for which redo logging
    has been disabled just before the crash. During runtime, InnoDB
    refuses to disable redo logging in read-only mode, but in such
    case, we must pretend redo logging is still disabled. It should
    not matter because in read-only, redo logging isn't used anyway.
    However to preserve old behaviour we call s_logging.disable in
    such case and we only prevent from calling log_persist_disable,
    which in the old code was no-op anyway in such case (read-only).*/
    log_persist_disable(*log_sys);
  } else {
    ut_ad(srv_is_being_started);
  }

  ib::warn(ER_IB_WRN_REDO_DISABLED);
  m_state.store(DISABLED);

  return 0;
}

int mtr_t::Logging::wait_no_log_mtr(THD *thd) {
  auto wait_cond = [&](bool, bool &result) {
    if (Counter::total(m_count_nologging_mtr) == 0) {
      result = false;
      return 0;
    }
    result = true;

    if (thd_killed(thd)) {
      my_error(ER_QUERY_INTERRUPTED, MYF(0));
      return ER_QUERY_INTERRUPTED;
    }
    return 0;
  };

  /* Sleep for 1 millisecond */
  Clone_Msec sleep_time(10);
  /* Generate alert message every 5 second. */
  Clone_Sec alert_interval(5);
  /* Wait for 5 minutes. */
  Clone_Sec time_out(Clone_Min(5));

  bool is_timeout = false;
  auto err = Clone_Sys::wait(sleep_time, time_out, alert_interval, wait_cond,
                             nullptr, is_timeout);

  if (err == 0 && is_timeout) {
    my_error(ER_INTERNAL_ERROR, MYF(0),
             "Innodb wait for no-log mtr timed out.");
    err = ER_INTERNAL_ERROR;
    ut_d(ut_error);
  }

  return err;
}

#ifdef UNIV_DEBUG
/** Check if memo contains the given item.
@return true if contains */
bool mtr_t::memo_contains(const mtr_buf_t *memo, const void *object,
                          ulint type) {
  Find find(object, type);
  Iterate<Find> iterator(find);

  return !memo->for_each_block_in_reverse(iterator);
}

/** Debug check for flags */
struct FlaggedCheck {
  FlaggedCheck(const void *ptr, ulint flags) : m_ptr(ptr), m_flags(flags) {
    // Do nothing
  }

  bool operator()(const mtr_memo_slot_t *slot) const {
    if (m_ptr == slot->object && (m_flags & slot->type)) {
      return false;
    }

    return true;
  }

  const void *m_ptr;
  ulint m_flags;
};

/** Check if memo contains the given item.
@param ptr              object to search
@param flags            specify types of object (can be ORred) of
                        MTR_MEMO_PAGE_S_FIX ... values
@return true if contains */
bool mtr_t::memo_contains_flagged(const void *ptr, ulint flags) const {
  ut_ad(m_impl.m_magic_n == MTR_MAGIC_N);
  ut_ad(is_committing() || is_active());

  FlaggedCheck check(ptr, flags);
  Iterate<FlaggedCheck> iterator(check);

  return !m_impl.m_memo.for_each_block_in_reverse(iterator);
}

/** Check if memo contains the given page.
@param[in]      ptr     pointer to within buffer frame
@param[in]      flags   specify types of object with OR of
                        MTR_MEMO_PAGE_S_FIX... values
@return the block
@retval NULL    if not found */
buf_block_t *mtr_t::memo_contains_page_flagged(const byte *ptr,
                                               ulint flags) const {
  Find_page check(ptr, flags);
  Iterate<Find_page> iterator(check);

  return m_impl.m_memo.for_each_block_in_reverse(iterator) ? nullptr
                                                           : check.get_block();
}

/** Mark the given latched page as modified.
@param[in]      ptr     pointer to within buffer frame */
void mtr_t::memo_modify_page(const byte *ptr) {
  buf_block_t *block = memo_contains_page_flagged(
      ptr, MTR_MEMO_PAGE_X_FIX | MTR_MEMO_PAGE_SX_FIX);
  ut_ad(block != nullptr);

  if (!memo_contains(get_memo(), block, MTR_MEMO_MODIFY)) {
    memo_push(block, MTR_MEMO_MODIFY);
  }
}

/** Print info of an mtr handle. */
void mtr_t::print() const {
  ib::info(ER_IB_MSG_1275) << "Mini-transaction handle: memo size "
                           << m_impl.m_memo.size() << " bytes log size "
                           << get_log()->size() << " bytes";
}

lsn_t mtr_commit_mlog_test(size_t payload) {
  constexpr size_t MAX_PAYLOAD_SIZE = 1024;
  ut_a(payload <= MAX_PAYLOAD_SIZE);

  /* Create MLOG_TEST record in the memory. */
  byte record[MLOG_TEST_REC_OVERHEAD + MAX_PAYLOAD_SIZE];

  byte *record_end =
      Log_test::create_mlog_rec(record, 1, MLOG_TEST_VALUE, payload);

  const size_t rec_len = record_end - record;

  mtr_t mtr;
  mtr_start(&mtr);

  /* Copy the created MLOG_TEST to mtr's local buffer. */
  byte *dst = nullptr;
  bool success = mlog_open(&mtr, rec_len, dst);
  ut_a(success);
  std::memcpy(dst, record, rec_len);
  mlog_close(&mtr, dst + rec_len);

  mtr.added_rec();

  ut_ad(mtr.get_expected_log_size() == MLOG_TEST_REC_OVERHEAD + payload);

  mtr_commit(&mtr);

  return mtr.commit_lsn();
}

static void mtr_commit_mlog_test_filling_block_low(log_t &log,
                                                   size_t req_space_left,
                                                   size_t recursive_level) {
  ut_a(req_space_left <= LOG_BLOCK_DATA_SIZE);

  /* Compute how much free space we have in current log block. */
  const lsn_t current_lsn = log_get_lsn(log);
  size_t cur_space_left = OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_TRL_SIZE -
                          current_lsn % OS_FILE_LOG_BLOCK_SIZE;

  /* Subtract minimum space required for a single MLOG_TEST. */
  if (cur_space_left < MLOG_TEST_REC_OVERHEAD) {
    /* Even the smallest MLOG_TEST was not fitting the left space,
    so we will need to use the next log block too. */
    cur_space_left += LOG_BLOCK_DATA_SIZE;
  }
  cur_space_left -= MLOG_TEST_REC_OVERHEAD;

  /* Compute how big payload is required to leave exactly the provided
  req_space_left bytes free in last block. */
  size_t payload;
  if (cur_space_left < req_space_left) {
    /* We requested to leave more free bytes, than we have currently
    in the last block, we need to use the next log block. */
    payload = cur_space_left + LOG_BLOCK_DATA_SIZE - req_space_left;
  } else {
    payload = cur_space_left - req_space_left;
  }

  /* Check if size of the record fits the maximum allowed size, which
  is defined by the dyn_buf_t used in mtr_t (mtr_buf_t). */

  if (MLOG_TEST_REC_OVERHEAD + payload <= mtr_buf_t::MAX_DATA_SIZE) {
    mtr_commit_mlog_test(payload);
  } else {
    /* It does not fit, so we need to write as much as possible here,
    but keep in mind that next record will need to take at least
    MLOG_TEST_REC_OVERHEAD bytes. Fortunately the MAX_DATA_SIZE is
    always at least twice larger than the MLOG_TEST_REC_OVERHEAD,
    so the payload has to be larger than MLOG_TEST_REC_OVERHEAD. */
    static_assert(mtr_buf_t::MAX_DATA_SIZE >= MLOG_TEST_REC_OVERHEAD * 2);
    ut_a(payload > MLOG_TEST_REC_OVERHEAD);

    /* Subtract space which we will occupy by usage of next record.
    The remaining space is maximum we are allowed to occupy within
    this record. */
    payload -= MLOG_TEST_REC_OVERHEAD;

    if (MLOG_TEST_REC_OVERHEAD + payload > mtr_buf_t::MAX_DATA_SIZE) {
      /* We still cannot fit mtr_buf_t::MAX_DATA_SIZE bytes, so write
      as much as possible within this record. */
      payload = mtr_buf_t::MAX_DATA_SIZE - MLOG_TEST_REC_OVERHEAD;
    }

    /* Write this MLOG_TEST record. */
    mtr_commit_mlog_test(payload);

    /* Compute upper bound for maximum level of recursion that is ever possible.
    This is to verify the guarantee that we don't go to deep.

    We do not want to depend on actual difference between the
    mtr_buf_t::MAX_DATA_SIZE and LOG_BLOCK_DATA_SIZE.

    Note that mtr_buf_t::MAX_DATA_SIZE is the maximum size of log record we
    could add. The LOG_BLOCK_DATA_SIZE consists of LOG_BLOCK_DATA_SIZE /
    mtr_buf_t::MAX_DATA_SIZE records of mtr_buf_t::MAX_DATA_SIZE size each (0 if
    MAX_DATA_SIZE is larger than the LOG_BLOCK_DATA_SIZE). If we shifted these
    records then possibly 2 more records are required at boundaries (beginning
    and ending) to cover the whole range. If the last record would not end at
    proper offset, we decrease its payload. If we needed to move its end to even
    smaller offset from beginning of log block than we reach with payload=0,
    then we subtract up to MLOG_TEST_REC_OVERHEAD bytes from payload of previous
    record, which is always possible because:
      MAX_DATA_SIZE - MLOG_TEST_REC_OVERHEAD >= MLOG_TEST_REC_OVERHEAD.

    If the initial free space minus MLOG_TEST_REC_OVERHEAD is smaller than the
    requested free space, then we need to move forward by at most
    LOG_BLOCK_DATA_SIZE bytes. For that we need at most LOG_BLOCK_DATA_SIZE /
    mtr_buf_t::MAX_DATA_SIZE + 2 records shifted in the way described above.

    This solution is reached by the loop of writing MAX_DATA_SIZE records until
    the distance to target is <= MAX_DATA_SIZE + MLOG_TEST_REC_OVERHEAD, in
    which case we adjust size of next record to end it exactly
    MLOG_TEST_REC_OVERHEAD bytes before the target (this is why we subtract
    MLOG_TEST_REC_OVERHEAD from payload). Then next recursive call will have an
    easy task of adding record with payload=0. The loop mentioned above is
    implemented by the recursion. */
    constexpr auto MAX_REC_N =
        LOG_BLOCK_DATA_SIZE / mtr_buf_t::MAX_DATA_SIZE + 2;

    ut_a(recursive_level + 1 <= MAX_REC_N);

    /* Write next MLOG_TEST record(s). */
    mtr_commit_mlog_test_filling_block_low(log, req_space_left,
                                           recursive_level + 1);
  }
}

void mtr_commit_mlog_test_filling_block(log_t &log, size_t req_space_left) {
  mtr_commit_mlog_test_filling_block_low(log, req_space_left, 1);
}

void mtr_t::wait_for_flush() {
  ut_ad(commit_lsn() > 0);
  log_write_up_to(*log_sys, commit_lsn(), true);
}

#endif /* UNIV_DEBUG */
#endif /* !UNIV_HOTBACKUP */
