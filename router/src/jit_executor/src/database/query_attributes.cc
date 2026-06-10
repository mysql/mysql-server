/*
 * Copyright (c) 2017, 2026, Oracle and/or its affiliates.
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
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "database/query_attributes.h"

#include <field_types.h>
#include <mysql_com.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "mysql/harness/logging/logging.h"
#include "objects/polyglot_date.h"
#include "utils/utils_string.h"

namespace shcore {
namespace polyglot {
namespace database {

Query_attribute::Query_attribute(
    std::string n, std::unique_ptr<IQuery_attribute_value> v) noexcept
    : name(std::move(n)), value(std::move(v)) {}

Classic_query_attribute::Classic_query_attribute() noexcept = default;

Classic_query_attribute::Classic_query_attribute(int64_t val) {
  value.i = val;
  type = MYSQL_TYPE_LONGLONG;
  data_ptr = &value.i;
  size = sizeof(int64_t);
  is_null = false;
}

Classic_query_attribute::Classic_query_attribute(uint64_t val) {
  value.ui = val;
  type = MYSQL_TYPE_LONGLONG;
  data_ptr = &value.ui;
  size = sizeof(uint64_t);
  flags = UNSIGNED_FLAG;
  is_null = false;
}

Classic_query_attribute::Classic_query_attribute(double val) {
  value.d = val;
  type = MYSQL_TYPE_DOUBLE;
  data_ptr = &value.d;
  size = sizeof(double);
  is_null = false;
}

Classic_query_attribute::Classic_query_attribute(const std::string &val) {
  value.s = new std::string(val);
  type = MYSQL_TYPE_STRING;
  data_ptr = value.s->data();
  size = val.size();
  is_null = false;
}

Classic_query_attribute::Classic_query_attribute(const MYSQL_TIME &val,
                                                 enum_field_types t) {
  value.t = val;
  type = t;
  data_ptr = &value.t;
  size = sizeof(MYSQL_TIME);
  is_null = false;
}

Classic_query_attribute::~Classic_query_attribute() {
  if (type == MYSQL_TYPE_STRING) {
    delete value.s;
  }
}

void Classic_query_attribute::update_data_ptr() {
  switch (type) {
    case MYSQL_TYPE_LONGLONG:
      if (flags & UNSIGNED_FLAG) {
        data_ptr = &value.ui;
      } else {
        data_ptr = &value.i;
      }
      break;
    case MYSQL_TYPE_DOUBLE:
      data_ptr = &value.d;
      break;
    case MYSQL_TYPE_STRING:
      data_ptr = value.s->data();
      break;
    default:
      if (type != MYSQL_TYPE_NULL) {
        data_ptr = &value.t;
      } else {
        data_ptr = nullptr;
      }
  }
}

Classic_query_attribute &Classic_query_attribute::operator=(
    const Classic_query_attribute &other) {
  if (type == MYSQL_TYPE_STRING) {
    delete value.s;
  }

  value = other.value;
  type = other.type;
  size = other.size;
  flags = other.flags;

  if (type == MYSQL_TYPE_STRING) {
    value.s = new std::string(*other.value.s);
  }

  update_data_ptr();

  return *this;
}

Classic_query_attribute &Classic_query_attribute::operator=(
    Classic_query_attribute &&other) noexcept {
  if (type == MYSQL_TYPE_STRING) {
    delete value.s;
    value.s = nullptr;
  }

  std::swap(value, other.value);
  std::swap(type, other.type);
  std::swap(size, other.size);
  std::swap(flags, other.flags);

  update_data_ptr();

  return *this;
}

bool Query_attribute_store::set(const std::string &name,
                                const shcore::Value &value) {
  constexpr size_t MAX_QUERY_ATTRIBUTES = 32;
  constexpr size_t MAX_QUERY_ATTRIBUTE_LENGTH = 1024;

  // Searches for the element first
  const auto it = m_store.find(name);

  // Validates name for any new attribute
  if (it == m_store.end()) {
    if (m_order.size() >= MAX_QUERY_ATTRIBUTES) {
      m_exceeded.push_back(name);
      return false;
    }
    if (name.size() > MAX_QUERY_ATTRIBUTE_LENGTH) {
      m_invalid_names.push_back(name);
      return false;
    };
  }

  // Validates the value type
  auto type = value.get_type();
  if (type == shcore::Value_type::Undefined ||
      type == shcore::Value_type::Array || type == shcore::Value_type::Map ||
      // type == shcore::Value_type::Function ||
      type == shcore::Value_type::Binary ||
      type == shcore::Value_type::Object ||
      (type == shcore::Value_type::ObjectBridge &&
       !value.as_object_bridge<shcore::polyglot::Date>())) {
    m_unsupported_type.push_back(name);
    return false;
  }

  // Validates the string value lengths
  if (type == shcore::Value_type::String &&
      value.get_string().size() > MAX_QUERY_ATTRIBUTE_LENGTH) {
    m_invalid_value_length.push_back(name);
    return false;
  }

  // Inserts or updates the value
  m_store[name] = value;

  // Adds the new value to the order
  if (it == m_store.end()) {
    m_order.emplace_back(m_store.find(name)->first);
  }

  return true;
}

bool Query_attribute_store::set(const shcore::Dictionary_t &attributes) {
  bool ret_val = true;

  clear();

  for (auto it = attributes->begin(); it != attributes->end(); it++) {
    if (!set(it->first, it->second)) {
      ret_val = false;
    }
  }

  return ret_val;
}

void Query_attribute_store::handle_errors(bool raise_error) {
  std::vector<std::string> issues;
  auto validate = [&issues](const std::vector<std::string> &data,
                            const std::string &message) -> void {
    if (!data.empty()) {
      bool singular = data.size() == 1;
      issues.push_back(
          shcore::str_format(message.data(), (singular ? "" : "s"),
                             shcore::str_join(data, ", ").c_str()));
    }
  };

  validate(m_invalid_names,
           "The following query attribute%s exceed the maximum name length "
           "(1024): %s");

  validate(m_invalid_value_length,
           "The following query attribute%s exceed the maximum value length "
           "(1024): %s");

  validate(m_unsupported_type,
           "The following query attribute%s have an unsupported data type: %s");

  validate(m_exceeded,
           "The following query attribute%s exceed the maximum limit (32): %s");

  if (!issues.empty()) {
    std::string error = "Invalid query attributes found";

    if (!raise_error) error.append(", they will be ignored");

    const auto message = shcore::str_format(
        "%s: %s", error.c_str(), shcore::str_join(issues, "\n\t").c_str());

    if (raise_error) {
      clear();
      throw std::invalid_argument(message);
    } else {
      mysql_harness::logging::log_warning("%s", message.c_str());
    }
  }
}

void Query_attribute_store::clear() {
  m_order.clear();
  m_store.clear();
  m_invalid_names.clear();
  m_invalid_value_length.clear();
  m_unsupported_type.clear();
  m_exceeded.clear();
}

std::vector<Query_attribute> Query_attribute_store::get_query_attributes(
    const std::function<std::unique_ptr<IQuery_attribute_value>(
        const shcore::Value &)> &translator_cb) const {
  assert(translator_cb);

  std::vector<Query_attribute> attributes;
  attributes.reserve(m_order.size());

  for (const auto &name : m_order) {
    attributes.emplace_back(name, translator_cb(m_store.at(std::string{name})));
  }

  return attributes;
}

}  // namespace database
}  // namespace polyglot
}  // namespace shcore