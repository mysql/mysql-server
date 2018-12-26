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


void xpl::Insert_statement_builder::build(const Insert &msg) const
{
  m_builder.put("INSERT INTO ");
  add_collection(msg.collection());
  add_projection(msg.projection(), is_table_data_model(msg));
  add_values(msg.row(), is_table_data_model(msg) ? msg.projection().size() : 1);
}


void xpl::Insert_statement_builder::add_projection(const Projection_list &projection,
                                                        const bool is_relational) const
{
  if (is_relational)
  {
    if (projection.size() != 0)
      m_builder.put(" (")
        .put_list(projection, ngs::bind(&Generator::put_identifier, m_builder,
                                        ngs::bind(&Mysqlx::Crud::Column::name, ngs::placeholders::_1)))
        .put(")");
  }
  else
  {
    if (projection.size() != 0)
      throw ngs::Error_code(ER_X_BAD_PROJECTION, "Invalid projection for document operation");
    m_builder.put(" (doc)");
  }
}


void xpl::Insert_statement_builder::add_values(const Row_list &values, const int projection_size) const
{
  if (values.size() == 0)
    throw ngs::Error_code(ER_X_MISSING_ARGUMENT, "Missing row data for Insert");

  m_builder.put(" VALUES ")
    .put_list(values, ngs::bind(&Insert_statement_builder::add_row, this,
                                ngs::bind(&Insert_statement_builder::get_row_fields, this, ngs::placeholders::_1),
                                projection_size));
}


void xpl::Insert_statement_builder::add_row(const Field_list &row, const int projection_size) const
{
  if ((row.size() == 0) || (projection_size && row.size() != projection_size))
    throw ngs::Error_code(ER_X_BAD_INSERT_DATA, "Wrong number of fields in row being inserted");

  m_builder.put("(").put_list(row, &Generator::put_expr).put(")");
}
