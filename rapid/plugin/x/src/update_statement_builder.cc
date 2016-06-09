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


xpl::Update_statement_builder::Update_statement_builder(const Update &msg, Query_string_builder &qb)
: Statement_builder(qb, msg.args(), msg.collection().schema(), msg.data_model() == Mysqlx::Crud::TABLE),
  m_msg(msg)
{}


void xpl::Update_statement_builder::add_statement() const
{
  m_builder.put("UPDATE ");
  add_table(m_msg.collection());
  add_operation(m_msg.operation());
  add_filter(m_msg.criteria());
  add_order(m_msg.order());
  add_limit(m_msg.limit(), true);
}


void xpl::Update_statement_builder::add_operation(const Operation_list &operation) const
{
  if (operation.size() == 0)
    throw ngs::Error_code(ER_X_BAD_UPDATE_DATA, "Invalid update expression list");

  m_builder.put(" SET ");
  if (m_is_relational)
    add_table_operation(operation);
  else
    add_document_operation(operation, "doc");
}


void xpl::Update_statement_builder::add_document_operation_item(const Operation_item &item, Builder &bld,
                                                                bool &is_id_synch, int &opeartion_id) const
{
  if (opeartion_id != item.operation())
    bld.put(")");
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

      if (item.source().document_path(0).value().empty())
        is_id_synch = false;
    }
    bld.put(",").gen(item.source().document_path());
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
    Builder(value, m_builder.get_generator()).gen(item.value());
    bld.put(",IF(JSON_TYPE(").put(value).
        put(")='OBJECT',JSON_REMOVE(").put(value).
        put(",'$._id'),'_ERROR_')");
    break;
  }

  default:
    bld.put(",").gen(item.value());
  }
}


void xpl::Update_statement_builder::add_document_operation(const Operation_list &operation,
                                                           const std::string &doc_column) const
{
  Query_string_builder buff;
  Builder bld(buff, m_builder.get_generator());

  int prev = -1;

  for (Operation_list::const_reverse_iterator o = operation.rbegin();
       o != operation.rend(); ++o)
  {
    if (prev == o->operation())
      continue;

    switch (o->operation())
    {
    case UpdateOperation::ITEM_REMOVE:
      bld.put("JSON_REMOVE(");
      break;

    case UpdateOperation::ITEM_SET:
      bld.put("JSON_SET(");
      break;

    case UpdateOperation::ITEM_REPLACE:
      bld.put("JSON_REPLACE(");
      break;

    case UpdateOperation::ITEM_MERGE:
      bld.put("JSON_MERGE(");
      break;

    case UpdateOperation::ARRAY_INSERT:
      bld.put("JSON_ARRAY_INSERT(");
      break;

    case UpdateOperation::ARRAY_APPEND:
      bld.put("JSON_ARRAY_APPEND(");
      break;

    default:
      throw ngs::Error_code(ER_X_BAD_TYPE_OF_UPDATE, "Invalid type of update operation for document");
    }
    prev = o->operation();
  }
  bool is_id_synch = true;
  bld.put(doc_column).
      put_each(operation.begin(), operation.end(),
               boost::bind(&Update_statement_builder::add_document_operation_item,
                           this, _1, bld, boost::ref(is_id_synch),
                           static_cast<int>(operation.begin()->operation()))).put(")");

  if (is_id_synch)
    m_builder.put("doc=").put(buff);
  else
    m_builder.put("doc=JSON_SET(").put(buff).put(",'$._id',_id)");
}


namespace
{
typedef ::Mysqlx::Crud::UpdateOperation Operation_item;
typedef xpl::Statement_builder::Builder Builder;

struct Add_member
{
  explicit Add_member(const Builder &bld)
  : m_qb(bld)
  {}

  void operator() (const Operation_item &item) const
  {
    if (item.source().document_path_size() == 0)
      throw ngs::Error_code(ER_X_BAD_MEMBER_TO_UPDATE, "Invalid member location");
    m_qb.put(",").gen(item.source().document_path());
  }

  const Builder &m_qb;
};


struct Add_value
{
  explicit Add_value(const Builder &bld)
  : m_bld(bld)
  {}

  void operator() (const Operation_item &item) const
  {
    m_bld.put(",").gen(item.value());
  }

  const Builder &m_bld;
};


struct Add_member_with_value: Add_member, Add_value
{
  explicit Add_member_with_value(const Builder &bld)
  : Add_member(bld), Add_value(bld)
  {}

  void operator() (const Operation_item &item) const
  {
    Add_member::operator()(item);
    Add_value::operator()(item);
  }
};


struct Add_field_with_value
{
  explicit Add_field_with_value(const Builder &bld)
  : m_bld(bld)
  {}

  void operator() (const Operation_item &item) const
  {
    m_bld.gen(item.source()).put("=").gen(item.value());
  }

  const Builder &m_bld;
};


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
    m_builder.put_each(begin, end, Add_field_with_value(m_builder));
    break;

  case UpdateOperation::ITEM_REMOVE:
    m_builder.put_identifier(begin->source().name()).put("=JSON_REMOVE(").
          put_identifier(begin->source().name()).
          put_each(begin, end, Add_member(m_builder)).put(")");
    break;

  case UpdateOperation::ITEM_SET:
    m_builder.put_identifier(begin->source().name()).put("=JSON_SET(").
          put_identifier(begin->source().name()).
          put_each(begin, end, Add_member_with_value(m_builder)).put(")");
    break;

  case UpdateOperation::ITEM_REPLACE:
    m_builder.put_identifier(begin->source().name()).put("=JSON_REPLACE(").
          put_identifier(begin->source().name()).
          put_each(begin, end, Add_member_with_value(m_builder)).put(")");
    break;

  case UpdateOperation::ITEM_MERGE:
    m_builder.put_identifier(begin->source().name()).put("=JSON_MERGE(").
          put_identifier(begin->source().name()).
          put_each(begin, end, Add_value(m_builder)).put(")");
    break;

  case UpdateOperation::ARRAY_INSERT:
    m_builder.put_identifier(begin->source().name()).put("=JSON_ARRAY_INSERT(").
          put_identifier(begin->source().name()).
          put_each(begin, end, Add_member_with_value(m_builder)).put(")");
    break;

  case UpdateOperation::ARRAY_APPEND:
    m_builder.put_identifier(begin->source().name()).put("=JSON_ARRAY_APPEND(").
          put_identifier(begin->source().name()).
          put_each(begin, end, Add_member_with_value(m_builder)).put(")");
    break;

  default:
    throw ngs::Error_code(ER_X_BAD_TYPE_OF_UPDATE,
                          "Invalid type of update operation for table");
  }
}
