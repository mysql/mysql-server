/* Copyright (C) 2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Classes for the thread scheduler
*/

#ifdef USE_PRAGMA_INTERFACE
#pragma interface
#endif

class THD;

/* Functions used when manipulating threads */

class scheduler_functions
{
public:
  uint max_threads;
  bool (*init)(void);
  bool (*init_new_connection_thread)(void);
  void (*add_connection)(THD *thd);
  void (*post_kill_notification)(THD *thd);
  bool (*end_thread)(THD *thd, bool cache_thread);
  void (*end)(void);
  scheduler_functions();
};

enum scheduler_types
{
  SCHEDULER_ONE_THREAD_PER_CONNECTION=0,
  SCHEDULER_NO_THREADS,
  SCHEDULER_POOL_OF_THREADS
};

void one_thread_per_connection_scheduler(scheduler_functions* func);
void one_thread_scheduler(scheduler_functions* func);

enum pool_command_op
{
  NOT_IN_USE_OP= 0, NORMAL_OP= 1, CONNECT_OP, KILL_OP, DIE_OP
};

#define HAVE_POOL_OF_THREADS 0                  /* For easyer tests */
#define pool_of_threads_scheduler(A) one_thread_per_connection_scheduler(A)

class thd_scheduler
{};
