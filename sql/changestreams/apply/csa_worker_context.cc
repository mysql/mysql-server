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

#include "sql/changestreams/apply/csa_worker_context.h"
#include "sql/changestreams/apply/service/csa_service.h"
#include "sql/changestreams/apply/service/rescue_task.h"
#include "sql/mdl.h"        // MDL_context
#include "sql/rpl_rli.h"    // Relay_log_info
#include "sql/sql_class.h"  // THD

namespace cs::apply {

Csa_worker_context::Csa_worker_context(Trx_id trx_seq_num, Worker_id worker_id,
                                       const std::string &channel_id,
                                       THD *trx_ctx, int current_retry,
                                       int retries_num)
    : m_trx_id(trx_seq_num),
      m_worker_id(worker_id),
      m_channel_id(channel_id),
      m_trx_ctx(trx_ctx),
      m_current_retry(current_retry),
      m_retries_num(retries_num) {}

void Csa_worker_context::report_commit_order_deadlock(bool in_commit) {
  m_is_commit_order_deadlock = true;
  if (in_commit) return;

  switch (m_rpco_state.load()) {
    case Rpco_state::preparing:
      return;
    case Rpco_state::prepared:
      handle_commit_order_deadlock();
      return;
    case Rpco_state::commit:
      m_trx_ctx->mdl_context.m_wait.set_status(MDL_wait::VICTIM);
      return;
  }
}

bool Csa_worker_context::found_commit_order_deadlock() const {
  return m_is_commit_order_deadlock;
}

bool Csa_worker_context::wait_for_rollback() {
  auto rollback_future = m_rollback_promise.get_future();
  rollback_future.wait();
  return rollback_future.get();
}

void Csa_worker_context::handle_commit_order_deadlock() {
  if (!m_rollback.exchange(true)) {
    m_rpco_state = Rpco_state::preparing;
    assert(m_trx_ctx->rli_slave);
    mysql::csa::Transaction_conflict_monitor::get(
        m_trx_ctx->rli_slave->get_channel_instance_id())
        ->enqueue(mysql::csa::Rescue_task(
            m_trx_ctx->rli_slave, mysql::csa::Rescue_operation_type::rollback,
            m_rollback_promise, get_trx_id()));
  }
}

void Csa_worker_context::set_applied() {
  m_rpco_state.store(Rpco_state::prepared);
}

void Csa_worker_context::set_committing() {
  Rpco_state expected{Rpco_state::prepared};
  m_rpco_state.compare_exchange_strong(expected, Rpco_state::commit);
}

void Csa_worker_context::reset_commit_order_deadlock() {
  m_is_commit_order_deadlock = false;
  m_rollback = false;
  m_rpco_state = Rpco_state::preparing;
  m_rollback_promise = std::promise<bool>{};
}

bool Csa_worker_context::is_same_channel(
    const Parallel_worker_context *arg) const {
  const Csa_worker_context *other =
      dynamic_cast<const Csa_worker_context *>(arg);
  assert(other);
  if (other != nullptr) {
    return m_channel_id == other->get_channel_id();
  }
  return false;
}

const std::string &Csa_worker_context::get_channel_id() const {
  return m_channel_id;
}

THD *Csa_worker_context::get_transaction_ctx() { return m_trx_ctx; }

Csa_worker_context::Worker_id Csa_worker_context::get_worker_id() const {
  return m_worker_id;
}

MDL_context *Csa_worker_context::get_mdl_context() {
  return &(m_trx_ctx->mdl_context);
}

// check Slave_reporting_capability::has_temporary_error
bool Csa_worker_context::has_temporary_error(THD *thd, int error_arg) {
  if (thd->is_fatal_error() || (!thd->is_error() && error_arg == 0)) return 0;

  const unsigned int error =
      (error_arg == 0) ? thd->get_stmt_da()->mysql_errno() : error_arg;

  // temporary error types
  if (error == ER_LOCK_DEADLOCK || error == ER_LOCK_WAIT_TIMEOUT) return 1;

  // Check if temporary error is indicated by warning pushed by the engine
  Diagnostics_area::Sql_condition_iterator it =
      thd->get_stmt_da()->sql_conditions();
  const Sql_condition *err;
  while ((err = it++)) {
    if (err->mysql_errno() == ER_GET_TEMPORARY_ERRMSG ||
        err->mysql_errno() == ER_REPLICA_SILENT_RETRY_TRANSACTION) {
      return 1;
    }
  }
  return 0;
}

bool Csa_worker_context::can_be_retried(THD *thd) {
  DBUG_TRACE;
  unsigned int error = 0;
  if (found_commit_order_deadlock()) {
    Diagnostics_area *da = thd->get_stmt_da();
    if (!da->is_error() ||
        has_temporary_error(thd, da->is_error() ? da->mysql_errno() : 0)) {
      error = ER_LOCK_DEADLOCK;
    }
  }
  if (!has_temporary_error(thd, error) ||
      thd->get_transaction()->cannot_safely_rollback(Transaction_ctx::SESSION))
    return false;

  if (m_current_retry >= m_retries_num) {
    thd->fatal_error();
    // cannot retry
    return false;
  }
  ++m_current_retry;
  return true;
}

Csa_worker_context::Trx_id Csa_worker_context::get_trx_id() { return m_trx_id; }

instruments::Worker_metrics &Csa_worker_context::get_worker_metrics() {
  // if (m_is_worker_metric_collection_enabled) return m_worker_metrics;
  return m_disabled_worker_metrics;
}

instruments::Dummy_worker_metrics
    Csa_worker_context::m_disabled_worker_metrics{};

const char *Csa_worker_context::get_for_channel_id(bool upper_case) const {
  if (!m_for_channel_id.empty()) {
    return m_for_channel_id.c_str();
  } else {
    std::stringstream ss;
    if (upper_case) {
      ss << " FOR CHANNEL ";
    } else {
      ss << " for channel ";
    }
    ss << "'" << m_channel_id << "'";
    m_for_channel_id = ss.str();
    return m_for_channel_id.c_str();
  }
}

void Csa_worker_context::update(Trx_id trx_seq_num, Worker_id worker_id,
                                THD *trx_ctx, int current_retry) {
  m_trx_id = trx_seq_num;
  m_trx_ctx = trx_ctx;
  update(worker_id, current_retry);
}

void Csa_worker_context::update(Worker_id worker_id, int current_retry) {
  m_worker_id = worker_id;
  m_current_retry = current_retry;
}

void Csa_worker_context::update(Trx_id trx_seq_num, Worker_id worker_id,
                                const std::string &channel_id, THD *trx_ctx,
                                int current_retry, int retries_num) {
  m_channel_id = channel_id;
  m_retries_num = retries_num;
  update(trx_seq_num, worker_id, trx_ctx, current_retry);
}

bool Csa_worker_context::is_csa() const { return true; }

}  // namespace cs::apply
