/*
  Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "gmock/gmock.h"
#include "gtest_consoleoutput.h"
#include "mysql/harness/filesystem.h"

/** @file
 * Stuff here could be considered an extension of stuff in
 * mysql/harness/filesystem.h, except stuff here is only used to help in
 * testing, and hasn't been tested itself.
 */

namespace {

#ifdef _WIN32

using mysql_harness::SecurityDescriptorPtr;
using mysql_harness::SidPtr;

void check_ace_access_rights_local_service(const std::string &file_name,
                                           ACCESS_ALLOWED_ACE *access_ace,
                                           const bool read_only,
                                           bool &is_local_service_ace) {
  is_local_service_ace = false;
  SID *sid = reinterpret_cast<SID *>(&access_ace->SidStart);
  DWORD sid_size = SECURITY_MAX_SID_SIZE;
  SidPtr local_service_sid(static_cast<SID *>(std::malloc(sid_size)));

  if (CreateWellKnownSid(WinLocalServiceSid, nullptr, local_service_sid.get(),
                         &sid_size) == FALSE) {
    throw std::runtime_error("CreateWellKnownSid() failed: " +
                             std::to_string(GetLastError()));
  }

  if (EqualSid(sid, local_service_sid.get())) {
    if (access_ace->Mask & (FILE_EXECUTE)) {
      FAIL() << "Invalid file access rights for file " << file_name
             << " (Execute privilege granted to Local Service user).";
    }

    const auto read_perm = FILE_READ_DATA | FILE_READ_EA | FILE_READ_ATTRIBUTES;
    if ((access_ace->Mask & read_perm) != read_perm) {
      FAIL() << "Invalid file access rights for file " << file_name
             << "(Read privilege for Local Service user missing).";
    }

    const auto write_perm =
        FILE_WRITE_DATA | FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES;
    if (read_only) {
      if ((access_ace->Mask & write_perm) != 0) {
        FAIL() << "Invalid file access rights for file " << file_name
               << "(Write privilege for Local Service user not expected).";
      }
    } else {
      if ((access_ace->Mask & write_perm) != write_perm) {
        FAIL() << "Invalid file access rights for file " << file_name
               << "(Write privilege for Local Service user missing).";
      }
    }

    is_local_service_ace = true;
  }
}

void check_acl_access_rights_local_service(const std::string &file_name,
                                           ACL *dacl, const bool read_only) {
  ACL_SIZE_INFORMATION dacl_size_info;

  if (GetAclInformation(dacl, &dacl_size_info, sizeof(dacl_size_info),
                        AclSizeInformation) == FALSE) {
    throw std::runtime_error("GetAclInformation() failed: " +
                             std::to_string(GetLastError()));
  }

  bool checked_local_service_ace = false;
  for (DWORD ace_idx = 0; ace_idx < dacl_size_info.AceCount; ++ace_idx) {
    LPVOID ace = nullptr;

    if (GetAce(dacl, ace_idx, &ace) == FALSE) {
      throw std::runtime_error("GetAce() failed: " +
                               std::to_string(GetLastError()));
      continue;
    }

    if (static_cast<ACE_HEADER *>(ace)->AceType == ACCESS_ALLOWED_ACE_TYPE) {
      bool is_local_service_ace;
      check_ace_access_rights_local_service(
          file_name, static_cast<ACCESS_ALLOWED_ACE *>(ace), read_only,
          is_local_service_ace);
      checked_local_service_ace |= is_local_service_ace;
    }
  }

  if (!checked_local_service_ace)
    FAIL() << "Permissions not set for user 'LocalService' on file '"
           << file_name << "'.";
}

void check_security_descriptor_access_rights(const std::string &file_name,
                                             SecurityDescriptorPtr &sec_desc,
                                             bool read_only) {
  BOOL dacl_present;
  ACL *dacl;
  BOOL dacl_defaulted;

  if (GetSecurityDescriptorDacl(sec_desc.get(), &dacl_present, &dacl,
                                &dacl_defaulted) == FALSE) {
    throw std::runtime_error("GetSecurityDescriptorDacl() failed: " +
                             std::to_string(GetLastError()));
  }

  if (!dacl_present) {
    // No DACL means: no access allowed. That's not good.
    FAIL() << "No access allowed to file: " << file_name;
    return;
  }

  if (!dacl) {
    // Empty DACL means: all access allowed.
    FAIL() << "Invalid file " << file_name
           << " access rights "
              "(Everyone has full access rights).";
  }

  check_acl_access_rights_local_service(file_name, dacl, read_only);
}

#endif

}  // namespace {

void check_config_file_access_rights(const std::string &file_name,
                                     const bool read_only) {
#ifdef _WIN32
  SecurityDescriptorPtr sec_descr;
  try {
    sec_descr = mysql_harness::get_security_descriptor(file_name);
  } catch (const std::system_error &) {
    // that means that the system does not support ACL, in that case nothing
    // really to check
    return;
  }
  check_security_descriptor_access_rights(file_name, sec_descr, read_only);
#else
  // on other OSes we ALWAYS expect 600 access rights for the config file
  // weather it's static or dynamic one
  (void)read_only;
  struct stat status;

  if (stat(file_name.c_str(), &status) != 0) {
    if (errno == ENOENT) return;
    FAIL() << "stat() failed (" << file_name
           << "): " << mysql_harness::get_strerror(errno);
  }

  static constexpr mode_t kFullAccessMask = (S_IRWXU | S_IRWXG | S_IRWXO);
  static constexpr mode_t kRequiredAccessMask = (S_IRUSR | S_IWUSR);

  if ((status.st_mode & kFullAccessMask) != kRequiredAccessMask)
    FAIL() << "Config file (" << file_name
           << ") has file permissions that are not strict enough"
              " (only RW for file's owner is allowed).";
#endif
}
