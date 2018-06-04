/*
   Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_class.h"

#include "mysys_err.h"                       // EE_DELETE
#include "connection_handler_manager.h"      // Connection_handler_manager
#include "debug_sync.h"                      // DEBUG_SYNC
#include "lock.h"                            // mysql_lock_abort_for_thread
#include "locking_service.h"                 // release_all_locking_service_locks
#include "mysqld_thd_manager.h"              // Global_THD_manager
#include "parse_tree_nodes.h"                // PT_select_var
#include "rpl_filter.h"                      // binlog_filter
#include "rpl_rli.h"                         // Relay_log_info
#include "sp_cache.h"                        // sp_cache_clear
#include "sp_rcontext.h"                     // sp_rcontext
#include "sql_audit.h"                       // mysql_audit_release
#include "sql_base.h"                        // close_temporary_tables
#include "sql_callback.h"                    // MYSQL_CALLBACK
#include "sql_handler.h"                     // mysql_ha_cleanup
#include "sql_parse.h"                       // is_update_query
#include "sql_plugin.h"                      // plugin_unlock
#include "sql_prepare.h"                     // Prepared_statement
#include "sql_time.h"                        // my_timeval_trunc
#include "sql_timer.h"                       // thd_timer_destroy
#include "sql_thd_internal_api.h"
#include "transaction.h"                     // trans_rollback
#ifdef HAVE_REPLICATION
#include "rpl_rli_pdb.h"                     // Slave_worker
#include "rpl_slave_commit_order_manager.h"
#endif

#include "pfs_file_provider.h"
#include "mysql/psi/mysql_file.h"

#include "pfs_idle_provider.h"
#include "mysql/psi/mysql_idle.h"

#include "mysql/psi/mysql_ps.h"

using std::min;
using std::max;

/*
  The following is used to initialise Table_ident with a internal
  table name
*/
char internal_table_name[2]= "";
char empty_c_string[1]= {0};    /* used for not defined db */

LEX_STRING EMPTY_STR= { (char *) "", 0 };
LEX_STRING NULL_STR=  { NULL, 0 };
LEX_CSTRING EMPTY_CSTR= { "", 0 };
LEX_CSTRING NULL_CSTR=  { NULL, 0 };

const char * const THD::DEFAULT_WHERE= "field list";


void THD::Transaction_state::backup(THD *thd)
{
  this->m_sql_command= thd->lex->sql_command;
  this->m_trx= thd->get_transaction();

  for (int i= 0; i < MAX_HA; ++i)
    this->m_ha_data[i]= thd->ha_data[i];

  this->m_tx_isolation= thd->tx_isolation;
  this->m_tx_read_only= thd->tx_read_only;
  this->m_thd_option_bits= thd->variables.option_bits;
  this->m_sql_mode= thd->variables.sql_mode;
  this->m_transaction_psi= thd->m_transaction_psi;
  this->m_server_status= thd->server_status;
}


void THD::Transaction_state::restore(THD *thd)
{
  thd->set_transaction(this->m_trx);

  for (int i= 0; i < MAX_HA; ++i)
    thd->ha_data[i]= this->m_ha_data[i];

  thd->tx_isolation= this->m_tx_isolation;
  thd->variables.sql_mode= this->m_sql_mode;
  thd->tx_read_only= this->m_tx_read_only;
  thd->variables.option_bits= this->m_thd_option_bits;

  thd->m_transaction_psi= this->m_transaction_psi;
  thd->server_status= this->m_server_status;
  thd->lex->sql_command= this->m_sql_command;
}

THD::Attachable_trx::Attachable_trx(THD *thd)
 :m_thd(thd)
{
  // The THD::transaction_rollback_request is expected to be unset in the
  // attachable transaction. It's weird to start attachable transaction when the
  // SE asked to rollback the regular transaction.
  DBUG_ASSERT(!m_thd->transaction_rollback_request);

  // Save the transaction state.

  m_trx_state.backup(m_thd);

  // Save and reset query-tables-list and reset the sql-command.
  //
  // NOTE: ha_innobase::store_lock() takes the current sql-command into account.
  // It must be SQLCOM_SELECT.
  //
  // Do NOT reset LEX if we're running tests. LEX is used by SELECT statements.

  if (DBUG_EVALUATE_IF("use_attachable_trx", false, true))
  {
    m_thd->lex->reset_n_backup_query_tables_list(&m_trx_state.m_query_tables_list);
    m_thd->lex->sql_command= SQLCOM_SELECT;
  }

  // Save and reset open-tables.

  m_thd->reset_n_backup_open_tables_state(&m_trx_state.m_open_tables_state);

  // Reset transaction state.

  m_thd->m_transaction.release(); // it's been backed up.
  m_thd->m_transaction.reset(new Transaction_ctx());

  // Prepare for a new attachable transaction for read-only DD-transaction.

  for (int i= 0; i < MAX_HA; ++i)
    m_thd->ha_data[i]= Ha_data();

  // The attachable transaction must used READ COMMITTED isolation level.

  m_thd->tx_isolation= ISO_READ_COMMITTED;

  // The attachable transaction must be read-only.

  m_thd->tx_read_only= true;

  // The attachable transaction must be AUTOCOMMIT.

  m_thd->variables.option_bits|= OPTION_AUTOCOMMIT;
  m_thd->variables.option_bits&= ~OPTION_NOT_AUTOCOMMIT;
  m_thd->variables.option_bits&= ~OPTION_BEGIN;

  // Possible parent's involvement to multi-statement transaction is masked

  m_thd->server_status&= ~SERVER_STATUS_IN_TRANS;
  m_thd->server_status&= ~SERVER_STATUS_IN_TRANS_READONLY;

  // Reset SQL_MODE during system operations.

  m_thd->variables.sql_mode= 0;

  // Reset transaction instrumentation.

  m_thd->m_transaction_psi= NULL;
}


THD::Attachable_trx::~Attachable_trx()
{
  // Ensure that the SE didn't request rollback in the attachable transaction.
  // Having THD::transaction_rollback_request set most likely means that we've
  // experienced some sort of deadlock/timeout while processing the attachable
  // transaction. That is not possible by the definition of an attachable
  // transaction.
  DBUG_ASSERT(!m_thd->transaction_rollback_request);

  // Commit the attachable transaction before discarding transaction state.
  // This is mostly needed to properly reset transaction state in SE.
  // Note: We can't rely on InnoDB hack which auto-magically commits InnoDB
  // transaction when the last table for a statement in auto-commit mode is
  // unlocked. Apparently it doesn't work correctly in some corner cases
  // (for example, when statement is killed just after tables are locked but
  // before any other operations on the table happes). We try not to rely on
  // it in other places on SQL-layer as well.
  trans_commit_attachable(m_thd);

  // Close all the tables that are open till now.

  close_thread_tables(m_thd);

  // Cleanup connection specific state which was created for attachable
  // transaction (for InnoDB removes cached transaction object).
  //
  // Note that we need to call handlerton::close_connection for all SEs
  // and not only SEs which participated in attachable transaction since
  // connection specific state can be created when TABLE object is simply
  // expelled from the Table Cache (e.g. this happens for MyISAM).
  ha_close_connection(m_thd);

  // Restore the transaction state.

  m_trx_state.restore(m_thd);

  m_thd->restore_backup_open_tables_state(&m_trx_state.m_open_tables_state);

  if (DBUG_EVALUATE_IF("use_attachable_trx", false, true))
  {
    m_thd->lex->restore_backup_query_tables_list(
      &m_trx_state.m_query_tables_list);
  }
}


/****************************************************************************
** User variables
****************************************************************************/

extern "C" uchar *get_var_key(user_var_entry *entry, size_t *length,
                              my_bool not_used MY_ATTRIBUTE((unused)))
{
  *length= entry->entry_name.length();
  return (uchar*) entry->entry_name.ptr();
}

extern "C" void free_user_var(user_var_entry *entry)
{
  entry->destroy();
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
  :Key(rhs, mem_root),
  ref_db(rhs.ref_db),
  ref_table(rhs.ref_table),
  ref_columns(rhs.ref_columns, mem_root),
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

/**
  @brief  validate
    Check if the foreign key options are compatible with columns
    on which the FK is created.

  @param table_fields         List of columns 

  @return
    false   Key valid
  @return
    true   Key invalid
 */
bool Foreign_key::validate(List<Create_field> &table_fields)
{
  Create_field  *sql_field;
  Key_part_spec *column;
  List_iterator<Key_part_spec> cols(columns);
  List_iterator<Create_field> it(table_fields);
  DBUG_ENTER("Foreign_key::validate");
  while ((column= cols++))
  {
    it.rewind();
    while ((sql_field= it++) &&
           my_strcasecmp(system_charset_info,
                         column->field_name.str,
                         sql_field->field_name)) {}
    if (!sql_field)
    {
      my_error(ER_KEY_COLUMN_DOES_NOT_EXITS, MYF(0), column->field_name.str);
      DBUG_RETURN(TRUE);
    }
    if (type == KEYTYPE_FOREIGN && sql_field->gcol_info)
    {
      if (delete_opt == FK_OPTION_SET_NULL)
      {
        my_error(ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN, MYF(0), 
                 "ON DELETE SET NULL");
        DBUG_RETURN(TRUE);
      }
      if (update_opt == FK_OPTION_SET_NULL)
      {
        my_error(ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN, MYF(0), 
                 "ON UPDATE SET NULL");
        DBUG_RETURN(TRUE);
      }
      if (update_opt == FK_OPTION_CASCADE)
      {
        my_error(ER_WRONG_FK_OPTION_FOR_GENERATED_COLUMN, MYF(0), 
                 "ON UPDATE CASCADE");
        DBUG_RETURN(TRUE);
      }
    }
  }
  DBUG_RETURN(FALSE);
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

PSI_thread* THD::get_psi()
{
  void *addr= & m_psi;
  void * volatile * typed_addr= static_cast<void * volatile *>(addr);
  void *ptr;
  ptr= my_atomic_loadptr(typed_addr);
  return static_cast<PSI_thread*>(ptr);
}

void THD::set_psi(PSI_thread *psi)
{
  void *addr= & m_psi;
  void * volatile * typed_addr= static_cast<void * volatile *>(addr);
  my_atomic_storeptr(typed_addr, psi);
}

/**
  Get reference to Performance Schema object for THD object

  @param thd            THD object

  @return               Performance schema object for thread on THD
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
void thd_clear_errors(THD *thd)
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

extern "C"
void thd_binlog_pos(const THD *thd,
                    const char **file_var,
                    unsigned long long *pos_var)
{
  thd->get_trans_pos(file_var, pos_var);
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
  Set reading/writing on socket, used by SHOW PROCESSLIST

  @param thd                       THD object
  @param val                       Value to set it to (0 or 1)
*/
void thd_set_net_read_write(THD *thd, uint val)
{
  thd->get_protocol_classic()->get_net()->reading_or_writing= val;
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
int thd_store_globals(THD* thd)
{
  return thd->store_globals();
}

/**
  Get thread attributes for connection threads

  @retval      Reference to thread attribute for connection threads
*/
my_thread_attr_t *get_connection_attrib(void)
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
  return mysql_tmpfile_path(mysql_tmpdir, prefix);
}


extern "C"
int thd_in_lock_tables(const THD *thd)
{
  return MY_TEST(thd->in_lock_tables);
}


extern "C"
int thd_tablespace_op(const THD *thd)
{
  return MY_TEST(thd->tablespace_op);
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
void set_thd_stage_info(void *opaque_thd,
                        const PSI_stage_info *new_stage,
                        PSI_stage_info *old_stage,
                        const char *calling_func,
                        const char *calling_file,
                        const unsigned int calling_line)
{
  THD *thd= (THD*) opaque_thd;
  if (thd == NULL)
    thd= current_thd;

  thd->enter_stage(new_stage, old_stage, calling_func, calling_file, calling_line);
}


void THD::enter_stage(const PSI_stage_info *new_stage,
                      PSI_stage_info *old_stage,
                      const char *calling_func,
                      const char *calling_file,
                      const unsigned int calling_line)
{
  DBUG_PRINT("THD::enter_stage",
             ("'%s' %s:%d", new_stage ? new_stage->m_name : "",
              calling_file, calling_line));

  if (old_stage != NULL)
  {
    old_stage->m_key= m_current_stage_key;
    old_stage->m_name= proc_info;
  }

  if (new_stage != NULL)
  {
    const char *msg= new_stage->m_name;

#if defined(ENABLED_PROFILING)
    profiling.status_change(msg, calling_func, calling_file, calling_line);
#endif

    m_current_stage_key= new_stage->m_key;
    proc_info= msg;

    m_stage_progress_psi= MYSQL_SET_STAGE(m_current_stage_key, calling_file, calling_line);
  }
  else
  {
    m_stage_progress_psi= NULL;
  }

  return;
}

extern "C"
void thd_enter_cond(void *opaque_thd, mysql_cond_t *cond, mysql_mutex_t *mutex,
                    const PSI_stage_info *stage, PSI_stage_info *old_stage,
                    const char *src_function, const char *src_file,
                    int src_line)
{
  THD *thd= static_cast<THD*>(opaque_thd);
  if (!thd)
    thd= current_thd;

  return thd->enter_cond(cond, mutex, stage, old_stage,
                         src_function, src_file, src_line);
}

extern "C"
void thd_exit_cond(void *opaque_thd, const PSI_stage_info *stage,
                   const char *src_function, const char *src_file,
                   int src_line)
{
  THD *thd= static_cast<THD*>(opaque_thd);
  if (!thd)
    thd= current_thd;

  thd->exit_cond(stage, src_function, src_file, src_line);
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
int thd_tx_is_read_only(const THD *thd)
{
  return (int) thd->tx_read_only;
}

extern "C"
int thd_tx_priority(const THD* thd)
{
  return (thd->thd_tx_priority != 0
          ? thd->thd_tx_priority
          : thd->tx_priority);
}

extern "C"
THD* thd_tx_arbitrate(THD *requestor, THD* holder)
{
 /* Should be different sessions. */
 DBUG_ASSERT(holder != requestor);

 return(thd_tx_priority(requestor) == thd_tx_priority(holder)
	? requestor
	: ((thd_tx_priority(requestor)
	    > thd_tx_priority(holder)) ? holder : requestor));
}

int thd_tx_is_dd_trx(const THD *thd)
{
  return (int) thd->is_attachable_ro_transaction_active();
}

extern "C"
void thd_inc_row_count(THD *thd)
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
char *thd_security_context(THD *thd, char *buffer, size_t length,
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


/**
  Returns the partition_info working copy.
  Used to see if a table should be created with partitioning.

  @param thd thread context

  @return Pointer to the working copy of partition_info or NULL.
*/

extern "C"
partition_info *thd_get_work_part_info(THD *thd)
{
  return thd->work_part_info;
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

  @return true if the condition is handled.
*/
bool Drop_table_error_handler::handle_condition(THD *thd,
                                                uint sql_errno,
                                                const char* sqlstate,
                                                Sql_condition::enum_severity_level *level,
                                                const char* msg)
{
  return ((sql_errno == EE_DELETE && my_errno() == ENOENT) ||
          sql_errno == ER_TRG_NO_DEFINER);
}


void Open_tables_state::set_open_tables_state(Open_tables_state *state)
{
  this->open_tables= state->open_tables;

  this->temporary_tables= state->temporary_tables;
  this->derived_tables= state->derived_tables;

  this->lock= state->lock;
  this->extra_lock= state->extra_lock;

  this->locked_tables_mode= state->locked_tables_mode;

  this->state_flags= state->state_flags;

  this->m_reprepare_observers= state->m_reprepare_observers;
}


void Open_tables_state::reset_open_tables_state()
{
  open_tables= NULL;
  temporary_tables= NULL;
  derived_tables= NULL;
  lock= NULL;
  extra_lock= NULL;
  locked_tables_mode= LTM_NONE;
  state_flags= 0U;
  reset_reprepare_observers();
}


THD::THD(bool enable_plugins)
  :Query_arena(&main_mem_root, STMT_CONVENTIONAL_EXECUTION),
   mark_used_columns(MARK_COLUMNS_READ),
   want_privilege(0),
   lex(&main_lex),
   gtid_executed_warning_issued(false),
   m_query_string(NULL_CSTR),
   m_db(NULL_CSTR),
   rli_fake(0), rli_slave(NULL),
#ifdef EMBEDDED_LIBRARY
   mysql(NULL),
#endif
   initial_status_var(NULL),
   status_var_aggregated(false),
   query_plan(this),
   m_current_stage_key(0),
   current_mutex(NULL),
   current_cond(NULL),
   in_sub_stmt(0),
   fill_status_recursion_level(0),
   fill_variables_recursion_level(0),
   binlog_row_event_extra_data(NULL),
   skip_readonly_check(false),
   binlog_unsafe_warning_flags(0),
   binlog_table_maps(0),
   binlog_accessed_db_names(NULL),
   m_trans_log_file(NULL),
   m_trans_fixed_log_file(NULL),
   m_trans_end_pos(0),
   m_transaction(new Transaction_ctx()),
   m_attachable_trx(NULL),
   table_map_for_update(0),
   m_examined_row_count(0),
   m_stage_progress_psi(NULL),
   m_digest(NULL),
   m_statement_psi(NULL),
   m_transaction_psi(NULL),
   m_idle_psi(NULL),
   m_server_idle(false),
   user_var_events(key_memory_user_var_entry),
   next_to_commit(NULL),
   binlog_need_explicit_defaults_ts(false),
   is_fatal_error(0),
   transaction_rollback_request(0),
   is_fatal_sub_stmt_error(false),
   rand_used(0),
   time_zone_used(0),
   in_lock_tables(0),
   bootstrap(0),
   derived_tables_processing(FALSE),
   sp_runtime_ctx(NULL),
   m_parser_state(NULL),
   work_part_info(NULL),
#ifndef EMBEDDED_LIBRARY
   // No need to instrument, highly unlikely to have that many plugins.
   audit_class_plugins(PSI_NOT_INSTRUMENTED),
   audit_class_mask(PSI_NOT_INSTRUMENTED),
#endif
#if defined(ENABLED_DEBUG_SYNC)
   debug_sync_control(0),
#endif /* defined(ENABLED_DEBUG_SYNC) */
   m_enable_plugins(enable_plugins),
#ifdef HAVE_GTID_NEXT_LIST
   owned_gtid_set(global_sid_map),
#endif
   skip_gtid_rollback(false),
   is_commit_in_middle_of_statement(false),
   has_gtid_consistency_violation(false),
   main_da(false),
   m_parser_da(false),
   m_query_rewrite_plugin_da(false),
   m_query_rewrite_plugin_da_ptr(&m_query_rewrite_plugin_da),
   m_stmt_da(&main_da),
   duplicate_slave_id(false),
   is_a_srv_session_thd(false)
{
  main_lex.reset();
  set_psi(NULL);
  mdl_context.init(this);
  init_sql_alloc(key_memory_thd_main_mem_root,
                 &main_mem_root,
                 global_system_variables.query_alloc_block_size,
                 global_system_variables.query_prealloc_size);
  stmt_arena= this;
  thread_stack= 0;
  m_catalog.str= "std";
  m_catalog.length= 3;
  m_security_ctx= &m_main_security_ctx;
  no_errors= 0;
  password= 0;
  query_start_usec_used= 0;
  count_cuted_fields= CHECK_FIELD_IGNORE;
  killed= NOT_KILLED;
  col_access=0;
  is_slave_error= thread_specific_used= FALSE;
  my_hash_clear(&handler_tables_hash);
  my_hash_clear(&ull_hash);
  tmp_table=0;
  cuted_fields= 0L;
  m_sent_row_count= 0L;
  current_found_rows= 0;
  previous_found_rows= 0;
  is_operating_gtid_table_implicitly= false;
  is_operating_substatement_implicitly= false;
  m_row_count_func= -1;
  statement_id_counter= 0UL;
  // Must be reset to handle error with THD's created for init of mysqld
  lex->thd= NULL;
  lex->set_current_select(0);
  utime_after_lock= 0L;
  current_linfo =  0;
  slave_thread = 0;
  memset(&variables, 0, sizeof(variables));
  m_thread_id= Global_THD_manager::reserved_thread_id;
  file_id = 0;
  query_id= 0;
  query_name_consts= 0;
  db_charset= global_system_variables.collation_database;
  memset(ha_data, 0, sizeof(ha_data));
  is_killable= false;
  binlog_evt_union.do_union= FALSE;
  enable_slow_log= 0;
  commit_error= CE_NONE;
  durability_property= HA_REGULAR_DURABILITY;
#ifndef DBUG_OFF
  dbug_sentry=THD_SENTRY_MAGIC;
#endif
#ifndef EMBEDDED_LIBRARY
  mysql_audit_init_thd(this);
  net.vio=0;
#endif
  system_thread= NON_SYSTEM_THREAD;
  cleanup_done= 0;
  m_release_resources_done= false;
  peer_port= 0;					// For SHOW PROCESSLIST
  get_transaction()->m_flags.enabled= true;
  active_vio = 0;
  m_SSL = NULL;
  mysql_mutex_init(key_LOCK_thd_data, &LOCK_thd_data, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_thd_query, &LOCK_thd_query, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_thd_sysvar, &LOCK_thd_sysvar, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_query_plan, &LOCK_query_plan, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(key_LOCK_current_cond, &LOCK_current_cond,
                   MY_MUTEX_INIT_FAST);
  mysql_cond_init(key_COND_thr_lock, &COND_thr_lock);

  /* Variables with default values */
  proc_info="login";
  where= THD::DEFAULT_WHERE;
  server_id = ::server_id;
  unmasked_server_id = server_id;
  slave_net = 0;
  set_command(COM_CONNECT);
  *scramble= '\0';

  /* Call to init() below requires fully initialized Open_tables_state. */
  reset_open_tables_state();

  init();
#if defined(ENABLED_PROFILING)
  profiling.set_thd(this);
#endif
  m_user_connect= NULL;
  my_hash_init(&user_vars, system_charset_info, USER_VARS_HASH_SIZE, 0, 0,
               (my_hash_get_key) get_var_key,
               (my_hash_free_key) free_user_var, 0,
               key_memory_user_var_entry);

  sp_proc_cache= NULL;
  sp_func_cache= NULL;

  /* Protocol */
  m_protocol= &protocol_text;			// Default protocol
  protocol_text.init(this);
  protocol_binary.init(this);
  protocol_text.set_client_capabilities(0); // minimalistic client

  tablespace_op= false;
  substitute_null_with_insert_id = FALSE;

  /*
    Make sure thr_lock_info_init() is called for threads which do not get
    assigned a proper thread_id value but keep using reserved_thread_id.
  */
  thr_lock_info_init(&lock_info, m_thread_id, &COND_thr_lock);

  m_internal_handler= NULL;
  m_binlog_invoker= FALSE;
  memset(&m_invoker_user, 0, sizeof(m_invoker_user));
  memset(&m_invoker_host, 0, sizeof(m_invoker_host));

  binlog_next_event_pos.file_name= NULL;
  binlog_next_event_pos.pos= 0;

  timer= NULL;
  timer_cache= NULL;

  m_token_array= NULL;
  if (max_digest_length > 0)
  {
    m_token_array= (unsigned char*) my_malloc(PSI_INSTRUMENT_ME,
                                              max_digest_length,
                                              MYF(MY_WME));
  }
}


void THD::set_transaction(Transaction_ctx *transaction_ctx)
{
  DBUG_ASSERT(is_attachable_ro_transaction_active());

  delete m_transaction.release();
  m_transaction.reset(transaction_ctx);
}


void THD::push_internal_handler(Internal_error_handler *handler)
{
  if (m_internal_handler)
  {
    handler->m_prev_internal_handler= m_internal_handler;
    m_internal_handler= handler;
  }
  else
    m_internal_handler= handler;
}


bool THD::handle_condition(uint sql_errno,
                           const char* sqlstate,
                           Sql_condition::enum_severity_level *level,
                           const char* msg)
{
  if (!m_internal_handler)
    return false;

  for (Internal_error_handler *error_handler= m_internal_handler;
       error_handler;
       error_handler= error_handler->m_prev_internal_handler)
  {
    if (error_handler->handle_condition(this, sql_errno, sqlstate, level, msg))
      return true;
  }
  return false;
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
                         Sql_condition::SL_ERROR,
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
                         Sql_condition::SL_ERROR,
                         ebuff);
  DBUG_VOID_RETURN;
}

void THD::raise_warning(uint sql_errno)
{
  const char* msg= ER(sql_errno);
  (void) raise_condition(sql_errno,
                         NULL,
                         Sql_condition::SL_WARNING,
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
                         Sql_condition::SL_WARNING,
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
                         Sql_condition::SL_NOTE,
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
                         Sql_condition::SL_NOTE,
                         ebuff);
  DBUG_VOID_RETURN;
}


struct timeval THD::query_start_timeval_trunc(uint decimals)
{
  struct timeval tv;
  tv.tv_sec= start_time.tv_sec;
  if (decimals)
  {
    tv.tv_usec= start_time.tv_usec;
    my_timeval_trunc(&tv, decimals);
    query_start_usec_used= 1;
  }
  else
  {
    tv.tv_usec= 0;
  }
  return tv;
}


Sql_condition* THD::raise_condition(uint sql_errno,
                                    const char* sqlstate,
                                    Sql_condition::enum_severity_level level,
                                    const char* msg,
                                    bool use_condition_handler)
{
  DBUG_ENTER("THD::raise_condition");

  if (!(variables.option_bits & OPTION_SQL_NOTES) &&
      (level == Sql_condition::SL_NOTE))
    DBUG_RETURN(NULL);

  DBUG_ASSERT(sql_errno != 0);
  if (sql_errno == 0) /* Safety in release build */
    sql_errno= ER_UNKNOWN_ERROR;
  if (msg == NULL)
    msg= ER(sql_errno);
  if (sqlstate == NULL)
   sqlstate= mysql_errno_to_sqlstate(sql_errno);

  if (use_condition_handler &&
      handle_condition(sql_errno, sqlstate, &level, msg))
    DBUG_RETURN(NULL);

  if (level == Sql_condition::SL_NOTE || level == Sql_condition::SL_WARNING)
    got_warning= true;

  query_cache.abort(&query_cache_tls);

  Diagnostics_area *da= get_stmt_da();
  if (level == Sql_condition::SL_ERROR)
  {
    is_slave_error= true; // needed to catch query errors during replication

    if (!da->is_error())
    {
      set_row_count_func(-1);
      da->set_error_status(sql_errno, msg, sqlstate);
    }
  }

  /*
    Avoid pushing a condition for fatal out of memory errors as this will
    require memory allocation and therefore might fail. Non fatal out of
    memory errors can occur if raised by SIGNAL/RESIGNAL statement.
  */
  Sql_condition *cond= NULL;
  if (!(is_fatal_error && (sql_errno == EE_OUTOFMEMORY ||
                           sql_errno == ER_OUTOFMEMORY)))
  {
    cond= da->push_warning(this, sql_errno, sqlstate, level, msg);
  }
  DBUG_RETURN(cond);
}

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
LEX_STRING *thd_make_lex_string(THD *thd, LEX_STRING *lex_str,
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

extern "C"
void thd_get_xid(const MYSQL_THD thd, MYSQL_XID *xid)
{
  *xid = *(MYSQL_XID *) thd->get_transaction()->xid_state()->get_xid();
}

#if defined(_WIN32)
extern "C"   THD *_current_thd_noinline(void)
{
  return my_thread_get_THR_THD();
}
#endif
/*
  Init common variables that has to be reset on start and on cleanup_connection
*/

void THD::init(void)
{
  mysql_mutex_lock(&LOCK_global_system_variables);
  plugin_thdvar_init(this, m_enable_plugins);
  /*
    variables= global_system_variables above has reset
    variables.pseudo_thread_id to 0. We need to correct it here to
    avoid temporary tables replication failure.
  */
  variables.pseudo_thread_id= m_thread_id;
  mysql_mutex_unlock(&LOCK_global_system_variables);

  /*
    NOTE: reset_connection command will reset the THD to its default state.
    All system variables whose scope is SESSION ONLY should be set to their
    default values here.
  */
  reset_first_successful_insert_id();
  user_time.tv_sec= user_time.tv_usec= 0;
  start_time.tv_sec= start_time.tv_usec= 0;
  set_time();
  auto_inc_intervals_forced.empty();
  {
    ulong tmp;
    tmp= sql_rnd_with_mutex();
    randominit(&rand, tmp + (ulong) &rand, tmp + (ulong) ::global_query_id);
  }

  server_status= SERVER_STATUS_AUTOCOMMIT;
  if (variables.sql_mode & MODE_NO_BACKSLASH_ESCAPES)
    server_status|= SERVER_STATUS_NO_BACKSLASH_ESCAPES;

  get_transaction()->reset_unsafe_rollback_flags(Transaction_ctx::SESSION);
  get_transaction()->reset_unsafe_rollback_flags(Transaction_ctx::STMT);
  open_options=ha_open_options;
  update_lock_default= (variables.low_priority_updates ?
			TL_WRITE_LOW_PRIORITY :
			TL_WRITE);
  insert_lock_default= (variables.low_priority_updates ?
                        TL_WRITE_LOW_PRIORITY :
                        TL_WRITE_CONCURRENT_INSERT);
  tx_isolation= (enum_tx_isolation) variables.tx_isolation;
  tx_read_only= variables.tx_read_only;
  tx_priority= 0;
  thd_tx_priority= 0;
  update_charset();
  reset_current_stmt_binlog_format_row();
  reset_binlog_local_stmt_filter();
  memset(&status_var, 0, sizeof(status_var));
  binlog_row_event_extra_data= 0;

  if (variables.sql_log_bin)
    variables.option_bits|= OPTION_BIN_LOG;
  else
    variables.option_bits&= ~OPTION_BIN_LOG;

#if defined(ENABLED_DEBUG_SYNC)
  /* Initialize the Debug Sync Facility. See debug_sync.cc. */
  debug_sync_init_thread(this);
#endif /* defined(ENABLED_DEBUG_SYNC) */

  /* Initialize session_tracker and create all tracker objects */
  session_tracker.init(this->charset());
  session_tracker.enable(this);

  owned_gtid.clear();
  owned_sid.clear();
  owned_gtid.dbug_print(NULL, "set owned_gtid (clear) in THD::init");

  rpl_thd_ctx.dependency_tracker_ctx().set_last_session_sequence_number(0);
}


/*
  Init THD for query processing.
  This has to be called once before we call mysql_parse.
  See also comments in sql_class.h.
*/

void THD::init_for_queries(Relay_log_info *rli)
{
  set_time(); 
  ha_enable_transaction(this,TRUE);

  reset_root_defaults(mem_root, variables.query_alloc_block_size,
                      variables.query_prealloc_size);
  get_transaction()->init_mem_root_defaults(variables.trans_alloc_block_size,
                                            variables.trans_prealloc_size);
  get_transaction()->xid_state()->reset();
#if defined(MYSQL_SERVER) && defined(HAVE_REPLICATION)
  if (rli)
  {
    if ((rli->deferred_events_collecting= rpl_filter->is_on()))
    {
      rli->deferred_events= new Deferred_log_events(rli);
    }
    rli_slave= rli;

    DBUG_ASSERT(rli_slave->info_thd == this && slave_thread);
  }
#endif
}


void THD::set_new_thread_id()
{
  m_thread_id= Global_THD_manager::get_instance()->get_new_thread_id();
  variables.pseudo_thread_id= m_thread_id;
  thr_lock_info_init(&lock_info, m_thread_id, &COND_thr_lock);
}


/*
  Do what's needed when one invokes change user

  SYNOPSIS
    cleanup_connection()

  IMPLEMENTATION
    Reset all resources that are connection specific
*/


void THD::cleanup_connection(void)
{
  mysql_mutex_lock(&LOCK_status);
  add_to_status(&global_status_var, &status_var, true);
  mysql_mutex_unlock(&LOCK_status);

  cleanup();
#if defined(ENABLED_DEBUG_SYNC)
  /* End the Debug Sync Facility. See debug_sync.cc. */
  debug_sync_end_thread(this);
#endif /* defined(ENABLED_DEBUG_SYNC) */
  killed= NOT_KILLED;
  cleanup_done= 0;
  init();
  stmt_map.reset();
  my_hash_init(&user_vars, system_charset_info, USER_VARS_HASH_SIZE, 0, 0,
               (my_hash_get_key) get_var_key,
               (my_hash_free_key) free_user_var, 0,
               key_memory_user_var_entry);
  sp_cache_clear(&sp_proc_cache);
  sp_cache_clear(&sp_func_cache);

  clear_error();
  // clear the warnings
  get_stmt_da()->reset_condition_info(this);
  // clear profiling information
#if defined(ENABLED_PROFILING)
  profiling.cleanup();
#endif

#ifndef DBUG_OFF
    /* DEBUG code only (begin) */
    bool check_cleanup= FALSE;
    DBUG_EXECUTE_IF("debug_test_cleanup_connection", check_cleanup= TRUE;);
    if(check_cleanup)
    {
      /* isolation level should be default */
      DBUG_ASSERT(variables.tx_isolation == ISO_REPEATABLE_READ);
      /* check autocommit is ON by default */
      DBUG_ASSERT(server_status == SERVER_STATUS_AUTOCOMMIT);
      /* check prepared stmts are cleaned up */
      DBUG_ASSERT(prepared_stmt_count == 0);
      /* check diagnostic area is cleaned up */
      DBUG_ASSERT(get_stmt_da()->status() == Diagnostics_area::DA_EMPTY);
      /* check if temp tables are deleted */
      DBUG_ASSERT(temporary_tables == NULL);
      /* check if tables are unlocked */
      DBUG_ASSERT(locked_tables_list.locked_tables() == NULL);
    }
    /* DEBUG code only (end) */
#endif

}


/*
  Do what's needed when one invokes change user.
  Also used during THD::release_resources, i.e. prior to THD destruction.
*/
void THD::cleanup(void)
{
  Transaction_ctx *trn_ctx= get_transaction();
  XID_STATE *xs= trn_ctx->xid_state();

  DBUG_ENTER("THD::cleanup");
  DBUG_ASSERT(cleanup_done == 0);
  DEBUG_SYNC(this, "thd_cleanup_start");

  killed= KILL_CONNECTION;
  if (trn_ctx->xid_state()->has_state(XID_STATE::XA_PREPARED))
  {
    transaction_cache_detach(trn_ctx);
  }
  else
  {
    xs->set_state(XID_STATE::XA_NOTR);
    trans_rollback(this);
    transaction_cache_delete(trn_ctx);
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

  mysql_ull_cleanup(this);
  /*
    All locking service locks must be released on disconnect.
  */
  release_all_locking_service_locks(this);

  /* All metadata locks must have been released by now. */
  DBUG_ASSERT(!mdl_context.has_locks());

  /* Protects user_vars. */
  mysql_mutex_lock(&LOCK_thd_data);
  my_hash_free(&user_vars);
  mysql_mutex_unlock(&LOCK_thd_data);

  /*
    When we call drop table for temporary tables, the
    user_var_events container is not cleared this might
    cause error if the container was filled before the
    drop table command is called.
    So call this before calling close_temporary_tables.
  */
  user_var_events.clear();
  close_temporary_tables(this);
  sp_cache_clear(&sp_proc_cache);
  sp_cache_clear(&sp_func_cache);

  /*
    Actions above might generate events for the binary log, so we
    commit the current transaction coordinator after executing cleanup
    actions.
   */
  if (tc_log && !trn_ctx->xid_state()->has_state(XID_STATE::XA_PREPARED))
    tc_log->commit(this, true);

  /*
    Destroy trackers only after finishing manipulations with transaction
    state to avoid issues with Transaction_state_tracker.
  */
  session_tracker.deinit();

  cleanup_done=1;
  DBUG_VOID_RETURN;
}


/**
  Release most resources, prior to THD destruction.
 */
void THD::release_resources()
{
  DBUG_ASSERT(m_release_resources_done == false);

  Global_THD_manager::get_instance()->release_thread_id(m_thread_id);

  mysql_mutex_lock(&LOCK_status);
  add_to_status(&global_status_var, &status_var, false);
  /*
    Status queries after this point should not aggregate THD::status_var
    since the values has been added to global_status_var.
    The status values are not reset so that they can still be read
    by performance schema.
  */
  status_var_aggregated= true;
  mysql_mutex_unlock(&LOCK_status);

  /* Ensure that no one is using THD */
  mysql_mutex_lock(&LOCK_thd_data);
  mysql_mutex_lock(&LOCK_query_plan);

  /* Close connection */
#ifndef EMBEDDED_LIBRARY
  if (is_classic_protocol() && get_protocol_classic()->get_vio())
  {
    vio_delete(get_protocol_classic()->get_vio());
    get_protocol_classic()->end_net();
  }
#endif

  /* modification plan for UPDATE/DELETE should be freed. */
  DBUG_ASSERT(query_plan.get_modification_plan() == NULL);
  mysql_mutex_unlock(&LOCK_query_plan);
  mysql_mutex_unlock(&LOCK_thd_data);
  mysql_mutex_lock(&LOCK_thd_query);
  mysql_mutex_unlock(&LOCK_thd_query);

  stmt_map.reset();                     /* close all prepared statements */
  if (!cleanup_done)
    cleanup();

  mdl_context.destroy();
  ha_close_connection(this);

  /*
    Debug sync system must be closed after ha_close_connection, because
    DEBUG_SYNC is used in InnoDB connection handlerton close.
  */
#if defined(ENABLED_DEBUG_SYNC)
  /* End the Debug Sync Facility. See debug_sync.cc. */
  debug_sync_end_thread(this);
#endif /* defined(ENABLED_DEBUG_SYNC) */

  plugin_thdvar_cleanup(this, m_enable_plugins);

  DBUG_ASSERT(timer == NULL);

  if (timer_cache)
    thd_timer_destroy(timer_cache);

#ifndef EMBEDDED_LIBRARY
  if (rli_fake)
  {
    rli_fake->end_info();
    delete rli_fake;
    rli_fake= NULL;
  }
  mysql_audit_free_thd(this);
#endif

  if (current_thd == this)
    restore_globals();
  m_release_resources_done= true;
}


THD::~THD()
{
  THD_CHECK_SENTRY(this);
  DBUG_ENTER("~THD()");
  DBUG_PRINT("info", ("THD dtor, this %p", this));

  if (!m_release_resources_done)
    release_resources();

  clear_next_event_pos();

  /* Ensure that no one is using THD */
  mysql_mutex_lock(&LOCK_thd_data);
  mysql_mutex_unlock(&LOCK_thd_data);
  mysql_mutex_lock(&LOCK_thd_query);
  mysql_mutex_unlock(&LOCK_thd_query);

  DBUG_ASSERT(!m_attachable_trx);

  my_free(const_cast<char*>(m_db.str));
  m_db= NULL_CSTR;
  get_transaction()->free_memory(MYF(0));
  mysql_mutex_destroy(&LOCK_query_plan);
  mysql_mutex_destroy(&LOCK_thd_data);
  mysql_mutex_destroy(&LOCK_thd_query);
  mysql_mutex_destroy(&LOCK_thd_sysvar);
  mysql_mutex_destroy(&LOCK_current_cond);
  mysql_cond_destroy(&COND_thr_lock);
#ifndef DBUG_OFF
  dbug_sentry= THD_SENTRY_GONE;
#endif

#ifndef EMBEDDED_LIBRARY
  if (variables.gtid_next_list.gtid_set != NULL)
  {
#ifdef HAVE_GTID_NEXT_LIST
    delete variables.gtid_next_list.gtid_set;
    variables.gtid_next_list.gtid_set= NULL;
    variables.gtid_next_list.is_non_null= false;
#else
    DBUG_ASSERT(0);
#endif
  }
  if (rli_slave)
    rli_slave->cleanup_after_session();
#endif

  free_root(&main_mem_root, MYF(0));

  if (m_token_array != NULL)
  {
    my_free(m_token_array);
  }
  DBUG_VOID_RETURN;
}


/*
  Add all status variables to another status variable array

  SYNOPSIS
   add_to_status()
   to_var       add to this array
   from_var     from this array
   reset_from_var if true, then memset from_var variable with 0

  NOTES
    This function assumes that all variables are longlong/ulonglong.
    If this assumption will change, then we have to explictely add
    the other variables after the while loop
*/

void add_to_status(STATUS_VAR *to_var, STATUS_VAR *from_var, bool reset_from_var)
{
  int        c;
  ulonglong *end= (ulonglong*) ((uchar*) to_var +
                                offsetof(STATUS_VAR, LAST_STATUS_VAR) +
                                sizeof(ulonglong));
  ulonglong *to= (ulonglong*) to_var, *from= (ulonglong*) from_var;

  while (to != end)
    *(to++)+= *(from++);

  to_var->com_other+= from_var->com_other;

  for (c= 0; c< SQLCOM_END; c++)
    to_var->com_stat[(uint) c] += from_var->com_stat[(uint) c];

  if (reset_from_var)
  {
    memset (from_var, 0, sizeof(*from_var));
  }
}

/*
  Add the difference between two status variable arrays to another one.

  SYNOPSIS
    add_diff_to_status
    to_var       add to this array
    from_var     from this array
    dec_var      minus this array
  
  NOTE
    This function assumes that all variables are longlong/ulonglong.
*/

void add_diff_to_status(STATUS_VAR *to_var, STATUS_VAR *from_var,
                        STATUS_VAR *dec_var)
{
  int        c;
  ulonglong *end= (ulonglong*) ((uchar*) to_var + offsetof(STATUS_VAR, LAST_STATUS_VAR) +
                                                  sizeof(ulonglong));
  ulonglong *to= (ulonglong*) to_var,
            *from= (ulonglong*) from_var,
            *dec= (ulonglong*) dec_var;

  while (to != end)
    *(to++)+= *(from++) - *(dec++);

  to_var->com_other+= from_var->com_other - dec_var->com_other;

  for (c= 0; c< SQLCOM_END; c++)
    to_var->com_stat[(uint) c] += from_var->com_stat[(uint) c] -dec_var->com_stat[(uint) c];
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

  /*
    Set killed flag if the connection is being killed (state_to_set
    is KILL_CONNECTION) or the connection is processing a query
    (state_to_set is KILL_QUERY and m_server_idle flag is not set).
    If the connection is idle and state_to_set is KILL QUERY, the
    the killed flag is not set so that it doesn't affect the next
    command incorrectly.
  */
  if (this->m_server_idle && state_to_set == KILL_QUERY)
  { /* nothing */ }
  else
  {
    killed= state_to_set;
  }

  if (state_to_set != THD::KILL_QUERY && state_to_set != THD::KILL_TIMEOUT)
  {
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

      shutdown_active_vio();
    }

    /* Send an event to the scheduler that a thread should be killed. */
    if (!slave_thread)
      MYSQL_CALLBACK(Connection_handler_manager::event_functions,
                     post_kill_notification, (this));
  }

  /* Interrupt target waiting inside a storage engine. */
  if (state_to_set != THD::NOT_KILLED)
    ha_kill_connection(this);

  if (state_to_set == THD::KILL_TIMEOUT)
    status_var.max_execution_time_exceeded++;


  /* Broadcast a condition to kick the target if it is waiting on it. */
  if (is_killable)
  {
    mysql_mutex_lock(&LOCK_current_cond);
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
    if (current_cond && current_mutex)
    {
      DBUG_EXECUTE_IF("before_dump_thread_acquires_current_mutex",
                      {
                      const char act[]=
                      "now signal dump_thread_signal wait_for go_dump_thread";
                      DBUG_ASSERT(!debug_sync_set_action(current_thd,
                                                         STRING_WITH_LEN(act)));
                      };);
      mysql_mutex_lock(current_mutex);
      mysql_cond_broadcast(current_cond);
      mysql_mutex_unlock(current_mutex);
    }
    mysql_mutex_unlock(&LOCK_current_cond);
  }
  DBUG_VOID_RETURN;
}


/**
  Close the Vio associated this session.

  @remark LOCK_thd_data is taken due to the fact that
          the Vio might be disassociated concurrently.
*/

void THD::disconnect(bool server_shutdown)
{
  Vio *vio= NULL;

  mysql_mutex_lock(&LOCK_thd_data);

  killed= THD::KILL_CONNECTION;

  /*
    Since a active vio might might have not been set yet, in
    any case save a reference to avoid closing a inexistent
    one or closing the vio twice if there is a active one.
  */
  vio= active_vio;
  shutdown_active_vio();

  /* Disconnect even if a active vio is not associated. */
  if (is_classic_protocol() &&
      get_protocol_classic()->get_vio() != vio &&
      get_protocol_classic()->connection_alive())
  {
    m_protocol->shutdown(server_shutdown);
  }

  mysql_mutex_unlock(&LOCK_thd_data);
}


void THD::notify_shared_lock(MDL_context_owner *ctx_in_use,
                             bool needs_thr_lock_abort)
{
  THD *in_use= ctx_in_use->get_thd();

  if (needs_thr_lock_abort)
  {
    mysql_mutex_lock(&in_use->LOCK_thd_data);
    for (TABLE *thd_table= in_use->open_tables;
         thd_table ;
         thd_table= thd_table->next)
    {
      /*
        Check for TABLE::needs_reopen() is needed since in some places we call
        handler::close() for table instance (and set TABLE::db_stat to 0)
        and do not remove such instances from the THD::open_tables
        for some time, during which other thread can see those instances
        (e.g. see partitioning code).
      */
      if (!thd_table->needs_reopen())
        mysql_lock_abort_for_thread(this, thd_table);
    }
    mysql_mutex_unlock(&in_use->LOCK_thd_data);
  }
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

  if (my_thread_set_THR_THD(this) ||
      my_thread_set_THR_MALLOC(&mem_root))
    return true;
  /*
    is_killable is concurrently readable by a killer thread.
    It is protected by LOCK_thd_data, it is not needed to lock while the
    value is changing from false not true. If the kill thread reads
    true we need to ensure that the thread doesn't proceed to assign
    another thread to the same TLS reference.
  */
  is_killable= true;
#ifndef DBUG_OFF
  /*
    Let mysqld define the thread id (not mysys)
    This allows us to move THD to different threads if needed.
  */
  set_my_thread_var_id(m_thread_id);
#endif
  real_id= my_thread_self();                      // For debugging

  return false;
}

/*
  Remove the thread specific info (THD and mem_root pointer) stored during
  store_global call for this thread.
*/
void THD::restore_globals()
{
  /*
    Assert that thread_stack is initialized: it's necessary to be able
    to track stack overrun.
  */
  DBUG_ASSERT(thread_stack);

  /* Undocking the thread specific data. */
  my_thread_set_THR_THD(NULL);
  my_thread_set_THR_MALLOC(NULL);
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
    binlog_accessed_db_names= NULL;

#ifndef EMBEDDED_LIBRARY
    /*
      Clean possible unused INSERT_ID events by current statement.
      is_update_query() is needed to ignore SET statements:
        Statements that don't update anything directly and don't
        used stored functions. This is mostly necessary to ignore
        statements in binlog between SET INSERT_ID and DML statement
        which is intended to consume its event (there can be other
        SET statements between them).
    */
    if ((rli_slave || rli_fake) && is_update_query(lex->sql_command))
      auto_inc_intervals_forced.empty();
#endif
  }

  /*
    In case of stored procedures, stored functions, triggers and events
    m_trans_fixed_log_file will not be set to NULL. The memory will be reused.
  */
  if (!sp_runtime_ctx)
    m_trans_fixed_log_file= NULL;

  /*
    Forget the binlog stmt filter for the next query.
    There are some code paths that:
    - do not call THD::decide_logging_format()
    - do call THD::binlog_query(),
    making this reset necessary.
  */
  reset_binlog_local_stmt_filter();
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
  if (lex)
  {
    lex->mi.repl_ignore_server_ids.clear();
  }
#ifndef EMBEDDED_LIBRARY
  if (rli_slave)
    rli_slave->cleanup_after_query();
#endif
}

LEX_CSTRING *
make_lex_string_root(MEM_ROOT *mem_root,
                     LEX_CSTRING *lex_str, const char* str, size_t length,
                     bool allocate_lex_string)
{
  if (allocate_lex_string)
    if (!(lex_str= (LEX_CSTRING *)alloc_root(mem_root, sizeof(LEX_CSTRING))))
      return 0;
  if (!(lex_str->str= strmake_root(mem_root, str, length)))
    return 0;
  lex_str->length= length;
  return lex_str;
}


LEX_STRING *
make_lex_string_root(MEM_ROOT *mem_root,
                     LEX_STRING *lex_str, const char* str, size_t length,
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



LEX_CSTRING *THD::make_lex_string(LEX_CSTRING *lex_str,
                                 const char* str, size_t length,
                                 bool allocate_lex_string)
{
  return make_lex_string_root (mem_root, lex_str, str,
                               length, allocate_lex_string);
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
                                 const char* str, size_t length,
                                 bool allocate_lex_string)
{
  return make_lex_string_root (mem_root, lex_str, str,
                               length, allocate_lex_string);
}


/*
  Convert a string to another character set

  @param to             Store new allocated string here
  @param to_cs          New character set for allocated string
  @param from           String to convert
  @param from_length    Length of string to convert
  @param from_cs        Original character set

  @note to will be 0-terminated to make it easy to pass to system funcs

  @retval false ok
  @retval true  End of memory.
                In this case to->str will point to 0 and to->length will be 0.
*/

bool THD::convert_string(LEX_STRING *to, const CHARSET_INFO *to_cs,
			 const char *from, size_t from_length,
			 const CHARSET_INFO *from_cs)
{
  DBUG_ENTER("convert_string");
  size_t new_length= to_cs->mbmaxlen * from_length;
  uint errors= 0;
  if (!(to->str= (char*) alloc(new_length+1)))
  {
    to->length= 0;				// Safety fix
    DBUG_RETURN(1);				// EOM
  }
  to->length= copy_and_convert(to->str, new_length, to_cs,
			       from, from_length, from_cs, &errors);
  to->str[to->length]=0;			// Safety
  if (errors != 0)
  {
    char printable_buff[32];
    convert_to_printable(printable_buff, sizeof(printable_buff),
                         from, from_length, from_cs, 6);
    push_warning_printf(this, Sql_condition::SL_WARNING,
                        ER_INVALID_CHARACTER_STRING,
                        ER_THD(this, ER_INVALID_CHARACTER_STRING),
                        from_cs->csname, printable_buff);
  }

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

bool THD::convert_string(String *s, const CHARSET_INFO *from_cs,
                         const CHARSET_INFO *to_cs)
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
  size_t not_used;
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
  if (get_transaction()->add_changed_table(key, key_length))
    killed= KILL_CONNECTION;
  DBUG_VOID_RETURN;
}


int THD::send_explain_fields(Query_result *result)
{
  List<Item> field_list;
  Item *item;
  CHARSET_INFO *cs= system_charset_info;
  field_list.push_back(new Item_return_int("id",3, MYSQL_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("select_type", 19, cs));
  field_list.push_back(item= new Item_empty_string("table", NAME_CHAR_LEN, cs));
  item->maybe_null= 1;
  /* Maximum length of string that make_used_partitions_str() can produce */
  item= new Item_empty_string("partitions", MAX_PARTITIONS * (1 + FN_LEN),
                              cs);
  field_list.push_back(item);
  item->maybe_null= 1;
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
  item->maybe_null= 1;
  field_list.push_back(item= new Item_float(NAME_STRING("filtered"),
                                            0.1234, 2, 4));
  item->maybe_null=1;
  field_list.push_back(new Item_empty_string("Extra", 255, cs));
  item->maybe_null= 1;
  return (result->send_result_set_metadata(field_list, Protocol::SEND_NUM_ROWS |
                                           Protocol::SEND_EOF));
}

enum_vio_type THD::get_vio_type()
{
#ifndef EMBEDDED_LIBRARY
  DBUG_ENTER("THD::get_vio_type");
  DBUG_RETURN(get_protocol()->connection_type());
#else
  return NO_VIO_TYPE;
#endif
}

void THD::shutdown_active_vio()
{
  DBUG_ENTER("shutdown_active_vio");
  mysql_mutex_assert_owner(&LOCK_thd_data);
#ifndef EMBEDDED_LIBRARY
  if (active_vio)
  {
    vio_shutdown(active_vio);
    active_vio = 0;
    m_SSL = NULL;
  }
#endif
  DBUG_VOID_RETURN;
}


/*
  Register an item tree tree transformation, performed by the query
  optimizer.
*/

void THD::nocheck_register_item_tree_change(Item **place,
                                            Item *new_value)
{
  Item_change_record *change;
  /*
    Now we use one node per change, which adds some memory overhead,
    but still is rather fast as we use alloc_root for allocations.
    A list of item tree changes of an average query should be short.
  */
  void *change_mem= alloc_root(mem_root, sizeof(*change));
  if (change_mem == 0)
  {
    /*
      OOM, thd->fatal_error() is called by the error handler of the
      memroot. Just return.
    */
    return;
  }
  change= new (change_mem) Item_change_record(place, new_value);
  change_list.push_front(change);
}


void THD::replace_rollback_place(Item **new_place)
{
  I_List_iterator<Item_change_record> it(change_list);
  Item_change_record *change;
  while ((change= it++))
  {
    if (change->new_value == *new_place)
    {
      DBUG_PRINT("info", ("replace_rollback_place new_value %p place %p",
                          *new_place, new_place));
      change->place= new_place;
      break;
    }
  }
}


void THD::rollback_item_tree_changes()
{
  I_List_iterator<Item_change_record> it(change_list);
  Item_change_record *change;
  DBUG_ENTER("rollback_item_tree_changes");

  while ((change= it++))
  {
    DBUG_PRINT("info",
               ("rollback_item_tree_changes "
                "place %p curr_value %p old_value %p",
                change->place, *change->place, change->old_value));
    *change->place= change->old_value;
  }
  /* We can forget about changes memory: it's allocated in runtime memroot */
  change_list.empty();
  DBUG_VOID_RETURN;
}


/*****************************************************************************
** Functions to provide a interface to select results
*****************************************************************************/

static const String default_line_term("\n",default_charset_info);
static const String default_escaped("\\",default_charset_info);
static const String default_field_term("\t",default_charset_info);
static const String default_xml_row_term("<row>", default_charset_info);
static const String my_empty_string("",default_charset_info);


sql_exchange::sql_exchange(const char *name, bool flag,
                           enum enum_filetype filetype_arg)
  :file_name(name), dumpfile(flag), skip_lines(0)
{
  field.opt_enclosed= 0;
  filetype= filetype_arg;
  field.field_term= &default_field_term;
  field.enclosed= line.line_start= &my_empty_string;
  line.line_term= filetype == FILETYPE_CSV ?
              &default_line_term : &default_xml_row_term;
  field.escaped= &default_escaped;
  cs= NULL;
}

bool sql_exchange::escaped_given(void)
{
  return field.escaped != &default_escaped;
}


bool Query_result_send::send_result_set_metadata(List<Item> &list, uint flags)
{
  bool res;
  if (!(res= thd->send_result_metadata(&list, flags)))
    is_result_set_started= 1;
  return res;
}

void Query_result_send::abort_result_set()
{
  DBUG_ENTER("Query_result_send::abort_result_set");

  if (is_result_set_started && thd->sp_runtime_ctx)
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
    thd->sp_runtime_ctx->end_partial_result_set= TRUE;
  }
  DBUG_VOID_RETURN;
}


/* Send data to client. Returns 0 if ok */

bool Query_result_send::send_data(List<Item> &items)
{
  Protocol *protocol= thd->get_protocol();
  DBUG_ENTER("Query_result_send::send_data");

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

  protocol->start_row();
  if (thd->send_result_set_row(&items))
  {
    protocol->abort_row();
    DBUG_RETURN(TRUE);
  }

  thd->inc_sent_row_count(1);
  DBUG_RETURN(protocol->end_row());
}

bool Query_result_send::send_eof()
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

void Query_result_to_file::send_error(uint errcode,const char *err)
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


bool Query_result_to_file::send_eof()
{
  int error= MY_TEST(end_io_cache(&cache));
  if (mysql_file_close(file, MYF(MY_WME)) || thd->is_error())
    error= true;

  if (!error)
  {
    ::my_ok(thd,row_count);
  }
  file= -1;
  return error;
}


void Query_result_to_file::cleanup()
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


Query_result_to_file::~Query_result_to_file()
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

  if (!dirname_length(exchange->file_name))
  {
    strxnmov(path, FN_REFLEN-1, mysql_real_data_home,
             thd->db().str ? thd->db().str : "",
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


int Query_result_export::prepare(List<Item> &list, SELECT_LEX_UNIT *u)
{
  bool blob_flag=0;
  bool string_results= FALSE, non_string_results= FALSE;
  unit= u;
  if (strlen(exchange->file_name) + NAME_LEN >= FN_REFLEN)
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
  if (exchange->field.escaped->numchars() > 1 ||
      exchange->field.enclosed->numchars() > 1)
  {
    my_error(ER_WRONG_FIELD_TERMINATORS, MYF(0));
    return TRUE;
  }
  if (exchange->field.escaped->length() > 1 ||
      exchange->field.enclosed->length() > 1 ||
      !my_isascii(exchange->field.escaped->ptr()[0]) ||
      !my_isascii(exchange->field.enclosed->ptr()[0]) ||
      !exchange->field.field_term->is_ascii() ||
      !exchange->line.line_term->is_ascii() ||
      !exchange->line.line_start->is_ascii())
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
    push_warning(thd, Sql_condition::SL_WARNING,
                 WARN_NON_ASCII_SEPARATOR_NOT_IMPLEMENTED,
                 ER(WARN_NON_ASCII_SEPARATOR_NOT_IMPLEMENTED));
  }
  field_term_length=exchange->field.field_term->length();
  field_term_char= field_term_length ?
                   (int) (uchar) (*exchange->field.field_term)[0] : INT_MAX;
  if (!exchange->line.line_term->length())
    exchange->line.line_term=exchange->field.field_term;// Use this if it exists
  field_sep_char= (exchange->field.enclosed->length() ?
                  (int) (uchar) (*exchange->field.enclosed)[0] :
                  field_term_char);
  if (exchange->field.escaped->length() && (exchange->escaped_given() ||
      !(thd->variables.sql_mode & MODE_NO_BACKSLASH_ESCAPES)))
    escape_char= (int) (uchar) (*exchange->field.escaped)[0];
  else
    escape_char= -1;
  is_ambiguous_field_sep= MY_TEST(strchr(ESCAPE_CHARS, field_sep_char));
  is_unsafe_field_sep= MY_TEST(strchr(NUMERIC_CHARS, field_sep_char));
  line_sep_char= (exchange->line.line_term->length() ?
                 (int) (uchar) (*exchange->line.line_term)[0] : INT_MAX);
  if (!field_term_length)
    exchange->field.opt_enclosed=0;
  if (!exchange->field.enclosed->length())
    exchange->field.opt_enclosed=1;                     // A little quicker loop
  fixed_row_size= (!field_term_length && !exchange->field.enclosed->length() &&
		   !blob_flag);
  if ((is_ambiguous_field_sep && exchange->field.enclosed->is_empty() &&
       (string_results || is_unsafe_field_sep)) ||
      (exchange->field.opt_enclosed && non_string_results &&
       field_term_length && strchr(NUMERIC_CHARS, field_term_char)))
  {
    push_warning(thd, Sql_condition::SL_WARNING,
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

bool Query_result_export::send_data(List<Item> &items)
{

  DBUG_ENTER("Query_result_export::send_data");
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
  size_t used_length=0;
  uint items_left=items.elements;
  List_iterator_fast<Item> li(items);

  if (my_b_write(&cache,(uchar*) exchange->line.line_start->ptr(),
		 exchange->line.line_start->length()))
    goto err;
  while ((item=li++))
  {
    Item_result result_type=item->result_type();
    bool enclosed = (exchange->field.enclosed->length() &&
                     (!exchange->field.opt_enclosed ||
                      result_type == STRING_RESULT));
    res=item->str_result(&tmp);
    if (res && !my_charset_same(write_cs, res->charset()) &&
        !my_charset_same(write_cs, &my_charset_bin))
    {
      const char *well_formed_error_pos;
      const char *cannot_convert_error_pos;
      const char *from_end_pos;
      const char *error_pos;
      size_t bytes;
      uint64 estimated_bytes=
        ((uint64) res->length() / res->charset()->mbminlen + 1) *
        write_cs->mbmaxlen + 1;
      set_if_smaller(estimated_bytes, UINT_MAX32);
      if (cvt_str.mem_realloc((uint32) estimated_bytes))
      {
        my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR), (uint32) estimated_bytes);
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
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_TRUNCATED_WRONG_VALUE_FOR_FIELD,
                            ER(ER_TRUNCATED_WRONG_VALUE_FOR_FIELD),
                            "string", printable_buff,
                            item->item_name.ptr(), static_cast<long>(row_count));
      }
      else if (from_end_pos < res->ptr() + res->length())
      { 
        /*
          result is longer than UINT_MAX32 and doesn't fit into String
        */
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            WARN_DATA_TRUNCATED, ER(WARN_DATA_TRUNCATED),
                            item->full_name(), static_cast<long>(row_count));
      }
      cvt_str.length(bytes);
      res= &cvt_str;
    }
    if (res && enclosed)
    {
      if (my_b_write(&cache,(uchar*) exchange->field.enclosed->ptr(),
		     exchange->field.enclosed->length()))
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
	used_length=min<size_t>(res->length(),item->max_length);
      else
	used_length=res->length();
      if ((result_type == STRING_RESULT || is_unsafe_field_sep) &&
           escape_char != -1)
      {
        char *pos, *start, *end;
        bool escape_4_bytes= false;
        int in_escapable_4_bytes= 0;
        const CHARSET_INFO *res_charset= res->charset();
        const CHARSET_INFO *character_set_client=
          thd->variables.character_set_client;
        bool check_following_byte= (res_charset == &my_charset_bin) &&
                                    character_set_client->
                                    escape_with_backslash_is_dangerous;
        /*
          The judgement of mbmaxlenlen == 2 is for gb18030 only.
          Since there are several charsets with mbmaxlen == 4,
          so we have to use mbmaxlenlen == 2 here, which is only true
          for gb18030 currently.
        */
        DBUG_ASSERT(character_set_client->mbmaxlen == 2 ||
                    my_mbmaxlenlen(character_set_client) == 2 ||
                    !character_set_client->escape_with_backslash_is_dangerous);
	for (start=pos=(char*) res->ptr(),end=pos+used_length ;
	     pos != end ;
	     pos++)
	{
          bool need_escape= false;
	  if (use_mb(res_charset))
	  {
	    int l;
	    if ((l=my_ismbchar(res_charset, pos, end)))
	    {
	      pos += l-1;
	      continue;
	    }
	  }

          /*
            Special case when dumping BINARY/VARBINARY/BLOB values
            for the clients with character sets big5, cp932, gbk, sjis
            and gb18030, which can have the escape character
            (0x5C "\" by default) as the second byte of a multi-byte sequence.

            The escape character had better be single-byte character,
            non-ASCII characters are not prohibited, but not fully supported.

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

            But gb18030 is an exception. First of all, 2-byte codes
            would be affected by the issue above without doubt.
            Then, 4-byte gb18030 codes would be affected as well.

            Supposing the input is GB+81358130, and the
            field_term_char is set to '5', escape char is 0x5C by default.
            When we come to the first byte 0x81, if we don't escape it but
            escape the second byte 0x35 as it's the field_term_char,
            we would get 0x81 0x5C 0x35 0x81 0x30 for the gb18030 character.
            That would be the same issue as mentioned above.

            Also, if we just escape the leading 2 bytes, we would get
            0x5C 0x81 0x5C 0x35 0x81 0x30 in this case.
            The reader of this sequence would assume that 0x81 0x30
            is the starting of a new gb18030 character, which would
            result in further confusion.

            Once we find any byte of the 4-byte gb18030 character should
            be escaped, we have to escape all the 4 bytes.
            So for GB+81358130, we will get:
            0x5C 0x81 0x5C 0x35 0x5C 0x81 0x30

            The byte 0x30 shouldn't be escaped(no matter it's the second
            or fourth byte in the sequence), since '\0' would be treated
            as 0x00, which is not what we expect. And 0x30 would be treated as
            an ASCII char when we read it, which is correct.
          */

          DBUG_ASSERT(in_escapable_4_bytes >= 0);
          if (in_escapable_4_bytes > 0)
          {
            DBUG_ASSERT(check_following_byte);
            /* We should escape or not escape all the 4 bytes. */
            need_escape= escape_4_bytes;
          }
          else if (NEED_ESCAPING(*pos))
          {
            need_escape= true;
            if (my_mbmaxlenlen(character_set_client) == 2 &&
                my_mbcharlen_ptr(character_set_client, pos, end) == 4)
            {
              in_escapable_4_bytes= 4;
              escape_4_bytes= true;
            }
          }
          else if (check_following_byte)
          {
             int len= my_mbcharlen_ptr(character_set_client, pos, end);
             if (len == 2 && pos + 1 < end && NEED_ESCAPING(pos[1]))
               need_escape= true;
             else if (len == 4 && my_mbmaxlenlen(character_set_client) == 2 &&
                      pos + 3 < end)
             {
               in_escapable_4_bytes= 4;
               escape_4_bytes= (NEED_ESCAPING(pos[1]) ||
                                NEED_ESCAPING(pos[2]) ||
                                NEED_ESCAPING(pos[3]));
               need_escape= escape_4_bytes;
             }
          }
          /* Mark how many coming bytes should be escaped, only for gb18030 */
          if (in_escapable_4_bytes > 0)
          {
            in_escapable_4_bytes--;
            /*
             Note that '0' (0x30) in the middle of a 4-byte sequence
             can't be escaped. Please read more details from above comments.
             2-byte codes won't be affected by this issue.
            */
            if (pos[0] == 0x30)
              need_escape= false;
          }

          if (need_escape &&
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

        /* Assert that no escape mode is active here */
        DBUG_ASSERT(in_escapable_4_bytes == 0);

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
	  memset(space, ' ', sizeof(space));
	}
	size_t length=item->max_length-used_length;
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
      if (my_b_write(&cache, (uchar*) exchange->field.enclosed->ptr(),
                     exchange->field.enclosed->length()))
        goto err;
    }
    if (--items_left)
    {
      if (my_b_write(&cache, (uchar*) exchange->field.field_term->ptr(),
                     field_term_length))
        goto err;
    }
  }
  if (my_b_write(&cache,(uchar*) exchange->line.line_term->ptr(),
		 exchange->line.line_term->length()))
    goto err;
  DBUG_RETURN(0);
err:
  DBUG_RETURN(1);
}


/***************************************************************************
** Dump of query to a binary file
***************************************************************************/


int Query_result_dump::prepare(List<Item> &list MY_ATTRIBUTE((unused)),
                               SELECT_LEX_UNIT *u)
{
  unit= u;
  return (int) ((file= create_file(thd, path, exchange, &cache)) < 0);
}


bool Query_result_dump::send_data(List<Item> &items)
{
  List_iterator_fast<Item> li(items);
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff),&my_charset_bin),*res;
  tmp.length(0);
  Item *item;
  DBUG_ENTER("Query_result_dump::send_data");

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
      char errbuf[MYSYS_STRERROR_SIZE];
      my_error(ER_ERROR_ON_WRITE, MYF(0), path, my_errno(),
               my_strerror(errbuf, sizeof(errbuf), my_errno()));
      goto err;
    }
  }
  DBUG_RETURN(0);
err:
  DBUG_RETURN(1);
}


/***************************************************************************
  Dump of select to variables
***************************************************************************/

int Query_dumpvar::prepare(List<Item> &list, SELECT_LEX_UNIT *u)
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


bool Query_dumpvar::check_simple_select() const
{
  my_error(ER_SP_BAD_CURSOR_SELECT, MYF(0));
  return TRUE;
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

C_MODE_START

static uchar *
get_statement_id_as_hash_key(const uchar *record, size_t *key_length,
                             my_bool not_used MY_ATTRIBUTE((unused)))
{
  const Prepared_statement *statement= (const Prepared_statement *) record;
  *key_length= sizeof(statement->id);
  return (uchar *) &(statement)->id;
}

static void delete_statement_as_hash_key(void *key)
{
  delete (Prepared_statement *) key;
}

static uchar *get_stmt_name_hash_key(Prepared_statement *entry, size_t *length,
                                     my_bool not_used MY_ATTRIBUTE((unused)))
{
  *length= entry->name().length;
  return reinterpret_cast<uchar *>(const_cast<char *>(entry->name().str));
}

C_MODE_END

Prepared_statement_map::Prepared_statement_map()
 :m_last_found_statement(NULL)
{
  enum
  {
    START_STMT_HASH_SIZE = 16,
    START_NAME_HASH_SIZE = 16
  };
  my_hash_init(&st_hash, &my_charset_bin, START_STMT_HASH_SIZE, 0, 0,
               get_statement_id_as_hash_key,
               delete_statement_as_hash_key, MYF(0),
               key_memory_prepared_statement_map);
  my_hash_init(&names_hash, system_charset_info, START_NAME_HASH_SIZE, 0, 0,
               (my_hash_get_key) get_stmt_name_hash_key,
               NULL, MYF(0),
               key_memory_prepared_statement_map);
}


int Prepared_statement_map::insert(THD *thd, Prepared_statement *statement)
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
  if (statement->name().str && my_hash_insert(&names_hash, (uchar*) statement))
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

  m_last_found_statement= statement;
  return 0;

err_max:
  if (statement->name().str)
    my_hash_delete(&names_hash, (uchar*) statement);
err_names_hash:
  my_hash_delete(&st_hash, (uchar*) statement);
err_st_hash:
  return 1;
}


Prepared_statement
*Prepared_statement_map::find_by_name(const LEX_CSTRING &name)
{
  return reinterpret_cast<Prepared_statement*>
    (my_hash_search(&names_hash, (uchar*)name.str, name.length));
}


Prepared_statement *Prepared_statement_map::find(ulong id)
{
  if (m_last_found_statement == NULL || id != m_last_found_statement->id)
  {
    Prepared_statement *stmt=
      reinterpret_cast<Prepared_statement*>
      (my_hash_search(&st_hash, (uchar *) &id, sizeof(id)));
    if (stmt && stmt->name().str)
      return NULL;
    m_last_found_statement= stmt;
  }
  return m_last_found_statement;
}


void Prepared_statement_map::erase(Prepared_statement *statement)
{
  if (statement == m_last_found_statement)
    m_last_found_statement= NULL;
  if (statement->name().str)
    my_hash_delete(&names_hash, (uchar *) statement);

  my_hash_delete(&st_hash, (uchar *) statement);
  mysql_mutex_lock(&LOCK_prepared_stmt_count);
  DBUG_ASSERT(prepared_stmt_count > 0);
  prepared_stmt_count--;
  mysql_mutex_unlock(&LOCK_prepared_stmt_count);
}

void Prepared_statement_map::claim_memory_ownership()
{
  my_hash_claim(&names_hash);
  my_hash_claim(&st_hash);
}

void Prepared_statement_map::reset()
{
  /* Must be first, hash_free will reset st_hash.records */
  if (st_hash.records > 0)
  {
#ifdef HAVE_PSI_PS_INTERFACE
    for (uint i=0 ; i < st_hash.records ; i++)
    {
      Prepared_statement *stmt=
        reinterpret_cast<Prepared_statement *>(my_hash_element(&st_hash, i));
      MYSQL_DESTROY_PS(stmt->get_PS_prepared_stmt());
    }
#endif
    mysql_mutex_lock(&LOCK_prepared_stmt_count);
    DBUG_ASSERT(prepared_stmt_count >= st_hash.records);
    prepared_stmt_count-= st_hash.records;
    mysql_mutex_unlock(&LOCK_prepared_stmt_count);
  }
  my_hash_reset(&names_hash);
  my_hash_reset(&st_hash);
  m_last_found_statement= NULL;
}


Prepared_statement_map::~Prepared_statement_map()
{
  /*
    We do not want to grab the global LOCK_prepared_stmt_count mutex here.
    reset() should already have been called to maintain prepared_stmt_count.
   */
  DBUG_ASSERT(st_hash.records == 0);

  my_hash_free(&names_hash);
  my_hash_free(&st_hash);
}


bool Query_dumpvar::send_data(List<Item> &items)
{
  List_iterator_fast<PT_select_var> var_li(var_list);
  List_iterator<Item> it(items);
  Item *item;
  PT_select_var *mv;
  DBUG_ENTER("Query_dumpvar::send_data");

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
    if (mv->is_local())
    {
      if (thd->sp_runtime_ctx->set_variable(thd, mv->get_offset(), &item))
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
        new Item_func_set_user_var(mv->name, item, true);
      if (suv->fix_fields(thd, 0))
        DBUG_RETURN(true);
      suv->save_item_result(item);
      if (suv->update())
        DBUG_RETURN(true);
    }
  }
  DBUG_RETURN(thd->is_error());
}

bool Query_dumpvar::send_eof()
{
  if (! row_count)
    push_warning(thd, Sql_condition::SL_WARNING,
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


void thd_increment_bytes_sent(size_t length)
{
  THD *thd= current_thd;
  if (likely(thd != NULL))
  { /* current_thd==NULL when close_connection() calls net_send_error() */
    thd->status_var.bytes_sent+= length;
  }
}


void thd_increment_bytes_received(size_t length)
{
  THD *thd= current_thd;
  if (likely(thd != NULL))
    thd->status_var.bytes_received+= length;
}


void THD::set_status_var_init()
{
  memset(&status_var, 0, sizeof(status_var));
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
  reset_open_tables_state();
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
              get_reprepare_observer() == NULL);

  set_open_tables_state(backup);
  DBUG_VOID_RETURN;
}


void THD::begin_attachable_ro_transaction()
{
  DBUG_ASSERT(!m_attachable_trx);

  m_attachable_trx= new Attachable_trx(this);
}


void THD::end_attachable_transaction()
{
  DBUG_ASSERT(m_attachable_trx);

  delete m_attachable_trx;
  m_attachable_trx= NULL;
}


/**
  Check the killed state of a user thread
  @param thd  user thread
  @retval 0 the user thread is active
  @retval 1 the user thread has been killed
*/
extern "C" int thd_killed(const MYSQL_THD thd)
{
  if (thd == NULL)
    return current_thd != NULL ? current_thd->killed : 0;
  return thd->killed;
}

/**
  Set the killed status of the current statement.

  @param thd  user thread connection handle
*/
extern "C" void thd_set_kill_status(const MYSQL_THD thd)
{
  thd->send_kill_message();
}

/**
  Return the thread id of a user thread
  @param thd user thread
  @return thread id
*/
extern "C" unsigned long thd_get_thread_id(const MYSQL_THD thd)
{
  return((unsigned long)thd->thread_id());
}

/**
  Check if batching is allowed for the thread
  @param thd  user thread
  @retval 1 batching allowed
  @retval 0 batching not allowed
*/
extern "C" int thd_allow_batch(MYSQL_THD thd)
{
  if ((thd->variables.option_bits & OPTION_ALLOW_BATCH) ||
      (thd->slave_thread && opt_slave_allow_batching))
    return 1;
  return 0;
}

enum_tx_isolation thd_get_trx_isolation(const MYSQL_THD thd)
{
	return thd->tx_isolation;
}

#ifdef INNODB_COMPATIBILITY_HOOKS
extern "C" const struct charset_info_st *thd_charset(MYSQL_THD thd)
{
  return(thd->charset());
}

/**
  Get the current query string for the thread.

  @param thd   The MySQL internal thread pointer

  @return query string and length. May be non-null-terminated.

  @note This function is not thread safe and should only be called
        from the thread owning thd. @see thd_query_safe().
*/
extern "C" LEX_CSTRING thd_query_unsafe(MYSQL_THD thd)
{
  DBUG_ASSERT(current_thd == thd);
  return thd->query();
}

/**
  Get the current query string for the thread.

  @param thd     The MySQL internal thread pointer
  @param buf     Buffer where the query string will be copied
  @param buflen  Length of the buffer

  @return Length of the query

  @note This function is thread safe as the query string is
        accessed under mutex protection and the string is copied
        into the provided buffer. @see thd_query_unsafe().
*/
extern "C" size_t thd_query_safe(MYSQL_THD thd, char *buf, size_t buflen)
{
  mysql_mutex_lock(&thd->LOCK_thd_query);
  LEX_CSTRING query_string= thd->query();
  size_t len= MY_MIN(buflen - 1, query_string.length);
  strncpy(buf, query_string.str, len);
  buf[len]= '\0';
  mysql_mutex_unlock(&thd->LOCK_thd_query);
  return len;
}

extern "C" int thd_slave_thread(const MYSQL_THD thd)
{
  return(thd->slave_thread);
}

extern "C" int thd_non_transactional_update(const MYSQL_THD thd)
{
  return thd->get_transaction()->has_modified_non_trans_table(
    Transaction_ctx::SESSION);
}

extern "C" int thd_binlog_format(const MYSQL_THD thd)
{
  if (mysql_bin_log.is_open() && (thd->variables.option_bits & OPTION_BIN_LOG))
    return (int) thd->variables.binlog_format;
  else
    return BINLOG_FORMAT_UNSPEC;
}

extern "C" void thd_mark_transaction_to_rollback(MYSQL_THD thd, int all)
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

extern "C" bool thd_binlog_filter_ok(const MYSQL_THD thd)
{
  return binlog_filter->db_ok(thd->db().str);
}

extern "C" bool thd_sqlcom_can_generate_row_events(const MYSQL_THD thd)
{
  return sqlcom_can_generate_row_events(thd->lex->sql_command);
}

extern "C" enum durability_properties thd_get_durability_property(const MYSQL_THD thd)
{
  enum durability_properties ret= HA_REGULAR_DURABILITY;
  
  if (thd != NULL)
    ret= thd->durability_property;

  return ret;
}

/** Get the auto_increment_offset auto_increment_increment.
Needed by InnoDB.
@param thd	Thread object
@param off	auto_increment_offset
@param inc	auto_increment_increment */
extern "C" void thd_get_autoinc(const MYSQL_THD thd, ulong* off, ulong* inc)
{
  *off = thd->variables.auto_increment_offset;
  *inc = thd->variables.auto_increment_increment;
}


/**
  Is strict sql_mode set.
  Needed by InnoDB.
  @param thd	Thread object
  @return True if sql_mode has strict mode (all or trans).
    @retval true  sql_mode has strict mode (all or trans).
    @retval false sql_mode has not strict mode (all or trans).
*/
extern "C" bool thd_is_strict_mode(const MYSQL_THD thd)
{
  return thd->is_strict_mode();
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

#ifndef EMBEDDED_LIBRARY
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
#else
extern "C"
void thd_report_row_lock_wait(THD *thd_wait_for)
{
  return;
}
#endif

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
  backup->current_found_rows= current_found_rows;
  backup->previous_found_rows= previous_found_rows;
  backup->examined_row_count= m_examined_row_count;
  backup->sent_row_count= m_sent_row_count;
  backup->cuted_fields=     cuted_fields;
  backup->client_capabilities= m_protocol->get_client_capabilities();
  backup->savepoints= get_transaction()->m_savepoints;
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
  if (is_classic_protocol())
    get_protocol_classic()->remove_client_capability(CLIENT_MULTI_RESULTS);
  in_sub_stmt|= new_state;
  m_examined_row_count= 0;
  m_sent_row_count= 0;
  cuted_fields= 0;
  get_transaction()->m_savepoints= 0;
  first_successful_insert_id_in_cur_stmt= 0;

  /* Reset savepoint on transaction write set */
  if (is_current_stmt_binlog_row_enabled_with_write_set_extraction())
  {
      get_transaction()->get_transaction_write_set_ctx()
          ->reset_savepoint_list();
  }
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
  if (get_transaction()->m_savepoints)
  {
    SAVEPOINT *sv;
    for (sv= get_transaction()->m_savepoints; sv->prev; sv= sv->prev)
    {}
    /* ha_release_savepoint() never returns error. */
    (void)ha_release_savepoint(this, sv);
  }
  count_cuted_fields= backup->count_cuted_fields;
  get_transaction()->m_savepoints= backup->savepoints;
  variables.option_bits= backup->option_bits;
  in_sub_stmt=      backup->in_sub_stmt;
  enable_slow_log=  backup->enable_slow_log;
  first_successful_insert_id_in_prev_stmt= 
    backup->first_successful_insert_id_in_prev_stmt;
  first_successful_insert_id_in_cur_stmt= 
    backup->first_successful_insert_id_in_cur_stmt;
  current_found_rows= backup->current_found_rows;
  previous_found_rows= backup->previous_found_rows;
  set_sent_row_count(backup->sent_row_count);
  if (is_classic_protocol())
    get_protocol_classic()->set_client_capabilities(backup->client_capabilities);

  /*
    If we've left sub-statement mode, reset the fatal error flag.
    Otherwise keep the current value, to propagate it up the sub-statement
    stack.

    NOTE: is_fatal_sub_stmt_error can be set only if we've been in the
    sub-statement mode.
  */

  if (!in_sub_stmt)
    is_fatal_sub_stmt_error= false;

  if ((variables.option_bits & OPTION_BIN_LOG) && is_update_query(lex->sql_command) &&
       !is_current_stmt_binlog_format_row())
    mysql_bin_log.stop_union_events(this);

  /*
    The following is added to the old values as we are interested in the
    total complexity of the query
  */
  inc_examined_row_count(backup->examined_row_count);
  cuted_fields+=       backup->cuted_fields;

  /* Restore savepoint on transaction write set */
  if (is_current_stmt_binlog_row_enabled_with_write_set_extraction())
  {
      get_transaction()->get_transaction_write_set_ctx()
          ->restore_savepoint_list();
  }

  DBUG_VOID_RETURN;
}

void THD::set_sent_row_count(ha_rows count)
{
  m_sent_row_count= count;
  MYSQL_SET_STATEMENT_ROWS_SENT(m_statement_psi, m_sent_row_count);
}

void THD::set_examined_row_count(ha_rows count)
{
  m_examined_row_count= count;
  MYSQL_SET_STATEMENT_ROWS_EXAMINED(m_statement_psi, m_examined_row_count);
}

void THD::inc_sent_row_count(ha_rows count)
{
  m_sent_row_count+= count;
  MYSQL_SET_STATEMENT_ROWS_SENT(m_statement_psi, m_sent_row_count);
}

void THD::inc_examined_row_count(ha_rows count)
{
  m_examined_row_count+= count;
  MYSQL_SET_STATEMENT_ROWS_EXAMINED(m_statement_psi, m_examined_row_count);
}

void THD::inc_status_created_tmp_disk_tables()
{
  status_var.created_tmp_disk_tables++;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_created_tmp_disk_tables)(m_statement_psi, 1);
#endif
}

void THD::inc_status_created_tmp_tables()
{
  status_var.created_tmp_tables++;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_created_tmp_tables)(m_statement_psi, 1);
#endif
}

void THD::inc_status_select_full_join()
{
  status_var.select_full_join_count++;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_select_full_join)(m_statement_psi, 1);
#endif
}

void THD::inc_status_select_full_range_join()
{
  status_var.select_full_range_join_count++;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_select_full_range_join)(m_statement_psi, 1);
#endif
}

void THD::inc_status_select_range()
{
  status_var.select_range_count++;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_select_range)(m_statement_psi, 1);
#endif
}

void THD::inc_status_select_range_check()
{
  status_var.select_range_check_count++;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_select_range_check)(m_statement_psi, 1);
#endif
}

void THD::inc_status_select_scan()
{
  status_var.select_scan_count++;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_select_scan)(m_statement_psi, 1);
#endif
}

void THD::inc_status_sort_merge_passes()
{
  status_var.filesort_merge_passes++;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_sort_merge_passes)(m_statement_psi, 1);
#endif
}

void THD::inc_status_sort_range()
{
  status_var.filesort_range_count++;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_sort_range)(m_statement_psi, 1);
#endif
}

void THD::inc_status_sort_rows(ha_rows count)
{
  status_var.filesort_rows+= count;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_sort_rows)(m_statement_psi,
                                              static_cast<ulong>(count));
#endif
}

void THD::inc_status_sort_scan()
{
  status_var.filesort_scan_count++;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(inc_statement_sort_scan)(m_statement_psi, 1);
#endif
}

void THD::set_status_no_index_used()
{
  server_status|= SERVER_QUERY_NO_INDEX_USED;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(set_statement_no_index_used)(m_statement_psi);
#endif
}

void THD::set_status_no_good_index_used()
{
  server_status|= SERVER_QUERY_NO_GOOD_INDEX_USED;
#ifdef HAVE_PSI_STATEMENT_INTERFACE
  PSI_STATEMENT_CALL(set_statement_no_good_index_used)(m_statement_psi);
#endif
}

void THD::set_command(enum enum_server_command command)
{
  m_command= command;
#ifdef HAVE_PSI_THREAD_INTERFACE
  PSI_STATEMENT_CALL(set_thread_command)(m_command);
#endif
}


void THD::set_query(const LEX_CSTRING& query_arg)
{
  DBUG_ASSERT(this == current_thd);
  mysql_mutex_lock(&LOCK_thd_query);
  m_query_string= query_arg;
  mysql_mutex_unlock(&LOCK_thd_query);

#ifdef HAVE_PSI_THREAD_INTERFACE
  PSI_THREAD_CALL(set_thread_info)(query_arg.str, query_arg.length);
#endif
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
    /*
      Also ensure that we don't release metadata locks for open HANDLERs
      and user-level locks.
    */
    if (handler_tables_hash.records)
      mysql_ha_set_explicit_lock_duration(this);
    if (ull_hash.records)
      mysql_ull_set_explicit_lock_duration(this);
  }
  locked_tables_mode= LTM_NONE;
}

void THD::get_definer(LEX_USER *definer)
{
  binlog_invoker();
#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
  if (slave_thread && has_invoker())
  {
    definer->user= m_invoker_user;
    definer->host= m_invoker_host;
    definer->plugin.str= (char *) "";
    definer->plugin.length= 0;
    definer->auth.str=  NULL;
    definer->auth.length= 0;
  }
  else
#endif
    get_default_definer(this, definer);
}


/**
  Mark transaction to rollback and mark error as fatal to a sub-statement.

  @param  all   TRUE <=> rollback main transaction.
*/

void THD::mark_transaction_to_rollback(bool all)
{
  /*
    There is no point in setting is_fatal_sub_stmt_error unless
    we are actually in_sub_stmt.
  */
  if (in_sub_stmt)
    is_fatal_sub_stmt_error= true;

  transaction_rollback_request= all;

}


void THD::set_next_event_pos(const char* _filename, ulonglong _pos)
{
  char*& filename= binlog_next_event_pos.file_name;
  if (filename == NULL)
  {
    /* First time, allocate maximal buffer */
    filename= (char*) my_malloc(key_memory_LOG_POS_COORD,
                                FN_REFLEN+1, MYF(MY_WME));
    if (filename == NULL) return;
  }

  assert(strlen(_filename) <= FN_REFLEN);
  strcpy(filename, _filename);
  filename[ FN_REFLEN ]= 0;

  binlog_next_event_pos.pos= _pos;
};

void THD::clear_next_event_pos()
{
  if (binlog_next_event_pos.file_name != NULL)
  {
    my_free(binlog_next_event_pos.file_name);
  }
  binlog_next_event_pos.file_name= NULL;
  binlog_next_event_pos.pos= 0;
};

#ifdef HAVE_REPLICATION
void THD::set_currently_executing_gtid_for_slave_thread()
{
  /*
    This function may be called in four cases:

    - From SQL thread while executing Gtid_log_event::do_apply_event

    - From an mts worker thread that executes a Gtid_log_event::do_apply_event.

    - From an mts worker thread that is processing an old binlog that
      is missing Gtid events completely, from gtid_pre_statement_checks().

    - From a normal client thread that is executing output from
      mysqlbinlog when mysqlbinlog is processing an old binlog file
      that is missing Gtid events completely, from
      gtid_pre_statement_checks() for a statement that appears after a
      BINLOG statement containing a Format_description_log_event
      originating from the master.

    Because of the last case, we need to add the following conditions to set
    currently_executing_gtid.
  */
  if (system_thread == SYSTEM_THREAD_SLAVE_SQL ||
      system_thread == SYSTEM_THREAD_SLAVE_WORKER)
    rli_slave->currently_executing_gtid= variables.gtid_next;
}
#endif

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
  DBUG_ENTER("THD::increment_con_per_hour_counter");

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
  DBUG_ENTER("THD::increment_questions_counter");

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
  if (check_time - m_user_connect->reset_utime >= 3600000000LL)
  {
    m_user_connect->questions=1;
    m_user_connect->updates=0;
    m_user_connect->conn_per_hour=0;
    m_user_connect->reset_utime= check_time;
  }

  DBUG_VOID_RETURN;
}


#ifndef DBUG_OFF
void THD::Query_plan::assert_plan_is_locked_if_other() const
{
  if (current_thd != thd)
    mysql_mutex_assert_owner(&thd->LOCK_query_plan);
}
#endif

void THD::Query_plan::set_query_plan(enum_sql_command sql_cmd,
                                     LEX *lex_arg, bool ps)
{
  DBUG_ASSERT(current_thd == thd);

  // No need to grab mutex for repeated (SQLCOM_END, NULL, false).
  if (sql_command == sql_cmd &&
      lex == lex_arg &&
      is_ps == ps)
  {
    return;
  }

  thd->lock_query_plan();
  sql_command= sql_cmd;
  lex= lex_arg;
  is_ps= ps;
  thd->unlock_query_plan();
}


void THD::Query_plan::set_modification_plan(Modification_plan *plan_arg)
{
  DBUG_ASSERT(current_thd == thd);
  mysql_mutex_assert_owner(&thd->LOCK_query_plan);
  modification_plan= plan_arg;
}

/**
  Push an error message into MySQL diagnostic area with line
  and position information.

  This function provides semantic action implementers with a way
  to push the famous "You have a syntax error near..." error
  message into the diagnostic area, which is normally produced only if
  a parse error is discovered internally by the Bison generated
  parser.

  @note Parse-time only function!

  @param thd            YYTHD
  @param location       YYSTYPE object: error position
  @param s              error message: NULL default means ER(ER_SYNTAX_ERROR)
*/

void THD::parse_error_at(const YYLTYPE &location, const char *s)
{
  uint lineno= location.raw.start ?
    m_parser_state->m_lip.get_lineno(location.raw.start) : 1;
  const char *pos= location.raw.start ? location.raw.start : "";
  ErrConvString err(pos, variables.character_set_client);
  my_printf_error(ER_PARSE_ERROR,  ER(ER_PARSE_ERROR), MYF(0),
                  s ? s : ER(ER_SYNTAX_ERROR), err.ptr(), lineno);
}

bool THD::send_result_metadata(List<Item> *list, uint flags)
{
  DBUG_ENTER("send_result_metadata");
  List_iterator_fast<Item> it(*list);
  Item *item;
  uchar buff[MAX_FIELD_WIDTH];
  String tmp((char *) buff, sizeof(buff), &my_charset_bin);

  if (m_protocol->start_result_metadata(list->elements, flags,
          variables.character_set_results))
    goto err;

#ifdef EMBEDDED_LIBRARY                  // bootstrap file handling
    if(!mysql)
      DBUG_RETURN(false);
#endif

  while ((item= it++))
  {
    Send_field field;
    item->make_field(&field);
#ifndef EMBEDDED_LIBRARY
    m_protocol->start_row();
    if (m_protocol->send_field_metadata(&field,
            item->charset_for_protocol()))
      goto err;
    if (flags & Protocol::SEND_DEFAULTS)
      item->send(m_protocol, &tmp);
    if (m_protocol->end_row())
      DBUG_RETURN(true);
#else
      if(m_protocol->send_field_metadata(&field, item->charset_for_protocol()))
        goto err;
      if (flags & Protocol::SEND_DEFAULTS)
        get_protocol_classic()->send_string_metadata(item->val_str(&tmp));
#endif
  }

  DBUG_RETURN(m_protocol->end_result_metadata());

  err:
  my_error(ER_OUT_OF_RESOURCES, MYF(0));        /* purecov: inspected */
  DBUG_RETURN(1);                               /* purecov: inspected */
}

bool THD::send_result_set_row(List<Item> *row_items)
{
  char buffer[MAX_FIELD_WIDTH];
  String str_buffer(buffer, sizeof (buffer), &my_charset_bin);
  List_iterator_fast<Item> it(*row_items);

  DBUG_ENTER("send_result_set_row");

  for (Item *item= it++; item; item= it++)
  {
    if (item->send(m_protocol, &str_buffer) || is_error())
      DBUG_RETURN(true);
    /*
      Reset str_buffer to its original state, as it may have been altered in
      Item::send().
    */
    str_buffer.set(buffer, sizeof(buffer), &my_charset_bin);
  }
  DBUG_RETURN(false);
}

void THD::send_statement_status()
{
  DBUG_ENTER("send_statement_status");
  DBUG_ASSERT(!get_stmt_da()->is_sent());
  bool error= false;
  Diagnostics_area *da= get_stmt_da();

  /* Can not be true, but do not take chances in production. */
  if (da->is_sent())
    DBUG_VOID_RETURN;

  switch (da->status())
  {
    case Diagnostics_area::DA_ERROR:
      /* The query failed, send error to log and abort bootstrap. */
      error= m_protocol->send_error(
              da->mysql_errno(), da->message_text(), da->returned_sqlstate());
          break;
    case Diagnostics_area::DA_EOF:
      error= m_protocol->send_eof(
              server_status, da->last_statement_cond_count());
          break;
    case Diagnostics_area::DA_OK:
      error= m_protocol->send_ok(
              server_status, da->last_statement_cond_count(),
              da->affected_rows(), da->last_insert_id(), da->message_text());
          break;
    case Diagnostics_area::DA_DISABLED:
      break;
    case Diagnostics_area::DA_EMPTY:
    default:
      DBUG_ASSERT(0);
          error= m_protocol->send_ok(server_status, 0, 0, 0, NULL);
          break;
  }
  if (!error)
    da->set_is_sent(true);
  DBUG_VOID_RETURN;
}

void THD::claim_memory_ownership()
{
  /*
    Ownership of the THD object is transfered to this thread.
    This happens typically:
    - in the event scheduler,
      when the scheduler thread creates a work item and
      starts a worker thread to run it
    - in the main thread, when the code that accepts a new
      network connection creates a work item and starts a
      connection thread to run it.
    Accounting for memory statistics needs to be told
    that memory allocated by thread X now belongs to thread Y,
    so that statistics by thread/account/user/host are accurate.
    Inspect every piece of memory allocated in THD,
    and call PSI_MEMORY_CALL(memory_claim)().
  */
#ifdef HAVE_PSI_MEMORY_INTERFACE
  claim_root(&main_mem_root);
  my_claim(m_token_array);
  Protocol_classic *p= get_protocol_classic();
  if (p != NULL)
    p->claim_memory_ownership();
  session_tracker.claim_memory_ownership();
  session_sysvar_res_mgr.claim_memory_ownership();
  my_hash_claim(&user_vars);
#if defined(ENABLED_DEBUG_SYNC)
  debug_sync_claim_memory_ownership(this);
#endif /* defined(ENABLED_DEBUG_SYNC) */
  get_transaction()->claim_memory_ownership();
  stmt_map.claim_memory_ownership();
#endif /* HAVE_PSI_MEMORY_INTERFACE */
}


void THD::rpl_detach_engine_ha_data()
{
#ifdef HAVE_REPLICATION
  Relay_log_info *rli=
    is_binlog_applier() ? rli_fake : (slave_thread ? rli_slave : NULL);

  DBUG_ASSERT(!rli_fake  || !rli_fake-> is_engine_ha_data_detached);
  DBUG_ASSERT(!rli_slave || !rli_slave->is_engine_ha_data_detached);

  if (rli)
    rli->detach_engine_ha_data(this);
#endif
};

void THD::rpl_reattach_engine_ha_data()
{
#ifdef HAVE_REPLICATION
  Relay_log_info *rli =
      is_binlog_applier() ? rli_fake : (slave_thread ? rli_slave : NULL);

  DBUG_ASSERT(!rli_fake || !rli_fake->is_engine_ha_data_detached);
  DBUG_ASSERT(!rli_slave || !rli_slave->is_engine_ha_data_detached);

  if (rli) rli->reattach_engine_ha_data(this);
#endif
}

bool THD::rpl_unflag_detached_engine_ha_data()
{
#ifdef HAVE_REPLICATION
  Relay_log_info *rli=
    is_binlog_applier() ? rli_fake : (slave_thread ? rli_slave : NULL);
  return rli ? rli->unflag_detached_engine_ha_data() : false;
#else
  return false;
#endif
}

/**
  Determine if binlogging is disabled for this session
  @retval 0 if the current statement binlogging is disabled
  (could be because of binlog closed/binlog option
  is set to false).
  @retval 1 if the current statement will be binlogged
*/
bool THD::is_current_stmt_binlog_disabled() const
{
  return (!(variables.option_bits & OPTION_BIN_LOG) ||
          !mysql_bin_log.is_open());
}

bool THD::is_current_stmt_binlog_row_enabled_with_write_set_extraction() const
{
  return ((variables.transaction_write_set_extraction != HASH_ALGORITHM_OFF) &&
          is_current_stmt_binlog_format_row() &&
          !is_current_stmt_binlog_disabled());
}
