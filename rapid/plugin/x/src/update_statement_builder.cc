/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
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

#include "update_statement_builder.h"
#include "ngs_common/protocol_protobuf.h"
#include "xpl_error.h"


using ::Mysqlx::Crud::UpdateOperation;


void xpl::Update_statement_builder::build(const Update &msg) const
{
  m_builder.put("UPDATE ");
  add_collection(msg.collection());
  add_operation(msg.operation(), is_table_data_model(msg));
  add_filter(msg.criteria());
  add_order(msg.order());
  add_limit(msg.limit(), true);
}


void xpl::Update_statement_builder::add_operation(const Operation_list &operation,
                                                  const bool is_relational) const
{
  if (operation.size() == 0)
    throw ngs::Error_code(ER_X_BAD_UPDATE_DATA, "Invalid update expression list");

  m_builder.put(" SET ");
  if (is_relational)
    add_table_operation(operation);
  else
    add_document_operation(operation);
}


void xpl::Update_statement_builder::add_document_operation_item(const Operation_item &item, int &opeartion_id) const
{
  if (opeartion_id != item.operation())
    m_builder.put(")");
  opeartion_id = item.operation();

  if (item.source().has_schema_name() ||
      item.source().has_table_name() ||
      item.source().has_name())
     throw ngs::Error_code(ER_X_BAD_COLUMN_TO_UPDATE, "Invalid column name to update");

  if (item.operation() != UpdateOperation::ITEM_MERGE)
  {
    if (item.source().document_path_size() == 0 ||
        (item.source().document_path(0).type() != ::Mysqlx::Expr::DocumentPathItem::MEMBER &&
         item.source().document_path(0).type() != ::Mysqlx::Expr::DocumentPathItem::MEMBER_ASTERISK))
      throw ngs::Error_code(ER_X_BAD_MEMBER_TO_UPDATE, "Invalid document member location");

    if (item.source().document_path_size() == 1 &&
        item.source().document_path(0).type() == ::Mysqlx::Expr::DocumentPathItem::MEMBER)
    {
      if (item.source().document_path(0).value() == "_id")
        throw ngs::Error(ER_X_BAD_MEMBER_TO_UPDATE, "Forbidden update operation on '$._id' member");
    }
    m_builder.put(",").put_expr(item.source().document_path());
  }

  switch (item.operation())
  {
  case UpdateOperation::ITEM_REMOVE:
    if (item.has_value())
      throw ngs::Error(ER_X_BAD_UPDATE_DATA, "Unexpected value argument for ITEM_REMOVE operation");
    break;

  case UpdateOperation::ITEM_MERGE:
  {
    Query_string_builder value;
    m_builder.m_gen.clone(value).feed(item.value());
    m_builder.put(",IF(JSON_TYPE(").put(value)
       .put(")='OBJECT',JSON_REMOVE(").put(value)
       .put(",'$._id'),'_ERROR_')");
    break;
  }

  default:
    m_builder.put(",").put_expr(item.value());
  }
}


void xpl::Update_statement_builder::add_document_operation(const Operation_list &operation) const
{
  int prev = -1;
  m_builder.put("doc=");

  for (Operation_list::const_reverse_iterator o = operation.rbegin();
       o != operation.rend(); ++o)
  {
    if (prev == o->operation())
      continue;

    switch (o->operation())
    {
    case UpdateOperation::ITEM_REMOVE:
      m_builder.put("JSON_REMOVE(");
      break;

    case UpdateOperation::ITEM_SET:
      m_builder.put("JSON_SET(");
      break;

    case UpdateOperation::ITEM_REPLACE:
      m_builder.put("JSON_REPLACE(");
      break;

    case UpdateOperation::ITEM_MERGE:
      m_builder.put("JSON_MERGE(");
      break;

    case UpdateOperation::ARRAY_INSERT:
      m_builder.put("JSON_ARRAY_INSERT(");
      break;

    case UpdateOperation::ARRAY_APPEND:
      m_builder.put("JSON_ARRAY_APPEND(");
      break;

    default:
      throw ngs::Error_code(ER_X_BAD_TYPE_OF_UPDATE, "Invalid type of update operation for document");
    }
    prev = o->operation();
  }
  m_builder.put("doc")
     .put_each(operation.begin(), operation.end(),
               ngs::bind(&Update_statement_builder::add_document_operation_item,
                         this, ngs::placeholders::_1,
                         static_cast<int>(operation.begin()->operation())))
     .put(")");
}

namespace
{
typedef ::Mysqlx::Crud::UpdateOperation Operation_item;

struct Is_not_equal
{
  Is_not_equal(const Operation_item &item)
  : m_item(item)
  {}

  bool operator() (const Operation_item &item) const
  {
    return
        item.source().name() != m_item.source().name() ||
        item.operation() != m_item.operation();
  }

  const Operation_item &m_item;
};

} // namespace


void xpl::Update_statement_builder::add_table_operation(const Operation_list &operation) const
{
  Operation_iterator
    b = operation.begin(),
    e = std::find_if(b, operation.end(), Is_not_equal(*b));
  add_table_operation_items(b, e);
  while (e != operation.end())
  {
    b = e;
    e = std::find_if(b, operation.end(), Is_not_equal(*b));
    m_builder.put(",");
    add_table_operation_items(b, e);
  }
}


void xpl::Update_statement_builder::add_table_operation_items(Operation_iterator begin,
                                                              Operation_iterator end) const
{
  if (begin->source().has_schema_name() ||
      begin->source().has_table_name() ||
      begin->source().name().empty())
    throw ngs::Error_code(ER_X_BAD_COLUMN_TO_UPDATE, "Invalid column name to update");

  switch (begin->operation())
  {
  case UpdateOperation::SET:
    if (begin->source().document_path_size() != 0)
      throw ngs::Error_code(ER_X_BAD_COLUMN_TO_UPDATE, "Invalid column name to update");
    m_builder.put_list(begin, end,
                       ngs::bind(&Update_statement_builder::add_field_with_value, this, ngs::placeholders::_1));
    break;

  case UpdateOperation::ITEM_REMOVE:
    m_builder.put_identifier(begin->source().name())
      .put("=JSON_REMOVE(")
      .put_identifier(begin->source().name())
      .put_each(begin, end, ngs::bind(&Update_statement_builder::add_member, this, ngs::placeholders::_1))
      .put(")");
    break;

  case UpdateOperation::ITEM_SET:
    m_builder.put_identifier(begin->source().name())
      .put("=JSON_SET(")
      .put_identifier(begin->source().name())
      .put_each(begin, end, ngs::bind(&Update_statement_builder::add_member_with_value, this, ngs::placeholders::_1))
      .put(")");
    break;

  case UpdateOperation::ITEM_REPLACE:
    m_builder.put_identifier(begin->source().name())
      .put("=JSON_REPLACE(")
      .put_identifier(begin->source().name())
      .put_each(begin, end, ngs::bind(&Update_statement_builder::add_member_with_value, this, ngs::placeholders::_1))
      .put(")");
    break;

  case UpdateOperation::ITEM_MERGE:
    m_builder.put_identifier(begin->source().name())
      .put("=JSON_MERGE(")
      .put_identifier(begin->source().name())
      .put_each(begin, end, ngs::bind(&Update_statement_builder::add_value, this, ngs::placeholders::_1))
      .put(")");
    break;

  case UpdateOperation::ARRAY_INSERT:
    m_builder.put_identifier(begin->source().name())
      .put("=JSON_ARRAY_INSERT(")
      .put_identifier(begin->source().name())
      .put_each(begin, end, ngs::bind(&Update_statement_builder::add_member_with_value, this, ngs::placeholders::_1))
      .put(")");
    break;

  case UpdateOperation::ARRAY_APPEND:
    m_builder.put_identifier(begin->source().name())
      .put("=JSON_ARRAY_APPEND(")
      .put_identifier(begin->source().name())
      .put_each(begin, end, ngs::bind(&Update_statement_builder::add_member_with_value, this, ngs::placeholders::_1))
      .put(")");
    break;

  default:
    throw ngs::Error_code(ER_X_BAD_TYPE_OF_UPDATE,
                          "Invalid type of update operation for table");
  }
}


void xpl::Update_statement_builder::add_member(const Operation_item &item) const
{
  if (item.source().document_path_size() == 0)
    throw ngs::Error_code(ER_X_BAD_MEMBER_TO_UPDATE, "Invalid member location");
  m_builder.put(",").put_expr(item.source().document_path());
}


void xpl::Update_statement_builder::add_value(const Operation_item &item) const
{
  m_builder.put(",").put_expr(item.value());
}


void xpl::Update_statement_builder::add_member_with_value(const Operation_item &item) const
{
  add_member(item);
  add_value(item);
}


void xpl::Update_statement_builder::add_field_with_value(const Operation_item &item) const
{
  m_builder.put_expr(item.source()).put("=").put_expr(item.value());
}
