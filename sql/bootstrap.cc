/* Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "bootstrap.h"

#include "log.h"                 // sql_print_warning
#include "mysqld_thd_manager.h"  // Global_THD_manager
#include "bootstrap_impl.h"
#include "sql_initialize.h"
#include "sql_class.h"           // THD
#include "sql_connect.h"         // close_connection
#include "sql_parse.h"           // mysql_parse

#include "pfs_file_provider.h"
#include "mysql/psi/mysql_file.h"

static MYSQL_FILE *bootstrap_file= NULL;
static const char *bootstrap_query= NULL;
static int bootstrap_error= 0;


class Query_command_iterator: public Command_iterator
{
public:
  Query_command_iterator(const char* query):
    m_query(query), m_is_read(false) {}
  virtual int next(std::string &query, int *read_error, int *query_source)
  {
    if (m_is_read)
      return READ_BOOTSTRAP_EOF;

    query= m_query;
    m_is_read= true;
    *read_error= 0;
    *query_source= QUERY_SOURCE_COMPILED;
    return READ_BOOTSTRAP_SUCCESS;
  }
private:
  const char *m_query; // Owned externally.
  bool m_is_read;
};


int File_command_iterator::next(std::string &query, int *error,
                                int *query_source)
{
  static char query_buffer[MAX_BOOTSTRAP_QUERY_SIZE];
  size_t length= 0;
  int rc;
  *query_source= QUERY_SOURCE_FILE;

  rc= read_bootstrap_query(query_buffer, &length, m_input, m_fgets_fn, error);
  if (rc == READ_BOOTSTRAP_SUCCESS)
    query.assign(query_buffer, length);
  return rc;
}


char *mysql_file_fgets_fn(char *buffer, size_t size, MYSQL_FILE* input, int *error)
{
  char *line= mysql_file_fgets(buffer, static_cast<int>(size), input);
  if (error)
    *error= (line == NULL) ? ferror(input->m_file) : 0;
  return line;
}

File_command_iterator::File_command_iterator(const char *file_name)
{
  is_allocated= false;
  if (!(m_input= mysql_file_fopen(key_file_init, file_name,
    O_RDONLY, MYF(MY_WME))))
    return;
  m_fgets_fn= mysql_file_fgets_fn;
  is_allocated= true;
}

File_command_iterator::~File_command_iterator()
{
  end();
}

void File_command_iterator::end(void)
{
  if (is_allocated)
  {
    mysql_file_fclose(m_input, MYF(0));
    is_allocated= false;
    m_input= NULL;
  }
}

Command_iterator *Command_iterator::current_iterator= NULL;

static void handle_bootstrap_impl(THD *thd)
{
  std::string query;

  DBUG_ENTER("handle_bootstrap");
  File_command_iterator file_iter(bootstrap_file, mysql_file_fgets_fn);
  Compiled_in_command_iterator comp_iter;
  Query_command_iterator query_iter(bootstrap_query);
  bool has_binlog_option= thd->variables.option_bits & OPTION_BIN_LOG;
  int query_source, last_query_source= -1;

  thd->thread_stack= (char*) &thd;
  thd->security_context()->assign_user(STRING_WITH_LEN("boot"));
  thd->security_context()->assign_priv_user("", 0);
  thd->security_context()->assign_priv_host("", 0);
  /*
    Make the "client" handle multiple results. This is necessary
    to enable stored procedures with SELECTs and Dynamic SQL
    in init-file.
  */
    thd->get_protocol_classic()->add_client_capability(
    CLIENT_MULTI_RESULTS);

  thd->init_for_queries();

  /*
    If a single bootstrap query is submitted, execute it regardless of the
    command line options. If no query is submitted, read commands from the
    executable or from file depending on option.
  */
  if (bootstrap_query)
  {
    Command_iterator::current_iterator= &query_iter;
    bootstrap_query= NULL;
  }
  else
  {
    if (opt_initialize)
      Command_iterator::current_iterator= &comp_iter;
    else
      Command_iterator::current_iterator= &file_iter;
  }

  Command_iterator::current_iterator->begin();
  for ( ; ; )
  {
    int error= 0;
    int rc;

    rc= Command_iterator::current_iterator->next(query, &error, &query_source);

    /*
      The server must avoid logging compiled statements into the binary log
      (and generating GTIDs for them when GTID_MODE is ON) during bootstrap/
      initialize procedures.
      We will disable SQL_LOG_BIN session variable before processing compiled
      statements, and will re-enable it before processing statements of the
      initialization file.
    */
    if (has_binlog_option && query_source != last_query_source)
    {
      switch (query_source)
      {
      case QUERY_SOURCE_COMPILED:
        thd->variables.option_bits&= ~OPTION_BIN_LOG;
        break;
      case QUERY_SOURCE_FILE:
        /*
          Some compiled script might have disable binary logging session
          variable during compiled scripts. Enabling it again as it was
          enabled before applying the compiled statements.
        */
        thd->variables.sql_log_bin= true;
        thd->variables.option_bits|= OPTION_BIN_LOG;
        break;
      default:
        DBUG_ASSERT(false);
        break;
      }
    }
    last_query_source= query_source;

    if (rc == READ_BOOTSTRAP_EOF)
      break;
    /*
      Check for bootstrap file errors. SQL syntax errors will be
      caught below.
    */
    if (rc != READ_BOOTSTRAP_SUCCESS)
    {
      /*
        mysql_parse() may have set a successful error status for the previous
        query. We must clear the error status to report the bootstrap error.
      */
      thd->get_stmt_da()->reset_diagnostics_area();

      /* Get the nearest query text for reference. */
      const char *err_ptr= query.c_str() + (query.length() <= MAX_BOOTSTRAP_ERROR_LEN ?
                                        0 : (query.length() - MAX_BOOTSTRAP_ERROR_LEN));
      switch (rc)
      {
      case READ_BOOTSTRAP_ERROR:
        my_printf_error(ER_UNKNOWN_ERROR,
                        "Bootstrap file error, return code (%d). "
                        "Nearest query: '%s'", MYF(0), error, err_ptr);
        break;

      case READ_BOOTSTRAP_QUERY_SIZE:
        my_printf_error(ER_UNKNOWN_ERROR, "Bootstrap file error. Query size "
                        "exceeded %d bytes near '%s'.", MYF(0),
                        MAX_BOOTSTRAP_LINE_SIZE, err_ptr);
        break;

      default:
        DBUG_ASSERT(false);
        break;
      }

      thd->send_statement_status();
      bootstrap_error= 1;
      break;
    }

    char *query_copy= static_cast<char*>(thd->alloc(query.length() + 1));
    if (query_copy == NULL)
    {
      bootstrap_error= 1;
      break;
    }
    memcpy(query_copy, query.c_str(), query.length());
    query_copy[query.length()]= '\0';
    thd->set_query(query_copy, query.length());
    thd->set_query_id(next_query_id());
    DBUG_PRINT("query",("%-.4096s",thd->query().str));
#if defined(ENABLED_PROFILING)
    thd->profiling.start_new_query();
    thd->profiling.set_query_source(thd->query().str, thd->query().length);
#endif

    thd->set_time();
    Parser_state parser_state;
    if (parser_state.init(thd, thd->query().str, thd->query().length))
    {
      thd->send_statement_status();
      bootstrap_error= 1;
      break;
    }

    mysql_parse(thd, &parser_state);

    bootstrap_error= thd->is_error();
    thd->send_statement_status();

#if defined(ENABLED_PROFILING)
    thd->profiling.finish_current_query();
#endif

    if (bootstrap_error)
      break;

    free_root(thd->mem_root,MYF(MY_KEEP_PREALLOC));
    thd->get_transaction()->free_memory(MYF(MY_KEEP_PREALLOC));

    /*
      If the last statement has enabled the session binary logging while
      processing queries that are compiled and must not be binary logged,
      we must disable binary logging again.
    */
    if (last_query_source == QUERY_SOURCE_COMPILED &&
        thd->variables.option_bits & OPTION_BIN_LOG)
      thd->variables.option_bits&= ~OPTION_BIN_LOG;

  }

  Command_iterator::current_iterator->end();

  /*
    We should re-enable SQL_LOG_BIN session if it was enabled by default
    but disabled during bootstrap/initialization.
  */
  if (has_binlog_option)
  {
    thd->variables.sql_log_bin= true;
    thd->variables.option_bits|= OPTION_BIN_LOG;
  }

  DBUG_VOID_RETURN;
}


/**
  Execute commands from bootstrap_file.

  Used when creating the initial grant tables.
*/

namespace {
extern "C" void *handle_bootstrap(void *arg)
{
  THD *thd=(THD*) arg;

  mysql_thread_set_psi_id(thd->thread_id());

  /* The following must be called before DBUG_ENTER */
  thd->thread_stack= (char*) &thd;
  if (my_thread_init() || thd->store_globals())
  {
#ifndef EMBEDDED_LIBRARY
    close_connection(thd, ER_OUT_OF_RESOURCES);
#endif
    thd->fatal_error();
    bootstrap_error= 1;
    thd->get_protocol_classic()->end_net();
  }
  else
  {
    Global_THD_manager *thd_manager= Global_THD_manager::get_instance();
    thd_manager->add_thd(thd);

    handle_bootstrap_impl(thd);

    thd->get_protocol_classic()->end_net();
    thd->release_resources();
    thd_manager->remove_thd(thd);
  }
  my_thread_end();
  return 0;
}
} // namespace


int bootstrap(MYSQL_FILE *file)
{
  DBUG_ENTER("bootstrap");

  THD *thd= new THD;
  thd->bootstrap= 1;
  thd->get_protocol_classic()->init_net(NULL);
  thd->security_context()->set_master_access(~(ulong)0);

  thd->set_new_thread_id();

  bootstrap_file=file;

  my_thread_attr_t thr_attr;
  my_thread_attr_init(&thr_attr);
#ifndef _WIN32
  pthread_attr_setscope(&thr_attr, PTHREAD_SCOPE_SYSTEM);
#endif
  my_thread_attr_setdetachstate(&thr_attr, MY_THREAD_CREATE_JOINABLE);
  my_thread_handle thread_handle;
  // What about setting THD::real_id?
  int error= mysql_thread_create(key_thread_bootstrap,
                                 &thread_handle, &thr_attr, handle_bootstrap, thd);
  if (error)
  {
    sql_print_warning("Can't create thread to handle bootstrap (errno= %d)",
                      error);
    DBUG_RETURN(-1);
  }
  /* Wait for thread to die */
  my_thread_join(&thread_handle, NULL);
  delete thd;
  DBUG_RETURN(bootstrap_error);
}

int bootstrap_single_query(const char* query)
{
  bootstrap_query= query;
  return bootstrap(NULL);
}
