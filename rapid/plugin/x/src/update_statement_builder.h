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

#ifndef UPDATE_STATEMENT_BUILDER_H_
#define UPDATE_STATEMENT_BUILDER_H_

#include "statement_builder.h"

namespace xpl
{

class Update_statement_builder: public Statement_builder
{
public:
  typedef ::Mysqlx::Crud::Update Update;

  Update_statement_builder(const Update &msg, Query_string_builder &qb);

protected:
  typedef ::Mysqlx::Crud::UpdateOperation Operation_item;
  typedef ::google::protobuf::RepeatedPtrField<Operation_item> Operation_list;
  typedef Operation_list::const_iterator Operation_iterator;

  virtual void add_statement() const;

  void add_operation(const Operation_list &operation) const;
  void add_table_operation(const Operation_list &operation) const;
  void add_table_operation_items(Operation_iterator begin, Operation_iterator end) const;
  void add_document_operation(const Operation_list &operation, const std::string &doc_column) const;
  void add_document_operation_item(const Operation_item &item, Builder &qb, bool &is_id_synch, int &opeartion_id) const;

  const Update &m_msg;
};

} // namespace xpl

#endif // UPDATE_STATEMENT_BUILDER_H_
