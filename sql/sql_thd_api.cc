/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <string.h>
#include <sys/types.h>
#include <algorithm>
#include <atomic>

#include "lex_string.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_io.h"
#include "my_macros.h"
#include "my_sqlcommand.h"
#include "my_thread.h"
#include "my_thread_local.h"
#include "mysql/components/services/bits/psi_stage_bits.h"
#include "mysql/components/services/bits/psi_thread_bits.h"
#include "mysql/plugin.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/service_thd_engine_lock.h"
#include "mysql/strings/m_ctype.h"
#include "mysql_com.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/conn_handler/connection_handler_manager.h"
#include "sql/current_thd.h"  // current_thd
#include "sql/handler.h"
#include "sql/mysqld.h"              // key_thread_one_connection
#include "sql/mysqld_thd_manager.h"  // Global_THD_manager
#include "sql/protocol_classic.h"
#include "sql/query_options.h"
#include "sql/resourcegroups/platform/thread_attrs_api.h"  // num_vcpus
#include "sql/rpl_replica_commit_order_manager.h"  // check_and_report_deadlock
#include "sql/rpl_rli.h"                           // is_mts_worker
#include "sql/sql_alter.h"
#include "sql/sql_callback.h"  // MYSQL_CALLBACK
#include "sql/sql_class.h"     // THD
#include "sql/sql_error.h"
#include "sql/sql_lex.h"
#include "sql/sql_plugin.h"  // plugin_unlock
#include "sql/sql_plugin_ref.h"
#include "sql/sql_thd_internal_api.h"
#include "sql/strfunc.h"
#include "sql/system_variables.h"
#include "sql/transaction_info.h"
#include "sql/xa.h"
#include "sql_string.h"
#include "string_with_len.h"
#include "violite.h"

struct MYSQL_LEX_STRING;

using std::min;

//////////////////////////////////////////////////////////
//
//  Definitions of functions declared in thread_pool_priv.h
//
//////////////////////////////////////////////////////////

/**
  Get reference to scheduler data object

  @param thd            THD object

  @retval               Scheduler data object on THD
*/

void *thd_get_scheduler_data(THD *thd) { return thd->scheduler.data; }

/**
  Set reference to Scheduler data object for THD object

  @param thd            THD object
  @param data           Scheduler data object to set on THD
*/

void thd_set_scheduler_data(THD *thd, void *data) {
  thd->scheduler.data = data;
}

/**
  Get reference to Performance Schema object for THD object

  @param thd            THD object

  @retval               Performance schema object for thread on THD
*/

PSI_thread *thd_get_psi(THD *thd) { return thd->get_psi(); }

/**
  Get net_wait_timeout for THD object

  @param thd            THD object

  @retval               net_wait_timeout value for thread on THD
*/

ulong thd_get_net_wait_timeout(THD *thd) {
  return thd->variables.net_wait_timeout;
}

/**
  Set reference to Performance Schema object for THD object

  @param thd            THD object
  @param psi            Performance schema object for thread
*/

void thd_set_psi(THD *thd, PSI_thread *psi) { thd->set_psi(psi); }

/**
  Set the state on connection to killed

  @param thd               THD object
*/

void thd_set_killed(THD *thd) {
  /*
    TODO: This method just sets the state of the THD::killed member. Now used
          for the idle threads. To awake and set killed status for active
          threads, THD::awake() should be used as part of this method or in a
          new API.
          Setting KILL state for a thread in a kill immune mode is handled
          as part of THD::awake(). Direct KILL state set for active thread
          breaks it.
  */
  thd->killed = THD::KILL_CONNECTION;
}

/**
  Clear errors from the previous THD

  @param thd              THD object
*/

void thd_clear_errors(THD *thd [[maybe_unused]]) { set_my_errno(0); }

/**
  Close the socket used by this connection

  @param thd                THD object
  @note Expects lock on thd->LOCK_thd_data.
*/

void thd_close_connection(THD *thd) {
  mysql_mutex_assert_owner(&thd->LOCK_thd_data);
  thd->shutdown_active_vio();
}

/**
  Get current THD object from thread local data

  @retval     The THD object for the thread, NULL if not connection thread
*/

THD *thd_get_current_thd() { return current_thd; }

/**
  Reset thread globals associated.

  @param thd     THD object
*/

void reset_thread_globals(THD *thd) {
  thd->restore_globals();
  thd->set_is_killable(false);
}

/**
  Lock data that needs protection in THD object

  @param thd                   THD object
*/

void thd_lock_data(THD *thd) { mysql_mutex_lock(&thd->LOCK_thd_data); }

/**
  Unlock data that needs protection in THD object

  @param thd                   THD object
*/

void thd_unlock_data(THD *thd) { mysql_mutex_unlock(&thd->LOCK_thd_data); }

/**
  Support method to check if connection has already started transaction

  @param thd Current thread

  @retval               true if connection already started transaction
*/

bool thd_is_transaction_active(THD *thd) {
  return thd->get_transaction()->is_active(Transaction_ctx::SESSION);
}

/**
  Predicate for determining if connection is in active multi-statement
  transaction.
 */
bool thd_in_active_multi_stmt_transaction(const THD *thd) {
  return thd->in_active_multi_stmt_transaction();
}

/**
  Check if there is buffered data on the socket representing the connection

  @param thd                  THD object
*/

int thd_connection_has_data(THD *thd) {
  Vio *vio = thd->get_protocol_classic()->get_vio();
  return vio->has_data(vio);
}

/**
  Get reading/writing on socket from THD object
  @param thd                       THD object

  @retval               net.reading_or_writing value for thread on THD.
*/

uint thd_get_net_read_write(THD *thd) {
  return thd->get_protocol_classic()->get_rw_status();
}

/**
  Set reading/writing on socket, used by SHOW PROCESSLIST

  @param thd                       THD object
  @param val                       Value to set it to (0 or 1)
*/

void thd_set_net_read_write(THD *thd, uint val) {
  thd->get_protocol_classic()->get_net()->reading_or_writing = val;
}

/**
  Mark the THD as not killable as it is not currently used by a thread.

  @param thd             THD object
*/

void thd_set_not_killable(THD *thd) { thd->set_is_killable(false); }

/**
  Get socket file descriptor for this connection

  @param thd            THD object

  @retval               Socket of the connection
*/

my_socket thd_get_fd(THD *thd) {
  return thd->get_protocol_classic()->get_socket();
}

/**
  Get MYSQL_SOCKET struct for this connection

  @param thd            THD object

  @retval               MYSQL_SOCKET struct of the connection
*/
MYSQL_SOCKET thd_get_mysql_socket(THD *thd) {
  return thd->get_protocol_classic()->get_vio()->mysql_socket;
}

/**
  Set thread specific environment required for thd cleanup in thread pool.

  @param thd            THD object
*/

void thd_store_globals(THD *thd) { thd->store_globals(); }

/**
  Get thread attributes for connection threads

  @retval      Reference to thread attribute for connection threads
*/

my_thread_attr_t *get_connection_attrib() { return &connection_attrib; }

/**
  Get max number of connections

  @retval         Max number of connections for MySQL Server
*/

ulong get_max_connections() { return max_connections; }

longlong get_incoming_connects() {
  return Connection_handler_manager::get_incoming_connects();
}

longlong get_aborted_connects() {
  return Connection_handler_manager::get_instance()->aborted_connects();
}

//////////////////////////////////////////////////////////
//
//  Definitions of functions declared in plugin.h
//
//////////////////////////////////////////////////////////

void thd_binlog_pos(const MYSQL_THD thd, const char **file_var,
                    unsigned long long *pos_var) {
  thd->get_trans_pos(file_var, pos_var);
}

int mysql_tmpfile(const char *prefix) {
  return mysql_tmpfile_path(mysql_tmpdir, prefix);
}

int thd_in_lock_tables(const MYSQL_THD thd) { return thd->in_lock_tables; }

int thd_tablespace_op(const MYSQL_THD thd) {
  /*
    The Alter_info is reset only at the beginning of an ALTER
    statement, so this function must check both the SQL command
    code and the Alter_info::flags.
  */
  int ret = 0;

  if (thd->lex->sql_command == SQLCOM_ALTER_TABLE) {
    if (thd->lex->alter_info->flags & Alter_info::ALTER_DISCARD_TABLESPACE) {
      ret = Alter_info::ALTER_DISCARD_TABLESPACE;
    }
    if (thd->lex->alter_info->flags & Alter_info::ALTER_IMPORT_TABLESPACE) {
      ret = Alter_info::ALTER_IMPORT_TABLESPACE;
    }
  }

  return (ret);
}

static void set_thd_stage_info(MYSQL_THD thd, const PSI_stage_info *new_stage,
                               PSI_stage_info *old_stage,
                               const char *calling_func,
                               const char *calling_file,
                               const unsigned int calling_line) {
  if (thd == nullptr) thd = current_thd;

  thd->enter_stage(new_stage, old_stage, calling_func, calling_file,
                   calling_line);
}

extern "C" const char *set_thd_proc_info(MYSQL_THD thd_arg, const char *info,
                                         const char *calling_function,
                                         const char *calling_file,
                                         const unsigned int calling_line) {
  PSI_stage_info old_stage;
  PSI_stage_info new_stage;

  old_stage.m_key = 0;
  old_stage.m_name = info;

  set_thd_stage_info(thd_arg, &old_stage, &new_stage, calling_function,
                     calling_file, calling_line);

  return new_stage.m_name;
}

void **thd_ha_data(const MYSQL_THD thd, const struct handlerton *hton) {
  return &(const_cast<THD *>(thd))->get_ha_data(hton->slot)->ha_ptr;
}

void thd_storage_lock_wait(MYSQL_THD thd, long long value) {
  thd->inc_lock_usec(value);
}

/**
  Provide a handler data getter to simplify coding
*/
void *thd_get_ha_data(const MYSQL_THD thd, const struct handlerton *hton) {
  return *thd_ha_data(thd, hton);
}

/**
  Provide a handler data setter to simplify coding
  @see thd_set_ha_data() definition in plugin.h
*/
void thd_set_ha_data(MYSQL_THD thd, const struct handlerton *hton,
                     const void *ha_data) {
  plugin_ref *lock = &thd->get_ha_data(hton->slot)->lock;
  if (ha_data && !*lock)
    *lock = ha_lock_engine(nullptr, hton);
  else if (!ha_data && *lock) {
    plugin_unlock(nullptr, *lock);
    *lock = nullptr;
  }
  *thd_ha_data(thd, hton) = const_cast<void *>(ha_data);
}

long long thd_test_options(const MYSQL_THD thd, long long test_options) {
  return thd->variables.option_bits & test_options;
}

int thd_sql_command(const MYSQL_THD thd) { return (int)thd->lex->sql_command; }

int thd_tx_isolation(const MYSQL_THD thd) { return (int)thd->tx_isolation; }

int thd_tx_is_read_only(const MYSQL_THD thd) {
  // If the transaction is marked to be skipped read-only  then we ignore
  // the value of tx_read_only variable and treat the transaction as
  // a normal read-write trx.
  if (thd->tx_read_only && thd->is_cmd_skip_transaction_read_only())
    return 0;
  else
    return (int)thd->tx_read_only;
}

int thd_tx_priority(const MYSQL_THD thd) {
  return (thd->thd_tx_priority != 0 ? thd->thd_tx_priority : thd->tx_priority);
}

MYSQL_THD thd_tx_arbitrate(MYSQL_THD requestor, MYSQL_THD holder) {
  /* Should be different sessions. */
  assert(holder != requestor);

  return (thd_tx_priority(requestor) == thd_tx_priority(holder)
              ? requestor
              : ((thd_tx_priority(requestor) > thd_tx_priority(holder))
                     ? holder
                     : requestor));
}

int thd_tx_is_dd_trx(const MYSQL_THD thd) {
  return (int)thd->is_attachable_ro_transaction_active();
}

void thd_inc_row_count(MYSQL_THD thd) {
  thd->get_stmt_da()->inc_current_row_for_condition();
}

/**
  Returns the size of the beginning part of a (multibyte) string,
  which can fit in max_size bytes.

  @param[in] cs charset_info
  @param[in] start pointer to the string
  @param[in] original_size the length of the string (in bytes)
  @param[in] max_size the size of the buffer which needs to hold the string
  @return  the maximum length of a prefix of the string, that can be stored
*/
static size_t truncated_str_length(const CHARSET_INFO *cs, const char *start,
                                   size_t original_size, size_t max_size) {
  if (max_size >= original_size) return original_size;

  uint next_char_len;
  auto next_char = start;
  auto end = start + original_size;

  while ((next_char_len = my_mbcharlen_ptr(cs, next_char, end)) > 0 &&
         (size_t)(next_char + next_char_len - start) <= max_size) {
    next_char += next_char_len;
    assert(next_char < end);
    // *next_char is always a valid expression, since max_size < original_size
  }
  return next_char - start;
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

char *thd_security_context(MYSQL_THD thd, char *buffer, size_t length,
                           size_t max_query_len) {
  String str(buffer, length, &my_charset_latin1);
  Security_context *sctx = &thd->m_main_security_ctx;
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
  const char *proc_info = thd->proc_info();

  len = snprintf(header, sizeof(header),
                 "MySQL thread id %u, OS thread handle %lu, query id %lu",
                 thd->thread_id(), (ulong)thd->real_id, (ulong)thd->query_id);
  str.length(0);
  str.append(header, len);

  if (sctx->host().length) {
    str.append(' ');
    str.append(sctx->host().str);
  }

  if (sctx->ip().length) {
    str.append(' ');
    str.append(sctx->ip().str);
  }

  if (sctx->user().str) {
    str.append(' ');
    str.append(sctx->user().str);
  }

  if (proc_info) {
    str.append(' ');
    str.append(proc_info);
  }

  mysql_mutex_lock(&thd->LOCK_thd_query);

  if (thd->query().str) {
    if (max_query_len < 1)
      len = thd->query().length;
    else
      len = min(thd->query().length, max_query_len);
    str.append('\n');
    str.append(thd->query().str,
               truncated_str_length(thd->charset(), thd->query().str,
                                    thd->query().length, len));
  }

  mysql_mutex_unlock(&thd->LOCK_thd_query);

  if (str.c_ptr_safe() == buffer) return buffer;

  /*
    We have to copy the new string to the destination buffer because the string
    was reallocated to a larger buffer to be able to fit.
  */
  assert(buffer != nullptr);
  length = truncated_str_length(thd->charset(), str.c_ptr_quick(), str.length(),
                                length - 1);
  memcpy(buffer, str.c_ptr_quick(), length);
  /* Make sure that the new string is null terminated */
  buffer[length] = '\0';
  return buffer;
}

void thd_get_xid(const MYSQL_THD thd, MYSQL_XID *xid) {
  *xid = *pointer_cast<const MYSQL_XID *>(
      thd->get_transaction()->xid_state()->get_xid());
}

int thd_killed(const void *v_thd) {
  const THD *thd = static_cast<const THD *>(v_thd);
  if (thd == nullptr) thd = current_thd;
  if (thd == nullptr) return 0;
  return thd->killed;
}

/**
  Set the killed status of the current statement.

  @param thd  user thread connection handle
*/

void thd_set_kill_status(const MYSQL_THD thd) { thd->send_kill_message(); }

unsigned long thd_get_thread_id(const MYSQL_THD thd) {
  return ((unsigned long)thd->thread_id());
}

/**
  Check if batching is allowed for the thread
  @param thd  user thread
  @retval 1 batching allowed
  @retval 0 batching not allowed
*/

int thd_allow_batch(MYSQL_THD thd) {
  if ((thd->variables.option_bits & OPTION_ALLOW_BATCH) ||
      (thd->slave_thread && opt_replica_allow_batching))
    return 1;
  return 0;
}

void thd_mark_transaction_to_rollback(MYSQL_THD thd, int all) {
  DBUG_TRACE;
  assert(thd);
  /*
    The parameter "all" has type int since the function is defined
    in plugin.h. The corresponding parameter in the call below has
    type bool. The comment in plugin.h states that "all != 0"
    means to rollback the main transaction. Thus, check this
    specifically.
  */
  thd->mark_transaction_to_rollback((all != 0));
}

//////////////////////////////////////////////////////////
//
//  Definitions of functions declared in service_thd_alloc.h
//
//////////////////////////////////////////////////////////

void *thd_alloc(MYSQL_THD thd, size_t size) { return thd->alloc(size); }

void *thd_calloc(MYSQL_THD thd, size_t size) { return thd->mem_calloc(size); }

char *thd_strdup(MYSQL_THD thd, const char *str) {
  return thd->mem_strdup(str);
}

char *thd_strmake(MYSQL_THD thd, const char *str, size_t size) {
  return thd->strmake(str, size);
}

MYSQL_LEX_STRING *thd_make_lex_string(MYSQL_THD thd, MYSQL_LEX_STRING *lex_str,
                                      const char *str, size_t size,
                                      int allocate_lex_string) {
  if (allocate_lex_string != 0)
    return make_lex_string_root(thd->mem_root, str, size);
  if (lex_string_strmake(thd->mem_root, lex_str, str, size)) return nullptr;
  return lex_str;
}

void *thd_memdup(MYSQL_THD thd, const void *str, size_t size) {
  return thd->memdup(str, size);
}

//////////////////////////////////////////////////////////
//
//  Definitions of functions declared in service_thd_wait.h
//
//////////////////////////////////////////////////////////

/**
  Interface for MySQL Server, plugins and storage engines to report
  when they are going to sleep/stall.

  This is currently only implemented by by the threadpool and used to have
  better knowledge of which threads that currently are actively running on CPUs.
  When not running with TP this makes a call, possibly through a service,
  to an empty function.

  thd_wait_end MUST be called immediately after waking up again.

  More info can be found in the TP documentation.

  @param thd Calling thread context. If nullptr is passed, current_thd is used.
  @param wait_type An enum value from the enum thd_wait_type (defined
                   in include/mysql/service_thd_wait.h) but passed as int
                   to preserve compatibility with exported service api.
*/
void thd_wait_begin(MYSQL_THD thd, int wait_type) {
  MYSQL_CALLBACK(Connection_handler_manager::event_functions, thd_wait_begin,
                 (thd, wait_type));
}

/**
  Interface for MySQL Server, plugins and storage engines to report
  when they waking up from a sleep/stall.

  This is currently only implemented by by the threadpool and used to have
  better knowledge of which threads that currently are actively running on CPUs.
  When not running with TP this makes a call, possibly through a service,
  to an empty function.

  More info can be found in the TP documentation.

  @param thd Calling thread context. If nullptr is passed, current_thd is used.
*/
void thd_wait_end(MYSQL_THD thd) {
  MYSQL_CALLBACK(Connection_handler_manager::event_functions, thd_wait_end,
                 (thd));
}

//////////////////////////////////////////////////////////
//
//  Definitions of functions declared in service_thd_engine_lock.h
//
//////////////////////////////////////////////////////////

void thd_report_row_lock_wait(THD *self, THD *wait_for) {
  DBUG_TRACE;
  thd_report_lock_wait(self, wait_for, true);
}

void thd_report_lock_wait(THD *self, THD *wait_for,
                          bool /* may_survive_prepare*/) {
  DBUG_TRACE;
  CONDITIONAL_SYNC_POINT("report_lock_collision");

  if (self != nullptr && wait_for != nullptr && is_mts_worker(self) &&
      is_mts_worker(wait_for))
    Commit_order_manager::check_and_report_deadlock(self, wait_for);
}

/**
  Interface for cleaning the openssl per thread error queue.
*/

void remove_ssl_err_thread_state() {
#if !defined(HAVE_OPENSSL11)
  ERR_remove_thread_state(nullptr);
#endif
}

unsigned int thd_get_num_vcpus() {
  return resourcegroups::platform::num_vcpus();
}

bool thd_check_connection_admin_privilege(MYSQL_THD thd) {
  Security_context *sctx = thd->security_context();
  return (!(sctx->check_access(SUPER_ACL) ||
            sctx->has_global_grant(STRING_WITH_LEN("CONNECTION_ADMIN")).first));
}

unsigned int thd_get_current_thd_terminology_use_previous() {
  if (!current_thd) return 0;
  return current_thd->variables.terminology_use_previous;
}
