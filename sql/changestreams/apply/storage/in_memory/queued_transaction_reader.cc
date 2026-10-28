// Copyright (c) 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include "sql/changestreams/apply/storage/in_memory/queued_transaction_reader.h"

#include <cassert>

#include "mysql/components/services/log_builtins.h"  // LogErr
#include "mysql/my_loglevel.h"  // ERROR_LEVEL, WARNING_LEVEL
#include "mysqld_error.h"       // ER_REPLICA_RELAY_LOG_READ_FAILURE
#include "sql/changestreams/apply/context/channel.h"
#include "sql/changestreams/apply/jobs/fetchable_transaction.h"
#include "sql/changestreams/apply/jobs/job_applier.h"
#include "sql/changestreams/apply/psi/psi.h"    // stage_csa_working
#include "sql/changestreams/apply/psi/stage.h"  // concurrency::set_thd_stage
#include "sql/changestreams/apply/storage/in_memory/transaction_envelope.h"
#include "sql/changestreams/apply/storage/in_memory/trx_envelope_queue.h"
#include "sql/changestreams/apply/storage/in_memory/trx_payload.h"
#include "sql/current_thd.h"  // current_thd
#include "sql/debug_sync.h"   // DBUG_SIGNAL_WAIT_FOR
#include "sql/derror.h"       // ER_THD
#include "sql/log_event.h"    // Gtid_log_event
#include "sql/mysqld.h"       // slave_trans_retries
#include "sql/rpl_mi.h"       // Master_info
#include "sql/rpl_replica.h"  // sql_slave_killed
#include "sql/rpl_rli.h"      // Relay_log_info

namespace mysql::csa {

namespace {
// Copy of the disk reader's check (relay_log_adaptive_reader.cpp).
void warn_on_inconsistent_logical_clock(Gtid_log_event *gtid_ev,
                                        int64_t &prev) {
  // No relay-log file holds the transaction; fills "in file %s".
  constexpr const char *log_name = "in-memory relay log";
  auto lc = gtid_ev->last_committed;
  auto seq = gtid_ev->sequence_number;
  if (seq <= 1 || lc == 0) {
    prev = -1;
  }
  if (unlikely(lc != 0 && lc >= seq)) {
    char buff_gtid[Gtid::MAX_TEXT_LENGTH + 1];
    gtid_ev->get_gtid_spec().to_string(gtid_ev->get_tsid(), buff_gtid);
    LogErr(WARNING_LEVEL, ER_RPL_INCONSISTENT_TIMESTAMPS_IN_TRX, buff_gtid,
           log_name, seq, lc);
  } else if (unlikely(seq <= prev)) {
    char buff_gtid[Gtid::MAX_TEXT_LENGTH + 1];
    gtid_ev->get_gtid_spec().to_string(gtid_ev->get_tsid(), buff_gtid);
    LogErr(WARNING_LEVEL, ER_RPL_INCONSISTENT_SEQUENCE_NO_IN_TRX, buff_gtid,
           log_name, seq, prev);
  }
  prev = seq;
}
}  // namespace

Queued_transaction_reader::Queued_transaction_reader(int instance_id,
                                                     Relay_log_info *rli,
                                                     Trx_envelope_queue *queue)
    : m_rli(rli),
      m_queue(queue),
      m_stat_monitor(scheduler::Statistics_monitor::get(instance_id)),
      m_resource_monitor(Resource_monitor::get(instance_id)) {
  m_channel.reset(new Channel(rli->mi->get_channel(), instance_id,
                              rli->get_commit_order_manager()));
}

Queued_transaction_reader::Queued_transaction_reader(Trx_envelope_queue *queue)
    : m_rli(nullptr),
      m_queue(queue),
      m_stat_monitor(scheduler::Statistics_monitor::get(0)),
      m_resource_monitor(Resource_monitor::get(0)) {
  // Test-only constructor.
}

Queued_transaction_reader::~Queued_transaction_reader() = default;

Job_ptr Queued_transaction_reader::read() {
  if (is_stopped()) {
    return nullptr;
  }

  // A spill file could not be deleted: stop, as a failed relay log purge does.
  if (m_queue->take_spill_delete_failed() && !m_is_error) {
    m_is_error = true;
    if (m_rli != nullptr) {
      m_rli->report(ERROR_LEVEL, ER_REPLICA_RELAY_LOG_READ_FAILURE,
                    ER_THD(current_thd, ER_REPLICA_RELAY_LOG_READ_FAILURE),
                    "could not delete an in-memory relay log spill file; see "
                    "the error log");
    }
    return nullptr;
  }

  // Report the coordinator as caught up while it blocks below waiting for the
  // next transaction, mirroring the on-disk reader.
  if (m_rli != nullptr)
    concurrency::set_thd_stage(m_rli->info_thd,
                               stage_replica_has_read_all_relay_log);

  // Sweeps the committed head envelope, and dispatches the next uncommitted
  // transaction.
  Transaction_envelope *envelope = m_queue->sweep_and_dispatch();
  if (envelope == nullptr) {
    // nullptr is a stop, an idle poll or a structural inconsistency. Report an
    // inconsistency once as Last_SQL_Error; the queue already logged the
    // counters.
    if (m_queue->applier_error() && !m_is_error) {
      m_is_error = true;
      if (m_rli != nullptr) {
        m_rli->report(ERROR_LEVEL, ER_REPLICA_RELAY_LOG_READ_FAILURE,
                      ER_THD(current_thd, ER_REPLICA_RELAY_LOG_READ_FAILURE),
                      "in-memory relay log queue structural inconsistency "
                      "(cursor/deque desynchronization); see the error log "
                      "for details");
      }
    }
    // UNTIL, empty queue: skipped after a queue error or once a stop began.
    // STOP sets abort_slave, KILL does not; one late check is fine.
    if (m_rli != nullptr && !m_is_error && !m_rli->abort_slave &&
        m_rli->is_until_satisfied_all_transactions_read_from_relay_log()) {
      stop_for_until();
    }
    return nullptr;
  }

  // A transaction is available again: restore the working stage.
  if (m_rli != nullptr) {
    concurrency::set_thd_stage(m_rli->info_thd, stage_csa_working);
    // Test hook: pause after the dispatch check and before the job is built,
    // so a test can truncate the dispatched transaction in this window.
    DBUG_SIGNAL_WAIT_FOR(m_rli->info_thd, "imr_pause_before_job_build",
                         "coordinator_reached_before_job_build",
                         "coordinator_continue_job_build");
  }

  // A dispatched ACTIVE envelope always has a payload.
  Trx_payload *payload = envelope->payload();
  if (payload == nullptr) {
    assert(false);
    // Stop the applier with an error instead of building a job without data.
    m_is_error = true;
    if (m_rli != nullptr) {
      m_rli->report(ERROR_LEVEL, ER_REPLICA_RELAY_LOG_READ_FAILURE,
                    ER_THD(current_thd, ER_REPLICA_RELAY_LOG_READ_FAILURE),
                    "in-memory relay log transaction has no payload");
    }
    return nullptr;
  }
  // Take a shared ownership copy of the transaction
  std::shared_ptr<Fetchable_transaction> fetchable = payload->fetchable();

  Job_applier *job =
      new Job_applier(m_channel.get(), slave_trans_retries, fetchable,
                      m_stat_monitor, m_resource_monitor);
  // The receiver may truncate after the dispatch check. Drop such a job before
  // any worker sees it. A truncation after this check reaches the worker,
  // which rolls back.
  if (fetchable != nullptr && fetchable->is_truncated()) {
    delete job;
    return nullptr;
  }
  // Applier stop: the first-event peek may have aborted; drop the unrun job.
  if (m_queue->applier_stop_flag()->load(std::memory_order_acquire)) {
    delete job;
    return nullptr;
  }
  if (m_rli != nullptr) {
    // UNTIL, before handout: drop the unrun job; the next START re-dispatches.
    const Log_event *first_event = job->get_first_event();
    if (first_event != nullptr &&
        m_rli->is_until_satisfied_before_dispatching_event(first_event)) {
      delete job;
      stop_for_until();
      return nullptr;
    }
    // UNTIL, after handout: stop after this job (disk reader parity).
    if (m_rli->is_until_satisfied_after_dispatching_event()) {
      stop_for_until();
    }
  }
  // get_gtid_spec() only reads, but is not const.
  auto *gtid_ev = dynamic_cast<Gtid_log_event *>(
      const_cast<Log_event *>(job->get_first_event()));
  if (gtid_ev != nullptr) {
    // A source Rotate resets on disk; here, the new source FDE after it.
    if (payload->fde() != m_prev_fde) {
      m_prev_fde = payload->fde();
      m_prev_seq = -1;
    }
    warn_on_inconsistent_logical_clock(gtid_ev, m_prev_seq);
  }
  return job;
}

bool Queued_transaction_reader::is_stopped() const {
  // Same stop test as the disk reader; the queue stop flag is covered by
  // m_stopped, abort_slave and m_is_error.
  return m_is_error || m_stopped ||
         (m_rli != nullptr && sql_slave_killed(m_rli->info_thd, m_rli));
}

bool Queued_transaction_reader::is_error() const { return m_is_error; }

void Queued_transaction_reader::stop() {
  m_stopped = true;
  // Stop the APPLIER role.
  m_queue->stop(Trx_envelope_queue::Scope::APPLIER);
}

void Queued_transaction_reader::stop_for_until() {
  assert(m_rli != nullptr);
  // Plain atomic store, no data_lock.
  m_rli->abort_slave = true;
  // Latch sql_thread_kill_accepted so applier_stop sees a requested stop.
  (void)sql_slave_killed(m_rli->info_thd, m_rli);
  m_stopped = true;
}

}  // namespace mysql::csa
