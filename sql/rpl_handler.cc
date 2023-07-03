/* Copyright (c) 2008, 2022, Oracle and/or its affiliates.

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

#include "sql/rpl_handler.h"

#include <string.h>
#include <memory>
#include <new>
#include <unordered_map>
#include <utility>
#include <vector>

#include "lex_string.h"
#include "map_helpers.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_io.h"
#include "my_loglevel.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/components/services/log_shared.h"
#include "mysql/plugin.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/service_mysql_alloc.h"
#include "mysqld_error.h"
#include "prealloced_array.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"  // DEBUG_SYNC
#include "sql/handler.h"
#include "sql/item_func.h"  // user_var_entry
#include "sql/key.h"
#include "sql/log.h"
#include "sql/mysqld.h"  // server_uuid
#include "sql/psi_memory_key.h"
#include "sql/raii/sentry.h"  // raii::Sentry<T>
#include "sql/replication.h"  // Trans_param
#include "sql/rpl_gtid.h"
#include "sql/rpl_mi.h"     // Master_info
#include "sql/set_var.h"    // OPT_PERSIST
#include "sql/sql_class.h"  // THD
#include "sql/sql_const.h"
#include "sql/sql_lex.h"
#include "sql/sql_parse.h"   // sql_command_flags
#include "sql/sql_plugin.h"  // plugin_int_to_ref
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/thr_malloc.h"
#include "sql/transaction_info.h"
#include "sql_string.h"

Trans_delegate *transaction_delegate;
Binlog_storage_delegate *binlog_storage_delegate;
Server_state_delegate *server_state_delegate;

Binlog_transmit_delegate *binlog_transmit_delegate;
Binlog_relay_IO_delegate *binlog_relay_io_delegate;

bool opt_replication_optimize_for_static_plugin_config{false};
std::atomic<bool> opt_replication_sender_observe_commit_only{false};

Observer_info::Observer_info(void *ob, st_plugin_int *p)
    : observer(ob), plugin_int(p) {
  plugin = plugin_int_to_ref(plugin_int);
}

Delegate::Delegate(
#ifdef HAVE_PSI_RWLOCK_INTERFACE
    PSI_rwlock_key key
#endif
) {
  inited = false;
  m_configured_lock_type.store(opt_replication_optimize_for_static_plugin_config
                                   ? DELEGATE_SPIN_LOCK
                                   : DELEGATE_OS_LOCK);
  m_acquired_locks.store(0);
#ifdef HAVE_PSI_RWLOCK_INTERFACE
  if (mysql_rwlock_init(key, &lock)) return;
#else
  if (mysql_rwlock_init(0, &lock)) return;
#endif
  inited = true;
}

Delegate::~Delegate() {
  inited = false;
  mysql_rwlock_destroy(&lock);
}

int Delegate::add_observer(void *observer, st_plugin_int *plugin) {
  int ret = false;
  if (!inited) return true;
  write_lock();
  Observer_info_iterator iter(observer_info_list);
  Observer_info *info = iter++;
  while (info && info->observer != observer) info = iter++;
  if (!info) {
    info = new Observer_info(observer, plugin);
    if (!info || observer_info_list.push_back(info, &memroot))
      ret = true;
    else if (this->use_spin_lock_type())
      acquire_plugin_ref_count(info);
  } else
    ret = true;
  unlock();
  return ret;
}

int Delegate::remove_observer(void *observer) {
  int ret = false;
  if (!inited) return true;
  write_lock();
  Observer_info_iterator iter(observer_info_list);
  Observer_info *info = iter++;
  while (info && info->observer != observer) info = iter++;
  if (info) {
    iter.remove();
    delete info;
  } else
    ret = true;
  unlock();
  return ret;
}

Delegate::Observer_info_iterator Delegate::observer_info_iter() {
  return Observer_info_iterator(observer_info_list);
}

bool Delegate::is_empty() {
  DBUG_PRINT("debug", ("is_empty: %d", observer_info_list.is_empty()));
  return observer_info_list.is_empty();
}

int Delegate::read_lock() {
  if (!inited) return true;
  this->lock_it(DELEGATE_LOCK_MODE_SHARED);
  return 0;
}

int Delegate::write_lock() {
  if (!inited) return true;
  this->lock_it(DELEGATE_LOCK_MODE_EXCLUSIVE);
  return 0;
}

int Delegate::unlock() {
  if (!inited) return true;

  int result = 0;

  if (m_acquired_locks.load() > 0) {
    m_acquired_locks -= DELEGATE_SPIN_LOCK;
    if (m_spin_lock.is_exclusive_acquisition())
      m_spin_lock.release_exclusive();
    else {
      assert(m_spin_lock.is_shared_acquisition());
      m_spin_lock.release_shared();
    }
  } else {
    assert(m_acquired_locks.load() < 0);
    m_acquired_locks -= DELEGATE_OS_LOCK;
    result = mysql_rwlock_unlock(&lock);
  }

  return result;
}

bool Delegate::is_inited() { return inited; }

void Delegate::update_lock_type() {
  if (!inited) return;

  int opt_value = opt_replication_optimize_for_static_plugin_config
                      ? DELEGATE_SPIN_LOCK
                      : DELEGATE_OS_LOCK;
  m_configured_lock_type.store(opt_value);
}

void Delegate::update_plugin_ref_count() {
  if (!inited) return;

  int opt_value = opt_replication_optimize_for_static_plugin_config
                      ? DELEGATE_SPIN_LOCK
                      : DELEGATE_OS_LOCK;
  int intern_value = m_configured_lock_type.load();

  if (intern_value == DELEGATE_SPIN_LOCK && opt_value == DELEGATE_OS_LOCK) {
    for (auto ref : m_acquired_references) {
      for (size_t count = ref.second; count != 0; --count)
        plugin_unlock(nullptr, ref.first);
    }
    m_acquired_references.clear();
  } else if (intern_value == DELEGATE_OS_LOCK &&
             opt_value == DELEGATE_SPIN_LOCK) {
    Observer_info_iterator iter = observer_info_iter();
    for (Observer_info *info = iter++; info; info = iter++) {
      acquire_plugin_ref_count(info);
    }
  }
}

bool Delegate::use_rw_lock_type() {
  return m_acquired_locks.load() < 0 ||  // If there are acquisitions using
                                         // the read-write lock
         (m_configured_lock_type.load() ==
              DELEGATE_OS_LOCK &&  // or the lock type has been set to use the
                                   // read-write lock
          m_acquired_locks.load() == 0);  // and there are no outstanding
                                          // acquisitions using shared
                                          // spin-lock, use the read-write
                                          // lock
}

bool Delegate::use_spin_lock_type() {
  return m_acquired_locks.load() > 0 ||  // If there are acquisitions using
                                         // the shared spin-lock
         (m_configured_lock_type.load() ==
              DELEGATE_SPIN_LOCK &&  // or the lock type has been set to use the
                                     // shared spin-lock
          m_acquired_locks.load() == 0);  // and there are no outstanding
                                          // acquisitions using read-write
                                          // lock, use the shared
                                          // sping-lock
}

void Delegate::acquire_plugin_ref_count(Observer_info *info) {
  plugin_ref internal_ref = plugin_lock(nullptr, &info->plugin);
  ++(m_acquired_references[internal_ref]);
}

void Delegate::lock_it(enum_delegate_lock_mode mode) {
  do {
    if (this->use_spin_lock_type()) {
      if (mode == DELEGATE_LOCK_MODE_SHARED)
        m_spin_lock.acquire_shared();
      else
        m_spin_lock.acquire_exclusive();

      if (m_configured_lock_type.load() !=
          DELEGATE_SPIN_LOCK) {  // Lock type changed in the meanwhile, lets
                                 // revert the acquisition and try again
        if (mode == DELEGATE_LOCK_MODE_SHARED)
          m_spin_lock.release_shared();
        else
          m_spin_lock.release_exclusive();
      } else {
        m_acquired_locks += DELEGATE_SPIN_LOCK;
        break;
      }
    }
    if (this->use_rw_lock_type()) {
      if (mode == DELEGATE_LOCK_MODE_SHARED)
        mysql_rwlock_rdlock(&lock);
      else
        mysql_rwlock_wrlock(&lock);

      if (m_configured_lock_type.load() !=
          DELEGATE_OS_LOCK)  // Lock type changed in the meanwhile, lets revert
                             // the acquisition and try again
        mysql_rwlock_unlock(&lock);
      else {
        m_acquired_locks += DELEGATE_OS_LOCK;
        break;
      }
    }
  } while (true);
}

/*
  structure to save transaction log filename and position
*/
typedef struct Trans_binlog_info {
  my_off_t log_pos;
  char log_file[FN_REFLEN];
} Trans_binlog_info;

int get_user_var_int(const char *name, long long int *value, int *null_value) {
  bool null_val;
  THD *thd = current_thd;

  /* Protects thd->user_vars. */
  mysql_mutex_lock(&thd->LOCK_thd_data);

  const auto it = thd->user_vars.find(name);
  if (it == thd->user_vars.end()) {
    mysql_mutex_unlock(&thd->LOCK_thd_data);
    return 1;
  }
  *value = it->second->val_int(&null_val);
  if (null_value) *null_value = null_val;
  mysql_mutex_unlock(&thd->LOCK_thd_data);
  return 0;
}

int get_user_var_real(const char *name, double *value, int *null_value) {
  bool null_val;
  THD *thd = current_thd;

  /* Protects thd->user_vars. */
  mysql_mutex_lock(&thd->LOCK_thd_data);

  const auto it = thd->user_vars.find(name);
  if (it == thd->user_vars.end()) {
    mysql_mutex_unlock(&thd->LOCK_thd_data);
    return 1;
  }
  *value = it->second->val_real(&null_val);
  if (null_value) *null_value = null_val;
  mysql_mutex_unlock(&thd->LOCK_thd_data);
  return 0;
}

int get_user_var_str(const char *name, char *value, size_t len,
                     unsigned int precision, int *null_value) {
  String str;
  bool null_val;
  THD *thd = current_thd;

  /* Protects thd->user_vars. */
  mysql_mutex_lock(&thd->LOCK_thd_data);

  const auto it = thd->user_vars.find(name);
  if (it == thd->user_vars.end()) {
    mysql_mutex_unlock(&thd->LOCK_thd_data);
    return 1;
  }
  it->second->val_str(&null_val, &str, precision);
  strncpy(value, str.c_ptr(), len);
  if (null_value) *null_value = null_val;
  mysql_mutex_unlock(&thd->LOCK_thd_data);
  return 0;
}

int delegates_init() {
  alignas(Trans_delegate) static char place_trans_mem[sizeof(Trans_delegate)];
  alignas(Binlog_storage_delegate) static char
      place_storage_mem[sizeof(Binlog_storage_delegate)];
  alignas(Server_state_delegate) static char
      place_state_mem[sizeof(Server_state_delegate)];
  alignas(Binlog_transmit_delegate) static char
      place_transmit_mem[sizeof(Binlog_transmit_delegate)];
  alignas(Binlog_relay_IO_delegate) static char
      place_relay_io_mem[sizeof(Binlog_relay_IO_delegate)];

  transaction_delegate = new (place_trans_mem) Trans_delegate;
  if (!transaction_delegate->is_inited()) {
    LogErr(ERROR_LEVEL, ER_RPL_TRX_DELEGATES_INIT_FAILED);
    return 1;
  }

  binlog_storage_delegate = new (place_storage_mem) Binlog_storage_delegate;
  if (!binlog_storage_delegate->is_inited()) {
    LogErr(ERROR_LEVEL, ER_RPL_BINLOG_STORAGE_DELEGATES_INIT_FAILED);
    return 1;
  }

  server_state_delegate = new (place_state_mem) Server_state_delegate;
  binlog_transmit_delegate = new (place_transmit_mem) Binlog_transmit_delegate;
  if (!binlog_transmit_delegate->is_inited()) {
    LogErr(ERROR_LEVEL, ER_RPL_BINLOG_TRANSMIT_DELEGATES_INIT_FAILED);
    return 1;
  }

  binlog_relay_io_delegate = new (place_relay_io_mem) Binlog_relay_IO_delegate;
  if (!binlog_relay_io_delegate->is_inited()) {
    LogErr(ERROR_LEVEL, ER_RPL_BINLOG_RELAY_DELEGATES_INIT_FAILED);
    return 1;
  }

  return 0;
}

void delegates_shutdown() {
  if (opt_replication_optimize_for_static_plugin_config) {
    opt_replication_optimize_for_static_plugin_config = false;
    delegates_acquire_locks();
    delegates_update_lock_type();
    delegates_release_locks();
  }
}

void delegates_destroy() {
  if (transaction_delegate) transaction_delegate->~Trans_delegate();
  if (binlog_storage_delegate)
    binlog_storage_delegate->~Binlog_storage_delegate();
  if (server_state_delegate) server_state_delegate->~Server_state_delegate();
  if (binlog_transmit_delegate)
    binlog_transmit_delegate->~Binlog_transmit_delegate();
  if (binlog_relay_io_delegate)
    binlog_relay_io_delegate->~Binlog_relay_IO_delegate();
}

static void delegates_update_plugin_ref_count() {
  if (transaction_delegate) transaction_delegate->update_plugin_ref_count();
  if (binlog_storage_delegate)
    binlog_storage_delegate->update_plugin_ref_count();
  if (server_state_delegate) server_state_delegate->update_plugin_ref_count();
  if (binlog_transmit_delegate)
    binlog_transmit_delegate->update_plugin_ref_count();
  if (binlog_relay_io_delegate)
    binlog_relay_io_delegate->update_plugin_ref_count();
}

void delegates_acquire_locks() {
  if (transaction_delegate) transaction_delegate->write_lock();
  if (binlog_storage_delegate) binlog_storage_delegate->write_lock();
  if (server_state_delegate) server_state_delegate->write_lock();
  if (binlog_transmit_delegate) binlog_transmit_delegate->write_lock();
  if (binlog_relay_io_delegate) binlog_relay_io_delegate->write_lock();
}

void delegates_release_locks() {
  if (transaction_delegate) transaction_delegate->unlock();
  if (binlog_storage_delegate) binlog_storage_delegate->unlock();
  if (server_state_delegate) server_state_delegate->unlock();
  if (binlog_transmit_delegate) binlog_transmit_delegate->unlock();
  if (binlog_relay_io_delegate) binlog_relay_io_delegate->unlock();
}

void delegates_update_lock_type() {
  delegates_update_plugin_ref_count();

  if (transaction_delegate) transaction_delegate->update_lock_type();
  if (binlog_storage_delegate) binlog_storage_delegate->update_lock_type();
  if (server_state_delegate) server_state_delegate->update_lock_type();
  if (binlog_transmit_delegate) binlog_transmit_delegate->update_lock_type();
  if (binlog_relay_io_delegate) binlog_relay_io_delegate->update_lock_type();
}

/*
  This macro is used by almost all the Delegate methods to iterate
  over all the observers running given callback function of the
  delegate .

  Add observer plugins to the thd->lex list, after each statement, all
  plugins add to thd->lex will be automatically unlocked.
 */
#define FOREACH_OBSERVER(r, f, args)                                   \
  r = 0;                                                               \
  Prealloced_array<plugin_ref, 8> plugins(PSI_NOT_INSTRUMENTED);       \
  read_lock();                                                         \
  Observer_info_iterator iter = observer_info_iter();                  \
  Observer_info *info = iter++;                                        \
  bool replication_optimize_for_static_plugin_config =                 \
      this->use_spin_lock_type();                                      \
  for (; info; info = iter++) {                                        \
    plugin_ref plugin = (replication_optimize_for_static_plugin_config \
                             ? info->plugin                            \
                             : my_plugin_lock(0, &info->plugin));      \
    if (!plugin) {                                                     \
      /* plugin is not initialized or deleted, this is not an error */ \
      continue;                                                        \
    }                                                                  \
    if (!replication_optimize_for_static_plugin_config)                \
      plugins.push_back(plugin);                                       \
    if (((Observer *)info->observer)->f &&                             \
        ((Observer *)info->observer)->f args) {                        \
      r = 1;                                                           \
      LogEvent()                                                       \
          .prio(ERROR_LEVEL)                                           \
          .errcode(ER_RPL_PLUGIN_FUNCTION_FAILED)                      \
          .subsys(LOG_SUBSYSTEM_TAG)                                   \
          .function(#f)                                                \
          .message("Run function '" #f "' in plugin '%s' failed",      \
                   info->plugin_int->name.str);                        \
      break;                                                           \
    }                                                                  \
  }                                                                    \
  unlock();                                                            \
  /*                                                                   \
     Unlock plugins should be done after we released the Delegate lock \
     to avoid possible deadlock when this is the last user of the      \
     plugin, and when we unlock the plugin, it will try to             \
     deinitialize the plugin, which will try to lock the Delegate in   \
     order to remove the observers.                                    \
  */                                                                   \
  if (!plugins.empty()) plugin_unlock_list(0, &plugins[0], plugins.size());

#define FOREACH_OBSERVER_ERROR_OUT(r, f, args, out)                    \
  r = 0;                                                               \
  Prealloced_array<plugin_ref, 8> plugins(PSI_NOT_INSTRUMENTED);       \
  read_lock();                                                         \
  Observer_info_iterator iter = observer_info_iter();                  \
  Observer_info *info = iter++;                                        \
                                                                       \
  bool replication_optimize_for_static_plugin_config =                 \
      this->use_spin_lock_type();                                      \
  int error_out = 0;                                                   \
  for (; info; info = iter++) {                                        \
    plugin_ref plugin = (replication_optimize_for_static_plugin_config \
                             ? info->plugin                            \
                             : my_plugin_lock(0, &info->plugin));      \
    if (!plugin) {                                                     \
      /* plugin is not initialized or deleted, this is not an error */ \
      continue;                                                        \
    }                                                                  \
    if (!replication_optimize_for_static_plugin_config)                \
      plugins.push_back(plugin);                                       \
                                                                       \
    if (nullptr == ((Observer *)info->observer)->f) {                  \
      continue;                                                        \
    }                                                                  \
    bool hook_error = false;                                           \
    hook_error = ((Observer *)info->observer)->f(args, error_out);     \
                                                                       \
    out += error_out;                                                  \
    if (hook_error) {                                                  \
      r = 1;                                                           \
      LogEvent()                                                       \
          .prio(ERROR_LEVEL)                                           \
          .errcode(ER_RPL_PLUGIN_FUNCTION_FAILED)                      \
          .subsys(LOG_SUBSYSTEM_TAG)                                   \
          .function(#f)                                                \
          .message("Run function '" #f "' in plugin '%s' failed",      \
                   info->plugin_int->name.str);                        \
      break;                                                           \
    }                                                                  \
  }                                                                    \
  unlock();                                                            \
  /*                                                                   \
     Unlock plugins should be done after we released the Delegate lock \
     to avoid possible deadlock when this is the last user of the      \
     plugin, and when we unlock the plugin, it will try to             \
     deinitialize the plugin, which will try to lock the Delegate in   \
     order to remove the observers.                                    \
  */                                                                   \
  if (!plugins.empty()) plugin_unlock_list(0, &plugins[0], plugins.size());

static bool se_before_commit(THD *, plugin_ref plugin, void *arg) {
  handlerton *hton = plugin_data<handlerton *>(plugin);
  if (hton->se_before_commit) hton->se_before_commit(arg);
  return false;
}

int Trans_delegate::before_commit(THD *thd, bool all,
                                  Binlog_cache_storage *trx_cache_log,
                                  Binlog_cache_storage *stmt_cache_log,
                                  ulonglong cache_log_max_size,
                                  bool is_atomic_ddl_arg) {
  DBUG_TRACE;
  Trans_param param;
  TRANS_PARAM_ZERO(param);
  param.server_id = thd->server_id;
  param.server_uuid = server_uuid;
  param.thread_id = thd->thread_id();
  param.gtid_info.type = thd->variables.gtid_next.type;
  param.gtid_info.sidno = thd->variables.gtid_next.gtid.sidno;
  param.gtid_info.gno = thd->variables.gtid_next.gtid.gno;
  param.trx_cache_log = trx_cache_log;
  param.stmt_cache_log = stmt_cache_log;
  param.cache_log_max_size = cache_log_max_size;
  param.original_commit_timestamp = &thd->variables.original_commit_timestamp;
  param.is_atomic_ddl = is_atomic_ddl_arg;
  param.rpl_channel_type = thd->rpl_thd_ctx.get_rpl_channel_type();
  param.group_replication_consistency =
      thd->variables.group_replication_consistency;
  param.original_server_version = &(thd->variables.original_server_version);
  param.immediate_server_version = &(thd->variables.immediate_server_version);
  param.is_create_table_as_query_block =
      (thd->lex->sql_command == SQLCOM_CREATE_TABLE &&
       !thd->lex->query_block->field_list_is_empty());

  bool is_real_trans =
      (all || !thd->get_transaction()->is_active(Transaction_ctx::SESSION));
  if (is_real_trans) param.flags |= TRANS_IS_REAL_TRANS;

  int ret = 0;

  /* After this debug point we mark the transaction as committing in THD. */
  DBUG_EXECUTE_IF("trans_delegate_before_commit_before_before_call_observers", {
    const char act[] =
        "now signal "
        "signal.trans_delegate_before_commit_before_before_call_observers_"
        "reached "
        "wait_for "
        "signal.trans_delegate_before_commit_before_before_call_observers_"
        "waiting";
    assert(!debug_sync_set_action(thd, STRING_WITH_LEN(act)));
  });

  thd->rpl_thd_ctx.set_tx_rpl_delegate_stage_status(
      Rpl_thd_context::TX_RPL_STAGE_BEFORE_COMMIT);

  /**
    If thread is killed or commits are blocked do not commit the transaction.
    Post this thread cannot be killed.
  */
  if (thd->is_killed() || m_rollback_transaction_not_reached_before_commit) {
    /**
      Disconnect the client connection if not already done.
      Do not KILL connection if the transaction is going to be rolledback.
    */
    if (!thd->is_killed()) {
      mysql_mutex_lock(&thd->LOCK_thd_data);
      thd->awake(THD::KILL_CONNECTION);
      mysql_mutex_unlock(&thd->LOCK_thd_data);
    }
    return 1;
  }
  FOREACH_OBSERVER(ret, before_commit, (&param));
  plugin_foreach(thd, se_before_commit, MYSQL_STORAGE_ENGINE_PLUGIN, &param);
  return ret;
}

/**
 Helper method to check if the given table has 'CASCADE' foreign key or not.

 @param[in]   table     Table object that needs to be verified.

 @return bool true      If the table has 'CASCADE' foreign key.
              false     If the table does not have 'CASCADE' foreign key.
*/
bool has_cascade_foreign_key(TABLE *table) {
  DBUG_TRACE;

  TABLE_SHARE_FOREIGN_KEY_INFO *fk = table->s->foreign_key;

  for (uint i = 0; i < table->s->foreign_keys; i++) {
    /*
      The supported values of update/delete_rule are: CASCADE, SET NULL,
      NO ACTION, RESTRICT and SET DEFAULT.
    */
    if (dd::Foreign_key::RULE_CASCADE == fk[i].update_rule ||
        dd::Foreign_key::RULE_CASCADE == fk[i].delete_rule ||
        dd::Foreign_key::RULE_SET_NULL == fk[i].update_rule ||
        dd::Foreign_key::RULE_SET_NULL == fk[i].delete_rule ||
        dd::Foreign_key::RULE_SET_DEFAULT == fk[i].update_rule ||
        dd::Foreign_key::RULE_SET_DEFAULT == fk[i].delete_rule) {
      return true;
    }
  }
  return false;
}

/**
 Helper method to create table information for the hook call
 */
void prepare_table_info(THD *thd, Trans_table_info *&table_info_list,
                        uint &number_of_tables) {
  DBUG_TRACE;

  TABLE *open_tables = thd->open_tables;

  // Fail if tables are not open
  if (open_tables == nullptr) {
    return;
  }

  // Gather table information
  std::vector<Trans_table_info> table_info_holder;
  for (; open_tables != nullptr; open_tables = open_tables->next) {
    Trans_table_info table_info = {nullptr, 0, 0, false};

    if (open_tables->no_replicate) {
      continue;
    }

    table_info.table_name = open_tables->s->table_name.str;

    uint primary_keys = 0;
    if (open_tables->key_info != nullptr &&
        (open_tables->s->primary_key < MAX_KEY)) {
      primary_keys = open_tables->s->primary_key;

      // if primary keys is still 0, lets double check on another var
      if (primary_keys == 0) {
        primary_keys = open_tables->key_info->user_defined_key_parts;
      }
    }

    table_info.number_of_primary_keys = primary_keys;

    table_info.db_type = open_tables->s->db_type()->db_type;

    /*
      Find out if the table has foreign key with ON UPDATE/DELETE CASCADE
      clause.
    */
    table_info.has_cascade_foreign_key = has_cascade_foreign_key(open_tables);

    table_info_holder.push_back(table_info);
  }

  // Now that one has all the information, one should build the
  // data that will be delivered to the plugin
  if (table_info_holder.size() > 0) {
    number_of_tables = table_info_holder.size();

    table_info_list = (Trans_table_info *)my_malloc(
        PSI_NOT_INSTRUMENTED, number_of_tables * sizeof(Trans_table_info),
        MYF(0));

    std::vector<Trans_table_info>::iterator table_info_holder_it =
        table_info_holder.begin();
    for (int table = 0; table_info_holder_it != table_info_holder.end();
         table_info_holder_it++, table++) {
      table_info_list[table].number_of_primary_keys =
          (*table_info_holder_it).number_of_primary_keys;
      table_info_list[table].table_name = (*table_info_holder_it).table_name;
      table_info_list[table].db_type = (*table_info_holder_it).db_type;
      table_info_list[table].has_cascade_foreign_key =
          (*table_info_holder_it).has_cascade_foreign_key;
    }
  }
}

/**
  Helper that gathers all table runtime information

  @param[in]   thd       the current execution thread
  @param[out]  ctx_info  Trans_context_info in which the result is stored.
 */
static void prepare_transaction_context(THD *thd,
                                        Trans_context_info &ctx_info) {
  // Extracting the session value of SQL binlogging
  ctx_info.binlog_enabled = thd->variables.sql_log_bin;

  // Extracting the session value of binlog format
  ctx_info.binlog_format = thd->variables.binlog_format;

  // Extracting the global mutable value of binlog checksum
  ctx_info.binlog_checksum_options = binlog_checksum_options;

  // Extracting the session value of transaction_write_set_extraction
  ctx_info.transaction_write_set_extraction =
      thd->variables.transaction_write_set_extraction;

  // Extracting transaction isolation level
  ctx_info.tx_isolation = thd->tx_isolation;
}

int Trans_delegate::before_dml(THD *thd, int &result) {
  DBUG_TRACE;
  Trans_param param;
  TRANS_PARAM_ZERO(param);

  param.server_id = thd->server_id;
  param.server_uuid = server_uuid;
  param.thread_id = thd->thread_id();

  prepare_table_info(thd, param.tables_info, param.number_of_tables);
  prepare_transaction_context(thd, param.trans_ctx_info);

  int ret = 0;
  FOREACH_OBSERVER_ERROR_OUT(ret, before_dml, &param, result);

  my_free(param.tables_info);

  return ret;
}

static bool se_before_rollback(THD *, plugin_ref plugin, void *arg) {
  handlerton *hton = plugin_data<handlerton *>(plugin);
  if (hton->se_before_rollback) hton->se_before_rollback(arg);
  return false;
}

int Trans_delegate::before_rollback(THD *thd, bool all) {
  DBUG_TRACE;
  Trans_param param;
  TRANS_PARAM_ZERO(param);
  param.server_id = thd->server_id;
  param.server_uuid = server_uuid;
  param.thread_id = thd->thread_id();
  param.rpl_channel_type = thd->rpl_thd_ctx.get_rpl_channel_type();

  bool is_real_trans =
      (all || !thd->get_transaction()->is_active(Transaction_ctx::SESSION));
  if (is_real_trans) param.flags |= TRANS_IS_REAL_TRANS;

  int ret = 0;
  thd->rpl_thd_ctx.set_tx_rpl_delegate_stage_status(
      Rpl_thd_context::TX_RPL_STAGE_BEFORE_ROLLBACK);
  FOREACH_OBSERVER(ret, before_rollback, (&param));
  plugin_foreach(thd, se_before_rollback, MYSQL_STORAGE_ENGINE_PLUGIN, &param);
  return ret;
}

static bool se_after_commit(THD *, plugin_ref plugin, void *arg) {
  handlerton *hton = plugin_data<handlerton *>(plugin);
  if (hton->se_after_commit) hton->se_after_commit(arg);
  return false;
}

int Trans_delegate::after_commit(THD *thd, bool all) {
  DBUG_TRACE;
  Trans_param param;
  TRANS_PARAM_ZERO(param);
  param.server_uuid = server_uuid;
  param.thread_id = thd->thread_id();

  Gtid gtid;
  thd->rpl_thd_ctx.last_used_gtid_tracker_ctx().get_last_used_gtid(gtid);
  param.gtid_info.sidno = gtid.sidno;
  param.gtid_info.gno = gtid.gno;

  bool is_real_trans =
      (all || !thd->get_transaction()->is_active(Transaction_ctx::SESSION));
  if (is_real_trans) param.flags |= TRANS_IS_REAL_TRANS;

  thd->get_trans_fixed_pos(&param.log_file, &param.log_pos);
  param.server_id = thd->server_id;
  param.rpl_channel_type = thd->rpl_thd_ctx.get_rpl_channel_type();

  DBUG_PRINT("enter",
             ("log_file: %s, log_pos: %llu", param.log_file, param.log_pos));
  DEBUG_SYNC(thd, "before_call_after_commit_observer");

  int ret = 0;
  FOREACH_OBSERVER(ret, after_commit, (&param));
  plugin_foreach(thd, se_after_commit, MYSQL_STORAGE_ENGINE_PLUGIN, &param);
  return ret;
}

int Trans_delegate::after_rollback(THD *thd, bool all) {
  DBUG_TRACE;
  Trans_param param;
  TRANS_PARAM_ZERO(param);
  param.server_uuid = server_uuid;
  param.thread_id = thd->thread_id();

  bool is_real_trans =
      (all || !thd->get_transaction()->is_active(Transaction_ctx::SESSION));
  if (is_real_trans) param.flags |= TRANS_IS_REAL_TRANS;
  thd->get_trans_fixed_pos(&param.log_file, &param.log_pos);
  param.server_id = thd->server_id;
  param.rpl_channel_type = thd->rpl_thd_ctx.get_rpl_channel_type();

  int ret = 0;
  FOREACH_OBSERVER(ret, after_rollback, (&param));
  return ret;
}

int Trans_delegate::trans_begin(THD *thd, int &out) {
  DBUG_TRACE;
  if (m_rollback_transaction_on_begin) {
    out = ER_OPERATION_NOT_ALLOWED_WHILE_PRIMARY_CHANGE_IS_RUNNING;
    return 0;
  }

  Trans_param param;
  TRANS_PARAM_ZERO(param);
  param.server_uuid = server_uuid;
  param.thread_id = thd->thread_id();
  param.group_replication_consistency =
      thd->variables.group_replication_consistency;
  param.hold_timeout = thd->variables.net_wait_timeout;
  param.server_id = thd->server_id;
  param.rpl_channel_type = thd->rpl_thd_ctx.get_rpl_channel_type();

  int ret = 0;
  thd->rpl_thd_ctx.set_tx_rpl_delegate_stage_status(
      Rpl_thd_context::TX_RPL_STAGE_BEGIN);
  FOREACH_OBSERVER_ERROR_OUT(ret, begin, &param, out);
  return ret;
}

int Trans_delegate::set_transactions_at_begin_must_fail() {
  DBUG_TRACE;
  m_rollback_transaction_on_begin = true;
  return false;
}

int Trans_delegate::set_no_restrictions_at_transaction_begin() {
  DBUG_TRACE;
  m_rollback_transaction_on_begin = false;
  return false;
}

int Trans_delegate::set_transactions_not_reached_before_commit_must_fail() {
  DBUG_TRACE;
  m_rollback_transaction_not_reached_before_commit = true;
  return false;
}

int Trans_delegate::set_no_restrictions_at_transactions_before_commit() {
  DBUG_TRACE;
  m_rollback_transaction_not_reached_before_commit = false;
  return false;
}

int Binlog_storage_delegate::after_flush(THD *thd, const char *log_file,
                                         my_off_t log_pos) {
  DBUG_TRACE;
  DBUG_PRINT("enter",
             ("log_file: %s, log_pos: %llu", log_file, (ulonglong)log_pos));
  Binlog_storage_param param;
  param.server_id = thd->server_id;

  int ret = 0;
  FOREACH_OBSERVER(ret, after_flush, (&param, log_file, log_pos));
  return ret;
}

/**
 * This hook MUST be invoked after ALL recovery operations are performed
 * and the server is ready to serve clients.
 *
 * @return 0 on success, >0 otherwise.
 */
int Server_state_delegate::before_handle_connection(THD *) {
  DBUG_TRACE;
  Server_state_param param;

  int ret = 0;
  FOREACH_OBSERVER(ret, before_handle_connection, (&param));
  return ret;
}

/**
 * This hook MUST be invoked before ANY recovery action is started.
 *
 * @return 0 on success, >0 otherwise.
 */
int Server_state_delegate::before_recovery(THD *) {
  DBUG_TRACE;
  Server_state_param param;

  int ret = 0;
  FOREACH_OBSERVER(ret, before_recovery, (&param));
  return ret;
}

/**
 * This hook MUST be invoked after the recovery from the engine
 * is complete.
 *
 * @return 0 on success, >0 otherwise.
 */
int Server_state_delegate::after_engine_recovery(THD *) {
  DBUG_TRACE;
  Server_state_param param;

  int ret = 0;
  FOREACH_OBSERVER(ret, after_engine_recovery, (&param));
  return ret;
}

/**
 * This hook MUST be invoked after the server has completed the
 * local recovery. The server can proceed with the further operations
 * like engaging in distributed recovery etc.
 *
 * @return 0 on success, >0 otherwise.
 */
int Server_state_delegate::after_recovery(THD *) {
  DBUG_TRACE;
  Server_state_param param;

  int ret = 0;
  FOREACH_OBSERVER(ret, after_recovery, (&param));
  return ret;
}

/**
 * This hook MUST be invoked before server shutdown action is
 * initiated.
 *
 * @return 0 on success, >0 otherwise.
 */
int Server_state_delegate::before_server_shutdown(THD *) {
  DBUG_TRACE;
  Server_state_param param;

  int ret = 0;
  FOREACH_OBSERVER(ret, before_server_shutdown, (&param));
  return ret;
}

/**
 * This hook MUST be invoked after server shutdown operation is
 * complete.
 *
 * @return 0 on success, >0 otherwise.
 */
int Server_state_delegate::after_server_shutdown(THD *) {
  DBUG_TRACE;
  Server_state_param param;

  int ret = 0;
  FOREACH_OBSERVER(ret, after_server_shutdown, (&param));
  return ret;
}

/**
 * This hook MUST be invoked after upgrade from .frm to data dictionary
 *
 * @return 0 on success, >0 otherwise.
 */
int Server_state_delegate::after_dd_upgrade_from_57(THD *) {
  DBUG_TRACE;
  Server_state_param param;

  int ret = 0;
  FOREACH_OBSERVER(ret, after_dd_upgrade_from_57, (&param));
  return ret;
}

int Binlog_storage_delegate::after_sync(THD *thd, const char *log_file,
                                        my_off_t log_pos) {
  DBUG_TRACE;
  DBUG_PRINT("enter",
             ("log_file: %s, log_pos: %llu", log_file, (ulonglong)log_pos));
  Binlog_storage_param param;
  param.server_id = thd->server_id;

  assert(log_pos != 0);
  int ret = 0;
  FOREACH_OBSERVER(ret, after_sync, (&param, log_file, log_pos));

  DEBUG_SYNC(thd, "after_call_after_sync_observer");
  return ret;
}

int Binlog_transmit_delegate::transmit_start(THD *thd, ushort flags,
                                             const char *log_file,
                                             my_off_t log_pos,
                                             bool *observe_transmission) {
  Binlog_transmit_param param;
  param.flags = flags;
  param.server_id = thd->server_id;

  int ret = 0;
  FOREACH_OBSERVER(ret, transmit_start, (&param, log_file, log_pos));
  *observe_transmission = param.should_observe();
  return ret;
}

int Binlog_transmit_delegate::transmit_stop(THD *thd, ushort flags) {
  Binlog_transmit_param param;
  param.flags = flags;
  param.server_id = thd->server_id;

  DBUG_EXECUTE_IF("crash_binlog_transmit_hook", DBUG_SUICIDE(););

  int ret = 0;
  FOREACH_OBSERVER(ret, transmit_stop, (&param));
  return ret;
}

int Binlog_transmit_delegate::reserve_header(THD *thd, ushort flags,
                                             String *packet) {
  /*
    NOTE2ME: Maximum extra header size for each observer, I hope 32
    bytes should be enough for each Observer to reserve their
    extra header. If later found this is not enough, we can increase this /HEZX
   */
  constexpr int RESERVE_HEADER_SIZE = 32;
  unsigned char header[RESERVE_HEADER_SIZE];
  ulong hlen;
  Binlog_transmit_param param;
  param.flags = flags;
  param.server_id = thd->server_id;

  DBUG_EXECUTE_IF("crash_binlog_transmit_hook", DBUG_SUICIDE(););

  int ret = 0;
  read_lock();
  Observer_info_iterator iter = observer_info_iter();
  Observer_info *info = iter++;
  bool replication_optimize_for_static_plugin_config =
      this->use_spin_lock_type();
  for (; info; info = iter++) {
    plugin_ref plugin = (replication_optimize_for_static_plugin_config
                             ? info->plugin
                             : my_plugin_lock(thd, &info->plugin));
    if (!plugin) {
      ret = 1;
      break;
    }
    hlen = 0;
    {  // `unlock_guard` scope
      raii::Sentry<> unlock_guard{[&]() -> void {
        if (!replication_optimize_for_static_plugin_config)
          plugin_unlock(thd, plugin);
      }};
      if (((Observer *)info->observer)->reserve_header &&
          ((Observer *)info->observer)
              ->reserve_header(&param, header, RESERVE_HEADER_SIZE, &hlen)) {
        ret = 1;
        break;
      }
    }  // `unlock_guard` scope
    if (hlen == 0) continue;
    if (hlen > RESERVE_HEADER_SIZE || packet->append((char *)header, hlen)) {
      ret = 1;
      break;
    }
  }
  unlock();
  return ret;
}

int Binlog_transmit_delegate::before_send_event(THD *thd, ushort flags,
                                                String *packet,
                                                const char *log_file,
                                                my_off_t log_pos) {
  Binlog_transmit_param param;
  param.flags = flags;
  param.server_id = thd->server_id;

  DBUG_EXECUTE_IF("crash_binlog_transmit_hook", DBUG_SUICIDE(););

  int ret = 0;
  FOREACH_OBSERVER(
      ret, before_send_event,
      (&param, pointer_cast<uchar *>(packet->ptr()), packet->length(),
       log_file + dirname_length(log_file), log_pos));
  return ret;
}

int Binlog_transmit_delegate::after_send_event(THD *thd, ushort flags,
                                               String *packet,
                                               const char *skipped_log_file,
                                               my_off_t skipped_log_pos) {
  Binlog_transmit_param param;
  param.flags = flags;
  param.server_id = thd->server_id;

  DBUG_EXECUTE_IF("crash_binlog_transmit_hook", DBUG_SUICIDE(););

  int ret = 0;
  FOREACH_OBSERVER(
      ret, after_send_event,
      (&param, packet->ptr(), packet->length(),
       skipped_log_file + dirname_length(skipped_log_file), skipped_log_pos));
  return ret;
}

int Binlog_transmit_delegate::after_reset_master(THD *thd, ushort flags)

{
  Binlog_transmit_param param;
  param.flags = flags;
  param.server_id = thd->server_id;

  int ret = 0;
  FOREACH_OBSERVER(ret, after_reset_master, (&param));
  return ret;
}

void Binlog_relay_IO_delegate::init_param(Binlog_relay_IO_param *param,
                                          Master_info *mi) {
  param->mysql = mi->mysql;
  param->channel_name = mi->get_channel();
  param->user = const_cast<char *>(mi->get_user());
  param->host = mi->host;
  param->port = mi->port;
  param->master_log_name = const_cast<char *>(mi->get_master_log_name());
  param->master_log_pos = mi->get_master_log_pos();
  param->source_connection_auto_failover =
      mi->is_source_connection_auto_failover();
}

int Binlog_relay_IO_delegate::thread_start(THD *thd, Master_info *mi) {
  Binlog_relay_IO_param param;
  init_param(&param, mi);
  param.server_id = thd->server_id;
  param.thread_id = thd->thread_id();

  int ret = 0;
  FOREACH_OBSERVER(ret, thread_start, (&param));
  return ret;
}

int Binlog_relay_IO_delegate::thread_stop(THD *thd, Master_info *mi) {
  Binlog_relay_IO_param param;
  init_param(&param, mi);
  param.server_id = thd->server_id;
  param.thread_id = thd->thread_id();

  int ret = 0;
  FOREACH_OBSERVER(ret, thread_stop, (&param));
  return ret;
}

int Binlog_relay_IO_delegate::applier_start(THD *thd, Master_info *mi) {
  Binlog_relay_IO_param param;
  init_param(&param, mi);
  param.server_id = thd->server_id;
  param.thread_id = thd->thread_id();

  int ret = 0;
  FOREACH_OBSERVER(ret, applier_start, (&param));
  return ret;
}

int Binlog_relay_IO_delegate::applier_stop(THD *thd, Master_info *mi,
                                           bool aborted) {
  Binlog_relay_IO_param param;
  init_param(&param, mi);
  param.server_id = thd->server_id;
  param.thread_id = thd->thread_id();

  int ret = 0;
  FOREACH_OBSERVER(ret, applier_stop, (&param, aborted));
  return ret;
}

int Binlog_relay_IO_delegate::before_request_transmit(THD *thd, Master_info *mi,
                                                      ushort flags) {
  Binlog_relay_IO_param param;
  init_param(&param, mi);
  param.server_id = thd->server_id;
  param.thread_id = thd->thread_id();

  int ret = 0;
  FOREACH_OBSERVER(ret, before_request_transmit, (&param, (uint32)flags));
  return ret;
}

int Binlog_relay_IO_delegate::after_read_event(THD *thd, Master_info *mi,
                                               const char *packet, ulong len,
                                               const char **event_buf,
                                               ulong *event_len) {
  Binlog_relay_IO_param param;
  init_param(&param, mi);
  param.server_id = thd->server_id;
  param.thread_id = thd->thread_id();

  int ret = 0;
  FOREACH_OBSERVER(ret, after_read_event,
                   (&param, packet, len, event_buf, event_len));
  return ret;
}

int Binlog_relay_IO_delegate::after_queue_event(THD *thd, Master_info *mi,
                                                const char *event_buf,
                                                ulong event_len, bool synced) {
  Binlog_relay_IO_param param;
  init_param(&param, mi);
  param.server_id = thd->server_id;
  param.thread_id = thd->thread_id();

  uint32 flags = 0;
  if (synced) flags |= BINLOG_STORAGE_IS_SYNCED;

  int ret = 0;
  FOREACH_OBSERVER(ret, after_queue_event,
                   (&param, event_buf, event_len, flags));
  return ret;
}

int Binlog_relay_IO_delegate::after_reset_slave(THD *thd, Master_info *mi)

{
  Binlog_relay_IO_param param;
  init_param(&param, mi);
  param.server_id = thd->server_id;
  param.thread_id = thd->thread_id();

  int ret = 0;
  FOREACH_OBSERVER(ret, after_reset_slave, (&param));
  return ret;
}

int Binlog_relay_IO_delegate::applier_log_event(THD *thd, int &out) {
  DBUG_TRACE;
  Trans_param trans_param;
  TRANS_PARAM_ZERO(trans_param);
  Binlog_relay_IO_param param;

  param.server_id = thd->server_id;
  param.thread_id = thd->thread_id();

  prepare_table_info(thd, trans_param.tables_info,
                     trans_param.number_of_tables);

  int ret = 0;
  FOREACH_OBSERVER(ret, applier_log_event, (&param, &trans_param, out));

  my_free(trans_param.tables_info);

  return ret;
}

int register_trans_observer(Trans_observer *observer, void *p) {
  return transaction_delegate->add_observer(observer, (st_plugin_int *)p);
}

int unregister_trans_observer(Trans_observer *observer, void *) {
  return transaction_delegate->remove_observer(observer);
}

int register_binlog_storage_observer(Binlog_storage_observer *observer,
                                     void *p) {
  DBUG_TRACE;
  int result =
      binlog_storage_delegate->add_observer(observer, (st_plugin_int *)p);
  return result;
}

int unregister_binlog_storage_observer(Binlog_storage_observer *observer,
                                       void *) {
  return binlog_storage_delegate->remove_observer(observer);
}

int register_server_state_observer(Server_state_observer *observer,
                                   void *plugin_var) {
  DBUG_TRACE;
  int result = server_state_delegate->add_observer(observer,
                                                   (st_plugin_int *)plugin_var);
  return result;
}

int unregister_server_state_observer(Server_state_observer *observer, void *) {
  DBUG_TRACE;
  int result = server_state_delegate->remove_observer(observer);
  return result;
}

int register_binlog_transmit_observer(Binlog_transmit_observer *observer,
                                      void *p) {
  return binlog_transmit_delegate->add_observer(observer, (st_plugin_int *)p);
}

int unregister_binlog_transmit_observer(Binlog_transmit_observer *observer,
                                        void *) {
  return binlog_transmit_delegate->remove_observer(observer);
}

int register_binlog_relay_io_observer(Binlog_relay_IO_observer *observer,
                                      void *p) {
  return binlog_relay_io_delegate->add_observer(observer, (st_plugin_int *)p);
}

int unregister_binlog_relay_io_observer(Binlog_relay_IO_observer *observer,
                                        void *) {
  return binlog_relay_io_delegate->remove_observer(observer);
}

static bool is_show_status(enum_sql_command sql_command) {
  switch (sql_command) {
    case SQLCOM_SHOW_VARIABLES:
    case SQLCOM_SHOW_STATUS:
    case SQLCOM_SHOW_ENGINE_LOGS:
    case SQLCOM_SHOW_ENGINE_STATUS:
    case SQLCOM_SHOW_ENGINE_MUTEX:
    case SQLCOM_SHOW_PROCESSLIST:
    case SQLCOM_SHOW_MASTER_STAT:
    case SQLCOM_SHOW_SLAVE_STAT:
    case SQLCOM_SHOW_CHARSETS:
    case SQLCOM_SHOW_COLLATIONS:
    case SQLCOM_SHOW_BINLOGS:
    case SQLCOM_SHOW_OPEN_TABLES:
    case SQLCOM_SHOW_SLAVE_HOSTS:
    case SQLCOM_SHOW_BINLOG_EVENTS:
    case SQLCOM_SHOW_WARNS:
    case SQLCOM_SHOW_ERRORS:
    case SQLCOM_SHOW_STORAGE_ENGINES:
    case SQLCOM_SHOW_PRIVILEGES:
    case SQLCOM_SHOW_STATUS_PROC:
    case SQLCOM_SHOW_STATUS_FUNC:
    case SQLCOM_SHOW_PLUGINS:
    case SQLCOM_SHOW_EVENTS:
    case SQLCOM_SHOW_PROFILE:
    case SQLCOM_SHOW_PROFILES:
    case SQLCOM_SHOW_RELAYLOG_EVENTS:
      return true;
    default:
      return false;
  }
}

int launch_hook_trans_begin(THD *thd, Table_ref *all_tables) {
  DBUG_TRACE;
  LEX *lex = thd->lex;
  enum_sql_command sql_command = lex->sql_command;
  // by default commands are put on hold
  bool hold_command = true;
  int ret = 0;

  // if command belong to a transaction that already pass by hook, it can
  // continue
  if (thd->get_transaction()->was_trans_begin_hook_invoked()) {
    return 0;
  }

  bool is_show = is_show_status(sql_command);
  bool is_set = (sql_command == SQLCOM_SET_OPTION);
  bool is_query_block =
      (sql_command == SQLCOM_SELECT || sql_command == SQLCOM_DO);
  bool is_empty = (sql_command == SQLCOM_EMPTY_QUERY);
  bool is_use = (sql_command == SQLCOM_CHANGE_DB);
  bool is_stop_gr = (sql_command == SQLCOM_STOP_GROUP_REPLICATION);
  bool is_shutdown = (sql_command == SQLCOM_SHUTDOWN);
  bool is_reset_persist =
      (sql_command == SQLCOM_RESET && lex->option_type == OPT_PERSIST);

  if ((is_set || is_show || is_empty || is_use || is_stop_gr || is_shutdown ||
       is_reset_persist) &&
      !lex->uses_stored_routines()) {
    return 0;
  }

  if (is_query_block) {
    bool is_udf = false;

    // If SELECT or DO is a UDF
    Query_block *query_block_elem = lex->unit->first_query_block();
    while (query_block_elem != nullptr) {
      for (Item *item : query_block_elem->visible_fields()) {
        if (item->type() == Item::FUNC_ITEM) {
          Item_func *func_item = down_cast<Item_func *>(item);
          Item_func::Functype functype = func_item->functype();
          if (functype == Item_func::FUNC_SP || functype == Item_func::UDF_FUNC)
            is_udf = true;
        }
      }
      query_block_elem = query_block_elem->next_query_block();
    }

    if (!is_udf && all_tables == nullptr) {
      // SELECT or DO that don't use tables and isn't a UDF
      hold_command = false;
    }

    if (hold_command && all_tables != nullptr) {
      // SELECT that use tables
      bool is_perf_schema_table = false;
      bool is_process_list = false;
      bool is_sys_db = false;
      bool stop_db_check = false;

      for (Table_ref *table = all_tables; table && !stop_db_check;
           table = table->next_global) {
        assert(table->db && table->table_name);

        if (is_perfschema_db(table->db, table->db_length))
          is_perf_schema_table = true;
        else if (is_infoschema_db(table->db, table->db_length) &&
                 !my_strcasecmp(system_charset_info, "PROCESSLIST",
                                table->table_name)) {
          is_process_list = true;
        } else if (table->db_length == 3 &&
                   !my_strcasecmp(system_charset_info, "sys", table->db)) {
          is_sys_db = true;
        } else {
          is_perf_schema_table = false;
          is_process_list = false;
          is_sys_db = false;
          stop_db_check = true;
        }
      }

      if (is_process_list || is_perf_schema_table || is_sys_db) {
        hold_command = false;
      }
    }
  }

  if (hold_command) {
    DBUG_EXECUTE_IF("launch_hook_trans_begin_assert_if_hold", { assert(0); };);

    PSI_stage_info old_stage;
    thd->enter_stage(&stage_hook_begin_trans, &old_stage, __func__, __FILE__,
                     __LINE__);
    RUN_HOOK(transaction, trans_begin, (thd, ret));
    THD_STAGE_INFO(thd, old_stage);
    if (!ret && (sql_command == SQLCOM_BEGIN ||
                 thd->in_active_multi_stmt_transaction())) {
      thd->get_transaction()->set_trans_begin_hook_invoked();
    }
  }

  return ret;
}
