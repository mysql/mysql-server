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
#include "error_handler.h"       // Internal_error_handler
#include "mysqld.h"              // key_file_init
#include "sql_initialize.h"
#include "sql_class.h"           // THD
#include "sql_connect.h"         // close_connection
#include "sql_parse.h"           // mysql_parse
#include "sys_vars_shared.h"     // intern_find_sys_var

#include "pfs_file_provider.h"
#include "mysql/psi/mysql_file.h"

namespace bootstrap {

static MYSQL_FILE *bootstrap_file= NULL;
static bool bootstrap_error= false;
static bootstrap_functor bootstrap_handler= NULL;

int File_command_iterator::next(std::string &query, int *error)
{
  static char query_buffer[MAX_BOOTSTRAP_QUERY_SIZE];
  size_t length= 0;
  int rc;

  rc= read_bootstrap_query(query_buffer, &length, m_input, m_fgets_fn, error);
  if (rc == READ_BOOTSTRAP_SUCCESS)
    query.assign(query_buffer, length);
  return rc;
}


static char *mysql_file_fgets_fn(char *buffer, size_t size, MYSQL_FILE* input,
                                 int *error)
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


// Disable ER_TOO_LONG_KEY for creation of system tables.
// See Bug#20629014.
class Key_length_error_handler : public Internal_error_handler
{
public:
  virtual bool handle_condition(THD *,
                                uint sql_errno,
                                const char*,
                                Sql_condition::enum_severity_level *,
                                const char*)
  {
    return (sql_errno == ER_TOO_LONG_KEY);
  }
};


static bool handle_bootstrap_impl(THD *thd)
{
  std::string query;

  DBUG_ENTER("handle_bootstrap");
  File_command_iterator file_iter(bootstrap_file, mysql_file_fgets_fn);
  Compiled_in_command_iterator comp_iter;
  Key_length_error_handler error_handler;

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

  thd->init_query_mem_roots();

  if (opt_initialize)
    Command_iterator::current_iterator= &comp_iter;
  else
    Command_iterator::current_iterator= &file_iter;

  Command_iterator::current_iterator->begin();
  for ( ; ; )
  {
    int error= 0;
    int rc;

    rc= Command_iterator::current_iterator->next(query, &error);

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
      bootstrap_error= true;
      break;
    }

    char *query_copy= static_cast<char*>(thd->alloc(query.length() + 1));
    if (query_copy == NULL)
    {
      /* purecov: begin inspected */
      bootstrap_error= true;
      break;
      /* purecov: end */
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
      /* purecov: begin inspected */
      thd->send_statement_status();
      bootstrap_error= true;
      break;
      /* purecov: end */
    }

    // Ignore ER_TOO_LONG_KEY for system tables.
    thd->push_internal_handler(&error_handler);
    mysql_parse(thd, &parser_state);
    thd->pop_internal_handler();

    bootstrap_error= thd->is_error();
    thd->send_statement_status();

#if defined(ENABLED_PROFILING)
    thd->profiling.finish_current_query();
#endif

    if (bootstrap_error)
      break;

    free_root(thd->mem_root,MYF(MY_KEEP_PREALLOC));
    thd->get_transaction()->free_memory(MYF(MY_KEEP_PREALLOC));
  }

  Command_iterator::current_iterator->end();

  DBUG_RETURN(bootstrap_error);
}


/**
  Execute commands from bootstrap_file.

  Used when creating the initial grant tables.
*/

extern "C" {
static void *handle_bootstrap(void *arg)
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
    bootstrap_error= true;
    thd->get_protocol_classic()->end_net();
  }
  else
  {
    Global_THD_manager *thd_manager= Global_THD_manager::get_instance();
    thd_manager->add_thd(thd);

    // Set tx_read_only to false to allow installing DD tables even
    // if the server is started with --transaction-read-only=true.
    thd->variables.tx_read_only= false;
    thd->tx_read_only= false;

    if (bootstrap_handler)
      bootstrap_error= (*bootstrap_handler)(thd);
    else
      bootstrap_error= handle_bootstrap_impl(thd);

    thd->get_protocol_classic()->end_net();
    thd->release_resources();
    thd_manager->remove_thd(thd);
  }
  my_thread_end();
  return 0;
}
} // extern "C"

bool run_bootstrap_thread(MYSQL_FILE *file, bootstrap_functor boot_handler,
                          enum_thread_type thread_type)
{
  DBUG_ENTER("bootstrap");

  THD *thd= new THD;
  thd->system_thread= thread_type;
  thd->get_protocol_classic()->init_net(NULL);
  thd->security_context()->set_master_access(~(ulong)0);

  thd->set_new_thread_id();

  bootstrap_file=file;
  bootstrap_handler= boot_handler;

  // Set server default sql_mode irrespective of
  // mysqld server command line argument.
  thd->variables.sql_mode= intern_find_sys_var("sql_mode", 0)->get_default();

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
    /* purecov: begin inspected */
    sql_print_warning("Can't create thread to handle bootstrap (errno= %d)",
                      error);
    DBUG_RETURN(true);
    /* purecov: end */
  }
  /* Wait for thread to die */
  my_thread_join(&thread_handle, NULL);
  delete thd;
  DBUG_RETURN(bootstrap_error);
}
} // namespace
