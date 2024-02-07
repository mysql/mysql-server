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

#include "plugin/group_replication/include/plugin_observers/recovery_metadata_observer.h"
#include "plugin/group_replication/include/applier.h"  // Recovery_metadata_processing_packets
#include "plugin/group_replication/include/plugin.h"

Recovery_metadata_observer::Recovery_metadata_observer() {
  group_events_observation_manager->register_group_event_observer(this);
}

Recovery_metadata_observer::~Recovery_metadata_observer() {
  group_events_observation_manager->unregister_group_event_observer(this);
}

int Recovery_metadata_observer::after_view_change(
    const std::vector<Gcs_member_identifier> &joining,
    const std::vector<Gcs_member_identifier> &leaving,
    const std::vector<Gcs_member_identifier> &group, bool is_leaving,
    bool *skip_election, enum_primary_election_mode *election_mode,
    std::string &suggested_primary) {
  /*
    If joiner is waiting for the recovery metadata, inform joiner of
    metadata-senders that have left the group. If no recovery metadata sender is
    present in the group, leave the group.
  */
  if (is_leaving || leaving.size())
    recovery_metadata_module
        ->delete_leaving_members_from_joiner_and_leave_group_if_no_valid_sender(
            leaving, is_leaving);
  /*
   If current member is leaving the group it needs to clean all the saved
   recovery metadata. This is done by setting a clear flag in
   "Recovery_metadata_processing_packets". If current member is not leaving the
   group then it needs to remove other saved leaving members, which is done by:
   1. Pass the list of members that are leaving the group via
   "Recovery_metadata_processing_packets"
   2. Recovery_metadata_processing_packets packets is added to the
   applier-pipeline.
   3. Since add of the Recovery metadata is handled by the applier-pipeline and
   delete of Recovery metadata is also being done from the applier pipeline we
   need not synchronize the add and delete operations. Applier-pipeline is
   single threaded.
   4. Pipeline will call the function
   delete_members_from_all_recovery_view_metadata_send_metadata_if_sender_left
   of Recovery_metadata_module to do the proper cleanup.
   5. During cleanup
   delete_members_from_all_recovery_view_metadata_send_metadata_if_sender_left
   will remove leaving members from the valid sender list.
   6. During cleanup
   delete_members_from_all_recovery_view_metadata_send_metadata_if_sender_left
   will remove leaving members from the joiner list.
   7. If post cleanup there are no joiner or the valid sender for the view,
   delete_members_from_all_recovery_view_metadata_send_metadata_if_sender_left
   mark such views for deletion.
   8.
   delete_members_from_all_recovery_view_metadata_send_metadata_if_sender_left
   deletes all the stored metadata of the views marked for deletion.
   9. If the metadata sender had left the group,
   delete_members_from_all_recovery_view_metadata_send_metadata_if_sender_left
   finds the new metadata sender and re-sends the recovery metadata. In short
   entire process is being done from Recovery_metadata_processing_packets
   pipeline packet.
  */
  Recovery_metadata_processing_packets *metadata_packet =
      new Recovery_metadata_processing_packets();

  if (is_leaving) {
    metadata_packet->m_current_member_leaving_the_group = true;
  } else {
    std::copy(leaving.begin(), leaving.end(),
              std::back_inserter(metadata_packet->m_member_left_the_group));
  }

  applier_module->add_metadata_processing_packet(metadata_packet);
  return 0;
}

int Recovery_metadata_observer::after_primary_election(
    std::string primary_uuid,
    enum_primary_election_primary_change_status primary_change_status,
    enum_primary_election_mode election_mode, int error) {
  return 0;
}

int Recovery_metadata_observer::before_message_handling(
    const Plugin_gcs_message &message, const std::string &message_origin,
    bool *skip_message) {
  return 0;
}
