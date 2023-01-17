/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_INSERT_STATEMENT_BUILDER_H_
#define PLUGIN_X_SRC_INSERT_STATEMENT_BUILDER_H_

#include <memory>
#include <string>
#include <vector>

#include "plugin/x/src/interface/document_id_aggregator.h"
#include "plugin/x/src/statement_builder.h"

namespace xpl {

class Insert_statement_builder : public Crud_statement_builder {
 public:
  using Insert = ::Mysqlx::Crud::Insert;
  using Document_id_list = iface::Document_id_aggregator::Document_id_list;

  explicit Insert_statement_builder(
      const Expression_generator &gen,
      iface::Document_id_aggregator *id_aggregator = nullptr)
      : Crud_statement_builder(gen), m_document_id_aggregator(id_aggregator) {}
  void build(const Insert &msg) const;

 protected:
  using Projection_list = Repeated_field_list<::Mysqlx::Crud::Column>;
  using Field_list = Repeated_field_list<::Mysqlx::Expr::Expr>;
  using Row_list = Repeated_field_list<::Mysqlx::Crud::Insert_TypedRow>;
  using Placeholder = ::google::protobuf::uint32;

  void add_projection(const Projection_list &projection,
                      const bool is_relational) const;
  void add_values(const Row_list &values, const int projection_size) const;
  void add_row(const Field_list &row, const int projection_size) const;
  void add_documents(const Row_list &values) const;
  void add_document(const Field_list &row) const;
  void add_upsert(const bool is_relational) const;
  bool add_document_literal(const Mysqlx::Datatypes::Scalar &arg) const;
  bool add_document_placeholder(const Placeholder &arg) const;
  void add_document_object(const Mysqlx::Expr::Object &arg) const;

 private:
  iface::Document_id_aggregator *m_document_id_aggregator;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_INSERT_STATEMENT_BUILDER_H_
