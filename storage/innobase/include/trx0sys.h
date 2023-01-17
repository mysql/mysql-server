/*****************************************************************************

Copyright (c) 1996, 2023, Oracle and/or its affiliates.

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

/** @file include/trx0sys.h
 Transaction system

 Created 3/26/1996 Heikki Tuuri
 *******************************************************/

#ifndef trx0sys_h
#define trx0sys_h

#include "univ.i"

#include "buf0buf.h"
#include "fil0fil.h"
#include "trx0types.h"
#ifndef UNIV_HOTBACKUP
#include "mem0mem.h"
#include "mtr0mtr.h"
#include "page0types.h"
#include "ut0byte.h"
#include "ut0class_life_cycle.h"
#include "ut0guarded.h"
#include "ut0lst.h"
#include "ut0mutex.h"
#endif /* !UNIV_HOTBACKUP */
#include <atomic>
#include <unordered_map>
#include <vector>
#include "trx0trx.h"

#ifndef UNIV_HOTBACKUP

// Forward declaration
class MVCC;
class ReadView;

/** The transaction system */
extern trx_sys_t *trx_sys;

/** Checks if a page address is the trx sys header page.
@param[in]      page_id page id
@return true if trx sys header page */
static inline bool trx_sys_hdr_page(const page_id_t &page_id);

/** Creates and initializes the central memory structures for the transaction
 system. This is called when the database is started.
 @return min binary heap of rsegs to purge */
purge_pq_t *trx_sys_init_at_db_start(void);
/** Creates the trx_sys instance and initializes purge_queue and mutex. */
void trx_sys_create(void);
/** Creates and initializes the transaction system at the database creation. */
void trx_sys_create_sys_pages(void);

/** Find the page number in the TRX_SYS page for a given slot/rseg_id
@param[in]      rseg_id         slot number in the TRX_SYS page rseg array
@return page number from the TRX_SYS page rseg array */
page_no_t trx_sysf_rseg_find_page_no(ulint rseg_id);

/** Look for a free slot for a rollback segment in the trx system file copy.
@param[in,out]  mtr             mtr
@return slot index or ULINT_UNDEFINED if not found */
ulint trx_sysf_rseg_find_free(mtr_t *mtr);

/** Gets a pointer to the transaction system file copy and x-locks its page.
 @return pointer to system file copy, page x-locked */
static inline trx_sysf_t *trx_sysf_get(mtr_t *mtr); /*!< in: mtr */

/** Gets the space of the nth rollback segment slot in the trx system
file copy.
@param[in]      sys_header      trx sys file copy
@param[in]      i               slot index == rseg id
@param[in]      mtr             mtr
@return space id */
static inline space_id_t trx_sysf_rseg_get_space(trx_sysf_t *sys_header,
                                                 ulint i, mtr_t *mtr);

/** Gets the page number of the nth rollback segment slot in the trx system
file copy.
@param[in]      sys_header      trx sys file copy
@param[in]      i               slot index == rseg id
@param[in]      mtr             mtr
@return page number, FIL_NULL if slot unused */
static inline page_no_t trx_sysf_rseg_get_page_no(trx_sysf_t *sys_header,
                                                  ulint i, mtr_t *mtr);

/** Sets the space id of the nth rollback segment slot in the trx system
file copy.
@param[in]      sys_header      trx sys file copy
@param[in]      i               slot index == rseg id
@param[in]      space           space id
@param[in]      mtr             mtr */
static inline void trx_sysf_rseg_set_space(trx_sysf_t *sys_header, ulint i,
                                           space_id_t space, mtr_t *mtr);

/** Set the page number of the nth rollback segment slot in the trx system
file copy.
@param[in]      sys_header      trx sys file copy
@param[in]      i               slot index == rseg id
@param[in]      page_no         page number, FIL_NULL if the slot is reset to
                                unused
@param[in]      mtr             mtr */
static inline void trx_sysf_rseg_set_page_no(trx_sysf_t *sys_header, ulint i,
                                             page_no_t page_no, mtr_t *mtr);

/** Allocates a new transaction id (for trx->id). Before calling,
the trx_sys_mutex must be acquired.
@return new, allocated trx id */
inline trx_id_t trx_sys_allocate_trx_id();

/** Allocates a new transaction number (for trx->no). Before calling,
the trx_sys_serialisation_mutex must be acquired.
@return new, allocated trx no */
inline trx_id_t trx_sys_allocate_trx_no();

/** Retrieves a next value that will be allocated if trx_sys_allocate_trx_id()
or trx_sys_allocate_trx_id_trx_no() was called.
@return the next trx->id or trx->no that will be allocated */
inline trx_id_t trx_sys_get_next_trx_id_or_no();

#ifdef UNIV_DEBUG
/* Flag to control TRX_RSEG_N_SLOTS behavior debugging. */
extern uint trx_rseg_n_slots_debug;
#endif
#endif /* !UNIV_HOTBACKUP */

/** Writes a trx id to an index page. In case that the id size changes in some
future version, this function should be used instead of mach_write_...
@param[in]      ptr     pointer to memory where written
@param[in]      id      id */
static inline void trx_write_trx_id(byte *ptr, trx_id_t id);

#ifndef UNIV_HOTBACKUP
/** Reads a trx id from an index page. In case that the id size changes in
 some future version, this function should be used instead of
 mach_read_...
 @return id */
static inline trx_id_t trx_read_trx_id(
    const byte *ptr); /*!< in: pointer to memory from where to read */

/** Checks if a rw transaction with the given id is active.
Please note, that positive result means only that the trx was active
at some moment during the call, but it might have already become
TRX_STATE_COMMITTED_IN_MEMORY before the call returns to the caller, as this
transition is protected by trx->mutex and Trx_shard's mutex, but it is
impossible for the caller to hold any of these mutexes when calling this
function as the function itself internally acquires Trx_shard's mutex which
would cause recurrent mutex acquisition if caller already had the same mutex,
or latching order violation in case of holding trx->mutex.
@param[in]      trx_id          trx id of the transaction
@param[in]      do_ref_count    if true then increment the trx_t::n_ref_count
@return transaction instance if active, or NULL; */
static inline trx_t *trx_rw_is_active(trx_id_t trx_id, bool do_ref_count);

/** Persist transaction number limit below which all transaction GTIDs
are persisted to disk table.
@param[in]      gtid_trx_no     transaction number */
void trx_sys_persist_gtid_num(trx_id_t gtid_trx_no);

/** @return oldest transaction number yet to be committed. */
trx_id_t trx_sys_oldest_trx_no();

/** Get a list of all binlog prepared transactions.
@param[out]     trx_ids all prepared transaction IDs. */
void trx_sys_get_binlog_prepared(std::vector<trx_id_t> &trx_ids);

/** Get current binary log positions stored.
@param[out]     file    binary log file name
@param[out]     offset  binary log file offset */
void trx_sys_read_binlog_position(char *file, uint64_t &offset);

/** Update binary log position if not already updated. This is called
by clone to update any stale binary log position if any transaction
is yet to update the binary log position in SE.
@param[in]      last_file       last noted binary log file name
@param[in]      last_offset     last noted binary log offset
@param[in]      file            current binary log file name
@param[in]      offset          current binary log file offset
@return true, if binary log position is updated with current. */
bool trx_sys_write_binlog_position(const char *last_file, uint64_t last_offset,
                                   const char *file, uint64_t offset);

/** Updates the offset information about the end of the MySQL binlog entry
which corresponds to the transaction being committed, external XA transaction
being prepared or rolled back. In a MySQL replication slave updates the latest
master binlog position up to which replication has proceeded.
@param[in]      trx     Current transaction
@param[in,out]  mtr     Mini-transaction for update */
void trx_sys_update_mysql_binlog_offset(trx_t *trx, mtr_t *mtr);

/** Shutdown/Close the transaction system. */
void trx_sys_close(void);

/** Determine if there are incomplete transactions in the system.
@return whether incomplete transactions need rollback */
static inline bool trx_sys_need_rollback();

/** Reads number of recovered transactions which have state
equal to TRX_STATE_ACTIVE (so are not prepared transactions).
@return number of active recovered transactions */
size_t trx_sys_recovered_active_trxs_count();

/** Validates lists of transactions at the very beginning of the
pre-dd-shutdown phase. */
void trx_sys_before_pre_dd_shutdown_validate();

/** Validates lists of transactions at the very end of the
pre-dd-shutdown phase. */
void trx_sys_after_pre_dd_shutdown_validate();

/** Validates lists of transactions after all background threads
of InnoDB exited during shutdown of MySQL. */
void trx_sys_after_background_threads_shutdown_validate();

/** Add the transaction to the RW transaction set.
@param trx              transaction instance to add */
static inline void trx_sys_rw_trx_add(trx_t *trx);

#endif /* !UNIV_HOTBACKUP */

#ifdef UNIV_DEBUG
/** Validate the trx_sys_t::rw_trx_list.
 @return true if the list is valid */
bool trx_sys_validate_trx_list();
#endif /* UNIV_DEBUG */

/** Initialize trx_sys_undo_spaces, called once during srv_start(). */
void trx_sys_undo_spaces_init();

/** Free the resources occupied by trx_sys_undo_spaces,
called once during thread de-initialization. */
void trx_sys_undo_spaces_deinit();

/** The automatically created system rollback segment has this id */
constexpr uint32_t TRX_SYS_SYSTEM_RSEG_ID = 0;

/** The offset of the transaction system header on the page */
constexpr uint32_t TRX_SYS = FSEG_PAGE_DATA;

/** Transaction system header */
/*------------------------------------------------------------- @{ */
/** the maximum trx id or trx number modulo TRX_SYS_TRX_ID_UPDATE_MARGIN written
   to a file page by any  transaction; the assignment of transaction ids
   continues from  this number rounded up by TRX_SYS_TRX_ID_UPDATE_MARGIN  plus
   TRX_SYS_TRX_ID_UPDATE_MARGIN when the database is started */
constexpr uint32_t TRX_SYS_TRX_ID_STORE = 0;
/** segment header for the  tablespace segment the trx system is created into */
constexpr uint32_t TRX_SYS_FSEG_HEADER = 8;
/** the start of the array of rollback segment specification slots */
constexpr uint32_t TRX_SYS_RSEGS = 8 + FSEG_HEADER_SIZE;
/*------------------------------------------------------------- @} */

/* Originally, InnoDB defined TRX_SYS_N_RSEGS as 256 but created only one
rollback segment.  It initialized some arrays with this number of entries.
We must remember this limit in order to keep file compatibility. */
constexpr size_t TRX_SYS_OLD_N_RSEGS = 256;

/* The system temporary tablespace was originally allocated rseg_id slot
numbers 1 through 32 in the TRX_SYS page.  But those slots were not used
because those Rollback segments were recreated at startup and after any
crash. These slots are now used for redo-enabled rollback segments.
The default number of rollback segments in the temporary tablespace
remains the same. */
constexpr size_t TRX_SYS_OLD_TMP_RSEGS = 32;

/** Maximum length of MySQL binlog file name, in bytes. */
constexpr uint32_t TRX_SYS_MYSQL_LOG_NAME_LEN = 512;
/** Contents of TRX_SYS_MYSQL_LOG_MAGIC_N_FLD */
constexpr uint32_t TRX_SYS_MYSQL_LOG_MAGIC_N = 873422344;

static_assert(UNIV_PAGE_SIZE_MIN >= 4096, "UNIV_PAGE_SIZE_MIN < 4096");
/** The offset of the MySQL binlog offset info in the trx system header */
#define TRX_SYS_MYSQL_LOG_INFO (UNIV_PAGE_SIZE - 1000)
/** magic number which is TRX_SYS_MYSQL_LOG_MAGIC_N if we have valid data in the
 MySQL binlog info */
constexpr uint32_t TRX_SYS_MYSQL_LOG_MAGIC_N_FLD = 0;
/** high 4 bytes of the offset within that file */
constexpr uint32_t TRX_SYS_MYSQL_LOG_OFFSET_HIGH = 4;
/** low 4 bytes of the offset within that file */
constexpr uint32_t TRX_SYS_MYSQL_LOG_OFFSET_LOW = 8;
/** MySQL log file name */
constexpr uint32_t TRX_SYS_MYSQL_LOG_NAME = 12;

/** Reserve next 8 bytes for transaction number up to which GTIDs
are persisted to table */
#define TRX_SYS_TRX_NUM_GTID \
  (TRX_SYS_MYSQL_LOG_INFO + TRX_SYS_MYSQL_LOG_NAME + TRX_SYS_MYSQL_LOG_NAME_LEN)
#define TRX_SYS_TRX_NUM_END (TRX_SYS_TRX_NUM_GTID + 8)
/** Doublewrite buffer */
/** @{ */
/** The offset of the doublewrite buffer header on the trx system header page */
#define TRX_SYS_DOUBLEWRITE (UNIV_PAGE_SIZE - 200)
/*-------------------------------------------------------------*/
/** fseg header of the fseg containing the doublewrite buffer */
constexpr uint32_t TRX_SYS_DOUBLEWRITE_FSEG = 0;
/** 4-byte magic number which shows if we already have created the doublewrite
 buffer */
constexpr uint32_t TRX_SYS_DOUBLEWRITE_MAGIC = FSEG_HEADER_SIZE;
/** page number of the first page in the first sequence of 64 (=
 FSP_EXTENT_SIZE) consecutive pages in the doublewrite buffer */
constexpr uint32_t TRX_SYS_DOUBLEWRITE_BLOCK1 = 4 + FSEG_HEADER_SIZE;
/** page number of the first page in the second sequence of 64 consecutive pages
 in the doublewrite buffer */
constexpr uint32_t TRX_SYS_DOUBLEWRITE_BLOCK2 = 8 + FSEG_HEADER_SIZE;
/** we repeat TRX_SYS_DOUBLEWRITE_MAGIC, TRX_SYS_DOUBLEWRITE_BLOCK1,
 TRX_SYS_DOUBLEWRITE_BLOCK2 so that if the trx sys header is half-written to
 disk, we still may be able to recover the information */
constexpr uint32_t TRX_SYS_DOUBLEWRITE_REPEAT = 12;
/** If this is not yet set to TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED_N,
we must reset the doublewrite buffer, because starting from 4.1.x the
space id of a data page is stored into
FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID. */
constexpr uint32_t TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED = 24 + FSEG_HEADER_SIZE;

/*-------------------------------------------------------------*/
/** Contents of TRX_SYS_DOUBLEWRITE_MAGIC */
constexpr uint32_t TRX_SYS_DOUBLEWRITE_MAGIC_N = 536853855;
/** Contents of TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED */
constexpr uint32_t TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED_N = 1783657386;

/** Size of the doublewrite block in pages */
#define TRX_SYS_DOUBLEWRITE_BLOCK_SIZE FSP_EXTENT_SIZE
/** @} */

/** List of undo tablespace IDs. */
class Space_Ids : public std::vector<space_id_t, ut::allocator<space_id_t>> {
 public:
  void sort() { std::sort(begin(), end()); }

  bool contains(space_id_t id) {
    if (size() == 0) {
      return (false);
    }

    iterator it = std::find(begin(), end(), id);

    return (it != end());
  }

  iterator find(space_id_t id) { return (std::find(begin(), end(), id)); }
};

/** Number of shards created for transactions. */
constexpr size_t TRX_SHARDS_N = 256;

/** Computes shard number for a given trx_id.
@param[in]  trx_id  trx_id for which shard_no should be computed
@return the computed shard number (number in range 0..TRX_SHARDS_N-1) */
inline size_t trx_get_shard_no(trx_id_t trx_id) {
  ut_ad(trx_id != 0);
  return trx_id % TRX_SHARDS_N;
}

#ifndef UNIV_HOTBACKUP
class Trx_by_id_with_min {
  struct Trx_track_hash {
    size_t operator()(const trx_id_t &key) const {
      return static_cast<size_t>(key / TRX_SHARDS_N);
    }
  };

  using By_id = std::unordered_map<trx_id_t, trx_t *, Trx_track_hash>;
  By_id m_by_id;

  /** For observers which use Trx_shard::mutex protection: each transaction id
  in the m_by_id is guaranteed to be at least m_min_id.
  Writes are protected with Trx_shard::mutex.
  Reads can be performed without any latch before accessing m_by_id,
  but care must be taken to interpret the result -
  @see trx_rw_is_active for details.*/
  std::atomic<trx_id_t> m_min_id{0};

 public:
  By_id const &by_id() const { return m_by_id; }
  trx_id_t min_id() const { return m_min_id.load(); }
  trx_t *get(trx_id_t trx_id) const {
    const auto it = m_by_id.find(trx_id);
    trx_t *trx = it == m_by_id.end() ? nullptr : it->second;
    /* We remove trx from active_rw_trxs and change state to
    TRX_STATE_COMMITTED_IN_MEMORY in a same critical section protected by
    Trx_shard's mutex, which we happen to hold here, so we expect the state
    of trx to match its presence in that set */
    ut_ad(trx == nullptr || !trx_state_eq(trx, TRX_STATE_COMMITTED_IN_MEMORY));
    return trx;
  }
  void insert(trx_t &trx) {
    const trx_id_t trx_id = trx.id;
    ut_ad(0 == m_by_id.count(trx_id));
    m_by_id.emplace(trx_id, &trx);
    if (m_by_id.size() == 1 ||
        trx_id < m_min_id.load(std::memory_order_relaxed)) {
      m_min_id.store(trx_id, std::memory_order_release);
    }
  }
  void erase(trx_id_t trx_id) {
    ut_ad(1 == m_by_id.count(trx_id));
    m_by_id.erase(trx_id);
    if (m_min_id.load(std::memory_order_relaxed) == trx_id) {
      // We want at most 1 release store, so we use a local variable for the
      // loop.
      trx_id_t new_min = trx_id + TRX_SHARDS_N;
      if (!m_by_id.empty()) {
#ifdef UNIV_DEBUG
        // These asserts ensure while loop terminates:
        const trx_id_t some_id = m_by_id.begin()->first;
        ut_a(new_min <= some_id);
        ut_a((some_id - new_min) % TRX_SHARDS_N == 0);
#endif /* UNIV_DEBUG */
        while (m_by_id.count(new_min) == 0) {
          new_min += TRX_SHARDS_N;
        }
      }
      m_min_id.store(new_min, std::memory_order_release);
    }
  }
};

/** Shard for subset of transactions. */
struct Trx_shard {
  /** Mapping from trx->id to trx of active rw transactions.
  The peek() interface can only be used safely for the min_id().
  Use latch_and_execute() interface to access other members. */
  ut::Cacheline_padded<ut::Guarded<Trx_by_id_with_min, LATCH_ID_TRX_SYS_SHARD>>
      active_rw_trxs;
};

/** The transaction system central memory data structure. */
struct trx_sys_t {
  /* Members protected by neither trx_sys_t::mutex nor serialisation_mutex. */
  char pad0[ut::INNODB_CACHE_LINE_SIZE];

  /** @{ */

  /** Multi version concurrency control manager */

  MVCC *mvcc;

  /** Vector of pointers to rollback segments. These rsegs are iterated
  and added to the end under a read lock. They are deleted under a write
  lock while the vector is adjusted. They are created and destroyed in
  single-threaded mode. */
  Rsegs rsegs;

  /** Vector of pointers to rollback segments within the temp tablespace;
  This vector is created and destroyed in single-threaded mode so it is not
  protected by any mutex because it is read-only during multi-threaded
  operation. */
  Rsegs tmp_rsegs;

  /** Length of the TRX_RSEG_HISTORY list (update undo logs for committed
  transactions). */
  std::atomic<uint64_t> rseg_history_len;

  /** @} */

  /* Members protected by either trx_sys_t::mutex or serialisation_mutex. */
  char pad1[ut::INNODB_CACHE_LINE_SIZE];

  /** @{ */

  /** The smallest number not yet assigned as a transaction id
  or transaction number. This is declared as atomic because it
  can be accessed without holding any mutex during AC-NL-RO
  view creation. When it is used for assignment of the trx->id,
  it is synchronized by the trx_sys_t::mutex. When it is used
  for assignment of the trx->no, it is synchronized by the
  trx_sys_t::serialisation_mutex. Note: it might be in parallel
  used for both trx->id and trx->no assignments (for different
  trx_t objects). */
  std::atomic<trx_id_t> next_trx_id_or_no;

  /** @} */

  /* Members protected by serialisation_mutex. */
  char pad2[ut::INNODB_CACHE_LINE_SIZE];

  /** @{ */

  /** Mutex to protect serialisation_list. */
  TrxSysMutex serialisation_mutex;

  /** Tracks minimal transaction id which has received trx->no, but has
  not yet finished commit for the mtr writing the trx commit. Protected
  by the serialisation_mutex. Ordered on the trx->no field. */
  UT_LIST_BASE_NODE_T(trx_t, no_list) serialisation_list;

#ifdef UNIV_DEBUG
  /** Max trx number of read-write transactions added for purge. */
  trx_id_t rw_max_trx_no;
#endif /* UNIV_DEBUG */

  char pad3[ut::INNODB_CACHE_LINE_SIZE];

  /* The minimum trx->no inside the serialisation_list. Protected by
  the serialisation_mutex. Might be read without the mutex. */
  std::atomic<trx_id_t> serialisation_min_trx_no;

  /** @} */

  /* Members protected by the trx_sys_t::mutex. */
  char pad4[ut::INNODB_CACHE_LINE_SIZE];

  /** @{ */

  /** Mutex protecting most fields in this structure (the default one). */
  TrxSysMutex mutex;

  char pad5[ut::INNODB_CACHE_LINE_SIZE];

  /** List of active and committed in memory read-write transactions, sorted
  on trx id, biggest first. Recovered transactions are always on this list. */
  UT_LIST_BASE_NODE_T(trx_t, trx_list) rw_trx_list;

  char pad6[ut::INNODB_CACHE_LINE_SIZE];

  /** List of transactions created for MySQL. All user transactions are
  on mysql_trx_list. The rw_trx_list can contain system transactions and
  recovered transactions that will not be in the mysql_trx_list.
  Additionally, mysql_trx_list may contain transactions that have not yet
  been started in InnoDB. */
  UT_LIST_BASE_NODE_T(trx_t, mysql_trx_list) mysql_trx_list;

  /** Array of Read write transaction IDs for MVCC snapshot. A ReadView would
  take a snapshot of these transactions whose changes are not visible to it.
  We should remove transactions from the list before committing in memory and
  releasing locks to ensure right order of removal and consistent snapshot. */
  trx_ids_t rw_trx_ids;

  char pad7[ut::INNODB_CACHE_LINE_SIZE];

  /** Mapping from transaction id to transaction instance. */
  Trx_shard shards[TRX_SHARDS_N];

  /** Number of transactions currently in the XA PREPARED state. */
  ulint n_prepared_trx;

  /** True if XA PREPARED trxs are found. */
  bool found_prepared_trx;

  /** @} */

  char pad_after[ut::INNODB_CACHE_LINE_SIZE];

  Trx_shard &get_shard_by_trx_id(trx_id_t trx_id) {
    return trx_sys->shards[trx_get_shard_no(trx_id)];
  }
  template <typename F>
  auto latch_and_execute_with_active_trx(trx_id_t trx_id, F &&f,
                                         const ut::Location &loc) {
    return get_shard_by_trx_id(trx_id).active_rw_trxs.latch_and_execute(
        [&](Trx_by_id_with_min &trx_by_id_with_min) {
          return std::forward<F>(f)(trx_by_id_with_min.get(trx_id));
        },
        loc);
  }
};

#endif /* !UNIV_HOTBACKUP */

/** A list of undo tablespace IDs found in the TRX_SYS page.
This cannot be part of the trx_sys_t object because it is initialized before
that object is created. These are the old type of undo tablespaces that do not
have space_IDs in the reserved range nor contain an RSEG_ARRAY page. */
extern Space_Ids *trx_sys_undo_spaces;

#ifndef UNIV_HOTBACKUP

/** When a trx id which is zero modulo this number (which must be a power of
two) is assigned, the field TRX_SYS_TRX_ID_STORE on the transaction system
page is updated */
constexpr trx_id_t TRX_SYS_TRX_ID_WRITE_MARGIN = 256;

/** Acquire the trx_sys->mutex. */
static inline void trx_sys_mutex_enter() { mutex_enter(&trx_sys->mutex); }

/** Release the trx_sys->mutex. */
static inline void trx_sys_mutex_exit() { trx_sys->mutex.exit(); }

#ifdef UNIV_DEBUG

/** Test if trx_sys->mutex is owned. */
static inline bool trx_sys_mutex_own() { return trx_sys->mutex.is_owned(); }

/** Test if trx_sys->serialisation_mutex is owned. */
static inline bool trx_sys_serialisation_mutex_own() {
  return trx_sys->serialisation_mutex.is_owned();
}
#endif

/** Acquire the trx_sys->serialisation_mutex. */
static inline void trx_sys_serialisation_mutex_enter() {
  mutex_enter(&trx_sys->serialisation_mutex);
}

/** Release the trx_sys->serialisation_mutex. */
static inline void trx_sys_serialisation_mutex_exit() {
  trx_sys->serialisation_mutex.exit();
}

#endif /* !UNIV_HOTBACKUP */

#include "trx0sys.ic"

#endif
