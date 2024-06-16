/*****************************************************************************

Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

/** @file clone/clone0repl.cc
 Innodb Clone Replication Coordinates

 *******************************************************/
#include "clone0repl.h"
#include "clone0api.h"
#include "clone0clone.h"
#include "mysql/gtid/gtid.h"
#include "sql/field.h"
#include "sql/mysqld.h"
#include "sql/rpl_gtid_persist.h"
#include "sql/sql_class.h"
#include "sql/sql_thd_internal_api.h"

/* To get current session thread default THD */
THD *thd_get_current_thd();

void Clone_persist_gtid::add(const Gtid_desc &gtid_desc) {
  /* Check if valid descriptor. */
  if (!gtid_desc.m_is_set) {
    return;
  }
  /* Check if GTID persistence is active. */
  if (!is_active() || gtid_table_persistor == nullptr) {
    return;
  }
  ut_ad(trx_sys_serialisation_mutex_own());

  /* If too many GTIDs are accumulated, wait for all to get flushed. Ignore
  timeout and loop to avoid possible hang. The insert should already be
  slowed down by the wait here. */
  if (check_max_gtid_threshold() && is_thread_active()) {
    trx_sys_serialisation_mutex_exit();
    wait_flush(false, false, nullptr);
    trx_sys_serialisation_mutex_enter();
  }

  ut_ad(trx_sys_serialisation_mutex_own());
  /* Get active GTID list */
  auto &current_gtids = get_active_list();

  /* Add input GTID to the set */
  current_gtids.push_back(gtid_desc);
  /* Atomic increment. */
  int current_value = ++m_num_gtid_mem;

  /* Wake up background if GTIDs crossed threshold. */
  if (current_value == s_gtid_threshold) {
    os_event_set(m_event);
  }

  DBUG_EXECUTE_IF("dont_compress_gtid_table", {
    /* For predictable outcome of mtr test we flush the GTID immediately. */
    trx_sys_serialisation_mutex_exit();
    wait_flush(false, false, nullptr);
    trx_sys_serialisation_mutex_enter();
  });
}

trx_undo_t::Gtid_storage Clone_persist_gtid::persists_gtid(const trx_t *trx) {
  auto thd = trx->mysql_thd;

  if (thd == nullptr) {
    thd = thd_get_current_thd();
  }

  if (thd == nullptr || !thd->se_persists_gtid()) {
    /* No need to persist GTID. */
    return trx_undo_t::Gtid_storage::NONE;
  }

  if (thd->is_extrenal_xa()) {
    /* Need to persist both XA prepare and commit GTID. */
    return trx_undo_t::Gtid_storage::PREPARE_AND_COMMIT;
  }

  /* Need to persist only commit GTID. */
  return trx_undo_t::Gtid_storage::COMMIT;
}

void Clone_persist_gtid::set_persist_gtid(trx_t *trx, bool set) {
  bool thd_check = false;
  auto thd = trx->mysql_thd;

  /* Check conditions if the session is good for persisting GTID. */
  static_cast<void>(has_gtid(trx, thd, thd_check));

  /* For attachable transaction, skip both set and reset. */
  if (thd == nullptr || thd->is_attachable_transaction_active() ||
      trx->internal) {
    return;
  }

  /* First do the reset. */
  if (!set) {
    thd->reset_gtid_persisted_by_se();
    /* Reset transaction flag also. */
    trx->persists_gtid = false;
    return;
  }

  ut_ad(set);
  /* Don't set if thread checks have failed. */
  if (!thd_check) {
    return;
  }

  /* This is an optimization to skip GTID allocation, if transaction
  is guaranteed to not have GTID. */
  if (!thd->se_persists_gtid()) {
    auto gtid_next = thd->variables.gtid_next.type;

    if (opt_bin_log) {
      /* This transaction would not have GTID. */
      if (gtid_next == ANONYMOUS_GTID) {
        return;
      }
    } else {
      /* If binary log is disabled, GTID must be directly assigned. */
      if (gtid_next != ASSIGNED_GTID) {
        return;
      }
    }
  }

  /* Test case to validate direct write to gtid_executed table. */
  DBUG_EXECUTE_IF("simulate_err_on_write_gtid_into_table", { return; });
  DBUG_EXECUTE_IF("disable_se_persists_gtid", { return; });

  /* Set or Reset GTID persist flag in THD session. The transaction flag
  is set later during prepare/commit/rollback. */
  thd->set_gtid_persisted_by_se();
}

bool Clone_persist_gtid::trx_check_set(trx_t *trx, bool prepare, bool rollback,
                                       bool &set_explicit) {
  auto thd = trx->mysql_thd;
  bool alloc_check = false;

  bool gtid_exists = has_gtid(trx, thd, alloc_check);

  set_explicit = false;

  if (prepare) {
    /* Check for XA prepare. */
    gtid_exists = check_gtid_prepare(thd, trx, gtid_exists, alloc_check);
  } else if (rollback) {
    /* Check for Rollback. */
    gtid_exists = check_gtid_rollback(thd, trx, gtid_exists);
    alloc_check = gtid_exists;
  } else {
    /* Check for Commit. */
    gtid_exists = check_gtid_commit(thd, gtid_exists, set_explicit);
    alloc_check = gtid_exists;
  }
  /* Set transaction to persist GTID. This is one single point of decision
  in prepare/commit and rollback. Once set, the GTID would be persisted
  in undo and added to in memory GTID list to be written to gtid_executed
  table later. */
  trx->persists_gtid = gtid_exists;
  return (alloc_check);
}

bool Clone_persist_gtid::check_gtid_prepare(THD *thd, trx_t *trx,
                                            bool found_gtid, bool &alloc) {
  /* Skip GTID if alloc checks have failed. */
  if (!alloc) {
    return (false);
  }
  alloc = false;
  /* Skip GTID if mysql internal XA Prepare created by binlog. */
  if (trx_is_mysql_xa(trx) || thd->is_one_phase_commit()) {
    return (false);
  }
  auto thd_trx = thd->get_transaction();
  auto xid_state = thd_trx->xid_state();
  /* In permissive mode GTID could be assigned during XA commit/rollback. */
  if (xid_state->has_state(XID_STATE::XA_IDLE)) {
    auto gtid_mode = global_gtid_mode.get();
    if (gtid_mode == Gtid_mode::ON_PERMISSIVE ||
        gtid_mode == Gtid_mode::OFF_PERMISSIVE) {
      alloc = true;
    }
  }
  /* Skip GTID if not set */
  if (!found_gtid) {
    return (false);
  }
  /* Skip GTID if External XA transaction is not in IDLE state. */
  if (!xid_state->has_state(XID_STATE::XA_IDLE)) {
    ut_d(ut_error);
    ut_o(return (false));
  }
  /* Skip if SE is not set to persist GTID.*/
  if (!thd->se_persists_gtid()) {
    return (false);
  }
  alloc = true;
  return (true);
}

bool Clone_persist_gtid::check_gtid_commit(THD *thd, bool found_gtid,
                                           bool &set_explicit) {
  set_explicit = (thd == nullptr) ? false : thd->se_persists_gtid_explicit();

  if (!found_gtid) {
    ut_ad(!set_explicit || thd->is_attachable_transaction_active());
    return (false);
  }

  /* Persist if SE is set to persist GTID.*/
  return (thd->se_persists_gtid());
}

bool Clone_persist_gtid::check_gtid_rollback(THD *thd, trx_t *trx,
                                             bool found_gtid) {
  if (!found_gtid) {
    return (false);
  }

  ut_ad(trx_can_be_handled_by_current_thread_or_is_hp_victim(trx));

  /* Skip GTID if XA transaction not in prepared state. */
  if (trx->state.load(std::memory_order_relaxed) != TRX_STATE_PREPARED) {
    return (false);
  }

  /* We don't need to persist GTID for binlog internal XA transaction.
  One issue here is xid could be NULL when
   1. External XA transaction is rolled back by XID
   2. binlog internal XA transaction is rolled back during recovery
  This is a side effect of trx_get_trx_by_xid() resetting the xid. We
  cannot use trx_is_mysql_xa() to differentiate external XA transactions
  in that case. However, it is safe to assume it is case (1) here as GTID
  is never set for case (2) and input found_gtid should be false. */
  if (!trx->xid->is_null() && trx_is_mysql_xa(trx)) {
    return (false);
  }

  /* Skip GTID if it is rollback in error case. Ideally we should
  not allow prepared transaction to be rolled back on error but
  currently server/replication does rollback and has test for it. */
  auto thd_trx = thd->get_transaction();
  auto xid_state = thd_trx->xid_state();
  if (xid_state->has_state(XID_STATE::XA_ROLLBACK_ONLY)) {
    return (false);
  }

  /* Persist if SE is set to persist GTID.*/
  return (thd->se_persists_gtid());
}

bool Clone_persist_gtid::has_gtid(trx_t *trx, THD *&thd, bool &passed_check) {
  passed_check = false;

  /* Note, that the assertion does nothing useful if thd == nullptr. */
  ut_ad(trx_can_be_handled_by_current_thread_or_is_hp_victim(trx));

  /* Transaction is not associated with mysql foreground session. */
  if (trx->state.load(std::memory_order_relaxed) == TRX_STATE_PREPARED &&
      thd == nullptr) {
    /* For XA transaction, the current transaction THD could be NULL. Also
    check the default THD of current thread. */
    thd = thd_get_current_thd();
  }
  /* Transaction should be associated with a THD session object. */
  if (thd == nullptr) {
    return (false);
  }
  /* Transaction is internal innodb transaction. */
  if (trx->internal) {
    return (false);
  }

  /* Attachable transactions can be started and committed while
  the main transaction is in progress. We don't want to consider
  GTID persistence for such transactions. */
  if (thd->is_attachable_transaction_active()) {
    return (false);
  }

  /* Explicit request is made while slave threads wants
  to persist GTID for non innodb table. */
  bool explicit_request = thd->se_persists_gtid_explicit();

  if (!explicit_request) {
    /* Transaction is updating GTID table implicitly. */
    if (thd->is_operating_gtid_table_implicitly ||
        thd->is_operating_substatement_implicitly) {
      /* On slave, explicit request can be set after making some
      modification and thus allocating undo. Allow space for GTID in
      undo log for sub-statements always. */
      passed_check = thd->is_operating_substatement_implicitly;
      return (false);
    }
  }

  /* Transaction passed checks other than GTID. */
  passed_check = true;

  auto &trx_gtid = thd->owned_gtid;
  /* Transaction is not assigned any GTID */
  if (trx_gtid.is_empty() || trx_gtid.sidno == THD::OWNED_SIDNO_ANONYMOUS) {
    return (false);
  }
  return (true);
}

void Clone_persist_gtid::get_gtid_info(trx_t *trx, Gtid_desc &gtid_desc) {
  gtid_desc.m_is_set = false;
  /* Check if transaction has GTID */
  if (!trx->persists_gtid) {
    return;
  }
  bool thd_check;
  auto thd = trx->mysql_thd;

  if (!has_gtid(trx, thd, thd_check)) {
    ut_d(ut_error);
    ut_o(return);
  }

  uint32_t encoded_version = GTID_VERSION;
  DBUG_EXECUTE_IF("gtid_persistor_use_gtid_version_one", encoded_version = 1;);
  gtid_desc.m_version = encoded_version;
  const auto &trx_gtid = thd->owned_gtid;
  const auto &trx_tsid = thd->owned_tsid;

  ut_ad(trx_gtid.sidno > 0);
  ut_ad(trx_gtid.gno > 0);
  ut_ad(trx_gtid.gno < GNO_END);

  /* Build GTID string. */
  gtid_desc.m_info.fill(0);
  auto char_buf = reinterpret_cast<char *>(&gtid_desc.m_info[0]);
  mysql::gtid::Gtid tsid_gtid(trx_tsid, trx_gtid.gno);
  std::size_t len = 0;
  if (unlikely(encoded_version ==
               1)) {  // encode version 1, for debug purposes only
    len = trx_gtid.to_string(trx_tsid, char_buf);
  } else {  // encode version 2
    len = tsid_gtid.encode_gtid_tagged(
        reinterpret_cast<unsigned char *>(char_buf));
  }
  ut_a((size_t)len <= GTID_INFO_SIZE);
  gtid_desc.m_is_set = true;
}

int Clone_persist_gtid::write_other_gtids() {
  int err = 0;
  if (opt_bin_log) {
    err = gtid_state->save_gtids_of_last_binlog_into_table();
  }
  return (err);
}

bool Clone_persist_gtid::check_compress() {
  /* Check for explicit flush request. */
  if (m_explicit_request.load()) {
    return (true);
  }

  /* Wait for explicit request when debug compress request is set.
  This is to make the debug test outcome predictable. */
  DBUG_EXECUTE_IF("compress_gtid_table", { return false; });

  /* Check replication global threshold on number of GTIDs. */
  if (!opt_bin_log && gtid_executed_compression_period != 0 &&
      m_compression_gtid_counter > gtid_executed_compression_period) {
    return (true);
  }

  /* Check local threshold on number of flush. */
  if (m_compression_counter >= s_compression_threshold) {
    return (true);
  }

  return (false);
}

bool Clone_persist_gtid::debug_skip_write(bool compression) {
  bool skip = false;
  DBUG_EXECUTE_IF("simulate_flush_commit_error", { skip = true; });
  DBUG_EXECUTE_IF("simulate_err_on_write_gtid_into_table", { skip = true; });
  DBUG_EXECUTE_IF("disable_gtid_background_persister", { skip = true; });
  if (compression) {
    DBUG_EXECUTE_IF("dont_compress_gtid_table", { skip = true; });
  }
  return (skip);
}

int Clone_persist_gtid::write_to_table(uint64_t flush_list_number,
                                       Gtid_set &table_gtid_set,
                                       Tsid_map &tsid_map) {
  int err = 0;
  Gtid_set write_gtid_set(&tsid_map, nullptr);

  /* Allocate some intervals from stack */
  static const int PREALLOCATED_INTERVAL_COUNT = 64;
  Gtid_set::Interval iv[PREALLOCATED_INTERVAL_COUNT];
  write_gtid_set.add_interval_memory(PREALLOCATED_INTERVAL_COUNT, iv);

  auto &flush_list = get_list(flush_list_number);
  /* Extract GTIDs from flush list. */
  for (auto &gtid_desc : flush_list) {
    auto status = RETURN_STATUS_UNREPORTED_ERROR;
    if (gtid_desc.m_version == 1) {
      auto gtid_str = reinterpret_cast<const char *>(&(gtid_desc.m_info[0]));
      status = write_gtid_set.add_gtid_text(gtid_str);
    } else {  // version 2
      auto gtid_str =
          reinterpret_cast<const unsigned char *>(&(gtid_desc.m_info[0]));
      mysql::gtid::Gtid saved_gtid;
      auto gtid_bytes_read =
          saved_gtid.decode_gtid_tagged(gtid_str, GTID_INFO_SIZE);
      if (gtid_bytes_read != 0) {
        status = write_gtid_set.add_gtid(saved_gtid);
      }
    }
    if (status != RETURN_STATUS_OK) {
      err = ER_INTERNAL_ERROR;
      return (err);
    }
  }

  /* Skip call if error test. We don't want to catch this error here. */
  if (debug_skip_write(false)) {
    flush_list.clear();
    ut_ad((m_flush_number + 1) == flush_list_number);
    m_flush_number.store(flush_list_number);
    return (0);
  }

  bool is_recovery = !m_thread_active.load();
  if (is_recovery) {
    /* During recovery, eliminate GTIDs already in  gtid_executed table. */
    write_gtid_set.remove_gtid_set(&table_gtid_set);
    table_gtid_set.add_gtid_set(&write_gtid_set);
  } else {
    /* Handle concurrent write by other threads when binlog is enabled. */
    gtid_state->update_prev_gtids(&write_gtid_set);
  }

  /* Write GTIDs to table. */
  if (!write_gtid_set.is_empty()) {
    ++m_compression_counter;
    err = gtid_table_persistor->save(&write_gtid_set, false);
  }

  /* Clear flush list and return */
  flush_list.clear();
  ut_ad((m_flush_number + 1) == flush_list_number);
  m_flush_number.store(flush_list_number);
  return (err);
}

void Clone_persist_gtid::update_gtid_trx_no(trx_id_t new_gtid_trx_no) {
  auto trx_no = m_gtid_trx_no.load();
  /* Noting to do if number hasn't increased. */
  if (trx_no != TRX_ID_MAX && trx_no >= new_gtid_trx_no) {
    ut_ad(trx_no == new_gtid_trx_no);
    return;
  }
  /* Update in memory variable. */
  m_gtid_trx_no.store(new_gtid_trx_no);

  /* Persist to disk. This would be useful during recovery. */
  trx_sys_persist_gtid_num(new_gtid_trx_no);

  /* Wake up purge thread. */
  srv_purge_wakeup();
}

void Clone_persist_gtid::flush_gtids(THD *thd) {
  int err = 0;
  Tsid_map tsid_map(nullptr);
  Gtid_set table_gtid_set(&tsid_map, nullptr);

  DBUG_EXECUTE_IF("gtid_persist_flush_disable", return;);

  /* During recovery, fetch existing GTIDs from gtid_executed table. */
  bool is_recovery = !m_thread_active.load();
  if (is_recovery && !opt_initialize) {
    gtid_table_persistor->fetch_gtids(&table_gtid_set);
  }

  bool explicit_request = m_explicit_request.load();

  trx_sys_serialisation_mutex_enter();
  /* Get oldest transaction number that is yet to be committed. Any transaction
  with lower transaction number is committed and is added to GTID list. */
  auto oldest_trx_no = trx_sys_oldest_trx_no();
  bool compress_recovery = false;
  /* Check and write if any GTID is accumulated. */
  if (m_num_gtid_mem.load() != 0) {
    m_flush_in_progress.store(true);
    /* Switch active list and get the previous list to write to disk table. */
    auto flush_list_number = switch_active_list();
    /* Exit trx mutex during write to table. */
    trx_sys_serialisation_mutex_exit();
    err = write_to_table(flush_list_number, table_gtid_set, tsid_map);
    m_flush_in_progress.store(false);
    /* Compress always after recovery, if GTIDs are added. */
    if (!m_thread_active.load()) {
      compress_recovery = true;
      ib::info(ER_IB_CLONE_GTID_PERSIST) << "GTID compression after recovery. ";
    }
  } else {
    trx_sys_serialisation_mutex_exit();
  }

  if (is_recovery) {
    /* Allocate buffer and fill GTIDs */
    char *gtid_buffer = nullptr;
    auto gtid_buffer_size = table_gtid_set.to_string(&gtid_buffer);
    /* Update GTID set to status for clone recovery. */
    std::string all_gtids;
    if (gtid_buffer_size > 0) {
      all_gtids.assign(gtid_buffer);
    }
    /* Must update GITD status even if no GTID. This call completes
    clone operation. */
    clone_update_gtid_status(all_gtids);
    my_free(gtid_buffer);
  }

  /* Update trx number up to which GTID is written to table. */
  update_gtid_trx_no(oldest_trx_no);

  /* Request Compression once the counter reaches threshold. */
  bool debug_skip = debug_skip_write(true);
  if (err == 0 && !debug_skip && (compress_recovery || check_compress())) {
    m_compression_counter = 0;
    m_compression_gtid_counter = 0;
    /* Write non-innodb GTIDs before compression. */
    write_other_gtids();
    err = gtid_table_persistor->compress(thd);
  }
  if (err != 0) {
    ib::error(ER_IB_CLONE_GTID_PERSIST) << "Error persisting GTIDs to table";
    ut_ad(debug_skip || srv_force_recovery > 0);
  }

  /* Reset the explicit compression request, if our previous check
  for explicit returned true. If the request is made after previous
  check then we do the compression next time. */
  if (explicit_request) {
    m_explicit_request.store(false);
  }
}

bool Clone_persist_gtid::check_max_gtid_threshold() {
  ut_ad(trx_sys_serialisation_mutex_own());
  /* Allow only one GTID to flush at a time. */
  DBUG_EXECUTE_IF("dont_compress_gtid_table",
                  { return m_num_gtid_mem.load() > 0; });
  return m_num_gtid_mem.load() >= s_max_gtid_threshold;
}

void Clone_persist_gtid::periodic_write() {
  auto thd = create_internal_thd();

  /* Allow GTID to be persisted on read only server. */
  thd->set_skip_readonly_check();

  /* Write all accumulated GTIDs while starting server. These GTIDs
  are found in undo log during recovery. We must make sure all these
  GTIDs are flushed and on disk before server is open for new operation
  and new GTIDs are generated.

  Why is it needed ?

  1. mysql.gtid_executed table must be up to date at this point as global
     variable gtid_executed is updated from it when binary log is disabled.

  2. In older versions we used to have only one GTID storage in undo log
     and PREAPARE GTID was stored in same place as COMMIT GTID. We used to
     wait for PREPARE GTID to flush before writing commit GTID. Now this
     limitation is removed and we no longer wait for PREPARE GTID to get
     flushed before COMMIT as we store the PREPARE GTID in separate location.
     However, while upgrading from previous version, there could be XA
     transaction in PREPARED state with GTID stored in place of commit GTID.
     Those GTIDs are also flushed here so that they are not overwritten later
     at COMMIT.
*/
  flush_gtids(thd);

  /* Let the caller wait till first set of GTIDs are persisted to table
  after recovery. */
  m_thread_active.store(true);

  for (;;) {
    /* Exit if last phase of shutdown */
    auto is_shutdown = (srv_shutdown_state.load() >= SRV_SHUTDOWN_CLEANUP);

    if (is_shutdown || m_close_thread.load()) {
      /* Stop accepting any more GTID */
      m_active.store(false);
      break;
    }

    if (!flush_immediate()) {
      os_event_wait_time(m_event, s_time_threshold);
    }
    os_event_reset(m_event);
    /* Write accumulated GTIDs to disk table */
    flush_gtids(thd);
  }

  /* For slow shutdown, consume remaining GTIDs so that undo can be purged. */
  if (m_num_gtid_mem.load() > 0 && srv_fast_shutdown < 2) {
    flush_gtids(thd);
    /* All GTIDs should have been flushed at this point. */
    if (m_num_gtid_mem.load() > 0) {
      ib::warn(ER_IB_MSG_GTID_FLUSH_AT_SHUTDOWN);
    }
  }

  m_active.store(false);
  destroy_internal_thd(thd);
  m_thread_active.store(false);
}

bool Clone_persist_gtid::wait_thread(bool start, bool wait_flush,
                                     uint64_t flush_number, bool compress,
                                     bool early_timeout, Clone_Alert_Func cbk) {
  size_t count = 0;

  auto wait_cond = [&](bool alert, bool &result) {
    if (wait_flush) {
      /* If the thread is not active, return. */
      if (!is_thread_active()) {
        result = false;
        return (0);
      }
      /* If it is flushed up to the point requested. */
      if (check_flushed(flush_number)) {
        /* Check if compression is done if requested. */
        if (!compress || !m_explicit_request.load()) {
          result = false;
          return (0);
        }
      }
    } else if (is_thread_active() == start) {
      result = false;
      return (0);
    }
    if (is_thread_active()) {
      os_event_set(m_event);
    }
    result = true;
    if (alert) {
      ib::info(ER_IB_CLONE_TIMEOUT) << "Waiting for Clone GTID thread";
      if (cbk) {
        auto err = cbk();
        if (err != 0) {
          return (err);
        }
      }
    }

    ++count;
    /* Force early exit from wait loop. We wait for about 1 sec
    for early timeout: 10 x 100ms + 5 iteration for ramp up
    from 1ms to 100ms. */
    if (early_timeout && count > 15) {
      return (ER_QUERY_TIMEOUT);
    }
    return (0);
  };

  bool is_timeout = false;

  /* Sleep starts with 1ms and backs off to 100 ms. */
  Clone_Msec sleep_time(100);
  /* Generate alert message every 5 seconds. */
  Clone_Sec alert_interval(5);
  /* Wait for 5 minutes. */
  Clone_Sec time_out(Clone_Min(5));

  static_cast<void>(Clone_Sys::wait(sleep_time, time_out, alert_interval,
                                    wait_cond, nullptr, is_timeout));
  return (!is_timeout);
}

/** Persist GTID to on disk table from time to time.
@param[in,out]  persist_gtid    GTID persister */
static void clone_gtid_thread(Clone_persist_gtid *persist_gtid) {
  persist_gtid->periodic_write();
}

bool Clone_persist_gtid::start() {
  if (m_thread_active.load()) {
    m_active.store(true);
    return (true);
  }

  srv_threads.m_gtid_persister =
      os_thread_create(clone_gtid_thread_key, 0, clone_gtid_thread, this);
  srv_threads.m_gtid_persister.start();

  if (!wait_thread(true, false, 0, false, false, nullptr)) {
    ib::error(ER_IB_CLONE_TIMEOUT) << "Wait for GTID thread to start timed out";
    ut_d(ut_error);
    ut_o(return (false));
  }
  m_active.store(true);
  return (true);
}

void Clone_persist_gtid::stop() {
  m_close_thread.store(true);
  if (m_thread_active.load() &&
      !wait_thread(false, false, 0, false, false, nullptr)) {
    ib::error(ER_IB_CLONE_TIMEOUT) << "Wait for GTID thread to stop timed out";
    ut_d(ut_error);
  }
}

void Clone_persist_gtid::wait_flush(bool compress_gtid, bool early_timeout,
                                    Clone_Alert_Func cbk) {
  /* During recovery, avoid wait if called before persister is active. */
  if (!is_thread_active()) {
    return;
  }
  auto request_number = request_immediate_flush(compress_gtid);
  os_event_set(m_event);

  /* For RESET BINARY LOGS AND GTIDS we must wait for the flush. */
  auto thd = thd_get_current_thd();
  if (thd != nullptr && thd->is_log_reset()) {
    early_timeout = false;
  }

  /* Wait for flush if test is asking for it. */
  DBUG_EXECUTE_IF("wait_for_flush_gtid_persister", { early_timeout = false; });

  auto success = wait_thread(false, true, request_number, compress_gtid,
                             early_timeout, cbk);
  /* No error for early timeout. */
  if (!success && !early_timeout) {
    ib::error(ER_IB_CLONE_TIMEOUT) << "Wait for GTID thread to flush timed out";
    ut_d(ut_error);
  }
}
