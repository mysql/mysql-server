/* Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#ifndef GROUP_EVENT_OBSERVER_INCLUDED
#define GROUP_EVENT_OBSERVER_INCLUDED

#include <mysql/group_replication_priv.h>
#include <list>
#include <string>
#include <vector>
#include "plugin/group_replication/include/gcs_plugin_messages.h"
#include "plugin/group_replication/include/plugin_handlers/primary_election_include.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"

/**
  @class Group_event_observer
  Class that others can extend to receive notifications about views and
  primary elections.
*/
class Group_event_observer {
 public:
  virtual ~Group_event_observer() = 0;
  /**
    Executed after view install and before primary election
    @param joining            members joining the group
    @param leaving            members leaving the group
    @param group              members in the group
    @param is_leaving         is the member leaving
    @param[out] skip_election skip primary election on view
    @param[out] election_mode election mode
    @param[out] suggested_primary what should be the next primary to elect
  */
  virtual int after_view_change(
      const std::vector<Gcs_member_identifier> &joining,
      const std::vector<Gcs_member_identifier> &leaving,
      const std::vector<Gcs_member_identifier> &group, bool is_leaving,
      bool *skip_election, enum_primary_election_mode *election_mode,
      std::string &suggested_primary) = 0;

  /**
    Executed after primary election
    @param primary_uuid    the elected primary
    @param primary_change_status if the primary changed after the election
    @param election_mode   what was the election mode
    @param error           if there was and error on the process
  */
  virtual int after_primary_election(
      std::string primary_uuid,
      enum_primary_election_primary_change_status primary_change_status,
      enum_primary_election_mode election_mode, int error) = 0;

  /**
    Executed before the message is processed
    @param message             The GCS message
    @param message_origin      The member that sent this message (address)
    @param[out] skip_message   skip message handling if true
  */
  virtual int before_message_handling(const Plugin_gcs_message &message,
                                      const std::string &message_origin,
                                      bool *skip_message) = 0;
};

/**
  @class Group_events_observation_manager
  Class alerts listeners of group events like view changes and elections.
*/
class Group_events_observation_manager {
 public:
  /*
    Initialize the Group_events_observation_manager class.
  */
  Group_events_observation_manager();

  /*
    Destructor.
    Deletes all observers
  */
  ~Group_events_observation_manager();

  /**
    The method to register new observers
    @param observer   An observer class to register
  */
  void register_group_event_observer(Group_event_observer *observer);

  /**
    The method to unregister new observers
    @param observer      An observer class to unregister
  */
  void unregister_group_event_observer(Group_event_observer *observer);

  /**
    Executed after view install and before primary election
    @param joining            members joining the group
    @param leaving            members leaving the group
    @param group              members in the group
    @param is_leaving         is the member leaving
    @param[out] skip_election skip primary election on view
    @param[out] election_mode election mode
    @param[out] suggested_primary what should be the next primary to elect
  */
  int after_view_change(const std::vector<Gcs_member_identifier> &joining,
                        const std::vector<Gcs_member_identifier> &leaving,
                        const std::vector<Gcs_member_identifier> &group,
                        bool is_leaving, bool *skip_election,
                        enum_primary_election_mode *election_mode,
                        std::string &suggested_primary);

  /**
    Executed after primary election
    @param primary_uuid    the elected primary
    @param primary_change_status if the primary changed after the election
    @param election_mode   what was the election mode
    @param error    if there was and error on the process
  */
  int after_primary_election(
      std::string primary_uuid,
      enum_primary_election_primary_change_status primary_change_status,
      enum_primary_election_mode election_mode, int error = 0);

  /**
    Executed before the message is processed
    @param message             The GCS message
    @param message_origin      The member that sent this message (address)
    @param[out] skip_message   skip message handling if true
  */
  int before_message_handling(const Plugin_gcs_message &message,
                              const std::string &message_origin,
                              bool *skip_message);

 private:
  /** Locks the observer list for reads */
  void read_lock_observer_list();
  /** Locks the observer list for writes */
  void write_lock_observer_list();
  /** Unlocks the observer list */
  void unlock_observer_list();

  /**The group event observers*/
  std::list<Group_event_observer *> group_events_observers;
  /**The lock to control access to the list*/
  Checkable_rwlock *observer_list_lock;
};

#endif /* GROUP_EVENT_OBSERVER_INCLUDED */
