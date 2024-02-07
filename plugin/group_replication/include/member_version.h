/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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
  /**
    Constructs the Member_version object from a hexadecimal (base 16)
    representation.
    Example: 8.0.1 -> 0x080001

    @param[in] version hexadecimal (base 16) number
   */
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

#ifndef NDEBUG
  /**
    Increment by 1 the major version.
   */
  void increment_major_version();

  /**
    Decrement by 1 the major version.
   */
  void decrement_major_version();

  /**
    Increment by 1 the minor version.

    @note If the minor version is 99 before the increment, the minor version
    becomes 0 and the major version is incremented by 1.
   */
  void increment_minor_version();

  /**
    Decrement by 1 the minor version.

    @note If the minor version is 0 before the increment, the minor version
    becomes 99 and the major version is decremented by 1.
   */
  void decrement_minor_version();

  /**
    Increment by 1 the patch version.

    @note If the patch version is 99 before the increment, the patch version
    becomes 0 and the minor version is incremented by 1 (which may increment by
    1 the major version).
   */
  void increment_patch_version();

  /**
    Decrement by 1 the patch version.

    @note If the patch version is 0 before the increment, the patch version
    becomes 99 and the minor version is decremented by 1 (which may decrement by
    1 the major version).
   */
  void decrement_patch_version();
#endif /* NDEBUG */

 private:
  uint32 version;

#ifndef NDEBUG
  /**
    Return the major version in decimal (base 10) representation.

    @return major version in decimal (base 10) format representation
   */
  uint8_t get_major_version_decimal() const;

  /**
    Return the minor version in decimal (base 10) representation.

    @return minor version in decimal (base 10) format representation
   */
  uint8_t get_minor_version_decimal() const;

  /**
    Return the patch version in decimal (base 10) representation.

    @return patch version in decimal (base 10) format representation
   */
  uint8_t get_patch_version_decimal() const;

  /**
    Update the version from a decimal (base 10) representation.

    @param[in] major major version in decimal (base 10) representation
    @param[in] minor minor version in decimal (base 10) representation
    @param[in] patch patch version in decimal (base 10) representation
   */
  void update_version_decimal(const uint8_t major, const uint8_t minor,
                              const uint8_t patch);
#endif /* NDEBUG */
};

#endif /* MEMBER_VERSION_INCLUDED */
