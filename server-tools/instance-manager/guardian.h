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
#include <my_sys.h>
#include <my_list.h>

#ifdef __GNUC__
#pragma interface
#endif

class Instance_map;

#include "thread_registry.h"
#include "instance.h"

C_MODE_START

pthread_handler_decl(guardian, arg);

C_MODE_END


struct Guardian_thread_args
{
  Thread_registry &thread_registry;
  Instance_map *instance_map;
  uint monitoring_interval;

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
  Guardian_thread(Thread_registry &thread_registry_arg,
                  Instance_map *instance_map_arg,
                  uint monitoring_interval_arg);
  ~Guardian_thread();
  void run();
  int init();
  int guard(const char *instance_name, uint name_len);
  int stop_guard(const char *instance_name, uint name_len);

private:
  pthread_mutex_t LOCK_guardian;
  Thread_info thread_info;
  LIST *guarded_instances;
  MEM_ROOT alloc;
  enum { MEM_ROOT_BLOCK_SIZE= 512 };
};

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_GUARDIAN_H */
