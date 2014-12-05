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

#ifndef GCS_COROSYNC_CONTROL_INTERFACE_INCLUDED
#define	GCS_COROSYNC_CONTROL_INTERFACE_INCLUDED

#include "gcs_control_interface.h"
#include "gcs_state_exchange.h"
#include "gcs_message.h"
#include "gcs_types.h"

#include "gcs_corosync_utils.h"

#include <unistd.h>
#include <cstring>
#include <map>
#include <cstdlib>

#include <corosync/cpg.h>
#include <corosync/corotypes.h>

using std::map;
using std::vector;

/*
  Max number of tries when extracting the local identifier from Corosync
*/
const int MAX_NUMBER_OF_ID_EXTRACTION_TENTATIVES= 10;

/**
  @class gcs_corosync_control_proxy

  This class is an abstraction layer between Corosync and the actual
  implementation. The purpose of this is to allow
  Gcs_corosync_control_interface to be unit testable by creating mock
  classes on top of it.
 */
class Gcs_corosync_control_proxy
{
public:
  virtual cs_error_t cpg_join (cpg_handle_t handle,
                               const struct cpg_name *group) = 0;

  virtual cs_error_t cpg_leave (cpg_handle_t handle,
                                const struct cpg_name *group) = 0;

  virtual cs_error_t cpg_local_get (cpg_handle_t handle,
                                    unsigned int *local_nodeid) = 0;

  virtual ~Gcs_corosync_control_proxy(){ }
};

/**
  @class gcs_corosync_control_proxy_impl

  Implementation of gcs_corosync_control_proxy to be used by whom
  instantiates Gcs_corosync_control_interface to be used in a real
  scenario.
 */
class Gcs_corosync_control_proxy_impl : public Gcs_corosync_control_proxy
{
public:
  cs_error_t cpg_join (cpg_handle_t handle, const struct cpg_name *group);

  cs_error_t cpg_leave (cpg_handle_t handle, const struct cpg_name *group);

  cs_error_t cpg_local_get (cpg_handle_t handle, unsigned int *local_nodeid);

  virtual ~Gcs_corosync_control_proxy_impl(){ }
};

/**
  @class Gcs_corosync_control_interface

  This class implements the generic Gcs_control_interface. It relates with:
  - Gcs_corosync_interface, since the view_changed registered callback will
     delegate its calls to an instance of this class.
  - Gcs_corosync_control_proxy in order to isolate Corosync calls from their
     actual implementation, to allow unit testing
  - Gcs_corosync_view_change_control_interface that implements a structure
     to allow View Safety. This ensures that, while the view installation
     procedure is not finished, all applications are not allowed to execute
     operations based upon a possible inconsistent state.
 */
class Gcs_corosync_control: public Gcs_control_interface
{
public:
  /**
    Gcs_corosync_control_interface constructor

    @param[in] handle reference to the Corosync handle
    @param[in] group_identifier Group identifier of this control interface
    @param[in] corosync_proxy Proxy implementation reference
    @param[in] se Reference to the State Exchange algorithm implementation
    @param[in] vce View change control interface reference
   */
  Gcs_corosync_control(cpg_handle_t handle,
                       Gcs_group_identifier group_identifier,
                       Gcs_corosync_control_proxy* corosync_proxy,
                       Gcs_corosync_state_exchange_interface* se,
                       Gcs_corosync_view_change_control_interface* vce);

  virtual ~Gcs_corosync_control();

  //Gcs_control_interface implementation
  bool join();

  bool leave();

  bool belongs_to_group();

  Gcs_view* get_current_view();

  Gcs_member_identifier* get_local_information();

  int add_event_listener(Gcs_control_event_listener* event_listener);

  void remove_event_listener(int event_listener_handle);

  void set_exchangeable_data(vector<uchar> *data);

  int add_data_exchange_event_listener(Gcs_control_data_exchange_event_listener* event_listener);

  void remove_data_exchange_event_listener(int event_listener_handle);

  /**
    The purpose of this method is to be called when in Gcs_corosync_interface
    callback method view_changed is invoked.

    This allows, in terms of software architecture, to concentrate all the
    view change logic and processing in a single place. The view_change
    callback that is registered in Gcs_corosync_interface should be a simple
    pass-through.

    @param[in] name group name where the view change happened
    @param[in] total All the members of the new view
    @param[in] total_entries All the member count of the new view
    @param[in] left Members that left from the previous view
    @param[in] left_entries Member count that left from the previous view
    @param[in] joined Members that joined this new view
    @param[in] joined_entries Member count that joined this new view
   */
  void view_changed(const struct cpg_name *name,
                    const struct cpg_address *total, size_t total_entries,
                    const struct cpg_address *left, size_t left_entries,
                    const struct cpg_address *joined, size_t joined_entries);

  /**
    Checks if a certain message is from the Control interface, mainly from
    State Exchange. If so, delegate it to it.

    @param[in] m the message to analyze

    @return true if it is a control message
   */
  bool process_possible_control_message(Gcs_message *msg);

  //For testing purposes
  map<int, Gcs_control_event_listener*>* get_event_listeners();
  map<int, Gcs_control_data_exchange_event_listener*>*
                                         get_data_exchange_listeners();

private:
  /**
    Copies from a set to a vector of Gcs_member_identifier

    @param[in] origin original set
    @param[in] to_fill destination vector
   */
  void build_member_list(set<Gcs_member_identifier*> *origin,
                         vector<Gcs_member_identifier> *to_fill);

  /**
    Makes all the necessary arrangements to install a new view in the binding
    and in all registered client applications

    @param[in] new_view_id new view identifier
    @param[in] group_name group name
    @param[in] total all the members
    @param[in] left members that left the last view
    @param[in] join members that joined from the last view
  */
  void install_view(Gcs_corosync_view_identifier* new_view_id,
                    string *group_name,
                    set<Gcs_member_identifier*> *total,
                    set<Gcs_member_identifier*> *left,
                    set<Gcs_member_identifier*> *join);

  //Handle returned by Corosync after registration
  cpg_handle_t          corosync_handle;

  //The group that this interface pertains
  Gcs_group_identifier* group_identifier;

  //Reference to the proxy between Corosync and this implementation
  Gcs_corosync_control_proxy* proxy;

  //Flag that states if one belongs to a group
  bool joined;

  //Reference to the currently installed view
  Gcs_view* current_view;

  //Map holding all the registered control event listeners
  map<int, Gcs_control_event_listener*> event_listeners;

  //Information about the local membership of this node
  Gcs_member_identifier *local_member_information;

  //Map holding all the registered data exchange event listeners
  map<int, Gcs_control_data_exchange_event_listener*> data_exchange_listeners;

  //A permanent reference to the exchangeable data when a VC occurs
  vector<uchar>* exchange_data;

  //A reference of the State Exchange algorithm implementation
  Gcs_corosync_state_exchange_interface* state_exchange;

  //Reference to the mechanism that ensures view safety
  Gcs_corosync_view_change_control_interface* view_notif;
};

#endif	/* GCS_COROSYNC_CONTROL_INTERFACE_INCLUDED */

