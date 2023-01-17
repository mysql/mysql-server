/*
 * Copyright (c) 2019, 2023, Oracle and/or its affiliates.
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

#include "plugin/x/client/xcompression_negotiator.h"

#include <string>
#include <vector>

#include "mysqlxclient/xcompression.h"

#include "plugin/x/client/mysqlxclient/xargument.h"
#include "plugin/x/client/mysqlxclient/xerror.h"
#include "plugin/x/client/validator/option_compression_validator.h"

namespace xcl {

namespace {

template <typename Validator>
class To_variable_validator : public Validator {
 public:
  using Base = Validator;
  using Array_of_enums = typename Base::Array_of_enums;
  using Array_of_strings = typename Base::Array_of_strings;

 public:
  Array_of_enums m_enum_result;
  Array_of_strings m_string_result;

  bool ignore_unkown_text_values() const override { return true; }

  void store_to_result(const Argument_value &value) {
    Base::store(nullptr, value);
  }

  void visit_translate_with_source(
      const Array_of_enums &enum_result,
      const Array_of_strings &string_result) override {
    m_enum_result = enum_result;
    m_string_result = string_result;
  }
};

}  // namespace

void Capabilities_negotiator::server_supports_algorithms(
    const Array_of_strings &server_supported_algorithms) {
  class Compression_algorithms_validator2
      : public Translate_array_validator<Compression_algorithm, Context,
                                         false> {
   public:
    Compression_algorithms_validator2()
        : Translate_array_validator(
              {{"DEFLATE_STREAM", Compression_algorithm::k_deflate},
               {"LZ4_MESSAGE", Compression_algorithm::k_lz4},
               {"ZSTD_STREAM", Compression_algorithm::k_zstd}}) {}

    void visit_translate(const Array_of_enums &algos) override {}
  };

  To_variable_validator<Compression_algorithms_validator2> validator;

  check_server_capability(&validator, server_supported_algorithms,
                          m_compression_negotiation_algorithm,
                          &m_choosen_algorithm, &m_choosen_algorithm_txt);
}

bool Capabilities_negotiator::is_negotiation_needed() const {
  return m_compression_mode != Compression_negotiation::k_disabled;
}

bool Capabilities_negotiator::update_compression_options(
    Compression_algorithm *out_algorithm, XError *out_error) {
  if (!was_chooses()) {
    if (is_compression_required()) {
      *out_error =
          XError{CR_X_REQUIRED_COMPRESSION_NOT_SUPPORTED,
                 "Client's requirement for compression configuration is "
                 "not supported by server or it was disabled"};
    }

    return false;
  }

  *out_algorithm = m_choosen_algorithm;

  return true;
}

bool Capabilities_negotiator::is_compression_preferred() const {
  return m_compression_mode == Compression_negotiation::k_preferred;
}

bool Capabilities_negotiator::is_compression_required() const {
  return m_compression_mode == Compression_negotiation::k_required;
}

bool Capabilities_negotiator::was_chooses() const {
  if (m_choosen_algorithm == Compression_algorithm::k_none) return false;

  if (!m_compression_negotiation_algorithm.empty() &&
      m_choosen_algorithm == Compression_algorithm::k_none)
    return false;

  return true;
}

}  // namespace xcl
