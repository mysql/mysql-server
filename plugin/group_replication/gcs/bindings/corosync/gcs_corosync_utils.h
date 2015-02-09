/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_COROSYNC_UTILS_INCLUDED
#define	GCS_COROSYNC_UTILS_INCLUDED

#include "gcs_member_identifier.h"
#include "gcs_types.h"

#include <sstream>

#define NUMBER_OF_GCS_RETRIES_ON_ERROR 3
#define GCS_SLEEP_TIME_ON_ERROR 1

using std::ostringstream;

#define GCS_COROSYNC_RETRIES(method, result)                  \
  result= CS_OK;                                              \
  uint number_of_retries= NUMBER_OF_GCS_RETRIES_ON_ERROR;     \
                                                              \
  while(number_of_retries > 0)                                \
  {                                                           \
    result= method;                                           \
                                                              \
    if(result == CS_ERR_TRY_AGAIN)                            \
    {                                                         \
      sleep(GCS_SLEEP_TIME_ON_ERROR);                         \
      number_of_retries--;                                    \
    }                                                         \
    else                                                      \
    {                                                         \
      number_of_retries= 0;                                   \
    }                                                         \
  }

/**
  @class gcs_corosync_utils

  Class where the common binding utilities reside as static methods
 */
class Gcs_corosync_utils
{
public:
  /**
    Create a Gcs_member_identifier from raw Corosync data

    @param[in] local_node_id local_node raw date from Corosync
    @param[in] pid process identifier of this node

    @return a generic Gcs_member_identifier instance. Yet, the identifier value
            contained in the returned instance is built specifically with
            Corosync values.
   */
  static Gcs_member_identifier* build_corosync_member_id(uint32 local_node_id,
                                                         int pid);

  virtual ~Gcs_corosync_utils();
};

/*
 @interface gcs_corosync_view_change_control_interface

  This interface will serve as a synchronization point to all those that are
  interested in maintaning view safety. This will guarantee that no actions are
  accomplished while a view change procedure is ongoing.

  The promotors of view change will indicate via start_view_exchange() and
  end_view_exchange() the boundaries of the process. Those that want to wait
  for the end, will synchronize on wait_for_view_change_end()
 */
class Gcs_corosync_view_change_control_interface
{
public:
  virtual ~Gcs_corosync_view_change_control_interface(){}

  virtual void start_view_exchange()= 0;
  virtual void end_view_exchange()= 0;
  virtual void wait_for_view_change_end()= 0;
};

/*
  @class gcs_corosync_view_change_control

 Implementation of gcs_corosync_view_change_control_interface
 */
class Gcs_corosync_view_change_control:
                      public Gcs_corosync_view_change_control_interface
{
public:
  Gcs_corosync_view_change_control();
  virtual ~Gcs_corosync_view_change_control();

  void start_view_exchange();
  void end_view_exchange();
  void wait_for_view_change_end();

private:
  bool view_changing;

  pthread_cond_t wait_for_view_cond;
  pthread_mutex_t wait_for_view_mutex;
};

#endif	/* GCS_COROSYNC_UTILS_INCLUDED */


