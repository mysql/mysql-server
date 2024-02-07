/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysql/harness/access_rights.h"

#ifdef _WIN32
#include <aclapi.h>
#include <sddl.h>  // ConvertSidToStringSidA
#endif

#include <functional>  // bad_function_call
#include <optional>
#include <sstream>

#include "mysql/harness/stdx/expected.h"

#ifdef _WIN32
namespace {
std::error_code last_error_code() {
  return {static_cast<int>(GetLastError()), std::system_category()};
}
}  // namespace

namespace mysql_harness::win32::access_rights {

stdx::expected<Allocated<SID>, std::error_code> create_well_known_sid(
    WELL_KNOWN_SID_TYPE well_known_sid) {
  DWORD sid_size = SECURITY_MAX_SID_SIZE;

  Allocated<SID> sid(sid_size);

  if (CreateWellKnownSid(well_known_sid, nullptr, sid.get(), &sid_size) ==
      FALSE) {
    return stdx::unexpected(last_error_code());
  }

  return {std::move(sid)};
}

class Handle {
 public:
  using native_handle_type = HANDLE;

  // INVALID_HANDLE_VALUE can't be constexpr due to c-style cast
  //
  // #define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
  static const native_handle_type kInvalidHandle;

  Handle() = default;

  Handle(HANDLE handle) : handle_{handle} {}

  Handle(const Handle &) = delete;
  Handle(Handle &&other)
      : handle_{std::exchange(other.handle_, kInvalidHandle)} {}

  Handle &operator=(Handle &&other) {
    Handle tmp(std::move(other));

    swap(tmp);

    return *this;
  }

  bool is_open() const { return handle_ != kInvalidHandle; }

  HANDLE native_handle() const { return handle_; }

  void close() {
    CloseHandle(handle_);

    handle_ = kInvalidHandle;
  }

  void swap(Handle &other) noexcept { std::swap(handle_, other.handle_); }

  ~Handle() { close(); }

 private:
  HANDLE handle_{kInvalidHandle};
};

const Handle::native_handle_type Handle::kInvalidHandle{INVALID_HANDLE_VALUE};

stdx::expected<Handle, std::error_code> open_process_token(
    HANDLE process_handle) {
  HANDLE h_token;

  // Gets security token of the current process
  if (!OpenProcessToken(process_handle, TOKEN_READ | TOKEN_QUERY, &h_token)) {
    return stdx::unexpected(last_error_code());
  }

  return {h_token};
}

stdx::expected<Allocated<TOKEN_USER>, std::error_code> token_user(
    const Handle &handle) {
  DWORD token_size = 0;
  TOKEN_INFORMATION_CLASS token_class = TokenUser;

  HANDLE h_token = handle.native_handle();
  // Gets the user token from the security token (this one only finds out the
  // buffer size required)
  if (!GetTokenInformation(h_token, token_class, nullptr, 0, &token_size)) {
    auto ec = last_error_code();

    if (ec !=
        std::error_code{ERROR_INSUFFICIENT_BUFFER, std::system_category()}) {
      return stdx::unexpected(ec);
    }
  }

  SizedAllocated<TOKEN_USER> user(token_size);

  // Gets the user token from the security token (this one retrieves the actual
  // user token)
  if (!GetTokenInformation(h_token, token_class, user.get(), user.size(),
                           &token_size)) {
    return stdx::unexpected(last_error_code());
  }

  return {std::move(user)};
}

stdx::expected<Allocated<SID>, std::error_code> current_user_sid() {
  auto process_token_res = open_process_token(GetCurrentProcess());
  if (!process_token_res) {
    auto ec = process_token_res.error();
    return stdx::unexpected(ec);
  }

  auto token_user_res = token_user(process_token_res.value());
  if (!token_user_res) {
    auto ec = token_user_res.error();
    return stdx::unexpected(ec);
  }

  auto token_user = std::move(token_user_res.value());

  // Copies from the user token the SID

  const auto sid_len = GetLengthSid(token_user->User.Sid);
  Allocated<SID> sid{sid_len};
  CopySid(sid_len, sid.get(), token_user->User.Sid);

  return {std::move(sid)};
}

std::string Sid::to_string() const {
  LPSTR sid_out{nullptr};

  ConvertSidToStringSidA(sid_, &sid_out);

  // LocalFree(sid_out) on exit.
  std::unique_ptr<char, decltype(&LocalFree)> sid_out_guard(sid_out,
                                                            &LocalFree);

  return sid_out;
}

Acl::iterator::reference Acl::iterator::operator*() {
  LPVOID ace = nullptr;

  if (GetAce(acl_, ndx_, &ace) == FALSE) {
    throw std::system_error(last_error_code());
  }

  ace_ = static_cast<ACE_HEADER *>(ace);

  return ace_;
}

Acl::iterator &Acl::iterator::operator++() {
  ++ndx_;

  return *this;
}

bool Acl::iterator::operator!=(const Acl::iterator &other) const {
  return !(acl_ == other.acl_ && ndx_ == other.ndx_);
}

size_t Acl::size() const {
  ACL_SIZE_INFORMATION dacl_size_info;

  if (GetAclInformation(acl_, &dacl_size_info, sizeof(dacl_size_info),
                        AclSizeInformation) == FALSE) {
    return 0;
  }

  return dacl_size_info.AceCount;
}

std::string Ace::to_string() const {
  if (type() == ACCESS_ALLOWED_ACE_TYPE) {
    AccessAllowedAce access_ace(static_cast<ACCESS_ALLOWED_ACE *>(data()));

    std::ostringstream ohex;

    ohex << std::showbase << std::hex << access_ace.mask();

    return "A;;" + ohex.str() + ";;;" + access_ace.sid().to_string();
  }

  return "U";  // unknown
}

std::string Acl::to_string() const {
  std::string out;

  for (const auto &ace : *this) {
    out += "(" + ace.to_string() + ")";
  }

  return out;
}

stdx::expected<void, std::error_code> SecurityDescriptor::initialize(
    DWORD revision) {
  if (!InitializeSecurityDescriptor(desc_, revision)) {
    return stdx::unexpected(last_error_code());
  }

  return {};
}

stdx::expected<OptionalDacl, std::error_code> SecurityDescriptor::dacl() const {
  BOOL dacl_present;
  ACL *dacl;
  BOOL dacl_defaulted;

  if (0 ==
      GetSecurityDescriptorDacl(desc_, &dacl_present, &dacl, &dacl_defaulted)) {
    return stdx::unexpected(last_error_code());
  }

  if (!dacl_present) {
    return {};
  }

  return stdx::expected<OptionalDacl, std::error_code>{std::in_place, dacl};
}

stdx::expected<void, std::error_code> SecurityDescriptor::dacl(
    const OptionalDacl &opt_dacl, bool dacl_defaulted) {
  ACL *dacl{};

  BOOL dacl_present = (bool)opt_dacl;
  if (dacl_present) {
    dacl = opt_dacl.value();
  }

  if (0 ==
      SetSecurityDescriptorDacl(desc_, dacl_present, dacl, dacl_defaulted)) {
    return stdx::unexpected(last_error_code());
  }

  return {};
}

std::string SecurityDescriptor::to_string() const {
  SECURITY_INFORMATION sec_info{DACL_SECURITY_INFORMATION};

  LPSTR out_s{nullptr};

  ConvertSecurityDescriptorToStringSecurityDescriptorA(
      desc_, SDDL_REVISION_1, sec_info, &out_s, nullptr);

  // LocalFree(out_s) on exit.
  std::unique_ptr<char, decltype(&LocalFree)> out_s_guard(out_s, &LocalFree);

  return out_s;
}

stdx::expected<SECURITY_DESCRIPTOR_CONTROL, std::error_code>
SecurityDescriptor::control() const {
  SECURITY_DESCRIPTOR_CONTROL control;
  DWORD revision;

  if (0 == GetSecurityDescriptorControl(desc_, &control, &revision)) {
    return stdx::unexpected(last_error_code());
  }

  return control;
}

stdx::expected<Allocated<SECURITY_DESCRIPTOR>, std::error_code>
SecurityDescriptor::make_self_relative() {
  DWORD self_rel_size = 0;

  // get the size of the buffer.
  if (0 == MakeSelfRelativeSD(desc_, nullptr, &self_rel_size)) {
    const auto ec = last_error_code();

    if (ec.value() != ERROR_INSUFFICIENT_BUFFER) {
      return stdx::unexpected(ec);
    }
    // fall through
  }

  Allocated<SECURITY_DESCRIPTOR> self_rel(self_rel_size);

  if (0 == MakeSelfRelativeSD(desc_, self_rel.get(), &self_rel_size)) {
    const auto ec = last_error_code();
    return stdx::unexpected(ec);
  }

  return self_rel;
}

//
// acl-builder
//

AclBuilder::AclBuilder() : old_desc_{SECURITY_DESCRIPTOR_MIN_LENGTH} {
  SecurityDescriptor sec_desc{old_desc_.get()};

  sec_desc.initialize();
}

AclBuilder::AclBuilder(security_descriptor_type old_desc)
    : old_desc_{std::move(old_desc)} {}

EXPLICIT_ACCESSW AclBuilder::ace_grant_access(SID *sid, DWORD rights) {
  TRUSTEEW trustee;

  BuildTrusteeWithSidW(&trustee, sid);

  return {rights, GRANT_ACCESS, NO_INHERITANCE, trustee};
}

EXPLICIT_ACCESSW AclBuilder::ace_set_access(SID *sid, DWORD rights) {
  TRUSTEEW trustee;

  BuildTrusteeWithSidW(&trustee, sid);

  return {rights, SET_ACCESS, NO_INHERITANCE, trustee};
}

EXPLICIT_ACCESSW AclBuilder::ace_revoke_access(SID *sid) {
  TRUSTEEW trustee;

  BuildTrusteeWithSidW(&trustee, sid);

  return {0, REVOKE_ACCESS, NO_INHERITANCE, trustee};
}

/**
 * grant additional access rights to the current user.
 */
AclBuilder &AclBuilder::grant(CurrentUser, DWORD rights) {
  auto sid_res = win32::access_rights::current_user_sid();
  if (!sid_res) {
    ec_ = sid_res.error();

    return *this;
  }

  return grant(std::move(sid_res.value()), rights);
}

/**
 * grant additional access rights to a well-known-sid.
 */
AclBuilder &AclBuilder::grant(const WellKnownSid &owner, DWORD rights) {
  auto sid_res = win32::access_rights::create_well_known_sid(owner.sid);
  if (!sid_res) {
    ec_ = sid_res.error();

    return *this;
  }

  return grant(std::move(sid_res.value()), rights);
}

AclBuilder &AclBuilder::grant(Allocated<SID> sid, DWORD rights) {
  // keep the allocated SID alive until the end of the Builder's lifetime
  owned_sids_.push_back(std::move(sid));

  perms_.push_back(ace_grant_access(owned_sids_.back().get(), rights));

  return *this;
}

AclBuilder &AclBuilder::set(CurrentUser, DWORD rights) {
  auto sid_res = win32::access_rights::current_user_sid();
  if (!sid_res) {
    ec_ = sid_res.error();

    return *this;
  }

  return set(std::move(sid_res.value()), rights);
}

AclBuilder &AclBuilder::set(const WellKnownSid &owner, DWORD rights) {
  auto sid_res = win32::access_rights::create_well_known_sid(owner.sid);
  if (!sid_res) {
    ec_ = sid_res.error();

    return *this;
  }

  return set(std::move(sid_res.value()), rights);
}

AclBuilder &AclBuilder::set(Allocated<SID> sid, DWORD rights) {
  // keep the allocated SID alive until the end of the Builder's lifetime
  owned_sids_.push_back(std::move(sid));

  perms_.push_back(ace_set_access(owned_sids_.back().get(), rights));

  return *this;
}

AclBuilder &AclBuilder::revoke(CurrentUser) {
  auto sid_res = win32::access_rights::current_user_sid();
  if (!sid_res) {
    ec_ = sid_res.error();

    return *this;
  }

  return revoke(std::move(sid_res.value()));
}

AclBuilder &AclBuilder::revoke(const WellKnownSid &owner) {
  auto sid_res = win32::access_rights::create_well_known_sid(owner.sid);
  if (!sid_res) {
    ec_ = sid_res.error();

    return *this;
  }

  return revoke(std::move(sid_res.value()));
}

AclBuilder &AclBuilder::revoke(Allocated<SID> sid) {
  // keep the allocated SID alive until the end of the Builder's lifetime
  owned_sids_.push_back(std::move(sid));

  perms_.push_back(ace_revoke_access(owned_sids_.back().get()));

  return *this;
}

stdx::expected<security_descriptor_type, std::error_code> AclBuilder::build() {
  using namespace mysql_harness::win32::access_rights;

  // if add/set/revoke failed, return the error.
  if (ec_) return stdx::unexpected(ec_);

  if (auto desc = old_desc_.get()) {
    // BuildSecurityDescriptorW needs the old security descriptor in
    // self-relative format.
    SecurityDescriptor s{desc};

    if (!s.is_self_relative()) {
      auto self_rel_res = s.make_self_relative();
      if (!self_rel_res) return stdx::unexpected(self_rel_res.error());

      old_desc_ = std::move(self_rel_res.value());
    }
  }

  ULONG new_sd_size{};
  PSECURITY_DESCRIPTOR new_sd{};
  const auto err =
      BuildSecurityDescriptorW(nullptr,                       // owner
                               nullptr,                       // group
                               perms_.size(), perms_.data(),  // DACL
                               0, nullptr,                    // SACL
                               old_desc_.get(),               // old sec-desc
                               &new_sd_size,                  // [out] size
                               &new_sd                        // [out] sec-desc
      );
  if (err != ERROR_SUCCESS) {
    std::error_code ec{static_cast<int>(err), std::system_category()};

    return stdx::unexpected(ec);
  }

  return stdx::expected<security_descriptor_type, std::error_code>{
      std::in_place, reinterpret_cast<SECURITY_DESCRIPTOR *>(new_sd)};
}

/*
 * win32 function lookup wrapper.
 */
template <typename Signature>
class Win32Function;

template <typename Result, typename... Args>
class Win32Function<Result(Args...)> {
 public:
  using result_type = Result;
  using signature = Result(Args...);

  /*
   * lookup function from module by name.
   *
   * @module
   */
  Win32Function(const char *module, const char *func_name) {
    if (HMODULE mod = GetModuleHandleA(module)) {
      func_ = reinterpret_cast<signature *>(GetProcAddress(mod, func_name));
    }
  }

  /**
   * check if a function is set.
   *
   * @retval true if function is set
   * @retval false if function is not set
   */
  explicit operator bool() const noexcept { return func_ != nullptr; }

  /**
   * call wrapped win32 function.
   *
   * forwards arguments to the wrapped function and returns its result.
   *
   * @throws std::bad_function_call if no function is set.
   */
  result_type operator()(Args... args) const {
    if (!*this) throw std::bad_function_call();

    return func_(std::forward<Args>(args)...);
  }

 private:
  signature *func_{nullptr};
};

/*
 * check if running under wine.
 */
static bool running_under_wine() {
  // check the function wine_get_version exists in ntdll.dll
  static Win32Function<const char *__cdecl(void)> wine_get_version(
      "ntdll.dll", "wine_get_version");

  return (bool)wine_get_version;
}

stdx::expected<void, std::error_code> AllowUserReadWritableVerifier::operator()(
    const security_descriptor_type &desc) {
  SecurityDescriptor sec_desc{desc.get()};

  auto dacl_res = sec_desc.dacl();
  if (!dacl_res) return stdx::unexpected(dacl_res.error());

  auto optional_dacl = std::move(dacl_res.value());

  if (!optional_dacl) {
    // No DACL means: all access allow.
    return stdx::unexpected(make_error_code(std::errc::permission_denied));
  }

  if (optional_dacl.value() == nullptr) {
    // Empty DACL means: no access allowed.
    return {};
  }

  // get the current user sid
  auto current_user_sid_res = current_user_sid();
  if (!current_user_sid_res) {
    return stdx::unexpected(current_user_sid_res.error());
  }

  auto allocated_current_user_sid = std::move(current_user_sid_res.value());

  Sid current_user_sid{allocated_current_user_sid.get()};

  // local system
  auto local_system_sid_res = create_well_known_sid(WinLocalSystemSid);
  if (!local_system_sid_res) {
    return stdx::unexpected(local_system_sid_res.error());
  }

  auto allocated_local_system_sid = std::move(local_system_sid_res.value());

  Sid local_system_sid{allocated_local_system_sid.get()};

  // local service
  auto local_service_sid_res = create_well_known_sid(WinLocalServiceSid);
  if (!local_service_sid_res) {
    return stdx::unexpected(local_service_sid_res.error());
  }

  auto allocated_local_service_sid = std::move(local_service_sid_res.value());

  Sid local_service_sid{allocated_local_service_sid.get()};

  // find the AccessAllowedAce of "everyone" and fail if it has any of the
  // file-permissions set.
  for (const auto &ace : Acl(optional_dacl.value())) {
    if (ace.type() != ACCESS_ALLOWED_ACE_TYPE) continue;

    AccessAllowedAce allowed_access(
        static_cast<ACCESS_ALLOWED_ACE *>(ace.data()));

    if (allowed_access.sid() == current_user_sid) {
      // all FILE_* bits except FILE_EXECUTE.
      //
      // FILE_EXECUTE is always set even if no one set it.
      const auto file_access_mask =
          FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_READ_EA |
          FILE_WRITE_EA | /* FILE_EXECUTE | */
          FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES;
      const auto expected =
          (FILE_GENERIC_WRITE | FILE_GENERIC_READ) & file_access_mask;

      // the current user
      if ((allowed_access.mask() & file_access_mask) != expected) {
        return stdx::unexpected(make_error_code(std::errc::permission_denied));
      }
    } else if (running_under_wine() &&
               allowed_access.sid() == local_system_sid) {
      // under wine LocalSystem will have permissions too.
    } else if (allowed_access.sid() == local_service_sid) {
      // make_file_public() also allows LocalService to
    } else {
      // everyone else.
      return stdx::unexpected(make_error_code(std::errc::permission_denied));
    }
  }

  return {};
}

stdx::expected<void, std::error_code> DenyOtherReadWritableVerifier::operator()(
    const security_descriptor_type &desc) {
  auto dacl_res = SecurityDescriptor(desc.get()).dacl();
  if (!dacl_res) return stdx::unexpected(dacl_res.error());

  auto optional_dacl = std::move(dacl_res.value());

  if (!optional_dacl) {
    // No DACL means: all access allow.
    return stdx::unexpected(make_error_code(std::errc::permission_denied));
  }

  if (optional_dacl.value() == nullptr) {
    // Empty DACL means: no access allowed.
    return {};
  }

  // everyone
  auto sid_res = create_well_known_sid(WinWorldSid);
  if (!sid_res) return stdx::unexpected(sid_res.error());

  auto allocated_everyone_sid = std::move(sid_res.value());

  Sid everyone_sid{allocated_everyone_sid.get()};

  // find the AccessAllowedAce of "everyone" and fail if it has any of the
  // file-permissions set.
  for (const auto &ace : Acl(optional_dacl.value())) {
    if (ace.type() != ACCESS_ALLOWED_ACE_TYPE) continue;

    AccessAllowedAce allowed_access(
        static_cast<ACCESS_ALLOWED_ACE *>(ace.data()));

    if (allowed_access.sid() == everyone_sid) {
      if (allowed_access.mask() &
          (FILE_EXECUTE |
           (FILE_WRITE_DATA | FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES) |
           (FILE_READ_DATA | FILE_READ_EA | FILE_READ_ATTRIBUTES))) {
        return stdx::unexpected(make_error_code(std::errc::permission_denied));
      }
    }
  }

  return {};
}

stdx::expected<security_descriptor_type, std::error_code> access_rights_get(
    const std::string &filename) noexcept {
  static constexpr SECURITY_INFORMATION kReqInfo = DACL_SECURITY_INFORMATION;

  // Get the size of the descriptor.
  DWORD sec_desc_size;

  if (GetFileSecurityA(filename.c_str(), kReqInfo, nullptr, 0,
                       &sec_desc_size) == FALSE) {
    const auto ec = last_error_code();

    // We expect to receive `ERROR_INSUFFICIENT_BUFFER`.
    if (ec !=
        std::error_code{ERROR_INSUFFICIENT_BUFFER, std::system_category()}) {
      return stdx::unexpected(ec);
    }
  }

  security_descriptor_type desc(sec_desc_size);

  if (GetFileSecurityA(filename.c_str(), kReqInfo, desc.get(), sec_desc_size,
                       &sec_desc_size) == FALSE) {
    const auto ec = last_error_code();

    return stdx::unexpected(ec);
  }

  return {std::move(desc)};
}

stdx::expected<void, std::error_code> access_rights_set(
    const std::string &file_name, const security_descriptor_type &desc) {
  if (0 == SetFileSecurityA(file_name.c_str(), DACL_SECURITY_INFORMATION,
                            desc.get())) {
    std::error_code ec{last_error_code()};
    return stdx::unexpected(ec);
  }

  return {};
}

}  // namespace mysql_harness::win32::access_rights
#endif

namespace mysql_harness {

stdx::expected<security_descriptor_type, std::error_code> access_rights_get(
    const std::string &filename) noexcept {
#if defined(_WIN32)
  return win32::access_rights::access_rights_get(filename);
#else
  struct stat st;

  if (-1 == ::stat(filename.c_str(), &st)) {
    return stdx::unexpected(std::error_code{errno, std::generic_category()});
  }

  return {st.st_mode};
#endif
}

stdx::expected<void, std::error_code> access_rights_set(
    const std::string &filename, const security_descriptor_type &rights) {
#if defined(_WIN32)
  return win32::access_rights::access_rights_set(filename, rights);
#else
  if (-1 == ::chmod(filename.c_str(), rights)) {
    return stdx::unexpected(std::error_code{errno, std::generic_category()});
  }

  return {};
#endif
}
}  // namespace mysql_harness
