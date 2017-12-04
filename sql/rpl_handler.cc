/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "rpl_handler.h"

#include "debug_sync.h"        // DEBUG_SYNC
#include "log.h"               // sql_print_error
#include "replication.h"       // Trans_param
#include "rpl_mi.h"            // Master_info
#include "sql_class.h"         // THD
#include "sql_plugin.h"        // plugin_int_to_ref


#include <vector>

Trans_delegate *transaction_delegate;
Binlog_storage_delegate *binlog_storage_delegate;
Server_state_delegate *server_state_delegate;

#ifdef HAVE_REPLICATION
Binlog_transmit_delegate *binlog_transmit_delegate;
Binlog_relay_IO_delegate *binlog_relay_io_delegate;
#endif /* HAVE_REPLICATION */

Observer_info::Observer_info(void *ob, st_plugin_int *p)
  : observer(ob), plugin_int(p)
{
  plugin= plugin_int_to_ref(plugin_int);
}


/*
  structure to save transaction log filename and position
*/
typedef struct Trans_binlog_info {
  my_off_t log_pos;
  char log_file[FN_REFLEN];
} Trans_binlog_info;

int get_user_var_int(const char *name,
                     long long int *value, int *null_value)
{
  my_bool null_val;
  THD *thd= current_thd;

  /* Protects thd->user_vars. */
  mysql_mutex_lock(&thd->LOCK_thd_data);

  user_var_entry *entry=
    (user_var_entry*) my_hash_search(&thd->user_vars,
                                  (uchar*) name, strlen(name));
  if (!entry)
  {
    mysql_mutex_unlock(&thd->LOCK_thd_data);
    return 1;
  }
  *value= entry->val_int(&null_val);
  if (null_value)
    *null_value= null_val;
  mysql_mutex_unlock(&thd->LOCK_thd_data);
  return 0;
}

int get_user_var_real(const char *name,
                      double *value, int *null_value)
{
  my_bool null_val;
  THD *thd= current_thd;

  /* Protects thd->user_vars. */
  mysql_mutex_lock(&thd->LOCK_thd_data);

  user_var_entry *entry=
    (user_var_entry*) my_hash_search(&thd->user_vars,
                                  (uchar*) name, strlen(name));
  if (!entry)
  {
    mysql_mutex_unlock(&thd->LOCK_thd_data);
    return 1;
  }
  *value= entry->val_real(&null_val);
  if (null_value)
    *null_value= null_val;
  mysql_mutex_unlock(&thd->LOCK_thd_data);
  return 0;
}

int get_user_var_str(const char *name, char *value,
                     size_t len, unsigned int precision, int *null_value)
{
  String str;
  my_bool null_val;
  THD *thd= current_thd;

  /* Protects thd->user_vars. */
  mysql_mutex_lock(&thd->LOCK_thd_data);

  user_var_entry *entry=
    (user_var_entry*) my_hash_search(&thd->user_vars,
                                  (uchar*) name, strlen(name));
  if (!entry)
  {
    mysql_mutex_unlock(&thd->LOCK_thd_data);
    return 1;
  }
  entry->val_str(&null_val, &str, precision);
  strncpy(value, str.c_ptr(), len);
  if (null_value)
    *null_value= null_val;
  mysql_mutex_unlock(&thd->LOCK_thd_data);
  return 0;
}

int delegates_init()
{
  static my_aligned_storage<sizeof(Trans_delegate),
                            MY_ALIGNOF(longlong)> trans_mem;
  static my_aligned_storage<sizeof(Binlog_storage_delegate),
                            MY_ALIGNOF(longlong)> storage_mem;
  static my_aligned_storage<sizeof(Server_state_delegate),
                            MY_ALIGNOF(longlong)> server_state_mem;
#ifdef HAVE_REPLICATION
  static my_aligned_storage<sizeof(Binlog_transmit_delegate),
                            MY_ALIGNOF(longlong)> transmit_mem;
  static my_aligned_storage<sizeof(Binlog_relay_IO_delegate),
                            MY_ALIGNOF(longlong)> relay_io_mem;
#endif

  void *place_trans_mem= trans_mem.data;
  void *place_storage_mem= storage_mem.data;
  void *place_state_mem= server_state_mem.data;

  transaction_delegate= new (place_trans_mem) Trans_delegate;

  if (!transaction_delegate->is_inited())
  {
    sql_print_error("Initialization of transaction delegates failed. "
                    "Please report a bug.");
    return 1;
  }

  binlog_storage_delegate= new (place_storage_mem) Binlog_storage_delegate;

  if (!binlog_storage_delegate->is_inited())
  {
    sql_print_error("Initialization binlog storage delegates failed. "
                    "Please report a bug.");
    return 1;
  }

  server_state_delegate= new (place_state_mem) Server_state_delegate;

#ifdef HAVE_REPLICATION
  void *place_transmit_mem= transmit_mem.data;
  void *place_relay_io_mem= relay_io_mem.data;

  binlog_transmit_delegate= new (place_transmit_mem) Binlog_transmit_delegate;

  if (!binlog_transmit_delegate->is_inited())
  {
    sql_print_error("Initialization of binlog transmit delegates failed. "
                    "Please report a bug.");
    return 1;
  }

  binlog_relay_io_delegate= new (place_relay_io_mem) Binlog_relay_IO_delegate;

  if (!binlog_relay_io_delegate->is_inited())
  {
    sql_print_error("Initialization binlog relay IO delegates failed. "
                    "Please report a bug.");
    return 1;
  }
#endif

  return 0;
}

void delegates_destroy()
{
  if (transaction_delegate)
    transaction_delegate->~Trans_delegate();
  if (binlog_storage_delegate)
    binlog_storage_delegate->~Binlog_storage_delegate();
  if (server_state_delegate)
    server_state_delegate->~Server_state_delegate();
#ifdef HAVE_REPLICATION
  if (binlog_transmit_delegate)
    binlog_transmit_delegate->~Binlog_transmit_delegate();
  if (binlog_relay_io_delegate)
    binlog_relay_io_delegate->~Binlog_relay_IO_delegate();
#endif /* HAVE_REPLICATION */
}

/*
  This macro is used by almost all the Delegate methods to iterate
  over all the observers running given callback function of the
  delegate .

  Add observer plugins to the thd->lex list, after each statement, all
  plugins add to thd->lex will be automatically unlocked.
 */
#define FOREACH_OBSERVER(r, f, thd, args)                               \
  /*
     Use a struct to make sure that they are allocated adjacent, check
     delete_dynamic().
  */                                                                    \
  Prealloced_array<plugin_ref, 8> plugins(PSI_NOT_INSTRUMENTED);        \
  read_lock();                                                          \
  Observer_info_iterator iter= observer_info_iter();                    \
  Observer_info *info= iter++;                                          \
  for (; info; info= iter++)                                            \
  {                                                                     \
    plugin_ref plugin=                                                  \
      my_plugin_lock(0, &info->plugin);                                 \
    if (!plugin)                                                        \
    {                                                                   \
      /* plugin is not intialized or deleted, this is not an error */   \
      r= 0;                                                             \
      break;                                                            \
    }                                                                   \
    plugins.push_back(plugin);                                          \
    if (((Observer *)info->observer)->f                                 \
        && ((Observer *)info->observer)->f args)                        \
    {                                                                   \
      r= 1;                                                             \
      sql_print_error("Run function '" #f "' in plugin '%s' failed",    \
                      info->plugin_int->name.str);                      \
      break;                                                            \
    }                                                                   \
  }                                                                     \
  unlock();                                                             \
  /*
     Unlock plugins should be done after we released the Delegate lock
     to avoid possible deadlock when this is the last user of the
     plugin, and when we unlock the plugin, it will try to
     deinitialize the plugin, which will try to lock the Delegate in
     order to remove the observers.
  */                                                                    \
  if (!plugins.empty())                                                 \
    plugin_unlock_list(0, &plugins[0], plugins.size());

#define FOREACH_OBSERVER_ERROR_OUT(r, f, thd, args, out)                \
  /*
     Use a struct to make sure that they are allocated adjacent, check
     delete_dynamic().
  */                                                                    \
  Prealloced_array<plugin_ref, 8> plugins(PSI_NOT_INSTRUMENTED);        \
  read_lock();                                                          \
  Observer_info_iterator iter= observer_info_iter();                    \
  Observer_info *info= iter++;                                          \
                                                                        \
  int error_out= 0;                                                     \
  for (; info; info= iter++)                                            \
  {                                                                     \
    plugin_ref plugin=                                                  \
      my_plugin_lock(0, &info->plugin);                                 \
    if (!plugin)                                                        \
    {                                                                   \
      /* plugin is not intialized or deleted, this is not an error */   \
      r= 0;                                                             \
      break;                                                            \
    }                                                                   \
    plugins.push_back(plugin);                                          \
                                                                        \
    bool hook_error= false;                                             \
    hook_error= ((Observer *)info->observer)->f (args, error_out);      \
                                                                        \
    out += error_out;                                                   \
    if (hook_error)                                                     \
    {                                                                   \
      r= 1;                                                             \
      sql_print_error("Run function '" #f "' in plugin '%s' failed",    \
                      info->plugin_int->name.str);                      \
      break;                                                            \
    }                                                                   \
  }                                                                     \
  unlock();                                                             \
  /*
     Unlock plugins should be done after we released the Delegate lock
     to avoid possible deadlock when this is the last user of the
     plugin, and when we unlock the plugin, it will try to
     deinitialize the plugin, which will try to lock the Delegate in
     order to remove the observers.
  */                                                                    \
  if (!plugins.empty())                                                 \
    plugin_unlock_list(0, &plugins[0], plugins.size());

int Trans_delegate::before_commit(THD *thd, bool all,
                                  IO_CACHE *trx_cache_log,
                                  IO_CACHE *stmt_cache_log,
                                  ulonglong cache_log_max_size)
{
  DBUG_ENTER("Trans_delegate::before_commit");
  Trans_param param;
  TRANS_PARAM_ZERO(param);
  param.server_id= thd->server_id;
  param.server_uuid= server_uuid;
  param.thread_id= thd->thread_id();
  param.gtid_info.type= thd->variables.gtid_next.type;
  param.gtid_info.sidno= thd->variables.gtid_next.gtid.sidno;
  param.gtid_info.gno= thd->variables.gtid_next.gtid.gno;
  param.trx_cache_log= trx_cache_log;
  param.stmt_cache_log= stmt_cache_log;
  param.cache_log_max_size= cache_log_max_size;

  bool is_real_trans=
    (all || !thd->get_transaction()->is_active(Transaction_ctx::SESSION));
  if (is_real_trans)
    param.flags|= TRANS_IS_REAL_TRANS;

  int ret= 0;
  FOREACH_OBSERVER(ret, before_commit, thd, (&param));
  DBUG_RETURN(ret);
}

/**
 Helper method to check if the given table has 'CASCADE' foreign key or not.

 @param[in]   TABLE     Table object that needs to be verified.
 @param[in]   THD       Current execution thread.

 @return bool TRUE      If the table has 'CASCADE' foreign key.
              FALSE     If the table does not have 'CASCADE' foreign key.
*/
bool has_cascade_foreign_key(TABLE *table, THD *thd)
{
  DBUG_ENTER("has_cascade_foreign_key");
  List<FOREIGN_KEY_INFO> f_key_list;
  table->file->get_foreign_key_list(thd, &f_key_list);

  FOREIGN_KEY_INFO *f_key_info;
  List_iterator_fast<FOREIGN_KEY_INFO> foreign_key_iterator(f_key_list);
  while ((f_key_info=foreign_key_iterator++))
  {
    /*
     The possible values for update_method are
     {"CASCADE", "SET NULL", "NO ACTION", "RESTRICT"}.

     Hence we are avoiding the usage of strncmp
     ("'update_method' value with 'CASCADE' or 'SET NULL'") and just comparing
     the first character of the update_method value with 'C' or 'S'.
    */
    if (f_key_info->update_method->str[0] == 'C' ||
        f_key_info->delete_method->str[0] == 'C' ||
        f_key_info->update_method->str[0] == 'S' ||
        f_key_info->delete_method->str[0] == 'S')
    {
      DBUG_ASSERT(!strncmp(f_key_info->update_method->str, "CASCADE", 7) ||
                  !strncmp(f_key_info->delete_method->str, "CASCADE", 7) ||
                  !strncmp(f_key_info->update_method->str, "SET NUL", 7) ||
                  !strncmp(f_key_info->delete_method->str, "SET NUL", 7));
      DBUG_RETURN(TRUE);
    }
  }
  DBUG_RETURN(FALSE);
}

/**
 Helper method to create table information for the hook call
 */
void
Trans_delegate::prepare_table_info(THD* thd,
                                   Trans_table_info*& table_info_list,
                                   uint& number_of_tables)
{
  DBUG_ENTER("Trans_delegate::prepare_table_info");

  TABLE* open_tables= thd->open_tables;

  // Fail if tables are not open
  if(open_tables == NULL)
  {
    DBUG_VOID_RETURN;
  }

  //Gather table information
  std::vector<Trans_table_info> table_info_holder;
  for(; open_tables != NULL; open_tables= open_tables->next)
  {
    Trans_table_info table_info = {0,0,0,0};

    if (open_tables->no_replicate)
    {
      continue;
    }

    table_info.table_name= open_tables->s->table_name.str;

    uint primary_keys= 0;
    if(open_tables->key_info != NULL && (open_tables->s->primary_key < MAX_KEY))
    {
      primary_keys= open_tables->s->primary_key;

      //if primary keys is still 0, lets double check on another var
      if(primary_keys == 0)
      {
        primary_keys= open_tables->key_info->user_defined_key_parts;
      }
    }

    table_info.number_of_primary_keys= primary_keys;

    table_info.db_type= open_tables->s->db_type()->db_type;

    /*
      Find out if the table has foreign key with ON UPDATE/DELETE CASCADE
      clause.
    */
    table_info.has_cascade_foreign_key= has_cascade_foreign_key(open_tables, thd);

    table_info_holder.push_back(table_info);
  }

  //Now that one has all the information, one should build the
  // data that will be delivered to the plugin
  if(table_info_holder.size() > 0)
  {
    number_of_tables= table_info_holder.size();

    table_info_list= (Trans_table_info*)my_malloc(
                               PSI_NOT_INSTRUMENTED,
                               number_of_tables * sizeof(Trans_table_info),
                               MYF(0));

    std::vector<Trans_table_info>::iterator table_info_holder_it
                                                  = table_info_holder.begin();
    for(int table= 0;
        table_info_holder_it != table_info_holder.end();
        table_info_holder_it++, table++)
    {
      table_info_list[table].number_of_primary_keys
                                = (*table_info_holder_it).number_of_primary_keys;
      table_info_list[table].table_name
                                = (*table_info_holder_it).table_name;
      table_info_list[table].db_type
                                = (*table_info_holder_it).db_type;
      table_info_list[table].has_cascade_foreign_key
                                = (*table_info_holder_it).has_cascade_foreign_key;
    }
  }

  DBUG_VOID_RETURN;
}

/**
  Helper that gathers all table runtime information

  @param[in]   THD       the current execution thread
  @param[out]  ctx_info  Trans_context_info in which the result is stored.
 */
void prepare_transaction_context(THD* thd, Trans_context_info& ctx_info)
{
  //Extracting the session value of SQL binlogging
  ctx_info.binlog_enabled= thd->variables.sql_log_bin;

  //Extracting the session value of binlog format
  ctx_info.binlog_format= thd->variables.binlog_format;

  //Extracting the global mutable value of binlog checksum
  ctx_info.binlog_checksum_options= binlog_checksum_options;

  //Extracting the session value of transaction_write_set_extraction
  ctx_info.transaction_write_set_extraction=
    thd->variables.transaction_write_set_extraction;

  //Extracting transaction isolation level
  ctx_info.tx_isolation= thd->tx_isolation;
}

int Trans_delegate::before_dml(THD* thd, int& result)
{
  DBUG_ENTER("Trans_delegate::before_dml");
  Trans_param param;
  TRANS_PARAM_ZERO(param);

  param.server_id= thd->server_id;
  param.server_uuid= server_uuid;
  param.thread_id= thd->thread_id();

  prepare_table_info(thd, param.tables_info, param.number_of_tables);
  prepare_transaction_context(thd, param.trans_ctx_info);

  int ret= 0;
  FOREACH_OBSERVER_ERROR_OUT(ret, before_dml, thd, &param, result);

  my_free(param.tables_info);

  DBUG_RETURN(ret);
}

int Trans_delegate::before_rollback(THD *thd, bool all)
{
  DBUG_ENTER("Trans_delegate::before_rollback");
  Trans_param param;
  TRANS_PARAM_ZERO(param);
  param.server_id= thd->server_id;
  param.server_uuid= server_uuid;
  param.thread_id= thd->thread_id();

  bool is_real_trans=
    (all || !thd->get_transaction()->is_active(Transaction_ctx::SESSION));
  if (is_real_trans)
    param.flags|= TRANS_IS_REAL_TRANS;

  int ret= 0;
  FOREACH_OBSERVER(ret, before_rollback, thd, (&param));
  DBUG_RETURN(ret);
}

int Trans_delegate::after_commit(THD *thd, bool all)
{
  DBUG_ENTER("Trans_delegate::after_commit");
  Trans_param param;
  TRANS_PARAM_ZERO(param);
  param.server_uuid= server_uuid;
  param.thread_id= thd->thread_id();

  bool is_real_trans=
    (all || !thd->get_transaction()->is_active(Transaction_ctx::SESSION));
  if (is_real_trans)
    param.flags|= TRANS_IS_REAL_TRANS;

  thd->get_trans_fixed_pos(&param.log_file, &param.log_pos);
  param.server_id= thd->server_id;

  DBUG_PRINT("enter", ("log_file: %s, log_pos: %llu", param.log_file, param.log_pos));
  DEBUG_SYNC(thd, "before_call_after_commit_observer");

  int ret= 0;
  FOREACH_OBSERVER(ret, after_commit, thd, (&param));
  DBUG_RETURN(ret);
}

int Trans_delegate::after_rollback(THD *thd, bool all)
{
  DBUG_ENTER("Trans_delegate::after_rollback");
  Trans_param param;
  TRANS_PARAM_ZERO(param);
  param.server_uuid= server_uuid;
  param.thread_id= thd->thread_id();

  bool is_real_trans=
    (all || !thd->get_transaction()->is_active(Transaction_ctx::SESSION));
  if (is_real_trans)
    param.flags|= TRANS_IS_REAL_TRANS;
  thd->get_trans_fixed_pos(&param.log_file, &param.log_pos);
  param.server_id= thd->server_id;

  int ret= 0;
  FOREACH_OBSERVER(ret, after_rollback, thd, (&param));
  DBUG_RETURN(ret);
}

int Binlog_storage_delegate::after_flush(THD *thd,
                                         const char *log_file,
                                         my_off_t log_pos)
{
  DBUG_ENTER("Binlog_storage_delegate::after_flush");
  DBUG_PRINT("enter", ("log_file: %s, log_pos: %llu",
                       log_file, (ulonglong) log_pos));
  Binlog_storage_param param;
  param.server_id= thd->server_id;

  int ret= 0;
  FOREACH_OBSERVER(ret, after_flush, thd, (&param, log_file, log_pos));
  DBUG_RETURN(ret);
}

/**
  * This hook MUST be invoked after ALL recovery operations are performed
  * and the server is ready to serve clients.
  *
  * @param[in] thd The thread context.
  * @return 0 on success, >0 otherwise.
*/
int Server_state_delegate::before_handle_connection(THD *thd)
{
  DBUG_ENTER("Server_state_delegate::before_client_connection");
  Server_state_param param;

  int ret= 0;
  FOREACH_OBSERVER(ret, before_handle_connection, thd, (&param));
  DBUG_RETURN(ret);

}

/**
  * This hook MUST be invoked before ANY recovery action is started.
  *
  * @param[in] thd The thread context.
  * @return 0 on success, >0 otherwise.
*/
int Server_state_delegate::before_recovery(THD *thd)
{
  DBUG_ENTER("Server_state_delegate::before_recovery");
  Server_state_param param;

  int ret= 0;
  FOREACH_OBSERVER(ret, before_recovery, thd, (&param));
  DBUG_RETURN(ret);
}

/**
  * This hook MUST be invoked after the recovery from the engine
  * is complete.
  *
  * @param[in] thd The thread context.
  * @return 0 on success, >0 otherwise.
*/
int Server_state_delegate::after_engine_recovery(THD *thd)
{
  DBUG_ENTER("Server_state_delegate::after_engine_recovery");
  Server_state_param param;

  int ret= 0;
  FOREACH_OBSERVER(ret, after_engine_recovery, thd, (&param));
  DBUG_RETURN(ret);

}

/**
  * This hook MUST be invoked after the server has completed the
  * local recovery. The server can proceed with the further operations
  * like engaging in distributed recovery etc.
  *
  * @param[in] thd The thread context.
  * @return 0 on success, >0 otherwise.
*/
int Server_state_delegate::after_recovery(THD *thd)
{
  DBUG_ENTER("Server_state_delegate::after_recovery");
  Server_state_param param;

  int ret= 0;
  FOREACH_OBSERVER(ret, after_recovery, thd, (&param));
  DBUG_RETURN(ret);
}

/**
  * This hook MUST be invoked before server shutdown action is
  * initiated.
  *
  * @param[in] thd The thread context.
  * @return 0 on success, >0 otherwise.
*/
int Server_state_delegate::before_server_shutdown(THD *thd)
{
  DBUG_ENTER("Server_state_delegate::before_server_shutdown");
  Server_state_param param;

  int ret= 0;
  FOREACH_OBSERVER(ret, before_server_shutdown, thd, (&param));
  DBUG_RETURN(ret);
}

/**
  * This hook MUST be invoked after server shutdown operation is
  * complete.
  *
  * @param[in] thd The thread context.
  * @return 0 on success, >0 otherwise.
*/
int Server_state_delegate::after_server_shutdown(THD *thd)
{
  DBUG_ENTER("Server_state_delegate::after_server_shutdown");
  Server_state_param param;

  int ret= 0;
  FOREACH_OBSERVER(ret, after_server_shutdown, thd, (&param));
  DBUG_RETURN(ret);
}

int Binlog_storage_delegate::after_sync(THD *thd,
                                        const char *log_file,
                                        my_off_t log_pos)
{
  DBUG_ENTER("Binlog_storage_delegate::after_sync");
  DBUG_PRINT("enter", ("log_file: %s, log_pos: %llu",
                       log_file, (ulonglong) log_pos));
  Binlog_storage_param param;
  param.server_id= thd->server_id;

  DBUG_ASSERT(log_pos != 0);
  int ret= 0;
  FOREACH_OBSERVER(ret, after_sync, thd, (&param, log_file, log_pos));

  DEBUG_SYNC(thd, "after_call_after_sync_observer");
  DBUG_RETURN(ret);
}

#ifdef HAVE_REPLICATION
int Binlog_transmit_delegate::transmit_start(THD *thd, ushort flags,
                                             const char *log_file,
                                             my_off_t log_pos,
                                             bool *observe_transmission)
{
  Binlog_transmit_param param;
  param.flags= flags;
  param.server_id= thd->server_id;

  int ret= 0;
  FOREACH_OBSERVER(ret, transmit_start, thd, (&param, log_file, log_pos));
  *observe_transmission= param.should_observe();
  return ret;
}

int Binlog_transmit_delegate::transmit_stop(THD *thd, ushort flags)
{
  Binlog_transmit_param param;
  param.flags= flags;
  param.server_id= thd->server_id;

  DBUG_EXECUTE_IF("crash_binlog_transmit_hook", DBUG_SUICIDE(););

  int ret= 0;
  FOREACH_OBSERVER(ret, transmit_stop, thd, (&param));
  return ret;
}

int Binlog_transmit_delegate::reserve_header(THD *thd, ushort flags,
                                             String *packet)
{
  /* NOTE2ME: Maximum extra header size for each observer, I hope 32
     bytes should be enough for each Observer to reserve their extra
     header. If later found this is not enough, we can increase this
     /HEZX
  */
#define RESERVE_HEADER_SIZE 32
  unsigned char header[RESERVE_HEADER_SIZE];
  ulong hlen;
  Binlog_transmit_param param;
  param.flags= flags;
  param.server_id= thd->server_id;

  DBUG_EXECUTE_IF("crash_binlog_transmit_hook", DBUG_SUICIDE(););

  int ret= 0;
  read_lock();
  Observer_info_iterator iter= observer_info_iter();
  Observer_info *info= iter++;
  for (; info; info= iter++)
  {
    plugin_ref plugin=
      my_plugin_lock(thd, &info->plugin);
    if (!plugin)
    {
      ret= 1;
      break;
    }
    hlen= 0;
    if (((Observer *)info->observer)->reserve_header
        && ((Observer *)info->observer)->reserve_header(&param,
                                                        header,
                                                        RESERVE_HEADER_SIZE,
                                                        &hlen))
    {
      ret= 1;
      plugin_unlock(thd, plugin);
      break;
    }
    plugin_unlock(thd, plugin);
    if (hlen == 0)
      continue;
    if (hlen > RESERVE_HEADER_SIZE || packet->append((char *)header, hlen))
    {
      ret= 1;
      break;
    }
  }
  unlock();
  return ret;
}

int Binlog_transmit_delegate::before_send_event(THD *thd, ushort flags,
                                                String *packet,
                                                const char *log_file,
                                                my_off_t log_pos)
{
  Binlog_transmit_param param;
  param.flags= flags;
  param.server_id= thd->server_id;

  DBUG_EXECUTE_IF("crash_binlog_transmit_hook", DBUG_SUICIDE(););

  int ret= 0;
  FOREACH_OBSERVER(ret, before_send_event, thd,
                   (&param, (uchar *)packet->ptr(),
                    packet->length(),
                    log_file+dirname_length(log_file), log_pos));
  return ret;
}

int Binlog_transmit_delegate::after_send_event(THD *thd, ushort flags,
                                               String *packet,
                                               const char *skipped_log_file,
                                               my_off_t skipped_log_pos)
{
  Binlog_transmit_param param;
  param.flags= flags;
  param.server_id= thd->server_id;

  DBUG_EXECUTE_IF("crash_binlog_transmit_hook", DBUG_SUICIDE(););

  int ret= 0;
  FOREACH_OBSERVER(ret, after_send_event, thd,
                   (&param, packet->ptr(), packet->length(),
                   skipped_log_file+dirname_length(skipped_log_file),
                    skipped_log_pos));
  return ret;
}

int Binlog_transmit_delegate::after_reset_master(THD *thd, ushort flags)

{
  Binlog_transmit_param param;
  param.flags= flags;
  param.server_id= thd->server_id;

  int ret= 0;
  FOREACH_OBSERVER(ret, after_reset_master, thd, (&param));
  return ret;
}

void Binlog_relay_IO_delegate::init_param(Binlog_relay_IO_param *param,
                                          Master_info *mi)
{
  param->mysql= mi->mysql;
  param->channel_name= mi->get_channel();
  param->user= const_cast<char *>(mi->get_user());
  param->host= mi->host;
  param->port= mi->port;
  param->master_log_name= const_cast<char *>(mi->get_master_log_name());
  param->master_log_pos= mi->get_master_log_pos();
}

int Binlog_relay_IO_delegate::thread_start(THD *thd, Master_info *mi)
{
  Binlog_relay_IO_param param;
  init_param(&param, mi);
  param.server_id= thd->server_id;
  param.thread_id= thd->thread_id();

  int ret= 0;
  FOREACH_OBSERVER(ret, thread_start, thd, (&param));
  return ret;
}


int Binlog_relay_IO_delegate::thread_stop(THD *thd, Master_info *mi)
{

  Binlog_relay_IO_param param;
  init_param(&param, mi);
  param.server_id= thd->server_id;
  param.thread_id= thd->thread_id();

  int ret= 0;
  FOREACH_OBSERVER(ret, thread_stop, thd, (&param));
  return ret;
}

int Binlog_relay_IO_delegate::applier_start(THD *thd, Master_info *mi)
{
  Binlog_relay_IO_param param;
  init_param(&param, mi);
  param.server_id= thd->server_id;
  param.thread_id= thd->thread_id();

  int ret= 0;
  FOREACH_OBSERVER(ret, applier_start, thd, (&param));
  return ret;
}

int Binlog_relay_IO_delegate::applier_stop(THD *thd,
                                           Master_info *mi,
                                           bool aborted)
{
  Binlog_relay_IO_param param;
  init_param(&param, mi);
  param.server_id= thd->server_id;
  param.thread_id= thd->thread_id();

  int ret= 0;
  FOREACH_OBSERVER(ret, applier_stop, thd, (&param, aborted));
  return ret;
}

int Binlog_relay_IO_delegate::before_request_transmit(THD *thd,
                                                      Master_info *mi,
                                                      ushort flags)
{
  Binlog_relay_IO_param param;
  init_param(&param, mi);
  param.server_id= thd->server_id;
  param.thread_id= thd->thread_id();

  int ret= 0;
  FOREACH_OBSERVER(ret, before_request_transmit, thd, (&param, (uint32)flags));
  return ret;
}

int Binlog_relay_IO_delegate::after_read_event(THD *thd, Master_info *mi,
                                               const char *packet, ulong len,
                                               const char **event_buf,
                                               ulong *event_len)
{
  Binlog_relay_IO_param param;
  init_param(&param, mi);
  param.server_id= thd->server_id;
  param.thread_id= thd->thread_id();

  int ret= 0;
  FOREACH_OBSERVER(ret, after_read_event, thd,
                   (&param, packet, len, event_buf, event_len));
  return ret;
}

int Binlog_relay_IO_delegate::after_queue_event(THD *thd, Master_info *mi,
                                                const char *event_buf,
                                                ulong event_len,
                                                bool synced)
{
  Binlog_relay_IO_param param;
  init_param(&param, mi);
  param.server_id= thd->server_id;
  param.thread_id= thd->thread_id();

  uint32 flags=0;
  if (synced)
    flags |= BINLOG_STORAGE_IS_SYNCED;

  int ret= 0;
  FOREACH_OBSERVER(ret, after_queue_event, thd,
                   (&param, event_buf, event_len, flags));
  return ret;
}

int Binlog_relay_IO_delegate::after_reset_slave(THD *thd, Master_info *mi)

{
  Binlog_relay_IO_param param;
  init_param(&param, mi);
  param.server_id= thd->server_id;
  param.thread_id= thd->thread_id();

  int ret= 0;
  FOREACH_OBSERVER(ret, after_reset_slave, thd, (&param));
  return ret;
}
#endif /* HAVE_REPLICATION */

int register_trans_observer(Trans_observer *observer, void *p)
{
  return transaction_delegate->add_observer(observer, (st_plugin_int *)p);
}

int unregister_trans_observer(Trans_observer *observer, void *p)
{
  return transaction_delegate->remove_observer(observer, (st_plugin_int *)p);
}

int register_binlog_storage_observer(Binlog_storage_observer *observer, void *p)
{
  DBUG_ENTER("register_binlog_storage_observer");
  int result= binlog_storage_delegate->add_observer(observer, (st_plugin_int *)p);
  DBUG_RETURN(result);
}

int unregister_binlog_storage_observer(Binlog_storage_observer *observer, void *p)
{
  return binlog_storage_delegate->remove_observer(observer, (st_plugin_int *)p);
}

int register_server_state_observer(Server_state_observer *observer, void *plugin_var)
{
  DBUG_ENTER("register_server_state_observer");
  int result= server_state_delegate->add_observer(observer, (st_plugin_int *)plugin_var);
  DBUG_RETURN(result);
}

int unregister_server_state_observer(Server_state_observer *observer, void *plugin_var)
{
  DBUG_ENTER("unregister_server_state_observer");
  int result= server_state_delegate->remove_observer(observer, (st_plugin_int *)plugin_var);
  DBUG_RETURN(result);
}

#ifdef HAVE_REPLICATION
int register_binlog_transmit_observer(Binlog_transmit_observer *observer, void *p)
{
  return binlog_transmit_delegate->add_observer(observer, (st_plugin_int *)p);
}

int unregister_binlog_transmit_observer(Binlog_transmit_observer *observer, void *p)
{
  return binlog_transmit_delegate->remove_observer(observer, (st_plugin_int *)p);
}

int register_binlog_relay_io_observer(Binlog_relay_IO_observer *observer, void *p)
{
  return binlog_relay_io_delegate->add_observer(observer, (st_plugin_int *)p);
}

int unregister_binlog_relay_io_observer(Binlog_relay_IO_observer *observer, void *p)
{
  return binlog_relay_io_delegate->remove_observer(observer, (st_plugin_int *)p);
}
#endif /* HAVE_REPLICATION */
