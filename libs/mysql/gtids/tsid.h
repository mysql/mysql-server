// Copyright (c) 2025, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_GTIDS_TSID_H
#define MYSQL_GTIDS_TSID_H

/// @file
/// Experimental API header

#include <concepts>                     // derived_from
#include <cstddef>                      // size_t
#include <string_view>                  // hash
#include "mysql/gtids/tag.h"            // Tag
#include "mysql/meta/not_decayed.h"     // Not_decayed
#include "mysql/strconv/strconv.h"      // Is_string_target
#include "mysql/utils/return_status.h"  // Return_status
#include "mysql/uuids/uuid.h"           // Uuid

/// @addtogroup GroupLibsMysqlGtids
/// @{

namespace mysql::gtids {

namespace detail {
/// Top of the hierarchy
class Tsid_base {};
}  // namespace detail

template <class Test>
concept Is_tsid = std::derived_from<Test, detail::Tsid_base>;

namespace detail {
/// Common interface, parameterized by tag type
template <class Self_tp, Is_tag Tag_tp>
class Tsid_interface : public Tsid_base {
 public:
  using Tag_t = Tag_tp;

  // "nolint": Problem not worth fixing, and workaround too intrusive.
  // NOLINTBEGIN(bugprone-crtp-constructor-accessibility)
  Tsid_interface() = default;
  explicit Tsid_interface(const mysql::uuids::Uuid &uuid,
                          const Is_tag auto &tag)
      : m_uuid(uuid), m_tag(tag) {}
  explicit Tsid_interface(const mysql::uuids::Uuid &uuid) : m_uuid(uuid) {
    m_tag.clear();
  }
  explicit Tsid_interface(const Is_tsid auto &other)
      : Tsid_interface(other.uuid(), other.tag()) {}
  // NOLINTEND(bugprone-crtp-constructor-accessibility)

  [[nodiscard]] const mysql::uuids::Uuid &uuid() const { return m_uuid; }
  [[nodiscard]] mysql::uuids::Uuid &uuid() { return m_uuid; }

  [[nodiscard]] const auto &tag() const { return m_tag; }
  [[nodiscard]] auto &tag() { return m_tag; }

  [[nodiscard]] auto assign(const Is_tsid auto &other) {
    uuid().assign(other.uuid());
    return tag().assign(other.tag());
  }

 private:
  mysql::uuids::Uuid m_uuid;
  Tag_t m_tag;
};  // class Tsid_interface

}  // namespace detail

class Tsid : public detail::Tsid_interface<Tsid, Tag> {
  using Base_t = detail::Tsid_interface<Tsid, Tag>;

 public:
  Tsid() = default;

  template <class... Args_t>
    requires mysql::meta::Not_decayed<Tsid, Args_t...>
  explicit Tsid(Args_t &&...args) : Base_t(std::forward<Args_t>(args)...) {}
};

class Tsid_trivial : public detail::Tsid_interface<Tsid_trivial, Tag_trivial> {
  using Base_t = detail::Tsid_interface<Tsid_trivial, Tag_trivial>;

 public:
  Tsid_trivial() = default;

  template <class... Args_t>
    requires mysql::meta::Not_decayed<Tsid_trivial, Args_t...>
  explicit Tsid_trivial(Args_t &&...args)
      : Base_t(std::forward<Args_t>(args)...) {}
};

bool operator==(const Is_tsid auto &tsid1, const Is_tsid auto &tsid2) {
  return tsid1.uuid() == tsid2.uuid() && tsid1.tag() == tsid2.tag();
}

bool operator!=(const Is_tsid auto &tsid1, const Is_tsid auto &tsid2) {
  return !(tsid1 == tsid2);
}

auto operator<=>(const Is_tsid auto &tsid1, const Is_tsid auto &tsid2) {
  auto uuid_cmp = tsid1.uuid() <=> tsid2.uuid();
  if (uuid_cmp != 0) return uuid_cmp;
  return tsid1.tag() <=> tsid2.tag();
}

}  // namespace mysql::gtids

/// Define std::hash<Tsid>.
///
// The recommended way to do this is to use a syntax that places the namespace
// as a name qualifier, like `struct std::hash<Gtid_t>`, rather than enclose
// the entire struct in a namespace block.
//
// However, gcc 11.4.0 on ARM has a bug that makes it produce "error:
// redefinition of 'struct std::hash<_Tp>'" when using that syntax. See
// https://godbolt.org/z/xo1v8rf6n vs https://godbolt.org/z/GzvrMese1 .
//
// Todo: Switch to the recommended syntax once we drop support for compilers
// having this bug.
//
// clang-tidy warns when not using the recommended syntax
// NOLINTBEGIN(cert-dcl58-cpp)
namespace std {
template <mysql::gtids::Is_tsid Tsid_t>
struct hash<Tsid_t> {
  std::size_t operator()(const Tsid_t &tsid) const {
    return std::hash(tsid.uuid()) ^ std::hash(tsid.tag());
  }
};
}  // namespace std
// NOLINTEND(cert-dcl58-cpp)

// addtogroup GroupLibsMysqlGtids
/// @}

#endif  // ifndef MYSQL_GTIDS_TSID_H
