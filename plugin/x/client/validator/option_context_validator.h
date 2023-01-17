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
#ifndef PLUGIN_X_CLIENT_VALIDATOR_OPTION_CONTEXT_VALIDATOR_H_
#define PLUGIN_X_CLIENT_VALIDATOR_OPTION_CONTEXT_VALIDATOR_H_

#include <set>
#include <string>
#include <vector>

#include "mysqlxclient/xcompression.h"

#include "plugin/x/client/context/xcontext.h"
#include "plugin/x/client/validator/translation_validator.h"

namespace xcl {

class Contex_ip_validator
    : public Translate_validator<Internet_protocol, Context, false> {
 public:
  Contex_ip_validator()
      : Translate_validator({{"ANY", Internet_protocol::Any},
                             {"IP4", Internet_protocol::V4},
                             {"IP6", Internet_protocol::V6}}) {}

  void visit_translate(const Internet_protocol mode) override {
    get_ctxt()->m_internet_protocol = mode;
  }
};

class Contex_auth_validator
    : public Translate_array_validator<Auth, Context, false> {
 public:
  Contex_auth_validator()
      : Translate_array_validator(
            {{"AUTO", Auth::k_auto},
             {"FROM_CAPABILITIES", Auth::k_auto_from_capabilities},
             {"FALLBACK", Auth::k_auto_fallback},
             {"MYSQL41", Auth::k_mysql41},
             {"PLAIN", Auth::k_plain},
             {"SHA256_MEMORY", Auth::k_sha256_memory}}) {}

  bool valid_array_value(const Array_of_enums &values) override {
    const std::set<Auth> scalar_only_values{Auth::k_auto,
                                            Auth::k_auto_from_capabilities};

    if (values.size() == 1) return true;

    for (const auto v : values) {
      if (scalar_only_values.count(v)) return false;
    }

    return true;
  }

  void visit_translate(const std::vector<Auth> &auth) override {
    get_ctxt()->m_use_auth_methods = auth;
  }
};

template <uint32_t(Context::*member)>
class Ctxt_uint32_store : public Value_validator<Context, Integer_validator> {
 public:
  void visit_integer(const int64_t value) override {
    get_ctxt()->*member = static_cast<uint32_t>(value);
  }
};

template <bool(Context::*member)>  // NOLINT
class Ctxt_bool_store : public Value_validator<Context, Bool_validator> {
 public:
  void visit_bool(const bool value) override { get_ctxt()->*member = value; }
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_VALIDATOR_OPTION_CONTEXT_VALIDATOR_H_
