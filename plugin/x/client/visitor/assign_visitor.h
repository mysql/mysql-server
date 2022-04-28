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
#ifndef PLUGIN_X_CLIENT_VISITOR_ASSIGN_VISITOR_H_
#define PLUGIN_X_CLIENT_VISITOR_ASSIGN_VISITOR_H_

#include <string>

#include "mysqlxclient/xargument.h"

#include "plugin/x/client/visitor/default_visitor.h"

namespace xcl {

template <typename Destination_type>
class Assign_visitor : public Default_visitor {
 public:
  Destination_type m_destination;
  bool m_set = false;

 private:
  /*
    This template handles all other types than Destination_type
    that are hold in Argument_value.

    When user gets other type than stored in Argument_value, then this
    callback is executed.
  */
  template <typename Value1_type, typename Value2_type>
  bool assign(Value1_type *value1, const Value2_type *value2) {
    // Does nothing, types doesn't match
    return false;
  }

  /*
    This specialization template handles the Destination_type that is stored
    in Argument_value.

    When user gets correct type from Argument_value, then this callback is
    executed.
   */
  template <typename Value1_type>
  bool assign(Value1_type *value1, const Value1_type *value2) {
    *value1 = *value2;

    return true;
  }

  template <typename Value1_type, typename Value2_type>
  void store_and_mark(Value1_type *value1, const Value2_type *value2) {
    m_set = assign(value1, value2);
  }

 private:  // Visitor
  void visit_integer(const int64_t value) override {
    store_and_mark(&m_destination, &value);
  }
  void visit_uinteger(const uint64_t value) override {
    store_and_mark(&m_destination, &value);
  }
  void visit_double(const double value) override {
    store_and_mark(&m_destination, &value);
  }
  void visit_float(const float value) override {
    store_and_mark(&m_destination, &value);
  }
  void visit_bool(const bool value) override {
    store_and_mark(&m_destination, &value);
  }
  void visit_object(const Argument_object &value) override {
    store_and_mark(&m_destination, &value);
  }
  void visit_uobject(const Argument_uobject &value) override {
    store_and_mark(&m_destination, &value);
  }
  void visit_array(const Argument_array &value) override {
    store_and_mark(&m_destination, &value);
  }
  void visit_string(const std::string &value) override {
    store_and_mark(&m_destination, &value);
  }
  void visit_octet(const std::string &value) {
    store_and_mark(&m_destination, &value);
  }
  void visit_decimal(const std::string &value) override {
    store_and_mark(&m_destination, &value);
  }
};

template <typename Destination_type>
bool get_argument_value(const Argument_value &value,
                        Destination_type *out_value) {
  Assign_visitor<Destination_type> assign;

  value.accept(&assign);

  if (!assign.m_set) return false;

  *out_value = assign.m_destination;
  return true;
}

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_VISITOR_ASSIGN_VISITOR_H_
