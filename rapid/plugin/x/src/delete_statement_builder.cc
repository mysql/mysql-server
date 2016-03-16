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

#include "delete_statement_builder.h"
#include "ngs_common/protocol_protobuf.h"


xpl::Delete_statement_builder::Delete_statement_builder(const Delete &msg, Query_string_builder &qb)
: Statement_builder(qb, msg.args(), msg.collection().schema(), msg.data_model() == Mysqlx::Crud::TABLE),
  m_msg(msg)
{}


void xpl::Delete_statement_builder::add_statement() const
{
  m_builder.put("DELETE FROM ");
  add_table(m_msg.collection());
  add_filter(m_msg.criteria());
  add_order(m_msg.order());
  add_limit(m_msg.limit(), true);
}
