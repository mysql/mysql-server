/* Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include "sql_bootstrap.h"       // MAX_BOOTSTRAP_QUERY_SIZE
#include "sql_class.h"           // THD
#include "sql_connect.h"         // close_connection
#include "sql_parse.h"           // mysql_parse

#include "pfs_file_provider.h"
#include "mysql/psi/mysql_file.h"

static MYSQL_FILE *bootstrap_file= NULL;
static int bootstrap_error= 0;


static char *fgets_fn(char *buffer, size_t size, MYSQL_FILE* input, int *error)
{
  char *line= mysql_file_fgets(buffer, static_cast<int>(size), input);
  if (error)
    *error= (line == NULL) ? ferror(input->m_file) : 0;
  return line;
}


static void handle_bootstrap_impl(THD *thd)
{
  MYSQL_FILE *file= bootstrap_file;
  char buffer[MAX_BOOTSTRAP_QUERY_SIZE];

  DBUG_ENTER("handle_bootstrap");

  thd->thread_stack= (char*) &thd;
  thd->security_context()->assign_user(STRING_WITH_LEN("boot"));
  thd->security_context()->assign_priv_user("", 0);
  thd->security_context()->assign_priv_host("", 0);
  /*
    Make the "client" handle multiple results. This is necessary
    to enable stored procedures with SELECTs and Dynamic SQL
    in init-file.
  */
  thd->client_capabilities|= CLIENT_MULTI_RESULTS;

  thd->init_for_queries();

  buffer[0]= '\0';

  for ( ; ; )
  {
    size_t length;
    int error= 0;
    int rc= read_bootstrap_query(buffer, &length, file, fgets_fn, &error);

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
      char *err_ptr= buffer + (length <= MAX_BOOTSTRAP_ERROR_LEN ?
                                        0 : (length - MAX_BOOTSTRAP_ERROR_LEN));
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

      thd->protocol->end_statement();
      bootstrap_error= 1;
      break;
    }

    char *query= static_cast<char*>(thd->alloc(length + 1));
    if (query == NULL)
    {
      bootstrap_error= 1;
      break;
    }
    memcpy(query, buffer, length);
    query[length]= '\0';
    thd->set_query(query, length);
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
      thd->protocol->end_statement();
      bootstrap_error= 1;
      break;
    }

    mysql_parse(thd, &parser_state);

    bootstrap_error= thd->is_error();
    thd->protocol->end_statement();

#if defined(ENABLED_PROFILING)
    thd->profiling.finish_current_query();
#endif

    if (bootstrap_error)
      break;

    free_root(thd->mem_root,MYF(MY_KEEP_PREALLOC));
    thd->get_transaction()->free_memory(MYF(MY_KEEP_PREALLOC));
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
    net_end(&thd->net);
  }
  else
  {
    Global_THD_manager *thd_manager= Global_THD_manager::get_instance();
    thd_manager->add_thd(thd);

    handle_bootstrap_impl(thd);

    net_end(&thd->net);
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
  my_net_init(&thd->net,(st_vio*) 0);
  thd->max_client_packet_length= thd->net.max_packet;
  thd->security_context()->set_master_access(~(ulong)0);

  thd->set_new_thread_id();

  bootstrap_file=file;

  my_thread_attr_t thr_attr;
  my_thread_attr_init(&thr_attr);
#ifndef _WIN32
  pthread_attr_setscope(&thr_attr, PTHREAD_SCOPE_SYSTEM);
  pthread_attr_setdetachstate(&thr_attr, PTHREAD_CREATE_JOINABLE);
#endif
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
