/* Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/bootstrap.h"

#include "my_config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <string>

#include "m_string.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_loglevel.h"
#include "my_sys.h"
#include "my_thread.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/components/services/log_shared.h"
#include "mysql/psi/mysql_file.h"
#include "mysql/psi/mysql_thread.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/bootstrap_impl.h"
#include "sql/error_handler.h"  // Internal_error_handler
#include "sql/log.h"
#include "sql/mysqld.h"              // key_file_init
#include "sql/mysqld_thd_manager.h"  // Global_THD_manager
#include "sql/protocol_classic.h"
#include "sql/query_options.h"
#include "sql/set_var.h"
#include "sql/sql_bootstrap.h"
#include "sql/sql_class.h"    // THD
#include "sql/sql_connect.h"  // close_connection
#include "sql/sql_error.h"
#include "sql/sql_initialize.h"
#include "sql/sql_lex.h"
#include "sql/sql_parse.h"  // mysql_parse
#include "sql/sql_profile.h"
#include "sql/sys_vars_shared.h"  // intern_find_sys_var
#include "sql/system_variables.h"
#include "sql/transaction_info.h"

namespace bootstrap {

static MYSQL_FILE *bootstrap_file = NULL;
static bool bootstrap_error = false;
static bootstrap_functor bootstrap_handler = NULL;

int File_command_iterator::next(std::string &query, int *error,
                                int *query_source) {
  static char query_buffer[MAX_BOOTSTRAP_QUERY_SIZE];
  size_t length = 0;
  int rc;
  *query_source = QUERY_SOURCE_FILE;

  rc = read_bootstrap_query(query_buffer, &length, m_input, m_fgets_fn, error);
  if (rc == READ_BOOTSTRAP_SUCCESS) query.assign(query_buffer, length);
  return rc;
}

static char *mysql_file_fgets_fn(char *buffer, size_t size, MYSQL_FILE *input,
                                 int *error) {
  char *line = mysql_file_fgets(buffer, static_cast<int>(size), input);
  if (error) *error = (line == NULL) ? ferror(input->m_file) : 0;
  return line;
}

File_command_iterator::File_command_iterator(const char *file_name) {
  is_allocated = false;
  if (!(m_input =
            mysql_file_fopen(key_file_init, file_name, O_RDONLY, MYF(MY_WME))))
    return;
  m_fgets_fn = mysql_file_fgets_fn;
  is_allocated = true;
}

File_command_iterator::~File_command_iterator() { end(); }

void File_command_iterator::end(void) {
  if (is_allocated) {
    mysql_file_fclose(m_input, MYF(0));
    is_allocated = false;
    m_input = NULL;
  }
}

Command_iterator *Command_iterator::current_iterator = NULL;

static bool handle_bootstrap_impl(THD *thd) {
  std::string query;

  DBUG_ENTER("handle_bootstrap");
  File_command_iterator file_iter(bootstrap_file, mysql_file_fgets_fn);
  Compiled_in_command_iterator comp_iter;
  Key_length_error_handler error_handler;
  bool has_binlog_option = thd->variables.option_bits & OPTION_BIN_LOG;
  int query_source, last_query_source = -1;

  thd->thread_stack = (char *)&thd;
  thd->security_context()->assign_user(STRING_WITH_LEN("boot"));
  thd->security_context()->assign_priv_user("", 0);
  thd->security_context()->assign_priv_host("", 0);
  /*
    Make the "client" handle multiple results. This is necessary
    to enable stored procedures with SELECTs and Dynamic SQL
    in init-file.
  */
  thd->get_protocol_classic()->add_client_capability(CLIENT_MULTI_RESULTS);

  thd->init_query_mem_roots();

  if (opt_initialize)
    Command_iterator::current_iterator = &comp_iter;
  else
    Command_iterator::current_iterator = &file_iter;

  Command_iterator::current_iterator->begin();
  for (;;) {
    int error = 0;
    int rc;

    rc = Command_iterator::current_iterator->next(query, &error, &query_source);

    /*
      Execution of statements included in the binary is only supported during
      initial start.
    */
    DBUG_ASSERT(opt_initialize || query_source != QUERY_SOURCE_COMPILED);

    /*
      The statements included in the binary should be executed before any
      statement from an init file.
    */
    DBUG_ASSERT(query_source == last_query_source || last_query_source == -1 ||
                query_source != QUERY_SOURCE_COMPILED);

    /*
      During --initialize, the server will also read SQL statements from a
      file submitted with --init-file. While processing the compiled-in
      statements, DD table access is permitted. This is needed as a short
      term solution to allow SRS data to be entered by INSERT statements
      instead of CREATE statements. However, we must not allow the statements
      from an init file to access the DD tables. Thus, whenever we execute a
      statement from an init file, we must make sure that the thread type is
      set to the appropriate value. We check this on purpose for each query
      to avoid side effects from thread type being set elsewhere in the
      server code.
    */
    if (query_source == QUERY_SOURCE_FILE &&
        thd->system_thread != SYSTEM_THREAD_INIT_FILE) {
      thd->system_thread = SYSTEM_THREAD_INIT_FILE;
    }

    /*
      The server must avoid logging compiled statements into the binary log
      (and generating GTIDs for them when GTID_MODE is ON) during bootstrap/
      initialize procedures.
      We will disable SQL_LOG_BIN session variable before processing compiled
      statements, and will re-enable it before processing statements of the
      initialization file.
    */
    if (has_binlog_option && query_source != last_query_source) {
      switch (query_source) {
        case QUERY_SOURCE_COMPILED:
          thd->variables.option_bits &= ~OPTION_BIN_LOG;
          break;
        case QUERY_SOURCE_FILE:
          /*
            Some compiled script might have disable binary logging session
            variable during compiled scripts. Enabling it again as it was
            enabled before applying the compiled statements.
          */
          thd->variables.sql_log_bin = true;
          thd->variables.option_bits |= OPTION_BIN_LOG;
          break;
        default:
          DBUG_ASSERT(false);
          break;
      }
    }
    last_query_source = query_source;

    if (rc == READ_BOOTSTRAP_EOF) break;
    /*
      Check for bootstrap file errors. SQL syntax errors will be
      caught below.
    */
    if (rc != READ_BOOTSTRAP_SUCCESS) {
      /*
        mysql_parse() may have set a successful error status for the previous
        query. We must clear the error status to report the bootstrap error.
      */
      thd->get_stmt_da()->reset_diagnostics_area();

      /* Get the nearest query text for reference. */
      const char *err_ptr =
          query.c_str() + (query.length() <= MAX_BOOTSTRAP_ERROR_LEN
                               ? 0
                               : (query.length() - MAX_BOOTSTRAP_ERROR_LEN));
      switch (rc) {
        case READ_BOOTSTRAP_ERROR:
          my_printf_error(ER_UNKNOWN_ERROR,
                          "Bootstrap file error, return code (%d). "
                          "Nearest query: '%s'",
                          MYF(0), error, err_ptr);
          break;

        case READ_BOOTSTRAP_QUERY_SIZE:
          my_printf_error(ER_UNKNOWN_ERROR,
                          "Bootstrap file error. Query size "
                          "exceeded %d bytes near '%s'.",
                          MYF(0), MAX_BOOTSTRAP_LINE_SIZE, err_ptr);
          break;

        default:
          DBUG_ASSERT(false);
          break;
      }

      thd->send_statement_status();
      bootstrap_error = true;
      break;
    }

    char *query_copy = static_cast<char *>(thd->alloc(query.length() + 1));
    if (query_copy == NULL) {
      /* purecov: begin inspected */
      bootstrap_error = true;
      break;
      /* purecov: end */
    }
    memcpy(query_copy, query.c_str(), query.length());
    query_copy[query.length()] = '\0';
    thd->set_query(query_copy, query.length());
    thd->set_query_id(next_query_id());
    DBUG_PRINT("query", ("%-.4096s", thd->query().str));
#if defined(ENABLED_PROFILING)
    thd->profiling->start_new_query();
    thd->profiling->set_query_source(thd->query().str, thd->query().length);
#endif

    thd->set_time();
    Parser_state parser_state;
    if (parser_state.init(thd, thd->query().str, thd->query().length)) {
      /* purecov: begin inspected */
      thd->send_statement_status();
      bootstrap_error = true;
      break;
      /* purecov: end */
    }

    // Ignore ER_TOO_LONG_KEY for system tables.
    thd->push_internal_handler(&error_handler);
    mysql_parse(thd, &parser_state);
    thd->pop_internal_handler();

    bootstrap_error = thd->is_error();
    thd->send_statement_status();

#if defined(ENABLED_PROFILING)
    thd->profiling->finish_current_query();
#endif

    if (bootstrap_error) break;

    free_root(thd->mem_root, MYF(MY_KEEP_PREALLOC));
    thd->get_transaction()->free_memory(MYF(MY_KEEP_PREALLOC));

    /*
      If the last statement has enabled the session binary logging while
      processing queries that are compiled and must not be binary logged,
      we must disable binary logging again.
    */
    if (last_query_source == QUERY_SOURCE_COMPILED &&
        thd->variables.option_bits & OPTION_BIN_LOG)
      thd->variables.option_bits &= ~OPTION_BIN_LOG;
  }

  Command_iterator::current_iterator->end();

  /*
    We should re-enable SQL_LOG_BIN session if it was enabled by default
    but disabled during bootstrap/initialization.
  */
  if (has_binlog_option) {
    thd->variables.sql_log_bin = true;
    thd->variables.option_bits |= OPTION_BIN_LOG;
  }

  DBUG_RETURN(bootstrap_error);
}

/**
  Execute commands from bootstrap_file.

  Used when creating the initial grant tables.
*/

extern "C" {
static void *handle_bootstrap(void *arg) {
  THD *thd = (THD *)arg;

  mysql_thread_set_psi_id(thd->thread_id());

  /* The following must be called before DBUG_ENTER */
  thd->thread_stack = (char *)&thd;
  if (my_thread_init() || thd->store_globals()) {
    close_connection(thd, ER_OUT_OF_RESOURCES);
    thd->fatal_error();
    bootstrap_error = true;
    thd->get_protocol_classic()->end_net();
  } else {
    Global_THD_manager *thd_manager = Global_THD_manager::get_instance();
    thd_manager->add_thd(thd);

    // Set tx_read_only to false to allow installing DD tables even
    // if the server is started with --transaction-read-only=true.
    thd->variables.transaction_read_only = false;
    thd->tx_read_only = false;

    if (bootstrap_handler)
      bootstrap_error = (*bootstrap_handler)(thd);
    else
      bootstrap_error = handle_bootstrap_impl(thd);

    thd->get_protocol_classic()->end_net();
    thd->release_resources();
    thd_manager->remove_thd(thd);
  }
  my_thread_end();
  return 0;
}
}  // extern "C"

bool run_bootstrap_thread(MYSQL_FILE *file, bootstrap_functor boot_handler,
                          enum_thread_type thread_type) {
  DBUG_ENTER("bootstrap");

  THD *thd = new THD;
  thd->system_thread = thread_type;
  thd->get_protocol_classic()->init_net(NULL);
  thd->security_context()->set_master_access(~(ulong)0);

  thd->set_new_thread_id();

  bootstrap_file = file;
  bootstrap_handler = boot_handler;

  // Set server default sql_mode irrespective of
  // mysqld server command line argument.
  thd->variables.sql_mode = intern_find_sys_var("sql_mode", 0)->get_default();

  /*
    Set default value for explicit_defaults_for_timestamp variable. Bootstrap
    thread creates dictionary tables. The creation of dictionary tables should
    be independent of the value of explicit_defaults_for_timestamp specified by
    the user.
  */
  thd->variables.explicit_defaults_for_timestamp =
      intern_find_sys_var("explicit_defaults_for_timestamp", 0)->get_default();

  my_thread_attr_t thr_attr;
  my_thread_attr_init(&thr_attr);
#ifndef _WIN32
  pthread_attr_setscope(&thr_attr, PTHREAD_SCOPE_SYSTEM);
#endif
  my_thread_attr_setdetachstate(&thr_attr, MY_THREAD_CREATE_JOINABLE);
  my_thread_handle thread_handle;
  // What about setting THD::real_id?
  int error = mysql_thread_create(key_thread_bootstrap, &thread_handle,
                                  &thr_attr, handle_bootstrap, thd);
  if (error) {
    /* purecov: begin inspected */
    LogErr(WARNING_LEVEL, ER_BOOTSTRAP_CANT_THREAD, errno).os_errno(errno);

    DBUG_RETURN(true);
    /* purecov: end */
  }
  /* Wait for thread to die */
  my_thread_join(&thread_handle, NULL);
  // Free Items that were created during this execution.
  thd->free_items();
  delete thd;
  DBUG_RETURN(bootstrap_error);
}
}  // namespace bootstrap
