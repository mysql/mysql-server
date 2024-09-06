// Copyright (c) 2024, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with MySQL.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_ABI_HELPERS_PACKET_H
#define MYSQL_ABI_HELPERS_PACKET_H

/// @file
/// Experimental API header

#include <cstddef>  // std::size_t
#include <cstring>  // std::memcpy
#include "array_view.h"
#include "field.h"
#ifdef MYSQL_SERVER
#include "my_sys.h"                     // MY_WME
#include "mysql/psi/psi_memory.h"       // PSI_memory_key
#include "mysql/service_mysql_alloc.h"  // my_malloc
#endif

/// @addtogroup GroupLibsMysqlAbiHelpers
/// @{

namespace mysql::abi_helpers {

/// @brief Class to store a number of fields of heterogeneous types, in a way
/// that provides ABI compatibility.
///
/// This provides ABI compatibility in two ways:
///
/// - Packets can be passed as function parameters in calls between two shared
/// objects that are linked dynamically, even if compiled with different
/// options/compilers. This is works because the class satisfies
/// std::is_standard_layout, which enforces a defined order of struct members.
/// This holds only if memory alignment is the same, which is not guaranteed by
/// the C++ standard, but typicall is the same across compilers on the same
/// system. If stricter guarantees on memory alignment is ever needed, this
/// needs to be replaced by something else.
///
/// - Packet definitions can evolve over time, by adding new type codes and
/// abandoning (but not removing) old ones, and then packets can be shared
/// between shared objects that expect different packet definitions.
///
/// @tparam Type_enum_t Enumeration type to use for the type code in the Field
/// objects.
template <class Type_enum_t>
using Packet = Array_view<Field<Type_enum_t>>;

#ifdef MYSQL_SERVER
/// @brief Class to help constructing a @c Packet, by pushing values one by one.
///
/// @tparam Type_enum_t Enumeration for the type codes.
template <class Type_enum_t>
class Packet_builder {
  using Packet_t = Packet<Type_enum_t>;

 public:
  /// Construct a new Packet_builder that can be used to store data in the given
  /// @c Packet.
  ///
  /// @param packet Target @c Packet.
  Packet_builder(Packet_t &packet) : m_packet(packet), m_position(0) {}

  /// @brief Append an int field.
  ///
  /// @param type the field type
  /// @param value the field value
  void push_int(Type_enum_t type, long long value) {
    m_packet[m_position].m_type = type;
    m_packet[m_position].m_data.m_int = value;
    m_position++;
  }

  /// @brief Append a string field, taking a copy of the parameter (raw pointer
  /// and length).
  ///
  /// @param type The field type
  /// @param value The string to copy (not necessarily null-terminated).
  /// @param length The number of bytes to copy
  /// @param key PSI_memory_key used to track the allocation.
  void push_string_copy(Type_enum_t type, const char *value, std::size_t length,
                        PSI_memory_key key) {
    char *copy = (char *)my_malloc(length + 1, key, MYF(MY_WME));
    std::memcpy(copy, value, length);
    copy[length] = '\0';
    push_string_view(type, copy);
  }

  /// @brief Append a string field, taking a copy of the parameter (raw pointer
  /// to null-terminated string).
  ///
  /// @param type The field type
  /// @param value The string to copy (null-terminated).
  /// @param key PSI_memory_key used to track the allocation.
  void push_string_copy(Type_enum_t type, const char *value,
                        PSI_memory_key key) {
    push_string_copy(type, value, std::strlen(value), key);
  }

  /// @brief Append a string field, taking a copy of the parameter
  /// (std::string).
  ///
  /// @param type The field type.
  /// @param value The string to copy.
  /// @param key PSI_memory_key used to track the allocation.
  void push_string_copy(Type_enum_t type, const std::string &value,
                        PSI_memory_key key) {
    push_string_copy(type, value.data(), value.size(), key);
  }

  /// @brief Append a string field, sharing memory with the caller (raw pointer
  /// to null-terminated string).
  ///
  /// @param type The field type.
  /// @param value The string to push (null-terminated).
  void push_string_view(Type_enum_t type, char *value) {
    m_packet[m_position].m_type = type;
    m_packet[m_position].m_data.m_string = value;
    m_position++;
  }

  /// @brief Append a string field, sharing memory with the caller
  /// (std::string).
  ///
  /// @param type The field type.
  /// @param value String object. The pointer `value.c_str()` will be pushed.
  void push_string_view(Type_enum_t type, const std::string &value) {
    push_string_view(type, value.c_str());
  }

  /// @brief Append a boolean field.
  /// @param type The field type.
  /// @param value The boolean value.
  void push_bool(Type_enum_t type, bool value) {
    m_packet[m_position].m_type = type;
    m_packet[m_position].m_data.m_bool = value;
    m_position++;
  }

  /// @return The current position.
  std::size_t get_position() { return m_position; }

 private:
  Packet_t &m_packet;
  std::size_t m_position;
};
#endif  // ifdef MYSQL_SERVER

}  // namespace mysql::abi_helpers

// addtogroup GroupLibsMysqlAbiHelpers
/// @}

#endif  // ifndef MYSQL_ABI_HELPERS_PACKET_H
