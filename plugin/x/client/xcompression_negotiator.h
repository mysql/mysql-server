/*
 * Copyright (c) 2019, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

// MySQL DB access module, for use by plugins and others
// For the module that implements interactive DB functionality see mod_db

#ifndef PLUGIN_X_CLIENT_XCOMPRESSION_NEGOTIATOR_H_
#define PLUGIN_X_CLIENT_XCOMPRESSION_NEGOTIATOR_H_

#include <string>
#include <vector>

#include "mysqlxclient/xargument.h"
#include "mysqlxclient/xcompression.h"
#include "mysqlxclient/xerror.h"

namespace xcl {

class Capabilities_negotiator {
 public:
  using Compression_algorithms = std::vector<Compression_algorithm>;
  using Array_of_strings = std::vector<std::string>;

 public:
  void server_supports_algorithms(
      const Array_of_strings &server_supported_algorithms);

  bool is_negotiation_needed() const;
  bool is_compression_preferred() const;
  bool update_compression_options(Compression_algorithm *out_algorithm,
                                  XError *out_Error);

 public:
  // Following variables represent what client supports (where user can limit
  // those lists).
  //
  // The order is important, its sorted from most preferred.
  //
  Compression_algorithms m_compression_negotiation_algorithm{
      Compression_algorithm::k_deflate, Compression_algorithm::k_lz4,
      Compression_algorithm::k_zstd};

  Compression_negotiation m_compression_mode =
      Compression_negotiation::k_disabled;

 private:
  template <typename Validator_type, typename Required_type,
            typename Output1_type, typename Output2_type>
  void check_server_capability(
      Validator_type *validator,
      const Array_of_strings &server_suppored_capability,
      const Required_type &client_requested_capability,
      Output1_type *out_value1, Output2_type *out_value2) {
    Argument_value server_suppored_capability_av{server_suppored_capability};

    if (validator->valid_value(server_suppored_capability_av)) {
      validator->store_to_result(server_suppored_capability_av);

      for (const auto needed_capability : client_requested_capability) {
        for (size_t index = 0; index < validator->m_enum_result.size();
             ++index) {
          if (validator->m_enum_result[index] == needed_capability) {
            *out_value1 = validator->m_enum_result[index];
            *out_value2 = validator->m_string_result[index];
            return;
          }
        }
      }
    }
  }
  bool was_chooses() const;
  bool is_compression_required() const;

  Compression_algorithm m_choosen_algorithm = Compression_algorithm::k_none;
  std::string m_choosen_algorithm_txt;
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_XCOMPRESSION_NEGOTIATOR_H_
