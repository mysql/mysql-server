/*
   Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_APPLIER_H
#define NDB_APPLIER_H

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

#include "my_alloc.h"  // MEM_ROOT
#include "ndb_types.h"
#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"
#include "storage/ndb/plugin/ndb_conflict.h"
#include "storage/ndb/plugin/ndb_replica.h"

class Thd_ndb;
class NdbRecord;
class Ndb_apply_status_table;
class Relay_log_info;

/*
   Contains functionality that extends Thd_ndb for replication applier
*/
class Ndb_applier {
  Thd_ndb *const m_thd_ndb;
  Relay_log_info *const m_rli;
  const std::string m_channel_name;
  std::unique_ptr<Ndb_apply_status_table> m_apply_status;
  Ndb_replica::ChannelPtr m_channel;
  const Uint32 m_applier_id;
  const Uint32 m_own_server_id;
  const Uint32 m_source_server_id;
  const std::vector<Uint32> m_ignored_server_ids;
  const Uint32 m_num_workers;

  // List of server_id's that have been written (to ndb_apply_status)
  // by current transaction
  std::vector<Uint32> m_written_server_ids;

 public:
  Ndb_applier(Thd_ndb *thd_ndb, std::string channel_name,
              Ndb_replica::ChannelPtr channel, Uint32 own_server_id,
              Uint32 source_server_id, Uint64 source_epoch,
              std::vector<Uint32> ignored_server_ids, Uint32 num_workers,
              std::vector<Uint32> written_server_ids);
  ~Ndb_applier();

  // Initialize the applier after construction
  bool init();

  // Return the number of configured workers
  Uint32 get_num_workers() const { return m_num_workers; }

 private:
  // Check if server_id is "local" to this cluster. This is used for circular
  // replication where the MySQL Server's connected to same cluster are
  // configured to be ignored in order to break the loop.
  bool is_serverid_local(Uint32 server_id) const {
    return server_id == m_own_server_id ||
           (std::find(m_ignored_server_ids.begin(), m_ignored_server_ids.end(),
                      server_id) != m_ignored_server_ids.end());
  }

  // Check if server_id is written by this transaction
  bool is_serverid_written_by_trans(Uint32 server_id) const {
    return std::find(m_written_server_ids.begin(), m_written_server_ids.end(),
                     server_id) != m_written_server_ids.end();
  }

  // The max replicated epoch from before transaction was started.
  // - used for the NDB$EPOCH conflict algorithm.
  Uint64 m_start_max_rep_epoch{0};

 public:
  Uint64 get_max_rep_epoch() const { return m_start_max_rep_epoch; }

 private:
  // First incoming epoch have relaxed verification
  bool m_first_epoch_verified{false};

  // Current incoming epoch transaction state
  struct {
    Uint64 epoch;          // current_master_server_epoch
    bool committed;        // current_master_server_epoch_committed
    Uint64 max_rep_epoch;  // current_max_rep_epoch
    bool is_epoch_transaction;
  } m_incoming_epoch;

  // Applier conflict flags
  uint8 m_conflict_flags{0};

 public:
  enum Flags {
    /* Conflict detection Ops defined */
    OPS_DEFINED = 1,
    /* Conflict detected on table with transactional resolution */
    TRANS_CONFLICT_DETECTED_THIS_PASS = 2
  };

  // Check if given flag is set
  bool check_flag(Flags flag) const { return (m_conflict_flags & flag); }

  // Set given flag
  void set_flag(Flags flag) { m_conflict_flags |= flag; }

 private:
  /*
   * Transactional conflict detection
   */

  // State of application from Ndb point of view.
  enum {
    /* Normal with optional row-level conflict detection */
    SAS_NORMAL,

    /*
      SAS_TRACK_TRANS_DEPENDENCIES
      Track inter-transaction dependencies
    */
    SAS_TRACK_TRANS_DEPENDENCIES,

    /*
      SAS_APPLY_TRANS_DEPENDENCIES
      Apply only non conflicting transactions
    */
    SAS_APPLY_TRANS_DEPENDENCIES
  } trans_conflict_apply_state{SAS_NORMAL};

  MEM_ROOT conflict_mem_root;
  class DependencyTracker *trans_dependency_tracker{nullptr};

  // Transactional conflict detection counters
  Uint32 trans_row_conflict_count{0};
  Uint32 trans_row_reject_count{0};
  Uint32 trans_in_conflict_count{0};
  Uint32 trans_detect_iter_count{0};

  // Transactional conflict handling
  void trans_conflict_handling_start();
  void trans_conflict_handling_end();

 private:
  /*
   * Conflict detection
   */

  // Currently applied transaction counters
  std::array<Uint32, CFT_NUMBER_OF_CFTS> violation_counters{};

  /*
   * Count of delete-delete conflicts detected
   * (delete op is applied, and row does not exist)
   */
  Uint32 delete_delete_count{0};

  /*
   * Count of reflected operations received that have been
   * prepared (defined) to be executed.
   */
  Uint32 reflect_op_prepare_count{0};

  /*
   * Count of reflected operations that were not applied as
   * they hit some error during execution
   */
  Uint32 reflect_op_discard_count{0};

  /*
   * Count of refresh operations that have been prepared
   */
  Uint32 refresh_op_count{0};

 public:
  void increment_violation_count(enum_conflict_fn_type cft) {
    violation_counters[cft]++;
  }
  void increment_delete_delete_count() { delete_delete_count++; }
  void increment_reflect_op_prepare_count() { reflect_op_prepare_count++; }
  void increment_reflect_op_discard_count() { reflect_op_discard_count++; }
  void increment_refresh_op_count() { refresh_op_count++; }

 private:
  // Set number of transaction retries for the worker thread
  void set_relay_log_trans_retries(unsigned number);

  // Applier transaction silent retry counter
  int retry_trans_counter{0};

 public:
  // Check if transaction should be retried or if max number of retries has
  // been reached.
  bool check_retry_trans();

 private:
  // The latest NdbApi statistics
  std::array<Uint64, Ndb_replica::NUM_API_STATS> m_api_stats{};

  // Publish stats and counters from Applier to the Channel
  void copyout_applier_stats();

  // Reset the per-epoch-transaction-application-attempt counters
  void reset_per_attempt_counters();

  /**
     @brief Verify NDB epoch transaction consistency

     Check that a new incoming epoch from the relay log is expected given the
     current applier state, previous epoch etc. This is checking generic
     replication errors, with a user warning thrown in too.

     Do some validation of the incoming epoch-transaction's epoch - to make sure
     that the sequence of epochs is sensible.

     In the multi threaded case, each applier verify next epoch against its
     own state since more than one epoch transaction can be prepared in
     parallel. The same rules should still apply in that the epoch value
     should be increasing, although there may be (undetectable) "gaps" for
     those epoch transactions handled by other threads.
  */
  bool verify_next_epoch(Uint64 next_epoch) const;

  // Extract current log positions of the channel
  struct Positions {
    const char *log_name;  // Source log name
    ulonglong start_pos;   // Group start position
    ulonglong end_pos;     // Group end position
  };
  Positions get_log_positions() const;

 public:
  /**
     @brief Write to ndb_apply_status is done

     @param row_server_id   The server_id in the written row
     @param row_epoch       The epoch in the written row
     @param skip_write[out] Flag telling the caller that write of this row
                            should be skipped.
     @return 0 for sucess
     @return > 0 for error
   */
  int atApplyStatusWrite(Uint32 row_server_id, Uint64 row_epoch,
                         bool &skip_write);

  /**
     @brief Transaction has been committed sucessfully

     @param committed_epoch_value Epoch value of committed transaction.
   */
  void atTransactionCommit(Uint64 committed_epoch_value);

  /**
     @brief Transaction has been aborted (because it failed to execute in NDB
     or by decision by conflict handling)
   */
  void atTransactionAbort();

  /**
     @brief Transaction is just about to be committed. This function is called
     in order to let conflict handling logic determine if transaction should
     be committed or "silently rollbacked and retried".

     @param retry_applier_trans [out] silently rollback and retry transaction

     @return 0 -> transaction is allowed to commit
     @return 1 -> transaction is NOT allowed to commit
   */
  int atConflictPreCommit(bool &retry_applier_trans);

  /**
     @brief Operation on a table with conflict detection is being prepared.
     This enables the conflict handling logic to determine conflicts per
     row/operation.

     @param table          The NDB table definition
     @param key_rec        The NdbRecord for current row
     @param row_data       The data for current row
     @param transaction_id The transaction id of the applied transaction
     @param handle_conflict_now [out] Conflict has ben detected and should be
     handled immediately by caller.

     @return 0 -> no error
     @return != 0 -> error, transaction will fail with returned error
   */
  int atPrepareConflictDetection(const NdbDictionary::Table *table,
                                 const NdbRecord *key_rec,
                                 const uchar *row_data, Uint64 transaction_id,
                                 bool &handle_conflict_now);

  /**
     @brief Transactional conflict has occured on an operation while executing
     transaction.

     @param transaction_id The transaction id of the applied transaction

     @return != 0 -> error occured while handling conflict
   */
  int atTransConflictDetected(Uint64 transaction_id);

  /**
     @brief Schema distribution has completed
   */
  void atSchemaDistCompleted();

  struct EpochState {
    const Uint32 own_server_id;
    const Uint32 source_server_id;
    const Uint64 epoch_value;
  };
  const EpochState get_current_epoch_state() {
    return {m_own_server_id, m_source_server_id, m_incoming_epoch.epoch};
  }

  /**
     @brief Define how applying a replicated transaction should change the
     ndb_apply_status table data. These data changes are done atomically as part
     of the applied transaction.

     For an applied NDB epoch transaction, the incoming
     WRITE(server_id=<source_server_id>, epoch=<source_epoch>) will be augmented
     with additional values using UPDATE(server_id=<source_server_id>, log_name,
     start_pos, end_pos).

     For other applied transactions, the ndb_apply_status table will be updated
     with current position with an UPDATE(server_id=<source_server_id>,
     log_name, start_pos, end_pos) or a WRITE(server_id=<source_server_id>,
     epoch=0, log_name, start_pos, end_pos) if the row is known to not exist.

     @return true for success
   */
  bool define_apply_status_operations();
};

#endif
