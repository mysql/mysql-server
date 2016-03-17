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

#include "insert_statement_builder.h"
#include "ngs_common/protocol_protobuf.h"
#include "xpl_error.h"


xpl::Insert_statement_builder::Insert_statement_builder(const Insert &msg, Query_string_builder &qb)
: Statement_builder(qb, msg.args(), msg.collection().schema(), msg.data_model() == Mysqlx::Crud::TABLE),
  m_msg(msg)
{}


void xpl::Insert_statement_builder::add_statement() const
{
  m_builder.put("INSERT INTO ");
  add_table(m_msg.collection());
  add_projection(m_msg.projection());
  add_values(m_msg.row());
}


void xpl::Insert_statement_builder::add_projection(const Projection_list &projection) const
{
  if (m_is_relational)
  {
    if (projection.size() != 0)
      m_builder.put(" (").put_list(projection,
                                   boost::bind(&Builder::put_identifier, m_builder,
                                               boost::bind(&Mysqlx::Crud::Column::name, _1))).put(")");
  }
  else
  {
    if (projection.size() != 0)
      throw ngs::Error_code(ER_X_BAD_PROJECTION, "Invalid projection for document operation");
    m_builder.put(" (doc)");
  }
}


void xpl::Insert_statement_builder::add_values(const Row_list &values) const
{
  if (values.size() == 0)
    throw ngs::Error_code(ER_X_MISSING_ARGUMENT, "Missing row data for Insert");

  m_builder.put(" VALUES ").put_list(values,
                                     boost::bind(&Insert_statement_builder::add_row, this,
                                                 boost::bind(&Mysqlx::Crud::Insert_TypedRow::field, _1),
                                                 m_is_relational ? m_msg.projection().size() : 1));
}


void xpl::Insert_statement_builder::add_row(const Field_list &row, int projection_size) const
{
  if ((row.size() == 0) || (projection_size && row.size() != projection_size))
    throw ngs::Error_code(ER_X_BAD_INSERT_DATA, "Wrong number of fields in row being inserted");

  m_builder.put("(").put_list(row).put(")");
}
