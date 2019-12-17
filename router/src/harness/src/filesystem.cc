/*
  Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/harness/filesystem.h"

#include <cstring>
#include <fstream>
#include <ostream>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#endif

using std::string;

namespace mysql_harness {

#ifndef _WIN32
const perm_mode kStrictDirectoryPerm = S_IRWXU;
#else
const perm_mode kStrictDirectoryPerm = 0;
#endif

////////////////////////////////////////////////////////////////
// class Path members and free functions

Path::Path() noexcept : type_(FileType::EMPTY_PATH) {}

// throws std::invalid_argument
Path::Path(const std::string &path)
    : path_(path), type_(FileType::TYPE_UNKNOWN) {
#ifdef _WIN32
  // in Windows, we normalize directory separator from \ to /, to not
  // confuse the rest of the code, which assume \ to be an escape char
  std::string::size_type p = path_.find('\\');
  while (p != std::string::npos) {
    path_[p] = '/';
    p = path_.find('\\');
  }
#endif
  string::size_type pos = path_.find_last_not_of(directory_separator);
  if (pos != string::npos)
    path_.erase(pos + 1);
  else if (path_.size() > 0)
    path_.erase(1);
  else
    throw std::invalid_argument("Empty path");
}

// throws std::invalid_argument
Path::Path(const char *path) : Path(string(path)) {}

// throws std::invalid_argument
void Path::validate_non_empty_path() const {
  if (!is_set()) {
    throw std::invalid_argument("Empty path");
  }
}

bool Path::operator==(const Path &rhs) const {
  return real_path().str() == rhs.real_path().str();
}

bool Path::operator<(const Path &rhs) const { return path_ < rhs.path_; }

Path Path::basename() const {
  validate_non_empty_path();  // throws std::invalid_argument
  string::size_type pos = path_.find_last_of(directory_separator);
  if (pos == string::npos)
    return *this;
  else if (pos > 1)
    return string(path_, pos + 1);
  else
    return Path(root_directory);
}

Path Path::dirname() const {
  validate_non_empty_path();  // throws std::invalid_argument
  string::size_type pos = path_.find_last_of(directory_separator);
  if (pos == string::npos)
    return Path(".");
  else if (pos > 0)
    return string(path_, 0, pos);
  else
    return Path(root_directory);
}

bool Path::is_directory() const {
  validate_non_empty_path();  // throws std::invalid_argument
  return type() == FileType::DIRECTORY_FILE;
}

bool Path::is_regular() const {
  validate_non_empty_path();  // throws std::invalid_argument
  return type() == FileType::REGULAR_FILE;
}

bool Path::is_absolute() const {
  validate_non_empty_path();  // throws std::invalid_argument
#ifdef _WIN32
  if (path_[0] == '\\' || path_[0] == '/' || path_[1] == ':') return true;
  return false;
#else
  if (path_[0] == '/') return true;
  return false;
#endif
}

bool Path::exists() const {
  validate_non_empty_path();  // throws std::invalid_argument
  return type() != FileType::FILE_NOT_FOUND && type() != FileType::STATUS_ERROR;
}

bool Path::is_readable() const {
  validate_non_empty_path();
  return exists() && std::ifstream(real_path().str()).good();
}

void Path::append(const Path &other) {
  validate_non_empty_path();        // throws std::invalid_argument
  other.validate_non_empty_path();  // throws std::invalid_argument
  path_.append(directory_separator + other.path_);
  type_ = FileType::TYPE_UNKNOWN;
}

Path Path::join(const Path &other) const {
  validate_non_empty_path();        // throws std::invalid_argument
  other.validate_non_empty_path();  // throws std::invalid_argument
  Path result(*this);
  result.append(other);
  return result;
}

std::ostream &operator<<(std::ostream &out, Path::FileType type) {
  static const char *type_names[]{
      "ERROR",        "not found",        "regular", "directory", "symlink",
      "block device", "character device", "FIFO",    "socket",    "UNKNOWN",
  };
  out << type_names[static_cast<int>(type)];
  return out;
}

///////////////////////////////////////////////////////////
// Directory::Iterator members

Directory::DirectoryIterator Directory::begin() {
  return DirectoryIterator(*this);
}

Directory::DirectoryIterator Directory::glob(const string &pattern) {
  return DirectoryIterator(*this, pattern);
}

Directory::DirectoryIterator Directory::end() { return DirectoryIterator(); }

///////////////////////////////////////////////////////////
// Directory members

Directory::~Directory() = default;

// throws std::invalid_argument
Directory::Directory(const Path &path) : Path(path) {}

////////////////////////////////////////////////////////////////////////////////
//
// Utility free functions
//
////////////////////////////////////////////////////////////////////////////////

int delete_dir_recursive(const std::string &dir) noexcept {
  mysql_harness::Directory d(dir);
  try {
    for (auto const &f : d) {
      if (f.is_directory()) {
        if (delete_dir_recursive(f.str()) < 0) return -1;
      } else {
        if (delete_file(f.str()) < 0) return -1;
      }
    }
  } catch (...) {
    return -1;
  }
  return delete_dir(dir);
}

std::string get_plugin_dir(const std::string &runtime_dir) {
  std::string cur_dir = Path(runtime_dir.c_str()).basename().str();
  if (cur_dir == "runtime_output_directory") {
    // single configuration build
    auto result = Path(runtime_dir.c_str()).dirname();
    return result.join("plugin_output_directory").str();
  } else {
    // multiple configuration build
    // in that case cur_dir has to be configuration name (Debug, Release etc.)
    // we need to go 2 levels up
    auto result = Path(runtime_dir.c_str()).dirname().dirname();
    return result.join("plugin_output_directory").join(cur_dir).str();
  }
}

HARNESS_EXPORT
std::string get_tests_data_dir(const std::string &runtime_dir) {
  std::string cur_dir = Path(runtime_dir.c_str()).basename().str();
  if (cur_dir == "runtime_output_directory") {
    // single configuration build
    auto result = Path(runtime_dir.c_str()).dirname();
    return result.join("router").join("tests").join("data").str();
  } else {
    // multiple configuration build
    // in that case cur_dir has to be configuration name (Debug, Release etc.)
    // we need to go 2 levels up
    auto result = Path(runtime_dir.c_str()).dirname().dirname();
    return result.join("router").join("tests").join("data").join(cur_dir).str();

    return result.str();
  }
}

int mkdir_wrapper(const std::string &dir, perm_mode mode);

int mkdir_recursive(const Path &path, perm_mode mode) {
  if (path.str().empty() || path.c_str() == Path::root_directory) return -1;

  // "mkdir -p" on Unix succeeds even if the directory one tries to create
  // exists, we want to mimic that
  if (path.exists()) {
    return path.is_directory() ? 0 : -1;
  }

  const auto parent = path.dirname();
  if (!parent.exists()) {
    auto res = mkdir_recursive(parent, mode);
    if (res != 0) return res;
  }

  return mkdir_wrapper(path.str(), mode);
}

int mkdir(const std::string &dir, perm_mode mode, bool recursive) {
  if (!recursive) {
    return mkdir_wrapper(dir, mode);
  }

  return mkdir_recursive(mysql_harness::Path(dir), mode);
}

#ifdef _WIN32

/**
 * Verifies permissions of an access ACE entry.
 *
 * @param[in] access_ace Access ACE entry.
 *
 * @throw std::exception Everyone has access to the ACE access entry or
 *                        an error occurred.
 */
static void check_ace_access_rights(ACCESS_ALLOWED_ACE *access_ace) {
  SID *sid = reinterpret_cast<SID *>(&access_ace->SidStart);
  DWORD sid_size = SECURITY_MAX_SID_SIZE;

  std::unique_ptr<SID, decltype(&free)> everyone_sid(
      static_cast<SID *>(malloc(sid_size)), &free);

  if (CreateWellKnownSid(WinWorldSid, nullptr, everyone_sid.get(), &sid_size) ==
      FALSE) {
    throw std::system_error(
        std::error_code(GetLastError(), std::system_category()),
        "CreateWellKnownSid() failed");
  }

  if (EqualSid(sid, everyone_sid.get())) {
    if (access_ace->Mask & (FILE_EXECUTE)) {
      throw std::system_error(make_error_code(std::errc::permission_denied),
                              "Expected no 'Execute' for 'Everyone'.");
    }
    if (access_ace->Mask &
        (FILE_WRITE_DATA | FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES)) {
      throw std::system_error(make_error_code(std::errc::permission_denied),
                              "Expected no 'Write' for 'Everyone'.");
    }
    if (access_ace->Mask &
        (FILE_READ_DATA | FILE_READ_EA | FILE_READ_ATTRIBUTES)) {
      throw std::system_error(make_error_code(std::errc::permission_denied),
                              "Expected no 'Read' for 'Everyone'.");
    }
  }
}

/**
 * Verifies access permissions in a DACL.
 *
 * @param[in] dacl DACL to be verified.
 *
 * @throw std::exception DACL contains an ACL entry that grants full access to
 *                        Everyone or an error occurred.
 */
static void check_acl_access_rights(ACL *dacl) {
  ACL_SIZE_INFORMATION dacl_size_info;

  if (GetAclInformation(dacl, &dacl_size_info, sizeof(dacl_size_info),
                        AclSizeInformation) == FALSE) {
    throw std::system_error(
        std::error_code(GetLastError(), std::system_category()),
        "GetAclInformation() failed");
  }

  for (DWORD ace_idx = 0; ace_idx < dacl_size_info.AceCount; ++ace_idx) {
    LPVOID ace = nullptr;

    if (GetAce(dacl, ace_idx, &ace) == FALSE) {
      throw std::system_error(
          std::error_code(GetLastError(), std::system_category()),
          "GetAce() failed");
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
 *                        Everyone or an error occurred.
 */
static void check_security_descriptor_access_rights(
    const std::unique_ptr<SECURITY_DESCRIPTOR, decltype(&free)> &sec_desc) {
  BOOL dacl_present;
  ACL *dacl;
  BOOL dacl_defaulted;

  if (GetSecurityDescriptorDacl(sec_desc.get(), &dacl_present, &dacl,
                                &dacl_defaulted) == FALSE) {
    throw std::system_error(
        std::error_code(GetLastError(), std::system_category()),
        "GetSecurityDescriptorDacl() failed");
  }

  if (!dacl_present) {
    // No DACL means: no access allowed. Which is fine.
    return;
  }

  if (!dacl) {
    // Empty DACL means: all access allowed.
    throw std::system_error(make_error_code(std::errc::permission_denied),
                            "Expected access denied for 'Everyone'.");
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
 *                        an error occurred.
 * @throw std::system_error OS and/or filesystem doesn't support file
 *                           permissions.
 */
void check_file_access_rights(const std::string &file_name) {
#ifdef _WIN32
  try {
    return check_security_descriptor_access_rights(
        mysql_harness::get_security_descriptor(file_name));
  } catch (const std::system_error &e) {
    if (e.code() == std::errc::permission_denied) {
      throw std::system_error(
          e.code(),
          "'" + file_name + "' has insecure permissions. " + e.what());
    } else {
      throw;
    }
  } catch (...) {
    throw;
  }
#else
  struct stat status;

  if (stat(file_name.c_str(), &status) != 0) {
    if (errno == ENOENT) return;
    throw std::system_error(errno, std::generic_category(),
                            "stat() failed for " + file_name + "'");
  }

  static constexpr mode_t kFullAccessMask = (S_IRWXU | S_IRWXG | S_IRWXO);
  static constexpr mode_t kRequiredAccessMask = (S_IRUSR | S_IWUSR);

  if ((status.st_mode & kFullAccessMask) != kRequiredAccessMask) {
    throw std::system_error(
        make_error_code(std::errc::permission_denied),
        "'" + file_name + "' has insecure permissions. Expected u+rw only.");
  }
#endif  // _WIN32
}

#ifdef _WIN32

/**
 * Gets the SID of the current process user.
 * The SID in Windows is the Security IDentifier, a security principal to which
 * permissions are attached (machine, user group, user).
 */
static void GetCurrentUserSid(std::unique_ptr<SID, decltype(&free)> &pSID) {
  DWORD dw_size = 0;
  HANDLE h_token;
  TOKEN_INFORMATION_CLASS token_class = TokenUser;
  // Gets security token of the current process
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_READ | TOKEN_QUERY,
                        &h_token)) {
    throw std::runtime_error("OpenProcessToken() failed: " +
                             std::to_string(GetLastError()));
  }
  // Gets the user token from the security token (this one only finds out the
  // buffer size required)
  if (!GetTokenInformation(h_token, token_class, NULL, 0, &dw_size)) {
    DWORD dwResult = GetLastError();
    if (dwResult != ERROR_INSUFFICIENT_BUFFER) {
      throw std::runtime_error("GetTokenInformation() failed: " +
                               std::to_string(dwResult));
    }
  }

  std::unique_ptr<TOKEN_USER, decltype(&free)> user(
      static_cast<TOKEN_USER *>(malloc(dw_size)), &free);
  if (user.get() == NULL) {
    throw std::runtime_error("LocalAlloc() failed: " +
                             std::to_string(GetLastError()));
  }

  // Gets the user token from the security token (this one retrieves the actual
  // user token)
  if (!GetTokenInformation(h_token, token_class, user.get(), dw_size,
                           &dw_size)) {
    throw std::runtime_error("GetTokenInformation() failed: " +
                             std::to_string(GetLastError()));
  }
  // Copies from the user token the SID
  DWORD dw_sid_len = GetLengthSid(user->User.Sid);
  pSID.reset(static_cast<SID *>(std::malloc(dw_sid_len)));
  CopySid(dw_sid_len, pSID.get(), user->User.Sid);
}

/**
 * Makes a file fully accessible by the current process user and (read only or
 * read/write depending on the second argument) for LocalService account (which
 * is the account under which the MySQL router runs as service). And not
 * accessible for everyone else.
 */
static void make_file_private_win32(const std::string &filename,
                                    const bool read_only_for_local_service) {
  PACL new_dacl = NULL;
  DWORD sid_size = SECURITY_MAX_SID_SIZE;
  SID local_service_sid;
  DWORD dw_res;
  // Obtains the SID of the LocalService account (the account under which runs
  // the Router as a service in Windows)
  if (CreateWellKnownSid(WinLocalServiceSid, NULL, &local_service_sid,
                         &sid_size) == FALSE) {
    throw std::runtime_error("CreateWellKnownSid() failed: " +
                             std::to_string(GetLastError()));
  }

  std::unique_ptr<SID, decltype(&free)> current_user(nullptr, &free);
  // Retrieves the current user process SID.
  GetCurrentUserSid(current_user);

  // Sets the actual permissions: two ACEs (access control entries) (one for
  // current user, one for LocalService) are configured and attached to a
  // Security Descriptor's DACL (Discretionary Access Control List), then
  // the Security Descriptors is used in SetFileSecurity API.
  EXPLICIT_ACCESSA ea[2];
  ZeroMemory(&ea, 2 * sizeof(EXPLICIT_ACCESSA));
  // Full acceess for current user
  ea[0].grfAccessPermissions =
      ACCESS_SYSTEM_SECURITY | READ_CONTROL | WRITE_DAC | GENERIC_ALL;
  ea[0].grfAccessMode = GRANT_ACCESS;
  ea[0].grfInheritance = NO_INHERITANCE;
  ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
  ea[0].Trustee.ptstrName = reinterpret_cast<char *>(current_user.get());

  // Read only or read/write access for LocalService account
  DWORD serviceRights = GENERIC_READ;
  if (!read_only_for_local_service) {
    serviceRights |= GENERIC_WRITE;
  }
  ea[1].grfAccessPermissions = serviceRights;

  ea[1].grfAccessMode = GRANT_ACCESS;
  ea[1].grfInheritance = NO_INHERITANCE;
  ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
  ea[1].Trustee.ptstrName = reinterpret_cast<char *>(&local_service_sid);
  // Make a new DACL with the two ACEs
  dw_res = SetEntriesInAclA(2, ea, NULL, &new_dacl);

  try {
    if (ERROR_SUCCESS != dw_res) {
      throw std::runtime_error("SetEntriesInAcl() failed: " +
                               std::to_string(dw_res));
    }

    // create and initialize a security descriptor.
    std::unique_ptr<SECURITY_DESCRIPTOR, decltype(&free)> psd(
        static_cast<SECURITY_DESCRIPTOR *>(
            std::malloc(SECURITY_DESCRIPTOR_MIN_LENGTH)),
        &free);
    if (psd.get() == NULL) {
      throw std::runtime_error("LocalAlloc() failed: " +
                               std::to_string(GetLastError()));
    }

    if (!InitializeSecurityDescriptor(psd.get(),
                                      SECURITY_DESCRIPTOR_REVISION)) {
      throw std::runtime_error("InitializeSecurityDescriptor failed: " +
                               std::to_string(GetLastError()));
    }
    // attach the DACL to the security descriptor
    if (!SetSecurityDescriptorDacl(psd.get(), TRUE, new_dacl, FALSE)) {
      throw std::runtime_error("SetSecurityDescriptorDacl failed: " +
                               std::to_string(GetLastError()));
    }

    if (!SetFileSecurityA(filename.c_str(), DACL_SECURITY_INFORMATION,
                          psd.get())) {
      dw_res = GetLastError();
      throw std::system_error(
          dw_res, std::system_category(),
          "SetFileSecurity failed: " + std::to_string(dw_res));
    }
    LocalFree((HLOCAL)new_dacl);
  } catch (...) {
    if (new_dacl != NULL) LocalFree((HLOCAL)new_dacl);
    throw;
  }
}

/**
 * Sets file permissions for Everyone group.
 *
 * @param[in] file_name File name.
 * @param[in] mask Access rights mask for Everyone group.
 *
 * @throw std::exception Failed to change file permissions.
 */
static void set_everyone_group_access_rights(const std::string &file_name,
                                             DWORD mask) {
  // Create Everyone SID.
  std::unique_ptr<SID, decltype(&free)> everyone_sid(
      static_cast<SID *>(std::malloc(SECURITY_MAX_SID_SIZE)), &free);

  DWORD sid_size = SECURITY_MAX_SID_SIZE;
  if (CreateWellKnownSid(WinWorldSid, NULL, everyone_sid.get(), &sid_size) ==
      FALSE) {
    throw std::runtime_error("CreateWellKnownSid() failed: " +
                             std::to_string(GetLastError()));
  }

  // Get file security descriptor.
  ACL *old_dacl;
  std::unique_ptr<SECURITY_DESCRIPTOR, decltype(&LocalFree)> sec_desc(
      nullptr, &LocalFree);

  {
    PSECURITY_DESCRIPTOR sec_desc_tmp;
    auto result = GetNamedSecurityInfoA(file_name.c_str(), SE_FILE_OBJECT,
                                        DACL_SECURITY_INFORMATION, NULL, NULL,
                                        &old_dacl, NULL, &sec_desc_tmp);

    if (result != ERROR_SUCCESS) {
      throw std::system_error(
          result, std::system_category(),
          "GetNamedSecurityInfo() failed: " + std::to_string(result));
    }

    // If everything went fine, we move raw pointer to smart pointer.
    sec_desc.reset(static_cast<SECURITY_DESCRIPTOR *>(sec_desc_tmp));
  }

  // Setting access permissions for Everyone group.
  EXPLICIT_ACCESSA ea[1];

  memset(&ea, 0, sizeof(ea));
  ea[0].grfAccessPermissions = mask;
  ea[0].grfAccessMode = SET_ACCESS;
  ea[0].grfInheritance = NO_INHERITANCE;
  ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
  ea[0].Trustee.ptstrName = reinterpret_cast<char *>(everyone_sid.get());

  // Create new ACL permission set.
  std::unique_ptr<ACL, decltype(&LocalFree)> new_dacl(nullptr, &LocalFree);

  {
    ACL *new_dacl_tmp;
    auto result = SetEntriesInAclA(1, &ea[0], old_dacl, &new_dacl_tmp);

    if (result != ERROR_SUCCESS) {
      throw std::runtime_error("SetEntriesInAcl() failed: " +
                               std::to_string(result));
    }

    // If everything went fine, we move raw pointer to smart pointer.
    new_dacl.reset(new_dacl_tmp);
  }

  // Set file security descriptor.
  auto result = SetNamedSecurityInfoA(const_cast<char *>(file_name.c_str()),
                                      SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                      NULL, NULL, new_dacl.get(), NULL);

  if (result != ERROR_SUCCESS) {
    throw std::system_error(
        result, std::system_category(),
        "SetNamedSecurityInfo() failed: " + std::to_string(result));
  }
}

#else

/**
 * Sets access permissions for a file.
 *
 * @param[in] file_name File name.
 * @param[in] mask Access permission mask.
 *
 * @throw std::exception Failed to change file permissions.
 */
static void throwing_chmod(const std::string &file_name, mode_t mask) {
  if (chmod(file_name.c_str(), mask) != 0) {
    auto ec = std::error_code(errno, std::generic_category());
    throw std::system_error(ec, "chmod() failed: " + file_name);
  }
}
#endif  // _WIN32

void make_file_public(const std::string &file_name) {
#ifdef _WIN32
  set_everyone_group_access_rights(
      file_name, FILE_GENERIC_EXECUTE | FILE_GENERIC_WRITE | FILE_GENERIC_READ);
#else
  throwing_chmod(file_name, S_IRWXU | S_IRWXG | S_IRWXO);
#endif
}

void make_file_private(const std::string &file_name,
                       const bool read_only_for_local_service) {
#ifdef _WIN32
  try {
    make_file_private_win32(file_name, read_only_for_local_service);
  } catch (const std::system_error &e) {
    throw std::system_error(e.code(), "Could not set permissions for file '" +
                                          file_name + "': " + e.what());
  }
#else
  (void)read_only_for_local_service;  // only relevant for Windows
  try {
    throwing_chmod(file_name, S_IRUSR | S_IWUSR);
  } catch (std::runtime_error &e) {
    throw std::runtime_error("Could not set permissions for file '" +
                             file_name + "': " + e.what());
  }
#endif
}

#ifdef _WIN32
void make_file_readable_for_everyone(const std::string &file_name) {
  try {
    set_everyone_group_access_rights(file_name, FILE_GENERIC_READ);
  } catch (const std::system_error &e) {
    throw std::system_error(e.code(), "Could not set permissions for file '" +
                                          file_name + "': " + e.what());
  }
}
#endif

void make_file_readonly(const std::string &file_name) {
#ifdef _WIN32
  set_everyone_group_access_rights(file_name,
                                   FILE_GENERIC_EXECUTE | FILE_GENERIC_READ);
#else
  throwing_chmod(file_name,
                 S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
#endif
}

}  // namespace mysql_harness
