/*
   Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"
#include "mysql/plugin.h"
#include "mysql/service_thd_alloc.h"
#include "mysql/thread_pool_priv.h"
#include "current_thd.h"                // current_thd
#include "mysqld_thd_manager.h"         // Global_THD_manager
#include "sql_class.h"                  // THD
#include "sql_plugin.h"                 // plugin_unlock
#include "mysqld.h"                     // key_thread_one_connection

#include <algorithm>
using std::min;


//////////////////////////////////////////////////////////
//
//  Defintions of functions declared in thread_pool_priv.h
//
//////////////////////////////////////////////////////////

/**
  Release resources of the THD, prior to destruction.

  @param    THD   pointer to THD object.
*/

void thd_release_resources(THD *thd)
{
  thd->release_resources();
}


/**
  Delete the THD object.

  @param    THD   pointer to THD object.
*/

void destroy_thd(THD *thd)
{
  delete thd;
}


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
  @param psi            Scheduler data object to set on THD
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
  return thd->scheduler.m_psi;
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
  thd->scheduler.m_psi= psi;
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

void thd_clear_errors(THD *thd)
{
  my_errno= 0;
  thd->mysys_var->abort= 0;
}


/**
  Set thread stack in THD object

  @param thd              Thread object
  @param stack_start      Start of stack to set in THD object
*/

void thd_set_thread_stack(THD *thd, char *stack_start)
{
  thd->thread_stack= stack_start;
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
  thd->set_mysys_var(NULL);
}


/**
  Set up various THD data for a new connection

  thd_new_connection_setup

  @param              thd            THD object
  @param              stack_start    Start of stack for connection
*/

void thd_new_connection_setup(THD *thd, char *stack_start)
{
  DBUG_ENTER("thd_new_connection_setup");
  thd->set_new_thread_id();
#ifdef HAVE_PSI_INTERFACE
  thd_set_psi(thd,
              PSI_THREAD_CALL(new_thread)
              (key_thread_one_connection, thd, thd->thread_id()));
#endif
  thd->set_time();
  thd->thr_create_utime= thd->start_utime= my_micro_time();

  Global_THD_manager *thd_manager= Global_THD_manager::get_instance();
  thd_manager->add_thd(thd);

  DBUG_PRINT("info", ("init new connection. thd: 0x%lx fd: %d",
          (ulong)thd, mysql_socket_getfd(
            thd->get_protocol_classic()->get_vio()->mysql_socket)));
  thd_set_thread_stack(thd, stack_start);
  DBUG_VOID_RETURN;
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

  @param client_cntx    Low level client context

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
  Set reference to mysys variable in THD object

  @param thd             THD object
  @param mysys_var       Reference to set
*/

void thd_set_mysys_var(THD *thd, st_my_thread_var *mysys_var)
{
  thd->set_mysys_var(mysys_var);
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
  char filename[FN_REFLEN];
  File fd = create_temp_file(filename, mysql_tmpdir, prefix,
#ifdef _WIN32
                             O_BINARY | O_TRUNC | O_SEQUENTIAL |
                             O_SHORT_LIVED |
#endif /* _WIN32 */
                             O_CREAT | O_EXCL | O_RDWR | O_TEMPORARY,
                             MYF(MY_WME));
  if (fd >= 0) {
#ifndef _WIN32
    /*
      This can be removed once the following bug is fixed:
      Bug #28903  create_temp_file() doesn't honor O_TEMPORARY option
                  (file not removed) (Unix)
    */
    unlink(filename);
#endif /* !_WIN32 */
  }

  return fd;
}


extern "C"
int thd_in_lock_tables(const MYSQL_THD thd)
{
  return MY_TEST(thd->in_lock_tables);
}


extern "C"
int thd_tablespace_op(const MYSQL_THD thd)
{
  return MY_TEST(thd->tablespace_op);
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
  return (void **) &thd->ha_data[hton->slot].ha_ptr;
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
  plugin_ref *lock= &thd->ha_data[hton->slot].lock;
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
  return (int) thd->is_attachable_transaction_active();
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
  length= min(static_cast<size_t>(str.length()), length-1);
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
  return(thd->killed);
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
