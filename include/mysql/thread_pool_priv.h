/*
  Copyright (C) 2010, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef THREAD_POOL_PRIV_INCLUDED
#define THREAD_POOL_PRIV_INCLUDED

/*
  A thread pool plugins requires inclusion of sql_class.h to get proper
  access to THD variables and functions.
  There are some DTrace probes that requires definition inside the plugin,
  this requires include of probes_mysql.h.
  scheduler.h contains definitions required by the plugin.
  A thread pool can also use DEBUG_SYNC and must thus include
  debug_sync.h
  To handle definitions of Information Schema plugins it is also required
  to include sql_profile.h and table.h.

  The goal is to move all dependencies from a thread pool plugin on the
  MySQL Server into a version-handled plugin API.
*/
#define MYSQL_SERVER 1
#include <sql_class.h>
#include <debug_sync.h>
#include <sql_profile.h>
#include <table.h>

/* Needed to get access to scheduler variables */
void* thd_get_scheduler_data(THD *thd);
void thd_set_scheduler_data(THD *thd, void *data);
PSI_thread* thd_get_psi(THD *thd);
void thd_set_psi(THD *thd, PSI_thread *psi);

/* Interface to THD variables and functions */
void thd_set_killed(THD *thd);
void thd_clear_errors(THD *thd);
void thd_set_thread_stack(THD *thd, char *stack_start);
void thd_lock_connection_data(THD *thd);
void thd_unlock_connection_data(THD *thd);
void thd_close_connection(THD *thd);
THD *thd_get_current_thd();
void thd_new_connection_setup(THD *thd, char *stack_start);
void thd_lock_data(THD *thd);
void thd_unlock_data(THD *thd);
bool thd_is_transaction_active(THD *thd);
int thd_connection_has_data(THD *thd);
void thd_set_net_read_write(THD *thd, uint val);
void thd_set_mysys_var(THD *thd, st_my_thread_var *mysys_var);

/*
  The thread pool must be able to execute commands using the connection
  state in THD object. This is the main objective of the thread pool to
  schedule the start of these commands.
*/
bool do_command(THD *thd);

/*
  The thread pool requires an interface to the connection logic in the
  MySQL Server since the thread pool will maintain the event logic on
  the TCP connection of the MySQL Server. Thus new connections, dropped
  connections will be discovered by the thread pool and it needs to
  ensure that the proper MySQL Server logic attached to these events is
  executed.
*/
bool thd_prepare_connection(THD *thd);
bool thd_is_connection_alive(THD *thd);
void end_connection(THD *thd);
void mysql_audit_release(THD *thd);
bool setup_connection_thread_globals(THD *thd);
bool init_new_connection_handler_thread();

/*
  thread_created is maintained by thread pool when activated since
  user threads are created by the thread pool (and also special
  threads to maintain the thread pool).
  max_connections is needed to calculate the maximum number of threads
  that is allowed to be started by the thread pool.
*/
extern MYSQL_PLUGIN_IMPORT ulong thread_created;
extern MYSQL_PLUGIN_IMPORT ulong max_connections;
extern MYSQL_PLUGIN_IMPORT mysql_cond_t COND_thread_count;
extern MYSQL_PLUGIN_IMPORT pthread_attr_t connection_attrib;
/* extern MYSQL_PLUGIN_IMPORT I_List<THD> threads; */
extern MYSQL_PLUGIN_IMPORT PSI_thread_key key_thread_one_connection;
#endif
