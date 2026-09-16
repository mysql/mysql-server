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

#include "sql/changestreams/apply/jobs/job_applier.h"
#include "mysql/scheduler/dispatch_reason.h"
#include "sql/changestreams/apply/psi/psi.h"
#include "sql/changestreams/apply/psi/stage.h"
#include "sql/changestreams/apply/resource/resource_map.h"
#include "sql/changestreams/apply/resource/statistics_map.h"
#include "sql/changestreams/apply/service/csa_service.h"
#include "sql/sql_class.h"

using Statistics_map = mysql::csa::Statistics_map;
using Statistics_monitor = mysql::scheduler::Statistics_monitor;
using namespace mysql::csa;

namespace mysql::csa {

Job_applier::Job_applier(Channel *channel, unsigned int max_retries,
                         std::shared_ptr<Fetchable_transaction> fetch_object,
                         Stat_monitor_ref stat_monitor,
                         Resource_monitor_ref res_monitor)
    : Job_binlog(channel, max_retries, std::move(fetch_object), stat_monitor),
      m_resource_monitor(res_monitor) {}

Job_applier::~Job_applier() {
  // If we have not released the session yet, release it now. Session release
  // here can be called only when the scheduler pops up unfinished jobs from
  // the queue (stop now request). In other cases, session must be released in
  // "detach" (normal execution / failure). This way we ensure that session
  // service can be destructed just after the sync in all of the cases.
  if (m_relay_context) {
    auto *w_ctx = m_relay_context->get_parallel_worker_context();
    if (w_ctx && w_ctx->found_commit_order_deadlock()) {
      w_ctx->wait_for_rollback();
      m_relay_context->detach_rli();
    } else {
      m_relay_context->detach_rli();
      m_relay_context->clean();
    }
    m_session_service->release_session(std::move(m_relay_context),
                                       get_attach_id());
    m_relay_context.reset();
  }
}

void Job_applier::prepare_for_apply(Session_service_ptr ss) {
  m_session_service = ss;
}

void Job_applier::ensure_session(uint thread_id) {
  if (!m_relay_context) {
    concurrency::set_stage(stage_csa_session_acquire.m_key);
    m_relay_context = m_session_service->acquire_session(get_attach_id());
    assert(m_relay_context);
    m_relay_context->attach_rli();
    // We use transaction id assigned by the receiver
    m_relay_context->set_parallel_worker_context(
        m_channel->get_commit_order_manager(), get_id(), thread_id,
        get_channel_id(), get_retries(), m_max_retries);
  }
}

// Attaches the given thread to this job, securing a session if necessary and
// handling special cases during scheduler unblocking.
//
// In normal operation, this method ensures a session is available, attaches it
// to the relay log context, and marks the job as attached for the thread.
//
// During unblocking by the scheduler, two threads may observe the same job:
// - The prepare thread may still own the attached session and continue running
//   the prepare phase.
// - The unblocked dispatch acts as the commit helper/owner. It preregisters to
//   the commit order manager without attaching, waits for the prepare thread to
//   detach, and then advances the job into commit ownership.
//
// After the handoff, retries stay in retry_commit and are executed by the
// commit owner; they do not go through the preregistration path again.
bool Job_applier::attach(Thread_id thread_id) {
  auto dispatch_reason = scheduler::current_dispatch_reason();
  const bool is_unblocked_dispatch =
      dispatch_reason == scheduler::Dispatch_reason::unblocked;

  if (is_unblocked_dispatch) {
    if (m_phase == Transaction_phase::prepare && !m_co_registered) {
      while (!is_done() && !is_stopped() && !m_is_attached &&
             m_phase == Transaction_phase::prepare) {
        std::this_thread::sleep_for(std::chrono::microseconds{100});
      }
      if (is_stopped()) {
        return true;
      }
      return false;
    }

    if (m_co_registered) {
      while (!is_done() && !is_stopped() &&
             (m_is_attached || m_phase == Transaction_phase::prepare)) {
        std::this_thread::sleep_for(std::chrono::microseconds{100});
      }
      if (is_stopped() || is_done()) {
        return true;
      }
      if (m_phase == Transaction_phase::commit_register) {
        m_phase = Transaction_phase::commit_binlog;
      }
    }
  }
  if (m_phase == Transaction_phase::commit_register) {
    // don't attach
    return false;
  }
  // secure a session only when needed, just before running
  ensure_session(thread_id);
  if (m_relay_context->get_session().is_killed()) {
    return true;
  }

  auto *w_ctx = m_relay_context->get_parallel_worker_context();
  if (m_phase == Transaction_phase::commit_binlog) {
    w_ctx->set_committing();
  }
  bool should_wait_for_rollback = (m_phase == Transaction_phase::commit_binlog)
                                      ? w_ctx->is_rollback_requested()
                                      : w_ctx->found_commit_order_deadlock();
  if (should_wait_for_rollback) {
    // THD is being rolled back, wait for rollback to finish, restart job and
    // report retry
    if (wait_for_rollback_and_restart()) {
      set_fatal_error();
      m_relay_context->attach_session();
      m_is_attached = true;
      return true;
    }
    m_phase = Transaction_phase::retry_commit;
  }
  m_relay_context->attach_session();
  m_relay_context->set_fde(m_fetch_metadata->get_fde());
  m_is_attached = true;
  m_stat_monitor.get()
      .get_stat(Statistics_map::worker_session)
      .store(m_relay_context->get_id(), thread_id);
  m_stat_monitor.get()
      .get_stat(Statistics_map::session_worker)
      .store(thread_id, m_relay_context->get_id());
  return !m_is_attached;
}

bool Job_applier::detach(Thread_id /*thread_id*/) {
  if (!m_is_attached) {
    return false;
  }
  finish_telemetry();
  m_relay_context->set_fde(nullptr);
  m_relay_context->detach_session();
  if (is_done()) {
    const bool is_unblocked_dispatch = scheduler::current_dispatch_reason() ==
                                       scheduler::Dispatch_reason::unblocked;
    if (!(is_unblocked_dispatch && is_error())) {
      m_relay_context->detach_rli();
      concurrency::set_stage(stage_csa_session_release.m_key);
      m_session_service->release_session(std::move(m_relay_context),
                                         get_attach_id());
      m_relay_context.reset();
    }
  }
  m_is_attached = false;
  return false;
}

bool Job_applier::commit(Thread_id thread_id) {
  if (m_skip) {
    finish_before_commit();
    return false;
  }

  start_telemetry();
  assert(m_phase == Transaction_phase::commit_binlog ||
         m_phase == Transaction_phase::retry_commit);
  assert(m_commit_event);
  THD *thd{m_relay_context->get_session().get_thd()};
  concurrency::set_thd_stage(thd, stage_csa_job_apply);
  assert(thd && m_relay_context->get_session().is_valid());
  if (this->apply_event(m_commit_event, thd) ||
      m_relay_context->get_parallel_worker_context()
          ->found_commit_order_deadlock()) {
    m_phase = Transaction_phase::retry_commit;
    return true;
  }

  m_stat_monitor.get()
      .get_stat(Statistics_map::applied_events_cnt)
      .add(1, thread_id);
  // we need to call wait and finish in case transaction does not binlog
  // anything
  Commit_order_manager::wait_and_finish(
      m_relay_context->get_session().get_thd(), false);

  m_stat_monitor.get()
      .get_stat(Statistics_map::committed_cnt)
      .add(1, thread_id);
  m_phase = Transaction_phase::done;
  return false;
}

bool Job_applier::commit_register(Thread_id thread_id) {
  const bool is_unblocked_dispatch = scheduler::current_dispatch_reason() ==
                                     scheduler::Dispatch_reason::unblocked;
  assert(m_phase == Transaction_phase::prepare ||
         m_phase == Transaction_phase::commit_register ||
         m_phase == Transaction_phase::retry_commit);
  const bool needs_commit_registration =
      (is_unblocked_dispatch && m_phase == Transaction_phase::prepare) ||
      m_phase == Transaction_phase::commit_register ||
      (m_phase == Transaction_phase::retry_commit && !m_co_registered);
  if (needs_commit_registration) {
    if (!m_co_registered) {
      assert(m_relay_context);
      m_relay_context->set_parallel_worker_context(
          m_channel->get_commit_order_manager(), get_id(), thread_id,
          get_channel_id(), get_retries(), m_max_retries);
      m_relay_context->register_to_commit_order();
      m_co_registered = true;
    }
    if (m_phase == Transaction_phase::commit_register) {
      m_phase = Transaction_phase::commit_binlog;
    }
  }
  if (m_phase == Transaction_phase::retry_commit) {
    assert(m_co_registered);
  }
  if (m_skip) {
    return false;
  }
  return false;
}

bool Job_applier::apply_event(const Log_event_ptr &ev, THD *thd) {
  auto *rli = m_relay_context->get_relay_log_info();

  // setup context
  ev->thd = thd;
  thd->server_id = ev->server_id;
  thd->unmasked_server_id = ev->common_header->unmasked_server_id;
  thd->set_time();

  // apply it
  int error = ev->apply_csa_event(rli);
  if (error == 0 &&
      ev->get_type_code() == mysql::binlog::event::ROWS_QUERY_LOG_EVENT) {
    m_rows_query_event = ev;
  }
  sync_rows_query_event_retention();
  return error;
}

bool Job_applier::prepare(Thread_id thread_id) {
  assert(m_phase == Transaction_phase::prepare ||
         m_phase == Transaction_phase::retry_commit);
  return run_phase(thread_id);
}

bool Job_applier::check_rpco_conflict(Thread_id thread_id) {
  if (!m_channel->get_commit_order_manager()) {
    return false;
  }
  auto *w_ctx = m_relay_context->get_parallel_worker_context();
  if (w_ctx->found_commit_order_deadlock()) {
    detach(thread_id);
    w_ctx->set_applied();
    w_ctx->handle_commit_order_deadlock();
    m_phase = Transaction_phase::retry_commit;
    return true;
  }
  return false;
}

void Job_applier::finish_before_commit() {
  auto *thd{m_relay_context->get_session().get_thd()};
  concurrency::set_thd_stage(thd, stage_csa_job_apply);
  assert(m_phase == Transaction_phase::commit_binlog);
  assert(!thd->get_stmt_da()->is_error());
  Commit_order_manager::wait_and_finish(thd, thd->get_stmt_da()->is_error());
  Commit_stage_manager::get_instance().finish_session_ticket(thd);
  m_phase = Transaction_phase::done;
}

bool Job_applier::run_phase(Thread_id thread_id) {
  bool is_error{false};
  auto event_counter{0};
  THD *thd{m_relay_context->get_session().get_thd()};
  assert(thd && m_relay_context->get_session().is_valid());

  concurrency::set_thd_stage(thd, stage_csa_job_apply);

  start_telemetry();

  while (m_fetch_metadata->wait_next()) {
    auto max_event_length = m_fetch_metadata->get_max_event_length();
    auto locked_resource = m_resource_monitor.get().acquire_resource(
        Resource_map::declared_channel_memory, max_event_length);
    if (!locked_resource.is_locked()) {
      return true;  // killed
    }

    auto fetch_result = m_fetch_metadata->fetch_next();
    if (!fetch_result.has_value()) {
      break;
    }
    auto &managed_event = fetch_result.value();
    auto ev = managed_event.get_event();

#ifndef DBUG_OFF
    if (ev->common_header->data_written > 3000) {
      DBUG_EXECUTE_IF("pause_csa_before_releasing_event_memory", {
        const char act[] =
            "now SIGNAL paused_csa_before_releasing_event_memory WAIT_FOR "
            "signal_continue_csa_releasing_event_memory";
        assert(!debug_sync_set_action(thd, STRING_WITH_LEN(act)));
      };);
    }
#endif

    if (ev->ends_group() || is_atomic_ddl_event(ev.get()) ||
        (managed_event.is_last_in_transaction() && is_trx())) {
      // change of phase to commit
      m_commit_event = ev;
      assert(m_phase == Transaction_phase::prepare ||
             m_phase == Transaction_phase::retry_commit);
      concurrency::set_thd_stage(thd,
                                 stage_worker_waiting_for_its_turn_to_commit);
      if (m_phase == Transaction_phase::prepare) {
        m_phase = Transaction_phase::commit_register;
        // we need to detach in case of rpco conflict
        if (m_channel->get_commit_order_manager()) {
          detach(thread_id);
          m_relay_context->get_parallel_worker_context()->set_applied();
          check_rpco_conflict(thread_id);
        }
      }
      // do not apply commit event
      return false;
    }
    event_counter++;

    if (check_rpco_conflict(thread_id)) {
      return false;  // no error, stop applying
    }

    if (this->apply_event(ev, thd)) {
      is_error = true;
      break;
    }

    m_stat_monitor.get()
        .get_stat(Statistics_map::applied_events_cnt)
        .add(1, thread_id);

    if (is_any_gtid_event(ev.get()) && is_already_logged_transaction(thd)) {
      // if we want to know if transaction was applied and we don't want
      // to check GTID state, we need to wait until we process GTID event
      m_phase = Transaction_phase::commit_register;
      m_skip = true;
      m_fetch_metadata->set_fetching_done();
      return false;
    }

    m_next_event++;
  }

  if (m_fetch_metadata->is_fetching_error()) {
    auto *rli = m_relay_context->get_relay_log_info();
    rli->report(ERROR_LEVEL, ER_REPLICA_RELAY_LOG_READ_FAILURE,
                ER_THD(thd, ER_REPLICA_RELAY_LOG_READ_FAILURE),
                m_fetch_metadata->get_fetch_error_msg().c_str());
    is_error = true;
    set_fatal_error();
    m_phase = Transaction_phase::done;
  } else if (m_fetch_metadata->is_truncated()) {
    // Truncation means the IO thread cut the metadata stream. Roll back
    // silently and let the next job replay the transaction from source.
    m_relay_context->get_relay_log_info()->cleanup_context(thd, true);
    sync_rows_query_event_retention();
    thd->clear_error();
    m_phase = Transaction_phase::done;
  } else {
    if (!is_trx()) {
      m_phase = Transaction_phase::done;
    }
  }

  if (is_error) {
    MYSQL_LIB_LOG_DEBUG() << "Unable to apply event #" << event_counter
                          << " for transaction " << get_trx_gtid().to_string()
                          << "internal id: " << get_id();
  } else {
    // allowed for retry commit or a fully applied control event stream
    assert(m_phase == Transaction_phase::retry_commit ||
           m_phase == Transaction_phase::done);
  }
  return is_error;
}

// else
//  TODO: support delayed applier
//  if (sql_delay_event(ev, m_thd, m_rli))
//  {
//    res = true;
//  }

bool Job_applier::wait_for_rollback_and_restart() {
  if (m_relay_context->wait_for_rollback()) {
    return true;
  }
  if (!can_be_retried()) {
    auto *thd = m_relay_context->get_session().get_thd();
    if (!thd->get_stmt_da()->is_set()) {
      thd->get_stmt_da()->set_error_status(thd, ER_LOCK_DEADLOCK);
    }
    return true;
  }
  this->inc_retries();
  m_skip_rollback = true;
  restart();
  int wait_before_retry = std::min((int)get_retries(), 5);
  std::this_thread::sleep_for(std::chrono::seconds(wait_before_retry));
  return false;
}

bool Job_applier::restart() {
  m_skip = false;
  assert(m_relay_context);
  // Retry resets cached decoded events before
  // Relay_context::retry_transaction() runs cleanup_context(). Keep the active
  // ROWS_QUERY event retained across that window so THD::query() never points
  // to freed event-owned memory.
  Job_binlog::restart();
  m_relay_context->retry_transaction(get_retries(), m_skip_rollback);
  sync_rows_query_event_retention();
  m_skip_rollback = false;
  return false;
}

bool Job_applier::is_attached() const { return m_is_attached; }

std::string Job_applier::to_string() {
  std::stringstream ss;
  ss << get_trx_gtid().to_string() << " id: " << get_id();
  return ss.str();
}

bool Job_applier::can_be_retried() {
  if (!Job::can_be_retried()) {
    return false;
  }
  return m_relay_context->can_be_retried();
}

void Job_applier::start_telemetry() {
#ifdef HAVE_PSI_THREAD_INTERFACE
  PSI_thread *thread = PSI_THREAD_CALL(get_thread)();
  if (thread != nullptr) {
    PSI_THREAD_CALL(detect_telemetry)(thread);
  }
#endif
}

void Job_applier::finish_telemetry() {
#ifdef HAVE_PSI_THREAD_INTERFACE
  PSI_thread *thread = PSI_THREAD_CALL(get_thread)();
  if (thread != nullptr) {
    PSI_THREAD_CALL(abort_telemetry)(thread);
  }
#endif
}

void Job_applier::sync_rows_query_event_retention() {
  auto *rli = m_relay_context ? m_relay_context->get_relay_log_info() : nullptr;
  if (rli == nullptr || rli->rows_query_ev == nullptr) {
    m_rows_query_event.reset();
  }
}

// Final failure. In MTA, we do the following cleanup:
// 1. remove from the COM queue
// 2. finish BGC ticket.
// In order to finish early, we skip the cleanup and wait
// for CSA to kill ongoing sessions.
// Both synchronization mechanisms are responsive to kill signals.
void Job_applier::set_failure() {
  // ensures session release in detach
  set_fatal_error();
  THD *thd{m_relay_context->get_session().get_thd()};
  // to fail quickly, don't unregister from COM queue or finish session ticket
  // in case of error, sessions will be killed
  m_relay_context->report_error(get_trx_gtid().to_string());
  bool is_err = thd->is_error();
  thd->clear_error();
  m_relay_context->get_relay_log_info()->cleanup_context(thd, is_err);
  sync_rows_query_event_retention();
}

}  // namespace mysql::csa
