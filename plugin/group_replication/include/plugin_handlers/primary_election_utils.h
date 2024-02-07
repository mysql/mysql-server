/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#ifndef PRIMARY_ELECTION_UTILS_INCLUDED
#define PRIMARY_ELECTION_UTILS_INCLUDED

#include <string>

#include "plugin/group_replication/include/member_version.h"

class Plugin_gcs_message;

class Election_member_info {
 public:
  Election_member_info(const std::string uuid, const Member_version &version,
                       bool is_primary);

  /**
    @return The member uuid
  */
  std::string &get_uuid();

  /**
    @return The member version
  */
  Member_version &get_member_version();

  /**
    @note This only returns true for single primary mode
    @return Is this member a primary?
  */
  bool is_primary();

  /**
    @return Did the member left the group
  */
  bool member_left();

  /**
    @return this member has channels?
  */
  bool has_channels();

  /**
    @return was this member information updated
  */
  bool is_information_set();

  /**
    Set the flag that tells if the member has channels
    @param running_channels the member has channels?
  */
  void set_has_running_channels(bool running_channels);

  /**
    Set the flag that tells if this class was updated
    @param set was the info updated?
  */
  void set_information_set(bool set);

  /**
    Did the member left the group
    @param left did the member left the group
  */
  void set_member_left(bool left);

 private:
  /** The member uuid */
  std::string member_uuid;
  /** This member version*/
  Member_version member_version;
  /** Is this member the primary*/
  bool is_member_primary;
  /** Does the member has running channels*/
  bool has_running_channels;
  /** Did the member leave?*/
  bool has_member_left;
  /** Was the info for this member set*/
  bool info_is_set;
};

bool send_message(Plugin_gcs_message *message);

/**
  Kill transactions and enable super_read_only mode
  @param err_msg                  the sql error message
*/
void kill_transactions_and_leave_on_election_error(std::string &err_msg);

#endif /* PRIMARY_ELECTION_UTILS_INCLUDED */
