/* Copyright (c) 2018, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include <algorithm>

#include <mysql/components/services/log_builtins.h>
#include "plugin/group_replication/include/consistency_manager.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_messages/sync_before_execution_message.h"
#include "plugin/group_replication/include/plugin_messages/transaction_prepared_message.h"
#include "plugin/group_replication/include/plugin_psi.h"

Transaction_consistency_info::Transaction_consistency_info(
    my_thread_id thread_id, bool local_transaction, const rpl_sid *sid,
    rpl_sidno sidno, rpl_gno gno,
    enum_group_replication_consistency_level consistency_level,
    std::list<Gcs_member_identifier> *members_that_must_prepare_the_transaction)
    : m_thread_id(thread_id),
      m_local_transaction(local_transaction),
      m_sid_specified(sid != NULL ? true : false),
      m_sidno(sidno),
      m_gno(gno),
      m_consistency_level(consistency_level),
      m_members_that_must_prepare_the_transaction(
          members_that_must_prepare_the_transaction),
      m_transaction_prepared_locally(local_transaction),
      m_transaction_prepared_remotely(false) {
  DBUG_TRACE;
  DBUG_ASSERT(m_consistency_level >= GROUP_REPLICATION_CONSISTENCY_AFTER);
  DBUG_ASSERT(NULL != m_members_that_must_prepare_the_transaction);
  DBUG_PRINT(
      "info",
      ("thread_id: %u; local_transaction: %d; gtid: %d:%llu; sid_specified: "
       "%d; consistency_level: %d; "
       "transaction_prepared_locally: %d; transaction_prepared_remotely: %d",
       m_thread_id, m_local_transaction, m_sidno, m_gno, m_sid_specified,
       m_consistency_level, m_transaction_prepared_locally,
       m_transaction_prepared_remotely));

  if (NULL != sid) {
    m_sid.copy_from(*sid);
  } else {
    m_sid.clear();
  }
}

Transaction_consistency_info::~Transaction_consistency_info() {
  delete m_members_that_must_prepare_the_transaction;
}

my_thread_id Transaction_consistency_info::get_thread_id() {
  return m_thread_id;
}

bool Transaction_consistency_info::is_local_transaction() {
  return m_local_transaction;
}

bool Transaction_consistency_info::is_transaction_prepared_locally() {
  return m_transaction_prepared_locally;
}

rpl_sidno Transaction_consistency_info::get_sidno() { return m_sidno; }

rpl_gno Transaction_consistency_info::get_gno() { return m_gno; }

enum_group_replication_consistency_level
Transaction_consistency_info::get_consistency_level() {
  return m_consistency_level;
}

bool Transaction_consistency_info::is_a_single_member_group() {
  return 0 == m_members_that_must_prepare_the_transaction->size();
}

bool Transaction_consistency_info::is_the_transaction_prepared_remotely() {
  return m_transaction_prepared_remotely ||
         m_members_that_must_prepare_the_transaction->empty();
}

int Transaction_consistency_info::after_applier_prepare(
    my_thread_id thread_id,
    Group_member_info::Group_member_status member_status) {
  DBUG_TRACE;
  DBUG_ASSERT(m_consistency_level >= GROUP_REPLICATION_CONSISTENCY_AFTER);
  /*
    Update thread_id with applier worker id in order to allow it to
    be resumed once all prepared messages are delivered.
  */
  m_thread_id = thread_id;
  m_transaction_prepared_locally = true;

  DBUG_PRINT(
      "info",
      ("thread_id: %u; local_transaction: %d; gtid: %d:%llu; sid_specified: "
       "%d; consistency_level: %d; "
       "transaction_prepared_locally: %d; transaction_prepared_remotely: %d; "
       "member_status: %d",
       m_thread_id, m_local_transaction, m_sidno, m_gno, m_sid_specified,
       m_consistency_level, m_transaction_prepared_locally,
       m_transaction_prepared_remotely, member_status));

  // Only ONLINE members do acknowledge transactions prepare.
  if (Group_member_info::MEMBER_ONLINE != member_status) {
    return 0;
  }

  DBUG_EXECUTE_IF(
      "group_replication_wait_before_message_send_after_applier_prepare", {
        const char act[] =
            "now signal "
            "signal.after_before_message_send_after_applier_prepare_waiting "
            "wait_for "
            "signal.after_before_message_send_after_applier_prepare_continue";
        DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
      };);

  DBUG_EXECUTE_IF(
      "group_replication_wait_on_supress_message_send_after_applier_prepare", {
        const char act[] =
            "now signal "
            "signal.after_supress_message_send_after_applier_prepare_waiting "
            "wait_for "
            "signal.after_supress_message_send_after_applier_prepare_continue";
        DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
        return 0;
      };);

  Transaction_prepared_message message((m_sid_specified ? &m_sid : NULL),
                                       m_gno);
  if (gcs_module->send_message(message)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_SEND_TRX_PREPARED_MESSAGE_FAILED,
                 m_sidno, m_gno, m_thread_id);
    return 1;
    /* purecov: end */
  }

  return 0;
}

int Transaction_consistency_info::handle_remote_prepare(
    const Gcs_member_identifier &gcs_member_id) {
  DBUG_TRACE;
  DBUG_PRINT(
      "info",
      ("thread_id: %u; local_transaction: %d; gtid: %d:%llu; sid_specified: "
       "%d; consistency_level: %d; "
       "transaction_prepared_locally: %d; transaction_prepared_remotely: %d",
       m_thread_id, m_local_transaction, m_sidno, m_gno, m_sid_specified,
       m_consistency_level, m_transaction_prepared_locally,
       m_transaction_prepared_remotely));

  m_members_that_must_prepare_the_transaction->remove(gcs_member_id);

  if (m_members_that_must_prepare_the_transaction->empty()) {
    m_transaction_prepared_remotely = true;

    if (m_transaction_prepared_locally) {
      if (transactions_latch->releaseTicket(m_thread_id)) {
        /* purecov: begin inspected */
        LogPluginErr(ERROR_LEVEL,
                     ER_GRP_RPL_RELEASE_COMMIT_AFTER_GROUP_PREPARE_FAILED,
                     m_sidno, m_gno, m_thread_id);
        return CONSISTENCY_INFO_OUTCOME_ERROR;
        /* purecov: end */
      }

      return CONSISTENCY_INFO_OUTCOME_COMMIT;
    }
  }

  return CONSISTENCY_INFO_OUTCOME_OK;
}

int Transaction_consistency_info::handle_member_leave(
    const std::vector<Gcs_member_identifier> &leaving_members) {
  DBUG_TRACE;
  int error = 0;

  std::vector<Gcs_member_identifier>::const_iterator leaving_members_it;
  for (leaving_members_it = leaving_members.begin();
       leaving_members_it != leaving_members.end(); leaving_members_it++) {
    int member_error = handle_remote_prepare(*leaving_members_it);
    error = std::max(error, member_error);
  }

  DBUG_PRINT(
      "info",
      ("thread_id: %u; local_transaction: %d; gtid: %d:%llu; sid_specified: "
       "%d; consistency_level: %d; "
       "transaction_prepared_locally: %d; transaction_prepared_remotely: %d; "
       "error: %d",
       m_thread_id, m_local_transaction, m_sidno, m_gno, m_sid_specified,
       m_consistency_level, m_transaction_prepared_locally,
       m_transaction_prepared_remotely, error));

  return error;
}

Transaction_consistency_manager::Transaction_consistency_manager()
    : m_plugin_stopping(true) {
  m_map_lock = new Checkable_rwlock(
#ifdef HAVE_PSI_INTERFACE
      key_GR_RWLOCK_transaction_consistency_manager_map
#endif
  );

  m_prepared_transactions_on_my_applier_lock = new Checkable_rwlock(
#ifdef HAVE_PSI_INTERFACE
      key_GR_RWLOCK_transaction_consistency_manager_prepared_transactions_on_my_applier
#endif
  );
}

Transaction_consistency_manager::~Transaction_consistency_manager() {
  DBUG_ASSERT(m_map.empty());
  DBUG_ASSERT(m_prepared_transactions_on_my_applier.empty());
  DBUG_ASSERT(m_new_transactions_waiting.empty());
  DBUG_ASSERT(m_delayed_view_change_events.empty());
  delete m_map_lock;
  delete m_prepared_transactions_on_my_applier_lock;
}

void Transaction_consistency_manager::clear() {
  DBUG_TRACE;
  m_map_lock->wrlock();
  for (typename Transaction_consistency_manager_map::iterator it =
           m_map.begin();
       it != m_map.end(); ++it) {
    delete it->second; /* purecov: inspected */
  }
  m_map.clear();
  m_map_lock->unlock();

  m_prepared_transactions_on_my_applier_lock->wrlock();
  m_prepared_transactions_on_my_applier.clear();
  m_new_transactions_waiting.clear();

  while (!m_delayed_view_change_events.empty()) {
    delete m_delayed_view_change_events.front();
    m_delayed_view_change_events.pop_front();
  }
  m_delayed_view_change_events.clear();
  m_prepared_transactions_on_my_applier_lock->unlock();
}

int Transaction_consistency_manager::after_certification(
    Transaction_consistency_info *transaction_info) {
  DBUG_TRACE;
  DBUG_ASSERT(transaction_info->get_consistency_level() >=
              GROUP_REPLICATION_CONSISTENCY_AFTER);
  int error = 0;
  Transaction_consistency_manager_key key(transaction_info->get_sidno(),
                                          transaction_info->get_gno());
  m_map_lock->wrlock();

  typename Transaction_consistency_manager_map::iterator it = m_map.find(key);
  if (it != m_map.end()) {
    /* purecov: begin inspected */
    m_map_lock->unlock();
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_TRX_ALREADY_EXISTS_ON_TCM_ON_AFTER_CERTIFICATION,
                 transaction_info->get_sidno(), transaction_info->get_gno());
    return 1;
    /* purecov: end */
  }

  // Group with only one member.
  if (transaction_info->is_local_transaction() &&
      transaction_info->is_a_single_member_group()) {
    transactions_latch->releaseTicket(transaction_info->get_thread_id());
    delete transaction_info;
    m_map_lock->unlock();
    return 0;
  }

  std::pair<typename Transaction_consistency_manager_map::iterator, bool> ret =
      m_map.insert(Transaction_consistency_manager_pair(key, transaction_info));
  if (ret.second == false) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_FAILED_TO_INSERT_TRX_ON_TCM_ON_AFTER_CERTIFICATION,
                 transaction_info->get_sidno(), transaction_info->get_gno());
    error = 1;
    /* purecov: end */
  }

  DBUG_PRINT(
      "info",
      ("gtid: %d:%llu; consistency_level: %d; ", transaction_info->get_sidno(),
       transaction_info->get_gno(), transaction_info->get_consistency_level()));

  m_map_lock->unlock();
  return error;
}

int Transaction_consistency_manager::after_applier_prepare(
    rpl_sidno sidno, rpl_gno gno, my_thread_id thread_id,
    Group_member_info::Group_member_status member_status) {
  DBUG_TRACE;
  int error = 0;
  Transaction_consistency_manager_key key(sidno, gno);

  m_map_lock->rdlock();
  typename Transaction_consistency_manager_map::iterator it = m_map.find(key);
  if (it == m_map.end()) {
    // Nothing to do, this is a transaction with eventual consistency.
    m_map_lock->unlock();
    return 0;
  }

  Transaction_consistency_info *transaction_info = it->second;
  const bool transaction_prepared_remotely =
      transaction_info->is_the_transaction_prepared_remotely();

  if (!transaction_prepared_remotely &&
      transactions_latch->registerTicket(thread_id)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_REGISTER_TRX_TO_WAIT_FOR_GROUP_PREPARE_FAILED,
                 sidno, gno, thread_id);
    m_map_lock->unlock();
    return 1;
    /* purecov: end */
  }

  DBUG_PRINT(
      "info",
      ("gtid: %d:%llu; consistency_level: %d; ", transaction_info->get_sidno(),
       transaction_info->get_gno(), transaction_info->get_consistency_level()));

  // Mark the transaction as prepared for consistent reads.
  m_prepared_transactions_on_my_applier_lock->wrlock();
  m_prepared_transactions_on_my_applier.push_back(key);
  m_prepared_transactions_on_my_applier_lock->unlock();

  if (transaction_info->after_applier_prepare(thread_id, member_status)) {
    /* purecov: begin inspected */
    m_map_lock->unlock();
    error = 1;
    goto end;
    /* purecov: end */
  }

  m_map_lock->unlock();

  DBUG_EXECUTE_IF("group_replication_wait_on_after_applier_prepare", {
    const char act[] =
        "now signal signal.after_applier_prepare_waiting wait_for "
        "signal.after_applier_prepare_continue";
    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  };);

  if (!transaction_prepared_remotely &&
      transactions_latch->waitTicket(thread_id)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_TRX_WAIT_FOR_GROUP_PREPARE_FAILED,
                 sidno, gno, thread_id);
    error = 1;
    goto end;
    /* purecov: end */
  }

  // Remove transaction from map
  if (transaction_prepared_remotely) {
    m_map_lock->wrlock();
    typename Transaction_consistency_manager_map::iterator it = m_map.find(key);
    if (it != m_map.end()) {
      delete it->second;
      m_map.erase(it);
    }
    m_map_lock->unlock();
  }

end:
  if (error) {
    /* purecov: begin inspected */
    remove_prepared_transaction(key);
    transactions_latch->releaseTicket(thread_id);
    transactions_latch->waitTicket(thread_id);
    /* purecov: end */
  }

  return error;
}

int Transaction_consistency_manager::handle_remote_prepare(
    const rpl_sid *sid, rpl_gno gno,
    const Gcs_member_identifier &gcs_member_id) {
  DBUG_TRACE;
  rpl_sidno sidno = 0;

  if (sid != NULL) {
    /*
     This transaction has a UUID different from the group name,
     thence we need to fetch the corresponding sidno from the
     global sid_map.
    */
    sidno = get_sidno_from_global_sid_map(*sid);
    if (sidno <= 0) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FAILED_TO_GENERATE_SIDNO_FOR_GRP);
      return 1;
      /* purecov: end */
    }
  } else {
    /*
      This transaction has the group name as UUID, so we can skip
      a lock on global sid_map and use the cached group sidno.
    */
    sidno = get_group_sidno();
  }
  Transaction_consistency_manager_key key(sidno, gno);

  m_map_lock->rdlock();
  typename Transaction_consistency_manager_map::iterator it = m_map.find(key);
  if (it == m_map.end()) {
    /*
      If this member is or just was in RECOVERING state, it may have applied
      consistent transactions through recovery channel, so before throw a
      error on a unknown prepare acknowledge message, first we check if the
      transaction is already committed on this member.
      This happens because the consistent transaction was executed while this
      member was in RECOVERING state, so the transaction was not being tracked.
    */
    Gtid gtid = {sidno, gno};
    if (is_gtid_committed(gtid)) {
      m_map_lock->unlock();
      return 0;
    }

    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_TRX_DOES_NOT_EXIST_ON_TCM_ON_HANDLE_REMOTE_PREPARE,
                 sidno, gno);
    m_map_lock->unlock();
    return 1;
    /* purecov: end */
  }

  Transaction_consistency_info *transaction_info = it->second;

  DBUG_PRINT(
      "info",
      ("gtid: %d:%llu; consistency_level: %d; ", transaction_info->get_sidno(),
       transaction_info->get_gno(), transaction_info->get_consistency_level()));

  int result = transaction_info->handle_remote_prepare(gcs_member_id);
  if (CONSISTENCY_INFO_OUTCOME_ERROR == result) {
    /* purecov: begin inspected */
    m_map_lock->unlock();
    return 1;
    /* purecov: end */
  }
  m_map_lock->unlock();

  // Remove transaction from map
  if (CONSISTENCY_INFO_OUTCOME_COMMIT == result) {
    m_map_lock->wrlock();
    typename Transaction_consistency_manager_map::iterator it = m_map.find(key);
    if (it != m_map.end()) {
      delete it->second;
      m_map.erase(it);
    }
    m_map_lock->unlock();
  }

  return 0;
}

int Transaction_consistency_manager::handle_member_leave(
    const std::vector<Gcs_member_identifier> &leaving_members) {
  DBUG_TRACE;

  m_map_lock->wrlock();
  if (m_map.empty()) {
    m_map_lock->unlock();
    return 0;
  }

  typename Transaction_consistency_manager_map::iterator it = m_map.begin();
  while (it != m_map.end()) {
    Transaction_consistency_info *transaction_info = it->second;
    int result = transaction_info->handle_member_leave(leaving_members);
    if (CONSISTENCY_INFO_OUTCOME_COMMIT == result) {
      delete it->second;
      m_map.erase(it++);
    } else {
      ++it;
    }
  }

  m_map_lock->unlock();
  return 0;
}

int Transaction_consistency_manager::after_commit(my_thread_id, rpl_sidno sidno,
                                                  rpl_gno gno) {
  DBUG_TRACE;
  DBUG_PRINT("info", ("gtid: %d:%llu", sidno, gno));
  int error = 0;

  // Only acquire a write lock if really needed.
  m_prepared_transactions_on_my_applier_lock->rdlock();
  bool empty = m_prepared_transactions_on_my_applier.empty();
  m_prepared_transactions_on_my_applier_lock->unlock();

  if (!empty) {
    Transaction_consistency_manager_key key(sidno, gno);
    error = remove_prepared_transaction(key);
  }

  return error;
}

int Transaction_consistency_manager::before_transaction_begin(
    my_thread_id thread_id, ulong gr_consistency_level, ulong timeout,
    enum_rpl_channel_type rpl_channel_type) {
  DBUG_TRACE;
  int error = 0;

  if (GR_RECOVERY_CHANNEL == rpl_channel_type ||
      GR_APPLIER_CHANNEL == rpl_channel_type) {
    return 0;
  }

  const enum_group_replication_consistency_level consistency_level =
      static_cast<enum_group_replication_consistency_level>(
          gr_consistency_level);

  // Transaction consistency can only be used on a ONLINE member.
  if (consistency_level >= GROUP_REPLICATION_CONSISTENCY_BEFORE &&
      local_member_info->get_recovery_status() !=
          Group_member_info::MEMBER_ONLINE) {
    return ER_GRP_TRX_CONSISTENCY_NOT_ALLOWED;
  }

  DBUG_PRINT("info", ("thread_id: %d; consistency_level: %d", thread_id,
                      consistency_level));

  if (GROUP_REPLICATION_CONSISTENCY_BEFORE == consistency_level ||
      GROUP_REPLICATION_CONSISTENCY_BEFORE_AND_AFTER == consistency_level) {
    error = transaction_begin_sync_before_execution(thread_id,
                                                    consistency_level, timeout);
    if (error) {
      /* purecov: begin inspected */
      return error;
      /* purecov: end */
    }
  }

  error = transaction_begin_sync_prepared_transactions(thread_id, timeout);
  if (error) {
    /* purecov: begin inspected */
    return error;
    /* purecov: end */
  }

  return 0;
}

int Transaction_consistency_manager::transaction_begin_sync_before_execution(
    my_thread_id thread_id,
    enum_group_replication_consistency_level consistency_level
        MY_ATTRIBUTE((unused)),
    ulong timeout) const {
  DBUG_TRACE;
  DBUG_ASSERT(GROUP_REPLICATION_CONSISTENCY_BEFORE == consistency_level ||
              GROUP_REPLICATION_CONSISTENCY_BEFORE_AND_AFTER ==
                  consistency_level);
  DBUG_PRINT("info", ("thread_id: %d; consistency_level: %d", thread_id,
                      consistency_level));

  if (m_plugin_stopping) {
    return ER_GRP_TRX_CONSISTENCY_BEGIN_NOT_ALLOWED;
  }

  if (transactions_latch->registerTicket(thread_id)) {
    /* purecov: begin inspected */
    LogPluginErr(
        ERROR_LEVEL,
        ER_GRP_RPL_REGISTER_TRX_TO_WAIT_FOR_SYNC_BEFORE_EXECUTION_FAILED,
        thread_id);
    return ER_GRP_TRX_CONSISTENCY_BEFORE;
    /* purecov: end */
  }

  // send message
  Sync_before_execution_message message(thread_id);
  if (gcs_module->send_message(message)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_SEND_TRX_SYNC_BEFORE_EXECUTION_FAILED,
                 thread_id);
    return ER_GRP_TRX_CONSISTENCY_BEFORE;
    /* purecov: end */
  }

  DBUG_PRINT("info", ("waiting for Sync_before_execution_message"));

  if (transactions_latch->waitTicket(thread_id)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_TRX_WAIT_FOR_SYNC_BEFORE_EXECUTION_FAILED,
                 thread_id);
    return ER_GRP_TRX_CONSISTENCY_BEFORE;
    /* purecov: end */
  }

  DBUG_PRINT("info", ("waiting for channel_wait_until_apply_queue_applied()"));

  if (channel_wait_until_apply_queue_applied("group_replication_applier",
                                             timeout) < 0) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_TRX_WAIT_FOR_GROUP_GTID_EXECUTED,
                 thread_id);
    return ER_GRP_TRX_CONSISTENCY_BEFORE;
    /* purecov: end */
  }

  return 0;
}

int Transaction_consistency_manager::handle_sync_before_execution_message(
    my_thread_id thread_id, const Gcs_member_identifier &gcs_member_id) const {
  DBUG_TRACE;
  DBUG_PRINT("info", ("thread_id: %d; gcs_member_id: %s", thread_id,
                      gcs_member_id.get_member_id().c_str()));
  if (local_member_info->get_gcs_member_id() == gcs_member_id &&
      transactions_latch->releaseTicket(thread_id)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_RELEASE_BEGIN_TRX_AFTER_WAIT_FOR_SYNC_BEFORE_EXEC,
                 thread_id);
    return 1;
    /* purecov: end */
  }

  return 0;
}

int Transaction_consistency_manager::
    transaction_begin_sync_prepared_transactions(my_thread_id thread_id,
                                                 ulong timeout) {
  DBUG_TRACE;
  Transaction_consistency_manager_key key(0, 0);

  // Take a read lock to check queue size.
  m_prepared_transactions_on_my_applier_lock->rdlock();
  bool empty = m_prepared_transactions_on_my_applier.empty();
  m_prepared_transactions_on_my_applier_lock->unlock();
  if (empty) {
    return 0;
  }

  m_prepared_transactions_on_my_applier_lock->wrlock();
  if (m_prepared_transactions_on_my_applier.empty()) {
    /* purecov: begin inspected */
    m_prepared_transactions_on_my_applier_lock->unlock();
    return 0;
    /* purecov: end */
  }

  if (m_plugin_stopping) {
    m_prepared_transactions_on_my_applier_lock->unlock();
    return ER_GRP_TRX_CONSISTENCY_BEGIN_NOT_ALLOWED;
  }

  DBUG_PRINT("info", ("thread_id: %d", thread_id));

  if (transactions_latch->registerTicket(thread_id)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_REGISTER_TRX_TO_WAIT_FOR_DEPENDENCIES_FAILED,
                 thread_id);
    m_prepared_transactions_on_my_applier_lock->unlock();
    return ER_GRP_TRX_CONSISTENCY_AFTER_ON_TRX_BEGIN;
    /* purecov: end */
  }

  // queue a transaction 0,0 to mark a begin
  m_prepared_transactions_on_my_applier.push_back(key);
  // queue this thread_id so that we can wake it up
  m_new_transactions_waiting.push_back(thread_id);

  m_prepared_transactions_on_my_applier_lock->unlock();

  DBUG_PRINT("info", ("waiting for prepared transactions"));

  if (transactions_latch->waitTicket(thread_id, timeout)) {
    /* purecov: begin inspected */
    remove_prepared_transaction(key);
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_WAIT_FOR_DEPENDENCIES_FAILED,
                 thread_id);
    return ER_GRP_TRX_CONSISTENCY_AFTER_ON_TRX_BEGIN;
    /* purecov: end */
  }

  return 0;
}

bool Transaction_consistency_manager::has_local_prepared_transactions() {
  DBUG_TRACE;
  bool result = false;
  m_map_lock->rdlock();

  for (typename Transaction_consistency_manager_map::iterator it =
           m_map.begin();
       it != m_map.end(); it++) {
    Transaction_consistency_info *transaction_info = it->second;

    if (transaction_info->is_local_transaction() &&
        transaction_info->is_transaction_prepared_locally()) {
      result = true;
      break;
    }
  }

  m_map_lock->unlock();
  return result;
}

int Transaction_consistency_manager::schedule_view_change_event(
    Pipeline_event *pevent) {
  DBUG_TRACE;
  Transaction_consistency_manager_key key(-1, -1);

  m_prepared_transactions_on_my_applier_lock->wrlock();
  m_prepared_transactions_on_my_applier.push_back(key);
  m_delayed_view_change_events.push_back(pevent);
  m_prepared_transactions_on_my_applier_lock->unlock();

  return 0;
}

int Transaction_consistency_manager::remove_prepared_transaction(
    Transaction_consistency_manager_key key) {
  DBUG_TRACE;
  int error = 0;

  DBUG_PRINT("info", ("gtid: %d:%llu", key.first, key.second));

  m_prepared_transactions_on_my_applier_lock->wrlock();
  if (key.first > 0 && key.second > 0) {
    m_prepared_transactions_on_my_applier.remove(key);
  }

  while (!m_prepared_transactions_on_my_applier.empty()) {
    Transaction_consistency_manager_key next_prepared =
        m_prepared_transactions_on_my_applier.front();

    if (0 == next_prepared.first && 0 == next_prepared.second) {
      DBUG_ASSERT(!m_new_transactions_waiting.empty());
      // This is a new transaction waiting, lets wake it up.
      m_prepared_transactions_on_my_applier.pop_front();
      my_thread_id waiting_thread_id = m_new_transactions_waiting.front();
      m_new_transactions_waiting.pop_front();

      DBUG_PRINT("info",
                 ("release transaction begin of thread %d", waiting_thread_id));

      if (transactions_latch->releaseTicket(waiting_thread_id)) {
        /* purecov: begin inspected */
        LogPluginErr(
            ERROR_LEVEL,
            ER_GRP_RPL_RELEASE_BEGIN_TRX_AFTER_DEPENDENCIES_COMMIT_FAILED,
            key.first, key.second, waiting_thread_id);
        error = 1;
        /* purecov: end */
      }
    } else if (-1 == next_prepared.first && -1 == next_prepared.second) {
      DBUG_ASSERT(!m_delayed_view_change_events.empty());
      /*
        This is a view change that was waiting for prepared
        transactions completion, now it is time to log it.
      */
      m_prepared_transactions_on_my_applier.pop_front();
      Pipeline_event *pevent = m_delayed_view_change_events.front();
      m_delayed_view_change_events.pop_front();

      Continuation cont;
      int error = applier_module->inject_event_into_pipeline(pevent, &cont);
      delete pevent;
      if (error) {
        abort_plugin_process("unable to log the View_change_log_event");
      }
    } else {
      break;
    }
  }

  m_prepared_transactions_on_my_applier_lock->unlock();

  return error;
}

void Transaction_consistency_manager::plugin_started() {
  m_plugin_stopping = false;
}

void Transaction_consistency_manager::plugin_is_stopping() {
  m_plugin_stopping = true;
}

void Transaction_consistency_manager::register_transaction_observer() {
  group_transaction_observation_manager->register_transaction_observer(this);
}

void Transaction_consistency_manager::unregister_transaction_observer() {
  group_transaction_observation_manager->unregister_transaction_observer(this);
}

/*
  These methods are necessary to fulfil the Group_transaction_listener
  interface.
*/
/* purecov: begin inspected */
int Transaction_consistency_manager::before_commit(
    my_thread_id, Group_transaction_listener::enum_transaction_origin) {
  return 0;
}

int Transaction_consistency_manager::before_rollback(
    my_thread_id, Group_transaction_listener::enum_transaction_origin) {
  return 0;
}

int Transaction_consistency_manager::after_rollback(my_thread_id) { return 0; }
/* purecov: end */
