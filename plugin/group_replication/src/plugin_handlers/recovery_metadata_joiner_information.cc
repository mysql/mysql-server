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

#include "plugin/group_replication/include/plugin_handlers/recovery_metadata_joiner_information.h"

bool Recovery_metadata_joiner_information::is_member_waiting_on_metadata() {
  return !m_joiner_view_id.empty();
}

bool Recovery_metadata_joiner_information::is_joiner_recovery_metadata(
    const std::string &view_id) {
  return view_id == m_joiner_view_id;
}

bool Recovery_metadata_joiner_information::is_valid_sender_list_empty() {
  return m_valid_senders_for_joiner.size() == 0;
}

void Recovery_metadata_joiner_information::set_valid_sender_list_of_joiner(
    const std::vector<Gcs_member_identifier> &valid_senders) {
  std::copy(valid_senders.begin(), valid_senders.end(),
            std::back_inserter(m_valid_senders_for_joiner));
}

void Recovery_metadata_joiner_information::delete_leaving_members_from_sender(
    std::vector<Gcs_member_identifier> member_left) {
  for (auto it : member_left) {
    m_valid_senders_for_joiner.erase(
        std::remove(m_valid_senders_for_joiner.begin(),
                    m_valid_senders_for_joiner.end(), it),
        m_valid_senders_for_joiner.end());
  }
}
