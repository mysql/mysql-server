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

#ifndef GCS_COROSYNC_COMMUNICATION_INTERFACE_INCLUDED
#define	GCS_COROSYNC_COMMUNICATION_INTERFACE_INCLUDED

#include "gcs_communication_interface.h"
#include "gcs_corosync_statistics_interface.h"
#include "gcs_corosync_utils.h"

#include <corosync/cpg.h>
#include <corosync/corotypes.h>

#include <map>
#include <cstdlib>
#include <unistd.h>

using std::map;

#define NUMBER_OF_GCS_RETRIES_ON_ERROR 3
#define GCS_SLEEP_TIME_ON_ERROR 1

/**
  @class Gcs_corosync_communication_proxy

  This class is an abstraction layer between Corosync and the actual
  implementation. The purpose of this is to allow
  Gcs_corosync_communication_interface to be unit testable by creating mock
  classes on top of it.
 */
class Gcs_corosync_communication_proxy
{
public:
  /**
   Bridge to the cpg_mcast_joined method of Corosync. This method is the one
   responsible to send messages to a group

   For more informations about its behaviour, please refer to the Corosync
   documentation or to its own man page.
   */
  virtual cs_error_t cpg_mcast_joined (cpg_handle_t handle,
                                       cpg_guarantee_t guarantee,
                                       const struct iovec *iovec,
                                       unsigned int iov_len) = 0;

  virtual ~Gcs_corosync_communication_proxy(){}
};

/**
  @class Gcs_corosync_communication_proxy_impl

  Implementation of Gcs_corosync_communication_proxy to be used by whom
  instantiates Gcs_corosync_communication_interface to be used in a real
  scenario.
 */
class Gcs_corosync_communication_proxy_impl:
                                  public Gcs_corosync_communication_proxy
{
public:
  cs_error_t cpg_mcast_joined (cpg_handle_t handle,
                               cpg_guarantee_t guarantee,
                               const struct iovec *iovec,
                               unsigned int iov_len);

  virtual ~Gcs_corosync_communication_proxy_impl(){ }
};

/*
  @interface Gcs_corosync_communication_interface

  Abstraction layer that adds Corosync specific methods to the generic
  communication interface

  This adds the following functionalities to the generic
  Gcs_communication_interface:
  - Ability to send messages without view safety and stats counter. This method
    shall be used by the State Exchange algorithm when the high-level view
    change is still occurring.
  - Delegation method that will contain all the business logic related with
    messages delivery to registered clients
 */
class Gcs_corosync_communication_interface: public Gcs_communication_interface
{
public:
  /**
    Sends a message that is internal to the binding implementation.
    This message will not be subject to the same restricions of send_message.
    As such, it will not observe view safety nor will count for the statistics
    of messages sent

    @param[in] message_to_send the message to send
    @return
      @retval >= 0 Success and the bytes sent
      @retval -1   In case of error.

   */
  virtual long send_binding_message(Gcs_message *message_to_send)= 0;

  /**
    The purpose of this method is to be called when in Gcs_corosync_interface
    callback method deliver_message is invoked.

    This allows, in terms of software architecture, to concentrate all the
    message delivery logic and processing in a single place.

    The deliver_message callback that is registered in Corosync
    (in gcs_corosync_interface.h) and that actually receives the low-level
    messages, is implemented as a delegator to this method.

    @param[in] name      Group name
    @param[in] nodeid    Corosync node identifier
    @param[in] pid local Pid from where the message was sent
    @param[in] data      The transmitted data
    @param[in] len       Data length
   */
  virtual void deliver_message(const struct cpg_name *name, uint32_t nodeid,
                               uint32_t pid, void *data, size_t len) = 0;

  virtual ~Gcs_corosync_communication_interface(){}
};

/**
  @class Gcs_corosync_communication_interface

  Implementation of the Gcs_communication_interface for Corosync
 */
class Gcs_corosync_communication: public Gcs_corosync_communication_interface
{
public:

  /**
    Gcs_corosync_communication_interface constructor

    @param[in] handle Corosync communication handle
    @param[in] stats a reference to the statistics interface
    @param[in] proxy a reference to an implementation of
                 Gcs_corosync_communication_proxy
    @param[in] vce a reference to a gcs_corosync_view_change_control_interface
               implementation
   */
  Gcs_corosync_communication
                      (cpg_handle_t handle,
                       Gcs_corosync_statistics_updater *stats,
                       Gcs_corosync_communication_proxy *proxy,
                       Gcs_corosync_view_change_control_interface* vce);

  virtual ~Gcs_corosync_communication();

  //Implementation of the Gcs_communication_interface

  /**
    Implementation of the public send_message method defined in
    Gcs_corosync_communication.
    Besides sending a message to the group, this method does two extra things:
    - Guarantees view safety, in which no messages can be sent when a view change
      is occurring
    - Registers in the statistics interface that a message was sent
   */
  bool send_message(Gcs_message *message_to_send);

  int add_event_listener(Gcs_communication_event_listener* event_listener);

  void remove_event_listener(int event_listener_handle);

  /**
   Delegation method from Gcs_corosync_interface object
   */
  void deliver_message(const struct cpg_name *name, uint32_t nodeid,
                       uint32_t pid, void *data, size_t len);

  //Implementation of the Gcs_corosync_communication_interface
  long send_binding_message(Gcs_message *message_to_send);

  //For unit testing purposes
  map<int, Gcs_communication_event_listener*>* get_event_listeners();

private:
  /**
    Converts from the interface guarantee to Corosync guarantee

    @param[in] param the interface guarantee
    @return the converted Corosync guarantee
      @retval CPG_TYPE_UNORDERED, in case of NO_ORDER or unknown param
      @retval CPG_TYPE_AGREED, in case of TOTAL_ORDER param
      @retval CPG_TYPE_SAFE, in case of UNIFORM param
   */
  cpg_guarantee_t to_corosync_guarantee(gcs_message_delivery_guarantee param);

  //Reference to the corosync handle
  cpg_handle_t corosync_handle;

  //Registered event listeners
  map<int, Gcs_communication_event_listener*> event_listeners;

  //Reference to the stats updater interface
  Gcs_corosync_statistics_updater *stats;

  //Reference to the Corosync proxy interface
  Gcs_corosync_communication_proxy *proxy;

  //Reference to the view change control object
  Gcs_corosync_view_change_control_interface* view_notif;
};

#endif	/* GCS_COROSYNC_COMMUNICATION_INTERFACE_INCLUDED */
