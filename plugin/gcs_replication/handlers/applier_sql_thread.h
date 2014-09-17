/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef SQL_THREAD_APPLIER_INCLUDE
#define SQL_THREAD_APPLIER_INCLUDE

#include "../gcs_applier.h"
#include <rpl_pipeline_interfaces.h>
#include "../gcs_replication_threads_api.h"

class Applier_sql_thread : public EventHandler
{
public:
  Applier_sql_thread();
  int handle_event(PipelineEvent *ev,Continuation *cont);
  int handle_action(PipelineAction *action);
  int initialize();
  int terminate();
  bool is_unique();
  int get_role();

  /**
    Checks if all the queued transactions were executed.

    @param timeout  the time (seconds) after which the method returns if the
                    above condition was not satisfied

    @return the operation status
      @retval 0      All transactions were executed
      @retval -1     A timeout occurred
      @retval -2     An error occurred
  */
  int wait_for_gtid_execution(longlong timeout);

  bool is_own_event_channel(my_thread_id id);

private:

  Replication_thread_api sql_thread_interface;

};

#endif /* SQL_THREAD_APPLIER_INCLUDE */
