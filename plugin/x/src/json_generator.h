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

#ifndef PLUGIN_X_SRC_JSON_GENERATOR_H_
#define PLUGIN_X_SRC_JSON_GENERATOR_H_

#include <algorithm>

#include "plugin/x/src/expr_generator.h"

namespace xpl {

class Json_generator : public Expression_generator {
 public:
  Json_generator(Query_string_builder *qb)
      : Expression_generator(qb, Expression_generator::Arg_list(), "", true) {}

  virtual ~Json_generator() override = default;

  using Expression_generator::generate;
  void generate(const Mysqlx::Datatypes::Object &arg) const override;
  void generate(const Mysqlx::Datatypes::Array &arg) const override;
  void generate(const Mysqlx::Datatypes::Scalar::Octets &arg) const override;

  template <typename T>
  void generate_json_for_each(const Repeated_field_list<T> &list) const {
    if (list.empty()) return;
    std::for_each(std::begin(list), std::end(list) - 1, [this](const T &arg) {
      generate(arg);
      m_qb->put(",");
    });
    generate(*std::prev(std::end(list)));
  }

 private:
  void handle_object_field(
      const Mysqlx::Datatypes::Object::ObjectField &arg) const override;
  void handle_string_scalar(
      const Mysqlx::Datatypes::Scalar &string_scalar) const override;
  void handle_bool_scalar(
      const Mysqlx::Datatypes::Scalar &bool_scalar) const override;
};

template <typename T>
void generate_json(Query_string_builder *qb, const T &expr) {
  Json_generator gen(qb);
  gen.feed(expr);
}

}  // namespace xpl

#endif  // PLUGIN_X_SRC_JSON_GENERATOR_H_
