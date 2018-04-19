/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef GCS_CONTROL_INTERFACE_INCLUDED
#define GCS_CONTROL_INTERFACE_INCLUDED

#include <vector>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_control_event_listener.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_view.h"

/**
  @class Gcs_control_interface

  This interface represents all the control functionalities that a binding
  implementation must provide. Like all interfaces in this API, it is
  group-oriented, meaning that a single instance shall exist per group,
  as it was returned by the class Gcs_interface::get_control_session.

  It contains methods for:
  - View Membership;
  - Control Events;
  - Generic Data Exchange.

  View Membership contain operations that will conduct one to:
  - Belong to a group: join() and leave();
  - Information about its status: belongs_to_group() and
    get_current_view() that shall return the active Gcs_view.

  Due to the asynchronous nature of this interface, the results of
  join() and leave() operations, in local or remote members, shall come
  via an event. Those events shall be delivered in form of callbacks
  defined in Gcs_control_event_listener, that should be implemented by a
  client of this interface interested in receiving those notifications.

  Regarding Generic Data Exchange, it is a functionality that was created
  to allow exchange of arbitrary data among members when one joins.
  This generic data is retrieved via the callback get_exchangeable_data() each
  time a View Change (VC) occurs. It states the data that one would like to
  offer in exchange whenever a VC happens.

  What must happen under the hood is that, when one receives a VC from
  the underlying GCS, the binding implementation should start a round
  of message exchange, in which members (one or all depending of the algorithm)
  send the Exchangeable Data to the joining node.

  That data is delivered when Gcs_control_event_listener::on_view_changed is
  called, which hands-out all exchanged data in one point in time.

  A typical usage for that interface would be:

  @code{.cpp}
  class my_Gcs_control_event_listener: Gcs_control_event_listener
  {
    void on_view_changed(const Gcs_view *new_view,
                         const Exchanged_data &exchanged_data)
    {
      // D something when view arrives...
      // It will also deliver all data that nodes decided to offer at join()
      // time
    }

    Gcs_message_data &get_exchangeable_data()
    {
      // Return whatever data we want to provide to a joining node
    }
  }

  // Meanwhile in your client code...
  Gcs_control_interface *gcs_control_interface_instance; // obtained
                                                         // previously

  Gcs_control_event_listener *listener_instance=
    new my_Gcs_control_event_listener();

  int ref_handler=
    gcs_control_interface_instance->add_event_listener(listener_instance);

  // Normal program flow... join(), send_message(), leave()...

  gcs_control_interface_instance->join();

  gcs_control_interface_instance->leave();

  // In the end...
  gcs_control_interface_instance->remove_event_listener(ref_handler);

  @endcode
*/
class Gcs_control_interface {
 public:
  /**
    Method that causes one to join the group that this
    interface pertains.

    The method is non-blocking, meaning that it shall only send the
    request to an underlying GCS. The final result shall come via a
    View Change event delivered through Gcs_control_event_listener.

    @retval GCS_OK in case of everything goes well. Any other value of
            gcs_error in case of error.
   */

  virtual enum_gcs_error join() = 0;

  /**
    Method that causes one to leave the group that this
    interface pertains.

    The method is non-blocking, meaning that it shall only send the
    request to an underlying GCS. The final result shall come via a
    View Change event delivered through Gcs_control_event_listener.

    @retval GCS_OK in case of everything goes well. Any other value of
            gcs_error in case of error
   */

  virtual enum_gcs_error leave() = 0;

  /**
    Reports if one has joined and belongs to a group.

    @retval true if belonging to a group
  */

  virtual bool belongs_to_group() = 0;

  /**
    Returns the currently installed view.

    @retval - a valid pointer to a Gcs_view object.
              If one has left a group, this shall be the last
              installed view. That view can be considered a best-effort
              view since, in some GCSs, the one that leaves might not
              have access to the exchanged information.
    @retval - NULL if one never joined a group.
  */

  virtual Gcs_view *get_current_view() = 0;

  /**
    Retrieves the local identifier of this member on a group.

    @retval - a reference to a valid Gcs_member_identifier instance
    @retval - NULL in case of error
  */

  virtual const Gcs_member_identifier get_local_member_identifier() const = 0;

  /**
    Registers an implementation of a Gcs_control_event_listener that will
    receive Control Events. See the class header for more details on
    implementations and usage.

    Note that a binding implementation shall not offer the possibility of
    changing listeners while the system is up and running. In that sense,
    listeners must be added to it only when booting up the system.

    @param[in] event_listener a class that implements Gcs_control_event_listener
    @return an handle representing the registration of this object to
            be used in remove_event_listener
  */

  virtual int add_event_listener(
      const Gcs_control_event_listener &event_listener) = 0;

  /**
    Removes a previously registered event listener.

    Note that a binding implementation shall not offer the possibility of
    changing listeners while the system is up and running. In that sense
    listeners must be removed from it only when shutting down the system.

    @param[in] event_listener_handle the handle returned when the listener was
                                     registered
   */

  virtual void remove_event_listener(int event_listener_handle) = 0;

  virtual ~Gcs_control_interface() {}
};

#endif  // GCS_CONTROL_INTERFACE_INCLUDED
