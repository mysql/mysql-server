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

#ifndef GCS_EVENT_HANDLERS_INCLUDE
#define GCS_EVENT_HANDLERS_INCLUDE

#include "gcs_control_event_listener.h"
#include "gcs_control_data_exchange_event_listener.h"
#include "gcs_communication_event_listener.h"

#include "recovery_message.h"
#include "applier.h"

#include "gcs_plugin_messages.h"
#include "recovery.h"

#include <algorithm>

using std::set;
using std::find;

/**
  Group_member_info_pointer_comparator to guarantee uniqueness
 */
struct Group_member_info_pointer_comparator
{
  bool operator()(Group_member_info* one,
                  Group_member_info* other) const
  {
    return *one < *other;
  }
};

/*
  @class Plugin_gcs_view_modification_notifier

  This class is used with the purpose of issuing a view changing event
  and wait for its completion.
 */
class Plugin_gcs_view_modification_notifier
{
public:
  Plugin_gcs_view_modification_notifier();
  virtual ~Plugin_gcs_view_modification_notifier();

  /**
    Signals that a view modification is about to start
   */
  void start_view_modification();

  /**
    Signals that a view modification has ended
   */
  void end_view_modification();

  /**
    Method in which one waits for the modification to end

    @param[in] timeout how long one wants to wait, in seconds
    @return true in case of error
   */
  bool wait_for_view_modification(long timeout);

private:
  bool view_changing;

  mysql_cond_t wait_for_view_cond;
  mysql_mutex_t wait_for_view_mutex;
#ifdef HAVE_PSI_INTERFACE
  PSI_mutex_key wait_for_view_key_mutex;
  PSI_cond_key  wait_for_view_key_cond;
#endif
};

/*
  @class Plugin_gcs_events_handler

  Implementation of all GCS event handlers to the plugin
 */
class Plugin_gcs_events_handler: public Gcs_communication_event_listener,
                                 public Gcs_control_event_listener,
                                 public Gcs_control_data_exchange_event_listener
{
public:
  /**
    Plugin_gcs_events_handler constructor

    It receives, via the constructor, all the necessary dependencies to work.
  */
  Plugin_gcs_events_handler(Applier_module_interface* applier_module,
                            Recovery_module* recovery_module,
                            Group_member_info_manager_interface* group_manager,
                            Group_member_info* local_member_info,
                            Plugin_gcs_view_modification_notifier* vc_notifier);
  virtual ~Plugin_gcs_events_handler();

  /*
   Implementation of all callback methods
   */
  void on_message_received(Gcs_message& message);
  void on_view_changed(Gcs_view *new_view);
  int  on_data(vector<uchar>* exchanged_data);

private:
  /*
   Individual handling methods for all possible message types
   received via on_message_received(...)
   */
  void handle_transactional_message(Gcs_message& message);
  void handle_certifier_message(Gcs_message& message);
  void handle_recovery_message(Gcs_message& message);

  /*
   Methods to act upon members after a on_view_change(...) is called
   */
  void update_group_info_manager(Gcs_view *new_view, bool is_leaving);
  void handle_joining_members(Gcs_view *new_view, bool is_joining);
  void handle_leaving_members(Gcs_view* new_view, bool is_joining,
                            bool is_leaving);

  void
  update_member_status(vector<Gcs_member_identifier>* members,
                       Group_member_info::Group_member_status status,
                       Group_member_info::Group_member_status condition_status);

  /**
    Verifies if a certain Vector of Member Ids contains a given member id.

    @param members   the vector with members to verify
    @param member_id the member to check if it contained.

    @return true if member_id occurs in members.
   */
  bool is_member_on_vector(vector<Gcs_member_identifier>* members,
                           Gcs_member_identifier member_id);

  Applier_module_interface* applier_module;
  Recovery_module* recovery_module;

  Group_member_info_manager_interface* group_info_mgr;
  Group_member_info* local_member_info;

  /*
    Holds, until view can be installed, all Member information received from
    other members
  */
  set<Group_member_info*,
      Group_member_info_pointer_comparator>* temporary_states;

  Plugin_gcs_view_modification_notifier* view_change_notifier;
};

#endif /* GCS_EVENT_HANDLERS_INCLUDE */
