/*
Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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
#ifndef ROUTING_PROTOCOL_INCLUDED
#define ROUTING_PROTOCOL_INCLUDED

#include "mysqlrouter/base_protocol.h"

#include <stdexcept>
#include <string>

class Protocol final {
 public:
  using Type = BaseProtocol::Type;

  static inline Type get_default() { return Type::kClassicProtocol; }

  /** @brief Returns type of the protocol by its name
   */
  static Type get_by_name(const std::string &name) {
    Type result = Type::kClassicProtocol;

    if (name == "classic") {
      /* already set */
    } else if (name == "x") {
      result = Type::kXProtocol;
    } else {
      throw std::invalid_argument("Invalid protocol name: '" + name + "'");
    }

    return result;
  }

  static std::string to_string(const Type &type) {
    std::string result;

    switch (type) {
      case Type::kClassicProtocol:
        result = "classic";
        break;
      case Type::kXProtocol:
        result = "x";
    }

    return result;
  }

  /** @brief Returns default port for the selected protocol
   */
  static uint16_t get_default_port(Type type) {
    uint16_t result{0};

    switch (type) {
      case Type::kClassicProtocol:
        result = kClassicProtocolDefaultPort;
        break;
      case Type::kXProtocol:
        result = kXProtocolDefaultPort;
        break;
      default:
        throw std::invalid_argument("Invalid protocol: " +
                                    std::to_string(static_cast<int>(type)));
    }

    return result;
  }

 private:
  /** @brief default server ports for supported protocols */
  static constexpr uint16_t kClassicProtocolDefaultPort{3306};
  static constexpr uint16_t kXProtocolDefaultPort{33060};
};

#endif  // ROUTING_PROTOCOL_INCLUDED
