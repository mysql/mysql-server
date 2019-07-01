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
#ifndef PLUGIN_X_CLIENT_VALIDATOR_OPTION_COMPRESSION_VALIDATOR_H_
#define PLUGIN_X_CLIENT_VALIDATOR_OPTION_COMPRESSION_VALIDATOR_H_

#include <string>

#include "mysqlxclient/xcompression.h"

#include "plugin/x/client/context/xcontext.h"
#include "plugin/x/client/validator/translation_validator.h"

namespace xcl {

class Compression_negotiation_validator
    : public Translate_validator<Compression_negotiation, Context, false> {
 public:
  Compression_negotiation_validator()
      : Translate_validator(
            {{"PREFERRED", Compression_negotiation::k_preferred},
             {"DISABLED", Compression_negotiation::k_disabled},
             {"REQUIRED", Compression_negotiation::k_required}}) {}

  void visit_translate(const Compression_negotiation mode) override {
    DBUG_TRACE;
    get_ctxt()->m_compression_config.m_negotiator.m_compression_mode = mode;
  }
};

class Compression_algorithms_validator
    : public Translate_array_validator<Compression_algorithm, Context, false> {
 public:
  Compression_algorithms_validator()
      : Translate_array_validator(
            {{"DEFLATE", Compression_algorithm::k_deflate},
             {"LZ4", Compression_algorithm::k_lz4}}) {}

  void visit_translate(const Array_of_enums &algos) override {
    DBUG_TRACE;
    get_ctxt()
        ->m_compression_config.m_negotiator
        .m_compression_negotiation_algorithm = algos;
  }
};

class Compression_client_styles_validator
    : public Translate_array_validator<Compression_style, Context, false> {
 public:
  Compression_client_styles_validator()
      : Translate_array_validator({{"SINGLE", Compression_style::k_single},
                                   {"MULTIPLE", Compression_style::k_multiple},
                                   {"GROUP", Compression_style::k_group}}) {}

  bool ignore_empty_array() const override {
    DBUG_TRACE;
    return true;
  }

  void visit_translate(const Array_of_enums &styles) override {
    DBUG_TRACE;
    get_ctxt()
        ->m_compression_config.m_negotiator
        .m_compression_negotiation_client_style = styles;
  }
};

class Compression_server_styles_validator
    : public Compression_client_styles_validator {
 public:
  void visit_translate(const Array_of_enums &styles) override {
    DBUG_TRACE;
    get_ctxt()
        ->m_compression_config.m_negotiator
        .m_compression_negotiation_server_style = styles;
  }
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_VALIDATOR_OPTION_COMPRESSION_VALIDATOR_H_
