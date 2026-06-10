/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#ifndef PRIMARY_ELECTION_MOST_UPTODATE
#define PRIMARY_ELECTION_MOST_UPTODATE

#include <cstdint>

class Primary_election_most_update {
 public:
  /**
    Get component group replication primary election most uptodate is enabled.

    @return the operation status
    @retval 0      OK
    @retval !=0    Error
    */
  static bool is_enabled();

  /**
    Update component group replication primary election most uptodate status
    variables.

    @param micro_seconds  Time since epoch in micro seconds.
    @param delta          Difference on number of transaction to most uptodate
    member.
    */
  static void update_status(unsigned long long int micro_seconds,
                            uint64_t delta);
};

#endif /* PRIMARY_ELECTION_MOST_UPTODATE */
