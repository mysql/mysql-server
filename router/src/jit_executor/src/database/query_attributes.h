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

#ifndef ROUTER_SRC_JIT_EXECUTOR_SRC_DATABASE_QUERY_ATTRIBUTES_H_
#define ROUTER_SRC_JIT_EXECUTOR_SRC_DATABASE_QUERY_ATTRIBUTES_H_

#include <field_types.h>
#include <mysql_time.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "mysqlrouter/jit_executor_value.h"

namespace shcore {
namespace polyglot {
namespace database {

struct IQuery_attribute_value {
  virtual ~IQuery_attribute_value() = default;
};

struct Classic_query_attribute : public IQuery_attribute_value {
 public:
  Classic_query_attribute() noexcept;
  explicit Classic_query_attribute(int64_t val);
  explicit Classic_query_attribute(uint64_t val);
  explicit Classic_query_attribute(double val);
  explicit Classic_query_attribute(const std::string &val);
  Classic_query_attribute(const MYSQL_TIME &val, enum_field_types t);

  Classic_query_attribute(Classic_query_attribute &&other) {
    *this = std::move(other);
  }

  Classic_query_attribute(const Classic_query_attribute &other) {
    *this = other;
  }

  Classic_query_attribute &operator=(const Classic_query_attribute &other);
  Classic_query_attribute &operator=(Classic_query_attribute &&other) noexcept;

  ~Classic_query_attribute();

 private:
  void update_data_ptr();
  friend class Session;
  union {
    int64_t i;
    uint64_t ui;
    std::string *s;
    double d;
    MYSQL_TIME t;

  } value;
  enum_field_types type = MYSQL_TYPE_NULL;
  void *data_ptr = nullptr;
  unsigned long size = 0;
  bool is_null = true;
  int flags = 0;
};

/**
 * @brief Normalized query attribute.
 *
 * This class represents a normalized query attribute which in general consist
 * of  a name/value pair.
 *
 * The value must be valid for the target connector, so the
 * IQuery_attribute_value interface is used at this level.
 */
struct Query_attribute {
  Query_attribute(std::string n,
                  std::unique_ptr<IQuery_attribute_value> v) noexcept;

  std::string name;
  std::unique_ptr<IQuery_attribute_value> value;
};

/**
 * @brief Cache for query attributes to be associated to the next user SQL
 * executed.
 *
 * This class serves as container and validator for the query attributes
 * coming from the following API call
 *
 * - setQueryAttributes() API
 *
 * Since the defined attributes are meant to be associated to the next user
 * SQL executed, the data needs to be cached while that happens.
 */
class Query_attribute_store {
 public:
  bool set(const std::string &name, const shcore::Value &value);
  bool set(const shcore::Dictionary_t &attributes);
  void handle_errors(bool raise_error = true);
  void clear();
  std::vector<Query_attribute> get_query_attributes(
      const std::function<std::unique_ptr<IQuery_attribute_value>(
          const shcore::Value &)> &translator_cb) const;

 private:
  // Real store of valid query attributes
  std::unordered_map<std::string, shcore::Value> m_store;

  // Honors the order of the attributes when given through \query_attributes
  std::vector<std::string> m_order;

  // Used to store the list of invalid attributes
  std::vector<std::string> m_exceeded;
  std::vector<std::string> m_invalid_names;
  std::vector<std::string> m_invalid_value_length;
  std::vector<std::string> m_unsupported_type;
};

}  // namespace database
}  // namespace polyglot
}  // namespace shcore

#endif  // ROUTER_SRC_JIT_EXECUTOR_SRC_DATABASE_QUERY_ATTRIBUTES_H_