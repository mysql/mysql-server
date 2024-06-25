/* Copyright (c) 2024, Oracle and/or its affiliates.

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
#include <string>

/* Clone-related macros to parse version strings and determine if clone
should be allowed */
/** Size of the parsed version strings array */
constexpr unsigned int CLONE_PARSE_ARRAY_SIZE = 4;
/** Parsed version strings array type */
typedef std::array<std::string, CLONE_PARSE_ARRAY_SIZE> ParseArray;

/** Index of the array correpsonding to parts of version */
constexpr unsigned int MAJOR = 0;
constexpr unsigned int MINOR = 1;
constexpr unsigned int PATCH = 2;
constexpr unsigned int BUILD = 3;

/* Patch version in 8.0.37 where wl15989 is backported */
constexpr unsigned long CLONE_BACKPORT_VERSION = 37;

/** Helper function to determine if version string is made up of digits only
@param[in] version version string being validated
@return true if the non-empty string only contains digits */
static inline bool is_valid_version(const std::string &version) {
  return !version.empty() &&
         std::find_if(version.begin(), version.end(), [](unsigned char c) {
           return !std::isdigit(c);
         }) == version.end();
}

/** Parse a version string into an array of strings corresponding to the MAJOR,
MINOR, PATCH and BUILD versions. A string of length 0 is filled in case a
particular version string could not be parsed. For example,
  "Major.Minor.Patch-Build" yields ["Major", "Minor", "Patch", "Build"],
  "8.0.23-SR1"              yields ["8", "0", "23", "SR1"],
@param[in]     version   input version string
@param[in,out] is_valid  true if version string is valid
@return an array of strings containing the parsed components of the version
string.
*/
static ParseArray parse_version_string(std::string version, bool &is_valid) {
  ParseArray parsed;
  is_valid = true;
  auto parse_next_part{
      [&parsed, &version, &is_valid](size_t index, char delimiter) {
        const auto pos = version.find(delimiter);
        if (pos != std::string::npos) {
          /* pos + 1 to skip the delimiter*/
          parsed[index] = version.substr(0, pos);
          version.erase(0, pos + 1);
        } else {
          /* unable to parse, store rest of the string and make it empty */
          parsed[index] = version.substr(0, version.length());
          version.erase(0, version.length());
        }
        /* retain invalidity if any of the version strings were invalid */
        is_valid = is_valid && is_valid_version(parsed[index]);
      }};

  parse_next_part(MAJOR, '.');
  parse_next_part(MINOR, '.');
  parse_next_part(PATCH, '-');
  parsed[BUILD] = version;
  return parsed;
}

/**
 Compares versions and determine if clone is allowed. Clone is allowed if both
 the donor and recipient have exactly same version string. In version series 8.1
 and above, cloning is allowed if Major and Minor versions match. In 8.0 series,
 clone is allowed if patch version is above clone backport version. In this
 comparison, suffixes are ignored: i.e. 8.0.25 should be the same as
 8.0.25-debug, but 8.0.25 isn't the same as 8.0.251
 @param ver1 version1 string
 @param ver2 version2 string
 @return true if cloning is allowed between ver1 and ver2, false otherwise
 */
bool are_versions_clone_compatible(std::string ver1, std::string ver2) {
  if (ver1 == ver2) {
    return true;
  }

  bool is_valid_v1, is_valid_v2;
  const auto parse_v1 = parse_version_string(ver1, is_valid_v1);
  const auto parse_v2 = parse_version_string(ver2, is_valid_v2);

  if (!is_valid_v1 || !is_valid_v2) {
    return false;
  }

  if ((parse_v1[MAJOR] != parse_v2[MAJOR]) ||
      (parse_v1[MINOR] != parse_v2[MINOR])) {
    return false;
  }

  if ((parse_v1[MAJOR] == "8") && (parse_v1[MINOR] == "0")) {
    /* Specific checks for clone across 8.0 series */
    try {
      return ((parse_v1[PATCH] == parse_v2[PATCH]) ||
              (std::stoul(parse_v1[PATCH]) >= CLONE_BACKPORT_VERSION &&
               std::stoul(parse_v2[PATCH]) >= CLONE_BACKPORT_VERSION));
    } catch (...) {
      return false;
    }
  }
  return true;
}
