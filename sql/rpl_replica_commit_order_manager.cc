/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/rpl_replica_commit_order_manager.h"

#include <array>

#include "debug_sync.h"  // debug_sync_set_action
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_sys.h"
#include "mysql/components/services/bits/psi_stage_bits.h"
#include "mysql/psi/mysql_cond.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysqld_error.h"
#include "sql/binlog.h"
#include "sql/handler.h"  // ha_flush_logs
#include "sql/mdl.h"
#include "sql/mysqld.h"       // key_commit_order_manager_mutex ..
#include "sql/raii/sentry.h"  // raii::Sentry<F>
#include "sql/rpl_rli_pdb.h"  // Slave_worker
#include "sql/sql_class.h"
#include "sql/sql_error.h"
#include "sql/sql_lex.h"
#include "string_with_len.h"

Commit_order_manager::Commit_order_manager(uint32 worker_numbers)
    : m_workers(worker_numbers) {
  unset_rollback_status();
}

Commit_order_manager::~Commit_order_manager() = default;

void Commit_order_manager::init_worker_context(Slave_worker &worker) {
  this->m_workers[worker.id].m_mdl_context = &worker.info_thd->mdl_context;
}

void Commit_order_manager::register_trx(Slave_worker *worker) {
  DBUG_TRACE;

  DBUG_PRINT("info", ("Worker %d added to the commit order queue",
                      (int)worker->info_thd->thread_id()));

  /* only transition allowed: FINISHED -> REGISTERED */
  assert(this->m_workers[worker->id].m_stage ==
         cs::apply::Commit_order_queue::enum_worker_stage::FINISHED);
  this->m_workers[worker->id].m_stage =
      cs::apply::Commit_order_queue::enum_worker_stage::REGISTERED;
  this->m_workers.push(worker->id);
}

bool Commit_order_manager::wait_on_graph(Slave_worker *worker) {
  auto &worker_stats = worker->get_worker_metrics();
  auto worker_thd = worker->info_thd;
  bool rollback_status{false};
  raii::Sentry<> wait_status_guard{[&]() -> void {
    worker_thd->mdl_context.m_wait.reset_status();
    if (rollback_status)
      this->m_workers[worker->id].m_stage =
          cs::apply::Commit_order_queue::enum_worker_stage::REGISTERED;
    else
      this->m_workers[worker->id].m_stage =
          cs::apply::Commit_order_queue::enum_worker_stage::WAITED;
  }};

  worker_thd->mdl_context.m_wait.reset_status();
  this->m_workers[worker->id].m_stage =
      cs::apply::Commit_order_queue::enum_worker_stage::FINISHED_APPLYING;

  if (this->m_workers.front() != worker->id) {
    if (worker->found_commit_order_deadlock()) {
      /* purecov: begin inspected */
      rollback_status = true;
      return true;
      /* purecov: end */
    }
    this->m_workers[worker->id].m_stage =
        cs::apply::Commit_order_queue::enum_worker_stage::REQUESTED_GRANT;

    Commit_order_lock_graph ticket{worker_thd->mdl_context, *this,
                                   static_cast<std::uint32_t>(worker->id)};
    worker_thd->mdl_context.will_wait_for(&ticket);
    worker_thd->mdl_context.find_deadlock();
    raii::Sentry<> ticket_guard{
        [&]() -> void { worker_thd->mdl_context.done_waiting_for(); }};

    worker_stats.inc_number_of_waits_on_commit_order();
    // Time summed is in nanoseconds
    auto time_increment_func = [&worker_stats](unsigned long value) {
      worker_stats.inc_waited_time_on_commit_order(value);
    };
    unsigned long timeout = LONG_TIMEOUT;  // Wait for a year
    auto wait_status = worker_thd->mdl_context.m_wait.observable_timed_wait(
        worker_thd, timeout, true, time_increment_func,
        &stage_worker_waiting_for_its_turn_to_commit);

    switch (wait_status) {
      case MDL_wait::GRANTED:
        return false;
      case MDL_wait::WS_EMPTY:
        /* purecov: begin inspected */
        assert(false);
        return false;
        /* purecov: end */
      case MDL_wait::TIMEOUT:
        /* purecov: begin inspected */
        my_error(ER_LOCK_WAIT_TIMEOUT, MYF(0));
        break;
        /* purecov: end */
      case MDL_wait::KILLED:
        /* purecov: begin inspected */
        if (worker_thd->is_killed() == ER_QUERY_TIMEOUT)
          my_error(ER_QUERY_TIMEOUT, MYF(0));
        else
          my_error(ER_QUERY_INTERRUPTED, MYF(0));
        break;
        /* purecov: end */
      case MDL_wait::VICTIM:
        my_error(ER_LOCK_DEADLOCK, MYF(0));
        break;
    }
    worker->report_commit_order_deadlock();
    rollback_status = true;
    return true;
  } else {
    // If the worker is at the head of the queue, then no point in killing
    // a transaction due to an ephemeral deadlock that might have happened.
    worker->reset_commit_order_deadlock();
  }
  return false;
}

bool Commit_order_manager::wait(Slave_worker *worker) {
  DBUG_TRACE;

  /*
    When prior transaction fail, current trx should stop and wait for signal
    to rollback itself
  */
  if (this->m_workers[worker->id].m_stage ==
      cs::apply::Commit_order_queue::enum_worker_stage::REGISTERED) {
    CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP("commit_order_manager_before_wait");

    if (this->wait_on_graph(worker)) return true;

    THD *worker_thd = worker->info_thd;
    bool rollback_status = m_rollback_trx.load();

    DBUG_EXECUTE_IF("rpl_fake_commit_order_deadlock_for_timestamp_100", {
      if (worker_thd->start_time.tv_sec == 100) {
        my_error(ER_UNKNOWN_ERROR, MYF(0));
        return true;
      }

      if (worker_thd->start_time.tv_sec == 200 && !rollback_status) {
        my_error(ER_UNKNOWN_ERROR, MYF(0));
        return true;
      }
    });

    CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP("commit_order_manager_after_wait");

    if (rollback_status) {
      finish_one(worker);

      DBUG_PRINT("info", ("thd has seen an error signal from old thread"));
      worker_thd->get_stmt_da()->set_overwrite_status(true);
      my_error(ER_REPLICA_WORKER_STOPPED_PREVIOUS_THD_ERROR, MYF(0));
    }
    /*
      Set HA_IGNORE_DURABILITY so that transaction is not flushed to the
      storage engine immediately, instead we hold all the applier worker
      threads and flush them together in group.
      The tx_commit_pending variable determines if transaction commit is
      pending, and next_to_commit is used to maintain the commit queue for
      group commit. The tx_commit_pending and next_to_commit variables are
      reset before thread enters group commit later.
    */
    else if (worker_thd->is_current_stmt_binlog_disabled()) {
      worker_thd->durability_property = HA_IGNORE_DURABILITY;
      worker_thd->tx_commit_pending = true;
      worker_thd->next_to_commit = nullptr;
    }

    return rollback_status;
  }

  return false;
}

void Commit_order_manager::flush_engine_and_signal_threads(
    Slave_worker *worker) {
  DBUG_TRACE;

  /*
    Enroll the session in FLUSH stage queue so that transactions can be flushed
    together in group with binlog queued sessions. enroll_for will return false
    in case this thread became a follower; in that case the leader has completed
    the flush and signal. If this thread is leader, enroll_for returns true and
    holds the queue lock for COMMIT_ORDER_FLUSH_STAGE as well as the stage lock
    for the the flush stage. In the rest of this function, this thread is a
    leader and will flush to engine and update gtid state, on behalf of all the
    followers.
  */
  if (!Commit_stage_manager::get_instance().enroll_for(
          Commit_stage_manager::COMMIT_ORDER_FLUSH_STAGE, worker->info_thd,
          nullptr, mysql_bin_log.get_log_lock())) {
    m_workers[worker->id].m_stage =
        cs::apply::Commit_order_queue::enum_worker_stage::FINISHED;
    return;
  }

  /* Fetch leader thread from commit order flush queue */
  THD *first =
      Commit_stage_manager::get_instance().fetch_queue_skip_acquire_lock(
          Commit_stage_manager::COMMIT_ORDER_FLUSH_STAGE);

  Commit_stage_manager::get_instance().unlock_queue(
      Commit_stage_manager::COMMIT_ORDER_FLUSH_STAGE);

  mysql_mutex_lock(mysql_bin_log.get_commit_lock());
  mysql_mutex_unlock(mysql_bin_log.get_log_lock());

  CONDITIONAL_SYNC_POINT_FOR_TIMESTAMP(
      "commit_order_leader_before_ha_flush_logs");

  /* flush transactions to the storage engine in a group */
  ha_flush_logs(true);

  reset_server_status(first);

  /* add to @@global.gtid_executed */
  gtid_state->update_commit_group(first);

  mysql_mutex_unlock(mysql_bin_log.get_commit_lock());

  /*
    awake all waiting threads for leader to flush transactions
    to the storage engine
  */
  Commit_stage_manager::get_instance().signal_done(
      first, Commit_stage_manager::COMMIT_ORDER_FLUSH_STAGE);
}

void Commit_order_manager::reset_server_status(THD *first_thd) {
  DBUG_TRACE;

  for (THD *thd = first_thd; thd != nullptr; thd = thd->next_to_commit) {
    thd->server_status &= ~SERVER_STATUS_IN_TRANS;
  }
}

void Commit_order_manager::finish_one(Slave_worker *worker) {
  DBUG_TRACE;

  if (this->m_workers[worker->id].m_stage ==
      cs::apply::Commit_order_queue::enum_worker_stage::WAITED) {
    assert(this->m_workers.front() == worker->id);
    assert(!this->m_workers.is_empty());

    cs::apply::Commit_order_queue::sequence_type this_seq_nr{0};
    auto this_worker{cs::apply::Commit_order_queue::NO_WORKER};
    std::tie(this_worker, this_seq_nr) = this->m_workers.pop();
    auto next_seq_nr =
        cs::apply::Commit_order_queue::get_next_sequence_nr(this_seq_nr);
    assert(worker->id == this_worker);

    auto next_worker = this->m_workers.front();
    if (next_worker !=
            cs::apply::Commit_order_queue::NO_WORKER &&  // There is a worker to
                                                         // unblock
        (this->m_workers[next_worker].m_stage ==
             cs::apply::Commit_order_queue::enum_worker_stage::
                 FINISHED_APPLYING ||             // but only if that worker
         this->m_workers[next_worker].m_stage ==  // stage implies the need
             cs::apply::Commit_order_queue::enum_worker_stage::
                 REQUESTED_GRANT) &&
        this->m_workers[next_worker].freeze_commit_sequence_nr(
            next_seq_nr)) {  // and this worker is the one that has
                             // to do it
      this->m_workers[next_worker].m_mdl_context->m_wait.set_status(
          MDL_wait::GRANTED);
      this->m_workers[next_worker].unfreeze_commit_sequence_nr(next_seq_nr);
    }

    this->m_workers[this_worker].m_mdl_context->m_wait.reset_status();
    this->m_workers[this_worker].m_stage =
        cs::apply::Commit_order_queue::enum_worker_stage::FINISHED;
  }
}

void Commit_order_manager::finish(Slave_worker *worker) {
  DBUG_TRACE;

  if (m_workers[worker->id].m_stage ==
      cs::apply::Commit_order_queue::enum_worker_stage::WAITED) {
    DBUG_PRINT("info",
               ("Worker %lu is signalling next transaction", worker->id));

    if (!get_rollback_status() &&
        worker->info_thd->is_current_stmt_binlog_disabled()) {
      /*
        If flush queue contains only threads executing slave preserve commit
        order, then flush committed transactions of all those waiting threads
        to the storage engine and awake them from wait. But if flush queue
        also contains threads writing to binlog, then it changes leader, so
        that the first BGC thread becomes leader. It waits until the new leader
        has committed and signalled all waiting commit order threads.
      */
      flush_engine_and_signal_threads(worker);

    } else {
      /*
        signal top worker of commit order queue to come out of wait
        and continue processing.
      */
      finish_one(worker);
    }
  }
}

void Commit_order_manager::check_and_report_deadlock(THD *thd_self,
                                                     THD *thd_wait_for) {
  DBUG_TRACE;

  Slave_worker *self_w = get_thd_worker(thd_self);
  Slave_worker *wait_for_w = get_thd_worker(thd_wait_for);
  Commit_order_manager *mngr = self_w->get_commit_order_manager();

  /* Check if both workers are working for the same channel */
  if (mngr != nullptr && self_w->c_rli == wait_for_w->c_rli &&
      wait_for_w->sequence_number() > self_w->sequence_number()) {
    DBUG_PRINT("info", ("Found replica order commit deadlock"));
    mngr->report_deadlock(wait_for_w);
  }
}

void Commit_order_manager::report_deadlock(Slave_worker *worker) {
  DBUG_TRACE;
  worker->report_commit_order_deadlock();
  DBUG_EXECUTE_IF("rpl_fake_cod_deadlock", {
    const char act[] = "now signal reported_deadlock";
    assert(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });
  this->m_workers[worker->id].m_mdl_context->m_wait.set_status(
      MDL_wait::VICTIM);
}

bool Commit_order_manager::wait(THD *thd) {
  DBUG_TRACE;
  assert(thd);

  if (has_commit_order_manager(thd)) {
    /*
      We only care about read/write transactions and those that
      have been registered in the commit order manager.
     */
    Slave_worker *worker = dynamic_cast<Slave_worker *>(thd->rli_slave);
    Commit_order_manager *mngr = worker->get_commit_order_manager();

    if (mngr->wait(worker)) {
      thd->commit_error = THD::CE_COMMIT_ERROR;
      return true;
    }
  }
  return false;
}

void Commit_order_manager::wait_and_finish(THD *thd, bool error) {
  DBUG_TRACE;
  assert(thd);

  if (has_commit_order_manager(thd)) {
    /*
      We only care about read/write transactions and those that
      have been registered in the commit order manager.
     */
    Slave_worker *worker = dynamic_cast<Slave_worker *>(thd->rli_slave);
    Commit_order_manager *mngr = worker->get_commit_order_manager();

    if (error || worker->found_commit_order_deadlock()) {
      // Error or deadlock: if not retryable, release next worker
      bool ret;
      std::tie(ret, std::ignore, std::ignore) =
          worker->check_and_report_end_of_retries(thd);
      if (ret) {
        /*
          worker can set m_rollback_trx when it is its turn to commit,
          so need to call wait() before updating m_rollback_trx.
        */
        mngr->wait(worker);
        mngr->set_rollback_status();
        mngr->finish(worker);
      }
    } else {
      // No error or deadlock: release next worker.
      mngr->wait(worker);
      mngr->finish(worker);
    }
  }
}

bool Commit_order_manager::get_rollback_status() {
  return m_rollback_trx.load();
}

void Commit_order_manager::set_rollback_status() { m_rollback_trx.store(true); }

void Commit_order_manager::unset_rollback_status() {
  m_rollback_trx.store(false);
}

bool Commit_order_manager::get_rollback_status(THD *thd) {
  DBUG_TRACE;
  assert(thd);
  if (has_commit_order_manager(thd)) {
    /*
      We only care about read/write transactions and those that
      have been registered in the commit order manager.
     */
    Slave_worker *worker = dynamic_cast<Slave_worker *>(thd->rli_slave);
    Commit_order_manager *mngr = worker->get_commit_order_manager();

    return mngr->get_rollback_status();
  }
  return false;
}

void Commit_order_manager::finish_one(THD *thd) {
  DBUG_TRACE;
  assert(thd);
  if (has_commit_order_manager(thd)) {
    /*
      We only care about read/write transactions and those that
      have been registered in the commit order manager.
     */
    Slave_worker *worker = dynamic_cast<Slave_worker *>(thd->rli_slave);
    Commit_order_manager *mngr = worker->get_commit_order_manager();

    mngr->finish_one(worker);
  }
}

bool Commit_order_manager::visit_lock_graph(
    Commit_order_lock_graph &wait_for_commit,
    MDL_wait_for_graph_visitor &visitor) {
  DBUG_TRACE;

  auto src_ctx = wait_for_commit.get_ctx();
  if (src_ctx->m_wait.get_status() != MDL_wait::WS_EMPTY)
    return false;  // Wait status changed in the meanwhile
  if (visitor.enter_node(src_ctx)) return true;

  raii::Sentry<> visitor_guard{[&]() -> void { visitor.leave_node(src_ctx); }};

  static const std::array<
      std::function<bool(cs::apply::Commit_order_queue::Node *,
                         MDL_wait_for_graph_visitor &)>,
      2>
      validators{// Inspect if the worker is a directly dependent node of the
                 // visitor
                 [](cs::apply::Commit_order_queue::Node *wkr,
                    MDL_wait_for_graph_visitor &vstr) -> bool {
                   return vstr.inspect_edge(wkr->m_mdl_context);
                 },
                 // Inspect if the visitor is a recursevly dependent node of the
                 // worker
                 [](cs::apply::Commit_order_queue::Node *wkr,
                    MDL_wait_for_graph_visitor &vstr) -> bool {
                   return wkr->m_mdl_context->visit_subgraph(&vstr);
                 }};

  auto src_worker_id = wait_for_commit.get_worker_id();
  for (auto validate : validators) {
    raii::Sentry<> freeze_guard{[&]() -> void { this->m_workers.unfreeze(); }};
    this->m_workers.freeze();

    for (auto w : this->m_workers) {
      assert(w != nullptr);
      if (w->m_worker_id == src_worker_id) break;
      if (w->m_stage ==
          cs::apply::Commit_order_queue::enum_worker_stage::REQUESTED_GRANT)
        continue;
      if (validate(w, visitor)) {
        return true;
      }
    }
  }

  return false;
}

bool has_commit_order_manager(const THD *thd) {
  return is_mts_worker(thd) &&
         thd->rli_slave->get_commit_order_manager() != nullptr;
}

bool Commit_order_manager::wait_for_its_turn_before_flush_stage(THD *thd) {
  switch (thd->lex->sql_command) {
    case SQLCOM_ALTER_TABLE:
    case SQLCOM_ANALYZE:
    case SQLCOM_DROP_DB:
    case SQLCOM_DROP_EVENT:
    case SQLCOM_DROP_FUNCTION:
    case SQLCOM_DROP_PROCEDURE:
    case SQLCOM_DROP_TRIGGER:
    case SQLCOM_DROP_TABLE:
    case SQLCOM_DROP_VIEW:
    case SQLCOM_OPTIMIZE:
    case SQLCOM_REPAIR:
      return has_commit_order_manager(thd);
    default:
      break;
  }
  return false;
}

Commit_order_lock_graph::Commit_order_lock_graph(MDL_context &ctx,
                                                 Commit_order_manager &mngr,
                                                 uint32 worker_id)
    : m_ctx{ctx}, m_mngr{mngr}, m_worker_id{worker_id} {}

MDL_context *Commit_order_lock_graph::get_ctx() const { return &(this->m_ctx); }

uint32 Commit_order_lock_graph::get_worker_id() const {
  return this->m_worker_id;
}

bool Commit_order_lock_graph::accept_visitor(
    MDL_wait_for_graph_visitor *visitor) {
  DBUG_TRACE;
  return this->m_mngr.visit_lock_graph(*this, *visitor);
}

uint Commit_order_lock_graph::get_deadlock_weight() const {
  return DEADLOCK_WEIGHT_CO;  // Return the lowest weight so that workers are
                              // the threads to back-off because of more
                              // advanced and automated transaction retry
                              // capabilities.
}
