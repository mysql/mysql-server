/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

namespace wire {
// basic POD types of the mysql classic-protocol's wire encoding:
//
// - fixed size integers
// - variable sized integers
// - fixed size strings
// - variable sized strings
// - nul-terminated strings
// - NULL

class VarInt {
 public:
  using value_type = int64_t;

  constexpr VarInt(value_type v) : v_{v} {}

  constexpr value_type value() const noexcept { return v_; }

 private:
  value_type v_;
};

constexpr bool operator==(const VarInt &a, const VarInt &b) {
  return a.value() == b.value();
}

class String {
 public:
  String() : s_{} {}
  String(std::string s) : s_{std::move(s)} {}

  std::string value() const { return s_; }

 private:
  std::string s_;
};

inline bool operator==(const String &a, const String &b) {
  return a.value() == b.value();
}

class NulTermString : public String {
 public:
  using String::String;
};

class VarString : public String {
 public:
  using String::String;
};

template <int Size>
class FixedInt;

template <>
class FixedInt<1> {
 public:
  using value_type = uint8_t;

  constexpr FixedInt(value_type v) : v_{std::move(v)} {}

  constexpr value_type value() const { return v_; }

 private:
  value_type v_;
};

template <int Size>
constexpr bool operator==(const FixedInt<Size> &a, const FixedInt<Size> &b) {
  return a.value() == b.value();
}

template <>
class FixedInt<2> {
 public:
  using value_type = uint16_t;

  constexpr FixedInt(value_type v) : v_{std::move(v)} {}

  constexpr value_type value() const { return v_; }

 private:
  value_type v_;
};

template <>
class FixedInt<3> {
 public:
  using value_type = uint32_t;

  constexpr FixedInt(value_type v) : v_{std::move(v)} {}

  constexpr value_type value() const { return v_; }

 private:
  value_type v_;
};

template <>
class FixedInt<4> {
 public:
  using value_type = uint32_t;

  constexpr FixedInt(value_type v) : v_{std::move(v)} {}

  constexpr value_type value() const { return v_; }

 private:
  value_type v_;
};

template <>
class FixedInt<8> {
 public:
  using value_type = uint64_t;

  constexpr FixedInt(value_type v) : v_{std::move(v)} {}

  constexpr value_type value() const { return v_; }

 private:
  value_type v_;
};

class Null {};

}  // namespace wire
}  // namespace classic_protocol

#endif
