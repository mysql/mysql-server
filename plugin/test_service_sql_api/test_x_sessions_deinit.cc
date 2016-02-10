/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <stdlib.h>
#include <my_global.h>
#include "my_sys.h"                             // my_write, my_malloc
#include <mysql/plugin.h>

static const char *log_filename= "test_x_sessions_deinit";

#define MAX_SESSIONS 128

#define STRING_BUFFER_SIZE 512

#define WRITE_STR(format) my_snprintf(buffer,sizeof(buffer),format); \
	                  my_write(outfile,(uchar*)buffer,strlen(buffer),MYF(0))

#define WRITE_VAL(format,value) my_snprintf(buffer,sizeof(buffer),format,value); \
	                  my_write(outfile,(uchar*)buffer,strlen(buffer),MYF(0))


static const char *sep = "========================================================================\n";

#define WRITE_SEP() my_write(outfile, (uchar*)sep, strlen(sep), MYF(0))

/* SQL (system) variable to control number of sessions                    */
/* Only effective at start od mysqld by setting it as option --loose-...  */
int nb_sessions;
static MYSQL_SYSVAR_INT  (nb_sessions, nb_sessions, PLUGIN_VAR_RQCMDARG,
                          "number of sessions", NULL, NULL, 1, 1, 500, 0);

static struct st_mysql_sys_var *test_services_sysvars[]= {
  MYSQL_SYSVAR(nb_sessions),
  NULL
};

static File outfile;


static void test_session_open(void *p)
{
  char buffer[STRING_BUFFER_SIZE];
  DBUG_ENTER("test_session_open");

  MYSQL_SESSION sessions[MAX_SESSIONS];
  void *plugin_ctx= NULL;

  WRITE_VAL("nb_sessions = %d\n", nb_sessions);
  /* Open sessions: Must pass */
  for (int i= 0; i < nb_sessions; i++)
  {
    WRITE_VAL("srv_session_open %d - ", i+1);
    sessions[i]= srv_session_open(NULL, plugin_ctx);
    if (!sessions[i])
    {
      WRITE_STR("Failed\n");
    }
    else
    {
      WRITE_STR("Success\n");
    }
  }

  DBUG_VOID_RETURN;
}


static void test_session(void *p)
{
  char buffer[STRING_BUFFER_SIZE];
  DBUG_ENTER("test_session");
  
  MYSQL_SESSION sessions[MAX_SESSIONS];
  void *plugin_ctx= NULL;
  bool session_ret= false;

  WRITE_VAL("nb_sessions = %d\n", nb_sessions);
  /* Open sessions: Must pass */  
  for (int i= 0; i < nb_sessions; i++)
  {
    WRITE_VAL("srv_session_open %d - ", i+1);
    sessions[i]= srv_session_open(NULL, plugin_ctx);
    if (!sessions[i])
    {
      WRITE_STR("Failed\n");
    }
    else
    {
      WRITE_STR("Success\n");
    }
  }

  /*  close sessions: Must pass */    
  for (int i= 0; i < nb_sessions; i++)
  {
    WRITE_VAL("srv_session_close %d - ", nb_sessions-1-i+1); //1 indexed
    session_ret= srv_session_close(sessions[nb_sessions-1-i]);
    if (session_ret)
    {
      WRITE_STR("Failed\n");
    }
    else
    {
      WRITE_STR("Success\n");
    }
  }
  
  /*  close sessions: Fail as not opened */    
  for (int i= 0; i < nb_sessions; i++)
  {
    WRITE_VAL("srv_session_close %d - ", nb_sessions-1-i+1); //1 indexed
    session_ret= srv_session_close(sessions[nb_sessions-1-i]);
    if (session_ret)
    {
      WRITE_STR("Failed\n");
    }
    else
    {
      WRITE_STR("Success\n");
    }
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


static int test_session_service_plugin_init(void *p)
{
  DBUG_ENTER("test_session_service_plugin_init");
  my_plugin_log_message(&p, MY_INFORMATION_LEVEL, "Installation.");
  DBUG_RETURN(0);
}


static int test_session_service_plugin_deinit(void *p)
{
  char buffer[STRING_BUFFER_SIZE];
  DBUG_ENTER("test_session_service_plugin_deinit");
  my_plugin_log_message(&p, MY_INFORMATION_LEVEL, "Uninstallation.");

  create_log_file(log_filename);

  WRITE_SEP();
  WRITE_STR("Test in a server thread\n");
  test_session(p);

  /* Test in a new thread */
  WRITE_STR("Follows threaded run\n");
  test_in_spawned_thread(p, test_session);

  WRITE_STR("Follows threaded run and leaves open session (Bug#21966621)\n");
  // Bug#21966621 - Leave session open at srv thread exit which is doing the release in following way:
  //                1) Lock LOCK_collection while doing release callback for every session
  //                2) Second attempt of LOCK_collection fails while removing session by the callback.
  //                While exiting both function LOCK_collection is left in invalid state because
  //                it was released 2 times, lock 1 time
  //                3) at next attempt of LOCK_collection usage causes deadlock
  test_in_spawned_thread(p, test_session_open); // step 1, 2
  test_in_spawned_thread(p, test_session);      // step 3

  WRITE_STR("Follows threaded run and leaves open session (Bug#21983102)\n");
  // Bug#21983102 - iterates through sessions list in which elements are removed
  //                thus the iterator becomes invalid causing random crashes and
  //                hangings
  test_in_spawned_thread(p, test_session_open);

  my_close(outfile, MYF(0));

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
  "test_x_sessions_deinit",
  "Horst Hunger, Andrey Hristov",
  "Test session service in deinit",
  PLUGIN_LICENSE_GPL,
  test_session_service_plugin_init,   /* Plugin Init      */
  test_session_service_plugin_deinit, /* Plugin Deinit    */
  0x0100,                             /* 1.0              */
  NULL,                               /* status variables */
  test_services_sysvars,              /* system variables */
  NULL,                               /* config options   */
  0,                                  /* flags            */
}
mysql_declare_plugin_end;
