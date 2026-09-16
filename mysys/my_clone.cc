/* Copyright (c) 2026 Oracle and/or its affiliates.

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
  @file mysys/my_clone.cc Functions to compare server version strings and
  and determine if clone should be allowed
*/

#include "my_clone.h"
#include "my_server_version.h"

const char *CLONE_BACKPORT_VERSION_STRING = "8.0.37";
const char *CLONE_TO_NEXT_LTS_SUPPORT_STRING = "9.7.0";

/* Version 8.0.37 where wl15989 is backported */
const My_server_version CLONE_BACKPORT_VERSION{
    .version =
        My_server_version::version_string_to_id(CLONE_BACKPORT_VERSION_STRING)};

/* Version 9.7.0 before which clone to next LTS is not supported */
const My_server_version CLONE_TO_NEXT_LTS_SUPPORT{
    .version = My_server_version::version_string_to_id(
        CLONE_TO_NEXT_LTS_SUPPORT_STRING)};

bool are_versions_clone_compatible(
    const std::string &recipient, const std::string &donor,
    const bool is_recipient_lts, const bool is_donor_lts,
    const std::optional<std::string> &recipient_prev_lts) {
  if (recipient == donor) {
    return true;
  }

  My_server_version recipient_version{
      .version = My_server_version::version_string_to_id(recipient),
      .is_lts = is_recipient_lts};

  const My_server_version donor_version{
      .version = My_server_version::version_string_to_id(donor),
      .is_lts = is_donor_lts};

  if (recipient_version.major() == 0 || donor_version.major() == 0) {
    return false;
  }

  if (recipient_version.base() == donor_version.base()) {
    if (recipient_version.base() == CLONE_BACKPORT_VERSION.base()) {
      /* Specific checks for clone across 8.0 series */
      return (recipient_version.patch() == donor_version.patch()) ||
             (recipient_version >= CLONE_BACKPORT_VERSION &&
              donor_version >= CLONE_BACKPORT_VERSION);
    }
    /* Ignore patch if Major and Minor match */
    return true;
  }

  if (!recipient_version.is_lts || !donor_version.is_lts ||
      !recipient_prev_lts) {
    return false;
  }

  if ((recipient_version < donor_version) ||
      (recipient_version < CLONE_TO_NEXT_LTS_SUPPORT) ||
      (donor_version < CLONE_TO_NEXT_LTS_SUPPORT)) {
    return false;
  }

  recipient_version.previous_lts =
      My_server_version::version_string_to_id(*recipient_prev_lts);
  return recipient_version.starts_lts_lineage_after(donor_version);
}
