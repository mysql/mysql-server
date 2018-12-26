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

#ifndef FIND_STATEMENT_BUILDER_H_
#define FIND_STATEMENT_BUILDER_H_

#include "statement_builder.h"

namespace xpl
{

class Find_statement_builder: public Crud_statement_builder
{
public:
  typedef ::Mysqlx::Crud::Find Find;

  explicit Find_statement_builder(const Expression_generator &gen)
      : Crud_statement_builder(gen) {}

  void build(const Find &msg) const;

protected:
  typedef ::Mysqlx::Crud::Projection Projection;
  typedef ::google::protobuf::RepeatedPtrField< Projection > Projection_list;
  typedef ::google::protobuf::RepeatedPtrField< ::Mysqlx::Expr::Expr > Grouping_list;
  typedef ::Mysqlx::Expr::Expr Grouping_criteria;
  typedef void (Find_statement_builder::*Object_item_adder)(const Projection &item) const;

  void add_statement_common(const Find &msg) const;
  void add_document_statement_with_grouping(const Find &msg) const;

  void add_grouping(const Grouping_list &group) const;
  void add_grouping_criteria(const Grouping_criteria &criteria) const;
  void add_table_projection(const Projection_list &projection) const;
  void add_table_projection_item(const Projection &item) const;
  void add_document_projection(const Projection_list &projection) const;
  void add_document_projection_item(const Projection &item) const;

  void add_document_object(const Projection_list &projection, const Object_item_adder &adder) const;
  void add_document_primary_projection_item(const Projection &item) const;
};

} // namespace xpl

#endif // FIND_STATEMENT_BUILDER_H_
