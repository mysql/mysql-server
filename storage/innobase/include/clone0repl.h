/*****************************************************************************

Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

/** @file include/clone0repl.h
 GTID persistence interface

 *******************************************************/

#ifndef CLONE_REPL_INCLUDE
#define CLONE_REPL_INCLUDE

#include <vector>
#include "clone0monitor.h"
#include "os0thread-create.h"
#include "sql/rpl_gtid.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "trx0sys.h"

class Clone_persist_gtid;

/** Serialized GTID information size */
static const size_t GTID_INFO_SIZE = 64;

/** GTID format version. */
static const uint32_t GTID_VERSION = 1;

/** Serialized GTID */
using Gtid_info = std::array<unsigned char, GTID_INFO_SIZE>;

/** List of GTIDs */
using Gitd_info_list = std::vector<Gtid_info>;

/** GTID descriptor with version information. */
struct Gtid_desc {
  /** If GTID descriptor is set. */
  bool m_is_set;
  /** Serialized GTID information. */
  Gtid_info m_info;
  /* GTID version. */
  uint32_t m_version;
};

/** Persist GTID along with transaction commit */
class Clone_persist_gtid {
 public:
  /** Constructor: start gtid thread */
  Clone_persist_gtid() {
    m_event = os_event_create();
    /* No background is created yet. */
    m_thread_active.store(false);
    m_gtid_trx_no.store(0);
    m_flush_number.store(0);
    m_explicit_request.store(false);
    m_active_number.store(m_flush_number.load() + 1);
    /* We accept GTID even before the background service is started. This
    is needed because we add GTIDs from undo log during recovery. */
    m_active.store(true);
    m_num_gtid_mem.store(0);
    m_flush_in_progress.store(false);
    m_close_thread.store(false);
  }

  /** Destructor: stop gtid thread */
  ~Clone_persist_gtid() {
    ut_ad(!m_thread_active.load());
    stop();
    os_event_destroy(m_event);
  }

  /** Start GTID persistence and background thread.
  @return true, if successful. */
  bool start();

  /* Stop GTID persistence. */
  void stop();

  /* Wait for immediate flush.
  @param[in]    compress_gtid   request GTID compression.
  @param[in]    early_timeout   don't wait long if flush is blocked.
  @param[in]    cbk             alert callback for long wait. */
  void wait_flush(bool compress_gtid, bool early_timeout, Clone_Alert_Func cbk);

  /**@return true, if GTID persistence is active. */
  bool is_active() const { return (m_active.load()); }

  /**@return true, if GTID thread is active. */
  bool is_thread_active() const { return (m_thread_active.load()); }

  /** Get oldest transaction number for which GTID is not persisted to table.
  Transactions committed after this point should not be purged.
  @return oldest transaction number. */
  trx_id_t get_oldest_trx_no() {
    trx_id_t ret_no = m_gtid_trx_no.load();
    /* Should never be zero. It can be set to max only before
    GTID persister is active and no GTID is persisted. */
    ut_ad(ret_no > 0 || srv_force_recovery >= SRV_FORCE_NO_UNDO_LOG_SCAN);
    if (ret_no == TRX_ID_MAX) {
      ut_ad(!is_thread_active());
      ut_ad(m_num_gtid_mem.load() == 0);
    } else if (m_num_gtid_mem.load() == 0) {
      /* For all transactions that are committed before this function is called
      have their GTID flushed if flush is not in progress. "flush not in
      progress" is sufficient but not necessary condition here. This is mainly
      for cases when there is no GTID and purge doesn't need to wait. */
      if (!m_flush_in_progress.load()) {
        ret_no = TRX_ID_MAX;
      }
    }
    return (ret_no);
  }

  /** Set oldest transaction number for which GTID is not persisted to table.
  This is set during recovery from persisted value.
  @param[in]    max_trx_no      transaction number */
  void set_oldest_trx_no_recovery(trx_id_t max_trx_no) {
    ib::info(ER_IB_CLONE_GTID_PERSIST)
        << "GTID recovery trx_no: " << max_trx_no;
    /* Zero is special value. It is from old database without GTID
    persistence. */
    if (max_trx_no == 0) {
      max_trx_no = TRX_ID_MAX;
    }
    m_gtid_trx_no.store(max_trx_no);
  }

  /** Get transaction GTID information.
  @param[in,out]        trx             innodb transaction
  @param[out]           gtid_desc       descriptor with serialized GTID */
  void get_gtid_info(trx_t *trx, Gtid_desc &gtid_desc);

  /** Set transaction flag to persist GTID and check if space need to be
  allocated for GTID.
  @param[in,out]        trx             current innodb transaction
  @param[in]            prepare         if operation is Prepare
  @param[in]            rollback        if operation is Rollback
  @param[out]           set_explicit    if explicitly set to persist GTID
  @return true, if undo space needs to be allocated. */
  bool trx_check_set(trx_t *trx, bool prepare, bool rollback,
                     bool &set_explicit);

  /** Check if current transaction has GTID.
  @param[in]            trx             innodb transaction
  @param[in,out]        thd             session THD
  @param[out]           passed_check    true if transaction is good for GTID
  @return true, if transaction has valid GTID. */
  bool has_gtid(trx_t *trx, THD *&thd, bool &passed_check);

  /** Check if GTID persistence is set
  @param[in]    trx     current innnodb transaction
  @return GTID storage type. */
  trx_undo_t::Gtid_storage persists_gtid(const trx_t *trx);

  /** Set or reset GTID persist flag in THD.
  @param[in,out]        trx     current innnodb transaction
  @param[in]            set     true, if need to set */
  void set_persist_gtid(trx_t *trx, bool set);

  /** Add GTID to in memory list.
  @param[in]    gtid_desc       Descriptor with serialized GTID */
  void add(const Gtid_desc &gtid_desc);

  /** Write GTIDs periodically to disk table. */
  void periodic_write();

  /** Write GTIDs of non Innodb transactions to table. */
  int write_other_gtids();

  /** Disable copy construction */
  Clone_persist_gtid(Clone_persist_gtid const &) = delete;

  /** Disable assignment */
  Clone_persist_gtid &operator=(Clone_persist_gtid const &) = delete;

 private:
  /** Check if GTID needs to persist at XA prepare.
  @param[in]            thd             session THD
  @param[in,out]        trx             current innnodb transaction
  @param[in]            found_gtid      session is owning GTID
  @param[in,out]        alloc           in:transaction checks are passed
                                        out:GTID space need to be allocated
  @return true, if GTID needs to be persisted */
  bool check_gtid_prepare(THD *thd, trx_t *trx, bool found_gtid, bool &alloc);

  /** Check if GTID needs to persist at commit.
  @param[in]            thd             session THD
  @param[in]            found_gtid      session is owning GTID
  @param[out]           set_explicit    if explicitly set to persist GTID
  @return true, if GTID needs to be persisted */
  bool check_gtid_commit(THD *thd, bool found_gtid, bool &set_explicit);

  /** Check if GTID needs to persist at rollback.
  @param[in]            thd             session THD
  @param[in,out]        trx             current innnodb transaction
  @param[in]            found_gtid      session is owning GTID
  @return true, if GTID needs to be persisted */
  bool check_gtid_rollback(THD *thd, trx_t *trx, bool found_gtid);

  /** Wait for gtid thread to start, finish or flush.
  @param[in]    start           if waiting for start
  @param[in]    flush           wait for immediate flush
  @param[in]    flush_number    wait flush to reach this number
  @param[in]    compress        wait also for compression
  @param[in]    early_timeout   don't wait long if flush is blocked
  @param[in]    cbk             alert callback for long wait
  @return true if successful. */
  bool wait_thread(bool start, bool flush, uint64_t flush_number, bool compress,
                   bool early_timeout, Clone_Alert_Func cbk);

  /** @return current active GTID list */
  Gitd_info_list &get_active_list() {
    ut_ad(trx_sys_serialisation_mutex_own());
    return (get_list(m_active_number));
  }

  /** @return GTID list by number.
  @param[in]    list_number     list number
  @return GTID list reference. */
  Gitd_info_list &get_list(uint64_t list_number) {
    int list_index = (list_number & static_cast<uint64_t>(1));
    return (m_gtids[list_index]);
  }

  /** Check if we need to skip write or compression based on debug variables.
  @param[in]    compression     check for compression
  @return true, if we should skip. */
  bool debug_skip_write(bool compression);

  /** Request immediate flush of all GTIDs accumulated.
  @param[in]    compress        request compression of GTID table
  @return flush list number to track and wait for flush to complete. */
  uint64_t request_immediate_flush(bool compress) {
    trx_sys_serialisation_mutex_enter();
    /* We want to flush all GTIDs. */
    uint64_t request_number = m_active_number.load();
    /* If no GTIDs added to active, wait for previous index. */
    if (m_num_gtid_mem.load() == 0) {
      ut_a(request_number > 0);
      --request_number;
    }
    m_flush_request_number = request_number;
    trx_sys_serialisation_mutex_exit();

    if (compress) {
      m_explicit_request.store(true);
    }
    return (request_number);
  }

  /** Check if flush has finished up to a list number.
  @param[in]    request_number  flush request number
  @return true, if it is already flushed. */
  bool check_flushed(uint64_t request_number) const {
    return (m_flush_number >= request_number);
  }

  /** @return true, iff background needs to flush immediately. */
  bool flush_immediate() const {
    return (m_flush_number < m_flush_request_number || m_explicit_request);
  }

  /** Check if GTID compression is necessary based on threshold.
  @return true, if GTID table needs to be compressed. */
  bool check_compress();

  /** Switch active GTID list. */
  uint64_t switch_active_list() {
    /* Switch active list under transaction system mutex. */
    ut_ad(trx_sys_serialisation_mutex_own());
    uint64_t flush_number = m_active_number;
    ++m_active_number;
    m_compression_gtid_counter += m_num_gtid_mem;
    m_num_gtid_mem.store(0);
#ifdef UNIV_DEBUG
    /* The new active list must have no elements. */
    auto &active_list = get_active_list();
    ut_ad(active_list.size() == 0);
#endif
    return (flush_number);
  }

  /** Persist GTID to gtid_executed table.
  @param[in]            flush_list_number       list number to flush
  @param[in,out]        table_gtid_set          GTIDs in table during recovery
  @param[in,out]        sid_map                 SID map for GTIDs
  @return mysql error code. */
  int write_to_table(uint64_t flush_list_number, Gtid_set &table_gtid_set,
                     Sid_map &sid_map);

  /** Update transaction number up to which GTIDs are flushed to table.
  @param[in]    new_gtid_trx_no GTID transaction number */
  void update_gtid_trx_no(trx_id_t new_gtid_trx_no);

  /** Write all GTIDs to table and update GTID transaction number.
  @param[in,out]        thd     current session thread */
  void flush_gtids(THD *thd);

  /** @return true iff number of GTIDs in active list exceeded threshold. */
  bool check_max_gtid_threshold();

 private:
  /** Time threshold to trigger persisting GTID. Insert GTID once per 1k
  transactions or every 100 millisecond. */
  static constexpr std::chrono::milliseconds s_time_threshold{100};

  /** Threshold for the count for compressing GTID. */
  const static uint32_t s_compression_threshold = 50;

  /** Number of transaction/GTID threshold for writing to disk table. */
  const static int s_gtid_threshold = 1024;

  /** Maximum Number of transaction/GTID to hold. Transaction commits
  must wait beyond this point. Not expected to happen as GTIDs are
  compressed and written together. */
  const static int s_max_gtid_threshold = 1024 * 1024;

  /** Two lists of GTID. One of them is active where running transactions
  add their GTIDs. Other list is used to persist them to table from time
  to time. */
  Gitd_info_list m_gtids[2];

  /** Number of the current GTID list. Increased when list is switched */
  std::atomic<uint64_t> m_active_number;

  /** Number up to which GTIDs are flushed. Increased when list is flushed.*/
  std::atomic<uint64_t> m_flush_number;

  /** If explicit request to flush is made. */
  std::atomic<bool> m_explicit_request;

  /** Number for which last flush request was made. */
  uint64_t m_flush_request_number{0};

  /** Event for GTID background thread. */
  os_event_t m_event;

  /** Counter to keep track of the number of writes till it reaches
  compression threshold. */
  uint32_t m_compression_counter{0};

  /** Counter to keep number of GTIDs flushed before compression. */
  uint32_t m_compression_gtid_counter{0};

  /* Oldest transaction number for which GTID is not persisted. */
  std::atomic<uint64_t> m_gtid_trx_no;

  /** Number of GTID accumulated in memory */
  std::atomic<int> m_num_gtid_mem;

  /** Flush of GTID is in progress. */
  std::atomic<bool> m_flush_in_progress;

  /** Set to true, when the background thread is asked to exit. */
  std::atomic<bool> m_close_thread;

  /** true, if background thread is active.*/
  std::atomic<bool> m_thread_active;

  /** true, if GTID persistence is active.*/
  std::atomic<bool> m_active;
};

#endif /* CLONE_REPL_INCLUDE */
