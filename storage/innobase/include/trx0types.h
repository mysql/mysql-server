/*****************************************************************************

Copyright (c) 1996, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file include/trx0types.h
 Transaction system global type definitions

 Created 3/26/1996 Heikki Tuuri
 *******************************************************/

#ifndef trx0types_h
#define trx0types_h

#include "page0size.h"
#include "sync0rw.h"
#include "ut0byte.h"
#include "ut0mutex.h"
#include "ut0new.h"

#include <atomic>
#include <queue>
#include <set>
#include <vector>

//#include <unordered_set>

/** printf(3) format used for printing DB_TRX_ID and other system fields */
#define TRX_ID_FMT IB_ID_FMT

/** maximum length that a formatted trx_t::id could take, not including
the terminating NUL character. */
static const ulint TRX_ID_MAX_LEN = 17;

/** Space id of the transaction system page (the system tablespace) */
static const space_id_t TRX_SYS_SPACE = 0;

/** Page number of the transaction system page */
#define TRX_SYS_PAGE_NO FSP_TRX_SYS_PAGE_NO

/** Random value to check for corruption of trx_t */
static const ulint TRX_MAGIC_N = 91118598;

/** If this flag is set then the transaction cannot be rolled back
asynchronously. */
static const ib_uint32_t TRX_FORCE_ROLLBACK_DISABLE = 1 << 29;

/** Was the transaction rolled back asynchronously or by the
owning thread. This flag is relevant only if TRX_FORCE_ROLLBACK
is set.  */
static const ib_uint32_t TRX_FORCE_ROLLBACK_ASYNC = 1 << 30;

/** Mark the transaction for forced rollback */
static const ib_uint32_t TRX_FORCE_ROLLBACK = 1 << 31;

/** For masking out the above four flags */
static const ib_uint32_t TRX_FORCE_ROLLBACK_MASK = 0x1FFFFFFF;

/** Transaction execution states when trx->state == TRX_STATE_ACTIVE */
enum trx_que_t {
  TRX_QUE_RUNNING,      /*!< transaction is running */
  TRX_QUE_LOCK_WAIT,    /*!< transaction is waiting for
                        a lock */
  TRX_QUE_ROLLING_BACK, /*!< transaction is rolling back */
  TRX_QUE_COMMITTING    /*!< transaction is committing */
};

/** Transaction states (trx_t::state) */
enum trx_state_t {

  TRX_STATE_NOT_STARTED,

  /** Same as not started but with additional semantics that it
  was rolled back asynchronously the last time it was active. */
  TRX_STATE_FORCED_ROLLBACK,

  TRX_STATE_ACTIVE,

  /** Support for 2PC/XA */
  TRX_STATE_PREPARED,

  TRX_STATE_COMMITTED_IN_MEMORY
};

/** Type of data dictionary operation */
enum trx_dict_op_t {
  /** The transaction is not modifying the data dictionary. */
  TRX_DICT_OP_NONE = 0,
  /** The transaction is creating a table or an index, or
  dropping a table.  The table must be dropped in crash
  recovery.  This and TRX_DICT_OP_NONE are the only possible
  operation modes in crash recovery. */
  TRX_DICT_OP_TABLE = 1,
  /** The transaction is creating or dropping an index in an
  existing table.  In crash recovery, the data dictionary
  must be locked, but the table must not be dropped. */
  TRX_DICT_OP_INDEX = 2
};

/** Memory objects */
/* @{ */
/** Transaction */
struct trx_t;
/** The locks and state of an active transaction */
struct trx_lock_t;
/** Transaction system */
struct trx_sys_t;
/** Signal */
struct trx_sig_t;
/** Rollback segment */
struct trx_rseg_t;
/** Transaction undo log */
struct trx_undo_t;
/** The control structure used in the purge operation */
struct trx_purge_t;
/** Rollback command node in a query graph */
struct roll_node_t;
/** Commit command node in a query graph */
struct commit_node_t;
/** SAVEPOINT command node in a query graph */
struct trx_named_savept_t;
/* @} */

/** Row identifier (DB_ROW_ID, DATA_ROW_ID) */
typedef ib_id_t row_id_t;
/** Transaction identifier (DB_TRX_ID, DATA_TRX_ID) */
typedef ib_id_t trx_id_t;
/** Rollback pointer (DB_ROLL_PTR, DATA_ROLL_PTR) */
typedef ib_id_t roll_ptr_t;
/** Undo number */
typedef ib_id_t undo_no_t;

/** Maximum transaction identifier */
#define TRX_ID_MAX IB_ID_MAX

/** Transaction savepoint */
struct trx_savept_t {
  undo_no_t least_undo_no; /*!< least undo number to undo */
};

/** File objects */
/* @{ */
/** Transaction system header */
typedef byte trx_sysf_t;
/** Rollback segment array header */
typedef byte trx_rsegsf_t;
/** Rollback segment header */
typedef byte trx_rsegf_t;
/** Undo segment header */
typedef byte trx_usegf_t;
/** Undo log header */
typedef byte trx_ulogf_t;
/** Undo log page header */
typedef byte trx_upagef_t;
/** Undo log record */
typedef byte trx_undo_rec_t;
/* @} */

typedef ib_mutex_t RsegMutex;
typedef ib_mutex_t TrxMutex;
typedef ib_mutex_t UndoMutex;
typedef ib_mutex_t PQMutex;
typedef ib_mutex_t TrxSysMutex;

/** The rollback segment memory object */
struct trx_rseg_t {
  /*--------------------------------------------------------*/
  /** rollback segment id == the index of its slot in the trx
  system file copy */
  ulint id;

  /** mutex protecting the fields in this struct except id,space,page_no
  which are constant */
  RsegMutex mutex;

  /** space ID where the rollback segment header is placed */
  space_id_t space_id;

  /** page number of the rollback segment header */
  page_no_t page_no;

  /** page size of the relevant tablespace */
  page_size_t page_size;

  /** maximum allowed size in pages */
  ulint max_size;

  /** current size in pages */
  ulint curr_size;

  /*--------------------------------------------------------*/
  /* Fields for update undo logs */
  /** List of update undo logs */
  UT_LIST_BASE_NODE_T(trx_undo_t) update_undo_list;

  /** List of update undo log segments cached for fast reuse */
  UT_LIST_BASE_NODE_T(trx_undo_t) update_undo_cached;

  /*--------------------------------------------------------*/
  /* Fields for insert undo logs */
  /** List of insert undo logs */
  UT_LIST_BASE_NODE_T(trx_undo_t) insert_undo_list;

  /** List of insert undo log segments cached for fast reuse */
  UT_LIST_BASE_NODE_T(trx_undo_t) insert_undo_cached;

  /*--------------------------------------------------------*/

  /** Page number of the last not yet purged log header in the history
  list; FIL_NULL if all list purged */
  page_no_t last_page_no;

  /** Byte offset of the last not yet purged log header */
  ulint last_offset;

  /** Transaction number of the last not yet purged log */
  trx_id_t last_trx_no;

  /** TRUE if the last not yet purged log needs purging */
  ibool last_del_marks;

  /** Reference counter to track rseg allocated transactions. */
  std::atomic<ulint> trx_ref_count;
};

using Rsegs_Vector = std::vector<trx_rseg_t *, ut_allocator<trx_rseg_t *>>;
using Rseg_Iterator = Rsegs_Vector::iterator;

/** This is a wrapper for a std::vector of trx_rseg_t object pointers. */
class Rsegs {
 public:
  /** Default constructor */
  Rsegs() : m_rsegs(), m_latch(), m_active(false) {
#ifndef UNIV_HOTBACKUP
    init();
#endif /* !UNIV_HOTBACKUP */
  }

  ~Rsegs() {
#ifndef UNIV_HOTBACKUP
    deinit();
#endif /* !UNIV_HOTBACKUP */
  }

  /** Initialize */
  void init();

  /** De-initialize */
  void deinit();

  /** Clear the vector of cached rollback segments leaving the
  reserved space allocated. */
  void clear();

  /** Add rollback segment.
  @param[in]	rseg	rollback segment to add. */
  void push_back(trx_rseg_t *rseg) { m_rsegs.push_back(rseg); }

  /** Number of registered rsegs.
  @return size of rseg list. */
  ulint size() { return (m_rsegs.size()); }

  /** beginning iterator
  @return an iterator to the first element */
  Rseg_Iterator begin() { return (m_rsegs.begin()); }

  /** ending iterator
  @return an iterator to the end */
  Rseg_Iterator end() { return (m_rsegs.end()); }

  /** Find the rseg at the given slot in this vector.
  @param[in]	slot	a slot within the vector.
  @return an iterator to the end */
  trx_rseg_t *at(ulint slot) { return (m_rsegs.at(slot)); }

  /** Find an rseg in the std::vector that uses the rseg_id given.
  @param[in]	rseg_id		A slot in a durable array such as
                                  the TRX_SYS page or RSEG_ARRAY page.
  @return a pointer to an trx_rseg_t that uses the rseg_id. */
  trx_rseg_t *find(ulint rseg_id);

  /** Sort the vector on trx_rseg_t::id */
  void sort() {
    if (m_rsegs.empty()) {
      return;
    }

    std::sort(
        m_rsegs.begin(), m_rsegs.end(),
        [](trx_rseg_t *lhs, trx_rseg_t *rhs) { return (rhs->id > lhs->id); });
  }

  /** Get a shared lock on m_rsegs. */
  void s_lock() { rw_lock_s_lock(m_latch); }

  /** Get a shared lock on m_rsegs. */
  void s_unlock() { rw_lock_s_unlock(m_latch); }

  /** Get a shared lock on m_rsegs. */
  void x_lock() { rw_lock_x_lock(m_latch); }

  /** Get a shared lock on m_rsegs. */
  void x_unlock() { rw_lock_x_unlock(m_latch); }

  /* Set the transaction active. */
  void set_active() { m_active = true; }

  /* Set the transaction inactive. */
  void set_inactive() { m_active = false; }

  /* Return whether the undo tablespace is active.
  @return true if active */
  bool is_active() { return (m_active); }

  /* Return whether the undo tablespace is inactive.
  @return true if active */
  bool is_inactive() { return (!m_active); }

  /** std::vector of rollback segments */
  Rsegs_Vector m_rsegs;

 private:
  /** RW lock to protect m_rsegs vector, m_active, and each
  trx_rseg_t::trx_ref_count within it.
  m_rsegs:   x for adding elements, s for scanning, size etc.
  m_active:  x for modification, s for read
  each trx_rseg_t::trx_ref_count within m_rsegs
             s and atomic increment for modification, x for read */
  rw_lock_t *m_latch;

  /** If true, then these rollback segments can be allocated
  to new transactions. */
  bool m_active;
};

/** Rollback segements from a given transaction with trx-no
scheduled for purge. */
class TrxUndoRsegs {
 public:
  /** Default constructor */
  TrxUndoRsegs() : m_trx_no() {}

  explicit TrxUndoRsegs(trx_id_t trx_no) : m_trx_no(trx_no) {
    // Do nothing
  }

  /** Get transaction number
  @return trx_id_t - get transaction number. */
  trx_id_t get_trx_no() const { return (m_trx_no); }

  /** Add rollback segment.
  @param rseg rollback segment to add. */
  void push_back(trx_rseg_t *rseg) { m_rsegs.push_back(rseg); }

  /** Erase the element pointed by given iterator.
  @param[in]	it	iterator */
  void erase(Rseg_Iterator &it) { m_rsegs.erase(it); }

  /** Number of registered rsegs.
  @return size of rseg list. */
  ulint size() const { return (m_rsegs.size()); }

  /**
  @return an iterator to the first element */
  Rseg_Iterator begin() { return (m_rsegs.begin()); }

  /**
  @return an iterator to the end */
  Rseg_Iterator end() { return (m_rsegs.end()); }

  /** Append rollback segments from referred instance to current
  instance. */
  void append(const TrxUndoRsegs &append_from) {
    ut_ad(get_trx_no() == append_from.get_trx_no());

    m_rsegs.insert(m_rsegs.end(), append_from.m_rsegs.begin(),
                   append_from.m_rsegs.end());
  }

  /** Compare two TrxUndoRsegs based on trx_no.
  @param lhs first element to compare
  @param rhs second element to compare
  @return true if elem1 > elem2 else false.*/
  bool operator()(const TrxUndoRsegs &lhs, const TrxUndoRsegs &rhs) {
    return (lhs.m_trx_no > rhs.m_trx_no);
  }

  /** Compiler defined copy-constructor/assignment operator
  should be fine given that there is no reference to a memory
  object outside scope of class object.*/

 private:
  /** The rollback segments transaction number. */
  trx_id_t m_trx_no;

  /** Rollback segments of a transaction, scheduled for purge. */
  Rsegs_Vector m_rsegs;
};

typedef std::priority_queue<
    TrxUndoRsegs, std::vector<TrxUndoRsegs, ut_allocator<TrxUndoRsegs>>,
    TrxUndoRsegs>
    purge_pq_t;

typedef std::vector<trx_id_t, ut_allocator<trx_id_t>> trx_ids_t;

/** Mapping read-write transactions from id to transaction instance, for
creating read views and during trx id lookup for MVCC and locking. */
struct TrxTrack {
  explicit TrxTrack(trx_id_t id, trx_t *trx = NULL) : m_id(id), m_trx(trx) {
    // Do nothing
  }

  trx_id_t m_id;
  trx_t *m_trx;
};

struct TrxTrackHash {
  size_t operator()(const TrxTrack &key) const { return (size_t(key.m_id)); }
};

/**
Comparator for TrxMap */
struct TrxTrackHashCmp {
  bool operator()(const TrxTrack &lhs, const TrxTrack &rhs) const {
    return (lhs.m_id == rhs.m_id);
  }
};

/**
Comparator for TrxMap */
struct TrxTrackCmp {
  bool operator()(const TrxTrack &lhs, const TrxTrack &rhs) const {
    return (lhs.m_id < rhs.m_id);
  }
};

// typedef std::unordered_set<TrxTrack, TrxTrackHash, TrxTrackHashCmp> TrxIdSet;
typedef std::set<TrxTrack, TrxTrackCmp, ut_allocator<TrxTrack>> TrxIdSet;

#endif /* trx0types_h */
