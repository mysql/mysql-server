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

#ifndef GCS_CONTROL_INTERFACE_INCLUDED
#define GCS_CONTROL_INTERFACE_INCLUDED

#include "gcs_control_data_exchange_event_listener.h"
#include "gcs_control_event_listener.h"
#include "gcs_member_identifier.h"
#include "gcs_view.h"

#include <vector>

/**
  @class Gcs_control_interface

  This interface represents all the control functionalities that a binding
  implementation must provide. It contains methods for:
  - View Membership
  - Control Events
  - Generic Data Exchange
 */
class Gcs_control_interface
{
public:
  /**
    Method that causes this node to join the group that this interface pertains

    @return true in case of any error
   */
  virtual bool join()= 0;

  /**
    Method that causes this node to leave the group that this interface pertains

    @return true in case of any error
   */
  virtual bool leave()= 0;

  /**
    Reports if this node has joined a group

    @return true if the node belongs to a group
   */
  virtual bool belongs_to_group()= 0;

  /**
    Returns the currently installed view.

    @return
      @retval a valid pointer to a Gcs_view object. If the node has joined and
              left a group, this will return the last installed view.
      @retval NULL, if the node never joined a group
   */
  virtual Gcs_view* get_current_view()= 0;

  /**
   Retrieves the local identifier of this node on a group.

    @return
      @retval a reference to a valid Gcs_member_identifier instance
      @retval NULL in case of error
   */
  virtual Gcs_member_identifier* get_local_information()= 0;

  /**
    Registers an implementation of a Gcs_control_event_listener that will
    receive Control Events

    @param[in] event_listener a class that implements Gcs_control_event_listener
    @return an handle representing the registration of this object to
            be used in remove_event_listener
   */
  virtual int add_event_listener
                           (Gcs_control_event_listener* event_listener)= 0;

  /**
   Removes a previously registered event listener

    @param[in] event_listener_handle the handle returned when the listener was
                                 registered
   */
  virtual void remove_event_listener(int event_listener_handle)= 0;

  /**
    Sets the data that will be exchanged when this node joins a group

    @param[in] data the data to be exchanged
  */
  virtual void set_exchangeable_data(vector<uchar> *data)= 0;

  /**
    Registers an implementation of a Gcs_control_data_exchange_event_listener
    that will receive the data exchanged in the join process

    @param[in] event_listener
    @return an handle representing the registration of this object to
            be used in remove_data_exchange_event_listener
  */
  virtual int add_data_exchange_event_listener
                 (Gcs_control_data_exchange_event_listener* event_listener)= 0;

  /**
    Removes a previously registered event listener

    @param[in] event_listener_handle the handle returned when the listener was
                                 registered
   */
  virtual void remove_data_exchange_event_listener
                                                (int event_listener_handle)= 0;

  virtual ~Gcs_control_interface(){}
};

#endif // GCS_CONTROL_INTERFACE_INCLUDED
