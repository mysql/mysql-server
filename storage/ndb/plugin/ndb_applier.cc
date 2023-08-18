/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

// Implements Ndb_applier related functionality
#include "storage/ndb/plugin/ndb_applier.h"

// Using
#include <algorithm>
#include <numeric>
#include <utility>

#include "sql/dynamic_ids.h"  // ignore_server_ids
#include "sql/rpl_msr.h"      // channel_map
#include "sql/rpl_rli.h"
#include "sql/sql_class.h"
#include "storage/ndb/plugin/ndb_apply_status_table.h"
#include "storage/ndb/plugin/ndb_conflict_trans.h"
#include "storage/ndb/plugin/ndb_log.h"
#include "storage/ndb/plugin/ndb_thd_ndb.h"

Ndb_applier::Ndb_applier(Thd_ndb *thd_ndb, std::string channel_name,
                         Ndb_replica::ChannelPtr channel, Uint32 own_server_id,
                         Uint32 source_server_id, Uint64 source_epoch,
                         std::vector<Uint32> ignored_server_ids,
                         Uint32 num_workers,
                         std::vector<Uint32> written_server_ids)
    : m_thd_ndb(thd_ndb),
      m_rli(thd_ndb->get_thd()->rli_slave),
      m_channel_name(channel_name),
      m_channel(channel),
      m_applier_id(channel->get_next_applier_id()),
      m_own_server_id(own_server_id),
      m_source_server_id(source_server_id),
      m_ignored_server_ids(std::move(ignored_server_ids)),
      m_num_workers(num_workers),
      m_written_server_ids(std::move(written_server_ids)),
      m_incoming_epoch{source_epoch, false, 0, false},
      conflict_mem_root(PSI_INSTRUMENT_ME, 32768) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("channel_name: '%s'", m_channel_name.c_str()));
  DBUG_PRINT("enter", ("applier_id: %u", m_applier_id));
  DBUG_PRINT("enter", ("source_server_id: %d", m_source_server_id));
  assert(channel->get_channel_name() == m_channel_name);
  assert(std::accumulate(violation_counters.begin(), violation_counters.end(),
                         0U) == 0);
  assert(m_incoming_epoch.committed == false);
  assert(m_incoming_epoch.max_rep_epoch == 0);

  DBUG_EXECUTE("test_flags", {
    // No flag(s) set from start
    assert(check_flag(OPS_DEFINED) == false);
    assert(check_flag(TRANS_CONFLICT_DETECTED_THIS_PASS) == false);
    // Set one flag
    set_flag(OPS_DEFINED);
    // Check flag set
    assert(check_flag(OPS_DEFINED) == true);
    // Check other flag not set
    assert(check_flag(TRANS_CONFLICT_DETECTED_THIS_PASS) == false);
    // Clear flags
    m_conflict_flags = 0;
    // No flag set
    assert(check_flag(OPS_DEFINED) == false);
  });
}

Ndb_applier::~Ndb_applier() {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("channel_name: '%s'", m_channel_name.c_str()));
  DBUG_PRINT("enter", ("source_server_id: %d", m_source_server_id));
}

bool Ndb_applier::init() {
  DBUG_TRACE;

  // Create and open the util for working with ndb_apply_status table
  m_apply_status = std::make_unique<Ndb_apply_status_table>(m_thd_ndb);
  if (!m_apply_status->open()) {
    ndb_log_error("Replica: Failed to open 'mysql.ndb_apply_status' table");
    return false;
  }
  return true;
}

bool Thd_ndb::init_applier() {
  DBUG_TRACE;
  if (!will_do_applier_work()) {
    // Don't need Ndb_applier extension
    return true;
  }

  const char *channel_name = m_thd->rli_slave->get_channel();

  Ndb_replica::ChannelPtr channel = ndb_replica->get_channel(channel_name);
  if (channel == nullptr) {
    return false;
  }

  const Uint32 own_server_id = channel->get_own_server_id();
  DBUG_PRINT("info", ("own_server_id: %d", own_server_id));

  // Extract settings from the channel_map
  Uint32 source_server_id;
  std::vector<Uint32> ignore_server_ids;
  Uint32 num_workers;
  {
    channel_map.rdlock();
    const Master_info *channel_mi = channel_map.get_mi(channel_name);
    source_server_id = channel_mi->master_id;
    DBUG_PRINT("info", ("source_server_id: %d", source_server_id));
    num_workers = channel_mi->rli->opt_replica_parallel_workers;

    // Copy list of ignored server_id's
    for (auto id : channel_mi->ignore_server_ids->dynamic_ids) {
      ignore_server_ids.push_back(id);
    }

    channel_map.unlock();
  }

  Uint64 source_epoch;
  Uint64 highest_applied_epoch;
  std::vector<Uint32> existing_server_ids;
  Ndb_apply_status_table apply_status(this);
  if (!apply_status.open()) {
    ndb_log_error("Replica: Failed to open 'mysql.ndb_apply_status' table");
    return false;
  }
  if (!apply_status.load_state(own_server_id, ignore_server_ids,
                               source_server_id, highest_applied_epoch,
                               source_epoch, existing_server_ids)) {
    ndb_log_error(
        "Replica: Failed to load state for channel: '%s', server_id: %d",
        channel_name, own_server_id);
    return false;
  }
  DBUG_PRINT("info", ("highest_applied_epoch: %llu", highest_applied_epoch));
  DBUG_PRINT("info", ("source_epoch: %llu", source_epoch));

  // This initialization is done by all replica workers, only the
  // first call to initialize_max_rep_epoch() will be saved in the channel state
  if (channel->initialize_max_rep_epoch(highest_applied_epoch)) {
    ndb_log_info(
        "Replica: MaxReplicatedEpoch set to %llu (%u/%u) at "
        "Replica start",
        highest_applied_epoch, (Uint32)(highest_applied_epoch >> 32),
        (Uint32)(highest_applied_epoch & 0xffffffff));
  }

  m_applier = std::make_unique<Ndb_applier>(
      this, channel_name, channel, own_server_id, source_server_id,
      source_epoch, ignore_server_ids, num_workers, existing_server_ids);

  if (!m_applier->init()) {
    ndb_log_error("Replica: Failed to init Applier for channel: '%s'",
                  channel_name);
  }

  return true;
}

bool Thd_ndb::will_do_applier_work() const {
  DBUG_TRACE;

  // Check for SQL thread without workers (i.e it will do work itself)
  if (m_thd->system_thread == SYSTEM_THREAD_SLAVE_SQL) {
    if (m_thd->rli_slave->opt_replica_parallel_workers > 0) {
      return false;
    }
    DBUG_PRINT("exit", ("The SQL thread will do work"));
    return true;
  }

  // Check for SQL worker thread
  if (m_thd->system_thread == SYSTEM_THREAD_SLAVE_WORKER) {
    return true;
  }

  return false;
}

int Ndb_applier::atApplyStatusWrite(Uint32 row_server_id, Uint64 row_epoch,
                                    bool &skip_write) {
  DBUG_TRACE;

  // Save written server_id
  m_written_server_ids.push_back(row_server_id);

  // Start of a NDB epoch transaction is detected when the server_id in an
  // incoming ndb_apply_status write matches the source_server_id of the
  // current channel.
  if (row_server_id == m_source_server_id) {
    // verify incoming epoch
    if (!verify_next_epoch(row_epoch)) {
      // Problem with the incoming epoch, return error to stop applier
      return HA_ERR_ROWS_EVENT_APPLY;
    }
    m_first_epoch_verified = true;

    // Save the epoch value as "current epoch"
    m_incoming_epoch.epoch = row_epoch;
    m_incoming_epoch.committed = false;
    m_incoming_epoch.is_epoch_transaction = true;
    assert(!is_serverid_local(row_server_id));

    skip_write = true;  // Deferred until commit

    // Save global max_rep_epoch, this will be used in some conflict algorithms
    m_start_max_rep_epoch = m_channel->get_max_rep_epoch();
    return 0;
  }

  if (is_serverid_local(row_server_id)) {
    DBUG_PRINT("info", ("Recording application of local server %u epoch %llu "
                        " which is %s.",
                        row_server_id, row_epoch,
                        (row_epoch > m_incoming_epoch.max_rep_epoch)
                            ? " new highest."
                            : " older than previously applied"));
    if (row_epoch > m_incoming_epoch.max_rep_epoch) {
      /*
        Store new highest epoch in thdvar.  If we commit successfully
        then this can become the new global max
      */
      m_incoming_epoch.max_rep_epoch = row_epoch;
    }
  }

  return 0;
}

extern bool opt_ndb_applier_allow_skip_epoch;
bool Ndb_applier::verify_next_epoch(Uint64 next_epoch) const {
  DBUG_TRACE;

  const Uint64 current_epoch = m_incoming_epoch.epoch;
  const bool current_epoch_comitted = m_incoming_epoch.committed;
  const bool first_epoch = !m_first_epoch_verified;

  DBUG_PRINT("info", ("First epoch since applier start: %u", first_epoch));
  DBUG_PRINT("enter", ("m_source_server_id: %u", m_source_server_id));
  DBUG_PRINT("info", ("next_epoch: %llu (%llu/%llu)", next_epoch >> 32,
                      next_epoch & 0xffffffff, next_epoch));
  DBUG_PRINT("info", ("current_epoch: %llu (%llu/%llu)", current_epoch >> 32,
                      current_epoch & 0xffffffff, current_epoch));
  DBUG_PRINT("info", ("current_epoch_comitted: %d", current_epoch_comitted));

  // Analysis of incoming epoch depends on whether it's the first or not
  if (first_epoch) {
    // First epoch since applier start, not being too strict about epoch
    // changes, but will warn.
    if (next_epoch < current_epoch) {
      const auto positions = get_log_positions();
      ndb_log_warning(
          "Replica: At SQL thread start "
          "applying epoch %llu/%llu (%llu) from "
          "Source ServerId %u which is lower than "
          "previously applied epoch %llu/%llu (%llu).  "
          "Group Source Log : %s  "
          "Group Source Log Pos : %llu.  "
          "Check replica positioning.  ",
          next_epoch >> 32, next_epoch & 0xffffffff, next_epoch,
          m_source_server_id, current_epoch >> 32, current_epoch & 0xffffffff,
          current_epoch, positions.log_name, positions.start_pos);
      /* Applier not stopped */
      return true;
    }

    if (next_epoch == current_epoch) {
      /**
         Could warn that started on already applied epoch,
         but this is often harmless.
      */
      return true;
    }

    /* next_epoch > current_epoch - fine. */
    return true;
  }

  /**
     ! first_epoch

     Applier has already applied some epoch in this run, so we expect
     either :
      a) previous epoch committed ok and next epoch is higher
                                or
      b) previous epoch not committed and next epoch is the same
         (Retry case)
  */
  if (next_epoch < current_epoch) {
    /* Should never happen */
    const auto positions = get_log_positions();
    ndb_log_error(
        "Replica: SQL thread stopped as "
        "applying epoch %llu/%llu (%llu) from "
        "Source ServerId %u which is lower than "
        "previously applied epoch %llu/%llu (%llu).  "
        "Group Source Log : %s  "
        "Group Source Log Pos : %llu",
        next_epoch >> 32, next_epoch & 0xffffffff, next_epoch,
        m_source_server_id, current_epoch >> 32, current_epoch & 0xffffffff,
        current_epoch, positions.log_name, positions.start_pos);
    return false;  // Stop the applier
  }

  if (next_epoch == current_epoch) {
    /**
       This is ok if we are retrying - e.g. the
       last epoch was not committed
    */
    if (current_epoch_comitted) {
      /* This epoch is committed already, why are we replaying it? */
      const auto positions = get_log_positions();
      ndb_log_error(
          "Replica: SQL thread stopped as attempted to "
          "reapply already committed epoch %llu/%llu (%llu) "
          "from server id %u.  "
          "Group Source Log : %s  "
          "Group Source Log Pos : %llu",
          current_epoch >> 32, current_epoch & 0xffffffff, current_epoch,
          m_source_server_id, positions.log_name, positions.start_pos);
      return false;  // Stop the applier
    }

    /* Probably a retry, no problem. Applier not stopped. */
    return true;
  }

  /**
     next_epoch > current_epoch

     This is the normal case, *unless* the previous epoch
     did not commit - in which case it may be a bug in
     transaction retry.
  */
  if (current_epoch_comitted) {
    return true;
  }

  /**
     We've moved onto a new epoch without committing
     the last - could be a bug, or perhaps the user
     has configured slave-skip-errors?
  */
  if (opt_ndb_applier_allow_skip_epoch) {
    const auto positions = get_log_positions();
    ndb_log_warning(
        "Replica: SQL thread attempting to "
        "apply new epoch %llu/%llu (%llu) while lower "
        "received epoch %llu/%llu (%llu) has not been "
        "committed.  Source Server id : %u.  "
        "Group Source Log : %s  "
        "Group Source Log Pos : %.llu"
        ".  "
        "Continuing as ndb_applier_allow_skip_epoch set.",
        next_epoch >> 32, next_epoch & 0xffffffff, next_epoch,
        current_epoch >> 32, current_epoch & 0xffffffff, current_epoch,
        m_source_server_id, positions.log_name, positions.start_pos);
    /* Continue. Applier not stopped */
    return true;
  }

  const auto positions = get_log_positions();
  ndb_log_error(
      "Replica: SQL thread stopped as attempting to "
      "apply new epoch %llu/%llu (%llu) while lower "
      "received epoch %llu/%llu (%llu) has not been "
      "committed.  Source Server id : %u.  "
      "Group Source Log : %s  "
      "Group Source Log Pos : %llu",
      next_epoch >> 32, next_epoch & 0xffffffff, next_epoch,
      current_epoch >> 32, current_epoch & 0xffffffff, current_epoch,
      m_source_server_id, positions.log_name, positions.start_pos);
  return false;  // Stop applier
}

Ndb_applier::Positions Ndb_applier::get_log_positions() const {
  DBUG_TRACE;

  mysql_mutex_lock(&m_rli->data_lock);
  const char *log_name = m_rli->get_group_master_log_name();
  const auto [start_pos, end_pos] = m_rli->get_group_source_log_start_end_pos();
  mysql_mutex_unlock(&m_rli->data_lock);

  DBUG_PRINT("exit", ("log_name: '%s'", log_name));
  DBUG_PRINT("exit", ("start_pos: %llu", start_pos));
  DBUG_PRINT("exit", ("end_pos: %llu", end_pos));

  return {log_name, start_pos, end_pos};
}

bool Ndb_applier::define_apply_status_operations() {
  const auto positions = get_log_positions();
  // Extract raw server_id of applied event
  const Uint32 anyvalue = thd_unmasked_server_id(m_thd_ndb->get_thd());

  if (m_incoming_epoch.is_epoch_transaction) {
    // Applying an incoming NDB epoch transaction. The incoming
    // "WRITE ndb_apply_status(server_id=X, epoch=<source_epoch>)" has been
    // deferred, now define the complete "WRITE ndb_apply_status (server_id=X,
    // epoch=<source_epoch>, log_name, start_pos, end_pos)"
    assert(is_serverid_written_by_trans(m_source_server_id));
    const NdbError *ndb_err = m_apply_status->define_write_row(
        m_thd_ndb->trans, m_source_server_id, m_incoming_epoch.epoch,
        positions.log_name, positions.start_pos, positions.end_pos, anyvalue);
    if (ndb_err) {
      m_thd_ndb->push_ndb_error_warning(*ndb_err);
      m_thd_ndb->push_warning(
          "Failed to define update of 'ndb_apply_status' for NDB epoch "
          "transaction");
      return false;
    }
    return true;
  }

  //  Not applying a transaction from NDB. Just update the log positions if
  //  it's already known that a row for source_server_id exists, otherwise
  //  insert a new row with epoch 0.
  if (is_serverid_written_by_trans(m_source_server_id) ||
      m_channel->serverid_exists(m_source_server_id)) {
    // UPDATE ndb_apply_status (server_id=X, log_name, start_pos, end_pos)
    const NdbError *ndb_err = m_apply_status->define_update_row(
        m_thd_ndb->trans, m_source_server_id, positions.log_name,
        positions.start_pos, positions.end_pos, anyvalue);
    if (ndb_err) {
      m_thd_ndb->push_ndb_error_warning(*ndb_err);
      m_thd_ndb->push_warning("Failed to define 'ndb_apply_status' update");
      return false;
    }
  } else {
    // WRITE ndb_apply_status (server_id=X,
    //                         epoch = 0, log_name, start_pos, end_pos)
    constexpr Uint64 zero_epoch = 0;
    const NdbError *ndb_err = m_apply_status->define_write_row(
        m_thd_ndb->trans, m_source_server_id, zero_epoch, positions.log_name,
        positions.start_pos, positions.end_pos, anyvalue);
    if (ndb_err) {
      m_thd_ndb->push_ndb_error_warning(*ndb_err);
      m_thd_ndb->push_warning("Failed to define 'ndb_apply_status' write");
      return false;
    }
    // Save written server_id
    m_written_server_ids.push_back(m_source_server_id);
  }
  return true;
}

void Ndb_applier::set_relay_log_trans_retries(unsigned number) {
  channel_map.rdlock();
  const Master_info *channel_mi = channel_map.get_mi(m_channel_name.c_str());
  channel_mi->rli->trans_retries = number;
  channel_map.unlock();
}

bool Ndb_applier::check_retry_trans() {
  constexpr int MAX_RETRY_TRANS_COUNT = 100;
  if (retry_trans_counter++ < MAX_RETRY_TRANS_COUNT) {
    // Warning is necessary to cause retry from exec_relay_log_event()
    m_thd_ndb->push_warning(ER_REPLICA_SILENT_RETRY_TRANSACTION,
                            "Replica transaction rollback requested");
    /*
      Set retry count to zero to:
      1) Avoid consuming slave-temp-error retry attempts
      2) Ensure no inter-attempt sleep

      Better fix : Save + restore retry count around transactional
      conflict handling
    */
    set_relay_log_trans_retries(0);
    return true;
  }
  return false;
}

void Ndb_applier::atTransactionCommit(Uint64 committed_epoch_value) {
  DBUG_TRACE;
  assert(((trans_dependency_tracker == nullptr) &&
          (trans_conflict_apply_state == SAS_NORMAL)) ||
         ((trans_dependency_tracker != nullptr) &&
          (trans_conflict_apply_state == SAS_TRACK_TRANS_DEPENDENCIES)));
  assert(trans_conflict_apply_state != SAS_APPLY_TRANS_DEPENDENCIES);

  m_channel->update_global_state(
      m_incoming_epoch.max_rep_epoch, committed_epoch_value,
      m_written_server_ids, violation_counters, delete_delete_count,
      reflect_op_prepare_count, reflect_op_discard_count, refresh_op_count,
      trans_row_conflict_count, trans_row_reject_count, trans_in_conflict_count,
      trans_detect_iter_count);

  copyout_applier_stats();

  reset_per_attempt_counters();

  // Clear per-epoch-transaction transaction retry counter
  retry_trans_counter = 0;

  // Mark incoming epoch as committed
  m_incoming_epoch.committed = true;

  if (DBUG_EVALUATE_IF("ndb_replica_fail_marking_epoch_committed", true,
                       false)) {
    const Uint64 current_master_server_epoch = m_incoming_epoch.epoch;
    fprintf(stderr,
            "Replica clearing epoch committed flag "
            "for epoch %llu/%llu (%llu)\n",
            current_master_server_epoch >> 32,
            current_master_server_epoch & 0xffffffff,
            current_master_server_epoch);
    m_incoming_epoch.committed = false;
  }
}

void Ndb_applier::atTransactionAbort() {
  DBUG_TRACE;

  /* Reset any gathered transaction dependency information */
  trans_conflict_handling_end();
  trans_conflict_apply_state = SAS_NORMAL;

  // NOTE! This code path will not update global stats, ie. the counters
  // collected in this Applier are discarded.
  copyout_applier_stats();

  reset_per_attempt_counters();
}

/**
   Called by the worker thread prior to committing the applier transaction.
   This method can request that the applier transaction is retried.


   State transitions :

                       START APPLIER /
                       RESET APPLIER /
                        STARTUP
                            |
                            |
                            v
                    ****************
                    *  SAS_NORMAL  *
                    ****************
                       ^       |
    No transactional   |       | Conflict on transactional table
       conflicts       |       | (Rollback)
       (Commit)        |       |
                       |       v
            **********************************
            *  SAS_TRACK_TRANS_DEPENDENCIES  *
            **********************************
               ^          I              ^
     More      I          I Dependencies |
    conflicts  I          I determined   | No new conflicts
     found     I          I (Rollback)   | (Commit)
    (Rollback) I          I              |
               I          v              |
           **********************************
           *  SAS_APPLY_TRANS_DEPENDENCIES  *
           **********************************


   Operation
     The initial state is SAS_NORMAL.

     On detecting a conflict on a transactional conflict detetecing table,
     SAS_TRACK_TRANS_DEPENDENCIES is entered, and the epoch transaction is
     rolled back and reapplied.

     In SAS_TRACK_TRANS_DEPENDENCIES state, transaction dependencies and
     conflicts are tracked as the epoch transaction is applied.

     Then the Applier transitions to SAS_APPLY_TRANS_DEPENDENCIES state, and
     the epoch transaction is rolled back and reapplied.

     In the SAS_APPLY_TRANS_DEPENDENCIES state, operations for transactions
     marked as in-conflict are not applied.

     If this results in no new conflicts, the epoch transaction is committed,
     and the SAS_TRACK_TRANS_DEPENDENCIES state is re-entered for processing
     the next replicated epch transaction.
     If it results in new conflicts, the epoch transactions is rolled back, and
     the SAS_TRACK_TRANS_DEPENDENCIES state is re-entered again, to determine
     the new set of dependencies.

     If no conflicts are found in the SAS_TRACK_TRANS_DEPENDENCIES state, then
     the epoch transaction is committed, and the Applier transitions to
     SAS_NORMAL state.


   Properties
     1) Normally, there is no transaction dependency tracking overhead paid by
        the applier.

     2) On first detecting a transactional conflict, the epoch transaction must
        be applied at least three times, with two rollbacks.

     3) Transactional conflicts detected in subsequent epochs require the epoch
        transaction to be applied two times, with one rollback.

     4) A loop between states SAS_TRACK_TRANS_DEPENDENCIES and SAS_APPLY_TRANS_
        DEPENDENCIES occurs when further transactional conflicts are discovered
        in SAS_APPLY_TRANS_DEPENDENCIES state.  This implies that the  conflicts
        discovered in the SAS_TRACK_TRANS_DEPENDENCIES state must not be
        complete, so we revisit that state to get a more complete picture.

     5) The number of iterations of this loop is fixed to a hard coded limit,
        after which the Applier will stop with an error.  This should be an
        unlikely occurrence, as it requires not just n conflicts, but at least 1
        new conflict appearing between the transactions in the epoch transaction
        and the database between the two states, n times in a row.

     6) Where conflicts are occasional, as expected, the post-commit transition
        to SAS_TRACK_TRANS_DEPENDENCIES rather than SAS_NORMAL results in one
        epoch transaction having its transaction dependencies needlessly
        tracked.
*/
int Ndb_applier::atConflictPreCommit(bool &retry_applier_trans) {
  DBUG_TRACE;

  /*
    Prior to committing an Applier transaction, we check whether
    Transactional conflicts have been detected which require
    us to retry the applying transaction
  */
  retry_applier_trans = false;
  switch (trans_conflict_apply_state) {
    case SAS_NORMAL: {
      DBUG_PRINT("info", ("SAS_NORMAL"));
      /*
         Normal case.  Only if we defined conflict detection on a table
         with transactional conflict detection, and saw conflicts (on any table)
         do we go to another state
       */
      if (check_flag(TRANS_CONFLICT_DETECTED_THIS_PASS)) {
        DBUG_PRINT("info", ("Conflict(s) detected this pass, transitioning to "
                            "SAS_TRACK_TRANS_DEPENDENCIES."));
        assert(check_flag(OPS_DEFINED));
        /* Transactional conflict resolution required, switch state */
        trans_conflict_handling_start();
        reset_per_attempt_counters();
        trans_conflict_apply_state = SAS_TRACK_TRANS_DEPENDENCIES;
        retry_applier_trans = true;
      }
      break;
    }
    case SAS_TRACK_TRANS_DEPENDENCIES: {
      DBUG_PRINT("info", ("SAS_TRACK_TRANS_DEPENDENCIES"));

      if (check_flag(TRANS_CONFLICT_DETECTED_THIS_PASS)) {
        /*
           Conflict on table with transactional detection this pass, we have
           collected the details and dependencies, now transition to
           SAS_APPLY_TRANS_DEPENDENCIES and reapply the epoch transaction
           without the conflicting transactions.
        */
        assert(check_flag(OPS_DEFINED));
        DBUG_PRINT("info", ("Transactional conflicts, transitioning to "
                            "SAS_APPLY_TRANS_DEPENDENCIES"));

        trans_conflict_apply_state = SAS_APPLY_TRANS_DEPENDENCIES;
        trans_detect_iter_count++;
        retry_applier_trans = true;
        break;
      } else {
        /*
           No transactional conflicts detected this pass, lets return to
           SAS_NORMAL state after commit for more efficient application of epoch
           transactions
        */
        DBUG_PRINT("info", ("No transactional conflicts, transitioning to "
                            "SAS_NORMAL"));
        trans_conflict_handling_end();
        trans_conflict_apply_state = SAS_NORMAL;
        break;
      }
    }
    case SAS_APPLY_TRANS_DEPENDENCIES: {
      DBUG_PRINT("info", ("SAS_APPLY_TRANS_DEPENDENCIES"));
      assert(check_flag(OPS_DEFINED));
      /*
         We've applied the Applier epoch transaction subject to the
         conflict detection.  If any further transactional
         conflicts have been observed, then we must repeat the
         process.
      */
      trans_conflict_handling_end();
      trans_conflict_handling_start();
      trans_conflict_apply_state = SAS_TRACK_TRANS_DEPENDENCIES;

      if (check_flag(TRANS_CONFLICT_DETECTED_THIS_PASS)) {
        DBUG_PRINT("info", ("Further conflict(s) detected, repeating the "
                            "TRACK_TRANS_DEPENDENCIES pass"));
        /*
           Further conflict observed when applying, need
           to re-determine dependencies
        */
        reset_per_attempt_counters();
        retry_applier_trans = true;
        break;
      }

      DBUG_PRINT("info", ("No further conflicts detected, committing and "
                          "returning to SAS_TRACK_TRANS_DEPENDENCIES state"));
      /*
         With dependencies taken into account, no further
         conflicts detected, can now proceed to commit
      */
      break;
    }
  }

  // Clear conflict flags to ensure detecting new conflicts
  m_conflict_flags = 0;

  if (retry_applier_trans) {
    DBUG_PRINT("info", ("Requesting transaction restart"));
    return 1;
  }

  DBUG_PRINT("info", ("Allowing commit to proceed"));
  return 0;
}

int Ndb_applier::atPrepareConflictDetection(const NdbDictionary::Table *table,
                                            const NdbRecord *key_rec,
                                            const uchar *row_data,
                                            Uint64 transaction_id,
                                            bool &handle_conflict_now) {
  DBUG_TRACE;
  /*
    Applier is preparing to apply an operation with conflict detection.
    If we're performing Transactional Conflict Resolution, take
    extra steps
  */
  switch (trans_conflict_apply_state) {
    case SAS_NORMAL:
      DBUG_PRINT("info", ("SAS_NORMAL : No special handling"));
      /* No special handling */
      break;
    case SAS_TRACK_TRANS_DEPENDENCIES: {
      DBUG_PRINT("info", ("SAS_TRACK_TRANS_DEPENDENCIES : Tracking operation"));
      /*
        Track this operation and its transaction id, to determine
        inter-transaction dependencies by {table, primary key}
      */
      assert(trans_dependency_tracker);

      const int res = trans_dependency_tracker->track_operation(
          table, key_rec, row_data, transaction_id);
      if (res != 0) {
        ndb_log_error("%s", trans_dependency_tracker->get_error_text());
        return res;
      }
      /* Proceed as normal */
      break;
    }
    case SAS_APPLY_TRANS_DEPENDENCIES: {
      DBUG_PRINT("info",
                 ("SAS_APPLY_TRANS_DEPENDENCIES : Deciding whether to apply"));
      /*
         Check if this operation's transaction id is marked in-conflict.
         If it is, we tell the caller to perform conflict resolution now instead
         of attempting to apply the operation.
      */
      assert(trans_dependency_tracker);

      if (trans_dependency_tracker->in_conflict(transaction_id)) {
        DBUG_PRINT("info",
                   ("Event for transaction %llu is conflicting.  Handling.",
                    transaction_id));
        trans_row_reject_count++;
        handle_conflict_now = true;
        return 0;
      }

      /*
         This transaction is not marked in-conflict, so continue with normal
         processing.
         Note that normal processing may subsequently detect a conflict which
         didn't exist at the time of the previous TRACK_DEPENDENCIES pass.
         In this case, we will rollback and repeat the TRACK_DEPENDENCIES
         stage.
      */
      DBUG_PRINT("info", ("Event for transaction %llu is OK, applying",
                          transaction_id));
      break;
    }
  }
  return 0;
}

int Ndb_applier::atTransConflictDetected(Uint64 transaction_id) {
  DBUG_TRACE;

  /*
     The Slave has detected a conflict on an operation applied
     to a table with Transactional Conflict Resolution defined.
     Handle according to current state.
  */
  set_flag(TRANS_CONFLICT_DETECTED_THIS_PASS);
  trans_row_conflict_count++;

  switch (trans_conflict_apply_state) {
    case SAS_NORMAL: {
      DBUG_PRINT("info",
                 ("SAS_NORMAL : Conflict on op on table with trans detection."
                  "Requires multi-pass resolution.  Will transition to "
                  "SAS_TRACK_TRANS_DEPENDENCIES at Commit."));
      /*
        Conflict on table with transactional conflict resolution
        defined.
        This is the trigger that we will do transactional conflict
        resolution.
        Record that we need to do multiple passes to correctly
        perform resolution.
        TODO : Early exit from applying epoch?
      */
      break;
    }
    case SAS_TRACK_TRANS_DEPENDENCIES: {
      DBUG_PRINT(
          "info",
          ("SAS_TRACK_TRANS_DEPENDENCIES : Operation in transaction %llu "
           "had conflict",
           transaction_id));
      /*
         Conflict on table with transactional conflict resolution
         defined.
         We will mark the operation's transaction_id as in-conflict,
         so that any other operations on the transaction are also
         considered in-conflict, and any dependent transactions are also
         considered in-conflict.
      */
      const int res = trans_dependency_tracker->mark_conflict(transaction_id);
      if (res != 0) {
        ndb_log_error("%s", trans_dependency_tracker->get_error_text());
        return res;
      }
      break;
    }
    case SAS_APPLY_TRANS_DEPENDENCIES: {
      /*
         This must be a new conflict, not noticed on the previous
         pass.
      */
      DBUG_PRINT("info", ("SAS_APPLY_TRANS_DEPENDENCIES : Conflict detected.  "
                          "Must be further conflict.  Will return to "
                          "SAS_TRACK_TRANS_DEPENDENCIES state at commit."));
      // TODO : Early exit from applying epoch
      break;
    }
    default:
      break;
  }

  return 0;
}

void Ndb_applier::atSchemaDistCompleted() {
  DBUG_TRACE;
  copyout_applier_stats();
}

void Ndb_applier::trans_conflict_handling_start() {
  DBUG_TRACE;
  assert(trans_dependency_tracker == nullptr);
  trans_dependency_tracker =
      DependencyTracker::newDependencyTracker(&conflict_mem_root);
}

void Ndb_applier::trans_conflict_handling_end() {
  DBUG_TRACE;
  if (trans_dependency_tracker) {
    trans_in_conflict_count = trans_dependency_tracker->get_conflict_count();
    trans_dependency_tracker = nullptr;
    conflict_mem_root.ClearForReuse();
  }
}

/*
  Reset the per-epoch-transaction-application-attempt counters
*/
void Ndb_applier::reset_per_attempt_counters() {
  DBUG_TRACE;

  violation_counters.fill(0);

  delete_delete_count = 0;
  reflect_op_prepare_count = 0;
  reflect_op_discard_count = 0;
  refresh_op_count = 0;

  trans_row_conflict_count = 0;
  trans_row_reject_count = 0;
  trans_in_conflict_count = 0;
  trans_detect_iter_count = 0;

  m_conflict_flags = 0;

  m_incoming_epoch.max_rep_epoch = 0;
  m_incoming_epoch.is_epoch_transaction = false;

  m_written_server_ids.clear();
}

void Ndb_applier::copyout_applier_stats() {
  DBUG_TRACE;

  // Update channel with NdbApi statistics difference since last
  static_assert(Ndb::NumClientStatistics == Ndb_replica::NUM_API_STATS);
  std::array<Uint64, Ndb_replica::NUM_API_STATS> current;
  std::array<Uint64, Ndb_replica::NUM_API_STATS> diff;
  for (int i = 0; i < Ndb::NumClientStatistics; i++) {
    // Get current statistics from NdbApi
    current[i] = m_thd_ndb->ndb->getClientStat(i);
    // Calculate diff since last
    diff[i] = current[i] - m_api_stats[i];
    DBUG_PRINT("info", ("api_stats, diff[%d]: %llu", i, diff[i]));
  }
  m_api_stats = current;  // Save current
  m_channel->update_api_stats(diff);

  // Trigger channel copyout
  m_channel->copyout_channel_stats();
}
