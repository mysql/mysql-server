/* Copyright (c) 2013, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <assert.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/service_rpl_transaction_ctx.h>
#include <mysql/service_rpl_transaction_write_set.h>
#include <stddef.h>
#include <string>
#include <vector>

#include "base64.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "plugin/group_replication/include/observer_trans.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/sql_service/sql_command_test.h"
#include "plugin/group_replication/include/sql_service/sql_service_command.h"
#include "plugin/group_replication/include/sql_service/sql_service_interface.h"

/*
  Buffer to read the write_set value as a string.
  Since we support up to 64 bits hashes, 8 bytes are enough to store the info.
*/
#define BUFFER_READ_PKE 8

/*
  Map to store all open unused IO_CACHE.
  Each ongoing transaction will have a busy cache, when the cache
  is no more needed, it is added to this list for future use by
  another transaction.
*/
typedef std::list<IO_CACHE *> IO_CACHE_unused_list;
static IO_CACHE_unused_list io_cache_unused_list;

/*
  Read/write lock to protect map find operations against new cache inserts.
*/
static Checkable_rwlock *io_cache_unused_list_lock = NULL;

void observer_trans_initialize() {
  DBUG_ENTER("observer_trans_initialize");

  io_cache_unused_list_lock = new Checkable_rwlock(
#ifdef HAVE_PSI_INTERFACE
      key_GR_RWLOCK_io_cache_unused_list
#endif /* HAVE_PSI_INTERFACE */
  );

  DBUG_VOID_RETURN;
}

void observer_trans_terminate() {
  DBUG_ENTER("observer_trans_terminate");

  delete io_cache_unused_list_lock;
  io_cache_unused_list_lock = NULL;

  DBUG_VOID_RETURN;
}

void observer_trans_clear_io_cache_unused_list() {
  DBUG_ENTER("observer_trans_clear_io_cache_unused_list");
  io_cache_unused_list_lock->wrlock();

  for (IO_CACHE_unused_list::iterator it = io_cache_unused_list.begin();
       it != io_cache_unused_list.end(); ++it) {
    IO_CACHE *cache = *it;
    close_cached_file(cache);
    my_free(cache);
  }

  io_cache_unused_list.clear();

  io_cache_unused_list_lock->unlock();
  DBUG_VOID_RETURN;
}

/*
  Internal auxiliary functions signatures.
*/
static bool reinit_cache(IO_CACHE *cache, enum cache_type type,
                         my_off_t position);

IO_CACHE *observer_trans_get_io_cache(my_thread_id thread_id,
                                      ulonglong cache_size);

void observer_trans_put_io_cache(IO_CACHE *cache);

void cleanup_transaction_write_set(
    Transaction_write_set *transaction_write_set) {
  DBUG_ENTER("cleanup_transaction_write_set");
  if (transaction_write_set != NULL) {
    my_free(transaction_write_set->write_set);
    my_free(transaction_write_set);
  }
  DBUG_VOID_RETURN;
}

int add_write_set(Transaction_context_log_event *tcle,
                  Transaction_write_set *set) {
  DBUG_ENTER("add_write_set");
  int iterator = set->write_set_size;
  for (int i = 0; i < iterator; i++) {
    uchar buff[BUFFER_READ_PKE];
    int8store(buff, set->write_set[i]);
    uint64 const tmp_str_sz =
        base64_needed_encoded_length((uint64)BUFFER_READ_PKE);
    char *write_set_value =
        (char *)my_malloc(PSI_NOT_INSTRUMENTED, tmp_str_sz, MYF(MY_WME));
    if (!write_set_value) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_OOM_FAILED_TO_GENERATE_IDENTIFICATION_HASH);
      DBUG_RETURN(1);
      /* purecov: end */
    }

    if (base64_encode(buff, (size_t)BUFFER_READ_PKE, write_set_value)) {
      /* purecov: begin inspected */
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_WRITE_IDENT_HASH_BASE64_ENCODING_FAILED);
      DBUG_RETURN(1);
      /* purecov: end */
    }

    tcle->add_write_set(write_set_value);
  }
  DBUG_RETURN(0);
}

/*
  Transaction lifecycle events observers.
*/

int group_replication_trans_before_dml(Trans_param *param, int &out) {
  DBUG_ENTER("group_replication_trans_before_dml");

  out = 0;

  // If group replication has not started, then moving along...
  if (!plugin_is_group_replication_running()) {
    DBUG_RETURN(0);
  }

  /*
   The first check to be made is if the session binlog is active
   If it is not active, this query is not relevant for the plugin.
   */
  if (!param->trans_ctx_info.binlog_enabled) {
    DBUG_RETURN(0);
  }

  /*
   In runtime, check the global variables that can change.
   */
  if ((out += (param->trans_ctx_info.binlog_format != BINLOG_FORMAT_ROW))) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_INVALID_BINLOG_FORMAT);
    DBUG_RETURN(0);
  }

  if ((out += (param->trans_ctx_info.binlog_checksum_options !=
               binary_log::BINLOG_CHECKSUM_ALG_OFF))) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_BINLOG_CHECKSUM_SET);
    DBUG_RETURN(0);
  }

  if ((out += (param->trans_ctx_info.transaction_write_set_extraction ==
               HASH_ALGORITHM_OFF))) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_TRANS_WRITE_SET_EXTRACTION_NOT_SET);
    DBUG_RETURN(0);
    /* purecov: end */
  }

  if (local_member_info->has_enforces_update_everywhere_checks() &&
      (out += (param->trans_ctx_info.tx_isolation == ISO_SERIALIZABLE))) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_UNSUPPORTED_TRANS_ISOLATION);
    DBUG_RETURN(0);
  }
  /*
    Cycle through all involved tables to assess if they all
    comply with the plugin runtime requirements. For now:
    - The table must be from a transactional engine
    - It must contain at least one primary key
    - It should not contain 'ON DELETE/UPDATE CASCADE' referential action
   */
  for (uint table = 0; out == 0 && table < param->number_of_tables; table++) {
    if (param->tables_info[table].db_type != DB_TYPE_INNODB) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_NEEDS_INNODB_TABLE,
                   param->tables_info[table].table_name);
      out++;
    }

    if (param->tables_info[table].number_of_primary_keys == 0) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_PRIMARY_KEY_NOT_DEFINED,
                   param->tables_info[table].table_name);
      out++;
    }
    if (local_member_info->has_enforces_update_everywhere_checks() &&
        param->tables_info[table].has_cascade_foreign_key) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FK_WITH_CASCADE_UNSUPPORTED,
                   param->tables_info[table].table_name);
      out++;
    }
  }

  DBUG_RETURN(0);
}

int group_replication_trans_before_commit(Trans_param *param) {
  DBUG_ENTER("group_replication_trans_before_commit");
  int error = 0;
  const int pre_wait_error = 1;
  const int post_wait_error = 2;

  DBUG_EXECUTE_IF("group_replication_force_error_on_before_commit_listener",
                  DBUG_RETURN(1););

  DBUG_EXECUTE_IF("group_replication_before_commit_hook_wait", {
    const char act[] = "now wait_for continue_commit";
    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });

  /*
    If the originating id belongs to a thread in the plugin, the transaction
    was already certified. Channel operations can deadlock against
    plugin/applier thread stops so they must remain outside the plugin stop
    lock below.
  */
  Replication_thread_api channel_interface;
  if (channel_interface.is_own_event_applier(param->thread_id,
                                             "group_replication_applier")) {
    // If plugin is stopping, there is no point in update the statistics.
    bool fail_to_lock = shared_plugin_stop_lock->try_grab_read_lock();
    if (!fail_to_lock) {
      if (local_member_info->get_recovery_status() ==
          Group_member_info::MEMBER_ONLINE) {
        applier_module->get_pipeline_stats_member_collector()
            ->decrement_transactions_waiting_apply();
        applier_module->get_pipeline_stats_member_collector()
            ->increment_transactions_applied();
      } else if (local_member_info->get_recovery_status() ==
                 Group_member_info::MEMBER_IN_RECOVERY) {
        applier_module->get_pipeline_stats_member_collector()
            ->increment_transactions_applied_during_recovery();
      }
      shared_plugin_stop_lock->release_read_lock();
    }

    DBUG_RETURN(0);
  }
  if (channel_interface.is_own_event_applier(param->thread_id,
                                             "group_replication_recovery")) {
    DBUG_RETURN(0);
  }

  shared_plugin_stop_lock->grab_read_lock();

  if (is_plugin_waiting_to_set_server_read_mode()) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CANNOT_EXECUTE_TRANS_WHILE_STOPPING);
    shared_plugin_stop_lock->release_read_lock();
    DBUG_RETURN(1);
  }

  /* If the plugin is not running, before commit should return success. */
  if (!plugin_is_group_replication_running()) {
    shared_plugin_stop_lock->release_read_lock();
    DBUG_RETURN(0);
  }

  DBUG_ASSERT(applier_module != NULL && recovery_module != NULL);
  Group_member_info::Group_member_status member_status =
      local_member_info->get_recovery_status();

  if (member_status == Group_member_info::MEMBER_IN_RECOVERY) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CANNOT_EXECUTE_TRANS_WHILE_RECOVERING);
    shared_plugin_stop_lock->release_read_lock();
    DBUG_RETURN(1);
    /* purecov: end */
  }

  if (member_status == Group_member_info::MEMBER_ERROR) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CANNOT_EXECUTE_TRANS_IN_ERROR_STATE);
    shared_plugin_stop_lock->release_read_lock();
    DBUG_RETURN(1);
  }

  if (member_status == Group_member_info::MEMBER_OFFLINE) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_CANNOT_EXECUTE_TRANS_IN_OFFLINE_MODE);
    shared_plugin_stop_lock->release_read_lock();
    DBUG_RETURN(1);
    /* purecov: end */
  }

  // Transaction information.
  const ulong transaction_size_limit = get_transaction_size_limit();
  my_off_t transaction_size = 0;

  const bool is_gtid_specified = param->gtid_info.type == ASSIGNED_GTID;
  Gtid gtid = {param->gtid_info.sidno, param->gtid_info.gno};
  if (!is_gtid_specified) {
    // Dummy values that will be replaced after certification.
    gtid.sidno = 1;
    gtid.gno = 1;
  }

  const Gtid_specification gtid_specification = {ASSIGNED_GTID, gtid};
  Gtid_log_event *gle = NULL;

  Transaction_context_log_event *tcle = NULL;

  // group replication cache.
  IO_CACHE *cache = NULL;

  // Todo optimize for memory (IO-cache's buf to start with, if not enough then
  // trans mem-root) to avoid New message create/delete and/or its implicit
  // MessageBuffer.
  Transaction_Message transaction_msg;

  enum enum_gcs_error send_error = GCS_OK;

  // Binlog cache.
  /*
    Atomic DDL:s are logged through the transactional cache so they should
    be exempted from considering as DML by the plugin: not
    everthing that is in the trans cache is actually DML.
  */
  bool is_dml = !param->is_atomic_ddl;
  bool may_have_sbr_stmts = !is_dml;
  IO_CACHE *cache_log = NULL;
  my_off_t cache_log_position = 0;
  bool reinit_cache_log_required = false;
  const my_off_t trx_cache_log_position = my_b_tell(param->trx_cache_log);
  const my_off_t stmt_cache_log_position = my_b_tell(param->stmt_cache_log);

  if (trx_cache_log_position > 0 && stmt_cache_log_position == 0) {
    cache_log = param->trx_cache_log;
    cache_log_position = trx_cache_log_position;
  } else if (trx_cache_log_position == 0 && stmt_cache_log_position > 0) {
    cache_log = param->stmt_cache_log;
    cache_log_position = stmt_cache_log_position;
    is_dml = false;
    may_have_sbr_stmts = true;
  } else {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_MULTIPLE_CACHE_TYPE_NOT_SUPPORTED_FOR_SESSION,
                 param->thread_id);
    shared_plugin_stop_lock->release_read_lock();
    DBUG_RETURN(1);
    /* purecov: end */
  }

  applier_module->get_pipeline_stats_member_collector()
      ->increment_transactions_local();

  DBUG_ASSERT(cache_log->type == WRITE_CACHE);
  DBUG_PRINT("cache_log", ("thread_id: %u, trx_cache_log_position: %llu,"
                           " stmt_cache_log_position: %llu",
                           param->thread_id, trx_cache_log_position,
                           stmt_cache_log_position));

  /*
    Open group replication cache.
    Reuse the same cache on each session for improved performance.
  */
  cache =
      observer_trans_get_io_cache(param->thread_id, param->cache_log_max_size);
  if (cache == NULL) {
    /* purecov: begin inspected */
    error = pre_wait_error;
    goto err;
    /* purecov: end */
  }

  // Reinit binlog cache to read.
  if (reinit_cache(cache_log, READ_CACHE, 0)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FAILED_TO_REINIT_BINLOG_CACHE_FOR_READ,
                 param->thread_id);
    error = pre_wait_error;
    goto err;
    /* purecov: end */
  }

  /*
    After this, cache_log should be reinit to old saved value when we
    are going out of the function scope.
  */
  reinit_cache_log_required = true;

  // Create transaction context.
  tcle = new Transaction_context_log_event(param->server_uuid,
                                           is_dml || param->is_atomic_ddl,
                                           param->thread_id, is_gtid_specified);
  if (!tcle->is_valid()) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FAILED_TO_CREATE_TRANS_CONTEXT,
                 param->thread_id);
    error = pre_wait_error;
    goto err;
    /* purecov: end */
  }

  if (is_dml) {
    Transaction_write_set *write_set =
        get_transaction_write_set(param->thread_id);
    /*
      When GTID is specified we may have empty transactions, that is,
      a transaction may have not write set at all because it didn't
      change any data, it will just persist that GTID as applied.
    */
    if ((write_set == NULL) && (!is_gtid_specified)) {
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FAILED_TO_EXTRACT_TRANS_WRITE_SET,
                   param->thread_id);
      error = pre_wait_error;
      goto err;
    }

    if (write_set != NULL) {
      if (add_write_set(tcle, write_set)) {
        /* purecov: begin inspected */
        cleanup_transaction_write_set(write_set);
        LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FAILED_TO_GATHER_TRANS_WRITE_SET,
                     param->thread_id);
        error = pre_wait_error;
        goto err;
        /* purecov: end */
      }
      cleanup_transaction_write_set(write_set);
      DBUG_ASSERT(is_gtid_specified || (tcle->get_write_set()->size() > 0));
    } else {
      /*
        For empty transactions we should set the GTID may_have_sbr_stmts. See
        comment at binlog_cache_data::may_have_sbr_stmts().
      */
      may_have_sbr_stmts = true;
    }
  }

  // Write transaction context to group replication cache.
  tcle->write(cache);

  if (*(param->original_commit_timestamp) == UNDEFINED_COMMIT_TIMESTAMP) {
    /*
     Assume that this transaction is original from this server and update status
     variable so that it won't be re-defined when this GTID is written to the
     binlog
    */
    *(param->original_commit_timestamp) = my_micro_time();
  }  // otherwise the transaction did not originate in this server

  // Notice the GTID of atomic DDL is written to the trans cache as well.
  gle = new Gtid_log_event(param->server_id, is_dml || param->is_atomic_ddl, 0,
                           1, may_have_sbr_stmts,
                           *(param->original_commit_timestamp), 0,
                           gtid_specification);
  /*
    GR does not support event checksumming. If GR start to support event
    checksumming, the calculation below should take the checksum payload into
    account.
  */
  gle->set_trx_length_by_cache_size(cache_log_position);
  gle->write(cache);

  transaction_size = cache_log_position + my_b_tell(cache);
  if (is_dml && transaction_size_limit &&
      transaction_size > transaction_size_limit) {
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_TRANS_SIZE_EXCEEDS_LIMIT,
                 param->thread_id, transaction_size, transaction_size_limit);
    error = pre_wait_error;
    goto err;
  }

  // Reinit group replication cache to read.
  if (reinit_cache(cache, READ_CACHE, 0)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_REINIT_OF_INTERNAL_CACHE_FOR_READ_FAILED,
                 param->thread_id);
    error = pre_wait_error;
    goto err;
    /* purecov: end */
  }

  // Copy group replication cache to buffer.
  if (transaction_msg.append_cache(cache)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_APPENDING_DATA_TO_INTERNAL_CACHE_FAILED,
                 param->thread_id);
    error = pre_wait_error;
    goto err;
    /* purecov: end */
  }

  // Copy binlog cache content to buffer.
  if (transaction_msg.append_cache(cache_log)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_WRITE_TO_BINLOG_CACHE_FAILED,
                 param->thread_id);
    error = pre_wait_error;
    goto err;
    /* purecov: end */
  }

  DBUG_ASSERT(certification_latch != NULL);
  if (certification_latch->registerTicket(param->thread_id)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_FAILED_TO_REGISTER_TRANS_OUTCOME_NOTIFICTION,
                 param->thread_id);
    error = pre_wait_error;
    goto err;
    /* purecov: end */
  }

#ifndef DBUG_OFF
  DBUG_EXECUTE_IF("test_basic_CRUD_operations_sql_service_interface", {
    DBUG_SET("-d,test_basic_CRUD_operations_sql_service_interface");
    DBUG_ASSERT(!sql_command_check());
  };);

  DBUG_EXECUTE_IF("group_replication_before_message_broadcast", {
    const char act[] = "now wait_for waiting";
    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });
#endif

  /*
    Check if member needs to throttle its transactions to avoid
    cause starvation on the group.
  */
  applier_module->get_flow_control_module()->do_wait();

  // Broadcast the Transaction Message
  send_error = gcs_module->send_message(transaction_msg);
  if (send_error == GCS_MESSAGE_TOO_BIG) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_MSG_TOO_LONG_BROADCASTING_TRANS_FAILED,
                 param->thread_id);
    error = pre_wait_error;
    goto err;
    /* purecov: end */
  } else if (send_error == GCS_NOK) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_BROADCASTING_TRANS_TO_GRP_FAILED,
                 param->thread_id);
    error = pre_wait_error;
    goto err;
    /* purecov: end */
  }

  shared_plugin_stop_lock->release_read_lock();

  DBUG_ASSERT(certification_latch != NULL);
  if (certification_latch->waitTicket(param->thread_id)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_ERROR_WHILE_WAITING_FOR_CONFLICT_DETECTION,
                 param->thread_id);
    error = post_wait_error;
    goto err;
    /* purecov: end */
  }

err:
  // Reinit binlog cache to write (revert what we did).
  if (reinit_cache_log_required &&
      reinit_cache(cache_log, WRITE_CACHE, cache_log_position)) {
    /* purecov: begin inspected */
    LogPluginErr(ERROR_LEVEL,
                 ER_GRP_RPL_REINIT_OF_INTERNAL_CACHE_FOR_WRITE_FAILED,
                 param->thread_id);
    /* purecov: end */
  }
  observer_trans_put_io_cache(cache);
  delete gle;
  delete tcle;

  if (error) {
    if (error == pre_wait_error) shared_plugin_stop_lock->release_read_lock();

    DBUG_ASSERT(certification_latch != NULL);
    // Release and remove certification latch ticket.
    certification_latch->releaseTicket(param->thread_id);
    certification_latch->waitTicket(param->thread_id);
  }

  DBUG_EXECUTE_IF("group_replication_after_before_commit_hook", {
    const char act[] = "now wait_for signal.commit_continue";
    DBUG_ASSERT(!debug_sync_set_action(current_thd, STRING_WITH_LEN(act)));
  });
  DBUG_RETURN(error);
}

int group_replication_trans_before_rollback(Trans_param *) {
  DBUG_ENTER("group_replication_trans_before_rollback");
  DBUG_RETURN(0);
}

int group_replication_trans_after_commit(Trans_param *) {
  DBUG_ENTER("group_replication_trans_after_commit");
  DBUG_RETURN(0);
}

int group_replication_trans_after_rollback(Trans_param *) {
  DBUG_ENTER("group_replication_trans_after_rollback");
  DBUG_RETURN(0);
}

Trans_observer trans_observer = {
    sizeof(Trans_observer),

    group_replication_trans_before_dml,
    group_replication_trans_before_commit,
    group_replication_trans_before_rollback,
    group_replication_trans_after_commit,
    group_replication_trans_after_rollback,
};

/*
  Internal auxiliary functions.
*/

/*
  Reinit IO_cache type.

  @param[in] cache     cache
  @param[in] type      type to which cache will change
  @param[in] position  position to which cache will seek
*/
static bool reinit_cache(IO_CACHE *cache, enum cache_type type,
                         my_off_t position) {
  DBUG_ENTER("reinit_cache");

  /*
    Avoid call flush_io_cache() before reinit_io_cache() if
    temporary file does not exist.
    Call flush_io_cache() forces the creation of the cache
    temporary file, even when it does not exist.
  */
  if (READ_CACHE == type && cache->file != -1 && flush_io_cache(cache))
    DBUG_RETURN(true); /* purecov: inspected */

  if (reinit_io_cache(cache, type, position, 0, 0))
    DBUG_RETURN(true); /* purecov: inspected */

  DBUG_RETURN(false);
}

/*
  Get already initialized cache or create a new cache for
  this session.

  @param[in] thread_id   the session
  @param[in] cache_size  the cache size

  @return The cache or NULL on error
*/
IO_CACHE *observer_trans_get_io_cache(my_thread_id thread_id,
                                      ulonglong cache_size) {
  DBUG_ENTER("observer_trans_get_io_cache");
  IO_CACHE *cache = NULL;

  io_cache_unused_list_lock->wrlock();
  if (io_cache_unused_list.empty()) {
    io_cache_unused_list_lock->unlock();
    // Open IO_CACHE file
    cache = (IO_CACHE *)my_malloc(PSI_NOT_INSTRUMENTED, sizeof(IO_CACHE),
                                  MYF(MY_ZEROFILL));
    if (!cache || (!my_b_inited(cache) &&
                   open_cached_file(cache, mysql_tmpdir,
                                    "group_replication_trans_before_commit",
                                    cache_size, MYF(MY_WME)))) {
      /* purecov: begin inspected */
      my_free(cache);
      cache = NULL;
      LogPluginErr(ERROR_LEVEL, ER_GRP_RPL_FAILED_TO_CREATE_COMMIT_CACHE,
                   thread_id);
      goto end;
      /* purecov: end */
    }
  } else {
    // Reuse cache created previously.
    cache = io_cache_unused_list.front();
    io_cache_unused_list.pop_front();
    io_cache_unused_list_lock->unlock();

    if (reinit_cache(cache, WRITE_CACHE, 0)) {
      /* purecov: begin inspected */
      close_cached_file(cache);
      my_free(cache);
      cache = NULL;
      LogPluginErr(ERROR_LEVEL,
                   ER_GRP_RPL_REINIT_OF_COMMIT_CACHE_FOR_WRITE_FAILED,
                   thread_id);
      goto end;
      /* purecov: end */
    }
  }

end:
  DBUG_RETURN(cache);
}

/*
  Save already initialized cache for a future session.

  @param[in] cache       the cache
*/
void observer_trans_put_io_cache(IO_CACHE *cache) {
  DBUG_ENTER("observer_trans_put_io_cache");

  io_cache_unused_list_lock->wrlock();
  io_cache_unused_list.push_back(cache);
  io_cache_unused_list_lock->unlock();

  DBUG_VOID_RETURN;
}

// Transaction Message implementation

Transaction_Message::Transaction_Message()
    : Plugin_gcs_message(CT_TRANSACTION_MESSAGE) {}

Transaction_Message::~Transaction_Message() {}

bool Transaction_Message::append_cache(IO_CACHE *src) {
  DBUG_ENTER("append_cache");
  DBUG_ASSERT(src->type == READ_CACHE);

  uchar *buffer = src->read_pos;
  size_t length = my_b_fill(src);
  if (src->file == -1) {
    // Read cache size directly when temporary file does not exist.
    length = my_b_bytes_in_cache(src);
  }

  while (length > 0 && !src->error) {
    data.insert(data.end(), buffer, buffer + length);

    src->read_pos = src->read_end;
    length = my_b_fill(src);
    buffer = src->read_pos;
  }

  DBUG_RETURN(src->error ? true : false);
}

void Transaction_Message::encode_payload(
    std::vector<unsigned char> *buffer) const {
  DBUG_ENTER("Transaction_Message::encode_payload");

  encode_payload_item_type_and_length(buffer, PIT_TRANSACTION_DATA,
                                      data.size());
  buffer->insert(buffer->end(), data.begin(), data.end());

  DBUG_VOID_RETURN;
}

void Transaction_Message::decode_payload(const unsigned char *buffer,
                                         const unsigned char *) {
  DBUG_ENTER("Transaction_Message::decode_payload");
  const unsigned char *slider = buffer;
  uint16 payload_item_type = 0;
  unsigned long long payload_item_length = 0;

  decode_payload_item_type_and_length(&slider, &payload_item_type,
                                      &payload_item_length);
  data.clear();
  data.insert(data.end(), slider, slider + payload_item_length);

  DBUG_VOID_RETURN;
}
