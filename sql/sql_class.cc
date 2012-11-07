/*
   Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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


/*****************************************************************************
**
** This file implements classes defined in sql_class.h
** Especially the classes to handle a result from a select
**
*****************************************************************************/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "my_global.h"                          /* NO_EMBEDDED_ACCESS_CHECKS */
#include "sql_priv.h"
#include "unireg.h"                    // REQUIRED: for other includes
#include "sql_class.h"
#include "sql_cache.h"                          // query_cache_abort
#include "sql_base.h"                           // close_thread_tables
#include "sql_time.h"                         // date_time_format_copy
#include "sql_acl.h"                          // NO_ACCESS,
                                              // acl_getroot_no_password
#include "sql_base.h"                         // close_temporary_tables
#include "sql_handler.h"                      // mysql_ha_cleanup
#include "rpl_rli.h"
#include "rpl_filter.h"
#include "rpl_record.h"
#include "slave.h"
#include <my_bitmap.h>
#include "log_event.h"
#include "sql_audit.h"
#include <m_ctype.h>
#include <sys/stat.h>
#include <thr_alarm.h>
#ifdef	__WIN__
#include <io.h>
#endif
#include <mysys_err.h>
#include <limits.h>

#include "sp_rcontext.h"
#include "sp_cache.h"
#include "transaction.h"
#include "debug_sync.h"
#include "sql_parse.h"                          // is_update_query
#include "sql_callback.h"

/*
  The following is used to initialise Table_ident with a internal
  table name
*/
char internal_table_name[2]= "*";
char empty_c_string[1]= {0};    /* used for not defined db */

const char * const THD::DEFAULT_WHERE= "field list";


/*****************************************************************************
** Instansiate templates
*****************************************************************************/

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
/* Used templates */
template class List<Key>;
template class List_iterator<Key>;
template class List<Key_part_spec>;
template class List_iterator<Key_part_spec>;
template class List<Alter_drop>;
template class List_iterator<Alter_drop>;
template class List<Alter_column>;
template class List_iterator<Alter_column>;
#endif

/****************************************************************************
** User variables
****************************************************************************/

extern "C" uchar *get_var_key(user_var_entry *entry, size_t *length,
                              my_bool not_used __attribute__((unused)))
{
  *length= entry->name.length;
  return (uchar*) entry->name.str;
}

extern "C" void free_user_var(user_var_entry *entry)
{
  char *pos= (char*) entry+ALIGN_SIZE(sizeof(*entry));
  if (entry->value && entry->value != pos)
    my_free(entry->value);
  my_free(entry);
}

bool Key_part_spec::operator==(const Key_part_spec& other) const
{
  return length == other.length &&
         !my_strcasecmp(system_charset_info, field_name.str,
                        other.field_name.str);
}

/**
  Construct an (almost) deep copy of this key. Only those
  elements that are known to never change are not copied.
  If out of memory, a partial copy is returned and an error is set
  in THD.
*/

Key::Key(const Key &rhs, MEM_ROOT *mem_root)
  :type(rhs.type),
  key_create_info(rhs.key_create_info),
  columns(rhs.columns, mem_root),
  name(rhs.name),
  generated(rhs.generated)
{
  list_copy_and_replace_each_value(columns, mem_root);
}

/**
  Construct an (almost) deep copy of this foreign key. Only those
  elements that are known to never change are not copied.
  If out of memory, a partial copy is returned and an error is set
  in THD.
*/

Foreign_key::Foreign_key(const Foreign_key &rhs, MEM_ROOT *mem_root)
  :Key(rhs,mem_root),
  ref_table(rhs.ref_table),
  ref_columns(rhs.ref_columns,mem_root),
  delete_opt(rhs.delete_opt),
  update_opt(rhs.update_opt),
  match_opt(rhs.match_opt)
{
  list_copy_and_replace_each_value(ref_columns, mem_root);
}

/*
  Test if a foreign key (= generated key) is a prefix of the given key
  (ignoring key name, key type and order of columns)

  NOTES:
    This is only used to test if an index for a FOREIGN KEY exists

  IMPLEMENTATION
    We only compare field names

  RETURN
    0	Generated key is a prefix of other key
    1	Not equal
*/

bool foreign_key_prefix(Key *a, Key *b)
{
  /* Ensure that 'a' is the generated key */
  if (a->generated)
  {
    if (b->generated && a->columns.elements > b->columns.elements)
      swap_variables(Key*, a, b);               // Put shorter key in 'a'
  }
  else
  {
    if (!b->generated)
      return TRUE;                              // No foreign key
    swap_variables(Key*, a, b);                 // Put generated key in 'a'
  }

  /* Test if 'a' is a prefix of 'b' */
  if (a->columns.elements > b->columns.elements)
    return TRUE;                                // Can't be prefix

  List_iterator<Key_part_spec> col_it1(a->columns);
  List_iterator<Key_part_spec> col_it2(b->columns);
  const Key_part_spec *col1, *col2;

#ifdef ENABLE_WHEN_INNODB_CAN_HANDLE_SWAPED_FOREIGN_KEY_COLUMNS
  while ((col1= col_it1++))
  {
    bool found= 0;
    col_it2.rewind();
    while ((col2= col_it2++))
    {
      if (*col1 == *col2)
      {
        found= TRUE;
	break;
      }
    }
    if (!found)
      return TRUE;                              // Error
  }
  return FALSE;                                 // Is prefix
#else
  while ((col1= col_it1++))
  {
    col2= col_it2++;
    if (!(*col1 == *col2))
      return TRUE;
  }
  return FALSE;                                 // Is prefix
#endif
}


/****************************************************************************
** Thread specific functions
****************************************************************************/

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
  Lock connection data for the set of connections this connection
  belongs to

  @param thd                       THD object
*/
void thd_lock_thread_count(THD *)
{
  mysql_mutex_lock(&LOCK_thread_count);
}

/**
  Lock connection data for the set of connections this connection
  belongs to

  @param thd                       THD object
*/
void thd_unlock_thread_count(THD *)
{
  mysql_cond_broadcast(&COND_thread_count);
  mysql_mutex_unlock(&LOCK_thread_count);
}

/**
  Close the socket used by this connection

  @param thd                THD object
*/
void thd_close_connection(THD *thd)
{
  if (thd->net.vio)
    vio_close(thd->net.vio);
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
  Set up various THD data for a new connection

  thd_new_connection_setup

  @param              thd            THD object
  @param              stack_start    Start of stack for connection
*/
void thd_new_connection_setup(THD *thd, char *stack_start)
{
#ifdef HAVE_PSI_INTERFACE
  if (PSI_server)
    thd_set_psi(thd,
                PSI_server->new_thread(key_thread_one_connection,
                                       thd,
                                       thd_get_thread_id((MYSQL_THD)thd)));
#endif
  thd->set_time();
  thd->prior_thr_create_utime= thd->thr_create_utime= thd->start_utime=
    my_micro_time();
  threads.append(thd);
  thd_unlock_thread_count(thd);
  DBUG_PRINT("info", ("init new connection. thd: 0x%lx fd: %d",
          (ulong)thd, thd->net.vio->sd));
  thd_set_thread_stack(thd, stack_start);
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
  Support method to check if connection has already started transcaction

  @param client_cntx    Low level client context

  @retval               TRUE if connection already started transaction
*/
bool thd_is_transaction_active(THD *thd)
{
  return thd->transaction.is_active();
}

/**
  Check if there is buffered data on the socket representing the connection

  @param thd                  THD object
*/
int thd_connection_has_data(THD *thd)
{
  Vio *vio= thd->net.vio;
  return vio->has_data(vio);
}

/**
  Set reading/writing on socket, used by SHOW PROCESSLIST

  @param thd                       THD object
  @param val                       Value to set it to (0 or 1)
*/
void thd_set_net_read_write(THD *thd, uint val)
{
  thd->net.reading_or_writing= val;
}

/**
  Get reading/writing on socket from THD object
  @param thd                       THD object

  @retval               net.reading_or_writing value for thread on THD.
*/
uint thd_get_net_read_write(THD *thd)
{
  return thd->net.reading_or_writing;
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
  return thd->net.vio->sd;
}

/**
  Get thread attributes for connection threads

  @retval      Reference to thread attribute for connection threads
*/
pthread_attr_t *get_connection_attrib(void)
{
  return &connection_attrib;
}

/**
  Get max number of connections

  @retval         Max number of connections for MySQL Server
*/
ulong get_max_connections(void)
{
  return max_connections;
}

/*
  The following functions form part of the C plugin API
*/

extern "C" int mysql_tmpfile(const char *prefix)
{
  char filename[FN_REFLEN];
  File fd = create_temp_file(filename, mysql_tmpdir, prefix,
#ifdef __WIN__
                             O_BINARY | O_TRUNC | O_SEQUENTIAL |
                             O_SHORT_LIVED |
#endif /* __WIN__ */
                             O_CREAT | O_EXCL | O_RDWR | O_TEMPORARY,
                             MYF(MY_WME));
  if (fd >= 0) {
#ifndef __WIN__
    /*
      This can be removed once the following bug is fixed:
      Bug #28903  create_temp_file() doesn't honor O_TEMPORARY option
                  (file not removed) (Unix)
    */
    unlink(filename);
#endif /* !__WIN__ */
  }

  return fd;
}


extern "C"
int thd_in_lock_tables(const THD *thd)
{
  return test(thd->in_lock_tables);
}


extern "C"
int thd_tablespace_op(const THD *thd)
{
  return test(thd->tablespace_op);
}


extern "C"
const char *set_thd_proc_info(void *thd_arg, const char *info,
                              const char *calling_function,
                              const char *calling_file,
                              const unsigned int calling_line)
{
  THD *thd= (THD *) thd_arg;

  if (!thd)
    thd= current_thd;

  const char *old_info= thd->proc_info;
  DBUG_PRINT("proc_info", ("%s:%d  %s", calling_file, calling_line, info));

#if defined(ENABLED_PROFILING)
  thd->profiling.status_change(info,
                               calling_function, calling_file, calling_line);
#endif
  thd->proc_info= info;
  return old_info;
}

extern "C"
const char* thd_enter_cond(MYSQL_THD thd, mysql_cond_t *cond,
                           mysql_mutex_t *mutex, const char *msg)
{
  if (!thd)
    thd= current_thd;

  return thd->enter_cond(cond, mutex, msg);
}

extern "C"
void thd_exit_cond(MYSQL_THD thd, const char *old_msg)
{
  if (!thd)
    thd= current_thd;

  thd->exit_cond(old_msg);
  return;
}

extern "C"
void **thd_ha_data(const THD *thd, const struct handlerton *hton)
{
  return (void **) &thd->ha_data[hton->slot].ha_ptr;
}

extern "C"
void thd_storage_lock_wait(THD *thd, long long value)
{
  thd->utime_after_lock+= value;
}

/**
  Provide a handler data getter to simplify coding
*/
extern "C"
void *thd_get_ha_data(const THD *thd, const struct handlerton *hton)
{
  return *thd_ha_data(thd, hton);
}


/**
  Provide a handler data setter to simplify coding
  @see thd_set_ha_data() definition in plugin.h
*/
extern "C"
void thd_set_ha_data(THD *thd, const struct handlerton *hton,
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
long long thd_test_options(const THD *thd, long long test_options)
{
  return thd->variables.option_bits & test_options;
}

extern "C"
int thd_sql_command(const THD *thd)
{
  return (int) thd->lex->sql_command;
}

extern "C"
int thd_tx_isolation(const THD *thd)
{
  return (int) thd->tx_isolation;
}

extern "C"
void thd_inc_row_count(THD *thd)
{
  thd->warning_info->inc_current_row_for_warning();
}


/**
  Dumps a text description of a thread, its security context
  (user, host) and the current query.

  @param thd thread context
  @param buffer pointer to preferred result buffer
  @param length length of buffer
  @param max_query_len how many chars of query to copy (0 for all)

  @req LOCK_thread_count
  
  @note LOCK_thread_count mutex is not necessary when the function is invoked on
   the currently running thread (current_thd) or if the caller in some other
   way guarantees that access to thd->query is serialized.
 
  @return Pointer to string
*/

extern "C"
char *thd_security_context(THD *thd, char *buffer, unsigned int length,
                           unsigned int max_query_len)
{
  String str(buffer, length, &my_charset_latin1);
  const Security_context *sctx= &thd->main_security_ctx;
  char header[256];
  int len;
  /*
    The pointers thd->query and thd->proc_info might change since they are
    being modified concurrently. This is acceptable for proc_info since its
    values doesn't have to very accurate and the memory it points to is static,
    but we need to attempt a snapshot on the pointer values to avoid using NULL
    values. The pointer to thd->query however, doesn't point to static memory
    and has to be protected by LOCK_thread_count or risk pointing to
    uninitialized memory.
  */
  const char *proc_info= thd->proc_info;

  len= my_snprintf(header, sizeof(header),
                   "MySQL thread id %lu, OS thread handle 0x%lx, query id %lu",
                   thd->thread_id, (ulong) thd->real_id, (ulong) thd->query_id);
  str.length(0);
  str.append(header, len);

  if (sctx->host)
  {
    str.append(' ');
    str.append(sctx->host);
  }

  if (sctx->ip)
  {
    str.append(' ');
    str.append(sctx->ip);
  }

  if (sctx->user)
  {
    str.append(' ');
    str.append(sctx->user);
  }

  if (proc_info)
  {
    str.append(' ');
    str.append(proc_info);
  }

  mysql_mutex_lock(&thd->LOCK_thd_data);

  if (thd->query())
  {
    if (max_query_len < 1)
      len= thd->query_length();
    else
      len= min(thd->query_length(), max_query_len);
    str.append('\n');
    str.append(thd->query(), len);
  }

  mysql_mutex_unlock(&thd->LOCK_thd_data);

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


/**
  Implementation of Drop_table_error_handler::handle_condition().
  The reason in having this implementation is to silence technical low-level
  warnings during DROP TABLE operation. Currently we don't want to expose
  the following warnings during DROP TABLE:
    - Some of table files are missed or invalid (the table is going to be
      deleted anyway, so why bother that something was missed);
    - A trigger associated with the table does not have DEFINER (One of the
      MySQL specifics now is that triggers are loaded for the table being
      dropped. So, we may have a warning that trigger does not have DEFINER
      attribute during DROP TABLE operation).

  @return TRUE if the condition is handled.
*/
bool Drop_table_error_handler::handle_condition(THD *thd,
                                                uint sql_errno,
                                                const char* sqlstate,
                                                MYSQL_ERROR::enum_warning_level level,
                                                const char* msg,
                                                MYSQL_ERROR ** cond_hdl)
{
  *cond_hdl= NULL;
  return ((sql_errno == EE_DELETE && my_errno == ENOENT) ||
          sql_errno == ER_TRG_NO_DEFINER);
}


THD::THD()
   :Statement(&main_lex, &main_mem_root, STMT_CONVENTIONAL_EXECUTION,
              /* statement id */ 0),
   rli_fake(0), rli_slave(NULL),
   user_time(0), in_sub_stmt(0),
   binlog_unsafe_warning_flags(0),
   binlog_table_maps(0),
   table_map_for_update(0),
   arg_of_last_insert_id_function(FALSE),
   first_successful_insert_id_in_prev_stmt(0),
   first_successful_insert_id_in_prev_stmt_for_binlog(0),
   first_successful_insert_id_in_cur_stmt(0),
   stmt_depends_on_first_successful_insert_id_in_prev_stmt(FALSE),
   examined_row_count(0),
   warning_info(&main_warning_info),
   stmt_da(&main_da),
   is_fatal_error(0),
   transaction_rollback_request(0),
   is_fatal_sub_stmt_error(0),
   rand_used(0),
   time_zone_used(0),
   in_lock_tables(0),
   bootstrap(0),
   derived_tables_processing(FALSE),
   spcont(NULL),
   m_parser_state(NULL),
#if defined(ENABLED_DEBUG_SYNC)
   debug_sync_control(0),
#endif /* defined(ENABLED_DEBUG_SYNC) */
   main_warning_info(0, false)
{
  ulong tmp;

  mdl_context.init(this);
  /*
    Pass nominal parameters to init_alloc_root only to ensure that
    the destructor works OK in case of an error. The main_mem_root
    will be re-initialized in init_for_queries().
  */
  init_sql_alloc(&main_mem_root, ALLOC_ROOT_MIN_BLOCK_SIZE, 0);
  stmt_arena= this;
  thread_stack= 0;
  catalog= (char*)"std"; // the only catalog we have for now
  main_security_ctx.init();
  security_ctx= &main_security_ctx;
  no_errors= 0;
  password= 0;
  query_start_used= 0;
  count_cuted_fields= CHECK_FIELD_IGNORE;
  killed= NOT_KILLED;
  col_access=0;
  is_slave_error= thread_specific_used= FALSE;
  my_hash_clear(&handler_tables_hash);
  tmp_table=0;
  cuted_fields= 0L;
  sent_row_count= 0L;
  limit_found_rows= 0;
  m_row_count_func= -1;
  statement_id_counter= 0UL;
  // Must be reset to handle error with THD's created for init of mysqld
  lex->current_select= 0;
  start_time=(time_t) 0;
  start_utime= prior_thr_create_utime= 0L;
  utime_after_lock= 0L;
  current_linfo =  0;
  slave_thread = 0;
  bzero(&variables, sizeof(variables));
  thread_id= 0;
  one_shot_set= 0;
  file_id = 0;
  query_id= 0;
  query_name_consts= 0;
  db_charset= global_system_variables.collation_database;
  bzero(ha_data, sizeof(ha_data));
  mysys_var=0;
  binlog_evt_union.do_union= FALSE;
  enable_slow_log= 0;
#ifndef DBUG_OFF
  dbug_sentry=THD_SENTRY_MAGIC;
#endif
#ifndef EMBEDDED_LIBRARY
  mysql_audit_init_thd(this);
  net.vio=0;
#endif
  client_capabilities= 0;                       // minimalistic client
  ull=0;
  system_thread= NON_SYSTEM_THREAD;
  cleanup_done= abort_on_warning= 0;
  peer_port= 0;					// For SHOW PROCESSLIST
  transaction.m_pending_rows_event= 0;
  transaction.on= 1;
#ifdef SIGNAL_WITH_VIO_CLOSE
  active_vio = 0;
#endif
  mysql_mutex_init(key_LOCK_thd_data, &LOCK_thd_data, MY_MUTEX_INIT_FAST);

  /* Variables with default values */
  proc_info="login";
  where= THD::DEFAULT_WHERE;
  server_id = ::server_id;
  slave_net = 0;
  command=COM_CONNECT;
  *scramble= '\0';

  /* Call to init() below requires fully initialized Open_tables_state. */
  reset_open_tables_state(this);

  init();
#if defined(ENABLED_PROFILING)
  profiling.set_thd(this);
#endif
  m_user_connect= NULL;
  my_hash_init(&user_vars, system_charset_info, USER_VARS_HASH_SIZE, 0, 0,
               (my_hash_get_key) get_var_key,
               (my_hash_free_key) free_user_var, 0);

  sp_proc_cache= NULL;
  sp_func_cache= NULL;

  /* For user vars replication*/
  if (opt_bin_log)
    my_init_dynamic_array(&user_var_events,
			  sizeof(BINLOG_USER_VAR_EVENT *), 16, 16);
  else
    bzero((char*) &user_var_events, sizeof(user_var_events));

  /* Protocol */
  protocol= &protocol_text;			// Default protocol
  protocol_text.init(this);
  protocol_binary.init(this);

  tablespace_op=FALSE;
  tmp= sql_rnd_with_mutex();
  randominit(&rand, tmp + (ulong) &rand, tmp + (ulong) ::global_query_id);
  substitute_null_with_insert_id = FALSE;
  thr_lock_info_init(&lock_info); /* safety: will be reset after start */

  m_internal_handler= NULL;
  m_binlog_invoker= FALSE;
  memset(&invoker_user, 0, sizeof(invoker_user));
  memset(&invoker_host, 0, sizeof(invoker_host));
}


void THD::push_internal_handler(Internal_error_handler *handler)
{
  if (m_internal_handler)
  {
    handler->m_prev_internal_handler= m_internal_handler;
    m_internal_handler= handler;
  }
  else
  {
    m_internal_handler= handler;
  }
}

bool THD::handle_condition(uint sql_errno,
                           const char* sqlstate,
                           MYSQL_ERROR::enum_warning_level level,
                           const char* msg,
                           MYSQL_ERROR ** cond_hdl)
{
  if (!m_internal_handler)
  {
    *cond_hdl= NULL;
    return FALSE;
  }

  for (Internal_error_handler *error_handler= m_internal_handler;
       error_handler;
       error_handler= error_handler->m_prev_internal_handler)
  {
    if (error_handler->handle_condition(this, sql_errno, sqlstate, level, msg,
					cond_hdl))
    {
      return TRUE;
    }
  }

  return FALSE;
}


Internal_error_handler *THD::pop_internal_handler()
{
  DBUG_ASSERT(m_internal_handler != NULL);
  Internal_error_handler *popped_handler= m_internal_handler;
  m_internal_handler= m_internal_handler->m_prev_internal_handler;
  return popped_handler;
}


void THD::raise_error(uint sql_errno)
{
  const char* msg= ER(sql_errno);
  (void) raise_condition(sql_errno,
                         NULL,
                         MYSQL_ERROR::WARN_LEVEL_ERROR,
                         msg);
}

void THD::raise_error_printf(uint sql_errno, ...)
{
  va_list args;
  char ebuff[MYSQL_ERRMSG_SIZE];
  DBUG_ENTER("THD::raise_error_printf");
  DBUG_PRINT("my", ("nr: %d  errno: %d", sql_errno, errno));
  const char* format= ER(sql_errno);
  va_start(args, sql_errno);
  my_vsnprintf(ebuff, sizeof(ebuff), format, args);
  va_end(args);
  (void) raise_condition(sql_errno,
                         NULL,
                         MYSQL_ERROR::WARN_LEVEL_ERROR,
                         ebuff);
  DBUG_VOID_RETURN;
}

void THD::raise_warning(uint sql_errno)
{
  const char* msg= ER(sql_errno);
  (void) raise_condition(sql_errno,
                         NULL,
                         MYSQL_ERROR::WARN_LEVEL_WARN,
                         msg);
}

void THD::raise_warning_printf(uint sql_errno, ...)
{
  va_list args;
  char    ebuff[MYSQL_ERRMSG_SIZE];
  DBUG_ENTER("THD::raise_warning_printf");
  DBUG_PRINT("enter", ("warning: %u", sql_errno));
  const char* format= ER(sql_errno);
  va_start(args, sql_errno);
  my_vsnprintf(ebuff, sizeof(ebuff), format, args);
  va_end(args);
  (void) raise_condition(sql_errno,
                         NULL,
                         MYSQL_ERROR::WARN_LEVEL_WARN,
                         ebuff);
  DBUG_VOID_RETURN;
}

void THD::raise_note(uint sql_errno)
{
  DBUG_ENTER("THD::raise_note");
  DBUG_PRINT("enter", ("code: %d", sql_errno));
  if (!(variables.option_bits & OPTION_SQL_NOTES))
    DBUG_VOID_RETURN;
  const char* msg= ER(sql_errno);
  (void) raise_condition(sql_errno,
                         NULL,
                         MYSQL_ERROR::WARN_LEVEL_NOTE,
                         msg);
  DBUG_VOID_RETURN;
}

void THD::raise_note_printf(uint sql_errno, ...)
{
  va_list args;
  char    ebuff[MYSQL_ERRMSG_SIZE];
  DBUG_ENTER("THD::raise_note_printf");
  DBUG_PRINT("enter",("code: %u", sql_errno));
  if (!(variables.option_bits & OPTION_SQL_NOTES))
    DBUG_VOID_RETURN;
  const char* format= ER(sql_errno);
  va_start(args, sql_errno);
  my_vsnprintf(ebuff, sizeof(ebuff), format, args);
  va_end(args);
  (void) raise_condition(sql_errno,
                         NULL,
                         MYSQL_ERROR::WARN_LEVEL_NOTE,
                         ebuff);
  DBUG_VOID_RETURN;
}

MYSQL_ERROR* THD::raise_condition(uint sql_errno,
                                  const char* sqlstate,
                                  MYSQL_ERROR::enum_warning_level level,
                                  const char* msg)
{
  MYSQL_ERROR *cond= NULL;
  DBUG_ENTER("THD::raise_condition");

  if (!(variables.option_bits & OPTION_SQL_NOTES) &&
      (level == MYSQL_ERROR::WARN_LEVEL_NOTE))
    DBUG_RETURN(NULL);

  warning_info->opt_clear_warning_info(query_id);

  /*
    TODO: replace by DBUG_ASSERT(sql_errno != 0) once all bugs similar to
    Bug#36768 are fixed: a SQL condition must have a real (!=0) error number
    so that it can be caught by handlers.
  */
  if (sql_errno == 0)
    sql_errno= ER_UNKNOWN_ERROR;
  if (msg == NULL)
    msg= ER(sql_errno);
  if (sqlstate == NULL)
   sqlstate= mysql_errno_to_sqlstate(sql_errno);

  if ((level == MYSQL_ERROR::WARN_LEVEL_WARN) &&
      really_abort_on_warning())
  {
    /*
      FIXME:
      push_warning and strict SQL_MODE case.
    */
    level= MYSQL_ERROR::WARN_LEVEL_ERROR;
    killed= THD::KILL_BAD_DATA;
  }

  switch (level)
  {
  case MYSQL_ERROR::WARN_LEVEL_NOTE:
  case MYSQL_ERROR::WARN_LEVEL_WARN:
    got_warning= 1;
    break;
  case MYSQL_ERROR::WARN_LEVEL_ERROR:
    break;
  default:
    DBUG_ASSERT(FALSE);
  }

  if (handle_condition(sql_errno, sqlstate, level, msg, &cond))
    DBUG_RETURN(cond);

  if (level == MYSQL_ERROR::WARN_LEVEL_ERROR)
  {
    is_slave_error=  1; // needed to catch query errors during replication

    /*
      thd->lex->current_select == 0 if lex structure is not inited
      (not query command (COM_QUERY))
    */
    if (lex->current_select &&
        lex->current_select->no_error && !is_fatal_error)
    {
      DBUG_PRINT("error",
                 ("Error converted to warning: current_select: no_error %d  "
                  "fatal_error: %d",
                  (lex->current_select ?
                   lex->current_select->no_error : 0),
                  (int) is_fatal_error));
    }
    else
    {
      if (! stmt_da->is_error())
      {
        set_row_count_func(-1);
        stmt_da->set_error_status(this, sql_errno, msg, sqlstate);
      }
    }
  }

  query_cache_abort(&query_cache_tls);

  /* When simulating OOM, skip writing to error log to avoid mtr errors */
  DBUG_EXECUTE_IF("simulate_out_of_memory", DBUG_RETURN(NULL););

  cond= warning_info->push_warning(this, sql_errno, sqlstate, level, msg);
  DBUG_RETURN(cond);
}

extern "C"
void *thd_alloc(MYSQL_THD thd, unsigned int size)
{
  return thd->alloc(size);
}

extern "C"
void *thd_calloc(MYSQL_THD thd, unsigned int size)
{
  return thd->calloc(size);
}

extern "C"
char *thd_strdup(MYSQL_THD thd, const char *str)
{
  return thd->strdup(str);
}

extern "C"
char *thd_strmake(MYSQL_THD thd, const char *str, unsigned int size)
{
  return thd->strmake(str, size);
}

extern "C"
LEX_STRING *thd_make_lex_string(THD *thd, LEX_STRING *lex_str,
                                const char *str, unsigned int size,
                                int allocate_lex_string)
{
  return thd->make_lex_string(lex_str, str, size,
                              (bool) allocate_lex_string);
}

extern "C"
void *thd_memdup(MYSQL_THD thd, const void* str, unsigned int size)
{
  return thd->memdup(str, size);
}

extern "C"
void thd_get_xid(const MYSQL_THD thd, MYSQL_XID *xid)
{
  *xid = *(MYSQL_XID *) &thd->transaction.xid_state.xid;
}

#ifdef _WIN32
extern "C"   THD *_current_thd_noinline(void)
{
  return my_pthread_getspecific_ptr(THD*,THR_THD);
}
#endif
/*
  Init common variables that has to be reset on start and on change_user
*/

void THD::init(void)
{
  mysql_mutex_lock(&LOCK_global_system_variables);
  plugin_thdvar_init(this);
  /*
    variables= global_system_variables above has reset
    variables.pseudo_thread_id to 0. We need to correct it here to
    avoid temporary tables replication failure.
  */
  variables.pseudo_thread_id= thread_id;
  mysql_mutex_unlock(&LOCK_global_system_variables);
  server_status= SERVER_STATUS_AUTOCOMMIT;
  if (variables.sql_mode & MODE_NO_BACKSLASH_ESCAPES)
    server_status|= SERVER_STATUS_NO_BACKSLASH_ESCAPES;

  transaction.all.modified_non_trans_table=
    transaction.stmt.modified_non_trans_table= FALSE;
  open_options=ha_open_options;
  update_lock_default= (variables.low_priority_updates ?
			TL_WRITE_LOW_PRIORITY :
			TL_WRITE);
  tx_isolation= (enum_tx_isolation) variables.tx_isolation;
  update_charset();
  reset_current_stmt_binlog_format_row();
  bzero((char *) &status_var, sizeof(status_var));

  if (variables.sql_log_bin)
    variables.option_bits|= OPTION_BIN_LOG;
  else
    variables.option_bits&= ~OPTION_BIN_LOG;

#if defined(ENABLED_DEBUG_SYNC)
  /* Initialize the Debug Sync Facility. See debug_sync.cc. */
  debug_sync_init_thread(this);
#endif /* defined(ENABLED_DEBUG_SYNC) */
}


/*
  Init THD for query processing.
  This has to be called once before we call mysql_parse.
  See also comments in sql_class.h.
*/

void THD::init_for_queries()
{
  set_time(); 
  ha_enable_transaction(this,TRUE);

  reset_root_defaults(mem_root, variables.query_alloc_block_size,
                      variables.query_prealloc_size);
  reset_root_defaults(&transaction.mem_root,
                      variables.trans_alloc_block_size,
                      variables.trans_prealloc_size);
  transaction.xid_state.xid.null();
  transaction.xid_state.in_thd=1;
}


/*
  Do what's needed when one invokes change user

  SYNOPSIS
    change_user()

  IMPLEMENTATION
    Reset all resources that are connection specific
*/


void THD::change_user(void)
{
  mysql_mutex_lock(&LOCK_status);
  add_to_status(&global_status_var, &status_var);
  mysql_mutex_unlock(&LOCK_status);

  cleanup();
  killed= NOT_KILLED;
  cleanup_done= 0;
  init();
  stmt_map.reset();
  my_hash_init(&user_vars, system_charset_info, USER_VARS_HASH_SIZE, 0, 0,
               (my_hash_get_key) get_var_key,
               (my_hash_free_key) free_user_var, 0);
  sp_cache_clear(&sp_proc_cache);
  sp_cache_clear(&sp_func_cache);
}


/* Do operations that may take a long time */

void THD::cleanup(void)
{
  DBUG_ENTER("THD::cleanup");
  DBUG_ASSERT(cleanup_done == 0);

  killed= KILL_CONNECTION;
#ifdef ENABLE_WHEN_BINLOG_WILL_BE_ABLE_TO_PREPARE
  if (transaction.xid_state.xa_state == XA_PREPARED)
  {
#error xid_state in the cache should be replaced by the allocated value
  }
#endif
  {
    transaction.xid_state.xa_state= XA_NOTR;
    trans_rollback(this);
    xid_cache_delete(&transaction.xid_state);
  }

  locked_tables_list.unlock_locked_tables(this);
  mysql_ha_cleanup(this);

  DBUG_ASSERT(open_tables == NULL);
  /*
    If the thread was in the middle of an ongoing transaction (rolled
    back a few lines above) or under LOCK TABLES (unlocked the tables
    and left the mode a few lines above), there will be outstanding
    metadata locks. Release them.
  */
  mdl_context.release_transactional_locks();

  /* Release the global read lock, if acquired. */
  if (global_read_lock.is_acquired())
    global_read_lock.unlock_global_read_lock(this);

  /* All metadata locks must have been released by now. */
  DBUG_ASSERT(!mdl_context.has_locks());

#if defined(ENABLED_DEBUG_SYNC)
  /* End the Debug Sync Facility. See debug_sync.cc. */
  debug_sync_end_thread(this);
#endif /* defined(ENABLED_DEBUG_SYNC) */

  delete_dynamic(&user_var_events);
  my_hash_free(&user_vars);
  close_temporary_tables(this);
  sp_cache_clear(&sp_proc_cache);
  sp_cache_clear(&sp_func_cache);

  if (ull)
  {
    mysql_mutex_lock(&LOCK_user_locks);
    item_user_lock_release(ull);
    mysql_mutex_unlock(&LOCK_user_locks);
    ull= NULL;
  }

  cleanup_done=1;
  DBUG_VOID_RETURN;
}


THD::~THD()
{
  THD_CHECK_SENTRY(this);
  DBUG_ENTER("~THD()");
  /* Ensure that no one is using THD */
  mysql_mutex_lock(&LOCK_thd_data);
  mysys_var=0;					// Safety (shouldn't be needed)
  mysql_mutex_unlock(&LOCK_thd_data);
  add_to_status(&global_status_var, &status_var);

  /* Close connection */
#ifndef EMBEDDED_LIBRARY
  if (net.vio)
  {
    vio_delete(net.vio);
    net_end(&net);
  }
#endif
  stmt_map.reset();                     /* close all prepared statements */
  if (!cleanup_done)
    cleanup();

  mdl_context.destroy();
  ha_close_connection(this);
  mysql_audit_release(this);
  plugin_thdvar_cleanup(this);

  DBUG_PRINT("info", ("freeing security context"));
  main_security_ctx.destroy();
  my_free(db);
  db= NULL;
  free_root(&transaction.mem_root,MYF(0));
  mysql_mutex_destroy(&LOCK_thd_data);
#ifndef DBUG_OFF
  dbug_sentry= THD_SENTRY_GONE;
#endif  
#ifndef EMBEDDED_LIBRARY
  if (rli_fake)
  {
    delete rli_fake;
    rli_fake= NULL;
  }
  
  mysql_audit_free_thd(this);
  if (rli_slave)
    rli_slave->cleanup_after_session();
#endif

  free_root(&main_mem_root, MYF(0));
  DBUG_VOID_RETURN;
}


/*
  Add all status variables to another status variable array

  SYNOPSIS
   add_to_status()
   to_var       add to this array
   from_var     from this array

  NOTES
    This function assumes that all variables are long/ulong.
    If this assumption will change, then we have to explictely add
    the other variables after the while loop
*/

void add_to_status(STATUS_VAR *to_var, STATUS_VAR *from_var)
{
  ulong *end= (ulong*) ((uchar*) to_var +
                        offsetof(STATUS_VAR, last_system_status_var) +
			sizeof(ulong));
  ulong *to= (ulong*) to_var, *from= (ulong*) from_var;

  while (to != end)
    *(to++)+= *(from++);

  to_var->bytes_received+= from_var->bytes_received;
  to_var->bytes_sent+= from_var->bytes_sent;
}

/*
  Add the difference between two status variable arrays to another one.

  SYNOPSIS
    add_diff_to_status
    to_var       add to this array
    from_var     from this array
    dec_var      minus this array
  
  NOTE
    This function assumes that all variables are long/ulong.
*/

void add_diff_to_status(STATUS_VAR *to_var, STATUS_VAR *from_var,
                        STATUS_VAR *dec_var)
{
  ulong *end= (ulong*) ((uchar*) to_var + offsetof(STATUS_VAR,
						  last_system_status_var) +
			sizeof(ulong));
  ulong *to= (ulong*) to_var, *from= (ulong*) from_var, *dec= (ulong*) dec_var;

  while (to != end)
    *(to++)+= *(from++) - *(dec++);

  to_var->bytes_received+= from_var->bytes_received - dec_var->bytes_received;;
  to_var->bytes_sent+= from_var->bytes_sent - dec_var->bytes_sent;
}


/**
  Awake a thread.

  @param[in]  state_to_set    value for THD::killed

  This is normally called from another thread's THD object.

  @note Do always call this while holding LOCK_thd_data.
*/

void THD::awake(THD::killed_state state_to_set)
{
  DBUG_ENTER("THD::awake");
  DBUG_PRINT("enter", ("this: %p current_thd: %p", this, current_thd));
  THD_CHECK_SENTRY(this);
  mysql_mutex_assert_owner(&LOCK_thd_data);

  /* Set the 'killed' flag of 'this', which is the target THD object. */
  killed= state_to_set;

  if (state_to_set != THD::KILL_QUERY)
  {
#ifdef SIGNAL_WITH_VIO_CLOSE
    if (this != current_thd)
    {
      /*
        Before sending a signal, let's close the socket of the thread
        that is being killed ("this", which is not the current thread).
        This is to make sure it does not block if the signal is lost.
        This needs to be done only on platforms where signals are not
        a reliable interruption mechanism.

        Note that the downside of this mechanism is that we could close
        the connection while "this" target thread is in the middle of
        sending a result to the application, thus violating the client-
        server protocol.

        On the other hand, without closing the socket we have a race
        condition. If "this" target thread passes the check of
        thd->killed, and then the current thread runs through
        THD::awake(), sets the 'killed' flag and completes the
        signaling, and then the target thread runs into read(), it will
        block on the socket. As a result of the discussions around
        Bug#37780, it has been decided that we accept the race
        condition. A second KILL awakes the target from read().

        If we are killing ourselves, we know that we are not blocked.
        We also know that we will check thd->killed before we go for
        reading the next statement.
      */

      close_active_vio();
    }
#endif

    /* Mark the target thread's alarm request expired, and signal alarm. */
    thr_alarm_kill(thread_id);

    /* Send an event to the scheduler that a thread should be killed. */
    if (!slave_thread)
      MYSQL_CALLBACK(thread_scheduler, post_kill_notification, (this));
  }

  /* Broadcast a condition to kick the target if it is waiting on it. */
  if (mysys_var)
  {
    mysql_mutex_lock(&mysys_var->mutex);
    if (!system_thread)		// Don't abort locks
      mysys_var->abort=1;
    /*
      This broadcast could be up in the air if the victim thread
      exits the cond in the time between read and broadcast, but that is
      ok since all we want to do is to make the victim thread get out
      of waiting on current_cond.
      If we see a non-zero current_cond: it cannot be an old value (because
      then exit_cond() should have run and it can't because we have mutex); so
      it is the true value but maybe current_mutex is not yet non-zero (we're
      in the middle of enter_cond() and there is a "memory order
      inversion"). So we test the mutex too to not lock 0.

      Note that there is a small chance we fail to kill. If victim has locked
      current_mutex, but hasn't yet entered enter_cond() (which means that
      current_cond and current_mutex are 0), then the victim will not get
      a signal and it may wait "forever" on the cond (until
      we issue a second KILL or the status it's waiting for happens).
      It's true that we have set its thd->killed but it may not
      see it immediately and so may have time to reach the cond_wait().

      However, where possible, we test for killed once again after
      enter_cond(). This should make the signaling as safe as possible.
      However, there is still a small chance of failure on platforms with
      instruction or memory write reordering.
    */
    if (mysys_var->current_cond && mysys_var->current_mutex)
    {
      mysql_mutex_lock(mysys_var->current_mutex);
      mysql_cond_broadcast(mysys_var->current_cond);
      mysql_mutex_unlock(mysys_var->current_mutex);
    }
    mysql_mutex_unlock(&mysys_var->mutex);
  }
  DBUG_VOID_RETURN;
}


/**
  Close the Vio associated this session.

  @remark LOCK_thd_data is taken due to the fact that
          the Vio might be disassociated concurrently.
*/

void THD::disconnect()
{
  Vio *vio= NULL;

  mysql_mutex_lock(&LOCK_thd_data);

  killed= THD::KILL_CONNECTION;

#ifdef SIGNAL_WITH_VIO_CLOSE
  /*
    Since a active vio might might have not been set yet, in
    any case save a reference to avoid closing a inexistent
    one or closing the vio twice if there is a active one.
  */
  vio= active_vio;
  close_active_vio();
#endif

  /* Disconnect even if a active vio is not associated. */
  if (net.vio != vio)
    vio_close(net.vio);

  mysql_mutex_unlock(&LOCK_thd_data);
}


/*
  Remember the location of thread info, the structure needed for
  sql_alloc() and the structure for the net buffer
*/

bool THD::store_globals()
{
  /*
    Assert that thread_stack is initialized: it's necessary to be able
    to track stack overrun.
  */
  DBUG_ASSERT(thread_stack);

  if (my_pthread_setspecific_ptr(THR_THD,  this) ||
      my_pthread_setspecific_ptr(THR_MALLOC, &mem_root))
    return 1;
  /*
    mysys_var is concurrently readable by a killer thread.
    It is protected by LOCK_thd_data, it is not needed to lock while the
    pointer is changing from NULL not non-NULL. If the kill thread reads
    NULL it doesn't refer to anything, but if it is non-NULL we need to
    ensure that the thread doesn't proceed to assign another thread to
    have the mysys_var reference (which in fact refers to the worker
    threads local storage with key THR_KEY_mysys. 
  */
  mysys_var=my_thread_var;
  /*
    Let mysqld define the thread id (not mysys)
    This allows us to move THD to different threads if needed.
  */
  mysys_var->id= thread_id;
  real_id= pthread_self();                      // For debugging

  /*
    We have to call thr_lock_info_init() again here as THD may have been
    created in another thread
  */
  thr_lock_info_init(&lock_info);
  return 0;
}

/*
  Remove the thread specific info (THD and mem_root pointer) stored during
  store_global call for this thread.
*/
bool THD::restore_globals()
{
  /*
    Assert that thread_stack is initialized: it's necessary to be able
    to track stack overrun.
  */
  DBUG_ASSERT(thread_stack);
  
  /* Undocking the thread specific data. */
  my_pthread_setspecific_ptr(THR_THD, NULL);
  my_pthread_setspecific_ptr(THR_MALLOC, NULL);
  
  return 0;
}


/*
  Cleanup after query.

  SYNOPSIS
    THD::cleanup_after_query()

  DESCRIPTION
    This function is used to reset thread data to its default state.

  NOTE
    This function is not suitable for setting thread data to some
    non-default values, as there is only one replication thread, so
    different master threads may overwrite data of each other on
    slave.
*/

void THD::cleanup_after_query()
{
  /*
    Reset rand_used so that detection of calls to rand() will save random 
    seeds if needed by the slave.

    Do not reset rand_used if inside a stored function or trigger because 
    only the call to these operations is logged. Thus only the calling 
    statement needs to detect rand() calls made by its substatements. These
    substatements must not set rand_used to 0 because it would remove the
    detection of rand() by the calling statement. 
  */
  if (!in_sub_stmt) /* stored functions and triggers are a special case */
  {
    /* Forget those values, for next binlogger: */
    stmt_depends_on_first_successful_insert_id_in_prev_stmt= 0;
    auto_inc_intervals_in_cur_stmt_for_binlog.empty();
    rand_used= 0;
  }
  if (first_successful_insert_id_in_cur_stmt > 0)
  {
    /* set what LAST_INSERT_ID() will return */
    first_successful_insert_id_in_prev_stmt= 
      first_successful_insert_id_in_cur_stmt;
    first_successful_insert_id_in_cur_stmt= 0;
    substitute_null_with_insert_id= TRUE;
  }
  arg_of_last_insert_id_function= 0;
  /* Free Items that were created during this execution */
  free_items();
  /* Reset where. */
  where= THD::DEFAULT_WHERE;
  /* reset table map for multi-table update */
  table_map_for_update= 0;
  m_binlog_invoker= FALSE;
  /* reset replication info structure */
  if (lex && lex->mi.repl_ignore_server_ids.buffer) 
  {
    delete_dynamic(&lex->mi.repl_ignore_server_ids);
  }
#ifndef EMBEDDED_LIBRARY
  if (rli_slave)
    rli_slave->cleanup_after_query();
#endif
}


LEX_STRING *
make_lex_string_root(MEM_ROOT *mem_root,
                     LEX_STRING *lex_str, const char* str, uint length,
                     bool allocate_lex_string)
{
  if (allocate_lex_string)
    if (!(lex_str= (LEX_STRING *)alloc_root(mem_root, sizeof(LEX_STRING))))
      return 0;
  if (!(lex_str->str= strmake_root(mem_root, str, length)))
    return 0;
  lex_str->length= length;
  return lex_str;
}

/**
  Create a LEX_STRING in this connection.

  @param lex_str  pointer to LEX_STRING object to be initialized
  @param str      initializer to be copied into lex_str
  @param length   length of str, in bytes
  @param allocate_lex_string  if TRUE, allocate new LEX_STRING object,
                              instead of using lex_str value
  @return  NULL on failure, or pointer to the LEX_STRING object
*/
LEX_STRING *THD::make_lex_string(LEX_STRING *lex_str,
                                 const char* str, uint length,
                                 bool allocate_lex_string)
{
  return make_lex_string_root (mem_root, lex_str, str,
                               length, allocate_lex_string);
}


/*
  Convert a string to another character set

  SYNOPSIS
    convert_string()
    to				Store new allocated string here
    to_cs			New character set for allocated string
    from			String to convert
    from_length			Length of string to convert
    from_cs			Original character set

  NOTES
    to will be 0-terminated to make it easy to pass to system funcs

  RETURN
    0	ok
    1	End of memory.
        In this case to->str will point to 0 and to->length will be 0.
*/

bool THD::convert_string(LEX_STRING *to, CHARSET_INFO *to_cs,
			 const char *from, uint from_length,
			 CHARSET_INFO *from_cs)
{
  DBUG_ENTER("convert_string");
  size_t new_length= to_cs->mbmaxlen * from_length;
  uint dummy_errors;
  if (!(to->str= (char*) alloc(new_length+1)))
  {
    to->length= 0;				// Safety fix
    DBUG_RETURN(1);				// EOM
  }
  to->length= copy_and_convert((char*) to->str, new_length, to_cs,
			       from, from_length, from_cs, &dummy_errors);
  to->str[to->length]=0;			// Safety
  DBUG_RETURN(0);
}


/*
  Convert string from source character set to target character set inplace.

  SYNOPSIS
    THD::convert_string

  DESCRIPTION
    Convert string using convert_buffer - buffer for character set 
    conversion shared between all protocols.

  RETURN
    0   ok
   !0   out of memory
*/

bool THD::convert_string(String *s, CHARSET_INFO *from_cs, CHARSET_INFO *to_cs)
{
  uint dummy_errors;
  if (convert_buffer.copy(s->ptr(), s->length(), from_cs, to_cs, &dummy_errors))
    return TRUE;
  /* If convert_buffer >> s copying is more efficient long term */
  if (convert_buffer.alloced_length() >= convert_buffer.length() * 2 ||
      !s->is_alloced())
  {
    return s->copy(convert_buffer);
  }
  s->swap(convert_buffer);
  return FALSE;
}


/*
  Update some cache variables when character set changes
*/

void THD::update_charset()
{
  uint32 not_used;
  charset_is_system_charset=
    !String::needs_conversion(0,
                              variables.character_set_client,
                              system_charset_info,
                              &not_used);
  charset_is_collation_connection= 
    !String::needs_conversion(0,
                              variables.character_set_client,
                              variables.collation_connection,
                              &not_used);
  charset_is_character_set_filesystem= 
    !String::needs_conversion(0,
                              variables.character_set_client,
                              variables.character_set_filesystem,
                              &not_used);
}


/* routings to adding tables to list of changed in transaction tables */

inline static void list_include(CHANGED_TABLE_LIST** prev,
				CHANGED_TABLE_LIST* curr,
				CHANGED_TABLE_LIST* new_table)
{
  if (new_table)
  {
    *prev = new_table;
    (*prev)->next = curr;
  }
}

/* add table to list of changed in transaction tables */

void THD::add_changed_table(TABLE *table)
{
  DBUG_ENTER("THD::add_changed_table(table)");

  DBUG_ASSERT(in_multi_stmt_transaction_mode() && table->file->has_transactions());
  add_changed_table(table->s->table_cache_key.str,
                    (long) table->s->table_cache_key.length);
  DBUG_VOID_RETURN;
}


void THD::add_changed_table(const char *key, long key_length)
{
  DBUG_ENTER("THD::add_changed_table(key)");
  CHANGED_TABLE_LIST **prev_changed = &transaction.changed_tables;
  CHANGED_TABLE_LIST *curr = transaction.changed_tables;

  for (; curr; prev_changed = &(curr->next), curr = curr->next)
  {
    int cmp =  (long)curr->key_length - (long)key_length;
    if (cmp < 0)
    {
      list_include(prev_changed, curr, changed_table_dup(key, key_length));
      DBUG_PRINT("info", 
		 ("key_length: %ld  %u", key_length,
                  (*prev_changed)->key_length));
      DBUG_VOID_RETURN;
    }
    else if (cmp == 0)
    {
      cmp = memcmp(curr->key, key, curr->key_length);
      if (cmp < 0)
      {
	list_include(prev_changed, curr, changed_table_dup(key, key_length));
	DBUG_PRINT("info", 
		   ("key_length:  %ld  %u", key_length,
		    (*prev_changed)->key_length));
	DBUG_VOID_RETURN;
      }
      else if (cmp == 0)
      {
	DBUG_PRINT("info", ("already in list"));
	DBUG_VOID_RETURN;
      }
    }
  }
  *prev_changed = changed_table_dup(key, key_length);
  DBUG_PRINT("info", ("key_length: %ld  %u", key_length,
		      (*prev_changed)->key_length));
  DBUG_VOID_RETURN;
}


CHANGED_TABLE_LIST* THD::changed_table_dup(const char *key, long key_length)
{
  CHANGED_TABLE_LIST* new_table = 
    (CHANGED_TABLE_LIST*) trans_alloc(ALIGN_SIZE(sizeof(CHANGED_TABLE_LIST))+
				      key_length + 1);
  if (!new_table)
  {
    my_error(EE_OUTOFMEMORY, MYF(ME_BELL),
             ALIGN_SIZE(sizeof(TABLE_LIST)) + key_length + 1);
    killed= KILL_CONNECTION;
    return 0;
  }

  new_table->key= ((char*)new_table)+ ALIGN_SIZE(sizeof(CHANGED_TABLE_LIST));
  new_table->next = 0;
  new_table->key_length = key_length;
  ::memcpy(new_table->key, key, key_length);
  return new_table;
}


int THD::send_explain_fields(select_result *result)
{
  List<Item> field_list;
  Item *item;
  CHARSET_INFO *cs= system_charset_info;
  field_list.push_back(new Item_return_int("id",3, MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("select_type", 19, cs));
  field_list.push_back(item= new Item_empty_string("table", NAME_CHAR_LEN, cs));
  item->maybe_null= 1;
  if (lex->describe & DESCRIBE_PARTITIONS)
  {
    /* Maximum length of string that make_used_partitions_str() can produce */
    item= new Item_empty_string("partitions", MAX_PARTITIONS * (1 + FN_LEN),
                                cs);
    field_list.push_back(item);
    item->maybe_null= 1;
  }
  field_list.push_back(item= new Item_empty_string("type", 10, cs));
  item->maybe_null= 1;
  field_list.push_back(item=new Item_empty_string("possible_keys",
						  NAME_CHAR_LEN*MAX_KEY, cs));
  item->maybe_null=1;
  field_list.push_back(item=new Item_empty_string("key", NAME_CHAR_LEN, cs));
  item->maybe_null=1;
  field_list.push_back(item=new Item_empty_string("key_len",
						  NAME_CHAR_LEN*MAX_KEY));
  item->maybe_null=1;
  field_list.push_back(item=new Item_empty_string("ref",
                                                  NAME_CHAR_LEN*MAX_REF_PARTS,
                                                  cs));
  item->maybe_null=1;
  field_list.push_back(item= new Item_return_int("rows", 10,
                                                 MYSQL_TYPE_LONGLONG));
  if (lex->describe & DESCRIBE_EXTENDED)
  {
    field_list.push_back(item= new Item_float("filtered", 0.1234, 2, 4));
    item->maybe_null=1;
  }
  item->maybe_null= 1;
  field_list.push_back(new Item_empty_string("Extra", 255, cs));
  return (result->send_result_set_metadata(field_list,
                                           Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF));
}

#ifdef SIGNAL_WITH_VIO_CLOSE
void THD::close_active_vio()
{
  DBUG_ENTER("close_active_vio");
  mysql_mutex_assert_owner(&LOCK_thd_data);
#ifndef EMBEDDED_LIBRARY
  if (active_vio)
  {
    vio_close(active_vio);
    active_vio = 0;
  }
#endif
  DBUG_VOID_RETURN;
}
#endif


struct Item_change_record: public ilink
{
  Item **place;
  Item *old_value;
  /* Placement new was hidden by `new' in ilink (TODO: check): */
  static void *operator new(size_t size, void *mem) { return mem; }
  static void operator delete(void *ptr, size_t size) {}
  static void operator delete(void *ptr, void *mem) { /* never called */ }
};


/*
  Register an item tree tree transformation, performed by the query
  optimizer. We need a pointer to runtime_memroot because it may be !=
  thd->mem_root (due to possible set_n_backup_active_arena called for thd).
*/

void THD::nocheck_register_item_tree_change(Item **place, Item *old_value,
                                            MEM_ROOT *runtime_memroot)
{
  Item_change_record *change;
  /*
    Now we use one node per change, which adds some memory overhead,
    but still is rather fast as we use alloc_root for allocations.
    A list of item tree changes of an average query should be short.
  */
  void *change_mem= alloc_root(runtime_memroot, sizeof(*change));
  if (change_mem == 0)
  {
    /*
      OOM, thd->fatal_error() is called by the error handler of the
      memroot. Just return.
    */
    return;
  }
  change= new (change_mem) Item_change_record;
  change->place= place;
  change->old_value= old_value;
  change_list.append(change);
}


void THD::rollback_item_tree_changes()
{
  I_List_iterator<Item_change_record> it(change_list);
  Item_change_record *change;
  DBUG_ENTER("rollback_item_tree_changes");

  while ((change= it++))
    *change->place= change->old_value;
  /* We can forget about changes memory: it's allocated in runtime memroot */
  change_list.empty();
  DBUG_VOID_RETURN;
}


/*****************************************************************************
** Functions to provide a interface to select results
*****************************************************************************/

select_result::select_result()
{
  thd=current_thd;
}

void select_result::send_error(uint errcode,const char *err)
{
  my_message(errcode, err, MYF(0));
}


void select_result::cleanup()
{
  /* do nothing */
}

bool select_result::check_simple_select() const
{
  my_error(ER_SP_BAD_CURSOR_QUERY, MYF(0));
  return TRUE;
}


static String default_line_term("\n",default_charset_info);
static String default_escaped("\\",default_charset_info);
static String default_field_term("\t",default_charset_info);
static String default_xml_row_term("<row>", default_charset_info);

sql_exchange::sql_exchange(char *name, bool flag,
                           enum enum_filetype filetype_arg)
  :file_name(name), opt_enclosed(0), dumpfile(flag), skip_lines(0)
{
  filetype= filetype_arg;
  field_term= &default_field_term;
  enclosed=   line_start= &my_empty_string;
  line_term=  filetype == FILETYPE_CSV ?
              &default_line_term : &default_xml_row_term;
  escaped=    &default_escaped;
  cs= NULL;
}

bool sql_exchange::escaped_given(void)
{
  return escaped != &default_escaped;
}


bool select_send::send_result_set_metadata(List<Item> &list, uint flags)
{
  bool res;
  if (!(res= thd->protocol->send_result_set_metadata(&list, flags)))
    is_result_set_started= 1;
  return res;
}

void select_send::abort_result_set()
{
  DBUG_ENTER("select_send::abort_result_set");

  if (is_result_set_started && thd->spcont)
  {
    /*
      We're executing a stored procedure, have an open result
      set and an SQL exception condition. In this situation we
      must abort the current statement, silence the error and
      start executing the continue/exit handler if one is found.
      Before aborting the statement, let's end the open result set, as
      otherwise the client will hang due to the violation of the
      client/server protocol.
    */
    thd->spcont->end_partial_result_set= TRUE;
  }
  DBUG_VOID_RETURN;
}


/** 
  Cleanup an instance of this class for re-use
  at next execution of a prepared statement/
  stored procedure statement.
*/

void select_send::cleanup()
{
  is_result_set_started= FALSE;
}

/* Send data to client. Returns 0 if ok */

bool select_send::send_data(List<Item> &items)
{
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("select_send::send_data");

  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    DBUG_RETURN(FALSE);
  }

  /*
    We may be passing the control from mysqld to the client: release the
    InnoDB adaptive hash S-latch to avoid thread deadlocks if it was reserved
    by thd
  */
  ha_release_temporary_latches(thd);

  protocol->prepare_for_resend();
  if (protocol->send_result_set_row(&items))
  {
    protocol->remove_last_row();
    DBUG_RETURN(TRUE);
  }

  thd->sent_row_count++;

  if (thd->vio_ok())
    DBUG_RETURN(protocol->write());

  DBUG_RETURN(0);
}

bool select_send::send_eof()
{
  /* 
    We may be passing the control from mysqld to the client: release the
    InnoDB adaptive hash S-latch to avoid thread deadlocks if it was reserved
    by thd 
  */
  ha_release_temporary_latches(thd);

  /* 
    Don't send EOF if we're in error condition (which implies we've already
    sent or are sending an error)
  */
  if (thd->is_error())
    return TRUE;
  ::my_eof(thd);
  is_result_set_started= 0;
  return FALSE;
}


/************************************************************************
  Handling writing to file
************************************************************************/

void select_to_file::send_error(uint errcode,const char *err)
{
  my_message(errcode, err, MYF(0));
  if (file > 0)
  {
    (void) end_io_cache(&cache);
    mysql_file_close(file, MYF(0));
    /* Delete file on error */
    mysql_file_delete(key_select_to_file, path, MYF(0));
    file= -1;
  }
}


bool select_to_file::send_eof()
{
  int error= test(end_io_cache(&cache));
  if (mysql_file_close(file, MYF(MY_WME)) || thd->is_error())
    error= true;

  if (!error)
  {
    ::my_ok(thd,row_count);
  }
  file= -1;
  return error;
}


void select_to_file::cleanup()
{
  /* In case of error send_eof() may be not called: close the file here. */
  if (file >= 0)
  {
    (void) end_io_cache(&cache);
    mysql_file_close(file, MYF(0));
    file= -1;
  }
  path[0]= '\0';
  row_count= 0;
}


select_to_file::~select_to_file()
{
  if (file >= 0)
  {					// This only happens in case of error
    (void) end_io_cache(&cache);
    mysql_file_close(file, MYF(0));
    file= -1;
  }
}

/***************************************************************************
** Export of select to textfile
***************************************************************************/

select_export::~select_export()
{
  thd->sent_row_count=row_count;
}


/*
  Create file with IO cache

  SYNOPSIS
    create_file()
    thd			Thread handle
    path		File name
    exchange		Excange class
    cache		IO cache

  RETURN
    >= 0 	File handle
   -1		Error
*/


static File create_file(THD *thd, char *path, sql_exchange *exchange,
			IO_CACHE *cache)
{
  File file;
  uint option= MY_UNPACK_FILENAME | MY_RELATIVE_PATH;

#ifdef DONT_ALLOW_FULL_LOAD_DATA_PATHS
  option|= MY_REPLACE_DIR;			// Force use of db directory
#endif

  if (!dirname_length(exchange->file_name))
  {
    strxnmov(path, FN_REFLEN-1, mysql_real_data_home, thd->db ? thd->db : "",
             NullS);
    (void) fn_format(path, exchange->file_name, path, "", option);
  }
  else
    (void) fn_format(path, exchange->file_name, mysql_real_data_home, "", option);

  if (!is_secure_file_path(path))
  {
    /* Write only allowed to dir or subdir specified by secure_file_priv */
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--secure-file-priv");
    return -1;
  }

  if (!access(path, F_OK))
  {
    my_error(ER_FILE_EXISTS_ERROR, MYF(0), exchange->file_name);
    return -1;
  }
  /* Create the file world readable */
  if ((file= mysql_file_create(key_select_to_file,
                               path, 0666, O_WRONLY|O_EXCL, MYF(MY_WME))) < 0)
    return file;
#ifdef HAVE_FCHMOD
  (void) fchmod(file, 0666);			// Because of umask()
#else
  (void) chmod(path, 0666);
#endif
  if (init_io_cache(cache, file, 0L, WRITE_CACHE, 0L, 1, MYF(MY_WME)))
  {
    mysql_file_close(file, MYF(0));
    /* Delete file on error, it was just created */
    mysql_file_delete(key_select_to_file, path, MYF(0));
    return -1;
  }
  return file;
}


int
select_export::prepare(List<Item> &list, SELECT_LEX_UNIT *u)
{
  bool blob_flag=0;
  bool string_results= FALSE, non_string_results= FALSE;
  unit= u;
  if ((uint) strlen(exchange->file_name) + NAME_LEN >= FN_REFLEN)
    strmake(path,exchange->file_name,FN_REFLEN-1);

  write_cs= exchange->cs ? exchange->cs : &my_charset_bin;

  if ((file= create_file(thd, path, exchange, &cache)) < 0)
    return 1;
  /* Check if there is any blobs in data */
  {
    List_iterator_fast<Item> li(list);
    Item *item;
    while ((item=li++))
    {
      if (item->max_length >= MAX_BLOB_WIDTH)
      {
	blob_flag=1;
	break;
      }
      if (item->result_type() == STRING_RESULT)
        string_results= TRUE;
      else
        non_string_results= TRUE;
    }
  }
  if (exchange->escaped->numchars() > 1 || exchange->enclosed->numchars() > 1)
  {
    my_error(ER_WRONG_FIELD_TERMINATORS, MYF(0));
    return TRUE;
  }
  if (exchange->escaped->length() > 1 || exchange->enclosed->length() > 1 ||
      !my_isascii(exchange->escaped->ptr()[0]) ||
      !my_isascii(exchange->enclosed->ptr()[0]) ||
      !exchange->field_term->is_ascii() || !exchange->line_term->is_ascii() ||
      !exchange->line_start->is_ascii())
  {
    /*
      Current LOAD DATA INFILE recognizes field/line separators "as is" without
      converting from client charset to data file charset. So, it is supposed,
      that input file of LOAD DATA INFILE consists of data in one charset and
      separators in other charset. For the compatibility with that [buggy]
      behaviour SELECT INTO OUTFILE implementation has been saved "as is" too,
      but the new warning message has been added:

        Non-ASCII separator arguments are not fully supported
    */
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                 WARN_NON_ASCII_SEPARATOR_NOT_IMPLEMENTED,
                 ER(WARN_NON_ASCII_SEPARATOR_NOT_IMPLEMENTED));
  }
  field_term_length=exchange->field_term->length();
  field_term_char= field_term_length ?
                   (int) (uchar) (*exchange->field_term)[0] : INT_MAX;
  if (!exchange->line_term->length())
    exchange->line_term=exchange->field_term;	// Use this if it exists
  field_sep_char= (exchange->enclosed->length() ?
                  (int) (uchar) (*exchange->enclosed)[0] : field_term_char);
  if (exchange->escaped->length() && (exchange->escaped_given() ||
      !(thd->variables.sql_mode & MODE_NO_BACKSLASH_ESCAPES)))
    escape_char= (int) (uchar) (*exchange->escaped)[0];
  else
    escape_char= -1;
  is_ambiguous_field_sep= test(strchr(ESCAPE_CHARS, field_sep_char));
  is_unsafe_field_sep= test(strchr(NUMERIC_CHARS, field_sep_char));
  line_sep_char= (exchange->line_term->length() ?
                 (int) (uchar) (*exchange->line_term)[0] : INT_MAX);
  if (!field_term_length)
    exchange->opt_enclosed=0;
  if (!exchange->enclosed->length())
    exchange->opt_enclosed=1;			// A little quicker loop
  fixed_row_size= (!field_term_length && !exchange->enclosed->length() &&
		   !blob_flag);
  if ((is_ambiguous_field_sep && exchange->enclosed->is_empty() &&
       (string_results || is_unsafe_field_sep)) ||
      (exchange->opt_enclosed && non_string_results &&
       field_term_length && strchr(NUMERIC_CHARS, field_term_char)))
  {
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                 ER_AMBIGUOUS_FIELD_TERM, ER(ER_AMBIGUOUS_FIELD_TERM));
    is_ambiguous_field_term= TRUE;
  }
  else
    is_ambiguous_field_term= FALSE;

  return 0;
}


#define NEED_ESCAPING(x) ((int) (uchar) (x) == escape_char    || \
                          (enclosed ? (int) (uchar) (x) == field_sep_char      \
                                    : (int) (uchar) (x) == field_term_char) || \
                          (int) (uchar) (x) == line_sep_char  || \
                          !(x))

bool select_export::send_data(List<Item> &items)
{

  DBUG_ENTER("select_export::send_data");
  char buff[MAX_FIELD_WIDTH],null_buff[2],space[MAX_FIELD_WIDTH];
  char cvt_buff[MAX_FIELD_WIDTH];
  String cvt_str(cvt_buff, sizeof(cvt_buff), write_cs);
  bool space_inited=0;
  String tmp(buff,sizeof(buff),&my_charset_bin),*res;
  tmp.length(0);

  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    DBUG_RETURN(0);
  }
  row_count++;
  Item *item;
  uint used_length=0,items_left=items.elements;
  List_iterator_fast<Item> li(items);

  if (my_b_write(&cache,(uchar*) exchange->line_start->ptr(),
		 exchange->line_start->length()))
    goto err;
  while ((item=li++))
  {
    Item_result result_type=item->result_type();
    bool enclosed = (exchange->enclosed->length() &&
                     (!exchange->opt_enclosed || result_type == STRING_RESULT));
    res=item->str_result(&tmp);
    if (res && !my_charset_same(write_cs, res->charset()) &&
        !my_charset_same(write_cs, &my_charset_bin))
    {
      const char *well_formed_error_pos;
      const char *cannot_convert_error_pos;
      const char *from_end_pos;
      const char *error_pos;
      uint32 bytes;
      uint64 estimated_bytes=
        ((uint64) res->length() / res->charset()->mbminlen + 1) *
        write_cs->mbmaxlen + 1;
      set_if_smaller(estimated_bytes, UINT_MAX32);
      if (cvt_str.realloc((uint32) estimated_bytes))
      {
        my_error(ER_OUTOFMEMORY, MYF(0), (uint32) estimated_bytes);
        goto err;
      }

      bytes= well_formed_copy_nchars(write_cs, (char *) cvt_str.ptr(),
                                     cvt_str.alloced_length(),
                                     res->charset(), res->ptr(), res->length(),
                                     UINT_MAX32, // copy all input chars,
                                                 // i.e. ignore nchars parameter
                                     &well_formed_error_pos,
                                     &cannot_convert_error_pos,
                                     &from_end_pos);
      error_pos= well_formed_error_pos ? well_formed_error_pos
                                       : cannot_convert_error_pos;
      if (error_pos)
      {
        char printable_buff[32];
        convert_to_printable(printable_buff, sizeof(printable_buff),
                             error_pos, res->ptr() + res->length() - error_pos,
                             res->charset(), 6);
        push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                            ER_TRUNCATED_WRONG_VALUE_FOR_FIELD,
                            ER(ER_TRUNCATED_WRONG_VALUE_FOR_FIELD),
                            "string", printable_buff,
                            item->name, static_cast<long>(row_count));
      }
      else if (from_end_pos < res->ptr() + res->length())
      { 
        /*
          result is longer than UINT_MAX32 and doesn't fit into String
        */
        push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                            WARN_DATA_TRUNCATED, ER(WARN_DATA_TRUNCATED),
                            item->full_name(), static_cast<long>(row_count));
      }
      cvt_str.length(bytes);
      res= &cvt_str;
    }
    if (res && enclosed)
    {
      if (my_b_write(&cache,(uchar*) exchange->enclosed->ptr(),
		     exchange->enclosed->length()))
	goto err;
    }
    if (!res)
    {						// NULL
      if (!fixed_row_size)
      {
	if (escape_char != -1)			// Use \N syntax
	{
	  null_buff[0]=escape_char;
	  null_buff[1]='N';
	  if (my_b_write(&cache,(uchar*) null_buff,2))
	    goto err;
	}
	else if (my_b_write(&cache,(uchar*) "NULL",4))
	  goto err;
      }
      else
      {
	used_length=0;				// Fill with space
      }
    }
    else
    {
      if (fixed_row_size)
	used_length=min(res->length(),item->max_length);
      else
	used_length=res->length();
      if ((result_type == STRING_RESULT || is_unsafe_field_sep) &&
           escape_char != -1)
      {
        char *pos, *start, *end;
        CHARSET_INFO *res_charset= res->charset();
        CHARSET_INFO *character_set_client= thd->variables.
                                            character_set_client;
        bool check_second_byte= (res_charset == &my_charset_bin) &&
                                 character_set_client->
                                 escape_with_backslash_is_dangerous;
        DBUG_ASSERT(character_set_client->mbmaxlen == 2 ||
                    !character_set_client->escape_with_backslash_is_dangerous);
	for (start=pos=(char*) res->ptr(),end=pos+used_length ;
	     pos != end ;
	     pos++)
	{
#ifdef USE_MB
	  if (use_mb(res_charset))
	  {
	    int l;
	    if ((l=my_ismbchar(res_charset, pos, end)))
	    {
	      pos += l-1;
	      continue;
	    }
	  }
#endif

          /*
            Special case when dumping BINARY/VARBINARY/BLOB values
            for the clients with character sets big5, cp932, gbk and sjis,
            which can have the escape character (0x5C "\" by default)
            as the second byte of a multi-byte sequence.
            
            If
            - pos[0] is a valid multi-byte head (e.g 0xEE) and
            - pos[1] is 0x00, which will be escaped as "\0",
            
            then we'll get "0xEE + 0x5C + 0x30" in the output file.
            
            If this file is later loaded using this sequence of commands:
            
            mysql> create table t1 (a varchar(128)) character set big5;
            mysql> LOAD DATA INFILE 'dump.txt' INTO TABLE t1;
            
            then 0x5C will be misinterpreted as the second byte
            of a multi-byte character "0xEE + 0x5C", instead of
            escape character for 0x00.
            
            To avoid this confusion, we'll escape the multi-byte
            head character too, so the sequence "0xEE + 0x00" will be
            dumped as "0x5C + 0xEE + 0x5C + 0x30".
            
            Note, in the condition below we only check if
            mbcharlen is equal to 2, because there are no
            character sets with mbmaxlen longer than 2
            and with escape_with_backslash_is_dangerous set.
            DBUG_ASSERT before the loop makes that sure.
          */

          if ((NEED_ESCAPING(*pos) ||
               (check_second_byte &&
                my_mbcharlen(character_set_client, (uchar) *pos) == 2 &&
                pos + 1 < end &&
                NEED_ESCAPING(pos[1]))) &&
              /*
               Don't escape field_term_char by doubling - doubling is only
               valid for ENCLOSED BY characters:
              */
              (enclosed || !is_ambiguous_field_term ||
               (int) (uchar) *pos != field_term_char))
          {
	    char tmp_buff[2];
            tmp_buff[0]= ((int) (uchar) *pos == field_sep_char &&
                          is_ambiguous_field_sep) ?
                          field_sep_char : escape_char;
	    tmp_buff[1]= *pos ? *pos : '0';
	    if (my_b_write(&cache,(uchar*) start,(uint) (pos-start)) ||
		my_b_write(&cache,(uchar*) tmp_buff,2))
	      goto err;
	    start=pos+1;
	  }
	}
	if (my_b_write(&cache,(uchar*) start,(uint) (pos-start)))
	  goto err;
      }
      else if (my_b_write(&cache,(uchar*) res->ptr(),used_length))
	goto err;
    }
    if (fixed_row_size)
    {						// Fill with space
      if (item->max_length > used_length)
      {
	/* QQ:  Fix by adding a my_b_fill() function */
	if (!space_inited)
	{
	  space_inited=1;
	  bfill(space,sizeof(space),' ');
	}
	uint length=item->max_length-used_length;
	for (; length > sizeof(space) ; length-=sizeof(space))
	{
	  if (my_b_write(&cache,(uchar*) space,sizeof(space)))
	    goto err;
	}
	if (my_b_write(&cache,(uchar*) space,length))
	  goto err;
      }
    }
    if (res && enclosed)
    {
      if (my_b_write(&cache, (uchar*) exchange->enclosed->ptr(),
                     exchange->enclosed->length()))
        goto err;
    }
    if (--items_left)
    {
      if (my_b_write(&cache, (uchar*) exchange->field_term->ptr(),
                     field_term_length))
        goto err;
    }
  }
  if (my_b_write(&cache,(uchar*) exchange->line_term->ptr(),
		 exchange->line_term->length()))
    goto err;
  DBUG_RETURN(0);
err:
  DBUG_RETURN(1);
}


/***************************************************************************
** Dump  of select to a binary file
***************************************************************************/


int
select_dump::prepare(List<Item> &list __attribute__((unused)),
		     SELECT_LEX_UNIT *u)
{
  unit= u;
  return (int) ((file= create_file(thd, path, exchange, &cache)) < 0);
}


bool select_dump::send_data(List<Item> &items)
{
  List_iterator_fast<Item> li(items);
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff),&my_charset_bin),*res;
  tmp.length(0);
  Item *item;
  DBUG_ENTER("select_dump::send_data");

  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    DBUG_RETURN(0);
  }
  if (row_count++ > 1) 
  {
    my_message(ER_TOO_MANY_ROWS, ER(ER_TOO_MANY_ROWS), MYF(0));
    goto err;
  }
  while ((item=li++))
  {
    res=item->str_result(&tmp);
    if (!res)					// If NULL
    {
      if (my_b_write(&cache,(uchar*) "",1))
	goto err;
    }
    else if (my_b_write(&cache,(uchar*) res->ptr(),res->length()))
    {
      my_error(ER_ERROR_ON_WRITE, MYF(0), path, my_errno);
      goto err;
    }
  }
  DBUG_RETURN(0);
err:
  DBUG_RETURN(1);
}


select_subselect::select_subselect(Item_subselect *item_arg)
{
  item= item_arg;
}


bool select_singlerow_subselect::send_data(List<Item> &items)
{
  DBUG_ENTER("select_singlerow_subselect::send_data");
  Item_singlerow_subselect *it= (Item_singlerow_subselect *)item;
  if (it->assigned())
  {
    my_message(ER_SUBQUERY_NO_1_ROW, ER(ER_SUBQUERY_NO_1_ROW), MYF(0));
    DBUG_RETURN(1);
  }
  if (unit->offset_limit_cnt)
  {				          // Using limit offset,count
    unit->offset_limit_cnt--;
    DBUG_RETURN(0);
  }
  List_iterator_fast<Item> li(items);
  Item *val_item;
  for (uint i= 0; (val_item= li++); i++)
    it->store(i, val_item);
  it->assigned(1);
  DBUG_RETURN(0);
}


void select_max_min_finder_subselect::cleanup()
{
  DBUG_ENTER("select_max_min_finder_subselect::cleanup");
  cache= 0;
  DBUG_VOID_RETURN;
}


bool select_max_min_finder_subselect::send_data(List<Item> &items)
{
  DBUG_ENTER("select_max_min_finder_subselect::send_data");
  Item_maxmin_subselect *it= (Item_maxmin_subselect *)item;
  List_iterator_fast<Item> li(items);
  Item *val_item= li++;
  it->register_value();
  if (it->assigned())
  {
    cache->store(val_item);
    if ((this->*op)())
      it->store(0, cache);
  }
  else
  {
    if (!cache)
    {
      cache= Item_cache::get_cache(val_item);
      switch (val_item->result_type())
      {
      case REAL_RESULT:
	op= &select_max_min_finder_subselect::cmp_real;
	break;
      case INT_RESULT:
	op= &select_max_min_finder_subselect::cmp_int;
	break;
      case STRING_RESULT:
	op= &select_max_min_finder_subselect::cmp_str;
	break;
      case DECIMAL_RESULT:
        op= &select_max_min_finder_subselect::cmp_decimal;
        break;
      case ROW_RESULT:
        // This case should never be choosen
	DBUG_ASSERT(0);
	op= 0;
      }
    }
    cache->store(val_item);
    it->store(0, cache);
  }
  it->assigned(1);
  DBUG_RETURN(0);
}

bool select_max_min_finder_subselect::cmp_real()
{
  Item *maxmin= ((Item_singlerow_subselect *)item)->element_index(0);
  double val1= cache->val_real(), val2= maxmin->val_real();
  if (fmax)
    return (cache->null_value && !maxmin->null_value) ||
      (!cache->null_value && !maxmin->null_value &&
       val1 > val2);
  return (maxmin->null_value && !cache->null_value) ||
    (!cache->null_value && !maxmin->null_value &&
     val1 < val2);
}

bool select_max_min_finder_subselect::cmp_int()
{
  Item *maxmin= ((Item_singlerow_subselect *)item)->element_index(0);
  longlong val1= cache->val_int(), val2= maxmin->val_int();
  if (fmax)
    return (cache->null_value && !maxmin->null_value) ||
      (!cache->null_value && !maxmin->null_value &&
       val1 > val2);
  return (maxmin->null_value && !cache->null_value) ||
    (!cache->null_value && !maxmin->null_value &&
     val1 < val2);
}

bool select_max_min_finder_subselect::cmp_decimal()
{
  Item *maxmin= ((Item_singlerow_subselect *)item)->element_index(0);
  my_decimal cval, *cvalue= cache->val_decimal(&cval);
  my_decimal mval, *mvalue= maxmin->val_decimal(&mval);
  if (fmax)
    return (cache->null_value && !maxmin->null_value) ||
      (!cache->null_value && !maxmin->null_value &&
       my_decimal_cmp(cvalue, mvalue) > 0) ;
  return (maxmin->null_value && !cache->null_value) ||
    (!cache->null_value && !maxmin->null_value &&
     my_decimal_cmp(cvalue,mvalue) < 0);
}

bool select_max_min_finder_subselect::cmp_str()
{
  String *val1, *val2, buf1, buf2;
  Item *maxmin= ((Item_singlerow_subselect *)item)->element_index(0);
  /*
    as far as both operand is Item_cache buf1 & buf2 will not be used,
    but added for safety
  */
  val1= cache->val_str(&buf1);
  val2= maxmin->val_str(&buf1);
  if (fmax)
    return (cache->null_value && !maxmin->null_value) ||
      (!cache->null_value && !maxmin->null_value &&
       sortcmp(val1, val2, cache->collation.collation) > 0) ;
  return (maxmin->null_value && !cache->null_value) ||
    (!cache->null_value && !maxmin->null_value &&
     sortcmp(val1, val2, cache->collation.collation) < 0);
}

bool select_exists_subselect::send_data(List<Item> &items)
{
  DBUG_ENTER("select_exists_subselect::send_data");
  Item_exists_subselect *it= (Item_exists_subselect *)item;
  if (unit->offset_limit_cnt)
  {				          // Using limit offset,count
    unit->offset_limit_cnt--;
    DBUG_RETURN(0);
  }
  it->value= 1;
  it->assigned(1);
  DBUG_RETURN(0);
}


/***************************************************************************
  Dump of select to variables
***************************************************************************/

int select_dumpvar::prepare(List<Item> &list, SELECT_LEX_UNIT *u)
{
  unit= u;

  if (var_list.elements != list.elements)
  {
    my_message(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT,
               ER(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT), MYF(0));
    return 1;
  }

  return 0;
}


bool select_dumpvar::check_simple_select() const
{
  my_error(ER_SP_BAD_CURSOR_SELECT, MYF(0));
  return TRUE;
}


void select_dumpvar::cleanup()
{
  row_count= 0;
}


Query_arena::Type Query_arena::type() const
{
  DBUG_ASSERT(0); /* Should never be called */
  return STATEMENT;
}


void Query_arena::free_items()
{
  Item *next;
  DBUG_ENTER("Query_arena::free_items");
  /* This works because items are allocated with sql_alloc() */
  for (; free_list; free_list= next)
  {
    next= free_list->next;
    free_list->delete_self();
  }
  /* Postcondition: free_list is 0 */
  DBUG_VOID_RETURN;
}


void Query_arena::set_query_arena(Query_arena *set)
{
  mem_root=  set->mem_root;
  free_list= set->free_list;
  state= set->state;
}


void Query_arena::cleanup_stmt()
{
  DBUG_ASSERT(! "Query_arena::cleanup_stmt() not implemented");
}

/*
  Statement functions
*/

Statement::Statement(LEX *lex_arg, MEM_ROOT *mem_root_arg,
                     enum enum_state state_arg, ulong id_arg)
  :Query_arena(mem_root_arg, state_arg),
  id(id_arg),
  mark_used_columns(MARK_COLUMNS_READ),
  lex(lex_arg),
  db(NULL),
  db_length(0)
{
  name.str= NULL;
}


Query_arena::Type Statement::type() const
{
  return STATEMENT;
}


void Statement::set_statement(Statement *stmt)
{
  id=             stmt->id;
  mark_used_columns=   stmt->mark_used_columns;
  lex=            stmt->lex;
  query_string=   stmt->query_string;
}


void
Statement::set_n_backup_statement(Statement *stmt, Statement *backup)
{
  DBUG_ENTER("Statement::set_n_backup_statement");
  backup->set_statement(this);
  set_statement(stmt);
  DBUG_VOID_RETURN;
}


void Statement::restore_backup_statement(Statement *stmt, Statement *backup)
{
  DBUG_ENTER("Statement::restore_backup_statement");
  stmt->set_statement(this);
  set_statement(backup);
  DBUG_VOID_RETURN;
}


void THD::end_statement()
{
  /* Cleanup SQL processing state to reuse this statement in next query. */
  lex_end(lex);
  delete lex->result;
  lex->result= 0;
  /* Note that free_list is freed in cleanup_after_query() */

  /*
    Don't free mem_root, as mem_root is freed in the end of dispatch_command
    (once for any command).
  */
}


void THD::set_n_backup_active_arena(Query_arena *set, Query_arena *backup)
{
  DBUG_ENTER("THD::set_n_backup_active_arena");
  DBUG_ASSERT(backup->is_backup_arena == FALSE);

  backup->set_query_arena(this);
  set_query_arena(set);
#ifndef DBUG_OFF
  backup->is_backup_arena= TRUE;
#endif
  DBUG_VOID_RETURN;
}


void THD::restore_active_arena(Query_arena *set, Query_arena *backup)
{
  DBUG_ENTER("THD::restore_active_arena");
  DBUG_ASSERT(backup->is_backup_arena);
  set->set_query_arena(this);
  set_query_arena(backup);
#ifndef DBUG_OFF
  backup->is_backup_arena= FALSE;
#endif
  DBUG_VOID_RETURN;
}

Statement::~Statement()
{
}

C_MODE_START

static uchar *
get_statement_id_as_hash_key(const uchar *record, size_t *key_length,
                             my_bool not_used __attribute__((unused)))
{
  const Statement *statement= (const Statement *) record; 
  *key_length= sizeof(statement->id);
  return (uchar *) &((const Statement *) statement)->id;
}

static void delete_statement_as_hash_key(void *key)
{
  delete (Statement *) key;
}

static uchar *get_stmt_name_hash_key(Statement *entry, size_t *length,
                                    my_bool not_used __attribute__((unused)))
{
  *length= entry->name.length;
  return (uchar*) entry->name.str;
}

C_MODE_END

Statement_map::Statement_map() :
  last_found_statement(0)
{
  enum
  {
    START_STMT_HASH_SIZE = 16,
    START_NAME_HASH_SIZE = 16
  };
  my_hash_init(&st_hash, &my_charset_bin, START_STMT_HASH_SIZE, 0, 0,
               get_statement_id_as_hash_key,
               delete_statement_as_hash_key, MYF(0));
  my_hash_init(&names_hash, system_charset_info, START_NAME_HASH_SIZE, 0, 0,
               (my_hash_get_key) get_stmt_name_hash_key,
               NULL,MYF(0));
}


/*
  Insert a new statement to the thread-local statement map.

  DESCRIPTION
    If there was an old statement with the same name, replace it with the
    new one. Otherwise, check if max_prepared_stmt_count is not reached yet,
    increase prepared_stmt_count, and insert the new statement. It's okay
    to delete an old statement and fail to insert the new one.

  POSTCONDITIONS
    All named prepared statements are also present in names_hash.
    Statement names in names_hash are unique.
    The statement is added only if prepared_stmt_count < max_prepard_stmt_count
    last_found_statement always points to a valid statement or is 0

  RETURN VALUE
    0  success
    1  error: out of resources or max_prepared_stmt_count limit has been
       reached. An error is sent to the client, the statement is deleted.
*/

int Statement_map::insert(THD *thd, Statement *statement)
{
  if (my_hash_insert(&st_hash, (uchar*) statement))
  {
    /*
      Delete is needed only in case of an insert failure. In all other
      cases hash_delete will also delete the statement.
    */
    delete statement;
    my_error(ER_OUT_OF_RESOURCES, MYF(0));
    goto err_st_hash;
  }
  if (statement->name.str && my_hash_insert(&names_hash, (uchar*) statement))
  {
    my_error(ER_OUT_OF_RESOURCES, MYF(0));
    goto err_names_hash;
  }
  mysql_mutex_lock(&LOCK_prepared_stmt_count);
  /*
    We don't check that prepared_stmt_count is <= max_prepared_stmt_count
    because we would like to allow to lower the total limit
    of prepared statements below the current count. In that case
    no new statements can be added until prepared_stmt_count drops below
    the limit.
  */
  if (prepared_stmt_count >= max_prepared_stmt_count)
  {
    mysql_mutex_unlock(&LOCK_prepared_stmt_count);
    my_error(ER_MAX_PREPARED_STMT_COUNT_REACHED, MYF(0),
             max_prepared_stmt_count);
    goto err_max;
  }
  prepared_stmt_count++;
  mysql_mutex_unlock(&LOCK_prepared_stmt_count);

  last_found_statement= statement;
  return 0;

err_max:
  if (statement->name.str)
    my_hash_delete(&names_hash, (uchar*) statement);
err_names_hash:
  my_hash_delete(&st_hash, (uchar*) statement);
err_st_hash:
  return 1;
}


void Statement_map::close_transient_cursors()
{
#ifdef TO_BE_IMPLEMENTED
  Statement *stmt;
  while ((stmt= transient_cursor_list.head()))
    stmt->close_cursor();                 /* deletes itself from the list */
#endif
}


void Statement_map::erase(Statement *statement)
{
  if (statement == last_found_statement)
    last_found_statement= 0;
  if (statement->name.str)
    my_hash_delete(&names_hash, (uchar *) statement);

  my_hash_delete(&st_hash, (uchar *) statement);
  mysql_mutex_lock(&LOCK_prepared_stmt_count);
  DBUG_ASSERT(prepared_stmt_count > 0);
  prepared_stmt_count--;
  mysql_mutex_unlock(&LOCK_prepared_stmt_count);
}


void Statement_map::reset()
{
  /* Must be first, hash_free will reset st_hash.records */
  mysql_mutex_lock(&LOCK_prepared_stmt_count);
  DBUG_ASSERT(prepared_stmt_count >= st_hash.records);
  prepared_stmt_count-= st_hash.records;
  mysql_mutex_unlock(&LOCK_prepared_stmt_count);

  my_hash_reset(&names_hash);
  my_hash_reset(&st_hash);
  last_found_statement= 0;
}


Statement_map::~Statement_map()
{
  /* Must go first, hash_free will reset st_hash.records */
  mysql_mutex_lock(&LOCK_prepared_stmt_count);
  DBUG_ASSERT(prepared_stmt_count >= st_hash.records);
  prepared_stmt_count-= st_hash.records;
  mysql_mutex_unlock(&LOCK_prepared_stmt_count);

  my_hash_free(&names_hash);
  my_hash_free(&st_hash);
}

bool select_dumpvar::send_data(List<Item> &items)
{
  List_iterator_fast<my_var> var_li(var_list);
  List_iterator<Item> it(items);
  Item *item;
  my_var *mv;
  DBUG_ENTER("select_dumpvar::send_data");

  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    DBUG_RETURN(false);
  }
  if (row_count++) 
  {
    my_message(ER_TOO_MANY_ROWS, ER(ER_TOO_MANY_ROWS), MYF(0));
    DBUG_RETURN(true);
  }
  while ((mv= var_li++) && (item= it++))
  {
    if (mv->local)
    {
      if (thd->spcont->set_variable(thd, mv->offset, &item))
	    DBUG_RETURN(true);
    }
    else
    {
      /*
        Create Item_func_set_user_vars with delayed non-constness. We
        do this so that Item_get_user_var::const_item() will return
        the same result during
        Item_func_set_user_var::save_item_result() as they did during
        optimization and execution.
       */
      Item_func_set_user_var *suv=
        new Item_func_set_user_var(mv->s, item, true);
      if (suv->fix_fields(thd, 0))
        DBUG_RETURN(true);
      suv->save_item_result(item);
      if (suv->update())
        DBUG_RETURN(true);
    }
  }
  DBUG_RETURN(thd->is_error());
}

bool select_dumpvar::send_eof()
{
  if (! row_count)
    push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                 ER_SP_FETCH_NO_DATA, ER(ER_SP_FETCH_NO_DATA));
  /*
    Don't send EOF if we're in error condition (which implies we've already
    sent or are sending an error)
  */
  if (thd->is_error())
    return true;

  ::my_ok(thd,row_count);
  return 0;
}

/****************************************************************************
  TMP_TABLE_PARAM
****************************************************************************/

void TMP_TABLE_PARAM::init()
{
  DBUG_ENTER("TMP_TABLE_PARAM::init");
  DBUG_PRINT("enter", ("this: 0x%lx", (ulong)this));
  field_count= sum_func_count= func_count= hidden_field_count= 0;
  group_parts= group_length= group_null_parts= 0;
  quick_group= 1;
  table_charset= 0;
  precomputed_group_by= 0;
  DBUG_VOID_RETURN;
}


void thd_increment_bytes_sent(ulong length)
{
  THD *thd=current_thd;
  if (likely(thd != 0))
  { /* current_thd==0 when close_connection() calls net_send_error() */
    thd->status_var.bytes_sent+= length;
  }
}


void thd_increment_bytes_received(ulong length)
{
  current_thd->status_var.bytes_received+= length;
}


void thd_increment_net_big_packet_count(ulong length)
{
  current_thd->status_var.net_big_packet_count+= length;
}


void THD::set_status_var_init()
{
  bzero((char*) &status_var, sizeof(status_var));
}


void Security_context::init()
{
  host= user= ip= external_user= 0;
  host_or_ip= "connecting host";
  priv_user[0]= priv_host[0]= proxy_user[0]= '\0';
  master_access= 0;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  db_access= NO_ACCESS;
#endif
}


void Security_context::destroy()
{
  // If not pointer to constant
  if (host != my_localhost)
  {
    my_free(host);
    host= NULL;
  }
  if (user != delayed_user)
  {
    my_free(user);
    user= NULL;
  }

  if (external_user)
  {
    my_free(external_user);
    user= NULL;
  }

  my_free(ip);
  ip= NULL;
}


void Security_context::skip_grants()
{
  /* privileges for the user are unknown everything is allowed */
  host_or_ip= (char *)"";
  master_access= ~NO_ACCESS;
  *priv_user= *priv_host= '\0';
}


bool Security_context::set_user(char *user_arg)
{
  my_free(user);
  user= my_strdup(user_arg, MYF(0));
  return user == 0;
}

#ifndef NO_EMBEDDED_ACCESS_CHECKS
/**
  Initialize this security context from the passed in credentials
  and activate it in the current thread.

  @param       thd
  @param       definer_user
  @param       definer_host
  @param       db
  @param[out]  backup  Save a pointer to the current security context
                       in the thread. In case of success it points to the
                       saved old context, otherwise it points to NULL.


  During execution of a statement, multiple security contexts may
  be needed:
  - the security context of the authenticated user, used as the
    default security context for all top-level statements
  - in case of a view or a stored program, possibly the security
    context of the definer of the routine, if the object is
    defined with SQL SECURITY DEFINER option.

  The currently "active" security context is parameterized in THD
  member security_ctx. By default, after a connection is
  established, this member points at the "main" security context
  - the credentials of the authenticated user.

  Later, if we would like to execute some sub-statement or a part
  of a statement under credentials of a different user, e.g.
  definer of a procedure, we authenticate this user in a local
  instance of Security_context by means of this method (and
  ultimately by means of acl_getroot), and make the
  local instance active in the thread by re-setting
  thd->security_ctx pointer.

  Note, that the life cycle and memory management of the "main" and
  temporary security contexts are different.
  For the main security context, the memory for user/host/ip is
  allocated on system heap, and the THD class frees this memory in
  its destructor. The only case when contents of the main security
  context may change during its life time is when someone issued
  CHANGE USER command.
  Memory management of a "temporary" security context is
  responsibility of the module that creates it.

  @retval TRUE  there is no user with the given credentials. The erro
                is reported in the thread.
  @retval FALSE success
*/

bool
Security_context::
change_security_context(THD *thd,
                        LEX_STRING *definer_user,
                        LEX_STRING *definer_host,
                        LEX_STRING *db,
                        Security_context **backup)
{
  bool needs_change;

  DBUG_ENTER("Security_context::change_security_context");

  DBUG_ASSERT(definer_user->str && definer_host->str);

  *backup= NULL;
  needs_change= (strcmp(definer_user->str, thd->security_ctx->priv_user) ||
                 my_strcasecmp(system_charset_info, definer_host->str,
                               thd->security_ctx->priv_host));
  if (needs_change)
  {
    if (acl_getroot(this, definer_user->str, definer_host->str,
                                definer_host->str, db->str))
    {
      my_error(ER_NO_SUCH_USER, MYF(0), definer_user->str,
               definer_host->str);
      DBUG_RETURN(TRUE);
    }
    *backup= thd->security_ctx;
    thd->security_ctx= this;
  }

  DBUG_RETURN(FALSE);
}


void
Security_context::restore_security_context(THD *thd,
                                           Security_context *backup)
{
  if (backup)
    thd->security_ctx= backup;
}
#endif


bool Security_context::user_matches(Security_context *them)
{
  return ((user != NULL) && (them->user != NULL) &&
          !strcmp(user, them->user));
}


/****************************************************************************
  Handling of open and locked tables states.

  This is used when we want to open/lock (and then close) some tables when
  we already have a set of tables open and locked. We use these methods for
  access to mysql.proc table to find definitions of stored routines.
****************************************************************************/

void THD::reset_n_backup_open_tables_state(Open_tables_backup *backup)
{
  DBUG_ENTER("reset_n_backup_open_tables_state");
  backup->set_open_tables_state(this);
  backup->mdl_system_tables_svp= mdl_context.mdl_savepoint();
  reset_open_tables_state(this);
  state_flags|= Open_tables_state::BACKUPS_AVAIL;
  DBUG_VOID_RETURN;
}


void THD::restore_backup_open_tables_state(Open_tables_backup *backup)
{
  DBUG_ENTER("restore_backup_open_tables_state");
  mdl_context.rollback_to_savepoint(backup->mdl_system_tables_svp);
  /*
    Before we will throw away current open tables state we want
    to be sure that it was properly cleaned up.
  */
  DBUG_ASSERT(open_tables == 0 && temporary_tables == 0 &&
              derived_tables == 0 &&
              lock == 0 &&
              locked_tables_mode == LTM_NONE &&
              m_reprepare_observer == NULL);

  set_open_tables_state(backup);
  DBUG_VOID_RETURN;
}

/**
  Check the killed state of a user thread
  @param thd  user thread
  @retval 0 the user thread is active
  @retval 1 the user thread has been killed
*/
extern "C" int thd_killed(const MYSQL_THD thd)
{
  return(thd->killed);
}

/**
  Return the thread id of a user thread
  @param thd user thread
  @return thread id
*/
extern "C" unsigned long thd_get_thread_id(const MYSQL_THD thd)
{
  return((unsigned long)thd->thread_id);
}


#ifdef INNODB_COMPATIBILITY_HOOKS
extern "C" struct charset_info_st *thd_charset(MYSQL_THD thd)
{
  return(thd->charset());
}

/**
  OBSOLETE : there's no way to ensure the string is null terminated.
  Use thd_query_string instead()
*/
extern "C" char **thd_query(MYSQL_THD thd)
{
  return (&thd->query_string.string.str);
}

/**
  Get the current query string for the thread.

  @param The MySQL internal thread pointer
  @return query string and length. May be non-null-terminated.
*/
extern "C" LEX_STRING * thd_query_string (MYSQL_THD thd)
{
  return(&thd->query_string.string);
}

extern "C" int thd_slave_thread(const MYSQL_THD thd)
{
  return(thd->slave_thread);
}

extern "C" int thd_non_transactional_update(const MYSQL_THD thd)
{
  return(thd->transaction.all.modified_non_trans_table);
}

extern "C" int thd_binlog_format(const MYSQL_THD thd)
{
  if (mysql_bin_log.is_open() && (thd->variables.option_bits & OPTION_BIN_LOG))
    return (int) thd->variables.binlog_format;
  else
    return BINLOG_FORMAT_UNSPEC;
}

extern "C" void thd_mark_transaction_to_rollback(MYSQL_THD thd, bool all)
{
  mark_transaction_to_rollback(thd, all);
}

extern "C" bool thd_binlog_filter_ok(const MYSQL_THD thd)
{
  return binlog_filter->db_ok(thd->db);
}

extern "C" bool thd_sqlcom_can_generate_row_events(const MYSQL_THD thd)
{
  return sqlcom_can_generate_row_events(thd);
}

#ifndef EMBEDDED_LIBRARY
extern "C" void thd_pool_wait_begin(MYSQL_THD thd, int wait_type);
extern "C" void thd_pool_wait_end(MYSQL_THD thd);

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
  MYSQL_CALLBACK(thread_scheduler, thd_wait_begin, (thd, wait_type));
}

/**
  Interface for MySQL Server, plugins and storage engines to report
  when they waking up from a sleep/stall.

  @param  thd   Thread handle
*/
extern "C" void thd_wait_end(MYSQL_THD thd)
{
  MYSQL_CALLBACK(thread_scheduler, thd_wait_end, (thd));
}
#else
extern "C" void thd_wait_begin(MYSQL_THD thd, int wait_type)
{
  /* do NOTHING for the embedded library */
  return;
}

extern "C" void thd_wait_end(MYSQL_THD thd)
{
  /* do NOTHING for the embedded library */
  return;
}
#endif
#endif // INNODB_COMPATIBILITY_HOOKS */

/****************************************************************************
  Handling of statement states in functions and triggers.

  This is used to ensure that the function/trigger gets a clean state
  to work with and does not cause any side effects of the calling statement.

  It also allows most stored functions and triggers to replicate even
  if they are used items that would normally be stored in the binary
  replication (like last_insert_id() etc...)

  The following things is done
  - Disable binary logging for the duration of the statement
  - Disable multi-result-sets for the duration of the statement
  - Value of last_insert_id() is saved and restored
  - Value set by 'SET INSERT_ID=#' is reset and restored
  - Value for found_rows() is reset and restored
  - examined_row_count is added to the total
  - cuted_fields is added to the total
  - new savepoint level is created and destroyed

  NOTES:
    Seed for random() is saved for the first! usage of RAND()
    We reset examined_row_count and cuted_fields and add these to the
    result to ensure that if we have a bug that would reset these within
    a function, we are not loosing any rows from the main statement.

    We do not reset value of last_insert_id().
****************************************************************************/

void THD::reset_sub_statement_state(Sub_statement_state *backup,
                                    uint new_state)
{
#ifndef EMBEDDED_LIBRARY
  /* BUG#33029, if we are replicating from a buggy master, reset
     auto_inc_intervals_forced to prevent substatement
     (triggers/functions) from using erroneous INSERT_ID value
   */
  if (rpl_master_erroneous_autoinc(this))
  {
    DBUG_ASSERT(backup->auto_inc_intervals_forced.nb_elements() == 0);
    auto_inc_intervals_forced.swap(&backup->auto_inc_intervals_forced);
  }
#endif
  
  backup->option_bits=     variables.option_bits;
  backup->count_cuted_fields= count_cuted_fields;
  backup->in_sub_stmt=     in_sub_stmt;
  backup->enable_slow_log= enable_slow_log;
  backup->limit_found_rows= limit_found_rows;
  backup->examined_row_count= examined_row_count;
  backup->sent_row_count=   sent_row_count;
  backup->cuted_fields=     cuted_fields;
  backup->client_capabilities= client_capabilities;
  backup->savepoints= transaction.savepoints;
  backup->first_successful_insert_id_in_prev_stmt= 
    first_successful_insert_id_in_prev_stmt;
  backup->first_successful_insert_id_in_cur_stmt= 
    first_successful_insert_id_in_cur_stmt;

  if ((!lex->requires_prelocking() || is_update_query(lex->sql_command)) &&
      !is_current_stmt_binlog_format_row())
  {
    variables.option_bits&= ~OPTION_BIN_LOG;
  }

  if ((backup->option_bits & OPTION_BIN_LOG) &&
       is_update_query(lex->sql_command) &&
       !is_current_stmt_binlog_format_row())
    mysql_bin_log.start_union_events(this, this->query_id);

  /* Disable result sets */
  client_capabilities &= ~CLIENT_MULTI_RESULTS;
  in_sub_stmt|= new_state;
  examined_row_count= 0;
  sent_row_count= 0;
  cuted_fields= 0;
  transaction.savepoints= 0;
  first_successful_insert_id_in_cur_stmt= 0;
}


void THD::restore_sub_statement_state(Sub_statement_state *backup)
{
  DBUG_ENTER("THD::restore_sub_statement_state");
#ifndef EMBEDDED_LIBRARY
  /* BUG#33029, if we are replicating from a buggy master, restore
     auto_inc_intervals_forced so that the top statement can use the
     INSERT_ID value set before this statement.
   */
  if (rpl_master_erroneous_autoinc(this))
  {
    backup->auto_inc_intervals_forced.swap(&auto_inc_intervals_forced);
    DBUG_ASSERT(backup->auto_inc_intervals_forced.nb_elements() == 0);
  }
#endif

  /*
    To save resources we want to release savepoints which were created
    during execution of function or trigger before leaving their savepoint
    level. It is enough to release first savepoint set on this level since
    all later savepoints will be released automatically.
  */
  if (transaction.savepoints)
  {
    SAVEPOINT *sv;
    for (sv= transaction.savepoints; sv->prev; sv= sv->prev)
    {}
    /* ha_release_savepoint() never returns error. */
    (void)ha_release_savepoint(this, sv);
  }
  count_cuted_fields= backup->count_cuted_fields;
  transaction.savepoints= backup->savepoints;
  variables.option_bits= backup->option_bits;
  in_sub_stmt=      backup->in_sub_stmt;
  enable_slow_log=  backup->enable_slow_log;
  first_successful_insert_id_in_prev_stmt= 
    backup->first_successful_insert_id_in_prev_stmt;
  first_successful_insert_id_in_cur_stmt= 
    backup->first_successful_insert_id_in_cur_stmt;
  limit_found_rows= backup->limit_found_rows;
  sent_row_count=   backup->sent_row_count;
  client_capabilities= backup->client_capabilities;
  /*
    If we've left sub-statement mode, reset the fatal error flag.
    Otherwise keep the current value, to propagate it up the sub-statement
    stack.
  */
  if (!in_sub_stmt)
    is_fatal_sub_stmt_error= FALSE;

  if ((variables.option_bits & OPTION_BIN_LOG) && is_update_query(lex->sql_command) &&
       !is_current_stmt_binlog_format_row())
    mysql_bin_log.stop_union_events(this);

  /*
    The following is added to the old values as we are interested in the
    total complexity of the query
  */
  examined_row_count+= backup->examined_row_count;
  cuted_fields+=       backup->cuted_fields;
  DBUG_VOID_RETURN;
}


void THD::set_statement(Statement *stmt)
{
  mysql_mutex_lock(&LOCK_thd_data);
  Statement::set_statement(stmt);
  mysql_mutex_unlock(&LOCK_thd_data);
}


/** Assign a new value to thd->query.  */

void THD::set_query(const CSET_STRING &string_arg)
{
  mysql_mutex_lock(&LOCK_thd_data);
  set_query_inner(string_arg);
  mysql_mutex_unlock(&LOCK_thd_data);
}

/** Assign a new value to thd->query and thd->query_id.  */

void THD::set_query_and_id(char *query_arg, uint32 query_length_arg,
                           CHARSET_INFO *cs,
                           query_id_t new_query_id)
{
  mysql_mutex_lock(&LOCK_thd_data);
  set_query_inner(query_arg, query_length_arg, cs);
  query_id= new_query_id;
  mysql_mutex_unlock(&LOCK_thd_data);
}

/** Assign a new value to thd->query_id.  */

void THD::set_query_id(query_id_t new_query_id)
{
  mysql_mutex_lock(&LOCK_thd_data);
  query_id= new_query_id;
  mysql_mutex_unlock(&LOCK_thd_data);
}

/** Assign a new value to thd->mysys_var.  */
void THD::set_mysys_var(struct st_my_thread_var *new_mysys_var)
{
  mysql_mutex_lock(&LOCK_thd_data);
  mysys_var= new_mysys_var;
  mysql_mutex_unlock(&LOCK_thd_data);
}

/**
  Leave explicit LOCK TABLES or prelocked mode and restore value of
  transaction sentinel in MDL subsystem.
*/

void THD::leave_locked_tables_mode()
{
  if (locked_tables_mode == LTM_LOCK_TABLES)
  {
    /*
      When leaving LOCK TABLES mode we have to change the duration of most
      of the metadata locks being held, except for HANDLER and GRL locks,
      to transactional for them to be properly released at UNLOCK TABLES.
    */
    mdl_context.set_transaction_duration_for_all_locks();
    /*
      Make sure we don't release the global read lock and commit blocker
      when leaving LTM.
    */
    global_read_lock.set_explicit_lock_duration(this);
    /* Also ensure that we don't release metadata locks for open HANDLERs. */
    if (handler_tables_hash.records)
      mysql_ha_set_explicit_lock_duration(this);
  }
  locked_tables_mode= LTM_NONE;
}

void THD::get_definer(LEX_USER *definer)
{
  binlog_invoker();
#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
  if (slave_thread && has_invoker())
  {
    definer->user = invoker_user;
    definer->host= invoker_host;
    definer->password.str= NULL;
    definer->password.length= 0;
    definer->plugin.str= (char *) "";
    definer->plugin.length= 0;
    definer->auth.str=  (char *) "";
    definer->auth.length= 0;
  }
  else
#endif
    get_default_definer(this, definer);
}


/**
  Mark transaction to rollback and mark error as fatal to a sub-statement.

  @param  thd   Thread handle
  @param  all   TRUE <=> rollback main transaction.
*/

void mark_transaction_to_rollback(THD *thd, bool all)
{
  if (thd)
  {
    thd->is_fatal_sub_stmt_error= TRUE;
    thd->transaction_rollback_request= all;
    /*
      Aborted transactions can not be IGNOREd.
      Switch off the IGNORE flag for the current
      SELECT_LEX. This should allow my_error()
      to report the error and abort the execution
      flow, even in presence
      of IGNORE clause.
    */
    if (thd->lex->current_select)
      thd->lex->current_select->no_error= FALSE;
  }
}
/***************************************************************************
  Handling of XA id cacheing
***************************************************************************/

mysql_mutex_t LOCK_xid_cache;
HASH xid_cache;

extern "C" uchar *xid_get_hash_key(const uchar *, size_t *, my_bool);
extern "C" void xid_free_hash(void *);

uchar *xid_get_hash_key(const uchar *ptr, size_t *length,
                                  my_bool not_used __attribute__((unused)))
{
  *length=((XID_STATE*)ptr)->xid.key_length();
  return ((XID_STATE*)ptr)->xid.key();
}

void xid_free_hash(void *ptr)
{
  if (!((XID_STATE*)ptr)->in_thd)
    my_free(ptr);
}

#ifdef HAVE_PSI_INTERFACE
static PSI_mutex_key key_LOCK_xid_cache;

static PSI_mutex_info all_xid_mutexes[]=
{
  { &key_LOCK_xid_cache, "LOCK_xid_cache", PSI_FLAG_GLOBAL}
};

static void init_xid_psi_keys(void)
{
  const char* category= "sql";
  int count;

  if (PSI_server == NULL)
    return;

  count= array_elements(all_xid_mutexes);
  PSI_server->register_mutex(category, all_xid_mutexes, count);
}
#endif /* HAVE_PSI_INTERFACE */

bool xid_cache_init()
{
#ifdef HAVE_PSI_INTERFACE
  init_xid_psi_keys();
#endif

  mysql_mutex_init(key_LOCK_xid_cache, &LOCK_xid_cache, MY_MUTEX_INIT_FAST);
  return my_hash_init(&xid_cache, &my_charset_bin, 100, 0, 0,
                      xid_get_hash_key, xid_free_hash, 0) != 0;
}

void xid_cache_free()
{
  if (my_hash_inited(&xid_cache))
  {
    my_hash_free(&xid_cache);
    mysql_mutex_destroy(&LOCK_xid_cache);
  }
}

XID_STATE *xid_cache_search(XID *xid)
{
  mysql_mutex_lock(&LOCK_xid_cache);
  XID_STATE *res=(XID_STATE *)my_hash_search(&xid_cache, xid->key(),
                                             xid->key_length());
  mysql_mutex_unlock(&LOCK_xid_cache);
  return res;
}


bool xid_cache_insert(XID *xid, enum xa_states xa_state)
{
  XID_STATE *xs;
  my_bool res;
  mysql_mutex_lock(&LOCK_xid_cache);
  if (my_hash_search(&xid_cache, xid->key(), xid->key_length()))
    res=0;
  else if (!(xs=(XID_STATE *)my_malloc(sizeof(*xs), MYF(MY_WME))))
    res=1;
  else
  {
    xs->xa_state=xa_state;
    xs->xid.set(xid);
    xs->in_thd=0;
    xs->rm_error=0;
    res=my_hash_insert(&xid_cache, (uchar*)xs);
  }
  mysql_mutex_unlock(&LOCK_xid_cache);
  return res;
}


bool xid_cache_insert(XID_STATE *xid_state)
{
  mysql_mutex_lock(&LOCK_xid_cache);
  DBUG_ASSERT(my_hash_search(&xid_cache, xid_state->xid.key(),
                             xid_state->xid.key_length())==0);
  my_bool res=my_hash_insert(&xid_cache, (uchar*)xid_state);
  mysql_mutex_unlock(&LOCK_xid_cache);
  return res;
}


void xid_cache_delete(XID_STATE *xid_state)
{
  mysql_mutex_lock(&LOCK_xid_cache);
  my_hash_delete(&xid_cache, (uchar *)xid_state);
  mysql_mutex_unlock(&LOCK_xid_cache);
}


/**
  Decide on logging format to use for the statement and issue errors
  or warnings as needed.  The decision depends on the following
  parameters:

  - The logging mode, i.e., the value of binlog_format.  Can be
    statement, mixed, or row.

  - The type of statement.  There are three types of statements:
    "normal" safe statements; unsafe statements; and row injections.
    An unsafe statement is one that, if logged in statement format,
    might produce different results when replayed on the slave (e.g.,
    INSERT DELAYED).  A row injection is either a BINLOG statement, or
    a row event executed by the slave's SQL thread.

  - The capabilities of tables modified by the statement.  The
    *capabilities vector* for a table is a set of flags associated
    with the table.  Currently, it only includes two flags: *row
    capability flag* and *statement capability flag*.

    The row capability flag is set if and only if the engine can
    handle row-based logging. The statement capability flag is set if
    and only if the table can handle statement-based logging.

  Decision table for logging format
  ---------------------------------

  The following table summarizes how the format and generated
  warning/error depends on the tables' capabilities, the statement
  type, and the current binlog_format.

     Row capable        N NNNNNNNNN YYYYYYYYY YYYYYYYYY
     Statement capable  N YYYYYYYYY NNNNNNNNN YYYYYYYYY

     Statement type     * SSSUUUIII SSSUUUIII SSSUUUIII

     binlog_format      * SMRSMRSMR SMRSMRSMR SMRSMRSMR

     Logged format      - SS-S----- -RR-RR-RR SRRSRR-RR
     Warning/Error      1 --2732444 5--5--6-- ---7--6--

  Legend
  ------

  Row capable:    N - Some table not row-capable, Y - All tables row-capable
  Stmt capable:   N - Some table not stmt-capable, Y - All tables stmt-capable
  Statement type: (S)afe, (U)nsafe, or Row (I)njection
  binlog_format:  (S)TATEMENT, (M)IXED, or (R)OW
  Logged format:  (S)tatement or (R)ow
  Warning/Error:  Warnings and error messages are as follows:

  1. Error: Cannot execute statement: binlogging impossible since both
     row-incapable engines and statement-incapable engines are
     involved.

  2. Error: Cannot execute statement: binlogging impossible since
     BINLOG_FORMAT = ROW and at least one table uses a storage engine
     limited to statement-logging.

  3. Error: Cannot execute statement: binlogging of unsafe statement
     is impossible when storage engine is limited to statement-logging
     and BINLOG_FORMAT = MIXED.

  4. Error: Cannot execute row injection: binlogging impossible since
     at least one table uses a storage engine limited to
     statement-logging.

  5. Error: Cannot execute statement: binlogging impossible since
     BINLOG_FORMAT = STATEMENT and at least one table uses a storage
     engine limited to row-logging.

  6. Error: Cannot execute row injection: binlogging impossible since
     BINLOG_FORMAT = STATEMENT.

  7. Warning: Unsafe statement binlogged in statement format since
     BINLOG_FORMAT = STATEMENT.

  In addition, we can produce the following error (not depending on
  the variables of the decision diagram):

  8. Error: Cannot execute statement: binlogging impossible since more
     than one engine is involved and at least one engine is
     self-logging.

  For each error case above, the statement is prevented from being
  logged, we report an error, and roll back the statement.  For
  warnings, we set the thd->binlog_flags variable: the warning will be
  printed only if the statement is successfully logged.

  @see THD::binlog_query

  @param[in] thd    Client thread
  @param[in] tables Tables involved in the query

  @retval 0 No error; statement can be logged.
  @retval -1 One of the error conditions above applies (1, 2, 4, 5, or 6).
*/

int THD::decide_logging_format(TABLE_LIST *tables)
{
  DBUG_ENTER("THD::decide_logging_format");
  DBUG_PRINT("info", ("query: %s", query()));
  DBUG_PRINT("info", ("variables.binlog_format: %lu",
                      variables.binlog_format));
  DBUG_PRINT("info", ("lex->get_stmt_unsafe_flags(): 0x%x",
                      lex->get_stmt_unsafe_flags()));

  /*
    We should not decide logging format if the binlog is closed or
    binlogging is off, or if the statement is filtered out from the
    binlog by filtering rules.
  */
  if (mysql_bin_log.is_open() && (variables.option_bits & OPTION_BIN_LOG) &&
      !(variables.binlog_format == BINLOG_FORMAT_STMT &&
        !binlog_filter->db_ok(db)))
  {
    /*
      Compute one bit field with the union of all the engine
      capabilities, and one with the intersection of all the engine
      capabilities.
    */
    handler::Table_flags flags_write_some_set= 0;
    handler::Table_flags flags_access_some_set= 0;
    handler::Table_flags flags_write_all_set=
      HA_BINLOG_ROW_CAPABLE | HA_BINLOG_STMT_CAPABLE;

    /* 
       If different types of engines are about to be updated.
       For example: Innodb and Falcon; Innodb and MyIsam.
    */
    my_bool multi_write_engine= FALSE;
    /*
       If different types of engines are about to be accessed 
       and any of them is about to be updated. For example:
       Innodb and Falcon; Innodb and MyIsam.
    */
    my_bool multi_access_engine= FALSE;
    /*
      Identifies if a table is changed.
    */
    my_bool is_write= FALSE;
    /*
      A pointer to a previous table that was changed.
    */
    TABLE* prev_write_table= NULL;
    /*
      A pointer to a previous table that was accessed.
    */
    TABLE* prev_access_table= NULL;

#ifndef DBUG_OFF
    {
      static const char *prelocked_mode_name[] = {
        "NON_PRELOCKED",
        "PRELOCKED",
        "PRELOCKED_UNDER_LOCK_TABLES",
      };
      DBUG_PRINT("debug", ("prelocked_mode: %s",
                           prelocked_mode_name[locked_tables_mode]));
    }
#endif

    /*
      Get the capabilities vector for all involved storage engines and
      mask out the flags for the binary log.
    */
    for (TABLE_LIST *table= tables; table; table= table->next_global)
    {
      if (table->placeholder())
        continue;

      if (table->table->s->table_category == TABLE_CATEGORY_PERFORMANCE ||
          table->table->s->table_category == TABLE_CATEGORY_LOG)
        lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_SYSTEM_TABLE);

      handler::Table_flags const flags= table->table->file->ha_table_flags();

      DBUG_PRINT("info", ("table: %s; ha_table_flags: 0x%llx",
                          table->table_name, flags));
      if (table->lock_type >= TL_WRITE_ALLOW_WRITE)
      {
        if (prev_write_table && prev_write_table->file->ht !=
            table->table->file->ht)
          multi_write_engine= TRUE;

        my_bool trans= table->table->file->has_transactions();

        if (table->table->s->tmp_table)
          lex->set_stmt_accessed_table(trans ? LEX::STMT_WRITES_TEMP_TRANS_TABLE :
                                               LEX::STMT_WRITES_TEMP_NON_TRANS_TABLE);
        else
          lex->set_stmt_accessed_table(trans ? LEX::STMT_WRITES_TRANS_TABLE :
                                               LEX::STMT_WRITES_NON_TRANS_TABLE);

        flags_write_all_set &= flags;
        flags_write_some_set |= flags;
        is_write= TRUE;

        prev_write_table= table->table;

      }
      flags_access_some_set |= flags;

      if (lex->sql_command != SQLCOM_CREATE_TABLE ||
          (lex->sql_command == SQLCOM_CREATE_TABLE &&
          (lex->create_info.options & HA_LEX_CREATE_TMP_TABLE)))
      {
        my_bool trans= table->table->file->has_transactions();

        if (table->table->s->tmp_table)
          lex->set_stmt_accessed_table(trans ? LEX::STMT_READS_TEMP_TRANS_TABLE :
                                               LEX::STMT_READS_TEMP_NON_TRANS_TABLE);
        else
          lex->set_stmt_accessed_table(trans ? LEX::STMT_READS_TRANS_TABLE :
                                               LEX::STMT_READS_NON_TRANS_TABLE);
      }

      if (prev_access_table && prev_access_table->file->ht !=
          table->table->file->ht)
        multi_access_engine= TRUE;

      prev_access_table= table->table;
    }

    DBUG_PRINT("info", ("flags_write_all_set: 0x%llx", flags_write_all_set));
    DBUG_PRINT("info", ("flags_write_some_set: 0x%llx", flags_write_some_set));
    DBUG_PRINT("info", ("flags_access_some_set: 0x%llx", flags_access_some_set));
    DBUG_PRINT("info", ("multi_write_engine: %d", multi_write_engine));
    DBUG_PRINT("info", ("multi_access_engine: %d", multi_access_engine));

    int error= 0;
    int unsafe_flags;

    bool multi_stmt_trans= in_multi_stmt_transaction_mode();
    bool trans_table= trans_has_updated_trans_table(this);
    bool binlog_direct= variables.binlog_direct_non_trans_update;

    if (lex->is_mixed_stmt_unsafe(multi_stmt_trans, binlog_direct,
                                  trans_table, tx_isolation))
      lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_MIXED_STATEMENT);
    else if (multi_stmt_trans && trans_table && !binlog_direct &&
             lex->stmt_accessed_table(LEX::STMT_WRITES_NON_TRANS_TABLE))
      lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_NONTRANS_AFTER_TRANS);

    /*
      If more than one engine is involved in the statement and at
      least one is doing it's own logging (is *self-logging*), the
      statement cannot be logged atomically, so we generate an error
      rather than allowing the binlog to become corrupt.
    */
    if (multi_write_engine &&
        (flags_write_some_set & HA_HAS_OWN_BINLOGGING))
      my_error((error= ER_BINLOG_MULTIPLE_ENGINES_AND_SELF_LOGGING_ENGINE),
               MYF(0));
    else if (multi_access_engine && flags_access_some_set & HA_HAS_OWN_BINLOGGING)
      lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_MULTIPLE_ENGINES_AND_SELF_LOGGING_ENGINE);

    /* both statement-only and row-only engines involved */
    if ((flags_write_all_set & (HA_BINLOG_STMT_CAPABLE | HA_BINLOG_ROW_CAPABLE)) == 0)
    {
      /*
        1. Error: Binary logging impossible since both row-incapable
           engines and statement-incapable engines are involved
      */
      my_error((error= ER_BINLOG_ROW_ENGINE_AND_STMT_ENGINE), MYF(0));
    }
    /* statement-only engines involved */
    else if ((flags_write_all_set & HA_BINLOG_ROW_CAPABLE) == 0)
    {
      if (lex->is_stmt_row_injection())
      {
        /*
          4. Error: Cannot execute row injection since table uses
             storage engine limited to statement-logging
        */
        my_error((error= ER_BINLOG_ROW_INJECTION_AND_STMT_ENGINE), MYF(0));
      }
      else if (variables.binlog_format == BINLOG_FORMAT_ROW &&
               sqlcom_can_generate_row_events(this))
      {
        /*
          2. Error: Cannot modify table that uses a storage engine
             limited to statement-logging when BINLOG_FORMAT = ROW
        */
        my_error((error= ER_BINLOG_ROW_MODE_AND_STMT_ENGINE), MYF(0));
      }
      else if ((unsafe_flags= lex->get_stmt_unsafe_flags()) != 0)
      {
        /*
          3. Error: Cannot execute statement: binlogging of unsafe
             statement is impossible when storage engine is limited to
             statement-logging and BINLOG_FORMAT = MIXED.
        */
        for (int unsafe_type= 0;
             unsafe_type < LEX::BINLOG_STMT_UNSAFE_COUNT;
             unsafe_type++)
          if (unsafe_flags & (1 << unsafe_type))
            my_error((error= ER_BINLOG_UNSAFE_AND_STMT_ENGINE), MYF(0),
                     ER(LEX::binlog_stmt_unsafe_errcode[unsafe_type]));
      }
      /* log in statement format! */
    }
    /* no statement-only engines */
    else
    {
      /* binlog_format = STATEMENT */
      if (variables.binlog_format == BINLOG_FORMAT_STMT)
      {
        if (lex->is_stmt_row_injection())
        {
          /*
            6. Error: Cannot execute row injection since
               BINLOG_FORMAT = STATEMENT
          */
          my_error((error= ER_BINLOG_ROW_INJECTION_AND_STMT_MODE), MYF(0));
        }
        else if ((flags_write_all_set & HA_BINLOG_STMT_CAPABLE) == 0 &&
                 sqlcom_can_generate_row_events(this))
        {
          /*
            5. Error: Cannot modify table that uses a storage engine
               limited to row-logging when binlog_format = STATEMENT
          */
          my_error((error= ER_BINLOG_STMT_MODE_AND_ROW_ENGINE), MYF(0), "");
        }
        else if (is_write && (unsafe_flags= lex->get_stmt_unsafe_flags()) != 0)
        {
          /*
            7. Warning: Unsafe statement logged as statement due to
               binlog_format = STATEMENT
          */
          binlog_unsafe_warning_flags|= unsafe_flags;

          DBUG_PRINT("info", ("Scheduling warning to be issued by "
                              "binlog_query: '%s'",
                              ER(ER_BINLOG_UNSAFE_STATEMENT)));
          DBUG_PRINT("info", ("binlog_unsafe_warning_flags: 0x%x",
                              binlog_unsafe_warning_flags));
        }
        /* log in statement format! */
      }
      /* No statement-only engines and binlog_format != STATEMENT.
         I.e., nothing prevents us from row logging if needed. */
      else
      {
        if (lex->is_stmt_unsafe() || lex->is_stmt_row_injection()
            || (flags_write_all_set & HA_BINLOG_STMT_CAPABLE) == 0)
        {
          /* log in row format! */
          set_current_stmt_binlog_format_row_if_mixed();
        }
      }
    }

    if (error) {
      DBUG_PRINT("info", ("decision: no logging since an error was generated"));
      DBUG_RETURN(-1);
    }
    DBUG_PRINT("info", ("decision: logging in %s format",
                        is_current_stmt_binlog_format_row() ?
                        "ROW" : "STATEMENT"));
  }
#ifndef DBUG_OFF
  else
    DBUG_PRINT("info", ("decision: no logging since "
                        "mysql_bin_log.is_open() = %d "
                        "and (options & OPTION_BIN_LOG) = 0x%llx "
                        "and binlog_format = %lu "
                        "and binlog_filter->db_ok(db) = %d",
                        mysql_bin_log.is_open(),
                        (variables.option_bits & OPTION_BIN_LOG),
                        variables.binlog_format,
                        binlog_filter->db_ok(db)));
#endif

  DBUG_RETURN(0);
}


/*
  Implementation of interface to write rows to the binary log through the
  thread.  The thread is responsible for writing the rows it has
  inserted/updated/deleted.
*/

#ifndef MYSQL_CLIENT

/*
  Template member function for ensuring that there is an rows log
  event of the apropriate type before proceeding.

  PRE CONDITION:
    - Events of type 'RowEventT' have the type code 'type_code'.
    
  POST CONDITION:
    If a non-NULL pointer is returned, the pending event for thread 'thd' will
    be an event of type 'RowEventT' (which have the type code 'type_code')
    will either empty or have enough space to hold 'needed' bytes.  In
    addition, the columns bitmap will be correct for the row, meaning that
    the pending event will be flushed if the columns in the event differ from
    the columns suppled to the function.

  RETURNS
    If no error, a non-NULL pending event (either one which already existed or
    the newly created one).
    If error, NULL.
 */

template <class RowsEventT> Rows_log_event* 
THD::binlog_prepare_pending_rows_event(TABLE* table, uint32 serv_id,
                                       MY_BITMAP const* cols,
                                       size_t colcnt,
                                       size_t needed,
                                       bool is_transactional,
				       RowsEventT *hint __attribute__((unused)))
{
  DBUG_ENTER("binlog_prepare_pending_rows_event");
  /* Pre-conditions */
  DBUG_ASSERT(table->s->table_map_id != ~0UL);

  /* Fetch the type code for the RowsEventT template parameter */
  int const type_code= RowsEventT::TYPE_CODE;

  /*
    There is no good place to set up the transactional data, so we
    have to do it here.
  */
  if (binlog_setup_trx_data())
    DBUG_RETURN(NULL);

  Rows_log_event* pending= binlog_get_pending_rows_event(is_transactional);

  if (unlikely(pending && !pending->is_valid()))
    DBUG_RETURN(NULL);

  /*
    Check if the current event is non-NULL and a write-rows
    event. Also check if the table provided is mapped: if it is not,
    then we have switched to writing to a new table.
    If there is no pending event, we need to create one. If there is a pending
    event, but it's not about the same table id, or not of the same type
    (between Write, Update and Delete), or not the same affected columns, or
    going to be too big, flush this event to disk and create a new pending
    event.
  */
  if (!pending ||
      pending->server_id != serv_id || 
      pending->get_table_id() != table->s->table_map_id ||
      pending->get_type_code() != type_code || 
      pending->get_data_size() + needed > opt_binlog_rows_event_max_size || 
      pending->get_width() != colcnt ||
      !bitmap_cmp(pending->get_cols(), cols)) 
  {
    /* Create a new RowsEventT... */
    Rows_log_event* const
	ev= new RowsEventT(this, table, table->s->table_map_id, cols,
                           is_transactional);
    if (unlikely(!ev))
      DBUG_RETURN(NULL);
    ev->server_id= serv_id; // I don't like this, it's too easy to forget.
    /*
      flush the pending event and replace it with the newly created
      event...
    */
    if (unlikely(
        mysql_bin_log.flush_and_set_pending_rows_event(this, ev,
                                                       is_transactional)))
    {
      delete ev;
      DBUG_RETURN(NULL);
    }

    DBUG_RETURN(ev);               /* This is the new pending event */
  }
  DBUG_RETURN(pending);        /* This is the current pending event */
}

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
/*
  Instantiate the versions we need, we have -fno-implicit-template as
  compiling option.
*/
template Rows_log_event*
THD::binlog_prepare_pending_rows_event(TABLE*, uint32, MY_BITMAP const*,
				       size_t, size_t, bool,
				       Write_rows_log_event*);

template Rows_log_event*
THD::binlog_prepare_pending_rows_event(TABLE*, uint32, MY_BITMAP const*,
				       size_t colcnt, size_t, bool,
				       Delete_rows_log_event *);

template Rows_log_event* 
THD::binlog_prepare_pending_rows_event(TABLE*, uint32, MY_BITMAP const*,
				       size_t colcnt, size_t, bool,
				       Update_rows_log_event *);
#endif

/* Declare in unnamed namespace. */
CPP_UNNAMED_NS_START

  /**
     Class to handle temporary allocation of memory for row data.

     The responsibilities of the class is to provide memory for
     packing one or two rows of packed data (depending on what
     constructor is called).

     In order to make the allocation more efficient for "simple" rows,
     i.e., rows that do not contain any blobs, a pointer to the
     allocated memory is of memory is stored in the table structure
     for simple rows.  If memory for a table containing a blob field
     is requested, only memory for that is allocated, and subsequently
     released when the object is destroyed.

   */
  class Row_data_memory {
  public:
    /**
      Build an object to keep track of a block-local piece of memory
      for storing a row of data.

      @param table
      Table where the pre-allocated memory is stored.

      @param length
      Length of data that is needed, if the record contain blobs.
     */
    Row_data_memory(TABLE *table, size_t const len1)
      : m_memory(0)
    {
#ifndef DBUG_OFF
      m_alloc_checked= FALSE;
#endif
      allocate_memory(table, len1);
      m_ptr[0]= has_memory() ? m_memory : 0;
      m_ptr[1]= 0;
    }

    Row_data_memory(TABLE *table, size_t const len1, size_t const len2)
      : m_memory(0)
    {
#ifndef DBUG_OFF
      m_alloc_checked= FALSE;
#endif
      allocate_memory(table, len1 + len2);
      m_ptr[0]= has_memory() ? m_memory        : 0;
      m_ptr[1]= has_memory() ? m_memory + len1 : 0;
    }

    ~Row_data_memory()
    {
      if (m_memory != 0 && m_release_memory_on_destruction)
        my_free(m_memory);
    }

    /**
       Is there memory allocated?

       @retval true There is memory allocated
       @retval false Memory allocation failed
     */
    bool has_memory() const {
#ifndef DBUG_OFF
      m_alloc_checked= TRUE;
#endif
      return m_memory != 0;
    }

    uchar *slot(uint s)
    {
      DBUG_ASSERT(s < sizeof(m_ptr)/sizeof(*m_ptr));
      DBUG_ASSERT(m_ptr[s] != 0);
      DBUG_ASSERT(m_alloc_checked == TRUE);
      return m_ptr[s];
    }

  private:
    void allocate_memory(TABLE *const table, size_t const total_length)
    {
      if (table->s->blob_fields == 0)
      {
        /*
          The maximum length of a packed record is less than this
          length. We use this value instead of the supplied length
          when allocating memory for records, since we don't know how
          the memory will be used in future allocations.

          Since table->s->reclength is for unpacked records, we have
          to add two bytes for each field, which can potentially be
          added to hold the length of a packed field.
        */
        size_t const maxlen= table->s->reclength + 2 * table->s->fields;

        /*
          Allocate memory for two records if memory hasn't been
          allocated. We allocate memory for two records so that it can
          be used when processing update rows as well.
        */
        if (table->write_row_record == 0)
          table->write_row_record=
            (uchar *) alloc_root(&table->mem_root, 2 * maxlen);
        m_memory= table->write_row_record;
        m_release_memory_on_destruction= FALSE;
      }
      else
      {
        m_memory= (uchar *) my_malloc(total_length, MYF(MY_WME));
        m_release_memory_on_destruction= TRUE;
      }
    }

#ifndef DBUG_OFF
    mutable bool m_alloc_checked;
#endif
    bool m_release_memory_on_destruction;
    uchar *m_memory;
    uchar *m_ptr[2];
  };

CPP_UNNAMED_NS_END

int THD::binlog_write_row(TABLE* table, bool is_trans, 
                          MY_BITMAP const* cols, size_t colcnt, 
                          uchar const *record) 
{ 
  DBUG_ASSERT(is_current_stmt_binlog_format_row() && mysql_bin_log.is_open());

  /*
    Pack records into format for transfer. We are allocating more
    memory than needed, but that doesn't matter.
  */
  Row_data_memory memory(table, max_row_length(table, record));
  if (!memory.has_memory())
    return HA_ERR_OUT_OF_MEM;

  uchar *row_data= memory.slot(0);

  size_t const len= pack_row(table, cols, row_data, record);

  Rows_log_event* const ev=
    binlog_prepare_pending_rows_event(table, server_id, cols, colcnt,
                                      len, is_trans,
                                      static_cast<Write_rows_log_event*>(0));

  if (unlikely(ev == 0))
    return HA_ERR_OUT_OF_MEM;

  return ev->add_row_data(row_data, len);
}

int THD::binlog_update_row(TABLE* table, bool is_trans,
                           MY_BITMAP const* cols, size_t colcnt,
                           const uchar *before_record,
                           const uchar *after_record)
{ 
  DBUG_ASSERT(is_current_stmt_binlog_format_row() && mysql_bin_log.is_open());

  size_t const before_maxlen = max_row_length(table, before_record);
  size_t const after_maxlen  = max_row_length(table, after_record);

  Row_data_memory row_data(table, before_maxlen, after_maxlen);
  if (!row_data.has_memory())
    return HA_ERR_OUT_OF_MEM;

  uchar *before_row= row_data.slot(0);
  uchar *after_row= row_data.slot(1);

  size_t const before_size= pack_row(table, cols, before_row,
                                        before_record);
  size_t const after_size= pack_row(table, cols, after_row,
                                       after_record);

  /*
    Don't print debug messages when running valgrind since they can
    trigger false warnings.
   */
#ifndef HAVE_purify
  DBUG_DUMP("before_record", before_record, table->s->reclength);
  DBUG_DUMP("after_record",  after_record, table->s->reclength);
  DBUG_DUMP("before_row",    before_row, before_size);
  DBUG_DUMP("after_row",     after_row, after_size);
#endif

  Rows_log_event* const ev=
    binlog_prepare_pending_rows_event(table, server_id, cols, colcnt,
				      before_size + after_size, is_trans,
				      static_cast<Update_rows_log_event*>(0));

  if (unlikely(ev == 0))
    return HA_ERR_OUT_OF_MEM;

  return
    ev->add_row_data(before_row, before_size) ||
    ev->add_row_data(after_row, after_size);
}

int THD::binlog_delete_row(TABLE* table, bool is_trans, 
                           MY_BITMAP const* cols, size_t colcnt,
                           uchar const *record)
{ 
  DBUG_ASSERT(is_current_stmt_binlog_format_row() && mysql_bin_log.is_open());

  /* 
     Pack records into format for transfer. We are allocating more
     memory than needed, but that doesn't matter.
  */
  Row_data_memory memory(table, max_row_length(table, record));
  if (unlikely(!memory.has_memory()))
    return HA_ERR_OUT_OF_MEM;

  uchar *row_data= memory.slot(0);

  size_t const len= pack_row(table, cols, row_data, record);

  Rows_log_event* const ev=
    binlog_prepare_pending_rows_event(table, server_id, cols, colcnt,
				      len, is_trans,
				      static_cast<Delete_rows_log_event*>(0));

  if (unlikely(ev == 0))
    return HA_ERR_OUT_OF_MEM;

  return ev->add_row_data(row_data, len);
}


int THD::binlog_remove_pending_rows_event(bool clear_maps,
                                          bool is_transactional)
{
  DBUG_ENTER("THD::binlog_remove_pending_rows_event");

  if (!mysql_bin_log.is_open())
    DBUG_RETURN(0);

  mysql_bin_log.remove_pending_rows_event(this, is_transactional);

  if (clear_maps)
    binlog_table_maps= 0;

  DBUG_RETURN(0);
}

int THD::binlog_flush_pending_rows_event(bool stmt_end, bool is_transactional)
{
  DBUG_ENTER("THD::binlog_flush_pending_rows_event");
  /*
    We shall flush the pending event even if we are not in row-based
    mode: it might be the case that we left row-based mode before
    flushing anything (e.g., if we have explicitly locked tables).
   */
  if (!mysql_bin_log.is_open())
    DBUG_RETURN(0);

  /*
    Mark the event as the last event of a statement if the stmt_end
    flag is set.
  */
  int error= 0;
  if (Rows_log_event *pending= binlog_get_pending_rows_event(is_transactional))
  {
    if (stmt_end)
    {
      pending->set_flags(Rows_log_event::STMT_END_F);
      binlog_table_maps= 0;
    }

    error= mysql_bin_log.flush_and_set_pending_rows_event(this, 0,
                                                          is_transactional);
  }

  DBUG_RETURN(error);
}


#if !defined(DBUG_OFF) && !defined(_lint)
static const char *
show_query_type(THD::enum_binlog_query_type qtype)
{
  switch (qtype) {
  case THD::ROW_QUERY_TYPE:
    return "ROW";
  case THD::STMT_QUERY_TYPE:
    return "STMT";
  case THD::QUERY_TYPE_COUNT:
  default:
    DBUG_ASSERT(0 <= qtype && qtype < THD::QUERY_TYPE_COUNT);
  }
  static char buf[64];
  sprintf(buf, "UNKNOWN#%d", qtype);
  return buf;
}
#endif

/*
  Constants required for the limit unsafe warnings suppression
*/
//seconds after which the limit unsafe warnings suppression will be activated
#define LIMIT_UNSAFE_WARNING_ACTIVATION_TIMEOUT 50
//number of limit unsafe warnings after which the suppression will be activated
#define LIMIT_UNSAFE_WARNING_ACTIVATION_THRESHOLD_COUNT 50

static ulonglong limit_unsafe_suppression_start_time= 0;
static bool unsafe_warning_suppression_is_activated= false;
static int limit_unsafe_warning_count= 0;

/**
  Auxiliary function to reset the limit unsafety warning suppression.
*/
static void reset_binlog_unsafe_suppression()
{
  DBUG_ENTER("reset_binlog_unsafe_suppression");
  unsafe_warning_suppression_is_activated= false;
  limit_unsafe_warning_count= 0;
  limit_unsafe_suppression_start_time= my_getsystime()/10000000;
  DBUG_VOID_RETURN;
}

/**
  Auxiliary function to print warning in the error log.
*/
static void print_unsafe_warning_to_log(int unsafe_type, char* buf,
                                 char* query)
{
  DBUG_ENTER("print_unsafe_warning_in_log");
  sprintf(buf, ER(ER_BINLOG_UNSAFE_STATEMENT),
          ER(LEX::binlog_stmt_unsafe_errcode[unsafe_type]));
  sql_print_warning(ER(ER_MESSAGE_AND_STATEMENT), buf, query);
  DBUG_VOID_RETURN;
}

/**
  Auxiliary function to check if the warning for limit unsafety should be
  thrown or suppressed. Details of the implementation can be found in the
  comments inline.
  SYNOPSIS:
  @params
   buf         - buffer to hold the warning message text
   unsafe_type - The type of unsafety.
   query       - The actual query statement.

  TODO: Remove this function and implement a general service for all warnings
  that would prevent flooding the error log.
*/
static void do_unsafe_limit_checkout(char* buf, int unsafe_type, char* query)
{
  ulonglong now= 0;
  DBUG_ENTER("do_unsafe_limit_checkout");
  DBUG_ASSERT(unsafe_type == LEX::BINLOG_STMT_UNSAFE_LIMIT);
  limit_unsafe_warning_count++;
  /*
    INITIALIZING:
    If this is the first time this function is called with log warning
    enabled, the monitoring the unsafe warnings should start.
  */
  if (limit_unsafe_suppression_start_time == 0)
  {
    limit_unsafe_suppression_start_time= my_getsystime()/10000000;
    print_unsafe_warning_to_log(unsafe_type, buf, query);
  }
  else
  {
    if (!unsafe_warning_suppression_is_activated)
      print_unsafe_warning_to_log(unsafe_type, buf, query);

    if (limit_unsafe_warning_count >=
        LIMIT_UNSAFE_WARNING_ACTIVATION_THRESHOLD_COUNT)
    {
      now= my_getsystime()/10000000;
      if (!unsafe_warning_suppression_is_activated)
      {
        /*
          ACTIVATION:
          We got LIMIT_UNSAFE_WARNING_ACTIVATION_THRESHOLD_COUNT warnings in
          less than LIMIT_UNSAFE_WARNING_ACTIVATION_TIMEOUT we activate the
          suppression.
        */
        if ((now-limit_unsafe_suppression_start_time) <=
                       LIMIT_UNSAFE_WARNING_ACTIVATION_TIMEOUT)
        {
          unsafe_warning_suppression_is_activated= true;
          DBUG_PRINT("info",("A warning flood has been detected and the limit \
unsafety warning suppression has been activated."));
        }
        else
        {
          /*
           there is no flooding till now, therefore we restart the monitoring
          */
          limit_unsafe_suppression_start_time= my_getsystime()/10000000;
          limit_unsafe_warning_count= 0;
        }
      }
      else
      {
        /*
          Print the suppression note and the unsafe warning.
        */
        sql_print_information("The following warning was suppressed %d times \
during the last %d seconds in the error log",
                              limit_unsafe_warning_count,
                              (int)
                              (now-limit_unsafe_suppression_start_time));
        print_unsafe_warning_to_log(unsafe_type, buf, query);
        /*
          DEACTIVATION: We got LIMIT_UNSAFE_WARNING_ACTIVATION_THRESHOLD_COUNT
          warnings in more than  LIMIT_UNSAFE_WARNING_ACTIVATION_TIMEOUT, the
          suppression should be deactivated.
        */
        if ((now - limit_unsafe_suppression_start_time) >
            LIMIT_UNSAFE_WARNING_ACTIVATION_TIMEOUT)
        {
          reset_binlog_unsafe_suppression();
          DBUG_PRINT("info",("The limit unsafety warning supression has been \
deactivated"));
        }
      }
      limit_unsafe_warning_count= 0;
    }
  }
  DBUG_VOID_RETURN;
}

/**
  Auxiliary method used by @c binlog_query() to raise warnings.

  The type of warning and the type of unsafeness is stored in
  THD::binlog_unsafe_warning_flags.
*/
void THD::issue_unsafe_warnings()
{
  char buf[MYSQL_ERRMSG_SIZE * 2];
  DBUG_ENTER("issue_unsafe_warnings");
  /*
    Ensure that binlog_unsafe_warning_flags is big enough to hold all
    bits.  This is actually a constant expression.
  */
  DBUG_ASSERT(LEX::BINLOG_STMT_UNSAFE_COUNT <=
              sizeof(binlog_unsafe_warning_flags) * CHAR_BIT);

  uint32 unsafe_type_flags= binlog_unsafe_warning_flags;
  /*
    For each unsafe_type, check if the statement is unsafe in this way
    and issue a warning.
  */
  for (int unsafe_type=0;
       unsafe_type < LEX::BINLOG_STMT_UNSAFE_COUNT;
       unsafe_type++)
  {
    if ((unsafe_type_flags & (1 << unsafe_type)) != 0)
    {
      push_warning_printf(this, MYSQL_ERROR::WARN_LEVEL_NOTE,
                          ER_BINLOG_UNSAFE_STATEMENT,
                          ER(ER_BINLOG_UNSAFE_STATEMENT),
                          ER(LEX::binlog_stmt_unsafe_errcode[unsafe_type]));
      if (global_system_variables.log_warnings)
      {
        if (unsafe_type == LEX::BINLOG_STMT_UNSAFE_LIMIT)
          do_unsafe_limit_checkout( buf, unsafe_type, query());
        else //cases other than LIMIT unsafety
          print_unsafe_warning_to_log(unsafe_type, buf, query());
      }
    }
  }
  DBUG_VOID_RETURN;
}

/**
  Log the current query.

  The query will be logged in either row format or statement format
  depending on the value of @c current_stmt_binlog_format_row field and
  the value of the @c qtype parameter.

  This function must be called:

  - After the all calls to ha_*_row() functions have been issued.

  - After any writes to system tables. Rationale: if system tables
    were written after a call to this function, and the master crashes
    after the call to this function and before writing the system
    tables, then the master and slave get out of sync.

  - Before tables are unlocked and closed.

  @see decide_logging_format

  @retval 0 Success

  @retval nonzero If there is a failure when writing the query (e.g.,
  write failure), then the error code is returned.
*/
int THD::binlog_query(THD::enum_binlog_query_type qtype, char const *query_arg,
                      ulong query_len, bool is_trans, bool direct, 
                      bool suppress_use, int errcode)
{
  DBUG_ENTER("THD::binlog_query");
  DBUG_PRINT("enter", ("qtype: %s  query: '%s'",
                       show_query_type(qtype), query_arg));
  DBUG_ASSERT(query_arg && mysql_bin_log.is_open());

  /*
    If we are not in prelocked mode, mysql_unlock_tables() will be
    called after this binlog_query(), so we have to flush the pending
    rows event with the STMT_END_F set to unlock all tables at the
    slave side as well.

    If we are in prelocked mode, the flushing will be done inside the
    top-most close_thread_tables().
  */
  if (this->locked_tables_mode <= LTM_LOCK_TABLES)
    if (int error= binlog_flush_pending_rows_event(TRUE, is_trans))
      DBUG_RETURN(error);

  /*
    Warnings for unsafe statements logged in statement format are
    printed in three places instead of in decide_logging_format().
    This is because the warnings should be printed only if the statement
    is actually logged. When executing decide_logging_format(), we cannot
    know for sure if the statement will be logged:

    1 - sp_head::execute_procedure which prints out warnings for calls to
    stored procedures.

    2 - sp_head::execute_function which prints out warnings for calls
    involving functions.

    3 - THD::binlog_query (here) which prints warning for top level
    statements not covered by the two cases above: i.e., if not insided a
    procedure and a function.

    Besides, we should not try to print these warnings if it is not
    possible to write statements to the binary log as it happens when
    the execution is inside a function, or generaly speaking, when
    the variables.option_bits & OPTION_BIN_LOG is false.
    
  */
  if ((variables.option_bits & OPTION_BIN_LOG) &&
      spcont == NULL && !binlog_evt_union.do_union)
    issue_unsafe_warnings();


  switch (qtype) {
    /*
      ROW_QUERY_TYPE means that the statement may be logged either in
      row format or in statement format.  If
      current_stmt_binlog_format is row, it means that the
      statement has already been logged in row format and hence shall
      not be logged again.
    */
  case THD::ROW_QUERY_TYPE:
    DBUG_PRINT("debug",
               ("is_current_stmt_binlog_format_row: %d",
                is_current_stmt_binlog_format_row()));
    if (is_current_stmt_binlog_format_row())
      DBUG_RETURN(0);
    /* Fall through */

    /*
      STMT_QUERY_TYPE means that the query must be logged in statement
      format; it cannot be logged in row format.  This is typically
      used by DDL statements.  It is an error to use this query type
      if current_stmt_binlog_format_row is row.

      @todo Currently there are places that call this method with
      STMT_QUERY_TYPE and current_stmt_binlog_format is row.  Fix those
      places and add assert to ensure correct behavior. /Sven
    */
  case THD::STMT_QUERY_TYPE:
    /*
      The MYSQL_LOG::write() function will set the STMT_END_F flag and
      flush the pending rows event if necessary.
    */
    {
      Query_log_event qinfo(this, query_arg, query_len, is_trans, direct,
                            suppress_use, errcode);
      /*
        Binlog table maps will be irrelevant after a Query_log_event
        (they are just removed on the slave side) so after the query
        log event is written to the binary log, we pretend that no
        table maps were written.
       */
      int error= mysql_bin_log.write(&qinfo);
      binlog_table_maps= 0;
      DBUG_RETURN(error);
    }
    break;

  case THD::QUERY_TYPE_COUNT:
  default:
    DBUG_ASSERT(0 <= qtype && qtype < QUERY_TYPE_COUNT);
  }
  DBUG_RETURN(0);
}

bool Discrete_intervals_list::append(ulonglong start, ulonglong val,
                                 ulonglong incr)
{
  DBUG_ENTER("Discrete_intervals_list::append");
  /* first, see if this can be merged with previous */
  if ((head == NULL) || tail->merge_if_contiguous(start, val, incr))
  {
    /* it cannot, so need to add a new interval */
    Discrete_interval *new_interval= new Discrete_interval(start, val, incr);
    DBUG_RETURN(append(new_interval));
  }
  DBUG_RETURN(0);
}

bool Discrete_intervals_list::append(Discrete_interval *new_interval)
{
  DBUG_ENTER("Discrete_intervals_list::append");
  if (unlikely(new_interval == NULL))
    DBUG_RETURN(1);
  DBUG_PRINT("info",("adding new auto_increment interval"));
  if (head == NULL)
    head= current= new_interval;
  else
    tail->next= new_interval;
  tail= new_interval;
  elements++;
  DBUG_RETURN(0);
}

#endif /* !defined(MYSQL_CLIENT) */

void THD::set_user_connect(USER_CONN *uc)
{
  DBUG_ENTER("THD::set_user_connect");

  m_user_connect= uc;

  DBUG_VOID_RETURN;
}

void THD::increment_user_connections_counter()
{
  DBUG_ENTER("THD::increment_user_connections_counter");

  m_user_connect->connections++;

  DBUG_VOID_RETURN;
}

void THD::decrement_user_connections_counter()
{
  DBUG_ENTER("THD::decrement_user_connections_counter");

  DBUG_ASSERT(m_user_connect->connections > 0);
  m_user_connect->connections--;

  DBUG_VOID_RETURN;
}

void THD::increment_con_per_hour_counter()
{
  DBUG_ENTER("THD::decrement_conn_per_hour_counter");

  m_user_connect->conn_per_hour++;

  DBUG_VOID_RETURN;
}

void THD::increment_updates_counter()
{
  DBUG_ENTER("THD::increment_updates_counter");

  m_user_connect->updates++;

  DBUG_VOID_RETURN;
}

void THD::increment_questions_counter()
{
  DBUG_ENTER("THD::increment_updates_counter");

  m_user_connect->questions++;

  DBUG_VOID_RETURN;
}

/*
  Reset per-hour user resource limits when it has been more than
  an hour since they were last checked

  SYNOPSIS:
    time_out_user_resource_limits()

  NOTE:
    This assumes that the LOCK_user_conn mutex has been acquired, so it is
    safe to test and modify members of the USER_CONN structure.
*/
void THD::time_out_user_resource_limits()
{
  mysql_mutex_assert_owner(&LOCK_user_conn);
  ulonglong check_time= start_utime;
  DBUG_ENTER("time_out_user_resource_limits");

  /* If more than a hour since last check, reset resource checking */
  if (check_time - m_user_connect->reset_utime >= LL(3600000000))
  {
    m_user_connect->questions=1;
    m_user_connect->updates=0;
    m_user_connect->conn_per_hour=0;
    m_user_connect->reset_utime= check_time;
  }

  DBUG_VOID_RETURN;
}
