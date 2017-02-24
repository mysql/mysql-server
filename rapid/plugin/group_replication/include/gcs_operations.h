/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_OPERATIONS_INCLUDE
#define GCS_OPERATIONS_INCLUDE

#include <mysql/gcs/gcs_interface.h>
#include <mysql/group_replication_priv.h>

#include "gcs_logger.h"
#include "gcs_plugin_messages.h"

#include <string>


/**
  @class Gcs_operations
  Coordinates all operations to GCS interface.
*/
class Gcs_operations
{
public:
  /**
    @enum enum_leave_state

    This enumeration describes the return values when a process tries to leave
    a group.
  */
  enum enum_leave_state
  {
      /* The request was accepted, the member should now be leaving. */
      NOW_LEAVING,
      /* The member is already leaving, no point in retrying */
      ALREADY_LEAVING,
      /* The member already left */
      ALREADY_LEFT,
      /* There was an error when trying to leave */
      ERROR_WHEN_LEAVING
  };

  /**
    Default constructor.
  */
  Gcs_operations();

  /**
    Destructor.
   */
  virtual ~Gcs_operations();

  /**
    Initialize the GCS interface.

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int initialize();

  /**
    Finalize the GCS interface.
  */
  void finalize();

  /**
    Get the group current view.

    @return a copy of the group current view.
            NULL if member does not belong to a group..
            The return value must deallocated by the caller.
  */
  Gcs_view* get_current_view();

  /**
    Configure the GCS interface.

    @param[in] parameters The configuration parameters

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  enum enum_gcs_error configure(const Gcs_interface_parameters& parameters);

  /**
    Request server to join the group.

    @param[in] communication_event_listener The communication event listener
    @param[in] control_event_listener       The control event listener

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  enum enum_gcs_error join(const Gcs_communication_event_listener& communication_event_listener,
                           const Gcs_control_event_listener& control_event_listener);

  /**
    Returns true if this server belongs to the group.
  */
  bool belongs_to_group();

  /**
    Request GCS interface to leave the group.

    Note: This method only asks to leave, it does not know if request was
          successful

    @return the operation status
      @retval NOW_LEAVING         Request accepted, the member is leaving
      @retval ALREADY_LEAVING     The member is already leaving
      @retval ALREADY_LEFT        The member already left
      @retval ERROR_WHEN_LEAVING  An error happened when trying to leave
  */
  enum_leave_state leave();

  /**
    Declare the member as being already out of the group.
  */
  void leave_coordination_member_left();

  /**
    Get the local member identifier.

    @param[out] identifier The local member identifier when the
                           method is successful

    @return Operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int get_local_member_identifier(std::string& identifier);

  /**
    Send a message to the group.

    @param[in] message  The message to send
    @param[in] skip_if_not_initialized If true, the message will not be sent
                                       and no errors will returned when the
                                       GCS interface is not initialized

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  enum enum_gcs_error send_message(const Plugin_gcs_message& message,
                                   bool skip_if_not_initialized= false);

  /**
    Forces a new group membership, on which the excluded members
    will not receive a new view and will be blocked.

    @param members  The list of members, comma
                    separated. E.g., host1:port1,host2:port2

    @return Operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int force_members(const char* members);

  /**
   * @return the communication engine being used
   */
  static const std::string& get_gcs_engine();

private:
  static const std::string gcs_engine;
  Gcs_gr_logger_impl gcs_logger;
  Gcs_interface *gcs_interface;

  /** Is the member leaving*/
  bool leave_coordination_leaving;
  /** Did the member already left*/
  bool leave_coordination_left;
  /** Is finalize ongoing*/
  bool finalize_ongoing;

  Checkable_rwlock *gcs_operations_lock;
  Checkable_rwlock *finalize_ongoing_lock;
};

#endif /* GCS_OPERATIONS_INCLUDE */
