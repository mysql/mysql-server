/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_ROUTING_GUIDELINES_DATATYPES_INCLUDED
#define MYSQLROUTER_ROUTING_GUIDELINES_DATATYPES_INCLUDED

#include "mysqlrouter/metadata_cache_export.h"

namespace routing_guidelines {

enum class routing_guidelines_errc {
  empty_routing_guidelines,
  not_supported_in_md,
  unsupported_version,
  parse_error
};

}  // namespace routing_guidelines

namespace std {
template <>
struct is_error_code_enum<routing_guidelines::routing_guidelines_errc>
    : public std::true_type {};
}  // namespace std

namespace routing_guidelines {
inline const std::error_category &routing_guidelines_category() noexcept {
  class routing_guidelines_category_impl : public std::error_category {
   public:
    const char *name() const noexcept override { return "routing guidelines"; }
    std::string message(int ev) const override {
      switch (static_cast<routing_guidelines_errc>(ev)) {
        case routing_guidelines_errc::empty_routing_guidelines:
          return "guidelines document is empty";
        case routing_guidelines_errc::not_supported_in_md:
          return "routing guidelines not supported in current metadata version";
        case routing_guidelines_errc::unsupported_version:
          return "routing guidelines version not supported";
        case routing_guidelines_errc::parse_error:
          return "errors while parsing routing guidelines document";
        default:
          return "unknown";
      }
    }
  };

  static routing_guidelines_category_impl instance;
  return instance;
}

inline std::error_code make_error_code(routing_guidelines_errc e) noexcept {
  return std::error_code(static_cast<int>(e), routing_guidelines_category());
}

}  // namespace routing_guidelines

#endif  // MYSQLROUTER_ROUTING_GUIDELINES_DATATYPES_INCLUDED
