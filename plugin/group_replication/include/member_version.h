/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#ifndef MEMBER_VERSION_INCLUDED
#define MEMBER_VERSION_INCLUDED

#include <string>

#include "my_inttypes.h"

/**
 * @brief represent the MySQL version of a Member within the Group Replication
 * group.
 */
class Member_version {
 public:
  Member_version(unsigned int version);

  /**
    @return returns the member version.
  */
  uint32 get_version() const;

  /**
    @return returns the member version in string format.
  */
  const std::string get_version_string() const;
  /**
    @return returns the major version (Major.v.v)
  */
  uint32 get_major_version() const;
  /**
    @return returns the minor version (v.Minor.v)
  */
  uint32 get_minor_version() const;
  /**
    @return returns the minor version (v.v.Patch)
  */
  uint32 get_patch_version() const;

  bool operator==(const Member_version &other) const;
  bool operator!=(const Member_version &other) const;
  bool operator<(const Member_version &other) const;
  bool operator>(const Member_version &other) const;
  bool operator>=(const Member_version &other) const;
  bool operator<=(const Member_version &other) const;

 private:
  uint32 version;
};

#endif /* MEMBER_VERSION_INCLUDED */
