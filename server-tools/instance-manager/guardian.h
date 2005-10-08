#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_GUARDIAN_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_GUARDIAN_H
/* Copyright (C) 2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <my_global.h>
#include "thread_registry.h"

#include <my_sys.h>
#include <my_list.h>

#if defined(__GNUC__) && defined(USE_PRAGMA_INTERFACE)
#pragma interface
#endif

class Instance;
class Instance_map;
class Thread_registry;
struct GUARD_NODE;

pthread_handler_t guardian(void *arg);

struct Guardian_thread_args
{
  Thread_registry &thread_registry;
  Instance_map *instance_map;
  int monitoring_interval;

  Guardian_thread_args(Thread_registry &thread_registry_arg,
                       Instance_map *instance_map_arg,
                       uint monitoring_interval_arg) :
    thread_registry(thread_registry_arg),
    instance_map(instance_map_arg),
    monitoring_interval(monitoring_interval_arg)
  {}
};


/*
  The guardian thread is responsible for monitoring and restarting of guarded
  instances.
*/

class Guardian_thread: public Guardian_thread_args
{
public:
  /* states of an instance */
  enum enum_instance_state { NOT_STARTED= 1, STARTING, STARTED, JUST_CRASHED,
                             CRASHED, CRASHED_AND_ABANDONED, STOPPING };

  /*
    The Guardian list node structure. Guardian utilizes it to store
    guarded instances plus some additional info.
  */

  struct GUARD_NODE
  {
    Instance *instance;
    /* state of an instance (i.e. STARTED, CRASHED, etc.) */
    enum_instance_state state;
    /* the amount of attemts to restart instance (cleaned up at success) */
    int restart_counter;
    /* triggered at a crash */
    time_t crash_moment;
    /* General time field. Used to provide timeouts (at shutdown and restart) */
    time_t last_checked;
  };


  Guardian_thread(Thread_registry &thread_registry_arg,
                  Instance_map *instance_map_arg,
                  uint monitoring_interval_arg);
  ~Guardian_thread();
  /* Main funtion of the thread */
  void run();
  /* Initialize or refresh the list of guarded instances */
  int init();
  /* Request guardian shutdown. Stop instances if needed */
  void request_shutdown(bool stop_instances);
  /* Start instance protection */
  int guard(Instance *instance, bool nolock= FALSE);
  /* Stop instance protection */
  int stop_guard(Instance *instance);
  /* Returns true if guardian thread is stopped */
  int is_stopped();
  void lock();
  void unlock();

public:
  pthread_cond_t COND_guardian;

private:
  /* Prepares Guardian shutdown. Stops instances is needed */
  int stop_instances(bool stop_instances_arg);
  /* check instance state and act accordingly */
  void process_instance(Instance *instance, GUARD_NODE *current_node,
                        LIST **guarded_instances, LIST *elem);
  int stopped;

private:
  pthread_mutex_t LOCK_guardian;
  Thread_info thread_info;
  LIST *guarded_instances;
  MEM_ROOT alloc;
  enum { MEM_ROOT_BLOCK_SIZE= 512 };
  /* this variable is set to TRUE when we want to stop Guardian thread */
  bool shutdown_requested;
};

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_GUARDIAN_H */
