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

#ifndef GR_RECOVERY_METADATA_JOINER_INCLUDE
#define GR_RECOVERY_METADATA_JOINER_INCLUDE

#include <string>
#include <vector>

#include "plugin/group_replication/include/gcs_operations.h"

class Recovery_metadata_joiner_information {
 public:
  /**
    Constructor
  */
  Recovery_metadata_joiner_information(const std::string &view_id)
      : m_joiner_view_id(view_id) {}

  /**
    Destructor
  */
  virtual ~Recovery_metadata_joiner_information() {}

  /**
    Is the member joiner and waiting for the recovery metadata.

    @return the send status
      @retval true   Member is joiner and waiting for recovery metadata.
      @retval false  Member is not joiner.
  */
  bool is_member_waiting_on_metadata();

  /**
    Does the metadata belongs to the joiners for which it was waiting?

    @param  view_id  The view ID of which metadata has to be compared

    @return the send status
      @retval true   View-ID matches with the metadata in which joiner joined
      @retval false  View-ID does not match with the metadata in which joiner
                     joined
  */
  bool is_joiner_recovery_metadata(const std::string &view_id);

  /**
    Is there any member in the group that has the recovery metadata for the
    joiner?

    @return the send status
      @retval true   Atleast 1 server in the group has recovery metadata for the
                     joiner.
      @retval false  No server in the group has recovery metadata for the
                     joiner.
  */
  bool is_valid_sender_list_empty();

  /**
    Saves the GCS Member ID of the metadata senders i.e. members that were
    ONLINE when the join request was received. This members will save the
    recovery metadata till the joiner receives it.

    @param  valid_senders   GCS Member ID of the member having recovery
                            metadata
  */
  void set_valid_sender_list_of_joiner(
      const std::vector<Gcs_member_identifier> &valid_senders);

  /**
    Delete the members that have left the group from the stored GCS Member ID of
    the valid sender list.

    @param  member_left     GCS Member ID of the member left the group.
  */
  void delete_leaving_members_from_sender(
      std::vector<Gcs_member_identifier> member_left);

 private:
  /** Stores valid recovery metadata senders for joiner. */
  std::vector<Gcs_member_identifier> m_valid_senders_for_joiner;

  /** View ID on which joiner joined */
  const std::string m_joiner_view_id;
};

#endif /* GR_RECOVERY_METADATA_JOINER_INCLUDE */
