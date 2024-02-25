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

#ifndef MYSQL_ROUTER_CLASSIC_PROTOCOL_WIRE_H_
#define MYSQL_ROUTER_CLASSIC_PROTOCOL_WIRE_H_

#include <cstddef>
#include <string>

namespace classic_protocol {

namespace borrowable {
namespace wire {

template <bool Borrowed>
class String {
 public:
  using value_type =
      std::conditional_t<Borrowed, std::string_view, std::string>;

  constexpr String() = default;
  constexpr String(value_type str) : str_{std::move(str)} {}

  constexpr value_type value() const { return str_; }

 private:
  value_type str_;
};

template <bool Borrowed>
inline bool operator==(const String<Borrowed> &lhs,
                       const String<Borrowed> &rhs) {
  return lhs.value() == rhs.value();
}

template <bool Borrowed>
class NulTermString : public String<Borrowed> {
 public:
  using String<Borrowed>::String;
};

template <bool Borrowed>
class VarString : public String<Borrowed> {
 public:
  using String<Borrowed>::String;
};

// basic POD types of the mysql classic-protocol's wire encoding:
//
// - fixed size integers
// - variable sized integers
// - fixed size strings
// - variable sized strings
// - nul-terminated strings
// - NULL

template <class U>
class BasicInt {
 public:
  using value_type = U;

  constexpr BasicInt(value_type val) : val_{val} {}

  constexpr value_type value() const { return val_; }

 private:
  value_type val_;
};

template <class U>
constexpr bool operator==(const BasicInt<U> &lhs, const BasicInt<U> &rhs) {
  return lhs.value() == rhs.value();
}

class VarInt : public BasicInt<int64_t> {
 public:
  using BasicInt::BasicInt;
};

template <int Size>
class FixedInt;

template <>
class FixedInt<1> : public BasicInt<uint8_t> {
 public:
  using BasicInt::BasicInt;
};

template <>
class FixedInt<2> : public BasicInt<uint16_t> {
 public:
  using BasicInt::BasicInt;
};

template <>
class FixedInt<3> : public BasicInt<uint32_t> {
 public:
  using BasicInt::BasicInt;
};

template <>
class FixedInt<4> : public BasicInt<uint32_t> {
 public:
  using BasicInt::BasicInt;
};

template <>
class FixedInt<8> : public BasicInt<uint64_t> {
 public:
  using BasicInt::BasicInt;
};

class Null {};
}  // namespace wire
}  // namespace borrowable

namespace borrowed {
namespace wire {

using String = borrowable::wire::String<true>;
using NulTermString = borrowable::wire::NulTermString<true>;
using VarString = borrowable::wire::VarString<true>;
using Null = borrowable::wire::Null;
template <int Size>
using FixedInt = borrowable::wire::FixedInt<Size>;
using VarInt = borrowable::wire::VarInt;
}  // namespace wire
}  // namespace borrowed

namespace wire {

using String = borrowable::wire::String<false>;
using NulTermString = borrowable::wire::NulTermString<false>;
using VarString = borrowable::wire::VarString<false>;
using Null = borrowable::wire::Null;

template <int Size>
using FixedInt = borrowable::wire::FixedInt<Size>;

using VarInt = borrowable::wire::VarInt;
}  // namespace wire

}  // namespace classic_protocol

#endif
