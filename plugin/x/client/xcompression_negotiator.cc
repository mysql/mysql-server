/*
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
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

#include <string>
#include <vector>

#include "mysqlxclient/xcompression.h"

#include "plugin/x/client/mysqlxclient/xargument.h"
#include "plugin/x/client/mysqlxclient/xerror.h"
#include "plugin/x/client/xcompression_negotiator.h"

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

void Capabilities_negotiator::server_supports_client_styles(
    const Array_of_strings &server_supported_client_styles) {}

void Capabilities_negotiator::server_supports_server_styles(
    const Array_of_strings &server_supported_server_styles) {}

void Capabilities_negotiator::server_supports_algorithms(
    const Array_of_strings &server_supported_algorithms) {}

bool Capabilities_negotiator::is_negotiation_needed() const {
  return m_compression_mode != Compression_negotiation::k_disabled;
}

bool Capabilities_negotiator::update_compression_options(
    Compression_algorithm *out_algorithm, Compression_style *out_client_style,
    Compression_style *out_server_style, Capabilities_builder *out_builder,
    XError *out_error) {
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
  *out_client_style = m_choosen_client_style;
  *out_server_style = m_choosen_server_style;

  out_builder->clear();
  Argument_object obj;

  obj["algorithm"] = m_choosen_algorithm_txt;
  if (Compression_style::k_none != m_choosen_client_style)
    obj["client_style"] = m_choosen_client_style_txt;
  if (Compression_style::k_none != m_choosen_server_style)
    obj["server_style"] = m_choosen_server_style_txt;

  out_builder->add_capability("compression", Argument_value{obj});

  return true;
}

bool Capabilities_negotiator::is_compression_required() const {
  return m_compression_mode == Compression_negotiation::k_required;
}

bool Capabilities_negotiator::was_chooses() const {
  if (m_choosen_algorithm == Compression_algorithm::k_none) return false;

  if (m_choosen_client_style == Compression_style::k_none &&
      m_choosen_server_style == Compression_style::k_none)
    return false;

  if (!m_compression_negotiation_client_style.empty() &&
      m_choosen_client_style == Compression_style::k_none)
    return false;

  if (!m_compression_negotiation_server_style.empty() &&
      m_choosen_server_style == Compression_style::k_none)
    return false;

  if (!m_compression_negotiation_algorithm.empty() &&
      m_choosen_algorithm == Compression_algorithm::k_none)
    return false;

  return true;
}

}  // namespace xcl
