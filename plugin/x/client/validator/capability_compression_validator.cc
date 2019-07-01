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

#include "plugin/x/client/validator/capability_compression_validator.h"

#include <algorithm>
#include <array>

#include "mysqlxclient/xcompression.h"

#include "my_dbug.h"  // NOLINT(build/include_subdir)

#include "plugin/x/client/context/xcontext.h"
#include "plugin/x/client/validator/translation_validator.h"

namespace xcl {

namespace {

class Capability_algorithm_validator
    : public Translate_validator<Compression_algorithm, Compression_algorithm> {
 public:
  Capability_algorithm_validator()
      : Translate_validator({{"deflate", Compression_algorithm::k_deflate},
                             {"lz4", Compression_algorithm::k_lz4}}) {}

  void visit_translate(const Compression_algorithm algorithm) override {
    DBUG_TRACE;
    *get_ctxt() = algorithm;
  }
};

class Capability_style_validator
    : public Translate_validator<Compression_style, Compression_style> {
 public:
  Capability_style_validator()
      : Translate_validator({{"single", Compression_style::k_single},
                             {"multiple", Compression_style::k_multiple},
                             {"group", Compression_style::k_group}}) {}

  void visit_translate(const Compression_style style) override {
    DBUG_TRACE;
    *get_ctxt() = style;
  }
};

const char *const k_algorithm_key = "algorithm";
const char *const k_server_style_key = "server_style";
const char *const k_client_style_key = "client_style";

class Type_visitor : public Default_visitor {
 public:
  void visit_object(const Argument_object &values) override {
    DBUG_TRACE;
    m_valid = true;
    for (const auto &value : values) {
      m_valid =
          m_valid && (value.second.type() == Argument_value::Type::k_string);
    }
  }

  bool is_valid() const { return m_valid; }

 private:
  bool m_valid{false};
};

class Value_visitor : public Default_visitor {
 public:
  void visit_object(const Argument_object &object) override {
    DBUG_TRACE;
    static const std::array<const char *const, 3> keys = {
        k_algorithm_key, k_server_style_key, k_client_style_key};

    m_valid = true;
    for (const auto &field : object)
      m_valid =
          m_valid &&
          std::any_of(keys.begin(), keys.end(), [&field](const char *const k) {
            return std::strcmp(field.first.c_str(), k) == 0;
          });

    m_valid =
        m_valid && object.size() > 1 && object.size() < 4 &&
        valid_field_value(object, k_algorithm_key, &m_algorithm_validator) &&
        (valid_field_value(object, k_server_style_key, &m_style_validator) ||
         valid_field_value(object, k_client_style_key, &m_style_validator));
  }

  template <typename Validator>
  bool valid_field_value(const Argument_object &value, const std::string &key,
                         Validator *validator) const {
    const auto i = value.find(key);
    if (i == value.end()) return false;
    return validator->valid_value(i->second);
  }

  bool is_valid() const { return m_valid; }

 private:
  bool m_valid{false};
  Capability_algorithm_validator m_algorithm_validator;
  Capability_style_validator m_style_validator;
};

class Storage_visitor : public Default_visitor {
 public:
  explicit Storage_visitor(Context *context) : m_context{context} {}

  void visit_object(const Argument_object &object) override {
    DBUG_TRACE;
    store_value(object, k_algorithm_key, &m_algorithm_validator,
                &m_context->m_compression_config.m_use_algorithm);
    store_value(object, k_server_style_key, &m_style_validator,
                &m_context->m_compression_config.m_use_server_style);
    store_value(object, k_client_style_key, &m_style_validator,
                &m_context->m_compression_config.m_use_client_style);
  }

  template <typename Validator>
  void store_value(const Argument_object &object, const std::string &key,
                   Validator *validator,
                   typename Validator::Context *value) const {
    const auto i = object.find(key);
    if (i != object.end()) validator->store(value, i->second);
  }

  Context *m_context;
  Capability_algorithm_validator m_algorithm_validator;
  Capability_style_validator m_style_validator;
};

}  // namespace

bool Capability_compression_validator::valid_type(const Argument_value &value) {
  DBUG_TRACE;
  if (!Object_validator::valid_type(value)) return false;
  Type_visitor check;
  value.accept(&check);
  return check.is_valid();
}

bool Capability_compression_validator::valid_value(
    const Argument_value &value) {
  DBUG_TRACE;
  Value_visitor check;
  value.accept(&check);
  return check.is_valid();
}

void Capability_compression_validator::store(void *context,
                                             const Argument_value &value) {
  DBUG_TRACE;
  Storage_visitor store{reinterpret_cast<Context *>(context)};
  value.accept(&store);
}

}  // namespace xcl
