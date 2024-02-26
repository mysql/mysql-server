/* Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/plugin_observers/group_event_observer.h"
#include "plugin/group_replication/include/plugin.h"
#include "plugin/group_replication/include/plugin_psi.h"

Group_event_observer::~Group_event_observer() = default;

Group_events_observation_manager::Group_events_observation_manager() {
  observer_list_lock = new Checkable_rwlock(
#ifdef HAVE_PSI_INTERFACE
      key_GR_RWLOCK_group_event_observation_list
#endif
  );
}

Group_events_observation_manager::~Group_events_observation_manager() {
  if (!group_events_observers.empty()) {
    /* purecov: begin inspected */
    std::list<Group_event_observer *>::const_iterator obs_iterator;
    for (obs_iterator = group_events_observers.begin();
         obs_iterator != group_events_observers.end(); ++obs_iterator) {
      delete (*obs_iterator);
    }
    group_events_observers.clear();
    /* purecov: end */
  }

  delete observer_list_lock;
}

void Group_events_observation_manager::register_group_event_observer(
    Group_event_observer *observer) {
  DBUG_TRACE;
  write_lock_observer_list();
  group_events_observers.push_back(observer);
  unlock_observer_list();
}

void Group_events_observation_manager::unregister_group_event_observer(
    Group_event_observer *observer) {
  DBUG_TRACE;
  write_lock_observer_list();
  group_events_observers.remove(observer);
  unlock_observer_list();
}

void Group_events_observation_manager::read_lock_observer_list() {
  observer_list_lock->rdlock();
}

void Group_events_observation_manager::write_lock_observer_list() {
  observer_list_lock->wrlock();
}

void Group_events_observation_manager::unlock_observer_list() {
  observer_list_lock->unlock();
}

int Group_events_observation_manager::after_view_change(
    const std::vector<Gcs_member_identifier> &joining,
    const std::vector<Gcs_member_identifier> &leaving,
    const std::vector<Gcs_member_identifier> &group, bool is_leaving,
    bool *skip_election, enum_primary_election_mode *election_mode,
    std::string &suggested_primary) {
  int error = 0;

  read_lock_observer_list();
  for (Group_event_observer *observer : group_events_observers) {
    bool skip_election_flag = false;
    error += observer->after_view_change(joining, leaving, group, is_leaving,
                                         &skip_election_flag, election_mode,
                                         suggested_primary);
    *skip_election = *skip_election || skip_election_flag;
  }
  unlock_observer_list();

  return error;
}

int Group_events_observation_manager::after_primary_election(
    std::string primary_uuid,
    enum_primary_election_primary_change_status primary_change_status,
    enum_primary_election_mode election_mode, int error_on_election) {
  int error = 0;
  assert(primary_change_status !=
             enum_primary_election_primary_change_status::PRIMARY_DID_CHANGE ||
         (primary_change_status ==
              enum_primary_election_primary_change_status::PRIMARY_DID_CHANGE &&
          group_member_mgr->is_member_info_present(primary_uuid)));
#ifndef NDEBUG
  if (primary_change_status == enum_primary_election_primary_change_status::
                                   PRIMARY_DID_CHANGE_WITH_ERROR ||
      primary_change_status == enum_primary_election_primary_change_status::
                                   PRIMARY_DID_NOT_CHANGE_NO_CANDIDATE) {
    assert(error_on_election != 0);
  }
#endif
  read_lock_observer_list();
  for (Group_event_observer *observer : group_events_observers) {
    error += observer->after_primary_election(
        primary_uuid, primary_change_status, election_mode, error_on_election);
  }
  unlock_observer_list();

  return error;
}

int Group_events_observation_manager::before_message_handling(
    const Plugin_gcs_message &message, const std::string &message_origin,
    bool *skip_message) {
  int error = 0;
  read_lock_observer_list();
  for (Group_event_observer *observer : group_events_observers) {
    bool skip_message_flag = false;
    error += observer->before_message_handling(message, message_origin,
                                               &skip_message_flag);
    *skip_message = *skip_message || skip_message_flag;
  }
  unlock_observer_list();

  return error;
}
