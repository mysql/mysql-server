/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_VIEW_MODIFICATION_NOTIFIER_INCLUDE
#define GCS_VIEW_MODIFICATION_NOTIFIER_INCLUDE

#include "plugin/group_replication/include/plugin_constants.h"
#include "plugin/group_replication/include/plugin_server_include.h"

/*
  @class Plugin_gcs_view_modification_notifier

  This class is used with the purpose of issuing a view changing event
  and wait for its completion.
 */
class Plugin_gcs_view_modification_notifier {
 public:
  Plugin_gcs_view_modification_notifier();
  virtual ~Plugin_gcs_view_modification_notifier();

  /**
    Signals that a view modification is about to start
   */
  void start_view_modification();

  /**
    Signals that a injected view modification, usually to
    unblock a group that did lost majority, is about to start.
   */
  void start_injected_view_modification();

  /**
    Checks if the view modification is a injected one.

    @return
      @retval true  if the current view modification is a injected one
      @retval false otherwise
   */
  bool is_injected_view_modification();

  /**
    Checks if there is a view modification ongoing.

    @return
      @retval true  there is a view modification ongoing
      @retval false otherwise
   */
  bool is_view_modification_ongoing();

  /**
    Signals that a view modification has ended
  */
  void end_view_modification();

  /**
    Signals that a view modification has been cancelled

    @param[in]  errnr  error that did cause the view modification to
                     be cancelled
  */
  void cancel_view_modification(
      int errnr = GROUP_REPLICATION_CONFIGURATION_ERROR);

  /**
    Check if view modification was cancelled.
    This method must only be called after view modification is complete.

    @return
      @retval true   view modification was cancelled
      @retval false  otherwise
  */
  bool is_cancelled();

  /**
    Method in which one waits for the view modification to end

    @param[in] timeout how long one wants to wait, in seconds

    @return Operation status
      @retval false  OK
      @retval true   error (timeout or view modification cancelled)
   */
  bool wait_for_view_modification(long timeout = VIEW_MODIFICATION_TIMEOUT);

  /**
    Get the error number that did happen on view modification.
    This method must only be called after view modification is complete.

    @return Operation
      @retval 0      OK (no error)
      @retval >0     error number
  */
  int get_error();

 private:
  bool view_changing;
  bool cancelled_view_change;
  bool injected_view_modification;
  int error;

  mysql_cond_t wait_for_view_cond;
  mysql_mutex_t wait_for_view_mutex;
};

#endif /* GCS_VIEW_MODIFICATION_NOTIFIER_INCLUDE */
