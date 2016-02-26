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

#include "find_statement_builder.h"
#include "mysqlx_crud.pb.h"
#include "xpl_error.h"


xpl::Find_statement_builder::Find_statement_builder(const Find &msg, Query_string_builder &qb)
: Statement_builder(qb, msg.args(), msg.collection().schema(), msg.data_model() == Mysqlx::Crud::TABLE),
  m_msg(msg)
{}


void xpl::Find_statement_builder::add_statement() const
{
  m_builder.put("SELECT ");
  add_projection(m_msg.projection());
  m_builder.put(" FROM ");
  add_table(m_msg.collection());
  add_filter(m_msg.criteria());
  add_grouping(m_msg.grouping(), m_msg.grouping_criteria());
  add_order(m_msg.order());
  add_limit(m_msg.limit(), false);
}


void xpl::Find_statement_builder::add_projection(const Projection_list &projection) const
{
  if (projection.size() == 0)
  {
    m_builder.put(m_is_relational ? "*" : "doc");
    return;
  }

  if (m_is_relational)
    add_table_projection(projection);
  else
    add_document_projection(projection);
}


void xpl::Find_statement_builder::add_table_projection(const Projection_list &projection) const
{
  m_builder.put_list(projection, boost::bind(&Find_statement_builder::add_table_projection_item, this, _1));
}


void xpl::Find_statement_builder::add_table_projection_item(const Projection &item) const
{
  m_builder.gen(item.source());
  if (item.has_alias())
    m_builder.put(" AS ").put_identifier(item.alias());
}


void xpl::Find_statement_builder::add_document_projection(const Projection_list &projection) const
{
  if (projection.size() == 1 &&
      !projection.Get(0).has_alias() &&
      projection.Get(0).source().type() == Mysqlx::Expr::Expr::OBJECT)
  {
    m_builder.gen(projection.Get(0).source()).put(" AS doc");
    return;
  }

  m_builder.put("JSON_OBJECT(").
      put_list(projection, boost::bind(&Find_statement_builder::add_document_projection_item, this, _1)).
      put(") AS doc");
}


void xpl::Find_statement_builder::add_document_projection_item(const Projection &item) const
{
  //TODO: if the source expression contains a *, then the fields in the original doc should be merged with the projected ones
  //TODO: when target_alias is nested documents
  if (!item.has_alias())
    throw ngs::Error_code(ER_X_PROJ_BAD_KEY_NAME, "Invalid projection target name");

  m_builder.put_quote(item.alias()).put(", ").gen(item.source());
}


void xpl::Find_statement_builder::add_grouping(const Grouping_list &group,
                                               const Having &having) const
{
  if (group.size() == 0)
    return;

  m_builder.put(" GROUP BY ").put_list(group);

  if (having.IsInitialized())
    m_builder.put(" HAVING ").gen(having);
}

