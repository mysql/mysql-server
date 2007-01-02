#ifndef INCLUDES_MYSQL_INSTANCE_MANAGER_GUARDIAN_H
#define INCLUDES_MYSQL_INSTANCE_MANAGER_GUARDIAN_H
/* Copyright (C) 2004 MySQL AB

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


#include <my_global.h>
#include <my_sys.h>
#include <my_list.h>

#include "thread_registry.h"

#if defined(__GNUC__) && defined(USE_PRAGMA_INTERFACE)
#pragma interface
#endif

class Instance;
class Instance_map;
class Thread_registry;

/**
  The guardian thread is responsible for monitoring and restarting of guarded
  instances.
*/

class Guardian: public Thread
{
public:
  Guardian(Thread_registry *thread_registry_arg,
           Instance_map *instance_map_arg);
  ~Guardian();

  void init();

public:
  void request_shutdown();

  bool is_stopped();

  void lock();
  void unlock();

  void ping();

protected:
  virtual void run();

private:
  void stop_instances();

  void process_instance(Instance *instance);

private:
  /*
    LOCK_guardian protectes the members in this section:
      - shutdown_requested;
      - stopped;

    Also, it is used for COND_guardian.
  */
  pthread_mutex_t LOCK_guardian;

  /*
    Guardian's main loop waits on this condition. So, it should be signalled
    each time, when instance state has been changed and we want Guardian to
    wake up.

    TODO: Change this to having data-scoped conditions, i.e. conditions,
    which indicate that some data has been changed.
  */
  pthread_cond_t COND_guardian;

  /*
    This variable is set to TRUE, when Manager thread is shutting down.
    The flag is used by Guardian thread to understand that it's time to
    finish.
  */
  bool shutdown_requested;

  /*
    This flag is set to TRUE on shutdown by Guardian thread, when all guarded
    mysqlds are stopped.

    The flag is used in the Manager thread to wait for Guardian to stop all
    mysqlds.
  */
  bool stopped;

  Thread_info thread_info;
  Thread_registry *thread_registry;
  Instance_map *instance_map;

private:
  Guardian(const Guardian &);
  Guardian&operator =(const Guardian &);
};

#endif /* INCLUDES_MYSQL_INSTANCE_MANAGER_GUARDIAN_H */
