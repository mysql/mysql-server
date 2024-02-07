/*
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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
#ifndef PLUGIN_X_CLIENT_VALIDATOR_OPTION_SSL_VALIDATOR_H_
#define PLUGIN_X_CLIENT_VALIDATOR_OPTION_SSL_VALIDATOR_H_

#include <string>

#include "mysqlxclient/xcompression.h"

#include "plugin/x/client/context/xcontext.h"
#include "plugin/x/client/validator/translation_validator.h"

namespace xcl {

class Ssl_mode_validator
    : public Translate_validator<Ssl_config::Mode, Context, false> {
 public:
  Ssl_mode_validator()
      : Translate_validator(
            {{"PREFERRED", Ssl_config::Mode::Ssl_preferred},
             {"DISABLED", Ssl_config::Mode::Ssl_disabled},
             {"REQUIRED", Ssl_config::Mode::Ssl_required},
             {"VERIFY_CA", Ssl_config::Mode::Ssl_verify_ca},
             {"VERIFY_IDENTITY", Ssl_config::Mode::Ssl_verify_identity}}) {}

  void visit_translate(const Ssl_config::Mode mode) override {
    get_ctxt()->m_ssl_config.m_mode = mode;
  }
};

class Ssl_fips_validator
    : public Translate_validator<Ssl_config::Mode_ssl_fips, Context, false> {
 public:
  Ssl_fips_validator()
      : Translate_validator(
            {{"OFF", Ssl_config::Mode_ssl_fips::Ssl_fips_mode_off},
             {"ON", Ssl_config::Mode_ssl_fips::Ssl_fips_mode_on},
             {"STRICT", Ssl_config::Mode_ssl_fips::Ssl_fips_mode_strict}}) {}

  void visit_translate(const Ssl_config::Mode_ssl_fips fips) override {
    get_ctxt()->m_ssl_config.m_ssl_fips_mode = fips;
  }
};

template <typename std::string(Ssl_config::*member)>
class Ssl_str_store : public Value_validator<Context, String_validator> {
 public:
  void visit_string(const std::string &value) override {
    get_ctxt()->m_ssl_config.*member = value;
  }
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_VALIDATOR_OPTION_SSL_VALIDATOR_H_
