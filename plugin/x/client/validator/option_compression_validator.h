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
            {{"DEFLATE_STREAM", Compression_algorithm::k_deflate},
             {"LZ4_MESSAGE", Compression_algorithm::k_lz4},
             {"ZSTD_STREAM", Compression_algorithm::k_zstd}}) {}

  void visit_translate(const Array_of_enums &algos) override {
    DBUG_TRACE;
    get_ctxt()
        ->m_compression_config.m_negotiator
        .m_compression_negotiation_algorithm = algos;
  }
};

template <int64_t(Compression_config::*member)>
class Compression_int_store
    : public Value_validator<Context, Integer_validator> {
 public:
  void visit_integer(const int64_t value) override {
    get_ctxt()->m_compression_config.*member = value;
  }
};

template <xpl::Optional_value<int32_t>(Compression_config::*member)>
class Compression_optional_int_store
    : public Value_validator<Context, Integer_validator> {
 public:
  void visit_integer(const int64_t value) override {
    get_ctxt()->m_compression_config.*member = value;
  }
};

template <bool(Compression_config::*member)>  // NOLINT(readability/casting)
class Compression_bool_store : public Value_validator<Context, Bool_validator> {
 public:
  void visit_bool(const bool value) override {
    get_ctxt()->m_compression_config.*member = value;
  }
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_VALIDATOR_OPTION_COMPRESSION_VALIDATOR_H_
