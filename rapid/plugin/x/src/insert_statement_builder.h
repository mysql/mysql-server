/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef INSERT_STATEMENT_BUILDER_H_
#define INSERT_STATEMENT_BUILDER_H_

#include "plugin/x/src/statement_builder.h"

namespace xpl {

class Insert_statement_builder : public Crud_statement_builder {
 public:
  using Insert = ::Mysqlx::Crud::Insert;

  explicit Insert_statement_builder(const Expression_generator &gen)
      : Crud_statement_builder(gen) {}

  void build(const Insert &msg) const;

 protected:
  using Projection_list =
      ::google::protobuf::RepeatedPtrField< ::Mysqlx::Crud::Column>;
  using Field_list =
      ::google::protobuf::RepeatedPtrField< ::Mysqlx::Expr::Expr>;
  using Row_list =
      ::google::protobuf::RepeatedPtrField< ::Mysqlx::Crud::Insert_TypedRow>;

  void add_projection(const Projection_list &projection,
                      const bool is_relational) const;
  void add_values(const Row_list &values, const int projection_size) const;
  void add_row(const Field_list &row, const int projection_size) const;
  const Field_list &get_row_fields(const Insert::TypedRow &row) const {
    return row.field();
  }
  void add_upsert(const bool is_relational) const;
};

}  // namespace xpl

#endif  // INSERT_STATEMENT_BUILDER_H_
