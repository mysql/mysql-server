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
#include "mysql_manager_error.h"
#include "log.h"
#include <string.h>

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
  thread_info(pthread_self())
{
  pthread_mutex_init(&LOCK_guardian, 0);
  pthread_cond_init(&COND_guardian, 0);
  shutdown_guardian= FALSE;
  is_stopped= FALSE;
  init_alloc_root(&alloc, MEM_ROOT_BLOCK_SIZE, 0);
  guarded_instances= NULL;
  starting_instances= NULL;
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


void Guardian_thread::shutdown()
{
  pthread_mutex_lock(&LOCK_guardian);
  shutdown_guardian= TRUE;
  pthread_mutex_unlock(&LOCK_guardian);
}


void Guardian_thread::request_stop_instances()
{
  pthread_mutex_lock(&LOCK_guardian);
  request_stop= TRUE;
  pthread_mutex_unlock(&LOCK_guardian);
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
  int restart_retry= 100;
  LIST *loop;
  struct timespec timeout;

  thread_registry.register_thread(&thread_info);

  my_thread_init();
  pthread_mutex_lock(&LOCK_guardian);


  while (!shutdown_guardian)
  {
    int status= 0;
    loop= guarded_instances;

    while (loop != NULL)
    {
      instance= ((GUARD_NODE *) loop->data)->instance;
      if (!instance->is_running())
      {
        int state= 0;                           /* state of guardian */

        if ((((GUARD_NODE *) loop->data)->crash_moment == 0))
          state= 1;                             /* an instance just crashed */
        else
           if (time(NULL) - ((GUARD_NODE *) loop->data)->crash_moment <= 2)
             /* try to restart an instance immediately */
             state= 2;
           else
             state= 3;                          /* try to restart it */

        if (state == 1)
          ((GUARD_NODE *) loop->data)->crash_moment= time(NULL);

        if ((state == 1) || (state == 2))
        {
          instance->start();
          ((GUARD_NODE *) loop->data)->restart_counter++;
          log_info("guardian: starting instance %s",
                   instance->options.instance_name);
        }
        else
        {
          if ((status == ETIMEDOUT) &&
             (((GUARD_NODE *) loop->data)->restart_counter < restart_retry))
          {
           instance->start();
           ((GUARD_NODE *) loop->data)->restart_counter++;
           log_info("guardian: starting instance %s",
                    instance->options.instance_name);
          }
        }
      }
      else /* clear status fields */
      {
        ((GUARD_NODE *) loop->data)->restart_counter= 0;
        ((GUARD_NODE *) loop->data)->crash_moment= 0;
      }
      loop= loop->next;
    }
    move_to_list(&starting_instances, &guarded_instances);
    timeout.tv_sec= time(NULL) + monitoring_interval;
    timeout.tv_nsec= 0;

    status= pthread_cond_timedwait(&COND_guardian, &LOCK_guardian, &timeout);
  }

  pthread_mutex_unlock(&LOCK_guardian);
  if (request_stop)
    stop_instances();
  is_stopped= TRUE;
  /* now, when the Guardian is stopped we can stop the IM */
  thread_registry.unregister_thread(&thread_info);
  thread_registry.request_shutdown();
  my_thread_end();
}


int Guardian_thread::start()
{
  Instance *instance;
  Instance_map::Iterator iterator(instance_map);

  instance_map->lock();
  while ((instance= iterator.next()))
  {
    if ((instance->options.nonguarded == NULL))
      if (add_instance_to_list(instance, &guarded_instances))
        return 1;
  }
  instance_map->unlock();

  return 0;
}


/*
  Start instance guarding

  SYNOPSYS
    guard()
    instance           the instance to be guarded

  DESCRIPTION

    The instance is added to the list of starting instances. Then after one guardian
    loop it is moved to the guarded instances list. Usually guard() is called after we
    start an instance, so we need to give some time to the instance to start.

  RETURN
    0 - ok
    1 - error occured
*/


int Guardian_thread::guard(Instance *instance)
{
  return add_instance_to_list(instance, &starting_instances);
}


void Guardian_thread::move_to_list(LIST **from, LIST **to)
{
  LIST *tmp;

  while (*from)
  {
    tmp= rest(*from);
    *to= list_add(*to, *from);
    *from= tmp;
  }
}


int Guardian_thread::add_instance_to_list(Instance *instance, LIST **list)
{
  LIST *node;
  GUARD_NODE *content;

  node= (LIST *) alloc_root(&alloc, sizeof(LIST));
  content= (GUARD_NODE *) alloc_root(&alloc, sizeof(GUARD_NODE));

  if ((node == NULL) || (content == NULL))
    return 1;
  /* we store the pointers to instances from the instance_map's MEM_ROOT */
  content->instance= instance;
  content->restart_counter= 0;
  content->crash_moment= 0;
  node->data= (void *) content;

  pthread_mutex_lock(&LOCK_guardian);
  *list= list_add(*list, node);
  pthread_mutex_unlock(&LOCK_guardian);

  return 0;
}


/*
  TODO: perhaps it would make sense to create a pool of the LIST elements
  elements and give them upon request. Now we are loosing a bit of memory when
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

int Guardian_thread::stop_instances()
{
  Instance *instance;
  Instance_map::Iterator iterator(instance_map);

  while ((instance= iterator.next()))
  {
    if ((instance->options.nonguarded == NULL))
    {
      if (stop_guard(instance))
        return 1;
      /* let us try to stop the server */
      instance->stop();
    }
  }

  return 0;
}
