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
bool login_connection(THD *thd);
void prepare_new_connection_state(THD* thd);
void end_connection(THD *thd);
bool setup_connection_thread_globals(THD *thd);
bool init_new_connection_handler_thread();

/*
  thread_created is maintained by thread pool when activated since
  user threads are created by the thread pool (and also special
  threads to maintain the thread pool).
  max_connections is needed to calculate the maximum number of threads
  that is allowed to be started by the thread pool.
*/
extern ulong thread_created, max_connections;
#endif
