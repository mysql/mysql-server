/* Copyright (c) 2015, 2026, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/compatibility_module.h"
#include "plugin/group_replication/include/plugin_constants.h"

#include <algorithm>
#include <cassert>
#include <cstddef>

Compatibility_module::Compatibility_module() : local_version(nullptr) {}

Compatibility_module::Compatibility_module(Member_version &local_version) {
  this->local_version = new Member_version(local_version.get_version());
}

Member_version &Compatibility_module::get_local_version() {
  return *this->local_version;
}

void Compatibility_module::set_local_version(Member_version &local_version) {
  delete this->local_version;
  this->local_version = new Member_version(local_version.get_version());
}

void Compatibility_module::add_incompatibility(Member_version &from,
                                               Member_version &to) {
  this->incompatibilities.insert(std::make_pair(
      from.get_version(), std::make_pair(to.get_version(), to.get_version())));
}

void Compatibility_module::add_incompatibility(Member_version &from,
                                               Member_version &to_min,
                                               Member_version &to_max) {
  assert(to_min.get_version() <= to_max.get_version());
  this->incompatibilities.insert(std::make_pair(
      from.get_version(),
      std::make_pair(to_min.get_version(), to_max.get_version())));
}

Compatibility_type Compatibility_module::check_local_incompatibility(
    Member_version &to, bool is_lowest_version,
    const std::set<Member_version> &all_members_versions) {
  return check_incompatibility(get_local_version(), to, is_lowest_version,
                               all_members_versions);
}

bool Compatibility_module::check_version_range_incompatibility(
    Member_version &from, unsigned int to_min, unsigned int to_max) {
  unsigned int const to_max_major_version = to_max >> 16;
  unsigned int const to_min_major_version = to_min >> 16;
  // if it is outside the range of the major version
  if (from.get_major_version() > to_max_major_version ||
      from.get_major_version() < to_min_major_version)
    return false;

  unsigned int const to_max_minor_version = (to_max >> 8) & 0xff;
  unsigned int const to_min_minor_version = (to_min >> 8) & 0xff;
  // if it is outside the range of the minor version
  if (from.get_minor_version() > to_max_minor_version ||
      from.get_minor_version() < to_min_minor_version)
    return false; /* purecov: inspected */

  unsigned int const to_max_patch_version = to_max & 0xff;
  unsigned int const to_min_patch_version = to_min & 0xff;
  // if it is outside the range of the patch version
  if (from.get_patch_version() > to_max_patch_version ||
      from.get_patch_version() < to_min_patch_version)
    return false;

  return true;
}

Compatibility_type Compatibility_module::check_incompatibility(
    Member_version &from, Member_version &to, bool do_version_check,
    const std::set<Member_version> &all_members_versions) {
  if (from == to) return COMPATIBLE;

  // Find if the values are present in the statically defined table...
  std::pair<std::multimap<unsigned int,
                          std::pair<unsigned int, unsigned int>>::iterator,
            std::multimap<unsigned int,
                          std::pair<unsigned int, unsigned int>>::iterator>
      search_its;

  search_its = this->incompatibilities.equal_range(from.get_version());
  for (auto it = search_its.first; it != search_its.second; ++it) {
    if (check_version_range_incompatibility(to, it->second.first,
                                            it->second.second)) {
      return INCOMPATIBLE;
    }
  }

  // It was not deemed incompatible by the table rules

  /*
    We can only check LTS compatibility rules after checking if the
    joining member was not explicitly made incompatible.
  */
  if (do_all_versions_belong_to_the_same_lts(all_members_versions)) {
    return COMPATIBLE;
  }

  /**
    We have already confirmed versions are not same.
    If joinee version is higher then group lowest version, its read compatible
    else joinee version is INCOMPATIBLE with the group.
   */
  if (do_version_check) {
    return check_version_incompatibility(from, to);
  }
  return COMPATIBLE;
}

bool Compatibility_module::do_all_versions_belong_to_the_same_lts(
    const std::set<Member_version> &all_members_versions) {
  if (all_members_versions.empty()) {
    return false;
  }

  const auto &first = *all_members_versions.begin();
  return std::all_of(
      all_members_versions.begin(), all_members_versions.end(),
      [&](const Member_version &version) {
        return is_lts_version(version) &&
               version.get_major_version() == first.get_major_version() &&
               version.get_minor_version() == first.get_minor_version();
      });
}

bool Compatibility_module::is_lts_version(const Member_version &version) {
  return version.get_major_version() >= 9 && version.get_minor_version() == 7;
}

/* Compatibility_module is independent, we cannot use local_member_info or
 * group_mgr. */
Compatibility_type Compatibility_module::check_version_incompatibility(
    Member_version from, Member_version to) {
  return (from == to)
             ? COMPATIBLE
             : ((from > to) ? READ_COMPATIBLE : INCOMPATIBLE_LOWER_VERSION);
}

Compatibility_module::~Compatibility_module() { delete this->local_version; }
