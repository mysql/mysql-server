/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#ifndef MY_SERVER_VERSION_INCLUDED
#define MY_SERVER_VERSION_INCLUDED

#include "my_inttypes.h"  // uint

struct My_server_version {
  uint version{0};
  bool is_lts{false};
  uint previous_lts{0};  // MYSQL_PREVIOUS_LTS_VERSION_ID, or 0 if absent.
  uint upgrade_threshold{0};
  uint downgrade_threshold{0};

  uint major() const { return version / 10000; }
  uint minor() const { return (version / 100) % 100; }
  uint patch() const { return version % 100; }
  uint base() const { return version / 100; }
  uint previous_lts_base() const { return previous_lts / 100; }

  bool operator<(const My_server_version &other) const {
    return version < other.version;
  }

  bool operator>=(const My_server_version &other) const {
    return version >= other.version;
  }

  bool has_same_lts_lineage_as(const My_server_version &other) const {
    return previous_lts_base() != 0 &&
           previous_lts_base() == other.previous_lts_base();
  }

  bool starts_lts_lineage_after(const My_server_version &source) const {
    return previous_lts_base() != 0 && previous_lts_base() == source.base();
  }

  bool is_calendar() const { return major() >= 26; }

  static constexpr uint encode_version(uint major, uint minor, uint patch) {
    return major * 10000 + minor * 100 + patch;
  }

  /**
    Parse a version string to extract the major, minor and patch from the
    version and encode into version ID. The version 0 is returned in case the
    version string could not be parsed. For example, a version string like
    "9.7.4" will be encoded as the integer 90704.

    @param[in]     version_string   input version string
    @return encoded version ID calculated as 10000 * Major + 100 * Minor + Patch
  */
  static uint version_string_to_id(const std::string &version_string);
};

#endif  // MY_SERVER_VERSION_INCLUDED
