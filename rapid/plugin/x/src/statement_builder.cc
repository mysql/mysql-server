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

#include "statement_builder.h"
#include "ngs_common/protocol_protobuf.h"
#include "xpl_error.h"
#include "ngs_common/bind.h"


void xpl::Statement_builder::add_collection(const Collection &collection) const
{
  if (!collection.has_name() || collection.name().empty())
    throw ngs::Error_code(ER_X_BAD_TABLE, "Invalid name of table/collection");

  if (collection.has_schema() && !collection.schema().empty())
    m_builder.put_identifier(collection.schema()).dot();

  m_builder.put_identifier(collection.name());
}


void xpl::Crud_statement_builder::add_filter(const Filter &filter) const
{
  if (filter.IsInitialized())
    m_builder.put(" WHERE ").put_expr(filter);
}


void xpl::Crud_statement_builder::add_order_item(const Order_item &item) const
{
  m_builder.put_expr(item.expr());
  if (item.direction() == ::Mysqlx::Crud::Order::DESC)
    m_builder.put(" DESC");
}


void xpl::Crud_statement_builder::add_order(const Order_list &order) const
{
  if (order.size() == 0)
    return;

  m_builder.put(" ORDER BY ")
      .put_list(order, ngs::bind(&Crud_statement_builder::add_order_item,
                                 this, ngs::placeholders::_1));
}


void xpl::Crud_statement_builder::add_limit(const Limit &limit,
                                            const bool no_offset) const
{
  if (!limit.IsInitialized())
    return;

  m_builder.put(" LIMIT ");
  if (limit.has_offset())
  {
    if (no_offset && limit.offset() != 0)
      throw ngs::Error_code(ER_X_INVALID_ARGUMENT,
                            "Invalid parameter: non-zero offset "
                            "value not allowed for this operation");
    if (!no_offset)
      m_builder.put(limit.offset()).put(", ");
  }
  m_builder.put(limit.row_count());
}
