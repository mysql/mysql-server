/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mysys/my_version.cc Functions to compare server version strings and
  and determine if clone should be allowed
*/

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <sstream>
#include <string>

/* Clone-related macros to parse version strings and determine if clone
should be allowed */
/** Type to hold different parts of a version */
struct version_t {
  size_t major;
  size_t minor;
  size_t patch;
  std::string build;

  bool operator<(const version_t &rhs) const {
    if (major != rhs.major) {
      return major < rhs.major;
    }
    if (minor != rhs.minor) {
      return minor < rhs.minor;
    }
    if (patch != rhs.patch) {
      return patch < rhs.patch;
    }
    return false;
  }
};

/* Patch version in 8.0.37 where wl15989 is backported */
constexpr unsigned long CLONE_BACKPORT_VERSION = 37;

/* Encoded version before which clone to next LTS is not supported */
const version_t CLONE_TO_NEXT_LTS_SUPPORT{9, 7, 0, ""};

/** Parse a version string and extract the major, minor, patch and build from
the version. A string of length 0 is filled in case a particular version string
could not be parsed.
@param[in]     version   input version string
@param[in,out] is_valid  true if version string is valid
@return parsed components of the version string.
*/
static version_t parse_version_string(std::string version, bool &is_valid) {
  version_t parsed;
  is_valid = true;
  auto parse_next_part{[&version, &is_valid](char delimiter) -> size_t {
    size_t version_part = 0;
    const auto pos = version.find(delimiter);
    try {
      if (pos != std::string::npos) {
        /* pos + 1 to skip the delimiter*/
        version_part = std::stoul(version.substr(0, pos));
        version.erase(0, pos + 1);
      } else {
        /* store rest of the string and make it empty */
        version_part = std::stoul(version.substr(0, version.length()));
        version.erase(0, version.length());
      }
    } catch (...) {
      is_valid = false;
    }
    return version_part;
  }};

  parsed.major = parse_next_part('.');
  parsed.minor = parse_next_part('.');
  parsed.patch = parse_next_part('-');
  parsed.build = version;
  return parsed;
}

/**
 Compares versions and determine if clone is allowed. Clone is allowed if both
 the donor and recipient have exactly same version string. In version series 8.1
 and above, cloning is allowed if Major and Minor versions match. In 8.0 series,
 clone is allowed if patch version is above clone backport version. In this
 comparison, suffixes are ignored: i.e. 8.0.25 should be the same as
 8.0.25-debug, but 8.0.25 isn't the same as 8.0.251.
 Beyond version 9.7, Cloning is also allowed from a Donor in one LTS to a
 recipient in the next LTS. For example, Cloning from 9.7.x to 10.7.y is allowed
 but not from 10.7.y to 9.7.x
 @param[in] recipient        Recipient's version string
 @param[in] donor            Donor's version string
 @param[in] is_recipient_lts true if recipient is LTS
 @param[in] is_donor_lts     true if donor is LTS
 @return true if cloning is allowed between recipient and donor, false otherwise
 */
bool are_versions_clone_compatible(const std::string &recipient,
                                   const std::string &donor,
                                   const bool is_recipient_lts = false,
                                   const bool is_donor_lts = false) {
  if (recipient == donor) {
    return true;
  }

  bool is_valid_v1, is_valid_v2;
  const auto recipient_version = parse_version_string(recipient, is_valid_v1);
  const auto donor_version = parse_version_string(donor, is_valid_v2);

  if (!is_valid_v1 || !is_valid_v2) {
    return false;
  }

  if (recipient_version.major != donor_version.major) {
    if (!is_recipient_lts || !is_donor_lts) {
      return false;
    }

    if ((recipient_version.major < donor_version.major) ||
        (recipient_version < CLONE_TO_NEXT_LTS_SUPPORT) ||
        (donor_version < CLONE_TO_NEXT_LTS_SUPPORT)) {
      return false;
    }

    if (recipient_version.major == donor_version.major + 1) {
      return true;
    }
    return false;
  }

  if (recipient_version.minor != donor_version.minor) {
    return false;
  }

  if ((recipient_version.major == 8) && (recipient_version.minor == 0)) {
    /* Specific checks for clone across 8.0 series */
    try {
      return ((recipient_version.patch == donor_version.patch) ||
              (recipient_version.patch >= CLONE_BACKPORT_VERSION &&
               donor_version.patch >= CLONE_BACKPORT_VERSION));
    } catch (...) {
      return false;
    }
  }
  return true;
}
