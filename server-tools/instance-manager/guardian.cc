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
#include "options.h"


/*************************************************************************
 {{{ Constructor & destructor.
*************************************************************************/

/**
  Guardian constructor.

  SYNOPSIS
    Guardian()
    thread_registry_arg
    instance_map_arg

  DESCRIPTION
    Nominal contructor intended for assigning references and initialize
    trivial objects. Real initialization is made by init() method.
*/

Guardian::Guardian(Thread_registry *thread_registry_arg,
                   Instance_map *instance_map_arg)
  :shutdown_requested(FALSE),
  stopped(FALSE),
  thread_registry(thread_registry_arg),
  instance_map(instance_map_arg)
{
  pthread_mutex_init(&LOCK_guardian, 0);
  pthread_cond_init(&COND_guardian, 0);
}


Guardian::~Guardian()
{
  /*
    NOTE: it's necessary to synchronize here, because Guiardian thread can be
    still alive an hold the mutex (because it is detached and we have no
    control over it).
  */

  lock();
  unlock();

  pthread_mutex_destroy(&LOCK_guardian);
  pthread_cond_destroy(&COND_guardian);
}

/*************************************************************************
  }}}
*************************************************************************/


/**
  Send request to stop Guardian.

  SYNOPSIS
    request_shutdown()
*/

void Guardian::request_shutdown()
{
  stop_instances();

  lock();
  shutdown_requested= TRUE;
  unlock();

  ping();
}


/**
  Process an instance.

  SYNOPSIS
    process_instance()
    instance  a pointer to the instance for processing

  MT-NOTE:
    - the given instance must be locked before calling this operation;
    - Guardian must be locked before calling this operation.
*/

void Guardian::process_instance(Instance *instance)
{
  int restart_retry= 100;
  time_t current_time= time(NULL);

  if (instance->get_state() == Instance::STOPPING)
  {
    /* This brach is executed during shutdown. */

    /* This returns TRUE if and only if an instance was stopped for sure. */
    if (instance->is_crashed())
    {
      log_info("Guardian: '%s' stopped.",
               (const char *) instance->get_name()->str);

      instance->set_state(Instance::STOPPED);
    }
    else if ((uint) (current_time - instance->last_checked) >=
             instance->options.get_shutdown_delay())
    {
      log_info("Guardian: '%s' hasn't stopped within %d secs.",
               (const char *) instance->get_name()->str,
               (int) instance->options.get_shutdown_delay());

      instance->kill_mysqld(SIGKILL);

      log_info("Guardian: pretend that '%s' is killed.",
               (const char *) instance->get_name()->str);

      instance->set_state(Instance::STOPPED);
    }
    else
    {
      log_info("Guardian: waiting for '%s' to stop (%d secs left).",
               (const char *) instance->get_name()->str,
               (int) (instance->options.get_shutdown_delay() -
                      current_time + instance->last_checked));
    }

    return;
  }

  if (instance->is_mysqld_running())
  {
    /* The instance can be contacted  on it's port */

    /* If STARTING also check that pidfile has been created */
    if (instance->get_state() == Instance::STARTING &&
        instance->options.load_pid() == 0)
    {
      /* Pid file not created yet, don't go to STARTED state yet  */
    }
    else if (instance->get_state() != Instance::STARTED)
    {
      /* clear status fields */
      log_info("Guardian: '%s' is running, set state to STARTED.",
               (const char *) instance->options.instance_name.str);
      instance->reset_stat();
      instance->set_state(Instance::STARTED);
    }
  }
  else
  {
    switch (instance->get_state()) {
    case Instance::NOT_STARTED:
      log_info("Guardian: starting '%s'...",
               (const char *) instance->options.instance_name.str);

      /* NOTE: set state to STARTING _before_ start() is called. */
      instance->set_state(Instance::STARTING);
      instance->last_checked= current_time;

      instance->start_mysqld();

      return;

    case Instance::STARTED:     /* fallthrough */
    case Instance::STARTING:    /* let the instance start or crash */
      if (!instance->is_crashed())
        return;

      instance->crash_moment= current_time;
      instance->last_checked= current_time;
      instance->set_state(Instance::JUST_CRASHED);
      /* fallthrough -- restart an instance immediately */

    case Instance::JUST_CRASHED:
      if (current_time - instance->crash_moment <= 2)
      {
        if (instance->is_crashed())
        {
          instance->start_mysqld();
          log_info("Guardian: starting '%s'...",
                   (const char *) instance->options.instance_name.str);
        }
      }
      else
        instance->set_state(Instance::CRASHED);

      return;

    case Instance::CRASHED:    /* just regular restarts */
      if ((ulong) (current_time - instance->last_checked) <=
          (ulong) Options::Main::monitoring_interval)
        return;

      if (instance->restart_counter < restart_retry)
      {
        if (instance->is_crashed())
        {
          instance->start_mysqld();
          instance->last_checked= current_time;

          log_info("Guardian: restarting '%s'...",
                   (const char *) instance->options.instance_name.str);
        }
      }
      else
      {
        log_info("Guardian: can not start '%s'. "
                 "Abandoning attempts to (re)start it",
                 (const char *) instance->options.instance_name.str);

        instance->set_state(Instance::CRASHED_AND_ABANDONED);
      }

      return;

    case Instance::CRASHED_AND_ABANDONED:
      return; /* do nothing */

    default:
      DBUG_ASSERT(0);
    }
  }
}


/**
  Main function of Guardian thread.

  SYNOPSIS
    run()

  DESCRIPTION
    Check for all guarded instances and restart them if needed.
*/

void Guardian::run()
{
  struct timespec timeout;

  log_info("Guardian: started.");

  thread_registry->register_thread(&thread_info);

  /* Loop, until all instances were shut down at the end. */

  while (true)
  {
    Instance_map::Iterator instances_it(instance_map);
    Instance *instance;
    bool all_instances_stopped= TRUE;

    instance_map->lock();

    while ((instance= instances_it.next()))
    {
      instance->lock();

      if (!instance->is_guarded() ||
          instance->get_state() == Instance::STOPPED)
      {
        instance->unlock();
        continue;
      }

      process_instance(instance);

      if (instance->get_state() != Instance::STOPPED)
        all_instances_stopped= FALSE;

      instance->unlock();
    }

    instance_map->unlock();

    lock();

    if (shutdown_requested && all_instances_stopped)
    {
      log_info("Guardian: all guarded mysqlds stopped.");

      stopped= TRUE;
      unlock();
      break;
    }

    set_timespec(timeout, Options::Main::monitoring_interval);

    thread_registry->cond_timedwait(&thread_info, &COND_guardian,
                                    &LOCK_guardian, &timeout);
    unlock();
  }

  log_info("Guardian: stopped.");

  /* Now, when the Guardian is stopped we can stop the IM. */

  thread_registry->unregister_thread(&thread_info);
  thread_registry->request_shutdown();

  log_info("Guardian: finished.");
}


/**
  Return the value of stopped flag.
*/

bool Guardian::is_stopped()
{
  int var;

  lock();
  var= stopped;
  unlock();

  return var;
}


/**
  Wake up Guardian thread.

  MT-NOTE: though usually the mutex associated with condition variable should
  be acquired before signalling the variable, here this is not needed.
  Signalling under locked mutex is used to avoid lost signals. In the current
  logic however locking mutex does not guarantee that the signal will not be
  lost.
*/

void Guardian::ping()
{
  pthread_cond_signal(&COND_guardian);
}


/**
  Prepare list of instances.

  SYNOPSIS
    init()

  MT-NOTE: Instance Map must be locked before calling the operation.
*/

void Guardian::init()
{
  Instance *instance;
  Instance_map::Iterator iterator(instance_map);

  while ((instance= iterator.next()))
  {
    instance->lock();

    instance->reset_stat();
    instance->set_state(Instance::NOT_STARTED);

    instance->unlock();
  }
}


/**
  An internal method which is called at shutdown to unregister instances and
  attempt to stop them if requested.

  SYNOPSIS
    stop_instances()

  DESCRIPTION
    Loops through the guarded_instances list and prepares them for shutdown.
    For each instance we issue a stop command and change the state
    accordingly.

  NOTE
    Guardian object should be locked by the caller.

*/

void Guardian::stop_instances()
{
  Instance_map::Iterator instances_it(instance_map);
  Instance *instance;

  instance_map->lock();

  while ((instance= instances_it.next()))
  {
    instance->lock();

    if (!instance->is_guarded() ||
        instance->get_state() == Instance::STOPPED)
    {
      instance->unlock();
      continue;
    }

    /*
      If instance is running or was running (and now probably hanging),
      request stop.
    */

    if (instance->is_mysqld_running() ||
        instance->get_state() == Instance::STARTED)
    {
      instance->set_state(Instance::STOPPING);
      instance->last_checked= time(NULL);
    }
    else
    {
      /* Otherwise mark it as STOPPED. */
      instance->set_state(Instance::STOPPED);
    }

    /* Request mysqld to stop. */

    instance->kill_mysqld(SIGTERM);

    instance->unlock();
  }

  instance_map->unlock();
}


/**
  Lock Guardian.
*/

void Guardian::lock()
{
  pthread_mutex_lock(&LOCK_guardian);
}


/**
  Unlock Guardian.
*/

void Guardian::unlock()
{
  pthread_mutex_unlock(&LOCK_guardian);
}
