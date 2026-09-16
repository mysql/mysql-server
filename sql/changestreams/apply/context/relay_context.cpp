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

#include "sql/changestreams/apply/csa_worker_context.h"  // Csa_worker_context
#include "sql/rpl_info_dummy.h"
#include "sql/rpl_info_factory.h"
#include "sql/rpl_mi.h"
#include "sql/rpl_replica.h"
#include "sql/rpl_utility.h"

#include <mysql/components/services/log_builtins.h>
#include "sql/changestreams/apply/context/relay_context.h"
#include "sql/changestreams/apply/psi/psi.h"
#include "sql/changestreams/apply/psi/stage.h"
#include "sql/changestreams/apply/session/session_guard.h"
#include "sql/changestreams/apply/session/session_service.h"

namespace mysql::csa {

Relay_context::Relay_context(std::size_t id, Relay_log_info *channel_config_rli)
    : m_rli(nullptr),
      m_session(channel_config_rli->info_thd, id),
      m_commit_order_manager(channel_config_rli->get_commit_order_manager()),
      m_channel_config_rli(channel_config_rli),
      m_id(id) {
  auto clean_up_on_error = [this]() -> auto{
    delete m_rli->current_mts_submode;
    m_rli.reset();
  };

  // setup dummy rli
  m_rli.reset(Rpl_info_factory::create_rli(INFO_REPOSITORY_DUMMY, false,
                                           (const char *)""));

  m_rli->current_mts_submode = new Mts_submode_logical_clock();

  m_rli->set_channel_instance_id(
      m_channel_config_rli->get_channel_instance_id());

  m_csa_worker_context =
      std::make_unique<Csa_worker_context>(0, 0, "", nullptr, 0, 0);
  m_rli->set_csa_worker_context(m_csa_worker_context.get());

  if (!m_rli || !m_rli->current_mts_submode || !m_csa_worker_context) {
    // out of memory
    clean_up_on_error();
    return;
  }

  THD *thd = m_session.get_thd();

  if (!strcmp(channel_config_rli->get_channel(), "group_replication_applier")) {
    thd->rpl_thd_ctx.set_rpl_channel_type(GR_APPLIER_CHANNEL);
  } else if (!strcmp(channel_config_rli->get_channel(),
                     "group_replication_recovery")) {
    thd->rpl_thd_ctx.set_rpl_channel_type(GR_RECOVERY_CHANNEL);
  } else {
    thd->rpl_thd_ctx.set_rpl_channel_type(RPL_STANDARD_CHANNEL);
  }

  // Set applier thread InnoDB priority
  thd->thd_tx_priority = channel_config_rli->get_thd_tx_priority();
  DBUG_EXECUTE_IF("dbug_set_high_prio_sql_thread",
                  { thd->thd_tx_priority = 1; });

  // Set write set related options
  thd->get_transaction()
      ->get_transaction_write_set_ctx()
      ->set_local_ignore_write_set_memory_limit(
          channel_config_rli->get_ignore_write_set_memory_limit());
  thd->get_transaction()
      ->get_transaction_write_set_ctx()
      ->set_local_allow_drop_write_set(
          channel_config_rli->get_allow_drop_write_set());

  if (Relay_log_info::PK_CHECK_STREAM !=
      channel_config_rli->get_require_table_primary_key_check())
    thd->variables.sql_require_primary_key =
        (channel_config_rli->get_require_table_primary_key_check() ==
         Relay_log_info::PK_CHECK_ON);
  m_rli->set_require_table_primary_key_check(
      channel_config_rli->get_require_table_primary_key_check());

  m_rli->set_parent_rli(channel_config_rli);

  thd->variables.sql_generate_invisible_primary_key = false;
  if (thd->rpl_thd_ctx.get_rpl_channel_type() != GR_APPLIER_CHANNEL &&
      thd->rpl_thd_ctx.get_rpl_channel_type() != GR_RECOVERY_CHANNEL &&
      Relay_log_info::PK_CHECK_GENERATE ==
          channel_config_rli->get_require_table_primary_key_check()) {
    thd->variables.sql_generate_invisible_primary_key = true;
  }

  thd->variables.restrict_fk_on_non_standard_key = false;

  m_rli->set_filter(channel_config_rli->rpl_filter);

  m_rli->deferred_events_collecting = m_rli->rpl_filter->is_on();
  if (m_rli->deferred_events_collecting) {
    m_rli->deferred_events = new Deferred_log_events();
    if (!m_rli->deferred_events) {
      clean_up_on_error();
      return;
    }
  }

  // copy row format option
  m_rli->set_require_row_format(channel_config_rli->is_row_format_required());
  thd->variables.require_row_format = m_rli->is_row_format_required();

  // privilege checks user security context
  if (!channel_config_rli->is_privilege_checks_user_null()) {
    m_rli->set_privilege_checks_user(
        channel_config_rli->get_privilege_checks_username().c_str(),
        channel_config_rli->get_privilege_checks_hostname().c_str());
  }

  if (!channel_config_rli->check_privilege_checks_user()) {
    Session_guard replace_guard(thd);
    channel_config_rli->initialize_security_context(thd);
  }

  if (m_rli->m_assign_gtids_to_anonymous_transactions_info.set_info(
          channel_config_rli->m_assign_gtids_to_anonymous_transactions_info
              .get_type(),
          (channel_config_rli->m_assign_gtids_to_anonymous_transactions_info
               .get_value()
               .c_str()))) {
    clean_up_on_error();
    return;
  }

  m_rli->abort_slave = 0;
  m_rli->trans_retries = 0;  // start from "no error"
  m_rli->set_applier_version(cs::apply::Applier_version::csa);
}

void Relay_context::report_error(const std::string &trx_id) {
  if (m_rli->is_csa_stop_error_suppression_enabled()) {
    return;
  }

  char channel_message[MAX_SLAVE_ERRMSG];
  snprintf(channel_message, MAX_SLAVE_ERRMSG,
           "Change Stream Applier channel stopped because of error(s): "
           "Job '%s' failed. See error log or performance schema tables "
           "for more details about this failure or others, if any.",
           trx_id.c_str());
  auto *thd = m_session.get_thd();
  // was THD killed externally
  bool thd_killed = thd->is_killed() && !m_session.self_killed();
  if (!thd->get_stmt_da()->is_error() && thd_killed) {
    thd->get_stmt_da()->set_error_status(thd, ER_QUERY_INTERRUPTED);
  }
  // true if THD has and error, but hasn't been killed internally
  bool real_thd_error =
      thd->get_stmt_da()->is_error() &&
      (thd->get_stmt_da()->mysql_errno() != ER_QUERY_INTERRUPTED || thd_killed);
  // Report only if THD / RLI has an error, but hasn't been killed internally
  if (!m_rli->last_error().number && real_thd_error) {
    // avoid treating error as transient
    thd->fatal_error();
    m_rli->report(ERROR_LEVEL, thd->get_stmt_da()->mysql_errno(),
                  thd->get_stmt_da()->message_text(),
                  thd->get_stmt_da()->message_text());
  }

  if (m_rli->last_error().number &&
      (m_rli->last_error().number != ER_QUERY_INTERRUPTED || thd_killed)) {
    // avoid treating error as transient
    m_channel_config_rli->info_thd->fatal_error();
    m_channel_config_rli->report(ERROR_LEVEL, m_rli->last_error().number,
                                 channel_message, m_rli->last_error().message);
  }
}

Relay_context::~Relay_context() {
  assert(m_rli);
  delete m_rli->current_mts_submode;
  delete m_rli->deferred_events;
  assert(m_attached == false);
}

Relay_log_info *Relay_context::get_relay_log_info() { return m_rli.get(); }

void Relay_context::clean() {
  THD *thd = m_session.get_thd();
  Session_guard replace_guard(thd);
  attach_session();
  attach_rli();
  thd->clear_error();
  m_rli->cleanup_context(thd, true);
  detach_rli();
  detach_session();
}

void Relay_context::awake(bool force_kill) { m_session.awake(force_kill); }

void Relay_context::enable_stop_error_suppression_if_clean() {
  auto *thd = m_session.get_thd();
  if ((thd->is_killed() && !m_session.self_killed()) || thd->is_fatal_error() ||
      thd->is_slave_error) {
    return;
  }
  m_rli->enable_csa_stop_error_suppression_if_clean();
}

void Relay_context::set_fde(Format_description_log_event *fde) {
  m_rli->set_fde_ptr(fde);
}

void Relay_context::attach_session() {
  m_session.attach(m_rli->save_temporary_tables);
  m_attached = true;
}

void Relay_context::set_parallel_worker_context(
    Commit_order_manager *com,
    [[maybe_unused]] Relay_context::Trx_id trx_seq_num,
    [[maybe_unused]] Relay_context::Worker_id worker_id,
    [[maybe_unused]] const std::string &channel_id,
    [[maybe_unused]] int current_retry, [[maybe_unused]] int retries_num) {
  m_commit_order_manager = com;
  // assert that we already have a session assigned
  auto *thd = m_session.get_thd();
  assert(thd);

  // create and set parallel worker context for the current transaction
  // connects to COM here
  m_csa_worker_context->update(trx_seq_num, worker_id, channel_id, thd,
                               current_retry, retries_num);
  m_rli->set_commit_order_manager(com);
}

void Relay_context::register_to_commit_order() {
  if (m_commit_order_manager && !m_session.self_killed()) {
    m_commit_order_manager->init_worker_context(
        m_csa_worker_context->get_worker_id(), m_session.get_thd());
    m_commit_order_manager->register_trx(m_rli->get_parallel_worker_context());
  }
}

cs::apply::Csa_worker_context *Relay_context::get_parallel_worker_context() {
  return m_csa_worker_context.get();
}

void Relay_context::detach_session() {
  m_session.detach();
  m_attached = false;
}

bool Relay_context::is_valid() { return m_rli.operator bool(); }

unsigned int Relay_context::get_thd_id() const {
  return m_session.get_thd_id();
}

std::size_t Relay_context::get_id() const { return m_id; }

void Relay_context::attach_rli() {
  m_rli->info_thd = m_session.get_thd();
  m_rli->info_thd->rli_slave = m_rli.get();
  concurrency::set_thd_stage(m_session.get_thd(), stage_csa_session_attached);
}

void Relay_context::detach_rli() {
  m_rli->info_thd->rli_slave = nullptr;
  m_rli->info_thd = nullptr;
  concurrency::set_thd_stage(m_session.get_thd(), stage_csa_session_detached);
}

bool Relay_context::wait_for_rollback() {
  assert(m_csa_worker_context);
  if (!m_csa_worker_context) {
    return true;
  }
  // do not touch THD data before rollback is finished
  if (m_csa_worker_context->wait_for_rollback()) {
    m_rli->report(ERROR_LEVEL, ER_LOCK_DEADLOCK, "%s",
                  ER_THD(get_session().get_thd(), ER_LOCK_DEADLOCK));
    return true;
  }
  return false;
}

bool Relay_context::can_be_retried() {
  if (!m_csa_worker_context) {
    return true;
  }
  return m_csa_worker_context->can_be_retried(get_session().get_thd());
}

void Relay_context::retry_transaction(int current_count, bool skip_rollback) {
  auto *thd = m_session.get_thd();
  unsigned int err = ER_LOCK_DEADLOCK;
  if (thd->get_stmt_da()->is_error()) {
    err = thd->get_stmt_da()->mysql_errno();
  } else {
    thd->get_stmt_da()->set_error_status(thd, ER_LOCK_DEADLOCK);
  }
  bool silent = false;
  bool temporary_error = m_rli->has_temporary_error(thd, err, &silent);
  if (!silent) {
    m_rli->retried_processing(err, ER_THD_NONCONST(thd, err), current_count);
  }
  // increment overall number of retries
  mysql_mutex_lock(&m_channel_config_rli->data_lock);
  ++m_channel_config_rli->retried_trans;
  mysql_mutex_unlock(&m_channel_config_rli->data_lock);
  if (!skip_rollback && temporary_error) {
    m_rli->cleanup_context(thd, thd->get_stmt_da()->is_error());
    thd->clear_error();
  }
  if (m_csa_worker_context) {
    m_csa_worker_context->reset_commit_order_deadlock();
  }
}

}  // namespace mysql::csa
