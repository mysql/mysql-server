/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef GR_AUTOREJOIN_INCLUDED
#define GR_AUTOREJOIN_INCLUDED

#include "plugin/group_replication/include/member_info.h"
#include "plugin/group_replication/include/plugin_utils.h"

/**
  Represents and encapsulates the logic responsible for handling the auto-rejoin
  process within Group Replication. The auto-rejoin process kicks in one of two
  possible situations: either the member was expelled from the group or the
  member lost contact to a majority of the group. The auto-rejoin feature must
  also be explicitly enabled by setting the group_replication_autorejoin_tries
  sysvar to a value greater than 0.

  This thread will do a busy-wait loop for group_replication_autorejoin_tries
  number of attempts, waiting 5 minutes between each attempt (this wait is
  achieved via a timed wait on a conditional variable).

  We execute the auto-rejoin process in its own thead because the join operation
  of the GCS layer is asynchronous. We cannot actually block while waiting for a
  confirmation if the server managed to join the group or not. As such, we wait
  on a callback invoked by an entity that is registered as a GCS event listener.

  @sa Plugin_gcs_events_handler
*/
class Autorejoin_thread {
 public:
  /**
    Deleted copy ctor.
  */
  Autorejoin_thread(const Autorejoin_thread &) = delete;

  /**
    Deleted move ctor.
  */
  Autorejoin_thread(const Autorejoin_thread &&) = delete;

  /**
    Deleted assignment operator.
  */
  Autorejoin_thread &operator=(const Autorejoin_thread &) = delete;

  /**
    Deleted move operator.
  */
  Autorejoin_thread &operator=(const Autorejoin_thread &&) = delete;

  /**
    Initializes the synchronization primitives of the thread.
  */
  Autorejoin_thread();

  /**
    The dtor for the thread will destroy the mutex and cond_var.
  */
  ~Autorejoin_thread();

  /**
    Initializes the auto-rejoin module with a clean slate, i.e. it
    resets any state/flags that are checked in start_autorejoin().

    @sa start_autorejoin
  */
  void init();

  /**
    Aborts the thread's main loop, effectively killing the thread.

    @return a flag indicating whether or not the auto-rejoin procedure was
    ongoing at the time the abort was requested.
    @retval true the auto-rejoin was ongoing
    @retval false the auto-rejoin wasn't running
  */
  bool abort_rejoin();

  /**
    Starts the process of auto-rejoin, launches the thread that will call
    attempt_rejoin() until it succeeds or until it reaches a given
    amount of maximum retries, waiting on a conditional variable each
    iteration with a given timeout.

    An auto-rejoin can only start if it isn't already running or if the
    auto-rejoin module is not in the process of being terminated.

    @param[in] attempts the number of attempts we will try to rejoin.
    @param[in] timeout the time to wait between each retry.
    @return whether or not we managed to launch the auto-rejoin thread.
      @retval 0 the thread launched successfully
      @retval != 0 the thread couldn't be launched
    @sa mysql_thread_create
  */
  int start_autorejoin(uint attempts, ulonglong timeout);

  /**
    Returns a flag indicating whether or not the auto-rejoin process is ongoing
    on this thread.

    @return the state of the rejoin process.
    @retval true if the auto-rejoin is ongoing
    @retval false otherwise
  */
  bool is_autorejoin_ongoing();

 private:
  /**
    The thread callback passed onto mysql_thread_create.

    @param[in] arg a pointer to an Autorejoin_thread instance.

    @return Does not return.
  */
  static void *launch_thread(void *arg);

  /**
    The thread handle, i.e. setups and tearsdown the infrastructure for this
    mysql_thread.
  */
  [[noreturn]] void autorejoin_thread_handle();

  /**
    Handles the busy-wait retry loop.
  */
  void execute_rejoin_process();

  /** the THD handle. */
  THD *m_thd;
  /** the state of the thread. */
  thread_state m_autorejoin_thd_state;
  /** the thread handle. */
  my_thread_handle m_handle;
  /** the mutex for controlling access to the thread itself. */
  mysql_mutex_t m_run_lock;
  /** the cond_var used to signal the thread. */
  mysql_cond_t m_run_cond;
  /** flag to indicate whether or not the thread is to be aborted. */
  std::atomic<bool> m_abort;
  /**
    flag that indicates that the auto-rejoin module is in the process of
    being terminated.
  */
  bool m_being_terminated;
  /** the number of attempts for the rejoin. */
  ulong m_attempts;
  /** the time to wait in seconds until the next rejoin attempt. */
  ulonglong m_rejoin_timeout;
};

#endif /* GR_AUTREJOIN_INCLUDED */
