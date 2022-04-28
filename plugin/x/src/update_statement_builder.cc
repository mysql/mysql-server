/*
 * Copyright (c) 2015, 2022, Oracle and/or its affiliates.
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

#include "plugin/x/src/update_statement_builder.h"

#include <string>

#include "plugin/x/src/xpl_error.h"

namespace xpl {

void Update_statement_builder::build(const Update &msg) const {
  m_builder.put("UPDATE ");
  add_collection(msg.collection());
  add_operation(msg.operation(), is_table_data_model(msg));
  add_filter(msg.criteria());
  add_order(msg.order());
  add_limit(msg, true);
}

void Update_statement_builder::add_operation(const Operation_list &operation,
                                             const bool is_relational) const {
  if (operation.size() == 0)
    throw ngs::Error_code(ER_X_BAD_UPDATE_DATA,
                          "Invalid update expression list");

  m_builder.put(" SET ");
  if (is_relational)
    add_table_operation(operation);
  else
    add_document_operation(operation);
}

namespace {
inline bool has_short_path(const Expression_generator::Document_path &path,
                           const std::string &value) {
  return path.size() == 1 &&
         path.Get(0).type() ==
             Expression_generator::Document_path::value_type::MEMBER &&
         path.Get(0).value() == value;
}
}  // namespace

void Update_statement_builder::add_document_operation_item(
    const Update_operation &item,
    Update_operation::UpdateType *operation_id) const {
  if (*operation_id != item.operation()) m_builder.put(")");
  *operation_id = item.operation();

  if (item.source().has_schema_name() || item.source().has_table_name() ||
      item.source().has_name())
    throw ngs::Error_code(ER_X_BAD_COLUMN_TO_UPDATE,
                          "Invalid column name to update");

  if (item.operation() != Update_operation::ITEM_MERGE &&
      item.operation() != Update_operation::MERGE_PATCH) {
    if (item.source().document_path_size() > 0 &&
        (item.source().document_path(0).type() !=
             ::Mysqlx::Expr::DocumentPathItem::MEMBER &&
         item.source().document_path(0).type() !=
             ::Mysqlx::Expr::DocumentPathItem::MEMBER_ASTERISK))
      throw ngs::Error_code(ER_X_BAD_MEMBER_TO_UPDATE,
                            "Invalid document member location");

    if (has_short_path(item.source().document_path(), "_id"))
      throw ngs::Error(ER_X_BAD_MEMBER_TO_UPDATE,
                       "Forbidden update operation on '$._id' member");

    if (item.source().document_path_size() > 0)
      m_builder.put(",").put_expr(item.source().document_path());
    else
      m_builder.put(",").put_quote("$");
  }

  switch (item.operation()) {
    case Update_operation::ITEM_REMOVE:
      if (item.has_value())
        throw ngs::Error(ER_X_BAD_UPDATE_DATA,
                         "Unexpected value argument for ITEM_REMOVE operation");
      break;

    case Update_operation::MERGE_PATCH:
    case Update_operation::ITEM_MERGE: {
      if (item.source().document_path_size() == 0 ||
          has_short_path(item.source().document_path(), ""))
        m_builder.put(",").put_expr(item.value());
      else
        // JSON_MERGE works on documents only;
        // use of ITEM_MERGE once is equal expression like
        // doc = JSON_REPLACE(doc, $->source,
        //                    JSON_MERGE(JSON_EXTRACT(doc, $->source), value)
        // use of ITEM_MERGE more than one times or mixing with other ITEM_*
        // make the expression really complicated
        throw ngs::Error(ER_X_BAD_UPDATE_DATA,
                         "Unexpected source for ITEM_MERGE operation");
      break;
    }

    default:
      m_builder.put(",").put_expr(item.value());
  }
}

void Update_statement_builder::add_document_operation(
    const Operation_list &operation) const {
  int prev = -1;
  m_builder.put("doc=JSON_SET(");

  for (Operation_list::const_reverse_iterator o = operation.rbegin();
       o != operation.rend(); ++o) {
    if (prev == o->operation()) continue;

    switch (o->operation()) {
      case Update_operation::ITEM_REMOVE:
        m_builder.put("JSON_REMOVE(");
        break;

      case Update_operation::ITEM_SET:
        m_builder.put("JSON_SET(");
        break;

      case Update_operation::ITEM_REPLACE:
        m_builder.put("JSON_REPLACE(");
        break;

      case Update_operation::ITEM_MERGE:
        m_builder.put("JSON_MERGE_PRESERVE(");
        break;

      case Update_operation::ARRAY_INSERT:
        m_builder.put("JSON_ARRAY_INSERT(");
        break;

      case Update_operation::ARRAY_APPEND:
        m_builder.put("JSON_ARRAY_APPEND(");
        break;

      case Update_operation::MERGE_PATCH:
        m_builder.put("JSON_MERGE_PATCH(");
        break;

      default:
        throw ngs::Error_code(ER_X_BAD_TYPE_OF_UPDATE,
                              "Invalid type of update operation for document");
    }
    prev = o->operation();
  }
  Update_operation::UpdateType operation_id = operation.begin()->operation();
  m_builder.put("doc")
      .put_each(
          operation,
          std::bind(&Update_statement_builder::add_document_operation_item,
                    this, std::placeholders::_1, &operation_id))
      .put("),'$._id',JSON_EXTRACT(`doc`,'$._id'))");
}

void Update_statement_builder::add_table_operation(
    const Operation_list &operation) const {
  Operation_iterator b = operation.begin();
  auto is_not_equal = [&b](const Update_operation &item) {
    return item.source().name() != b->source().name() ||
           item.operation() != b->operation();
  };
  Operation_iterator e = std::find_if(b, operation.end(), is_not_equal);
  add_table_operation_items(b, e);
  while (e != operation.end()) {
    b = e;
    e = std::find_if(b, operation.end(), is_not_equal);
    m_builder.put(",");
    add_table_operation_items(b, e);
  }
}

void Update_statement_builder::add_table_operation_items(
    Operation_iterator begin, Operation_iterator end) const {
  if (begin->source().has_schema_name() || begin->source().has_table_name() ||
      begin->source().name().empty())
    throw ngs::Error_code(ER_X_BAD_COLUMN_TO_UPDATE,
                          "Invalid column name to update");

  switch (begin->operation()) {
    case Update_operation::SET:
      if (begin->source().document_path_size() != 0)
        throw ngs::Error_code(ER_X_BAD_COLUMN_TO_UPDATE,
                              "Invalid column name to update");
      m_builder.put_list(
          begin, end,
          std::bind(&Update_statement_builder::add_field_with_value, this,
                    std::placeholders::_1));
      break;

    case Update_operation::ITEM_REMOVE:
      m_builder.put_identifier(begin->source().name())
          .put("=JSON_REMOVE(")
          .put_identifier(begin->source().name())
          .put_each(begin, end,
                    std::bind(&Update_statement_builder::add_member, this,
                              std::placeholders::_1))
          .put(")");
      break;

    case Update_operation::ITEM_SET:
      m_builder.put_identifier(begin->source().name())
          .put("=JSON_SET(")
          .put_identifier(begin->source().name())
          .put_each(begin, end,
                    std::bind(&Update_statement_builder::add_member_with_value,
                              this, std::placeholders::_1))
          .put(")");
      break;

    case Update_operation::ITEM_REPLACE:
      m_builder.put_identifier(begin->source().name())
          .put("=JSON_REPLACE(")
          .put_identifier(begin->source().name())
          .put_each(begin, end,
                    std::bind(&Update_statement_builder::add_member_with_value,
                              this, std::placeholders::_1))
          .put(")");
      break;

    case Update_operation::ITEM_MERGE:
      m_builder.put_identifier(begin->source().name())
          .put("=JSON_MERGE_PRESERVE(")
          .put_identifier(begin->source().name())
          .put_each(begin, end,
                    std::bind(&Update_statement_builder::add_value, this,
                              std::placeholders::_1))
          .put(")");
      break;

    case Update_operation::ARRAY_INSERT:
      m_builder.put_identifier(begin->source().name())
          .put("=JSON_ARRAY_INSERT(")
          .put_identifier(begin->source().name())
          .put_each(begin, end,
                    std::bind(&Update_statement_builder::add_member_with_value,
                              this, std::placeholders::_1))
          .put(")");
      break;

    case Update_operation::ARRAY_APPEND:
      m_builder.put_identifier(begin->source().name())
          .put("=JSON_ARRAY_APPEND(")
          .put_identifier(begin->source().name())
          .put_each(begin, end,
                    std::bind(&Update_statement_builder::add_member_with_value,
                              this, std::placeholders::_1))
          .put(")");
      break;

    case Update_operation::MERGE_PATCH:
      m_builder.put_identifier(begin->source().name())
          .put("=JSON_MERGE_PATCH(")
          .put_identifier(begin->source().name())
          .put_each(begin, end,
                    std::bind(&Update_statement_builder::add_value, this,
                              std::placeholders::_1))
          .put(")");
      break;

    default:
      throw ngs::Error_code(ER_X_BAD_TYPE_OF_UPDATE,
                            "Invalid type of update operation for table");
  }
}

void Update_statement_builder::add_member(const Update_operation &item) const {
  if (item.source().document_path_size() == 0)
    throw ngs::Error_code(ER_X_BAD_MEMBER_TO_UPDATE, "Invalid member location");
  m_builder.put(",").put_expr(item.source().document_path());
}

void Update_statement_builder::add_value(const Update_operation &item) const {
  m_builder.put(",").put_expr(item.value());
}

void Update_statement_builder::add_member_with_value(
    const Update_operation &item) const {
  add_member(item);
  add_value(item);
}

void Update_statement_builder::add_field_with_value(
    const Update_operation &item) const {
  m_builder.put_expr(item.source()).put("=").put_expr(item.value());
}

}  // namespace xpl
