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

#include "sql/changestreams/apply/service/rescue_task.h"

namespace mysql::csa {

Rescue_task::Rescue_task(Relay_log_info *rli, Rescue_operation_type op_type,
                         std::promise<bool> &action_promise, int job_id)
    : m_rli(rli),
      m_rescue_operation_type(op_type),
      m_action_promise(action_promise),
      m_job_id(job_id) {
  assert(m_rli->info_thd);
}

// Perform rescue operation
void Rescue_task::operator()() {
  if (m_rescue_operation_type == Rescue_operation_type::rollback) {
    rollback_trx();
    return;
  }
  set_promise();
  MYSQL_LIB_LOG_DEBUG()
      << "CSA mitigation action: There was a mitigation action "
         "requested, but type of operation is unknown. Skipping; job id: "
      << m_job_id;
}

void Rescue_task::set_promise() {
  if (!m_action_promise.has_value()) {
    return;
  }
  m_action_promise.value().get().set_value(true);
}

// Execute rollback
void Rescue_task::rollback_trx() {
  if (!m_action_promise.has_value()) return;
  MYSQL_LIB_LOG_DEBUG() << "CSA mitigation action: rolling back transaction "
                           "due to detected commit order deadlock. Job id: "
                        << m_job_id << ". Rollback start...";

  long stack_ptr = 0;

  assert(m_rli);
  auto *thd = m_rli->info_thd;
  assert(thd);

  thd->store_globals();
  thd->temporary_tables = m_rli->save_temporary_tables;
  thd->rli_slave = m_rli;
  const char *m_saved_thread_stack = thd->thread_stack;
  thd->thread_stack = reinterpret_cast<char *>(&stack_ptr);
  bool rollback_failed{false};

  if (thd->get_transaction()->cannot_safely_rollback(
          Transaction_ctx::SESSION)) {
    rollback_failed = true;
  } else {
    // force rollback by setting an error
    if (!thd->get_stmt_da()->is_set()) {
      thd->get_stmt_da()->set_error_status(thd, ER_LOCK_DEADLOCK);
    }
    m_rli->cleanup_context(thd, true);
    thd->clear_error();
  }

  thd->restore_globals();
  thd->temporary_tables = nullptr;
  // restore the thread stack
  thd->thread_stack = m_saved_thread_stack;
  m_saved_thread_stack = nullptr;
  if (!rollback_failed) {
    MYSQL_LIB_LOG_DEBUG()
        << "CSA mitigation action: rollback finished. "
           "Applier will continue normally. No action is needed. Job id: "
        << m_job_id;
  } else {
    MYSQL_LIB_LOG_DEBUG()
        << "CSA RPCO deadlock was found and transaction was "
           "requested to rollback. It is unsafe to rollback this "
           "transaction, therefore, CSA will stop. Job id: "
        << m_job_id;
  }
  m_action_promise.value().get().set_value(rollback_failed);
}

}  // namespace mysql::csa
