/*
  Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "filesystem_utils.h"

#include <fstream>
#include <sstream>

#include <gmock/gmock.h>

#include "gtest_consoleoutput.h"
#include "mysql/harness/access_rights.h"
#include "mysql/harness/filesystem.h"

/** @file
 * Stuff here could be considered an extension of stuff in
 * mysql/harness/filesystem.h, except stuff here is only used to help in
 * testing, and hasn't been tested itself.
 */

namespace {

#ifdef _WIN32

void check_ace_access_rights_local_service(
    const std::string &file_name,
    const mysql_harness::win32::access_rights::AccessAllowedAce &access_ace,
    const bool read_only) {
  if (access_ace.mask() & (FILE_EXECUTE)) {
    FAIL() << "Invalid file access rights for file " << file_name
           << " (Execute privilege granted to Local Service user).";
  }

  const auto read_perm = FILE_READ_DATA | FILE_READ_EA | FILE_READ_ATTRIBUTES;
  if ((access_ace.mask() & read_perm) != read_perm) {
    FAIL() << "Invalid file access rights for file " << file_name
           << "(Read privilege for Local Service user missing).";
  }

  const auto write_perm =
      FILE_WRITE_DATA | FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES;
  if (read_only) {
    if ((access_ace.mask() & write_perm) != 0) {
      FAIL() << "Invalid file access rights for file " << file_name
             << "(Write privilege for Local Service user not expected).";
    }
  } else {
    if ((access_ace.mask() & write_perm) != write_perm) {
      FAIL() << "Invalid file access rights for file " << file_name
             << "(Write privilege for Local Service user missing).";
    }
  }
}

void check_acl_access_rights_local_service(const std::string &file_name,
                                           ACL *dacl, const bool read_only) {
  auto sid_res = mysql_harness::win32::access_rights::create_well_known_sid(
      WinLocalServiceSid);

  if (!sid_res) {
    auto ec = sid_res.error();

    FAIL() << "getting the sid for 'LocalService' failed :( " << ec;
  }

  mysql_harness::win32::access_rights::Sid local_service_sid{sid_res->get()};

  bool checked_local_service_ace = false;
  for (auto const &ace : mysql_harness::win32::access_rights::Acl(dacl)) {
    if (ace.type() != ACCESS_ALLOWED_ACE_TYPE) continue;

    mysql_harness::win32::access_rights::AccessAllowedAce access_ace(
        static_cast<ACCESS_ALLOWED_ACE *>(ace.data()));

    if (access_ace.sid() == local_service_sid) {
      check_ace_access_rights_local_service(file_name, access_ace, read_only);
      checked_local_service_ace |= true;
    }
  }

  if (!checked_local_service_ace)
    FAIL() << "Permissions not set for user 'LocalService' on file '"
           << file_name << "'.";
}

void check_security_descriptor_access_rights(
    const std::string &file_name,
    mysql_harness::security_descriptor_type &sec_desc, bool read_only) {
  auto optional_dacl_res =
      mysql_harness::win32::access_rights::SecurityDescriptor(sec_desc.get())
          .dacl();

  if (!optional_dacl_res) {
    auto ec = optional_dacl_res.error();

    FAIL() << "getting the dacl failed :( " << ec;
  }

  auto optional_dacl = std::move(optional_dacl_res.value());

  if (!optional_dacl) {
    // No DACL means: no access allowed. That's not good.
    FAIL() << "No access allowed to file: " << file_name;
    return;
  }

  auto *dacl = std::move(optional_dacl.value());

  if (dacl == nullptr) {
    // Empty DACL means: all access allowed.
    FAIL() << "Invalid file " << file_name
           << " access rights "
              "(Everyone has full access rights).";
  }

  check_acl_access_rights_local_service(file_name, dacl, read_only);
}

#endif

}  // namespace

void check_config_file_access_rights(const std::string &file_name,
                                     const bool read_only) {
  auto rights_res = mysql_harness::access_rights_get(file_name);
  if (!rights_res) {
    auto ec = rights_res.error();

    FAIL() << "get-access-rights() failed: " << ec;
  }

#ifdef _WIN32
  check_security_descriptor_access_rights(file_name, rights_res.value(),
                                          read_only);
#else
  (void)read_only;  // unused.

  auto verify_res = mysql_harness::access_rights_verify(
      rights_res.value(), mysql_harness::AllowUserReadWritableVerifier());
  if (!verify_res) {
    FAIL() << "Config file (" << file_name
           << ") has file permissions that are not strict enough"
              " (only RW for file's owner is allowed).";
  }
#endif
}
