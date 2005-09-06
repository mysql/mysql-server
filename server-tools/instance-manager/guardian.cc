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


#ifdef __GNUC__
#pragma implementation
#endif

#include "guardian.h"

#include "instance_map.h"
#include "instance.h"
#include "mysql_manager_error.h"
#include "log.h"
#include "portability.h"

#include <string.h>
#include <sys/types.h>
#include <signal.h>



C_MODE_START

pthread_handler_decl(guardian, arg)
{
  Guardian_thread *guardian_thread= (Guardian_thread *) arg;
  guardian_thread->run();
  return 0;
}

C_MODE_END


Guardian_thread::Guardian_thread(Thread_registry &thread_registry_arg,
                                 Instance_map *instance_map_arg,
                                 uint monitoring_interval_arg) :
  Guardian_thread_args(thread_registry_arg, instance_map_arg,
                       monitoring_interval_arg),
  thread_info(pthread_self()), guarded_instances(0)
{
  pthread_mutex_init(&LOCK_guardian, 0);
  pthread_cond_init(&COND_guardian, 0);
  shutdown_requested= FALSE;
  stopped= FALSE;
  init_alloc_root(&alloc, MEM_ROOT_BLOCK_SIZE, 0);
}


Guardian_thread::~Guardian_thread()
{
  /* delay guardian destruction to the moment when no one needs it */
  pthread_mutex_lock(&LOCK_guardian);
  free_root(&alloc, MYF(0));
  pthread_mutex_unlock(&LOCK_guardian);
  pthread_mutex_destroy(&LOCK_guardian);
  pthread_cond_destroy(&COND_guardian);
}


void Guardian_thread::request_shutdown(bool stop_instances_arg)
{
  pthread_mutex_lock(&LOCK_guardian);
  /* stop instances or just clean up Guardian repository */
  stop_instances(stop_instances_arg);
  shutdown_requested= TRUE;
  pthread_mutex_unlock(&LOCK_guardian);
}


void Guardian_thread::process_instance(Instance *instance,
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
    /* this brach is executed during shutdown */
    if (instance->options.shutdown_delay_val)
      waitchild= instance->options.shutdown_delay_val;

    /* this returns true if and only if an instance was stopped for sure */
    if (instance->is_crashed())
      *guarded_instances= list_delete(*guarded_instances, node);
    else if ( (uint) (current_time - current_node->last_checked) > waitchild)
    {
      instance->kill_instance(SIGKILL);
      /*
        Later we do node= node->next. This is ok, as we are only removing
        the node from the list. The pointer to the next one is still valid.
      */
      *guarded_instances= list_delete(*guarded_instances, node);
    }

    return;
  }

  if (instance->is_running())
  {
    /* clear status fields */
    current_node->restart_counter= 0;
    current_node->crash_moment= 0;
    current_node->state= STARTED;
  }
  else
  {
    switch (current_node->state) {
    case NOT_STARTED:
      instance->start();
      current_node->last_checked= current_time;
      log_info("guardian: starting instance %s",
               instance->options.instance_name);
      current_node->state= STARTING;
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
        instance->start();
        log_info("guardian: starting instance %s",
                 instance->options.instance_name);
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
          instance->start();
          current_node->last_checked= current_time;
          current_node->restart_counter++;
          log_info("guardian: restarting instance %s",
                   instance->options.instance_name);
        }
        else
          current_node->state= CRASHED_AND_ABANDONED;
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
  Run guardian thread

  SYNOPSYS
    run()

  DESCRIPTION

    Check for all guarded instances and restart them if needed. If everything
    is fine go and sleep for some time.
*/

void Guardian_thread::run()
{
  Instance *instance;
  LIST *node;
  struct timespec timeout;

  thread_registry.register_thread(&thread_info);

  my_thread_init();
  pthread_mutex_lock(&LOCK_guardian);

  /* loop, until all instances were shut down at the end */
  while (!(shutdown_requested && (guarded_instances == NULL)))
  {
    node= guarded_instances;

    while (node != NULL)
    {
      struct timespec timeout;

      GUARD_NODE *current_node= (GUARD_NODE *) node->data;
      instance= ((GUARD_NODE *) node->data)->instance;
      process_instance(instance, current_node, &guarded_instances, node);

      node= node->next;
    }
    timeout.tv_sec= time(NULL) + monitoring_interval;
    timeout.tv_nsec= 0;

    /* check the loop predicate before sleeping */
    if (!(shutdown_requested && (!(guarded_instances))))
      thread_registry.cond_timedwait(&thread_info, &COND_guardian,
                                     &LOCK_guardian, &timeout);
  }

  stopped= TRUE;
  pthread_mutex_unlock(&LOCK_guardian);
  /* now, when the Guardian is stopped we can stop the IM */
  thread_registry.unregister_thread(&thread_info);
  thread_registry.request_shutdown();
  my_thread_end();
}


int Guardian_thread::is_stopped()
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

  SYNOPSYS
    Guardian_thread::init()

  NOTE: One should always lock guardian before calling this routine.

  RETURN
    0 - ok
    1 - error occured
*/

int Guardian_thread::init()
{
  Instance *instance;
  Instance_map::Iterator iterator(instance_map);

  instance_map->lock();
  /* clear the list of guarded instances */
  free_root(&alloc, MYF(0));
  init_alloc_root(&alloc, MEM_ROOT_BLOCK_SIZE, 0);
  guarded_instances= NULL;

  while ((instance= iterator.next()))
  {
    if (!(instance->options.nonguarded))
      if (guard(instance, TRUE))                /* do not lock guardian */
      {
        instance_map->unlock();
        return 1;
      }
  }

  instance_map->unlock();
  return 0;
}


/*
  Add instance to the Guardian list

  SYNOPSYS
    guard()
    instance           the instance to be guarded
    nolock             whether we prefer do not lock Guardian here,
                       but use external locking instead

  DESCRIPTION

    The instance is added to the guarded instances list. Usually guard() is
    called after we start an instance.

  RETURN
    0 - ok
    1 - error occured
*/

int Guardian_thread::guard(Instance *instance, bool nolock)
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

int Guardian_thread::stop_guard(Instance *instance)
{
  LIST *node;

  pthread_mutex_lock(&LOCK_guardian);
  node= guarded_instances;

  while (node != NULL)
  {
    /*
      We compare only pointers, as we always use pointers from the
      instance_map's MEM_ROOT.
    */
    if (((GUARD_NODE *) node->data)->instance == instance)
    {
      guarded_instances= list_delete(guarded_instances, node);
      pthread_mutex_unlock(&LOCK_guardian);
      return 0;
    }
    else
      node= node->next;
  }
  pthread_mutex_unlock(&LOCK_guardian);
  /* if there is nothing to delete it is also fine */
  return 0;
}

/*
  An internal method which is called at shutdown to unregister instances and
  attempt to stop them if requested.

  SYNOPSYS
    stop_instances()
    stop_instances_arg          whether we should stop instances at shutdown

  DESCRIPTION
    Loops through the guarded_instances list and prepares them for shutdown.
    If stop_instances was requested, we need to issue a stop command and change
    the state accordingly. Otherwise we simply delete an entry.

  NOTE
    Guardian object should be locked by the calling function.

  RETURN
    0 - ok
    1 - error occured
*/

int Guardian_thread::stop_instances(bool stop_instances_arg)
{
  LIST *node;
  node= guarded_instances;
  while (node != NULL)
  {
    if (!stop_instances_arg)
    {
      /* just forget about an instance */
      guarded_instances= list_delete(guarded_instances, node);
      /*
        This should still work fine, as we have only removed the
        node from the list. The pointer to the next one is still valid
      */
      node= node->next;
    }
    else
    {
      GUARD_NODE *current_node= (GUARD_NODE *) node->data;
      /*
        If instance is running or was running (and now probably hanging),
        request stop.
      */
      if (current_node->instance->is_running() ||
          (current_node->state == STARTED))
      {
        current_node->state= STOPPING;
        current_node->last_checked= time(NULL);
      }
      else
        /* otherwise remove it from the list */
        guarded_instances= list_delete(guarded_instances, node);
      /* But try to kill it anyway. Just in case */
      current_node->instance->kill_instance(SIGTERM);
      node= node->next;
    }
  }
  return 0;
}


void Guardian_thread::lock()
{
  pthread_mutex_lock(&LOCK_guardian); 
}


void Guardian_thread::unlock()
{
  pthread_mutex_unlock(&LOCK_guardian);
}
