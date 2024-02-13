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

#ifndef COMPATIBILITY_MODULE_INCLUDED
#define COMPATIBILITY_MODULE_INCLUDED

#include <map>
#include <set>

#include "plugin/group_replication/include/member_version.h"

/* Possible outcomes when checking inter version compatibility */
typedef enum st_compatibility_types {
  INCOMPATIBLE = 0,            // Versions not compatible
  INCOMPATIBLE_LOWER_VERSION,  // Versions not compatible, member has lower
                               // version
  COMPATIBLE,                  // Versions compatible
  READ_COMPATIBLE              // Member can read but not write
} Compatibility_type;

class Compatibility_module {
 public:
  Compatibility_module();
  Compatibility_module(Member_version &local_version);

  /**
    Returns this member version
  */
  Member_version &get_local_version();

  /**
    Set the module local version
    @param local_version the new local version to be set
  */
  void set_local_version(Member_version &local_version);

  /**
    Add a incompatibility between the given members.
    @param from  The member that is not compatible with 'to'
    @param to    The member with which 'from' is not compatible with
  */
  void add_incompatibility(Member_version &from, Member_version &to);

  /**
    Add a incompatibility between a range of member versions.
    @param from   The member that is not compatible with 'to'
    @param to_min The minimum version with which 'from' is not compatible with
    @param to_max The maximum version with which 'from' is not compatible with
  */
  void add_incompatibility(Member_version &from, Member_version &to_min,
                           Member_version &to_max);

  /**
    Checks if the given version is incompatible with another version.
    @param from  The member that may be not compatible with 'to'
    @param to    The member with which 'from' may be not compatible with
    @param do_version_check If version compatibility check is needed
    @param all_members_versions The version of all members on the group
    @return the compatibility status
      @retval INCOMPATIBLE     The versions are not compatible with each other
      @retval COMPATIBLE       The versions are compatible with each other
      @retval READ_COMPATIBLE  The version 'from' can only read from 'to'
  */
  Compatibility_type check_incompatibility(
      Member_version &from, Member_version &to, bool do_version_check,
      const std::set<Member_version> &all_members_versions);

  /**
    Checks if the given version is incompatible with another version.
    @param from  The member that may be not compatible with 'to'
    @param to    The member with which 'from' may be not compatible with
    @return the compatibility status
      @retval INCOMPATIBLE     The versions are not compatible with each other
      @retval COMPATIBLE       The versions are compatible with each other
      @retval READ_COMPATIBLE  The version 'from' can only read from 'to'
  */
  static Compatibility_type check_version_incompatibility(Member_version from,
                                                          Member_version to);
  /**
    Checks if the given version is incompatible with another version.
    @param from  The member that may be not compatible with 'to'
    @param to_min The minimum version with which 'from' is not compatible with
    @param to_max The maximum version with which 'from' is not compatible with

    @return the compatibility status
      @retval true   The version is in the range of the incompatible versions
      @retval false  The version is not in the range of the incompatible
    versions
  */
  bool check_version_range_incompatibility(Member_version &from,
                                           unsigned int to_min,
                                           unsigned int to_max);
  /**
    Checks if the given version is compatible with this member local version.
    @param to    The member with which 'from' may be not compatible with
    @param is_lowest_version If to version is lowest in the group
    @param all_members_versions The version of all members on the group
    @return the compatibility status
      @retval INCOMPATIBLE     The versions are not compatible with each other
      @retval COMPATIBLE       The versions are compatible with each other
      @retval READ_COMPATIBLE  The version 'from' can only read from 'to'
  */
  Compatibility_type check_local_incompatibility(
      Member_version &to, bool is_lowest_version,
      const std::set<Member_version> &all_members_versions);

  /**
    Checks if the provided versions belong to the same LTS version
    @param all_members_versions  The versions

      @retval true   All versions belong to the same LTS version
      @retval false  Otherwise
  */
  static bool do_all_versions_belong_to_the_same_lts(
      const std::set<Member_version> &all_members_versions);

  virtual ~Compatibility_module();

 private:
  /*The configured local version*/
  Member_version *local_version;
  /*
    The incompatibility matrix: <version V, version incompatible with Vmin to
    Vmax>
  */
  std::multimap<unsigned int, std::pair<unsigned int, unsigned int>>
      incompatibilities;

  /**
    Checks if the version is 8.4.X.
    @param version  A server version

      @retval true   The version is 8.4.X
      @retval false  Otherwise
  */
  static bool is_version_8_4_lts(const Member_version &version);
};

#endif /* COMPATIBILITY_MODULE_INCLUDED */
