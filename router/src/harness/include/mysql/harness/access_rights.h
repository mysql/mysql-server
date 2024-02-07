/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_ACCESS_RIGHTS_INCLUDED
#define MYSQL_HARNESS_ACCESS_RIGHTS_INCLUDED

#include "harness_export.h"

#ifdef _WIN32
#include <aclapi.h>
#else
#include <sys/stat.h>  // stat
#include <sys/types.h>
#include <unistd.h>
#endif

#include <optional>
#include <system_error>
#include <utility>  // exchange
#include <vector>

#include "mysql/harness/stdx/expected.h"

namespace mysql_harness {

#ifndef _WIN32

namespace posix::access_rights {

using security_descriptor_type = mode_t;

/**
 * denies permissions.
 */
template <int Mask>
class DenyPermissionVerifier {
 public:
  static constexpr const mode_t kMask = Mask;
  static constexpr const mode_t kFullAccessMask = (S_IRWXU | S_IRWXG | S_IRWXO);

  // Mask MUST not have flags output of FullAccessMask
  static_assert(!(kMask & ~kFullAccessMask));

  stdx::expected<void, std::error_code> operator()(
      const security_descriptor_type &perms) {
    if ((perms & kMask) != 0) {
      return stdx::unexpected(make_error_code(std::errc::permission_denied));
    }

    return {};
  }
};

/**
 * allows permissions.
 */
template <int Mask>
class AllowPermissionVerifier {
 public:
  static constexpr const mode_t kMask = Mask;
  static constexpr const mode_t kFullAccessMask = (S_IRWXU | S_IRWXG | S_IRWXO);

  stdx::expected<void, std::error_code> operator()(
      const security_descriptor_type &perms) {
    if ((perms & kFullAccessMask) != kMask) {
      return stdx::unexpected(make_error_code(std::errc::permission_denied));
    }

    return {};
  }
};

}  // namespace posix::access_rights
#endif

#ifdef _WIN32
namespace win32::access_rights {

class LocalDeleter {
 public:
  void operator()(void *ptr) { LocalFree(ptr); }
};

template <class T>
using LocalAllocated = std::unique_ptr<T, LocalDeleter>;

/**
 * a smart-pointer for types whose size is discovered at runtime.
 *
 * used to wrap APIs like the SECURITY_DESCRIPTOR
 * on windows which has variable size and explicit no allocation
 * and no free function.
 *
 * It is up to the caller to:
 *
 * - ask for the size of the data
 * - allocate and free
 *
 * Uses LocalFree() to free its owned memory which makes it suitable
 * win32 APIs which explicitly "must be freed with LocalFree()".
 */
template <class T>
class Allocated {
 public:
  using allocated_type = LocalAllocated<T>;
  using pointer = typename allocated_type::pointer;
  using element_type = typename allocated_type::element_type;

  /**
   * allocate memory.
   *
   * @param size size to allocate.
   */
  explicit Allocated(size_t size)
      : allocated_{reinterpret_cast<pointer>(LocalAlloc(LPTR, size))} {}

  /**
   * take ownership of p.
   *
   * p MUST be allocated by LocalAlloc()
   */
  Allocated(pointer p) : allocated_{p} {}

  pointer get() const noexcept { return allocated_.get(); }

  pointer operator->() const { return allocated_.get(); }

  void reset(pointer ptr) { allocated_.reset(ptr); }

 private:
  allocated_type allocated_;
};

/**
 * a Allocated which remembers its size().
 */
template <class T>
class SizedAllocated : public Allocated<T> {
 public:
  /**
   * allocate memory.
   *
   * @param size size to allocate.
   */
  SizedAllocated(size_t size) : Allocated<T>{size}, size_{size} {}

  [[nodiscard]] size_t size() const noexcept { return size_; }

 private:
  size_t size_;
};

using security_descriptor_type = Allocated<SECURITY_DESCRIPTOR>;

/**
 * a SID structure of a "well-known-sid".
 */
stdx::expected<Allocated<SID>, std::error_code> HARNESS_EXPORT
create_well_known_sid(WELL_KNOWN_SID_TYPE well_known_sid);

/**
 * get current users SID.
 */
stdx::expected<Allocated<SID>, std::error_code> HARNESS_EXPORT
current_user_sid();

/**
 * Security Identifier.
 */
class HARNESS_EXPORT Sid {
 public:
  /**
   * wrap a native SID pointer.
   */
  Sid(SID *sid) : sid_{std::move(sid)} {}

  BYTE revision() const { return sid_->Revision; }
  BYTE sub_authority_count() const { return sid_->SubAuthorityCount; }
  SID_IDENTIFIER_AUTHORITY identifier_authority() const {
    return sid_->IdentifierAuthority;
  }

  std::string to_string() const;

  SID *native() { return sid_; }

  friend bool operator==(const Sid &, const Sid &);

 private:
  SID *sid_;
};

inline bool operator==(const Sid &a, const Sid &b) {
  return EqualSid(a.sid_, b.sid_);
}

/**
 * Access Control Entry.
 *
 * header of all ACE structures.
 */
class HARNESS_EXPORT Ace {
 public:
  Ace(ACE_HEADER *ace) : ace_{std::move(ace)} {}

  BYTE type() const { return ace_->AceType; }
  BYTE flags() const { return ace_->AceFlags; }
  WORD size() const { return ace_->AceSize; }

  void *data() const { return ace_; }

  std::string to_string() const;

 private:
  ACE_HEADER *ace_;
};

/**
 * Access Control List.
 */
class HARNESS_EXPORT Acl {
 public:
  explicit Acl(ACL *acl) : acl_{std::move(acl)} {}

  class HARNESS_EXPORT iterator {
   public:
    using value_type = Ace;
    using reference = value_type &;

    iterator(ACL *acl, size_t ndx) : acl_{acl}, ndx_{ndx} {}

    reference operator*();
    iterator &operator++();
    bool operator!=(const iterator &other) const;

   private:
    ACL *acl_;
    size_t ndx_;

    value_type ace_{nullptr};
  };

  iterator begin() const { return {acl_, 0}; }
  iterator end() const { return {acl_, size()}; }

  size_t size() const;

  std::string to_string() const;

 private:
  ACL *acl_;
};

/**
 * Allowed Access ACE (Access Control Entry).
 */
class HARNESS_EXPORT AccessAllowedAce {
 public:
  explicit AccessAllowedAce(ACCESS_ALLOWED_ACE *ace) : ace_{ace} {}

  ACCESS_MASK mask() const { return ace_->Mask; }
  Sid sid() const { return reinterpret_cast<SID *>(&ace_->SidStart); }

  std::string to_string() const;

 private:
  ACCESS_ALLOWED_ACE *ace_;
};

/**
 * a optional DACL.
 *
 * a optional DACL allows to differentiate between an empty DACL and a no DACL:
 *
 * - if no DACL is set, everything is allowed.
 * - if a DACL is set, but empty, nothing is allowed.
 */
using OptionalDacl = std::optional<ACL *>;

/**
 * Security Descriptor.
 *
 * contains a DACL.
 *
 * may be in absolute or self-relative form.
 */
class HARNESS_EXPORT SecurityDescriptor {
 public:
  /**
   * wrap a native SECURITY_DESCRITOR pointer.
   *
   * does NOT take ownership of the SECURITY_DESCRIPTOR.
   */
  explicit SecurityDescriptor(SECURITY_DESCRIPTOR *desc) : desc_{desc} {}

  /**
   * initialize an security descriptor with a revision.
   *
   * the descriptor will:
   *
   * - have no SACL
   * - no DACL
   * - no owner
   * - no primary group
   * - all control flags set to false.
   */
  stdx::expected<void, std::error_code> initialize(
      DWORD revision = SECURITY_DESCRIPTOR_REVISION);

  /**
   * set optional ACL
   */
  stdx::expected<void, std::error_code> dacl(const OptionalDacl &dacl,
                                             bool dacl_defaulted);

  /**
   * get a optional ACL.
   */
  stdx::expected<OptionalDacl, std::error_code> dacl() const;

  /**
   * check if a security descriptor is self-relative.
   */
  bool is_self_relative() const {
    return control().value_or(0) & SE_SELF_RELATIVE;
  }

  /**
   * get the control bits of a security descritor.
   */
  stdx::expected<SECURITY_DESCRIPTOR_CONTROL, std::error_code> control() const;

  /**
   * transform a security descriptor into self-relative form.
   */
  stdx::expected<Allocated<SECURITY_DESCRIPTOR>, std::error_code>
  make_self_relative();

  /**
   * get string representation.
   */
  std::string to_string() const;

 private:
  SECURITY_DESCRIPTOR *desc_;
};

class HARNESS_EXPORT AclBuilder {
 public:
  struct WellKnownSid {
    WELL_KNOWN_SID_TYPE sid;
  };

  /**
   * identify a current-user-lookup.
   */
  struct CurrentUser {};

  /**
   * grant additional rights to a trustee identified by a SID.
   *
   * when applied will combine the specified rights with the existing allowed
   * or denied rights of the trustee.
   *
   * @param sid SID of the trustee
   * @param rights rights to grant.
   */
  static EXPLICIT_ACCESSW ace_grant_access(SID *sid, DWORD rights);

  /**
   * set rights of a trustee identified by a SID.
   *
   * when applied will set the specified rights of the trustee.
   *
   * @param sid SID of the trustee
   * @param rights rights to grant.
   */
  static EXPLICIT_ACCESSW ace_set_access(SID *sid, DWORD rights);

  /**
   * revoke access of a trustee identified by a SID.
   *
   * when applied will revoke the all rights of the trustee.
   *
   * @param sid SID of the trustee
   */
  static EXPLICIT_ACCESSW ace_revoke_access(SID *sid);

  /**
   * create a AclBuilder from a empty security descriptor.
   */
  AclBuilder();

  /**
   * create a AclBuilder from an existing security descritor.
   */
  explicit AclBuilder(security_descriptor_type old_desc);

  /**
   * grant additional access rights to the current user.
   */
  AclBuilder &grant(CurrentUser, DWORD rights);

  /**
   * grant additional access rights to a well-known-sid.
   */
  AclBuilder &grant(const WellKnownSid &owner, DWORD rights);

  AclBuilder &grant(Allocated<SID> sid, DWORD rights);

  AclBuilder &set(CurrentUser, DWORD rights);

  AclBuilder &set(const WellKnownSid &owner, DWORD rights);

  AclBuilder &set(Allocated<SID> sid, DWORD rights);

  AclBuilder &revoke(CurrentUser);

  AclBuilder &revoke(const WellKnownSid &owner);

  AclBuilder &revoke(Allocated<SID> sid);

  stdx::expected<security_descriptor_type, std::error_code> build();

 private:
  std::vector<Allocated<SID>> owned_sids_;

  std::error_code ec_{};
  std::vector<EXPLICIT_ACCESSW> perms_;
  mysql_harness::win32::access_rights::OptionalDacl dacl_;
  // Allocated<SECURITY_DESCRIPTOR>
  security_descriptor_type old_desc_;
};

class HARNESS_EXPORT AllowUserReadWritableVerifier {
 public:
  stdx::expected<void, std::error_code> operator()(
      const security_descriptor_type &perms);
};

class HARNESS_EXPORT DenyOtherReadWritableVerifier {
 public:
  stdx::expected<void, std::error_code> operator()(
      const security_descriptor_type &perms);
};

}  // namespace win32::access_rights
#endif

#ifdef _WIN32
using security_descriptor_type = win32::access_rights::security_descriptor_type;
#else
using security_descriptor_type = posix::access_rights::security_descriptor_type;
#endif

/**
 * fail access_rights_verify() if others can read or write or execute.
 */
using DenyOtherReadWritableVerifier =
#ifdef _WIN32
    win32::access_rights::DenyOtherReadWritableVerifier;
#else
    posix::access_rights::DenyPermissionVerifier<(S_IRWXO)>;
#endif

/**
 * fail access_rights_verify() if someone else then the owner of the file can
 * read or write.
 */
using AllowUserReadWritableVerifier =
#ifdef _WIN32
    win32::access_rights::AllowUserReadWritableVerifier;
#else
    posix::access_rights::AllowPermissionVerifier<(S_IRUSR | S_IWUSR)>;
#endif

/**
 * get a access rights of file.
 *
 * @param file_name of a file.
 */
HARNESS_EXPORT stdx::expected<security_descriptor_type, std::error_code>
access_rights_get(const std::string &file_name) noexcept;

/**
 * check if a security descriptor satisfies a verifier.
 */
template <class Func>
stdx::expected<void, std::error_code> access_rights_verify(
    const security_descriptor_type &rights, Func &&func) {
  return func(rights);
}

/**
 * set access rights of a file.
 */
HARNESS_EXPORT stdx::expected<void, std::error_code> access_rights_set(
    const std::string &file_name, const security_descriptor_type &sec_desc);

}  // namespace mysql_harness
#endif
