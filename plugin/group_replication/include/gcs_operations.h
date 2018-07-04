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

#ifndef GCS_OPERATIONS_INCLUDE
#define GCS_OPERATIONS_INCLUDE

#include <mysql/group_replication_priv.h>
#include <string>

#include "plugin/group_replication/include/gcs_logger.h"
#include "plugin/group_replication/include/gcs_plugin_messages.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_interface.h"

/**
  @class Gcs_operations
  Coordinates all operations to GCS interface.
*/
class Gcs_operations {
 public:
  /**
    @enum enum_leave_state

    This enumeration describes the return values when a process tries to leave
    a group.
  */
  enum enum_leave_state {
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
  Gcs_view *get_current_view();

  /**
    Configure the GCS interface.

    @param[in] parameters The configuration parameters

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  enum enum_gcs_error configure(const Gcs_interface_parameters &parameters);

  /**
    Reconfigure the GCS interface, i.e. update its configuration parameters.

    @param[in] parameters The configuration parameters

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  enum enum_gcs_error reconfigure(const Gcs_interface_parameters &parameters);

  /**
    Configure the debug options that shall be used by GCS.

    @param debug_options Set of debug options separated by comma

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  enum enum_gcs_error set_debug_options(std::string &debug_options) const;

  /**
    Request server to join the group.

    @param[in] communication_event_listener The communication event listener
    @param[in] control_event_listener       The control event listener

    @return the operation status
      @retval 0      OK
      @retval !=0    Error
  */
  enum enum_gcs_error join(
      const Gcs_communication_event_listener &communication_event_listener,
      const Gcs_control_event_listener &control_event_listener);

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
  int get_local_member_identifier(std::string &identifier);

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
  enum enum_gcs_error send_message(const Plugin_gcs_message &message,
                                   bool skip_if_not_initialized = false);

  /**
    Forces a new group membership, on which the excluded members
    will not receive a new view and will be blocked.

    @param members  The list of members, comma
                    separated. E.g., host1:port1,host2:port2

    @return Operation status
      @retval 0      OK
      @retval !=0    Error
  */
  int force_members(const char *members);

  /**
    Retrieves the minimum supported "write concurrency" value.
  */
  uint32_t get_minimum_write_concurrency() const;

  /**
    Retrieves the maximum supported "write concurrency" value.
  */
  uint32_t get_maximum_write_concurrency() const;

  /**
    Retrieves the group's "write concurrency" value.

    @param[out] write_concurrency A reference to where the group's "write
                concurrency" will be written to

    @retval GCS_OK if successful
    @retval GCS_NOK if unsuccessful
  */
  enum enum_gcs_error get_write_concurrency(uint32_t &write_concurrency);

  /**
    Reconfigures the group's "write concurrency" value.

    The caller should ensure that the supplied value is between @c
    minimum_write_concurrency and @c maximum_write_concurrency.

    The method is non-blocking, meaning that it shall only send the
    request to an underlying GCS. The final result can be polled via @c
    get_write_concurrency.

    @param write_concurrency The desired "write concurrency" value.

    @retval GCS_OK if successful
    @retval GCS_NOK if unsuccessful
  */
  enum enum_gcs_error set_write_concurrency(uint32_t new_write_concurrency);

  /**
   * @return the communication engine being used
   */
  static const std::string &get_gcs_engine();

 private:
  /**
    Internal function that configures the debug options that shall be used by
    GCS.
  */
  enum enum_gcs_error do_set_debug_options(std::string &debug_options) const;
  Gcs_group_management_interface *get_gcs_group_manager() const;

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
