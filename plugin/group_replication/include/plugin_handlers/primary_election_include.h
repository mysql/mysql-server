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

#ifndef PRIMARY_ELECTION_INCLUDE_INCLUDED
#define PRIMARY_ELECTION_INCLUDE_INCLUDED

/** The server version in which member weight was introduced.*/
#define PRIMARY_ELECTION_MEMBER_WEIGHT_VERSION 0x050720
/** The server version in which online leader elections were introduced */
#define PRIMARY_ELECTION_LEGACY_ALGORITHM_VERSION 0x080013

/** Enum for election types */
enum enum_primary_election_mode {
  SAFE_OLD_PRIMARY = 0,    // Migrating from multi primary to single primary
  UNSAFE_OLD_PRIMARY = 1,  // Changing from one primary to another
  DEAD_OLD_PRIMARY = 2,    // Old primary died
  LEGACY_ELECTION_PRIMARY = 3,  // Electing a primary using legacy election
  ELECTION_MODE_END = 4         // Enum end
};

/** Enum for election errors */
enum enum_primary_election_error {
  PRIMARY_ELECTION_NO_ERROR = 0,             // No errors
  PRIMARY_ELECTION_NO_CANDIDATES_ERROR = 1,  // No candidates for election
  PRIMARY_ELECTION_PROCESS_ERROR = 2,  // There was a problem in the election
  PRIMARY_ELECTION_ERROR_END = 3       // Enum end
};

/** Enum for primary change status */
enum enum_primary_election_primary_change_status {
  PRIMARY_CHANGE_INIT = 0,  // Primary change still running
  PRIMARY_DID_CHANGE = 1,   // Primary changes successfully
  PRIMARY_DID_CHANGE_WITH_ERROR =
      2,  // Primary changed with error like cannot send
          // message to other members, or cannot changed
          // variables etc., member will move to ERROR state
  PRIMARY_DID_NOT_CHANGE_NO_CANDIDATE =
      3,  // Primary member is not present or primary cannot
          // be selected, no servers in the group
  PRIMARY_DID_NOT_CHANGE = 4,  // Primary did not change
  PRIMARY_DID_NOT_CHANGE_PRIMARY_LEFT_FORCE_ELECTION_END =
      5,  // Primary member left during election, end the election for a new
          // election to start
};
#endif /* PRIMARY_ELECTION_INCLUDE_INCLUDED */
