/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_ROUTER_CLASSIC_PROTOCOL_CODEC_ERROR_H_
#define MYSQL_ROUTER_CLASSIC_PROTOCOL_CODEC_ERROR_H_

// error-domain for classic_protocol::codec errors

#include <system_error>  // error_code

namespace classic_protocol {

enum class codec_errc {
  // precondition failed like "first byte == cmd_byte()"
  invalid_input = 1,
  // not enough input to satisfy the length requirements like "FixedInt<1>"
  not_enough_input,
  // no nul-terminator found in input
  missing_nul_term,
  // capability not supported for this message
  capability_not_supported,
  // statement-id not found
  statement_id_not_found,
  // field-type unknown
  field_type_unknown
};
}  // namespace classic_protocol

namespace std {
template <>
struct is_error_code_enum<classic_protocol::codec_errc>
    : public std::true_type {};
}  // namespace std

namespace classic_protocol {
inline const std::error_category &codec_category() noexcept {
  class error_category_impl : public std::error_category {
   public:
    const char *name() const noexcept override { return "codec"; }
    std::string message(int ev) const override {
      switch (static_cast<codec_errc>(ev)) {
        case codec_errc::invalid_input:
          return "invalid input";
        case codec_errc::not_enough_input:
          return "input too short";
        case codec_errc::missing_nul_term:
          return "missing nul-terminator";
        case codec_errc::capability_not_supported:
          return "capability not supported";
        case codec_errc::statement_id_not_found:
          return "statement-id not found";
        case codec_errc::field_type_unknown:
          return "unknown field-type";
        default:
          return "unknown";
      }
    }
  };

  static error_category_impl instance;
  return instance;
}

inline std::error_code make_error_code(codec_errc e) noexcept {
  return {static_cast<int>(e), codec_category()};
}

}  // namespace classic_protocol

#endif
