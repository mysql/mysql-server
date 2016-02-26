/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
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

#include "statement_builder.h"

namespace xpl
{

class Insert_statement_builder: public Statement_builder
{
public:
  typedef ::Mysqlx::Crud::Insert Insert;

  Insert_statement_builder(const Insert &msg, Query_string_builder &qb);

protected:
  typedef ::google::protobuf::RepeatedPtrField< ::Mysqlx::Crud::Column > Projection_list;
  typedef ::google::protobuf::RepeatedPtrField< ::Mysqlx::Expr::Expr > Field_list;
  typedef ::google::protobuf::RepeatedPtrField< ::Mysqlx::Crud::Insert_TypedRow > Row_list;

  virtual void add_statement() const;
  void add_projection(const Projection_list &projection) const;
  void add_values(const Row_list &values) const;
  void add_row(const Field_list &row, int size) const;

  const Insert &m_msg;
};

} // namespace xpl

#endif // INSERT_STATEMENT_BUILDER_H_
