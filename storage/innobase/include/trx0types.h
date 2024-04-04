/*****************************************************************************

Copyright (c) 1996, 2024, Oracle and/or its affiliates.

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

/** printf(3) format used for printing DB_TRX_ID and other system fields */
#define TRX_ID_FMT IB_ID_FMT

/** Space id of the transaction system page (the system tablespace) */
static const space_id_t TRX_SYS_SPACE = 0;

/** Page number of the transaction system page */
constexpr uint32_t TRX_SYS_PAGE_NO = FSP_TRX_SYS_PAGE_NO;

/** Random value to check for corruption of trx_t */
static const ulint TRX_MAGIC_N = 91118598;

/** If this flag is set then the transaction cannot be rolled back
asynchronously. */
static const uint32_t TRX_FORCE_ROLLBACK_DISABLE = 1 << 29;

/** Mark the transaction for forced rollback */
static const uint32_t TRX_FORCE_ROLLBACK = 1U << 31;

/** For masking out the above flags */
static const uint32_t TRX_FORCE_ROLLBACK_MASK = 0x1FFFFFFF;

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
/** @{ */
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
/** @} */

/** Row identifier (DB_ROW_ID, DATA_ROW_ID) */
typedef ib_id_t row_id_t;
/** Transaction identifier (DB_TRX_ID, DATA_TRX_ID) */
typedef ib_id_t trx_id_t;
/** Rollback pointer (DB_ROLL_PTR, DATA_ROLL_PTR) */
typedef ib_id_t roll_ptr_t;
/** Undo number */
typedef ib_id_t undo_no_t;

/** Maximum transaction identifier */
constexpr trx_id_t TRX_ID_MAX = IB_ID_MAX;

/** Transaction savepoint */
struct trx_savept_t {
  undo_no_t least_undo_no; /*!< least undo number to undo */
};

/** File objects */
/** @{ */
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
/** @} */

typedef ib_mutex_t RsegMutex;
typedef ib_mutex_t TrxMutex;
typedef ib_mutex_t UndoMutex;
typedef ib_mutex_t PQMutex;
typedef ib_mutex_t TrxSysMutex;

/** Used to identify trx uniquely over time */
struct trx_guid_t {
  /** The immutable id of trx_t object - if you have a pointer to trx_t then we
  guarantee that immutable id of it will not change over time. Also there are
  never two trx_t objects at the same time with same immutable id. However it
  may happen that two different transactions that do not occur at the same time
  reuse the same trx_t and thus have same immutable id. Use m_version to detect
  this situation. */
  uint64_t m_immutable_id{};

  /** As trx_t objects and thus immutable ids can be reused we need also trx's
  version, which is incremented each time trx_t object gets reused. */
  uint64_t m_version{};

  /** Initializes trx_guid_t object to a value which doesn't match any real
  transaction. */
  trx_guid_t() = default;

  /** Initializes trx_guid_t with data uniquely identifying the transaction
  represented by trx_t object.
  @param[in]  trx   the object representing the transaction */
  trx_guid_t(const trx_t &trx);

  /** Checks if two guids represent the same transaction:
  they refer to the same trx_t struct and it was not reused meanwhile.
  @param[in]  rhs   another guid to compare against
  @return true iff the two guids are equal and thus represent same transaction*/
  bool operator==(const trx_guid_t &rhs) const {
    return m_immutable_id == rhs.m_immutable_id && m_version == rhs.m_version;
  }

  /** Checks if the instance is non-empty, i.e. was not default-constructed,
  but rather initialized to correspond to a real trx_t.
  @return true iff this guid was initialized to match a real transaction */
  operator bool() const { return m_immutable_id != 0; }
};

/** The rollback segment memory object */
struct trx_rseg_t {
#ifdef UNIV_DEBUG
  /** Validate the curr_size member by re-calculating it.
  @param[in]  take_mutex  take the rseg->mutex. default is true.
  @return true if valid, false otherwise. */
  bool validate_curr_size(bool take_mutex = true);
#endif /* UNIV_DEBUG */

  /** Enter the rseg->mutex. */
  void latch() {
    mutex_enter(&mutex);
    ut_ad(validate_curr_size(false));
  }

  /** Exit the rseg->mutex. */
  void unlatch() {
    ut_ad(validate_curr_size(false));
    mutex_exit(&mutex);
  }

  /** Decrement the current size of the rollback segment by the given number
  of pages.
  @param[in]  npages  number of pages to reduce in size. */
  void decr_curr_size(page_no_t npages = 1) {
    ut_ad(curr_size >= npages);
    curr_size -= npages;
  }

  /** Increment the current size of the rollback segment by the given number
  of pages. */
  void incr_curr_size() { ++curr_size; }

  /* Get the current size of the rollback segment in pages.
   @return current size of the rollback segment in pages. */
  page_no_t get_curr_size() const { return (curr_size); }

  /* Set the current size of the rollback segment in pages.
  @param[in]  npages  new value for the current size. */
  void set_curr_size(page_no_t npages) { curr_size = npages; }

  /*--------------------------------------------------------*/
  /** rollback segment id == the index of its slot in the trx
  system file copy */
  size_t id{};

  /** mutex protecting the fields in this struct except id,space,page_no
  which are constant */
  RsegMutex mutex;

  /** space ID where the rollback segment header is placed */
  space_id_t space_id{};

  /** page number of the rollback segment header */
  page_no_t page_no{};

  /** page size of the relevant tablespace */
  page_size_t page_size;

  /** maximum allowed size in pages */
  page_no_t max_size{};

 private:
  /** current size in pages */
  page_no_t curr_size{};

 public:
  using Undo_list = UT_LIST_BASE_NODE_T_EXTERN(trx_undo_t, undo_list);
  /*--------------------------------------------------------*/
  /* Fields for update undo logs */
  /** List of update undo logs */
  Undo_list update_undo_list;

  /** List of update undo log segments cached for fast reuse */
  Undo_list update_undo_cached;

  /*--------------------------------------------------------*/
  /* Fields for insert undo logs */
  /** List of insert undo logs */
  Undo_list insert_undo_list;

  /** List of insert undo log segments cached for fast reuse */
  Undo_list insert_undo_cached;

  /*--------------------------------------------------------*/

  /** Page number of the last not yet purged log header in the history
  list; FIL_NULL if all list purged */
  page_no_t last_page_no{};

  /** Byte offset of the last not yet purged log header */
  size_t last_offset{};

  /** Transaction number of the last not yet purged log */
  trx_id_t last_trx_no;

  /** true if the last not yet purged log needs purging */
  bool last_del_marks{};

  /** Reference counter to track rseg allocated transactions. */
  std::atomic<size_t> trx_ref_count{};

  std::ostream &print(std::ostream &out) const {
    out << "[trx_rseg_t: this=" << (void *)this << ", id=" << id
        << ", space_id=" << space_id << ", page_no=" << page_no
        << ", curr_size=" << curr_size << "]";
    return (out);
  }
};

inline std::ostream &operator<<(std::ostream &out, const trx_rseg_t &rseg) {
  return (rseg.print(out));
}

using Rsegs_Vector = std::vector<trx_rseg_t *, ut::allocator<trx_rseg_t *>>;
using Rseg_Iterator = Rsegs_Vector::iterator;

/** This is a wrapper for a std::vector of trx_rseg_t object pointers. */
class Rsegs {
 public:
  /** Default constructor */
  Rsegs() : m_rsegs(), m_latch(), m_state(INIT) {
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
  @param[in]    rseg    rollback segment to add. */
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
  @param[in]    slot    a slot within the vector.
  @return an iterator to the end */
  trx_rseg_t *at(ulint slot) { return (m_rsegs.at(slot)); }

  /** Find an rseg in the std::vector that uses the rseg_id given.
  @param[in]    rseg_id         A slot in a durable array such as
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

  /** Acquire the shared lock on m_rsegs. */
  void s_lock() { rw_lock_s_lock(m_latch, UT_LOCATION_HERE); }

  /** Release the shared lock on m_rsegs. */
  void s_unlock() { rw_lock_s_unlock(m_latch); }

  /** Acquire the exclusive lock on m_rsegs. */
  void x_lock() { rw_lock_x_lock(m_latch, UT_LOCATION_HERE); }

  /** Release the exclusive lock on m_rsegs. */
  void x_unlock() { rw_lock_x_unlock(m_latch); }

  /** Return whether the undo tablespace is active.
  @return true if active */
  bool is_active() { return (m_state == ACTIVE); }

  /** Return whether the undo tablespace is inactive due to
  implicit selection by the purge thread.
  @return true if marked for truncation by the purge thread */
  bool is_inactive_implicit() { return (m_state == INACTIVE_IMPLICIT); }

  /** Return whether the undo tablespace was made inactive by
  ALTER TABLESPACE.
  @return true if altered */
  bool is_inactive_explicit() { return (m_state == INACTIVE_EXPLICIT); }

  /** Return whether the undo tablespace is empty and ready
  to be dropped.
  @return true if empty */
  bool is_empty() { return (m_state == EMPTY); }

  /** Return whether the undo tablespace is being initialized.
  @return true if empty */
  bool is_init() { return (m_state == INIT); }

  /** Set the state of the rollback segments in this undo tablespace
  to ACTIVE for use by new transactions. */
  void set_active() { m_state = ACTIVE; }

  /** Set the state of the rollback segments in this undo
  tablespace to inactive_implicit. This means that it will be
  truncated and then made active again by the purge thread.
  It will not be used for new transactions until it becomes
  active again. */
  void set_inactive_implicit() {
    ut_ad(m_state == ACTIVE || m_state == INACTIVE_EXPLICIT);
    m_state = INACTIVE_IMPLICIT;
  }

  /** Make the undo tablespace inactive so that it will not be
  used for new transactions.  The purge thread will clear out
  all the undo logs, truncate it, and then mark it empty. */
  void set_inactive_explicit() { m_state = INACTIVE_EXPLICIT; }

  /** Set the state of the undo tablespace to empty so that it
  can be dropped. */
  void set_empty() {
    ut_ad(m_state == INACTIVE_EXPLICIT || m_state == ACTIVE ||
          m_state == INIT || m_state == EMPTY);
    m_state = EMPTY;
  }

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

  /* The four states of an undo tablespace.
  INIT:     The initial state of an undo space that is being created or opened.
  ACTIVE:   The rollback segments in this tablespace can be allocated to new
            transactions.  The undo tablespace is ready for undo logs.
  INACTIVE_IMPLICIT: These rollback segments are no longer being used by new
            transactions.  They are 'inactive'. The truncate process
            is happening. This undo tablespace was selected by the
            purge thread implicitly. When the truncation process
            is complete, the next state is ACTIVE.
  INACTIVE_EXPLICIT:  These rollback segments are no longer being used by new
            transactions.  They are 'inactive'. The truncate process
            is happening. This undo tablespace was selected by the
            an ALTER UNDO TABLESPACE  SET INACTIVE command. When the
            truncation process is complete, the next state is EMPTY.
  EMPTY:    The undo tablespace has been truncated but is no longer
            active. It is ready to be either dropped or set active
            explicitly. This state is also used when the undo tablespace and
            its rollback segments are being inititalized.

  These states are changed under an exclusive lock on m_latch and are read
  under a shared lock.

  The following actions can cause changes in these states:
  Init:         Implicit undo spaces are created at startup.
  Create:       Explicit undo tablespace creation at runtime.
  Mark:         Purge thread implicitly selects an undo space to truncate.
  SetInactive:  This ALTER UNDO TABLESPACE causes an explicit truncation.
  SetActive:    This ALTER UNDO TABLESPACE changes the target state from
                EMPTY to ACTIVE.
  Truncate:     The truncate process is completed by the purge thread.
  Drop:         Delete an EMPTY undo tablespace
  Crash:        A crash occurs
  Fixup:        At startup, if an undo space was being truncated with a crash.
  SaveDDState:  At startup, once the DD is available the state saved there
                will be applied.  INACTIVE_IMPLICIT is never saved to the DD.
                So the DD state INACTIVE means INACTIVE_EXPLICIT.
                See apply_dd_undo_state()

  State changes allowed: (Actions on states not mentioned are not allowed.)
  Init         from null -> INIT -> ACTIVE see srv_start()
               from null -> INIT -> EMPTY  see trx_rsegs_init()
  Create       from null -> EMPTY -> ACTIVE
  Mark         from INACTIVE_EXPLICIT -> INACTIVE_EXPLICIT -> Truncate
               from ACTIVE -> INACTIVE_IMPLICIT -> Truncate
  SetInactive  from ACTIVE -> INACTIVE_EXPLICIT -> Mark
               from INACTIVE_IMPLICIT -> INACTIVE_EXPLICIT
               from INACTIVE_EXPLICIT -> INACTIVE_EXPLICIT
               from EMPTY -> EMPTY
  SetActive    from ACTIVE -> ACTIVE
               from INACTIVE_IMPLICIT -> INACTIVE_IMPLICIT
               from INACTIVE_EXPLICIT -> INACTIVE_IMPLICIT
               from EMPTY -> ACTIVE
  Truncate     from INACTIVE_IMPLICIT -> ACTIVE
               from INACTIVE_EXPLICIT -> EMPTY
  Drop         if ACTIVE -> error returned
               if INACTIVE_IMPLICIT -> error returned
               if INACTIVE_EXPLICIT -> error returned
               from EMPTY -> null
  Crash        if ACTIVE, at startup:  ACTIVE
               if INACTIVE_IMPLICIT, at startup: Fixup
               if INACTIVE_EXPLICIT, at startup: Fixup
               if EMPTY, at startup:  EMPTY
  Fixup        from INACTIVE_IMPLICIT before crash -> INACTIVE_IMPLICIT -> Mark
               from INACTIVE_EXPLICIT before crash -> INACTIVE_IMPLICIT -> Mark
  SaveDDState  from ACTIVE before crash -> ACTIVE
               from INACTIVE_IMPLICIT before crash -> ACTIVE
               from INACTIVE_EXPLICIT before crash -> INACTIVE_EXPLICIT -> Mark
               from EMPTY -> EMPTY
  */
  enum undo_space_states {
    INIT,
    ACTIVE,
    INACTIVE_IMPLICIT,
    INACTIVE_EXPLICIT,
    EMPTY
  };

  /** The current state of this undo tablespace. */
  undo_space_states m_state;
};

template <size_t N>
using Rsegs_array = std::array<trx_rseg_t *, N>;

/** Rollback segments from a given transaction with trx-no
scheduled for purge. */
class TrxUndoRsegs {
 public:
  explicit TrxUndoRsegs(trx_id_t trx_no) : m_trx_no(trx_no) {
    for (auto &rseg : m_rsegs) {
      rseg = nullptr;
    }
  }

  /** Default constructor */
  TrxUndoRsegs() : TrxUndoRsegs(0) {}

  void set_trx_no(trx_id_t trx_no) { m_trx_no = trx_no; }

  /** Get transaction number
  @return trx_id_t - get transaction number. */
  trx_id_t get_trx_no() const { return (m_trx_no); }

  /** Add rollback segment.
  @param rseg rollback segment to add. */
  void insert(trx_rseg_t *rseg) {
    for (size_t i = 0; i < m_rsegs_n; ++i) {
      if (m_rsegs[i] == rseg) {
        return;
      }
    }
    ut_a(m_rsegs_n < 2);
    m_rsegs[m_rsegs_n++] = rseg;
  }

  /** Number of registered rsegs.
  @return size of rseg list. */
  size_t size() const { return (m_rsegs_n); }

  /**
  @return an iterator to the first element */
  typename Rsegs_array<2>::iterator begin() { return m_rsegs.begin(); }

  /**
  @return an iterator to the end */
  typename Rsegs_array<2>::iterator end() {
    return m_rsegs.begin() + m_rsegs_n;
  }

  /** Append rollback segments from referred instance to current
  instance. */
  void insert(const TrxUndoRsegs &append_from) {
    ut_ad(get_trx_no() == append_from.get_trx_no());
    for (size_t i = 0; i < append_from.m_rsegs_n; ++i) {
      insert(append_from.m_rsegs[i]);
    }
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

  size_t m_rsegs_n{};

  /** Rollback segments of a transaction, scheduled for purge. */
  Rsegs_array<2> m_rsegs;
};

typedef std::priority_queue<
    TrxUndoRsegs, std::vector<TrxUndoRsegs, ut::allocator<TrxUndoRsegs>>,
    TrxUndoRsegs>
    purge_pq_t;

typedef std::vector<trx_id_t, ut::allocator<trx_id_t>> trx_ids_t;

struct TrxVersion {
  TrxVersion(trx_t *trx);

  trx_t *m_trx;
  uint64_t m_version;
};

typedef std::vector<TrxVersion, ut::allocator<TrxVersion>> hit_list_t;
#endif /* trx0types_h */
