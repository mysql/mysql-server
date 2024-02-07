/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_SCHEDULER_H
#define MYSQL_SCHEDULER_H

#include <mysql/components/service.h>
#include <mysql/components/services/bits/mysql_scheduler_bits.h>

/**
  @ingroup group_components_services_inventory

  The Scheduler queueing interface.

  This is a service that will allow registering a callback to be called
  every X seconds.

  Each callback has a name. And a comment for the DBA to check.

  A caller registers a callback together with its argument and gets a handle
  back. The callback will be put in rotation immediately and will be called
  after the defined interval. And it will keep being called until it's
  unregisterered using the handle.

  Passing an argument is useful if you are to reuse the callback function
  to operate on many different states through registering it in multiple
  scheduled events.
  Having the argument passed down from the registering code to the callback
  saves the need of the callback to consult a global structure in a multi-
  thread safe fashion.

  @warning Keep the runnable function *SHORT*! Or design a way for it to be
  graciously shut down through a flag inside the argument.

  To use it one would do something like this:

  @code
  bool sheduled_callback(mysql_scheduler_runnable_handle handle, void *arg) {
     int *ctr = reinterpret_cast<std::atomic<int> *>(arg);
     *ctr++;
  }

  REQUIRES_SERVICE(mysql_scheduler);

  int caller() {
     std::atomic<int> data= 0;
     mysql_scheduler_runnable_handle handle = nullptr;
     mysql_service_mysql_scheduler->create(
       &handle, sheduled_callback,
       (void *)&data,
       "gizmo", "This is a sheduled task to call gizmo",
       10);

     ....

     mysql_service_mysql_scheduler->destroy(handle);
     handle = nullptr;
  }
  @endcode

  @sa @ref mysql_scheduler_runnable_handle,
  @ref mysql_scheduler_runnable_function

*/
BEGIN_SERVICE_DEFINITION(mysql_scheduler)

/**
  Schedule a runnable task

  @param[out] out_handle   The context of the newly scheduled task. To be kept
                            as an interaction vector with the running task.
  @param runnable          The function to call
  @param arg               An argument to pass to the runnable.
                            Must be valid until unregistered.
  @param name              The name of the runnable in UTF8.
  @param comment           A free form comment in UTF8. No more than 1024 bytes.
  @param interval_secs     How frequently to schedule the task (in seconds).
                            Must be greater than 1 since this is the resolution.
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(create,
                    (mysql_scheduler_runnable_handle * out_handle,
                     mysql_scheduler_runnable_function runnable, void *arg,
                     const char *name, const char *comment, int interval_secs));

/**
  End a scheduled task/subtask

  This will:
  * unschedule the task
  * wait for the scheduled task runnable to end, if running currently

  @param handle The context of the task to end
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(destroy, (mysql_scheduler_runnable_handle handle));

END_SERVICE_DEFINITION(mysql_scheduler)

#endif /* MYSQL_SCHEDULER_H */
