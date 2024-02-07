/*
 * Copyright (c) 2020, 2024, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_META_SCHEMA_VALIDATOR_H_
#define PLUGIN_X_SRC_META_SCHEMA_VALIDATOR_H_

#include "my_rapidjson_size_t.h"  // NOLINT(build/include_subdir)

#include <rapidjson/document.h>  // NOLINT(build/include_order)
#include <rapidjson/pointer.h>   // NOLINT(build/include_order)
#include <rapidjson/schema.h>    // NOLINT(build/include_order)

#include <string>  // NOLINT(build/include_order)

#include "plugin/x/src/ngs/error_code.h"
#include "plugin/x/src/ngs/protocol/protocol_protobuf.h"
#include "plugin/x/src/xpl_error.h"

namespace xpl {

class Meta_schema_validator {
 public:
  using Any = ::Mysqlx::Datatypes::Any;

  static const char *const k_reference_schema;

  explicit Meta_schema_validator(const uint32_t max_depth = 100)
      : m_max_depth{max_depth} {}

  ngs::Error_code validate(const std::string &schema) const;
  ngs::Error_code validate(const Any &schema, std::string *schema_string) const;

 private:
  ngs::Error_code validate_impl(rapidjson::Document *input_document) const;
  const rapidjson::Document &get_schema_document() const;
  const rapidjson::SchemaDocument &get_meta_schema() const;
  ngs::Error_code pre_validate_impl(const uint32_t level,
                                    const rapidjson::Pointer &pointer,
                                    rapidjson::Document *document,
                                    rapidjson::Value *value) const;
  ngs::Error_code pre_validate(rapidjson::Document *document) const;

  const uint32_t m_max_depth;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_META_SCHEMA_VALIDATOR_H_
