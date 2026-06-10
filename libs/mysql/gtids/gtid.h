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

#ifndef MYSQL_GTIDS_GTID_H
#define MYSQL_GTIDS_GTID_H

/// @file
/// Experimental API header

#include <cassert>                        // assert
#include <concepts>                       // derived_from
#include <cstddef>                        // size_t
#include <stdexcept>                      // domain_error
#include <string_view>                    // hash
#include "mysql/gtids/sequence_number.h"  // Sequence_number
#include "mysql/gtids/tag.h"              // Tag
#include "mysql/gtids/tsid.h"             // Tsid
#include "mysql/utils/call_and_catch.h"   // call_and_catch
#include "mysql/utils/return_status.h"    // Return_status
#include "mysql/uuids/uuid.h"             // Uuid

/// @addtogroup GroupLibsMysqlGtids
/// @{

namespace mysql::gtids {

namespace detail {
/// Top of the hierarchy
class Gtid_base {};
}  // namespace detail

template <class Test>
concept Is_gtid = std::derived_from<Test, detail::Gtid_base>;

namespace detail {

/// Base class for classes representing a single Gtid, parameterized by the type
/// of the Tsid.
template <Is_tsid Tsid_tp>
class Gtid_interface : public Gtid_base {
 protected:
  /// Construct a new object from the given Uuid, Tag, and Sequence_number.
  ///
  /// @throw std::domain_error if the given number is out of range.
  explicit Gtid_interface(const mysql::uuids::Uuid &uuid,
                          const Is_tag auto &tag,
                          Sequence_number sequence_number)
      : m_tsid(uuid, tag), m_sequence_number(sequence_number) {
    assert_sequence_number(sequence_number);
  }

  /// Construct a new object from the given Uuid and Sequence_number, using an
  /// empty tag.
  ///
  /// @throw std::domain_error if the given number is out of range.
  explicit Gtid_interface(const mysql::uuids::Uuid &uuid,
                          Sequence_number sequence_number)
      : m_tsid(uuid), m_sequence_number(sequence_number) {
    assert_sequence_number(sequence_number);
  }

  /// Construct a new object from the given Tsid and Sequence_number.
  ///
  /// @throw std::domain_error if the given number is out of range.
  explicit Gtid_interface(const Is_tsid auto &tsid,
                          Sequence_number sequence_number)
      : m_tsid(tsid), m_sequence_number(sequence_number) {
    assert_sequence_number(sequence_number);
  }

 public:
  /// Construct a new, uninitialized object. Note that this must not initialize
  /// any members because we need the subclass Gtid_trivial to satisfy
  /// std::is_trivially_default_constructible.
  Gtid_interface() = default;

  /// Construct a new object by copying the given Gtid.
  explicit Gtid_interface(const Is_gtid auto &gtid)
      : m_tsid(gtid.tsid()), m_sequence_number(gtid.get_sequence_number()) {}

  /// Return const reference to the Tsid.
  [[nodiscard]] const auto &tsid() const { return m_tsid; }

  /// Return non-const reference to the Tsid.
  [[nodiscard]] auto &tsid() { return m_tsid; }

  /// Return const reference to the Uuid.
  [[nodiscard]] const auto &uuid() const { return m_tsid.uuid(); }

  /// Return non-const reference to the Uuid.
  [[nodiscard]] auto &uuid() { return m_tsid.uuid(); }

  /// Return const reference to the Tag.
  [[nodiscard]] const auto &tag() const { return m_tsid.tag(); }

  /// Return non-const reference to the Tag.
  [[nodiscard]] auto &tag() { return m_tsid.tag(); }

  /// Return the Sequence_number.
  [[nodiscard]] Sequence_number get_sequence_number() const {
    return m_sequence_number;
  }

  /// Set and validate the Sequence_number.
  ///
  /// @throw std::domain_error if the given number is out of range.
  void throwing_set_sequence_number(const Sequence_number &sequence_number) {
    assert_sequence_number(sequence_number);
    m_sequence_number = sequence_number;
  }

  /// Set and validate the Sequence_number.
  ///
  /// @return ok on success; error if the given number is out of range.
  [[nodiscard]] auto set_sequence_number(
      const Sequence_number &sequence_number) {
    return mysql::utils::call_and_catch([&sequence_number, this] {
      this->throwing_set_sequence_number(sequence_number);
    });
  }

  /// Copy from other to this.
  ///
  /// @return Currently `void`. If we introduce allocating tag types, the
  /// allocation may fail, in which case this will return Return_status.
  [[nodiscard]] auto assign(const Is_gtid auto &other) {
#ifndef NDEBUG
    assert_sequence_number(other.sequence_number);
#endif
    m_sequence_number = other.sequence_number;

    // Assignment may fail if the tag is allocating and an out-of-memory
    // condition occurs. If the tag is not allocating, other.assign returns void
    // and then this function returns void.
    return tsid().assign(other.tsid());
  }

 private:
  /// Assert that the sequence number is in range.
  ///
  /// @throw std::domain_error if the given number is out of range.
  void assert_sequence_number(Sequence_number sequence_number) const {
    if (sequence_number < sequence_number_min) {
      throw std::domain_error{"Out-of-range: sequence_number < minimum"};
    }
    if (sequence_number > sequence_number_max_inclusive) {
      throw std::domain_error{"Out-of-range: sequence_number > maximum"};
    }
  }

  /// The Tsid object.
  Tsid_tp m_tsid;

  /// The Sequence_number.
  ///
  /// This must not be initialized by default, because this is a base class for
  /// of Gtid_trivial, and initializing it would make Gtid_trivial violate
  /// std::is_trivially_default_constructible.
  Sequence_number m_sequence_number;
};  // class Gtid_base

}  // namespace detail

/// Represents a single Gtid, consisting of a Tsid and a Sequence_number.
///
/// The default constructor for this class will initialize the sequence number
/// to sequence_number_min, the tag to empty, and leave the UUID uninitialized.
/// Thus, it does not satisfy std::is_trivially_default_constructible.
class Gtid : public detail::Gtid_interface<Tsid> {
 private:
  using Base_t = detail::Gtid_interface<Tsid>;

 protected:
  /// Construct a new object from the given Uuid, Tag, and Sequence_number.
  ///
  /// @throw std::domain_error if the given number is out of range.
  explicit Gtid(const mysql::uuids::Uuid &uuid, const Is_tag auto &tag,
                Sequence_number sequence_number)
      : Base_t(uuid, tag, sequence_number) {}

  /// Construct a new object from the given Uuid and Sequence_number, using an
  /// empty tag.
  ///
  /// @throw std::domain_error if the given number is out of range.
  explicit Gtid(const mysql::uuids::Uuid &uuid, Sequence_number sequence_number)
      : Base_t(uuid, sequence_number) {}

  /// Construct a new object from the given Tsid and Sequence_number.
  ///
  /// @throw std::domain_error if the given number is out of range.
  explicit Gtid(const Is_tsid auto &tsid, Sequence_number sequence_number)
      : Base_t(tsid, sequence_number) {}

 public:
  /// Construct a new Gtid, leaving the UUID uninitialized, setting the tag to
  /// empty, and the sequence number to 1.
  Gtid() : Base_t({}, sequence_number_min) {}

  /// Construct a new object by copying the given Gtid.
  explicit Gtid(const Is_gtid auto &gtid) : Base_t(gtid) {}

  /// Return a new object constructed from the given Uuid, Tag, and
  /// Sequence_number.
  ///
  /// Use in exception-free code only if the sequence_number has been validated
  /// already.
  ///
  /// @throw std::domain_error if the given number is out of range.
  static Gtid throwing_make(const mysql::uuids::Uuid &uuid,
                            const Is_tag auto &tag,
                            Sequence_number sequence_number) {
    return Gtid(uuid, tag, sequence_number);
  }

  /// Return a new object constructed from the given Uuid and Sequence_number,
  /// using an empty tag.
  ///
  /// Use in exception-free code only if the sequence_number has been validated
  /// already.
  ///
  /// @throw std::domain_error if the given number is out of range.
  static Gtid throwing_make(const mysql::uuids::Uuid &uuid,
                            Sequence_number sequence_number) {
    return Gtid(uuid, sequence_number);
  }

  /// Return a new object constructed from the given Tsid and Sequence_number.
  ///
  /// Use in exception-free code only if the sequence_number has been validated
  /// already.
  ///
  /// @throw std::domain_error if the given number is out of range.
  static Gtid throwing_make(const Is_tsid auto &tsid,
                            Sequence_number sequence_number) {
    return Gtid(tsid, sequence_number);
  }
};

/// Represents a single Gtid, consisting of a Tsid and a Sequence_number.
///
/// The default constructor leaves all fields uninitialized. Thus, it satisfies
/// std::is_trivially_default_constructible.
class Gtid_trivial : public detail::Gtid_interface<Tsid_trivial> {
 private:
  using Base_t = detail::Gtid_interface<Tsid_trivial>;

 protected:
  /// Construct a new object from the given Uuid, Tag, and Sequence_number. This
  /// throws an exception if the sequence number is out of range.
  explicit Gtid_trivial(const mysql::uuids::Uuid &uuid, const Is_tag auto &tag,
                        Sequence_number sequence_number)
      : Base_t(uuid, tag, sequence_number) {}

  /// Construct a new object from the given Uuid and Sequence_number, using an
  /// empty tag. This throws an exception if the sequence number is out of
  /// range.
  explicit Gtid_trivial(const mysql::uuids::Uuid &uuid,
                        Sequence_number sequence_number)
      : Base_t(uuid, sequence_number) {}

  /// Construct a new object from the given Tsid and Sequence_number. This
  /// throws an exception if the sequence number is out of range.
  explicit Gtid_trivial(const Is_tsid auto &tsid,
                        Sequence_number sequence_number)
      : Base_t(tsid, sequence_number) {}

 public:
  /// Construct a new Gtid with all fields uninitialized.
  Gtid_trivial() = default;

  /// Construct a new object by copying the given Gtid.
  explicit Gtid_trivial(const Is_gtid auto &gtid) : Base_t(gtid) {}

  /// Return a new object constructed from the given Uuid, Tag, and
  /// Sequence_number. This throws an exception if the sequence number is out of
  /// range: use in exception-free code only if the sequence_number has been
  /// validated already.
  static Gtid_trivial throwing_make(const mysql::uuids::Uuid &uuid,
                                    const Is_tag auto &tag,
                                    Sequence_number sequence_number) {
    return Gtid_trivial(uuid, tag, sequence_number);
  }

  /// Return a new object constructed from the given Uuid and Sequence_number,
  /// using an empty tag. This throws an exception if the sequence number is out
  /// of range: use in exception-free code only if the sequence_number has been
  /// validated already.
  static Gtid_trivial throwing_make(const mysql::uuids::Uuid &uuid,
                                    Sequence_number sequence_number) {
    return Gtid_trivial(uuid, sequence_number);
  }

  /// Return a new object constructed from the given Tsid and Sequence_number.
  /// This throws an exception if the sequence number is out of range: use in
  /// exception-free code only if the sequence_number has been validated
  /// already.
  static Gtid_trivial throwing_make(const Is_tsid auto &tsid,
                                    Sequence_number sequence_number) {
    return Gtid_trivial(tsid, sequence_number);
  }
};

bool operator==(const Is_gtid auto &gtid1, const Is_gtid auto &gtid2) {
  return gtid1.tsid() == gtid2.tsid() &&
         gtid1.get_sequence_number() == gtid2.get_sequence_number();
}

bool operator!=(const Is_gtid auto &gtid1, const Is_gtid auto &gtid2) {
  return !(gtid1 == gtid2);
}

auto operator<=>(const Is_gtid auto &gtid1, const Is_gtid auto &gtid2) {
  auto tsid_cmp = gtid1.tsid() <=> gtid2.tsid();
  if (tsid_cmp != 0) return tsid_cmp;
  return gtid1.get_sequence_number() <=> gtid2.get_sequence_number();
}

}  // namespace mysql::gtids

/// Define std::hash<Gtid>.
//
// The recommended way to do this is to use a syntax that places the namespace
// as a name qualifier, like `struct std::hash<Gtid_t>`, rather than enclose the
// entire struct in a namespace block.
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
template <mysql::gtids::Is_gtid Gtid_t>
struct hash<Gtid_t> {
  std::size_t operator()(const Gtid_t &gtid) const {
    return std::hash(gtid.tsid()) ^ std::hash(gtid.sequence_number());
  }
};
}  // namespace std
// NOLINTEND(cert-dcl58-cpp)

// addtogroup GroupLibsMysqlGtids
/// @}

#endif  // ifndef MYSQL_GTIDS_GTID_H
