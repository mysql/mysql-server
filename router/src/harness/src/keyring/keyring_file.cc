/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "keyring/keyring_file.h"
#include <string.h>
#include <fstream>
#include <memory>
#include <system_error>
#include "common.h"

#ifdef _WIN32
#include <aclapi.h>
#else
#include <sys/stat.h>
#endif

constexpr const char kKeyringFileSignature[] = {'M', 'R', 'K', 'R'};

#ifdef _WIN32

// Smart pointers for WinAPI structures that use C-style memory management.
using SecurityDescriptorPtr =
    std::unique_ptr<SECURITY_DESCRIPTOR,
                    mysql_harness::StdFreeDeleter<SECURITY_DESCRIPTOR>>;
using SidPtr = std::unique_ptr<SID, mysql_harness::StdFreeDeleter<SID>>;

/**
 * Retrieves file's DACL security descriptor.
 *
 * @param[in] file_name File name.
 *
 * @return File's DACL security descriptor.
 *
 * @throw std::exception Failed to retrieve security descriptor.
 */
static SecurityDescriptorPtr get_security_descriptor(
    const std::string &file_name) {
  static constexpr SECURITY_INFORMATION kReqInfo = DACL_SECURITY_INFORMATION;

  // Get the size of the descriptor.
  DWORD sec_desc_size;

  if (GetFileSecurityA(file_name.c_str(), kReqInfo, nullptr, 0,
                       &sec_desc_size) == FALSE) {
    // calling code checks for errno
    // also multiple calls to GetLastError() erase error value
    errno = GetLastError();

    // We expect to receive `ERROR_INSUFFICIENT_BUFFER`.
    if (errno != ERROR_INSUFFICIENT_BUFFER) {
      throw std::system_error(errno, std::system_category(),
                              "GetFileSecurity() failed (" + file_name +
                                  "): " + std::to_string(errno));
    }
  }

  SecurityDescriptorPtr sec_desc(
      static_cast<SECURITY_DESCRIPTOR *>(std::malloc(sec_desc_size)));

  if (GetFileSecurityA(file_name.c_str(), kReqInfo, sec_desc.get(),
                       sec_desc_size, &sec_desc_size) == FALSE) {
    errno = GetLastError();
    throw std::system_error(errno, std::system_category(),
                            "GetFileSecurity() failed (" + file_name +
                                "): " + std::to_string(GetLastError()));
  }

  return sec_desc;
}

/**
 * Verifies permissions of an access ACE entry.
 *
 * @param[in] access_ace Access ACE entry.
 *
 * @throw std::exception Everyone has access to the ACE access entry or
 *                        an error occured.
 */
static void check_ace_access_rights(ACCESS_ALLOWED_ACE *access_ace) {
  SID *sid = reinterpret_cast<SID *>(&access_ace->SidStart);
  DWORD sid_size = SECURITY_MAX_SID_SIZE;
  SidPtr everyone_sid(static_cast<SID *>(std::malloc(sid_size)));

  if (CreateWellKnownSid(WinWorldSid, nullptr, everyone_sid.get(), &sid_size) ==
      FALSE) {
    throw std::runtime_error("CreateWellKnownSid() failed: " +
                             std::to_string(GetLastError()));
  }

  if (EqualSid(sid, everyone_sid.get())) {
    if (access_ace->Mask & (FILE_EXECUTE)) {
      throw std::runtime_error(
          "Invalid keyring file access rights "
          "(Execute privilege granted to Everyone).");
    }
    if (access_ace->Mask &
        (FILE_WRITE_DATA | FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES)) {
      throw std::runtime_error(
          "Invalid keyring file access rights "
          "(Write privilege granted to Everyone).");
    }
    if (access_ace->Mask &
        (FILE_READ_DATA | FILE_READ_EA | FILE_READ_ATTRIBUTES)) {
      throw std::runtime_error(
          "Invalid keyring file access rights "
          "(Read privilege granted to Everyone).");
    }
  }
}

/**
 * Verifies access permissions in a DACL.
 *
 * @param[in] dacl DACL to be verified.
 *
 * @throw std::exception DACL contains an ACL entry that grants full access to
 *                        Everyone or an error occured.
 */
static void check_acl_access_rights(ACL *dacl) {
  ACL_SIZE_INFORMATION dacl_size_info;

  if (GetAclInformation(dacl, &dacl_size_info, sizeof(dacl_size_info),
                        AclSizeInformation) == FALSE) {
    throw std::runtime_error("GetAclInformation() failed: " +
                             std::to_string(GetLastError()));
  }

  for (DWORD ace_idx = 0; ace_idx < dacl_size_info.AceCount; ++ace_idx) {
    LPVOID ace = nullptr;

    if (GetAce(dacl, ace_idx, &ace) == FALSE) {
      throw std::runtime_error("GetAce() failed: " +
                               std::to_string(GetLastError()));
      continue;
    }

    if (static_cast<ACE_HEADER *>(ace)->AceType == ACCESS_ALLOWED_ACE_TYPE)
      check_ace_access_rights(static_cast<ACCESS_ALLOWED_ACE *>(ace));
  }
}

/**
 * Verifies access permissions in a security descriptor.
 *
 * @param[in] sec_desc Security descriptor to be verified.
 *
 * @throw std::exception Security descriptor grants full access to
 *                        Everyone or an error occured.
 */
static void check_security_descriptor_access_rights(
    SecurityDescriptorPtr sec_desc) {
  BOOL dacl_present;
  ACL *dacl;
  BOOL dacl_defaulted;

  if (GetSecurityDescriptorDacl(sec_desc.get(), &dacl_present, &dacl,
                                &dacl_defaulted) == FALSE) {
    throw std::runtime_error("GetSecurityDescriptorDacl() failed: " +
                             std::to_string(GetLastError()));
  }

  if (!dacl_present) {
    // No DACL means: no access allowed. Which is fine.
    return;
  }

  if (!dacl) {
    // Empty DACL means: all access allowed.
    throw std::runtime_error(
        "Invalid keyring file access rights "
        "(Everyone has full access rights).");
  }

  check_acl_access_rights(dacl);
}

#endif  // _WIN32

/**
 * Verifies access permissions of a file.
 *
 * On Unix systems it throws if file's permissions differ from 600.
 * On Windows it throws if file can be accessed by Everyone group.
 *
 * @param[in] file_name File to be verified.
 *
 * @throw std::exception File access rights are too permissive or
 *                        an error occured.
 * @throw std::system_error OS and/or filesystem doesn't support file
 *                           permissions.
 */
static void check_file_access_rights(const std::string &file_name) {
#ifdef _WIN32
  check_security_descriptor_access_rights(get_security_descriptor(file_name));
#else
  struct stat status;

  if (stat(file_name.c_str(), &status) != 0) {
    if (errno == ENOENT) return;
    throw std::runtime_error("stat() failed (" + file_name +
                             "): " + mysql_harness::get_strerror(errno));
  }

  static constexpr mode_t kFullAccessMask = (S_IRWXU | S_IRWXG | S_IRWXO);
  static constexpr mode_t kRequiredAccessMask = (S_IRUSR | S_IWUSR);

  if ((status.st_mode & kFullAccessMask) != kRequiredAccessMask)
    throw std::runtime_error("Keyring file (" + file_name +
                             ") has file permissions that are not strict enough"
                             " (only RW for file's owner is allowed).");

#endif  // _WIN32
}

namespace mysql_harness {

void KeyringFile::set_header(const std::string &data) { header_ = data; }

void KeyringFile::save(const std::string &file_name,
                       const std::string &key) const {
  if (key.empty()) {
    throw std::runtime_error("Keyring encryption key must not be blank");
  }
  // Serialize keyring.
  auto buffer = serialize(key);

  // Save keyring data to file.
  std::ofstream file;

  file.exceptions(std::ofstream::failbit | std::ofstream::badbit);

#ifndef _WIN32
  try {
    file.open(file_name, std::ofstream::out | std::ofstream::binary |
                             std::ofstream::trunc);
  } catch (std::exception &e) {
    throw std::runtime_error(
        std::string("Failed to open keyring file for writing: ") + file_name +
        ": " + get_strerror(errno));
  }
#else
  // For Microsoft Windows, on repeated saving of files (like our unit tests)
  // the file opening sometimes fails with "Access Denied", since it works fine
  // when disabling indexing of file contents for the whole folder we assume the
  // indexer is not releasing the file fast enough. So here we simply retry the
  // opening of the file.
  int retries = 5;
  do {
    try {
      file.open(file_name, std::ofstream::out | std::ofstream::binary |
                               std::ofstream::trunc);
      break;
    } catch (const std::exception &) {
      if (retries-- > 0) {
        Sleep(100);
        continue;
      }
      throw std::runtime_error(
          std::string("Failed to open keyring file for writing: ") + file_name +
          ": " + get_strerror(errno));
    }
  } while (true);
#endif

  try {
    make_file_private(file_name);
  } catch (const std::system_error &e) {
#ifdef _WIN32
    if (e.code() !=
        std::error_code(ERROR_INVALID_FUNCTION, std::system_category()))
    // if the filesystem can't set permissions, ignore it
#endif
      throw;
  }

  try {
    // write signature
    file.write(kKeyringFileSignature, sizeof(kKeyringFileSignature));
    // write header
    uint32_t header_size = static_cast<uint32_t>(header_.size());
    file.write(reinterpret_cast<char *>(&header_size), sizeof(header_size));
    if (header_.size() > 0)
      file.write(header_.data(), static_cast<std::streamsize>(header_.size()));
    // write data
    file.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();
  } catch (std::exception &e) {
    throw std::runtime_error(std::string("Failed to save keyring file: ") +
                             e.what());
  }
}

static void verify_file_permissions(const std::string &file_name) {
  // Verify keyring file's access permissions.
  try {
    // throws std::system_error if permissions are not supported by OS and/or
    // filesystem throws std::runtime_error on bad permissions or error in
    // retrieval file not existing is ok
    check_file_access_rights(file_name);

  } catch (const std::system_error &e) {
#ifdef _WIN32
    if (e.code() !=
        std::error_code(ERROR_INVALID_FUNCTION, std::system_category()))
    // if the filesystem can't set permissions, ignore it
#endif
      throw;
  }
}

void KeyringFile::load(const std::string &file_name, const std::string &key) {
  // throws std::runtime_error with appropriate error message on verification
  // failure
  verify_file_permissions(file_name);

  // Read keyring data from file.
  std::ifstream file;

  file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  try {
    file.open(file_name,
              std::ifstream::in | std::ifstream::binary | std::ifstream::ate);
  } catch (const std::exception &) {
    throw std::runtime_error(std::string("Failed to load keyring file: ") +
                             file_name + ": " + get_strerror(errno));
  }

  file.seekg(0, file.end);
  std::size_t file_size = static_cast<std::size_t>(file.tellg());

  // read and check signature
  file.seekg(0, file.beg);
  {
    char sig[sizeof(kKeyringFileSignature)];
    try {
      file.read(sig, sizeof(sig));
    } catch (const std::ios_base::failure &e) {
      throw std::runtime_error("Failure reading contents of keyring file " +
                               file_name);
    }
    if (strncmp(sig, kKeyringFileSignature, sizeof(kKeyringFileSignature)) != 0)
      throw std::runtime_error("Invalid data found in keyring file " +
                               file_name);
  }
  // read header, if there's one
  {
    uint32_t header_size;
    file.read(reinterpret_cast<char *>(&header_size), sizeof(header_size));
    if (header_size > 0) {
      if (header_size >
          file_size - sizeof(kKeyringFileSignature) - sizeof(header_size)) {
        throw std::runtime_error("Invalid data found in keyring file " +
                                 file_name);
      }
      header_.resize(header_size);
      file.read(&header_[0], static_cast<std::streamsize>(header_.size()));
    }
  }

  std::size_t data_size = file_size - static_cast<std::size_t>(file.tellg());

  std::vector<char> buffer(static_cast<std::size_t>(data_size));
  file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

  // Parse keyring data.
  parse(key, buffer.data(), buffer.size());
}

std::string KeyringFile::read_header(const std::string &file_name) {
  // throws std::runtime_error with appropriate error message on verification
  // failure
  verify_file_permissions(file_name);

  // Read keyring data from file.
  std::ifstream file;

  file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  try {
    file.open(file_name,
              std::ifstream::in | std::ifstream::binary | std::ifstream::ate);
  } catch (const std::exception &) {
    throw std::runtime_error(std::string("Failed to open keyring file: ") +
                             file_name + ": " + get_strerror(errno));
  }

  std::size_t file_size = static_cast<std::size_t>(file.tellg());

  file.seekg(0);
  // read and check signature
  {
    char sig[sizeof(kKeyringFileSignature)];
    file.read(sig, sizeof(sig));
    if (strncmp(sig, kKeyringFileSignature, sizeof(kKeyringFileSignature)) != 0)
      throw std::runtime_error("Invalid data found in keyring file " +
                               file_name);
  }
  // read header, if there's one
  std::string header;
  {
    uint32_t header_size;
    file.read(reinterpret_cast<char *>(&header_size), sizeof(header_size));
    if (header_size > 0) {
      if (header_size >
          file_size - sizeof(kKeyringFileSignature) - sizeof(header_size)) {
        throw std::runtime_error("Invalid data found in keyring file " +
                                 file_name);
      }
      header.resize(header_size);
      file.read(&header[0], static_cast<std::streamsize>(header.size()));
    }
  }
  return header;
}
}  // namespace mysql_harness
