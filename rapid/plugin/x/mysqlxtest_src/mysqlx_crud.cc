/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "mysqlx_crud.h"
#include "mysqlx_connection.h"
#include "ngs_common/protocol_protobuf.h"

#include "mysqlx_parser.h"

#include "compilerutils.h"
#include <boost/algorithm/string.hpp>

#define CONTENT_TYPE_GEOMETRY 0x0001
#define CONTENT_TYPE_JSON 0x0002
#define CONTENT_TYPE_XML 0x0002

using namespace mysqlx;

Schema::Schema(boost::shared_ptr<Session> conn, const std::string &name_)
: m_sess(conn), m_name(name_)
{
}

boost::shared_ptr<Table> Schema::getTable(const std::string &name_)
{
  std::map<std::string, boost::shared_ptr<Table> >::const_iterator iter = m_tables.find(name_);
  if (iter != m_tables.end())
    return iter->second;
  return m_tables[name_] = boost::shared_ptr<Table>(new Table(shared_from_this(), name_));
}

boost::shared_ptr<Collection> Schema::getCollection(const std::string &name_)
{
  std::map<std::string, boost::shared_ptr<Collection> >::const_iterator iter = m_collections.find(name_);
  if (iter != m_collections.end())
    return iter->second;
  return m_collections[name_] = boost::shared_ptr<Collection>(new Collection(shared_from_this(), name_));
}

Table::Table(boost::shared_ptr<Schema> schema_, const std::string &name_)
: m_schema(schema_), m_name(name_)
{
}

UpdateStatement Table::update()
{
  UpdateStatement tmp(shared_from_this());
  return tmp;
}

DeleteStatement Table::remove()
{
  DeleteStatement tmp(shared_from_this());
  return tmp;
}

InsertStatement Table::insert()
{
  InsertStatement tmp(shared_from_this());
  return tmp;
}

SelectStatement Table::select(const std::vector<std::string> &fieldList)
{
  SelectStatement tmp(shared_from_this(), fieldList);
  return tmp;
}

Collection::Collection(boost::shared_ptr<Schema> schema_, const std::string &name_)
: m_schema(schema_), m_name(name_)
{
}

FindStatement Collection::find(const std::string &searchCondition)
{
  FindStatement tmp(shared_from_this(), searchCondition);
  return tmp;
}

ModifyStatement Collection::modify(const std::string &searchCondition)
{
  ModifyStatement tmp(shared_from_this(), searchCondition);
  return tmp;
}

AddStatement Collection::add(const Document &doc)
{
  AddStatement tmp(shared_from_this(), doc);
  return tmp;
}

RemoveStatement Collection::remove(const std::string &searchCondition)
{
  RemoveStatement tmp(shared_from_this(), searchCondition);
  return tmp;
}

Statement::Statement(const Statement& other) :
m_placeholders(other.m_placeholders), m_bound_values(other.m_bound_values)
{
}

Statement::~Statement()
{
}

void Statement::init_bound_values()
{
  // Initializes the bound values array on the first call to bind
  if (!m_bound_values.size())
  {
    for (size_t index = 0; index < m_placeholders.size(); index++)
      m_bound_values.push_back(NULL);
  }
}

void Statement::validate_bind_placeholder(const std::string& name)
{
  // Now sets the right value on the position of the indicated placeholder
  std::vector<std::string>::iterator index = std::find(m_placeholders.begin(), m_placeholders.end(), name);
  if (index == m_placeholders.end())
    throw std::logic_error("Unable to bind value for unexisting placeholder: " + name);
}

void Statement::insert_bound_values(::google::protobuf::RepeatedPtrField< ::Mysqlx::Datatypes::Scalar >* target)
{
  // First validates that all the placeholders have a bound value
  std::string str_undefined;
  if (m_placeholders.size() && !m_bound_values.size())
    str_undefined = boost::algorithm::join(m_placeholders, ", ");
  else
  {
    std::vector<std::string> undefined;
    for (size_t index = 0; index < m_bound_values.size(); index++)
    {
      if (!m_bound_values[index])
        undefined.push_back(m_placeholders[index]);
    }

    str_undefined = boost::algorithm::join(undefined, ", ");
  }

  // Throws the error if needed
  if (!str_undefined.empty())
    throw std::logic_error("Missing value bindings for the next placeholders: " + str_undefined);

  // No errors, proceeds to set the values if any
  target->Clear();
  std::vector<Mysqlx::Datatypes::Scalar*>::const_iterator index, end = m_bound_values.end();
  for (index = m_bound_values.begin(); index != end; index++)
    target->AddAllocated(*index);
}

Collection_Statement::Collection_Statement(boost::shared_ptr<Collection> coll)
  : m_coll(coll)
{
}

Collection_Statement::Collection_Statement(const Collection_Statement& other)
: Statement(other), m_coll(other.m_coll)
{
}

Collection_Statement &Collection_Statement::bind(const std::string &name, const DocumentValue &value)
{
  init_bound_values();

  validate_bind_placeholder(name);

  // Now sets the right value on the position of the indicated placeholder
  std::vector<std::string>::iterator index = std::find(m_placeholders.begin(), m_placeholders.end(), name);
  m_bound_values[index - m_placeholders.begin()] = convert_document_value(value);

  return *this;
}

Mysqlx::Datatypes::Scalar* Collection_Statement::convert_document_value(const DocumentValue& value)
{
  Mysqlx::Datatypes::Scalar *my_scalar = new Mysqlx::Datatypes::Scalar;

  mysqlx::DocumentValue column_value(value);

  switch (value.type())
  {
    case DocumentValue::TInteger:
      my_scalar->set_type(Mysqlx::Datatypes::Scalar::V_SINT);
      my_scalar->set_v_signed_int(column_value);
      break;
    case DocumentValue::TFloat:
      my_scalar->set_type(Mysqlx::Datatypes::Scalar::V_DOUBLE);
      my_scalar->set_v_double(column_value);
      break;
    case DocumentValue::TString:
      my_scalar->set_type(Mysqlx::Datatypes::Scalar::V_STRING);
      my_scalar->mutable_v_string()->set_value(column_value);
      break;
    case DocumentValue::TDocument:
    case DocumentValue::TArray:
      my_scalar->set_type(Mysqlx::Datatypes::Scalar::V_OCTETS);
      my_scalar->mutable_v_octets()->set_content_type(CONTENT_TYPE_JSON);
      my_scalar->mutable_v_octets()->set_value(column_value);
      break;
    case DocumentValue::TExpression:
    case DocumentValue::TNull:
      throw std::logic_error("Only scalar values supported on this conversion");
      break;
  }

  return my_scalar;
}

// -----

Find_Base::Find_Base(boost::shared_ptr<Collection> coll)
: Collection_Statement(coll), m_find(new Mysqlx::Crud::Find())
{
}

Find_Base::Find_Base(const Find_Base &other)
: Collection_Statement(other), m_find(other.m_find)
{
}

Find_Base &Find_Base::operator = (const Find_Base &other)
{
  m_find = other.m_find;
  return *this;
}

boost::shared_ptr<Result> Find_Base::execute()
{
  insert_bound_values(m_find->mutable_args());

  if (!m_find->IsInitialized())
    throw std::logic_error("FindStatement is not completely initialized: " + m_find->InitializationErrorString());

  SessionRef session(m_coll->schema()->session());

  boost::shared_ptr<Result> result(session->connection()->execute_find(*m_find));

  // wait for results (at least metadata) to arrive
  result->wait();

  return result;
}

Find_Base &Find_Skip::skip(uint64_t skip_)
{
  m_find->mutable_limit()->set_offset(skip_);
  return *this;
}

Find_Skip &Find_Limit::limit(uint64_t limit_)
{
  m_find->mutable_limit()->set_row_count(limit_);
  return *this;
}

Find_Limit &Find_Sort::sort(const std::vector<std::string> &sortFields)
{
  std::vector<std::string>::const_iterator index, end = sortFields.end();

  for (index = sortFields.begin(); index != end; index++)
    parser::parse_collection_sort_column(*m_find->mutable_order(), *index);

  return *this;
}

Find_Sort &Find_Having::having(const std::string &searchCondition)
{
  if (!searchCondition.empty())
    m_find->set_allocated_grouping_criteria(parser::parse_collection_filter(searchCondition, &m_placeholders));

  return *this;
}

Find_Having &Find_GroupBy::groupBy(const std::vector<std::string> &searchFields)
{
  std::vector<std::string>::const_iterator index, end = searchFields.end();

  for (index = searchFields.begin(); index != end; index++)
    m_find->mutable_grouping()->AddAllocated(parser::parse_table_filter(*index));

  return *this;
}

Find_GroupBy &FindStatement::fields(const std::string& projection)
{
  ::mysqlx::Expr_parser parser(projection, true, false, &m_placeholders);

  Mysqlx::Expr::Expr *expr_obj = parser.expr();

  m_find->mutable_projection()->Add()->set_allocated_source(expr_obj);

  return *this;
}

Find_GroupBy &FindStatement::fields(const std::vector<std::string> &searchFields)
{
  std::vector<std::string>::const_iterator index, end = searchFields.end();

  for (index = searchFields.begin(); index != end; index++)
    parser::parse_collection_column_list_with_alias(*m_find->mutable_projection(), *index);

  return *this;
}

FindStatement::FindStatement(boost::shared_ptr<Collection> coll, const std::string &searchCondition)
: Find_GroupBy(coll)
{
  m_find->mutable_collection()->set_schema(coll->schema()->name());
  m_find->mutable_collection()->set_name(coll->name());
  m_find->set_data_model(Mysqlx::Crud::DOCUMENT);

  if (!searchCondition.empty())
    m_find->set_allocated_criteria(parser::parse_collection_filter(searchCondition, &m_placeholders));
}

//----------------------------------

Add_Base::Add_Base(boost::shared_ptr<Collection> coll)
: Collection_Statement(coll), m_insert(new Mysqlx::Crud::Insert())
{
}

Add_Base::Add_Base(const Add_Base &other)
: Collection_Statement(other), m_insert(other.m_insert)
{
}

Add_Base &Add_Base::operator = (const Add_Base &other)
{
  m_insert = other.m_insert;
  return *this;
}

boost::shared_ptr<Result> Add_Base::execute()
{
  // TODO: Inserte MUST have mustable_args to enable parameter binding so this will be hidden for now
  //insert_bound_values(m_insert->mutable_args());

  if (!m_insert->IsInitialized())
    throw std::logic_error("AddStatement is not completely initialized: " + m_insert->InitializationErrorString());

  SessionRef session(m_coll->schema()->session());

  boost::shared_ptr<Result> result;
  if (m_insert->mutable_row()->size())
  {
    result = session->connection()->execute_insert(*m_insert);
    result->wait();
  }
  else
    result = session->connection()->new_empty_result();

  result->setLastDocumentIDs(m_last_document_ids);
  m_last_document_ids.clear();

  return result;
}

AddStatement::AddStatement(boost::shared_ptr<Collection> coll)
  : Add_Base(coll)
{
  m_insert->mutable_collection()->set_schema(coll->schema()->name());
  m_insert->mutable_collection()->set_name(coll->name());
  m_insert->set_data_model(Mysqlx::Crud::DOCUMENT);
}

AddStatement::AddStatement(boost::shared_ptr<Collection> coll, const Document &doc)
: Add_Base(coll)
{
  m_insert->mutable_collection()->set_schema(coll->schema()->name());
  m_insert->mutable_collection()->set_name(coll->name());
  m_insert->set_data_model(Mysqlx::Crud::DOCUMENT);
  add(doc);
}

AddStatement &AddStatement::add(const Document &doc)
{
    ::mysqlx::Expr_parser parser(doc.str(), true, false, &m_placeholders);
    Mysqlx::Expr::Expr *expr_obj = parser.expr();

  if (expr_obj->type() == Mysqlx::Expr::Expr_Type_OBJECT)
    {
    bool found = false;
    int size = expr_obj->object().fld_size();
    int index = 0;
    while (index < size && !found)
    {
      found = expr_obj->object().fld(index).key() == "_id";

      // The document ID is stored as literal-octests
      if (found &&
          expr_obj->object().fld(index).value().has_literal() &&
          expr_obj->object().fld(index).value().literal().has_v_octets())
        m_last_document_ids.push_back(expr_obj->object().fld(index).value().literal().v_octets().value());
      else
        throw std::logic_error("missing document _id");

      index++;
    }

    m_insert->mutable_row()->Add()->mutable_field()->AddAllocated(expr_obj);
  }

  return *this;
}

//--------------------------------------------------------------

Remove_Base::Remove_Base(boost::shared_ptr<Collection> coll)
: Collection_Statement(coll), m_delete(new Mysqlx::Crud::Delete())
{
}

Remove_Base::Remove_Base(const Remove_Base &other)
: Collection_Statement(other), m_delete(other.m_delete)
{
}

Remove_Base &Remove_Base::operator = (const Remove_Base &other)
{
  m_delete = other.m_delete;
  return *this;
}

boost::shared_ptr<Result> Remove_Base::execute()
{
  insert_bound_values(m_delete->mutable_args());

  if (!m_delete->IsInitialized())
    throw std::logic_error("RemoveStatement is not completely initialized: " + m_delete->InitializationErrorString());

  SessionRef session(m_coll->schema()->session());

  boost::shared_ptr<Result> result(session->connection()->execute_delete(*m_delete));

  result->wait();

  return result;
}

Remove_Base &Remove_Limit::limit(uint64_t limit_)
{
  m_delete->mutable_limit()->set_row_count(limit_);
  return *this;
}

RemoveStatement::RemoveStatement(boost::shared_ptr<Collection> coll, const std::string &searchCondition)
: Remove_Limit(coll)
{
  m_delete->mutable_collection()->set_schema(coll->schema()->name());
  m_delete->mutable_collection()->set_name(coll->name());
  m_delete->set_data_model(Mysqlx::Crud::DOCUMENT);

  if (!searchCondition.empty())
    m_delete->set_allocated_criteria(parser::parse_collection_filter(searchCondition, &m_placeholders));
}

Remove_Limit &RemoveStatement::sort(const std::vector<std::string> &sortFields)
{
  std::vector<std::string>::const_iterator index, end = sortFields.end();

  for (index = sortFields.begin(); index != end; index++)
    parser::parse_collection_sort_column(*m_delete->mutable_order(), *index);

  return *this;
}

//--------------------------------------------------------------

Modify_Base::Modify_Base(boost::shared_ptr<Collection> coll)
: Collection_Statement(coll), m_update(new Mysqlx::Crud::Update())
{
}

Modify_Base::Modify_Base(const Modify_Base &other)
: Collection_Statement(other), m_update(other.m_update)
{
}

Modify_Base &Modify_Base::operator = (const Modify_Base &other)
{
  m_coll = other.m_coll;
  m_update = other.m_update;
  return *this;
}

boost::shared_ptr<Result> Modify_Base::execute()
{
  insert_bound_values(m_update->mutable_args());

  if (!m_update->IsInitialized())
    throw std::logic_error("ModifyStatement is not completely initialized: " + m_update->InitializationErrorString());

  SessionRef session(m_coll->schema()->session());

  boost::shared_ptr<Result> result(session->connection()->execute_update(*m_update));

  result->wait();

  return result;
}

Modify_Base &Modify_Limit::limit(uint64_t limit_)
{
  m_update->mutable_limit()->set_row_count(limit_);
  return *this;
}

Modify_Limit &Modify_Sort::sort(const std::vector<std::string> &sortFields)
{
  std::vector<std::string>::const_iterator index, end = sortFields.end();

  for (index = sortFields.begin(); index != end; index++)
    parser::parse_collection_sort_column(*m_update->mutable_order(), *index);

  return *this;
}

Modify_Operation &Modify_Operation::set_operation(int type, const std::string &path, const DocumentValue *value, bool validate_array)
{
  // Sets the operation
  Mysqlx::Crud::UpdateOperation * operation = m_update->mutable_operation()->Add();
  operation->set_operation(Mysqlx::Crud::UpdateOperation_UpdateType(type));

  Mysqlx::Expr::Expr *docpath = parser::parse_column_identifier(path.empty() ? "$" : path);
  Mysqlx::Expr::ColumnIdentifier identifier(docpath->identifier());

  // Validates the source is an array item
  int size = identifier.document_path().size();
  if (size)
  {
    if (validate_array)
    {
      if (identifier.document_path().Get(size - 1).type() != Mysqlx::Expr::DocumentPathItem::ARRAY_INDEX)
        throw std::logic_error("An array document path must be specified");
    }
  }
  else if (type != Mysqlx::Crud::UpdateOperation::ITEM_MERGE)
    throw std::logic_error("Invalid document path");

  // Sets the source
  operation->mutable_source()->CopyFrom(identifier);

  // Sets the value if applicable
  if (value)
  {
    if (value->type() == DocumentValue::TExpression ||
        value->type() == DocumentValue::TDocument ||
        value->type() == DocumentValue::TArray)
    {
      DocumentValue expression(*value);
      Expr_parser parser(expression, true, false, &m_placeholders);
      operation->set_allocated_value(parser.expr());
    }
    else
    {
      operation->mutable_value()->set_type(Mysqlx::Expr::Expr::LITERAL);
      operation->mutable_value()->set_allocated_literal(convert_document_value(*value));
    }
  }

  delete docpath;

  return *this;
}

Modify_Operation &Modify_Operation::remove(const std::string &path)
{
  return set_operation(Mysqlx::Crud::UpdateOperation::ITEM_REMOVE, path);
}

Modify_Operation &Modify_Operation::arrayDelete(const std::string &path)
{
  return set_operation(Mysqlx::Crud::UpdateOperation::ITEM_REMOVE, path, NULL, true);
}

Modify_Operation &Modify_Operation::set(const std::string &path, const DocumentValue &value)
{
  return set_operation(Mysqlx::Crud::UpdateOperation::ITEM_SET, path, &value);
}

Modify_Operation &Modify_Operation::change(const std::string &path, const DocumentValue &value)
{
  return set_operation(Mysqlx::Crud::UpdateOperation::ITEM_REPLACE, path, &value);
}

Modify_Operation &Modify_Operation::merge(const DocumentValue &document)
{
  return set_operation(Mysqlx::Crud::UpdateOperation::ITEM_MERGE, "", &document);
}

Modify_Operation &Modify_Operation::arrayInsert(const std::string &path, const DocumentValue &value)
{
  return set_operation(Mysqlx::Crud::UpdateOperation::ARRAY_INSERT, path, &value, true);
}

Modify_Operation &Modify_Operation::arrayAppend(const std::string &path, const DocumentValue &value)
{
  return set_operation(Mysqlx::Crud::UpdateOperation::ARRAY_APPEND, path, &value);
}

ModifyStatement::ModifyStatement(boost::shared_ptr<Collection> coll, const std::string& searchCondition)
: Modify_Operation(coll)
{
  m_update->mutable_collection()->set_schema(coll->schema()->name());
  m_update->mutable_collection()->set_name(coll->name());
  m_update->set_data_model(Mysqlx::Crud::DOCUMENT);

  if (!searchCondition.empty())
    m_update->set_allocated_criteria(parser::parse_collection_filter(searchCondition, &m_placeholders));
}

//--------------------------------------------------------------

Table_Statement::Table_Statement(boost::shared_ptr<Table> table)
: m_table(table)
{
}

Table_Statement::Table_Statement(const Table_Statement& other)
: Statement(other), m_table(other.m_table)
{
}

Table_Statement &Table_Statement::bind(const std::string &name, const TableValue &value)
{
  init_bound_values();

  validate_bind_placeholder(name);

  // Now sets the right value on the position of the indicated placeholder
  std::vector<std::string>::iterator index = std::find(m_placeholders.begin(), m_placeholders.end(), name);
  m_bound_values[index - m_placeholders.begin()] = convert_table_value(value);

  return *this;
}

Mysqlx::Datatypes::Scalar* Table_Statement::convert_table_value(const TableValue& value)
{
  Mysqlx::Datatypes::Scalar *my_scalar = new Mysqlx::Datatypes::Scalar;

  mysqlx::TableValue column_value(value);

  switch (value.type())
  {
    case TableValue::TInteger:
      my_scalar->set_type(Mysqlx::Datatypes::Scalar::V_SINT);
      my_scalar->set_v_signed_int(column_value);
      break;
    case TableValue::TUInteger:
      my_scalar->set_type(Mysqlx::Datatypes::Scalar::V_UINT);
      my_scalar->set_v_unsigned_int(column_value);
      break;
    case TableValue::TBool:
      my_scalar->set_type(Mysqlx::Datatypes::Scalar::V_BOOL);
      my_scalar->set_v_bool(column_value);
      break;
    case TableValue::TDouble:
      my_scalar->set_type(Mysqlx::Datatypes::Scalar::V_DOUBLE);
      my_scalar->set_v_double(column_value);
      break;
    case TableValue::TFloat:
      my_scalar->set_type(Mysqlx::Datatypes::Scalar::V_FLOAT);
      my_scalar->set_v_float(column_value);
      break;
    case TableValue::TNull:
      my_scalar->set_type(Mysqlx::Datatypes::Scalar::V_NULL);
      break;
    case TableValue::TOctets:
      my_scalar->set_type(Mysqlx::Datatypes::Scalar::V_OCTETS);
      my_scalar->mutable_v_octets()->set_value(column_value);
      break;
    case TableValue::TString:
      my_scalar->set_type(Mysqlx::Datatypes::Scalar::V_STRING);
      my_scalar->mutable_v_string()->set_value(column_value);
      break;
    case TableValue::TExpression:
      //XXX TODO
      break;
  }

  return my_scalar;
}

//Collection_Statement &Collection_Statement::bind(const std::string &UNUSED(name), const DocumentValue &UNUSED(value))
//{
//  return *this;
//}

Delete_Base::Delete_Base(boost::shared_ptr<Table> table)
: Table_Statement(table), m_delete(new Mysqlx::Crud::Delete())
{
}

Delete_Base::Delete_Base(const Delete_Base &other)
: Table_Statement(other), m_delete(other.m_delete)
{
}

Delete_Base &Delete_Base::operator = (const Delete_Base &other)
{
  m_delete = other.m_delete;
  return *this;
}

boost::shared_ptr<Result> Delete_Base::execute()
{
  insert_bound_values(m_delete->mutable_args());

  if (!m_delete->IsInitialized())
    throw std::logic_error("DeleteStatement is not completely initialized: " + m_delete->InitializationErrorString());

  SessionRef session(m_table->schema()->session());

  boost::shared_ptr<Result> result(session->connection()->execute_delete(*m_delete));

  result->wait();

  return result;
}

Delete_Base &Delete_Limit::limit(uint64_t limit_)
{
  m_delete->mutable_limit()->set_row_count(limit_);
  return *this;
}

Delete_Limit &Delete_OrderBy::orderBy(const std::vector<std::string> &sortFields)
{
  std::vector<std::string>::const_iterator index, end = sortFields.end();

  for (index = sortFields.begin(); index != end; index++)
    parser::parse_table_sort_column(*m_delete->mutable_order(), *index);

  return *this;
}

DeleteStatement::DeleteStatement(boost::shared_ptr<Table> table)
: Delete_OrderBy(table)
{
  m_delete->mutable_collection()->set_schema(table->schema()->name());
  m_delete->mutable_collection()->set_name(table->name());
  m_delete->set_data_model(Mysqlx::Crud::TABLE);
}

Delete_OrderBy &DeleteStatement::where(const std::string& searchCondition)
{
  if (!searchCondition.empty())
    m_delete->set_allocated_criteria(parser::parse_table_filter(searchCondition, &m_placeholders));

  return *this;
}

//--------------------------------------------------------------

Update_Base::Update_Base(boost::shared_ptr<Table> table)
: Table_Statement(table), m_update(new Mysqlx::Crud::Update())
{
}

Update_Base::Update_Base(const Update_Base &other)
: Table_Statement(other), m_update(other.m_update)
{
}

Update_Base &Update_Base::operator = (const Update_Base &other)
{
  m_update = other.m_update;
  return *this;
}

boost::shared_ptr<Result> Update_Base::execute()
{
  insert_bound_values(m_update->mutable_args());

  if (!m_update->IsInitialized())
    throw std::logic_error("UpdateStatement is not completely initialized: " + m_update->InitializationErrorString());

  SessionRef session(m_table->schema()->session());

  boost::shared_ptr<Result> result(session->connection()->execute_update(*m_update));

  result->wait();

  return result;
}

Update_Base &Update_Limit::limit(uint64_t limit_)
{
  m_update->mutable_limit()->set_row_count(limit_);
  return *this;
}

Update_Limit &Update_OrderBy::orderBy(const std::vector<std::string> &sortFields)
{
  std::vector<std::string>::const_iterator index, end = sortFields.end();

  for (index = sortFields.begin(); index != end; index++)
    parser::parse_table_sort_column(*m_update->mutable_order(), *index);

  return *this;
}

Update_OrderBy &Update_Where::where(const std::string& searchCondition)
{
  if (!searchCondition.empty())
    m_update->set_allocated_criteria(parser::parse_table_filter(searchCondition, &m_placeholders));

  return *this;
}

Update_Set &Update_Set::set(const std::string &field, const TableValue& value)
{
  Mysqlx::Crud::UpdateOperation *operation = m_update->mutable_operation()->Add();

  operation->mutable_source()->set_name(field);

  operation->set_operation(Mysqlx::Crud::UpdateOperation::SET);
  operation->mutable_value()->set_type(Mysqlx::Expr::Expr::LITERAL);
  operation->mutable_value()->set_allocated_literal(convert_table_value(value));

  return *this;
}

Update_Set &Update_Set::set(const std::string &field, const std::string& expression)
{
  Mysqlx::Crud::UpdateOperation *operation = m_update->mutable_operation()->Add();

  operation->mutable_source()->set_name(field);

  operation->set_operation(Mysqlx::Crud::UpdateOperation::SET);

  Expr_parser parser(expression, false, false, &m_placeholders);
  operation->set_allocated_value(parser.expr());

  return *this;
}

UpdateStatement::UpdateStatement(boost::shared_ptr<Table> table)
: Update_Set(table)
{
  m_update->mutable_collection()->set_schema(table->schema()->name());
  m_update->mutable_collection()->set_name(table->name());
  m_update->set_data_model(Mysqlx::Crud::TABLE);
}

//--------------------------------------------------------------

Select_Base::Select_Base(boost::shared_ptr<Table> table)
: Table_Statement(table), m_find(new Mysqlx::Crud::Find())
{
}

Select_Base::Select_Base(const Select_Base &other)
: Table_Statement(other), m_find(other.m_find)
{
}

Select_Base &Select_Base::operator = (const Select_Base &other)
{
  m_find = other.m_find;
  return *this;
}

boost::shared_ptr<Result> Select_Base::execute()
{
  insert_bound_values(m_find->mutable_args());

  if (!m_find->IsInitialized())
    throw std::logic_error("SelectStatement is not completely initialized: " + m_find->InitializationErrorString());

  SessionRef session(m_table->schema()->session());

  boost::shared_ptr<Result> result(session->connection()->execute_find(*m_find));

  // wait for results (at least metadata) to arrive
  result->wait();

  return result;
}

Select_Base &Select_Offset::offset(uint64_t offset_)
{
  m_find->mutable_limit()->set_offset(offset_);
  return *this;
}

Select_Offset &Select_Limit::limit(uint64_t limit_)
{
  m_find->mutable_limit()->set_row_count(limit_);
  return *this;
}

Select_Limit &Select_OrderBy::orderBy(const std::vector<std::string> &sortFields)
{
  std::vector<std::string>::const_iterator index, end = sortFields.end();

  for (index = sortFields.begin(); index != end; index++)
    parser::parse_table_sort_column(*m_find->mutable_order(), *index);

  return *this;
}

Select_OrderBy &Select_Having::having(const std::string &searchCondition)
{
  if (!searchCondition.empty())
    m_find->set_allocated_grouping_criteria(parser::parse_table_filter(searchCondition, &m_placeholders));

  return *this;
}

Select_Having &Select_GroupBy::groupBy(const std::vector<std::string> &searchFields)
{
  std::vector<std::string>::const_iterator index, end = searchFields.end();

  for (index = searchFields.begin(); index != end; index++)
    m_find->mutable_grouping()->AddAllocated(parser::parse_table_filter(*index));

  return *this;
}

SelectStatement::SelectStatement(boost::shared_ptr<Table> table, const std::vector<std::string> &fieldList)
: Select_GroupBy(table)
{
  m_find->mutable_collection()->set_schema(table->schema()->name());
  m_find->mutable_collection()->set_name(table->name());
  m_find->set_data_model(Mysqlx::Crud::TABLE);

  if (!fieldList.empty())
  {
    std::vector<std::string>::const_iterator index, end = fieldList.end();

    for (index = fieldList.begin(); index != end; index++)
      parser::parse_table_column_list_with_alias(*m_find->mutable_projection(), *index);
  }
}

Select_GroupBy &SelectStatement::where(const std::string &searchCondition)
{
  if (!searchCondition.empty())
    m_find->set_allocated_criteria(parser::parse_table_filter(searchCondition, &m_placeholders));

  return *this;
}

//--------------------------------------------------------------

Insert_Base::Insert_Base(boost::shared_ptr<Table> table)
: Table_Statement(table), m_insert(new Mysqlx::Crud::Insert())
{
}

Insert_Base::Insert_Base(const Insert_Base &other)
: Table_Statement(other), m_insert(other.m_insert)
{
}

Insert_Base &Insert_Base::operator = (const Insert_Base &other)
{
  m_insert = other.m_insert;
  return *this;
}

boost::shared_ptr<Result> Insert_Base::execute()
{
  if (!m_insert->IsInitialized())
    throw std::logic_error("InsertStatement is not completely initialized: " + m_insert->InitializationErrorString());

  SessionRef session(m_table->schema()->session());

  boost::shared_ptr<Result> result(session->connection()->execute_insert(*m_insert));

  result->wait();

  return result;
}

Insert_Values::Insert_Values(boost::shared_ptr<Table> table)
: Insert_Base(table)
{
}

Insert_Values &Insert_Values::values(const std::vector<TableValue> &row_data)
{
  Mysqlx::Crud::Insert_TypedRow* row = m_insert->mutable_row()->Add();
  std::vector<TableValue>::const_iterator index, end = row_data.end();

  for (index = row_data.begin(); index != end; index++)
  {
    if (index->type() == TableValue::TExpression)
    {
      TableValue expression(*index);
      Expr_parser parser(expression, false, false, &m_placeholders);
      row->mutable_field()->AddAllocated(parser.expr());
    }
    else
    {
    Mysqlx::Expr::Expr* expr = new Mysqlx::Expr::Expr();
    expr->set_type(Mysqlx::Expr::Expr::LITERAL);
    Mysqlx::Datatypes::Scalar* scalar = convert_table_value(*index);
    expr->set_allocated_literal(scalar);
    row->mutable_field()->AddAllocated(expr);
  }
  }

  return *this;
}

Insert_Values &InsertStatement::insert(const std::vector<std::string> &columns)
{
  for (size_t index = 0; index < columns.size(); index++)
    m_insert->mutable_projection()->Add()->set_name(columns[index]);

  return *this;
}

InsertStatement::InsertStatement(boost::shared_ptr<Table> table)
: Insert_Values(table)
{
  m_insert->mutable_collection()->set_schema(table->schema()->name());
  m_insert->mutable_collection()->set_name(table->name());
  m_insert->set_data_model(Mysqlx::Crud::TABLE);
}

//--------------------------------------------------------------