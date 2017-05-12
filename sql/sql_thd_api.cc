/*
   Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <string.h>
#include <sys/types.h>
#include <algorithm>

#include "binlog_event.h"
#include "channel_info.h"
#include "connection_handler_manager.h"
#include "current_thd.h"                // current_thd
#include "handler.h"
#include "key.h"
#include "lex_string.h"
#include "m_ctype.h"
#include "my_dbug.h"
#include "my_io.h"
#include "my_macros.h"
#include "my_sqlcommand.h"
#include "my_thread.h"
#include "my_thread_local.h"
#include "mysql/mysql_lex_string.h"
#include "mysql/plugin.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/psi/psi_stage.h"
#include "mysql/psi/psi_thread.h"
#include "mysql/service_my_snprintf.h"
#include "mysql_com.h"
#include "mysqld.h"                     // key_thread_one_connection
#include "protocol_classic.h"
#include "query_options.h"
#include "rpl_rli.h"                    // is_mts_worker
#include "rpl_slave_commit_order_manager.h"
#include "session_tracker.h"
#include "sql_alter.h"
                                        // commit_order_manager_check_deadlock
#include "sql_cache.h"                  // query_cache
#include "sql_callback.h"               // MYSQL_CALLBACK
#include "sql_class.h"                  // THD
#include "sql_error.h"
#include "sql_lex.h"
#include "sql_plugin.h"                 // plugin_unlock
#include "sql_plugin_ref.h"
#include "sql_security_ctx.h"
#include "sql_string.h"
#include "sql_table.h"                  // filename_to_tablename
#include "sql_thd_internal_api.h"
#include "system_variables.h"
#include "transaction_info.h"
#include "violite.h"
#include "xa.h"

using std::min;


//////////////////////////////////////////////////////////
//
//  Defintions of functions declared in thread_pool_priv.h
//
//////////////////////////////////////////////////////////

/**
  Get reference to scheduler data object

  @param thd            THD object

  @retval               Scheduler data object on THD
*/

void *thd_get_scheduler_data(THD *thd)
{
  return thd->scheduler.data;
}


/**
  Set reference to Scheduler data object for THD object

  @param thd            THD object
  @param data           Scheduler data object to set on THD
*/

void thd_set_scheduler_data(THD *thd, void *data)
{
  thd->scheduler.data= data;
}


/**
  Get reference to Performance Schema object for THD object

  @param thd            THD object

  @retval               Performance schema object for thread on THD
*/

PSI_thread *thd_get_psi(THD *thd)
{
  return thd->get_psi();
}


/**
  Get net_wait_timeout for THD object

  @param thd            THD object

  @retval               net_wait_timeout value for thread on THD
*/

ulong thd_get_net_wait_timeout(THD* thd)
{
  return thd->variables.net_wait_timeout;
}


/**
  Set reference to Performance Schema object for THD object

  @param thd            THD object
  @param psi            Performance schema object for thread
*/

void thd_set_psi(THD *thd, PSI_thread *psi)
{
  thd->set_psi(psi);
}


/**
  Set the state on connection to killed

  @param thd               THD object
*/

void thd_set_killed(THD *thd)
{
  thd->killed= THD::KILL_CONNECTION;
}


/**
  Clear errors from the previous THD

  @param thd              THD object
*/

void thd_clear_errors(THD *thd MY_ATTRIBUTE((unused)))
{
  set_my_errno(0);
}


/**
  Close the socket used by this connection

  @param thd                THD object
*/

void thd_close_connection(THD *thd)
{
  thd->get_protocol_classic()->shutdown();
}


/**
  Get current THD object from thread local data

  @retval     The THD object for the thread, NULL if not connection thread
*/

THD *thd_get_current_thd()
{
  return current_thd;
}


/**
  Reset thread globals associated.

  @param thd     THD object
*/

void reset_thread_globals(THD* thd)
{
  thd->restore_globals();
  thd->set_is_killable(false);
}


/**
  Lock data that needs protection in THD object

  @param thd                   THD object
*/

void thd_lock_data(THD *thd)
{
  mysql_mutex_lock(&thd->LOCK_thd_data);
}


/**
  Unlock data that needs protection in THD object

  @param thd                   THD object
*/

void thd_unlock_data(THD *thd)
{
  mysql_mutex_unlock(&thd->LOCK_thd_data);
}


/**
  Support method to check if connection has already started transaction

  @param thd Current thread

  @retval               TRUE if connection already started transaction
*/

bool thd_is_transaction_active(THD *thd)
{
  return thd->get_transaction()->is_active(Transaction_ctx::SESSION);
}


/**
  Check if there is buffered data on the socket representing the connection

  @param thd                  THD object
*/

int thd_connection_has_data(THD *thd)
{
  Vio *vio= thd->get_protocol_classic()->get_vio();
  return vio->has_data(vio);
}


/**
  Get reading/writing on socket from THD object
  @param thd                       THD object

  @retval               net.reading_or_writing value for thread on THD.
*/

uint thd_get_net_read_write(THD *thd)
{
  return thd->get_protocol_classic()->get_rw_status();
}


/**
  Set reading/writing on socket, used by SHOW PROCESSLIST

  @param thd                       THD object
  @param val                       Value to set it to (0 or 1)
*/

void thd_set_net_read_write(THD *thd, uint val)
{
  thd->get_protocol_classic()->get_net()->reading_or_writing= val;
}


/**
  Mark the THD as not killable as it is not currently used by a thread.

  @param thd             THD object
*/

void thd_set_not_killable(THD *thd)
{
  thd->set_is_killable(false);
}


/**
  Get socket file descriptor for this connection

  @param thd            THD object

  @retval               Socket of the connection
*/

my_socket thd_get_fd(THD *thd)
{
  return thd->get_protocol_classic()->get_socket();
}


/**
  Set thread specific environment required for thd cleanup in thread pool.

  @param thd            THD object

  @retval               1 if thread-specific enviroment could be set else 0
*/

int thd_store_globals(THD *thd)
{
  return thd->store_globals();
}


/**
  Get thread attributes for connection threads

  @retval      Reference to thread attribute for connection threads
*/

my_thread_attr_t *get_connection_attrib()
{
  return &connection_attrib;
}


/**
  Get max number of connections

  @retval         Max number of connections for MySQL Server
*/

ulong get_max_connections()
{
  return max_connections;
}


//////////////////////////////////////////////////////////
//
//  Definitions of functions declared in plugin.h
//
//////////////////////////////////////////////////////////


extern "C"
void thd_binlog_pos(const MYSQL_THD thd,
                    const char **file_var,
                    unsigned long long *pos_var)
{
  thd->get_trans_pos(file_var, pos_var);
}


extern "C"
int mysql_tmpfile(const char *prefix)
{
  return mysql_tmpfile_path(mysql_tmpdir, prefix);
}


extern "C"
int thd_in_lock_tables(const MYSQL_THD thd)
{
  return MY_TEST(thd->in_lock_tables);
}


extern "C"
int thd_tablespace_op(const MYSQL_THD thd)
{
  /*
    The Alter_info is reset only at the beginning of an ALTER
    statement, so this function must check both the SQL command
    code and the Alter_info::flags.
  */
  return MY_TEST(thd->lex->sql_command == SQLCOM_ALTER_TABLE &&
                 (thd->lex->alter_info.flags &
                  (Alter_info::ALTER_DISCARD_TABLESPACE |
                   Alter_info::ALTER_IMPORT_TABLESPACE)));
}


static void set_thd_stage_info(MYSQL_THD thd,
                               const PSI_stage_info *new_stage,
                               PSI_stage_info *old_stage,
                               const char *calling_func,
                               const char *calling_file,
                               const unsigned int calling_line)
{
  if (thd == NULL)
    thd= current_thd;

  thd->enter_stage(new_stage, old_stage, calling_func, calling_file, calling_line);
}


extern "C"
const char *set_thd_proc_info(MYSQL_THD thd_arg, const char *info,
                              const char *calling_function,
                              const char *calling_file,
                              const unsigned int calling_line)
{
  PSI_stage_info old_stage;
  PSI_stage_info new_stage;

  old_stage.m_key= 0;
  old_stage.m_name= info;

  set_thd_stage_info(thd_arg, & old_stage, & new_stage,
                     calling_function, calling_file, calling_line);

  return new_stage.m_name;
}


extern "C"
void **thd_ha_data(const MYSQL_THD thd, const struct handlerton *hton)
{
  return &(const_cast<THD*>(thd))->get_ha_data(hton->slot)->ha_ptr;
}


extern "C"
void thd_storage_lock_wait(MYSQL_THD thd, long long value)
{
  thd->utime_after_lock+= value;
}


/**
  Provide a handler data getter to simplify coding
*/
extern "C"
void *thd_get_ha_data(const MYSQL_THD thd, const struct handlerton *hton)
{
  return *thd_ha_data(thd, hton);
}


/**
  Provide a handler data setter to simplify coding
  @see thd_set_ha_data() definition in plugin.h
*/
extern "C"
void thd_set_ha_data(MYSQL_THD thd, const struct handlerton *hton,
                     const void *ha_data)
{
  plugin_ref *lock= &thd->get_ha_data(hton->slot)->lock;
  if (ha_data && !*lock)
    *lock= ha_lock_engine(NULL, (handlerton*) hton);
  else if (!ha_data && *lock)
  {
    plugin_unlock(NULL, *lock);
    *lock= NULL;
  }
  *thd_ha_data(thd, hton)= (void*) ha_data;
}


extern "C"
long long thd_test_options(const MYSQL_THD thd, long long test_options)
{
  return thd->variables.option_bits & test_options;
}


extern "C"
int thd_sql_command(const MYSQL_THD thd)
{
  return (int) thd->lex->sql_command;
}


extern "C"
int thd_tx_isolation(const MYSQL_THD thd)
{
  return (int) thd->tx_isolation;
}


extern "C"
int thd_tx_is_read_only(const MYSQL_THD thd)
{
  return (int) thd->tx_read_only;
}


extern "C"
int thd_tx_priority(const MYSQL_THD thd)
{
  return (thd->thd_tx_priority != 0
          ? thd->thd_tx_priority
          : thd->tx_priority);
}


extern "C"
MYSQL_THD thd_tx_arbitrate(MYSQL_THD requestor, MYSQL_THD holder)
{
 /* Should be different sessions. */
 DBUG_ASSERT(holder != requestor);

 return(thd_tx_priority(requestor) == thd_tx_priority(holder)
	? requestor
	: ((thd_tx_priority(requestor)
	    > thd_tx_priority(holder)) ? holder : requestor));
}


extern "C"
int thd_tx_is_dd_trx(const MYSQL_THD thd)
{
  return (int) thd->is_attachable_ro_transaction_active();
}


extern "C"
void thd_inc_row_count(MYSQL_THD thd)
{
  thd->get_stmt_da()->inc_current_row_for_condition();
}


/**
  Dumps a text description of a thread, its security context
  (user, host) and the current query.

  @param thd thread context
  @param buffer pointer to preferred result buffer
  @param length length of buffer
  @param max_query_len how many chars of query to copy (0 for all)

  @return Pointer to string
*/

extern "C"
char *thd_security_context(MYSQL_THD thd, char *buffer, size_t length,
                           size_t max_query_len)
{
  String str(buffer, length, &my_charset_latin1);
  Security_context *sctx= &thd->m_main_security_ctx;
  char header[256];
  size_t len;
  /*
    The pointers thd->query and thd->proc_info might change since they are
    being modified concurrently. This is acceptable for proc_info since its
    values doesn't have to very accurate and the memory it points to is static,
    but we need to attempt a snapshot on the pointer values to avoid using NULL
    values. The pointer to thd->query however, doesn't point to static memory
    and has to be protected by LOCK_thd_query or risk pointing to
    uninitialized memory.
  */
  const char *proc_info= thd->proc_info;

  len= my_snprintf(header, sizeof(header),
                   "MySQL thread id %u, OS thread handle %lu, query id %lu",
                   thd->thread_id(), (ulong)thd->real_id, (ulong)thd->query_id);
  str.length(0);
  str.append(header, len);

  if (sctx->host().length)
  {
    str.append(' ');
    str.append(sctx->host().str);
  }

  if (sctx->ip().length)
  {
    str.append(' ');
    str.append(sctx->ip().str);
  }

  if (sctx->user().str)
  {
    str.append(' ');
    str.append(sctx->user().str);
  }

  if (proc_info)
  {
    str.append(' ');
    str.append(proc_info);
  }

  mysql_mutex_lock(&thd->LOCK_thd_query);

  if (thd->query().str)
  {
    if (max_query_len < 1)
      len= thd->query().length;
    else
      len= min(thd->query().length, max_query_len);
    str.append('\n');
    str.append(thd->query().str, len);
  }

  mysql_mutex_unlock(&thd->LOCK_thd_query);

  if (str.c_ptr_safe() == buffer)
    return buffer;

  /*
    We have to copy the new string to the destination buffer because the string
    was reallocated to a larger buffer to be able to fit.
  */
  DBUG_ASSERT(buffer != NULL);
  length= min(str.length(), length-1);
  memcpy(buffer, str.c_ptr_quick(), length);
  /* Make sure that the new string is null terminated */
  buffer[length]= '\0';
  return buffer;
}


extern "C"
void thd_get_xid(const MYSQL_THD thd, MYSQL_XID *xid)
{
  *xid = *(MYSQL_XID *) thd->get_transaction()->xid_state()->get_xid();
}


/**
  Check the killed state of a user thread
  @param thd  user thread
  @retval 0 the user thread is active
  @retval 1 the user thread has been killed
*/

extern "C"
int thd_killed(const MYSQL_THD thd)
{
  if (thd == NULL)
    return current_thd != NULL ? current_thd->killed : 0;
  return thd->killed;
}


/**
  Set the killed status of the current statement.

  @param thd  user thread connection handle
*/

extern "C"
void thd_set_kill_status(const MYSQL_THD thd)
{
  thd->send_kill_message();
}


/**
  Return the thread id of a user thread
  @param thd user thread
  @return thread id
*/

extern "C"
unsigned long thd_get_thread_id(const MYSQL_THD thd)
{
  return((unsigned long)thd->thread_id());
}


/**
  Check if batching is allowed for the thread
  @param thd  user thread
  @retval 1 batching allowed
  @retval 0 batching not allowed
*/

extern "C"
int thd_allow_batch(MYSQL_THD thd)
{
  if ((thd->variables.option_bits & OPTION_ALLOW_BATCH) ||
      (thd->slave_thread && opt_slave_allow_batching))
    return 1;
  return 0;
}


extern "C"
void thd_mark_transaction_to_rollback(MYSQL_THD thd, int all)
{
  DBUG_ENTER("thd_mark_transaction_to_rollback");
  DBUG_ASSERT(thd);
  /*
    The parameter "all" has type int since the function is defined
    in plugin.h. The corresponding parameter in the call below has
    type bool. The comment in plugin.h states that "all != 0"
    means to rollback the main transaction. Thus, check this
    specifically.
  */
  thd->mark_transaction_to_rollback((all != 0));
  DBUG_VOID_RETURN;
}


/**
  This is a convenience function used by the innodb plugin.
*/
extern "C"
void mysql_query_cache_invalidate4(THD *thd,
                                   const char *key,
                                   unsigned key_length MY_ATTRIBUTE((unused)),
                                   int using_trx)
{
  char qcache_key_name[2 * (NAME_LEN + 1)];
  char db_name[NAME_CHAR_LEN * FILENAME_CHARSET_MBMAXLEN + 1];
  const char *key_ptr;
  size_t tabname_len, dbname_len;

  // Extract the database name.
  key_ptr= strchr(key, '/');
  memcpy(db_name, key, (key_ptr - key));
  db_name[(key_ptr - key)]= '\0';

  /*
    Construct the key("db@002dname\0table@0024name\0") in a canonical format for
    the query cache using the key("db-name\0table$name\0") which is
    in its non-canonical form.
  */
  dbname_len= filename_to_tablename(db_name, qcache_key_name,
                                    sizeof(qcache_key_name));
  tabname_len= filename_to_tablename(++key_ptr,
                                     (qcache_key_name + dbname_len + 1),
                                     sizeof(qcache_key_name) - dbname_len - 1);

  query_cache.invalidate(thd, qcache_key_name, (dbname_len + tabname_len + 2),
                         using_trx);
}


//////////////////////////////////////////////////////////
//
//  Definitions of functions declared in service_thd_alloc.h
//
//////////////////////////////////////////////////////////


extern "C"
void *thd_alloc(MYSQL_THD thd, size_t size)
{
  return thd->alloc(size);
}


extern "C"
void *thd_calloc(MYSQL_THD thd, size_t size)
{
  return thd->mem_calloc(size);
}


extern "C"
char *thd_strdup(MYSQL_THD thd, const char *str)
{
  return thd->mem_strdup(str);
}


extern "C"
char *thd_strmake(MYSQL_THD thd, const char *str, size_t size)
{
  return thd->strmake(str, size);
}


extern "C"
MYSQL_LEX_STRING *thd_make_lex_string(MYSQL_THD thd,
                                      MYSQL_LEX_STRING *lex_str,
                                      const char *str, size_t size,
                                      int allocate_lex_string)
{
  return thd->make_lex_string(lex_str, str, size,
                              (bool) allocate_lex_string);
}


extern "C"
void *thd_memdup(MYSQL_THD thd, const void* str, size_t size)
{
  return thd->memdup(str, size);
}


//////////////////////////////////////////////////////////
//
//  Definitions of functions declared in service_thd_wait.h
//
//////////////////////////////////////////////////////////

/*
  Interface for MySQL Server, plugins and storage engines to report
  when they are going to sleep/stall.

  SYNOPSIS
  thd_wait_begin()
  thd                     Thread object
  wait_type               Type of wait
                          1 -- short wait (e.g. for mutex)
                          2 -- medium wait (e.g. for disk io)
                          3 -- large wait (e.g. for locked row/table)
  NOTES
    This is used by the threadpool to have better knowledge of which
    threads that currently are actively running on CPUs. When a thread
    reports that it's going to sleep/stall, the threadpool scheduler is
    free to start another thread in the pool most likely. The expected wait
    time is simply an indication of how long the wait is expected to
    become, the real wait time could be very different.

  thd_wait_end MUST be called immediately after waking up again.
*/
extern "C" void thd_wait_begin(MYSQL_THD thd, int wait_type)
{
  MYSQL_CALLBACK(Connection_handler_manager::event_functions,
                 thd_wait_begin, (thd, wait_type));
}

/**
  Interface for MySQL Server, plugins and storage engines to report
  when they waking up from a sleep/stall.

  @param  thd   Thread handle
*/
extern "C" void thd_wait_end(MYSQL_THD thd)
{
  MYSQL_CALLBACK(Connection_handler_manager::event_functions,
                 thd_wait_end, (thd));
}


//////////////////////////////////////////////////////////
//
//  Definitions of functions declared in service_thd_engine_lock.h
//
//////////////////////////////////////////////////////////

/**
   Interface for Engine to report row lock conflict.
   The caller should guarantee thd_wait_for does not be freed, when it is
   called.
*/
extern "C"
void thd_report_row_lock_wait(THD* self, THD *wait_for)
{
  DBUG_ENTER("thd_report_row_lock_wait");

  if (self != NULL && wait_for != NULL &&
      is_mts_worker(self) && is_mts_worker(wait_for))
    commit_order_manager_check_deadlock(self, wait_for);

  DBUG_VOID_RETURN;
}
