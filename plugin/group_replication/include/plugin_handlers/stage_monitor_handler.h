/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#ifndef STAGE_MONITOR_HANDLER_INCLUDED
#define STAGE_MONITOR_HANDLER_INCLUDED

#include <mysql/components/my_service.h>
#include "plugin/group_replication/include/plugin_psi.h"

class Plugin_stage_monitor_handler {
 public:
  Plugin_stage_monitor_handler();
  virtual ~Plugin_stage_monitor_handler();

  /**
    Fetch the registry and the service for this class

    @returns 0 in case of success, or 1 otherwise
  */
  int initialize_stage_monitor();

  /**
    Terminate the stage monitor.
    It means the stage monitor is declared as not running and the service is
    released.

    @returns 0 in case of success, or 1 otherwise
  */
  int terminate_stage_monitor();

  /**
     Set that a new stage is now in progress.
     @param key The PSI key for the stage
     @param file the file for this stage
     @param line the line of the file for this stage
     @param estimated_work what work is estimated for this stage
     @param work_completed what work already completed for this stage

     @returns 0 in case of success, or 1 otherwise
   */
  int set_stage(PSI_stage_key key, const char *file, int line,
                ulonglong estimated_work, ulonglong work_completed);

  /**
    Set the currently estimated work for this stage
  */
  void set_estimated_work(ulonglong estimated_work);

  /**
    Set the currently completed work for this stage
  */
  void set_completed_work(ulonglong completed_work);

  // get methods

  /**
    End the current stage
  */
  void end_stage();

 private:
  /** The generic service handle for the PSI stage service*/
  my_h_service generic_service;
  /** The progress handler when a stage is running*/
  PSI_stage_progress *stage_progress_handler;
  /** Is the reference to the PSI stage service still valid*/
  bool service_running;
  /** Lock for use vs termination scenarios */
  mysql_mutex_t stage_monitor_lock;
};

#endif /* STAGE_MONITOR_HANDLER_INCLUDED */
