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
  thread_registry.register_thread(&thread_info);
  init_alloc_root(&alloc, MEM_ROOT_BLOCK_SIZE, 0);
  guarded_instances= NULL;
}


Guardian_thread::~Guardian_thread()
{
  /* delay guardian destruction to the moment when no one needs it */
  pthread_mutex_lock(&LOCK_guardian);
  free_root(&alloc, MYF(0));
  thread_registry.unregister_thread(&thread_info);
  pthread_mutex_unlock(&LOCK_guardian);
  pthread_mutex_destroy(&LOCK_guardian);
}


/*
  Run guardian thread

  SYNOPSYS
    run()

  DESCRIPTION

    Check for all guarded instances and restart them if needed. If everything
    is fine go and sleep for some time.

  RETURN
    The function return no value
*/

void Guardian_thread::run()
{
  Instance *instance;
  LIST *loop;
  int i=0;

  my_thread_init();

  while (!thread_registry.is_shutdown())
  {
    pthread_mutex_lock(&LOCK_guardian);
    loop= guarded_instances;
    while (loop != NULL)
    {
      instance= (Instance *) loop->data;
      if (instance != NULL)
      {
        if (!instance->is_running())
          instance->start();
      }
      loop= loop->next;
    }
    pthread_mutex_unlock(&LOCK_guardian);
    sleep(monitoring_interval);
  }

  my_thread_end();
}


/*
  Start instance guarding

  SYNOPSYS
    guard()
    instance_name      the name of the instance to be guarded
    name_len           the length of the name

  DESCRIPTION

    The instance is added to the list of guarded instances.

  RETURN
    0 - ok
    1 - error occured
*/

int Guardian_thread::guard(const char *instance_name, uint name_len)
{
  LIST *lst;
  Instance *instance;

  lst= (LIST *) alloc_root(&alloc, sizeof(LIST));
  if (lst == NULL) return 1;
  instance= instance_map->find(instance_name, name_len);
  /* we store the pointers to instances from the instance_map's MEM_ROOT */
  lst->data= (void *) instance;

  pthread_mutex_lock(&LOCK_guardian);
  guarded_instances= list_add(guarded_instances, lst);
  pthread_mutex_unlock(&LOCK_guardian);

  return 0;
}


/*
  TODO: perhaps it would make sense to create a pool of the LIST elements
  elements and give them upon request. Now we are loosing a bit of memory when
  guarded instance was stopped and then restarted (since we cannot free just
  a piece of the MEM_ROOT).
*/

int Guardian_thread::stop_guard(const char *instance_name, uint name_len)
{
  LIST *lst;
  Instance *instance;

  instance= instance_map->find(instance_name, name_len);

  lst= guarded_instances;
  if (lst == NULL) return 1;

  pthread_mutex_lock(&LOCK_guardian);
  while (lst != NULL)
  {
    /*
      We compare only pointers, as we always use pointers from the
      instance_map's MEM_ROOT.
    */
    if ((Instance *) lst->data == instance)
    {
      guarded_instances= list_delete(guarded_instances, lst);
      pthread_mutex_unlock(&LOCK_guardian);
      return 0;
    }
    else lst= lst->next;
  }
  pthread_mutex_unlock(&LOCK_guardian);
  /* if there is nothing to delete it is also fine */
  return 0;
}

