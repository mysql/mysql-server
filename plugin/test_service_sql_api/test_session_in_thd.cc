/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <ctype.h>
#include <fcntl.h>
#include <mysql/plugin.h>
#include <mysql_version.h>
#include <stdlib.h>

#include "m_string.h"                           // strlen
#include "my_dbug.h"
#include "my_dir.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_sys.h"                             // my_write, my_malloc
#include "sql/sql_plugin.h"                     // st_plugin_int

static const char *log_filename= "test_session_in_thd";

#define MAX_SESSIONS 500

#define STRING_BUFFER_SIZE 512

#define WRITE_STR(format) my_snprintf(buffer,sizeof(buffer),format); \
                                      my_write(outfile,(uchar*)buffer,strlen(buffer),MYF(0))
#define WRITE_VAL(format,value) my_snprintf(buffer,sizeof(buffer),format,value); \
                                            my_write(outfile,(uchar*)buffer,strlen(buffer),MYF(0))
static const char *sep = "============================================================================================\n";

#define WRITE_SEP() my_write(outfile, (uchar*)sep, strlen(sep), MYF(0))

static File outfile;

struct test_services_context
{
  my_thread_handle test_services_thread;
};

/* SQL (system) variable to control number of sessions                    */
/* Only effective at start of mysqld by setting it as option --loose-...  */
int nb_sessions;
static MYSQL_SYSVAR_INT (nb_sessions, nb_sessions, PLUGIN_VAR_RQCMDARG,
		        "number of sessions", NULL, NULL, 1, 1, 500, 0);

static struct st_mysql_sys_var *test_services_sysvars[]= {
  MYSQL_SYSVAR(nb_sessions),
  NULL
};

static void test_session(void *p)
{
  char buffer[STRING_BUFFER_SIZE];
  DBUG_ENTER("test_session");

  MYSQL_SESSION sessions[MAX_SESSIONS];
  bool session_ret= false;
  void *plugin_ctx= p;

  /* Open session 1: Must pass */  
  for (int i= 0; i < nb_sessions; i++)
  {
    WRITE_VAL("sql open session %d.\n", i);
    sessions[i]= srv_session_open(NULL, plugin_ctx);
    if (!sessions[i])
      my_plugin_log_message(&p, MY_ERROR_LEVEL, "srv_session_open_%d failed.", i);
  }

  /* close session 1: Must pass i*/
  WRITE_VAL("close following nb of sessions: %d\n",nb_sessions);
  for (int i= 0; i < nb_sessions; i++)
  {
    WRITE_VAL("sql session close session %d.\n", nb_sessions-1-i);
    session_ret= srv_session_close(sessions[nb_sessions-1-i]);
    if (session_ret)
      my_plugin_log_message(&p, MY_ERROR_LEVEL, "srv_session_close_%d failed.", nb_sessions-1-i);
  }
  
  /* Open session 1: Must pass */  
  for (int i= 0; i < nb_sessions; i++)
  {
    WRITE_VAL("sql open session %d.\n", i);
    sessions[i]= srv_session_open(NULL, plugin_ctx);
    if (!sessions[i])
      my_plugin_log_message(&p, MY_ERROR_LEVEL, "srv_session_open_%d failed.", i);
  }

  /* close session 1: Must pass */
  WRITE_VAL("close following nb of sessions: %d\n",nb_sessions);
  for (int i= 0; i < nb_sessions; i++)
  {
    WRITE_VAL("sql session close session %d.\n", i);
    session_ret= srv_session_close(sessions[i]);
    if (session_ret)
      my_plugin_log_message(&p, MY_ERROR_LEVEL, "srv_session_close_%d failed.", i);
  }
  
  DBUG_VOID_RETURN;
}


struct test_thread_context
{
  my_thread_handle thread;
  void *p;
  bool thread_finished;
  void (*test_function)(void *);
};


static void* test_sql_threaded_wrapper(void *param)
{
  char buffer[STRING_BUFFER_SIZE];
  struct test_thread_context *context= (struct test_thread_context*) param;

  WRITE_SEP();
  WRITE_STR("init thread\n");
  if (srv_session_init_thread(context->p))
    my_plugin_log_message(&context->p, MY_ERROR_LEVEL, "srv_session_init_thread failed.");

  context->test_function(context->p);

  WRITE_STR("deinit thread\n");
  srv_session_deinit_thread();

  context->thread_finished= true;
  return NULL;
}


static void create_log_file(const char * log_name)
{
  char filename[FN_REFLEN];

  fn_format(filename, log_name, "", ".log",
            MY_REPLACE_EXT | MY_UNPACK_FILENAME);
  unlink(filename);
  outfile= my_open(filename, O_CREAT|O_RDWR, MYF(0));
}


static void test_in_spawned_thread(void *p, void (*test_function)(void *))
{
  my_thread_attr_t attr;          /* Thread attributes */
  my_thread_attr_init(&attr);
  (void) my_thread_attr_setdetachstate(&attr, MY_THREAD_CREATE_JOINABLE);

  struct test_thread_context context;

  context.p= p;
  context.thread_finished= false;
  context.test_function= test_function;

  /* now create the thread and call test_session within the thread. */
  if (my_thread_create(&(context.thread), &attr, test_sql_threaded_wrapper, &context) != 0)
    my_plugin_log_message(&p, MY_ERROR_LEVEL, "Could not create test session thread");
  else
    my_thread_join(&context.thread, NULL);
}

static int test_sql_service_plugin_init(void *p)
{
  char buffer[STRING_BUFFER_SIZE];
  DBUG_ENTER("test_sql_service_plugin_init");
  my_plugin_log_message(&p, MY_INFORMATION_LEVEL, "Installation.");

  create_log_file(log_filename);

  WRITE_STR("Follows threaded run\n");
  test_in_spawned_thread(p, test_session);

  my_close(outfile, MYF(0));

  DBUG_RETURN(0);
}


static int test_sql_service_plugin_deinit(void*)
{
  DBUG_ENTER("test_sql_service_plugin_deinit");
  DBUG_RETURN(0);
}


struct st_mysql_daemon test_session_service_plugin=
{ MYSQL_DAEMON_INTERFACE_VERSION };


/*
  Plugin library descriptor
*/

mysql_declare_plugin(test_daemon)
{
  MYSQL_DAEMON_PLUGIN,
  &test_session_service_plugin,
  "test_session_in_thd",
  "Horst Hunger, Andrey Hristov",
  "Test sessions in thread",
  PLUGIN_LICENSE_GPL,
  test_sql_service_plugin_init,   /* Plugin Init      */
  NULL, /* Plugin Check uninstall    */
  test_sql_service_plugin_deinit, /* Plugin Deinit    */
  0x0100,                             /* 1.0              */
  NULL,                               /* status variables */
  test_services_sysvars,              /* system variables */
  NULL,                               /* config options   */
  0,                                  /* flags            */
}
mysql_declare_plugin_end;
