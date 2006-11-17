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

/**
  The guardian thread is responsible for monitoring and restarting of guarded
  instances.
*/

class Guardian: public Thread
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

  /* Return client state name. */
  static const char *get_instance_state_name(enum_instance_state state);

  Guardian(Thread_registry *thread_registry_arg,
           Instance_map *instance_map_arg,
           uint monitoring_interval_arg);
  virtual ~Guardian();
  /* Initialize or refresh the list of guarded instances */
  int init();
  /* Request guardian shutdown. Stop instances if needed */
  void request_shutdown();
  /* Start instance protection */
  int guard(Instance *instance, bool nolock= FALSE);
  /* Stop instance protection */
  int stop_guard(Instance *instance);
  /* Returns TRUE if guardian thread is stopped */
  int is_stopped();
  void lock();
  void unlock();

  /*
    Return an internal list node for the given instance if the instance is
    managed by Guardian. Otherwise, return NULL.

    MT-NOTE: must be called under acquired lock.
  */
  LIST *find_instance_node(Instance *instance);

  /* The operation is used to check if the instance is active or not. */
  bool is_active(Instance *instance);

  /*
    Return state of the given instance list node. The pointer must specify
    a valid list node.
  */
  inline enum_instance_state get_instance_state(LIST *instance_node);
protected:
  /* Main funtion of the thread */
  virtual void run();

public:
  pthread_cond_t COND_guardian;

private:
  /* Prepares Guardian shutdown. Stops instances is needed */
  int stop_instances();
  /* check instance state and act accordingly */
  void process_instance(Instance *instance, GUARD_NODE *current_node,
                        LIST **guarded_instances, LIST *elem);

  int stopped;

private:
  pthread_mutex_t LOCK_guardian;
  Thread_info thread_info;
  int monitoring_interval;
  Thread_registry *thread_registry;
  Instance_map *instance_map;
  LIST *guarded_instances;
  MEM_ROOT alloc;
  /* this variable is set to TRUE when we want to stop Guardian thread */
  bool shutdown_requested;
};


inline Guardian::enum_instance_state
Guardian::get_instance_state(LIST *instance_node)
{
  return ((GUARD_NODE *) instance_node->data)->state;
}

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_GUARDIAN_H */
