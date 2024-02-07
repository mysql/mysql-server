/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef RECOVERY_METADATA_OBSERVER_H
#define RECOVERY_METADATA_OBSERVER_H

#include "plugin/group_replication/include/plugin_observers/group_event_observer.h"
#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_member_identifier.h"

/**
  @class Recovery_metadata_observer
  This class is used by:
  1. joiner: to keep track of all leaving recovery metadata donors,
             so that joiner can error out if no donor is available.
  2. recovery metadata donors:
        to keep track of all leaving recovery metadata donors,
        so that other donor can send metadata if any exisiting
        donor has left before successfully sending data.
*/
class Recovery_metadata_observer : public Group_event_observer {
 public:
  /**
    Constructor
    Register observer to get notification on view change.
  */
  Recovery_metadata_observer();

  /**
    Destructor.
    Unregister observers
  */
  virtual ~Recovery_metadata_observer() override;

 private:
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
                        std::string &suggested_primary) override;

  /**
    Executed after primary election

    @param primary_uuid    the elected primary
    @param primary_change_status if the primary changed after the election
    @param election_mode   what was the election mode
    @param error           if there was and error on the process
  */
  int after_primary_election(
      std::string primary_uuid,
      enum_primary_election_primary_change_status primary_change_status,
      enum_primary_election_mode election_mode, int error) override;

  /**
    Executed before the message is processed

    @param message             The GCS message
    @param message_origin      The member that sent this message (address)
    @param[out] skip_message   skip message handling if true
  */
  int before_message_handling(const Plugin_gcs_message &message,
                              const std::string &message_origin,
                              bool *skip_message) override;
};

#endif /* RECOVERY_METADATA_OBSERVER_H */
