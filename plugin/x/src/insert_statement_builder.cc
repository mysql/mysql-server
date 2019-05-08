/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "plugin/x/src/insert_statement_builder.h"

#include "plugin/x/ngs/include/ngs/protocol/protocol_protobuf.h"
#include "plugin/x/src/json_utils.h"
#include "plugin/x/src/xpl_error.h"
#include "plugin/x/src/xpl_log.h"

namespace xpl {

void Insert_statement_builder::build(const Insert &msg) const {
  m_builder.put("INSERT INTO ");
  const bool is_relational = is_table_data_model(msg);
  add_collection(msg.collection());
  add_projection(msg.projection(), is_relational);
  if (is_relational)
    add_values(msg.row(), msg.projection().size());
  else
    add_documents(msg.row());
  if (msg.upsert()) add_upsert(is_relational);
}

void Insert_statement_builder::add_projection(const Projection_list &projection,
                                              const bool is_relational) const {
  if (is_relational) {
    if (projection.size() != 0)
      m_builder.put(" (")
          .put_list(projection, std::bind(&Generator::put_identifier, m_builder,
                                          std::bind(&Mysqlx::Crud::Column::name,
                                                    std::placeholders::_1)))
          .put(")");
  } else {
    if (projection.size() != 0)
      throw ngs::Error_code(ER_X_BAD_PROJECTION,
                            "Invalid projection for document operation");
    m_builder.put(" (doc)");
  }
}

void Insert_statement_builder::add_values(const Row_list &values,
                                          const int projection_size) const {
  if (values.size() == 0)
    throw ngs::Error_code(ER_X_MISSING_ARGUMENT, "Missing row data for Insert");

  m_builder.put(" VALUES ")
      .put_list(values, [&](const Row_list::value_type &row) {
        this->add_row(row.field(), projection_size);
      });
}

void Insert_statement_builder::add_row(const Field_list &row,
                                       const int projection_size) const {
  if ((row.size() == 0) || (projection_size && row.size() != projection_size))
    throw ngs::Error_code(ER_X_BAD_INSERT_DATA,
                          "Wrong number of fields in row being inserted");

  m_builder.put("(").put_list(row, &Generator::put_expr).put(")");
}

void Insert_statement_builder::add_documents(const Row_list &values) const {
  if (values.size() == 0)
    throw ngs::Error_code(ER_X_MISSING_ARGUMENT, "Missing row data for Insert");

  m_builder.put(" VALUES ")
      .put_list(values, [this](const Row_list::value_type &row) {
        this->add_document(row.field());
      });
}

void Insert_statement_builder::add_document(const Field_list &row) const {
  if (row.size() != 1)
    throw ngs::Error_code(ER_X_BAD_INSERT_DATA,
                          "Wrong number of fields in row being inserted");
  const Mysqlx::Expr::Expr &doc = row.Get(0);
  if (is_prep_stmt_mode()) {
    m_builder
        .put(
            "((SELECT JSON_INSERT(`_DERIVED_TABLE_`.`value`,'$._id',"
            "CONVERT(MYSQLX_GENERATE_DOCUMENT_ID(@@AUTO_INCREMENT_OFFSET,"
            "@@AUTO_INCREMENT_INCREMENT,"
            "JSON_CONTAINS_PATH(`_DERIVED_TABLE_`.`value`,'one','$._id')) "
            "USING utf8mb4)) FROM (SELECT ")
        .put_expr(doc)
        .put(" AS `value`) AS `_DERIVED_TABLE_`))");
    return;
  }
  switch (doc.type()) {
    case Mysqlx::Expr::Expr::LITERAL:
      if (add_document_literal(doc.literal())) return;
      break;

    case Mysqlx::Expr::Expr::PLACEHOLDER:
      if (add_document_placeholder(doc.position())) return;
      break;

    case Mysqlx::Expr::Expr::OBJECT:
      add_document_object(doc.object());
      return;

    default: {}
  }
  m_builder.put("(").put_expr(doc).put(")");
}

void Insert_statement_builder::add_upsert(const bool is_relational) const {
  if (is_relational)
    throw ngs::Error_code(
        ER_X_BAD_INSERT_DATA,
        "Unable update on duplicate key for TABLE data model");
  m_builder.put(
      " ON DUPLICATE KEY UPDATE"
      " doc = IF(JSON_UNQUOTE(JSON_EXTRACT(doc, '$._id'))"
      " = JSON_UNQUOTE(JSON_EXTRACT(VALUES(doc), '$._id')),"
      " VALUES(doc), MYSQLX_ERROR(" STRINGIFY_ARG(ER_X_BAD_UPSERT_DATA) "))");
}

bool Insert_statement_builder::add_document_literal(
    const Mysqlx::Datatypes::Scalar &arg) const {
  switch (arg.type()) {
    case Mysqlx::Datatypes::Scalar::V_OCTETS:
      if (arg.v_octets().content_type() != Expression_generator::CT_PLAIN &&
          arg.v_octets().content_type() != Expression_generator::CT_JSON)
        return false;
      if (is_id_in_json(arg.v_octets().value()))
        m_builder.put("(").put_quote(arg.v_octets().value()).put(")");
      else
        m_builder.put("(JSON_SET(")
            .put_quote(arg.v_octets().value())
            .put(", '$._id', ")
            .put_quote(m_document_id_aggregator->generate_id())
            .put("))");
      return true;

    case Mysqlx::Datatypes::Scalar::V_STRING:
      if (is_id_in_json(arg.v_string().value()))
        m_builder.put("(").put_expr(arg).put(")");
      else
        m_builder.put("(JSON_SET(")
            .put_quote(arg.v_string().value())
            .put(", '$._id', ")
            .put_quote(m_document_id_aggregator->generate_id())
            .put("))");
      return true;

    default: {}
  }
  return false;
}

bool Insert_statement_builder::add_document_placeholder(
    const Placeholder &arg) const {
  if (arg < static_cast<Placeholder>(m_builder.args().size()))
    return add_document_literal(m_builder.args().Get(arg));
  return false;
}

namespace {
bool is_id_in_object(const Mysqlx::Expr::Object &arg) {
  for (const auto &field : arg.fld())
    if (field.key() == "_id") return true;
  return false;
}
}  // namespace

void Insert_statement_builder::add_document_object(
    const Mysqlx::Expr::Object &arg) const {
  if (is_id_in_object(arg))
    m_builder.put("(").put_expr(arg).put(")");
  else
    m_builder.put("(JSON_SET(")
        .put_expr(arg)
        .put(", '$._id', ")
        .put_quote(m_document_id_aggregator->generate_id())
        .put("))");
}

}  // namespace xpl
