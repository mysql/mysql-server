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


#if defined(__GNUC__) && defined(USE_PRAGMA_IMPLEMENTATION)
#pragma implementation
#endif

#include "guardian.h"
#include <string.h>
#include <sys/types.h>
#include <signal.h>

#include "instance.h"
#include "instance_map.h"
#include "log.h"
#include "mysql_manager_error.h"

const char *
Guardian::get_instance_state_name(enum_instance_state state)
{
  switch (state) {
  case NOT_STARTED:
    return "offline";

  case STARTING:
    return "starting";

  case STARTED:
    return "online";

  case JUST_CRASHED:
    return "failed";

  case CRASHED:
    return "crashed";

  case CRASHED_AND_ABANDONED:
    return "abandoned";

  case STOPPING:
    return "stopping";
  }

  return NULL; /* just to ignore compiler warning. */
}

/* {{{ Constructor & destructor. */

Guardian::Guardian(Thread_registry *thread_registry_arg,
                   Instance_map *instance_map_arg,
                   uint monitoring_interval_arg)
  :stopped(FALSE),
  monitoring_interval(monitoring_interval_arg),
  thread_registry(thread_registry_arg),
  instance_map(instance_map_arg),
  guarded_instances(0),
  shutdown_requested(FALSE)
{
  pthread_mutex_init(&LOCK_guardian, 0);
  pthread_cond_init(&COND_guardian, 0);
  init_alloc_root(&alloc, MEM_ROOT_BLOCK_SIZE, 0);
}


Guardian::~Guardian()
{
  /* delay guardian destruction to the moment when no one needs it */
  pthread_mutex_lock(&LOCK_guardian);
  free_root(&alloc, MYF(0));
  pthread_mutex_unlock(&LOCK_guardian);
  pthread_mutex_destroy(&LOCK_guardian);
  pthread_cond_destroy(&COND_guardian);
}

/* }}} */


void Guardian::request_shutdown()
{
  pthread_mutex_lock(&LOCK_guardian);
  /* STOP Instances or just clean up Guardian repository */
  stop_instances();
  shutdown_requested= TRUE;
  pthread_mutex_unlock(&LOCK_guardian);
}


void Guardian::process_instance(Instance *instance,
                                GUARD_NODE *current_node,
                                LIST **guarded_instances,
                                LIST *node)
{
  uint waitchild= (uint) Instance::DEFAULT_SHUTDOWN_DELAY;
  /* The amount of times, Guardian attempts to restart an instance */
  int restart_retry= 100;
  time_t current_time= time(NULL);

  if (current_node->state == STOPPING)
  {
    waitchild= instance->options.get_shutdown_delay();

    /* this returns TRUE if and only if an instance was stopped for sure */
    if (instance->is_crashed())
      *guarded_instances= list_delete(*guarded_instances, node);
    else if ( (uint) (current_time - current_node->last_checked) > waitchild)
    {
      instance->kill_mysqld(SIGKILL);
      /*
        Later we do node= node->next. This is ok, as we are only removing
        the node from the list. The pointer to the next one is still valid.
      */
      *guarded_instances= list_delete(*guarded_instances, node);
    }

    return;
  }

  if (instance->is_mysqld_running())
  {
    /* The instance can be contacted  on it's port */

    /* If STARTING also check that pidfile has been created */
    if (current_node->state == STARTING &&
        current_node->instance->options.load_pid() == 0)
    {
      /* Pid file not created yet, don't go to STARTED state yet  */
    }
    else if (current_node->state != STARTED)
    {
      /* clear status fields */
      log_info("Guardian: '%s' is running, set state to STARTED.",
               (const char *) instance->options.instance_name.str);
      current_node->restart_counter= 0;
      current_node->crash_moment= 0;
      current_node->state= STARTED;
    }
  }
  else
  {
    switch (current_node->state) {
    case NOT_STARTED:
      log_info("Guardian: starting '%s'...",
               (const char *) instance->options.instance_name.str);

      /* NOTE, set state to STARTING _before_ start() is called */
      current_node->state= STARTING;
      instance->start();
      current_node->last_checked= current_time;
      break;
    case STARTED:     /* fallthrough */
    case STARTING:    /* let the instance start or crash */
      if (instance->is_crashed())
      {
        current_node->crash_moment= current_time;
        current_node->last_checked= current_time;
        current_node->state= JUST_CRASHED;
        /* fallthrough -- restart an instance immediately */
      }
      else
        break;
    case JUST_CRASHED:
      if (current_time - current_node->crash_moment <= 2)
      {
        if (instance->is_crashed())
        {
          instance->start();
          log_info("Guardian: starting '%s'...",
                   (const char *) instance->options.instance_name.str);
        }
      }
      else
        current_node->state= CRASHED;
      break;
    case CRASHED:    /* just regular restarts */
      if (current_time - current_node->last_checked >
          monitoring_interval)
      {
        if ((current_node->restart_counter < restart_retry))
        {
          if (instance->is_crashed())
          {
            instance->start();
            current_node->last_checked= current_time;
            current_node->restart_counter++;
            log_info("Guardian: restarting '%s'...",
                     (const char *) instance->options.instance_name.str);
          }
        }
        else
        {
          log_info("Guardian: can not start '%s'. "
                   "Abandoning attempts to (re)start it",
                   (const char *) instance->options.instance_name.str);
          current_node->state= CRASHED_AND_ABANDONED;
        }
      }
      break;
    case CRASHED_AND_ABANDONED:
      break; /* do nothing */
    default:
      DBUG_ASSERT(0);
    }
  }
}


/*
  Main function of Guardian thread.

  SYNOPSIS
    run()

  DESCRIPTION
    Check for all guarded instances and restart them if needed. If everything
    is fine go and sleep for some time.
*/

void Guardian::run()
{
  Instance *instance;
  LIST *node;
  struct timespec timeout;

  log_info("Guardian: started.");

  thread_registry->register_thread(&thread_info);

  pthread_mutex_lock(&LOCK_guardian);

  /* loop, until all instances were shut down at the end */
  while (!(shutdown_requested && (guarded_instances == NULL)))
  {
    node= guarded_instances;

    while (node != NULL)
    {
      GUARD_NODE *current_node= (GUARD_NODE *) node->data;
      instance= ((GUARD_NODE *) node->data)->instance;
      process_instance(instance, current_node, &guarded_instances, node);

      node= node->next;
    }
    set_timespec(timeout, monitoring_interval);
    
    /* check the loop predicate before sleeping */
    if (!(shutdown_requested && (!(guarded_instances))))
      thread_registry->cond_timedwait(&thread_info, &COND_guardian,
                                      &LOCK_guardian, &timeout);
  }

  log_info("Guardian: stopped.");

  stopped= TRUE;
  pthread_mutex_unlock(&LOCK_guardian);
  /* now, when the Guardian is stopped we can stop the IM */
  thread_registry->unregister_thread(&thread_info);
  thread_registry->request_shutdown();

  log_info("Guardian: finished.");
}


int Guardian::is_stopped()
{
  int var;
  pthread_mutex_lock(&LOCK_guardian);
  var= stopped;
  pthread_mutex_unlock(&LOCK_guardian);
  return var;
}


/*
  Initialize the list of guarded instances: loop through the Instance_map and
  add all of the instances, which don't have 'nonguarded' option specified.

  SYNOPSIS
    Guardian::init()

  NOTE: The operation should be invoked with the following locks acquired:
    - Guardian;
    - Instance_map;

  RETURN
    0 - ok
    1 - error occurred
*/

int Guardian::init()
{
  Instance *instance;
  Instance_map::Iterator iterator(instance_map);

  /* clear the list of guarded instances */
  free_root(&alloc, MYF(0));
  init_alloc_root(&alloc, MEM_ROOT_BLOCK_SIZE, 0);
  guarded_instances= NULL;

  while ((instance= iterator.next()))
  {
    if (instance->options.nonguarded)
      continue;

    if (guard(instance, TRUE))                /* do not lock guardian */
      return 1;
  }

  return 0;
}


/*
  Add instance to the Guardian list

  SYNOPSIS
    guard()
    instance           the instance to be guarded
    nolock             whether we prefer do not lock Guardian here,
                       but use external locking instead

  DESCRIPTION

    The instance is added to the guarded instances list. Usually guard() is
    called after we start an instance.

  RETURN
    0 - ok
    1 - error occurred
*/

int Guardian::guard(Instance *instance, bool nolock)
{
  LIST *node;
  GUARD_NODE *content;

  node= (LIST *) alloc_root(&alloc, sizeof(LIST));
  content= (GUARD_NODE *) alloc_root(&alloc, sizeof(GUARD_NODE));

  if ((!(node)) || (!(content)))
    return 1;
  /* we store the pointers to instances from the instance_map's MEM_ROOT */
  content->instance= instance;
  content->restart_counter= 0;
  content->crash_moment= 0;
  content->state= NOT_STARTED;
  node->data= (void*) content;

  if (nolock)
    guarded_instances= list_add(guarded_instances, node);
  else
  {
    pthread_mutex_lock(&LOCK_guardian);
    guarded_instances= list_add(guarded_instances, node);
    pthread_mutex_unlock(&LOCK_guardian);
  }

  return 0;
}


/*
  TODO: perhaps it would make sense to create a pool of the LIST nodeents
  and give them upon request. Now we are loosing a bit of memory when
  guarded instance was stopped and then restarted (since we cannot free just
  a piece of the MEM_ROOT).
*/

int Guardian::stop_guard(Instance *instance)
{
  LIST *node;

  pthread_mutex_lock(&LOCK_guardian);

  node= find_instance_node(instance);

  if (node != NULL)
    guarded_instances= list_delete(guarded_instances, node);

  pthread_mutex_unlock(&LOCK_guardian);

  /* if there is nothing to delete it is also fine */
  return 0;
}

/*
  An internal method which is called at shutdown to unregister instances and
  attempt to stop them if requested.

  SYNOPSIS
    stop_instances()

  DESCRIPTION
    Loops through the guarded_instances list and prepares them for shutdown.
    For each instance we issue a stop command and change the state
    accordingly.

  NOTE
    Guardian object should be locked by the calling function.

  RETURN
    0 - ok
    1 - error occurred
*/

int Guardian::stop_instances()
{
  LIST *node;
  node= guarded_instances;
  while (node != NULL)
  {
    GUARD_NODE *current_node= (GUARD_NODE *) node->data;
    /*
      If instance is running or was running (and now probably hanging),
      request stop.
    */
    if (current_node->instance->is_mysqld_running() ||
        (current_node->state == STARTED))
    {
      current_node->state= STOPPING;
      current_node->last_checked= time(NULL);
    }
    else
      /* otherwise remove it from the list */
      guarded_instances= list_delete(guarded_instances, node);
    /* But try to kill it anyway. Just in case */
    current_node->instance->kill_mysqld(SIGTERM);
    node= node->next;
  }
  return 0;
}


void Guardian::lock()
{
  pthread_mutex_lock(&LOCK_guardian);
}


void Guardian::unlock()
{
  pthread_mutex_unlock(&LOCK_guardian);
}


LIST *Guardian::find_instance_node(Instance *instance)
{
  LIST *node= guarded_instances;

  while (node != NULL)
  {
    /*
      We compare only pointers, as we always use pointers from the
      instance_map's MEM_ROOT.
    */
    if (((GUARD_NODE *) node->data)->instance == instance)
      return node;

    node= node->next;
  }

  return NULL;
}


bool Guardian::is_active(Instance *instance)
{
  bool guarded;

  lock();

  guarded= find_instance_node(instance) != NULL;

  /* is_running() can take a long time, so let's unlock mutex first. */
  unlock();

  if (guarded)
    return true;

  return instance->is_mysqld_running();
}
