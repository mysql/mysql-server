/*
  Copyright (c) 2015, 2021, Oracle and/or its affiliates.

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

#include <direct.h>
#include <cassert>
#include <cerrno>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

#include <shlwapi.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <windows.h>

using std::ostringstream;
using std::string;

namespace {
const std::string extsep(".");
}  // namespace

namespace mysql_harness {

const perm_mode kStrictDirectoryPerm = 0;

////////////////////////////////////////////////////////////////
// class Path members and free functions

// We normalize the Path class to use / internally, to avoid problems
// with code that assume \ to be an escape character
const char *const Path::directory_separator = "/";
const char *const Path::root_directory = "/";

Path::FileType Path::type(bool refresh) const {
  validate_non_empty_path();
  if (type_ == FileType::TYPE_UNKNOWN || refresh) {
    struct _stat stat_buf;
    if (_stat(c_str(), &stat_buf) == -1) {
      if (errno == ENOENT) {
        // Special case, a drive name like "C:"
        if (path_[path_.size() - 1] == ':') {
          DWORD flags = GetFileAttributesA(path_.c_str());
          // API reports it as directory if it exist
          if (flags & FILE_ATTRIBUTE_DIRECTORY) {
            type_ = FileType::DIRECTORY_FILE;
            return type_;
          }
        }
        type_ = FileType::FILE_NOT_FOUND;
      } else if (errno == EINVAL)
        type_ = FileType::STATUS_ERROR;
    } else {
      switch (stat_buf.st_mode & S_IFMT) {
        case S_IFDIR:
          type_ = FileType::DIRECTORY_FILE;
          break;
        case S_IFCHR:
          type_ = FileType::CHARACTER_FILE;
          break;
        case S_IFREG:
          type_ = FileType::REGULAR_FILE;
          break;
        default:
          type_ = FileType::TYPE_UNKNOWN;
          break;
      }
    }
  }
  return type_;
}

bool Path::is_absolute() const {
  validate_non_empty_path();  // throws std::invalid_argument
  if (path_[0] == '\\' || path_[0] == '/' || path_[1] == ':') return true;
  return false;
}

bool Path::is_readable() const {
  validate_non_empty_path();
  if (!exists()) return false;

  if (!is_directory())
    return std::ifstream(real_path().str()).good();
  else {
    WIN32_FIND_DATA find_data;
    HANDLE h = FindFirstFile(TEXT(real_path().str().c_str()), &find_data);
    if (h == INVALID_HANDLE_VALUE) {
      return GetLastError() != ERROR_ACCESS_DENIED;
    } else {
      FindClose(h);
      return true;
    }
  }
}

////////////////////////////////////////////////////////////////
// Directory::Iterator::State

class Directory::DirectoryIterator::State {
 public:
  State();
  State(const Path &path, const string &pattern);  // throws std::system_error
  ~State();

  void fill_result(bool first_entry_set = false);  // throws std::system_error

  template <typename IteratorType>
  static bool equal(const IteratorType &lhs, const IteratorType &rhs) {
    assert(lhs != nullptr && rhs != nullptr);

    // If either interator is an end iterator, they are equal if both
    // are end iterators.
    if (!lhs->more_ || !rhs->more_) return lhs->more_ == rhs->more_;

    // Otherwise, they are not equal (since we are using input
    // iterators, they do not compare equal in any other cases).
    return false;
  }

  WIN32_FIND_DATA data_;
  HANDLE handle_;
  bool more_;
  const string pattern_;

 private:
  static const char *dot;
  static const char *dotdot;
};

const char *Directory::DirectoryIterator::State::dot = ".";
const char *Directory::DirectoryIterator::State::dotdot = "..";

Directory::DirectoryIterator::State::State()
    : handle_(INVALID_HANDLE_VALUE), more_(false), pattern_("") {}

// throws std::system_error
Directory::DirectoryIterator::State::State(const Path &path,
                                           const string &pattern)
    : handle_(INVALID_HANDLE_VALUE), more_(true), pattern_(pattern) {
  const Path r_path = path.real_path();
  const string pat = r_path.join(pattern.size() > 0 ? pattern : "*").str();

  if (pat.size() > MAX_PATH) {
    ostringstream msg;
    msg << "Failed to open path '" << path << "'";
    throw std::system_error(std::make_error_code(std::errc::filename_too_long),
                            msg.str());
  }

  handle_ = FindFirstFile(pat.c_str(), &data_);
  if (handle_ == INVALID_HANDLE_VALUE)
    throw std::system_error(GetLastError(), std::system_category(),
                            "Failed to read first directory entry");

  fill_result(true);  // throws std::system_error
}

Directory::DirectoryIterator::State::~State() {
  if (handle_ != INVALID_HANDLE_VALUE) FindClose(handle_);
}

// throws std::system_error
void Directory::DirectoryIterator::State::fill_result(
    bool first_entry_set /*= false*/) {
  assert(handle_ != INVALID_HANDLE_VALUE);
  while (true) {
    if (first_entry_set) {
      more_ = true;
      first_entry_set = false;
    } else {
      more_ = (FindNextFile(handle_, &data_) != 0);
    }

    if (!more_) {
      int error = GetLastError();
      if (error != ERROR_NO_MORE_FILES) {
        throw std::system_error(GetLastError(), std::system_category(),
                                "Failed to read directory entry");
      } else {
        break;
      }
    } else {
      // Skip current directory and parent directory.
      if (!strcmp(data_.cFileName, dot) || !strcmp(data_.cFileName, dotdot))
        continue;

      // If no pattern is given, we're done.
      if (pattern_.size() == 0) break;

      // Skip any entries that do not match the pattern
      BOOL result = PathMatchSpecA(data_.cFileName, pattern_.c_str());
      if (!result)
        continue;
      else
        break;
    }
  }
}

////////////////////////////////////////////////////////////////
// Directory::Iterator

// These definition of the default constructor and destructor need to
// be here since the automatically generated default
// constructor/destructor uses the definition of the class 'State',
// which is not available when the header file is read.
#if !defined(_MSC_VER) || (_MSC_VER >= 1900)
Directory::DirectoryIterator::~DirectoryIterator() = default;
Directory::DirectoryIterator::DirectoryIterator(DirectoryIterator &&) = default;
Directory::DirectoryIterator::DirectoryIterator(const DirectoryIterator &) =
    default;
#elif defined(_MSC_VER)
Directory::DirectoryIterator::~DirectoryIterator() { state_.reset(); }
#endif

Directory::DirectoryIterator::DirectoryIterator()
    : path_("*END*"), state_(std::make_shared<State>()) {}

Directory::DirectoryIterator::DirectoryIterator(const Path &path,
                                                const std::string &pattern)
    : path_(path.real_path()), state_(std::make_shared<State>(path, pattern)) {}

Directory::DirectoryIterator &Directory::DirectoryIterator::operator++() {
  assert(state_ != nullptr);
  state_->fill_result();  // throws std::system_error
  return *this;
}

Path Directory::DirectoryIterator::operator*() const {
  assert(state_ != nullptr && state_->handle_ != INVALID_HANDLE_VALUE);
  return path_.join(state_->data_.cFileName);
}

bool Directory::DirectoryIterator::operator!=(
    const DirectoryIterator &rhs) const {
  return !State::equal(state_, rhs.state_);
}

Path Path::make_path(const Path &dir, const std::string &base,
                     const std::string &ext) {
  return dir.join(base + extsep + ext);
}

Path Path::real_path() const {
  validate_non_empty_path();

  // store a copy of str() in native_path
  assert(0 < str().size() && str().size() < MAX_PATH);
  char native_path[MAX_PATH];
  std::memcpy(native_path, c_str(),
              str().size() + 1);  // +1 for null terminator

  // replace all '/' with '\'
  char *p = native_path;
  while (*p) {
    if (*p == '/') {
      *p = '\\';
    }
    p++;
  }

  // resolve absolute path
  char path[MAX_PATH];
  if (GetFullPathNameA(native_path, sizeof(path), path, nullptr) == 0) {
    return Path();
  }

  // check if the path exists, to match posix behaviour
  WIN32_FIND_DATA find_data;
  HANDLE h = FindFirstFile(path, &find_data);
  if (h == INVALID_HANDLE_VALUE) {
    auto error = GetLastError();
    // If we got ERROR_ACCESS_DENIED here that does not necessarily mean
    // that the path does not exist. We still can have the access to the
    // file itself but we can't call the Find on the directory that contains
    // the file. (This is true for example when the config file is placed in
    // the User's directory and it is accesseed by the router that is run
    // as a Windows service.)
    // In that case we do not treat that as an error.
    if (error != ERROR_ACCESS_DENIED) {
      return Path();
    }
  } else {
    FindClose(h);
  }

  return Path(path);
}

////////////////////////////////////////////////////////////////////////////////
//
// Utility free functions
//
////////////////////////////////////////////////////////////////////////////////

stdx::expected<void, std::error_code> delete_dir(
    const std::string &dir) noexcept {
  if (_rmdir(dir.c_str()) != 0) {
    return stdx::make_unexpected(
        std::error_code(errno, std::generic_category()));
  }

  return {};
}

stdx::expected<void, std::error_code> delete_file(
    const std::string &path) noexcept {
  // In Windows a file recently closed may fail to be deleted because it may
  // still be locked (or have a 3rd party reading it, like an Indexer service
  // or AntiVirus). So the recommended is to retry the delete operation.
  for (int attempts{}; attempts < 10; ++attempts) {
    if (DeleteFile(path.c_str())) {
      return {};
    }
    const auto err = GetLastError();
    if (err == ERROR_ACCESS_DENIED) {
      Sleep(100);
      continue;
    } else {
      break;
    }
  }

  return stdx::make_unexpected(
      std::error_code(GetLastError(), std::system_category()));
}

std::string get_tmp_dir(const std::string &name) {
  char buf[MAX_PATH];
  auto res = GetTempPath(MAX_PATH, buf);
  if (res == 0 || res > MAX_PATH) {
    throw std::runtime_error("Could not get temporary directory");
  }

  auto generate_random_sequence = [](size_t len) -> std::string {
    std::random_device rd;
    std::string result;
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz";
    std::uniform_int_distribution<unsigned long> dist(0, sizeof(alphabet) - 2);

    for (size_t i = 0; i < len; ++i) {
      result += alphabet[dist(rd)];
    }

    return result;
  };

  std::string dir_name = name + "-" + generate_random_sequence(10);
  std::string result = Path(buf).join(dir_name).str();
  int err = _mkdir(result.c_str());
  if (err != 0) {
    throw std::runtime_error("Error creating temporary directory " + result);
  }
  return result;
}

std::unique_ptr<SECURITY_DESCRIPTOR, decltype(&free)> get_security_descriptor(
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

  std::unique_ptr<SECURITY_DESCRIPTOR, decltype(&free)> sec_desc(
      static_cast<SECURITY_DESCRIPTOR *>(std::malloc(sec_desc_size)), &free);

  if (GetFileSecurityA(file_name.c_str(), kReqInfo, sec_desc.get(),
                       sec_desc_size, &sec_desc_size) == FALSE) {
    errno = GetLastError();
    throw std::system_error(errno, std::system_category(),
                            "GetFileSecurity() failed (" + file_name +
                                "): " + std::to_string(GetLastError()));
  }

  return sec_desc;
}

int mkdir_wrapper(const std::string &dir, perm_mode /* mode */) {
  auto res = _mkdir(dir.c_str());
  if (res != 0) return errno;
  return 0;
}

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

void check_file_access_rights(const std::string &file_name) {
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
}

void make_file_public(const std::string &file_name) {
  set_everyone_group_access_rights(
      file_name, FILE_GENERIC_EXECUTE | FILE_GENERIC_WRITE | FILE_GENERIC_READ);
}

void make_file_private(const std::string &file_name,
                       const bool read_only_for_local_service) {
  try {
    make_file_private_win32(file_name, read_only_for_local_service);
  } catch (const std::system_error &e) {
    throw std::system_error(e.code(), "Could not set permissions for file '" +
                                          file_name + "': " + e.what());
  }
}

void make_file_readable_for_everyone(const std::string &file_name) {
  try {
    set_everyone_group_access_rights(file_name, FILE_GENERIC_READ);
  } catch (const std::system_error &e) {
    throw std::system_error(e.code(), "Could not set permissions for file '" +
                                          file_name + "': " + e.what());
  }
}

void make_file_readonly(const std::string &file_name) {
  set_everyone_group_access_rights(file_name,
                                   FILE_GENERIC_EXECUTE | FILE_GENERIC_READ);
}

}  // namespace mysql_harness
