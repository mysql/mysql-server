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
#include "ngs_common/protocol_protobuf.h"
#include "xpl_error.h"


xpl::Find_statement_builder::Find_statement_builder(const Find &msg, Query_string_builder &qb)
: Statement_builder(qb, msg.args(), msg.collection().schema(), msg.data_model() == Mysqlx::Crud::TABLE),
  m_msg(msg)
{}


void xpl::Find_statement_builder::add_statement() const
{
  if (!m_is_relational && m_msg.grouping_size() > 0)
    add_document_statement_with_grouping();
  else
    add_statement_common();
}


void xpl::Find_statement_builder::add_statement_common() const
{
  m_builder.put("SELECT ");
  add_projection(m_msg.projection());
  m_builder.put(" FROM ");
  add_table(m_msg.collection());
  add_filter(m_msg.criteria());
  add_grouping(m_msg.grouping());
  add_grouping_criteria(m_msg.grouping_criteria());
  add_order(m_msg.order());
  add_limit(m_msg.limit(), false);
}


namespace
{
const char* const DERIVED_TABLE_NAME = "`_DERIVED_TABLE_`";
} // namespace


void xpl::Find_statement_builder::add_document_statement_with_grouping() const
{
  if (m_msg.projection_size() == 0)
    throw ngs::Error_code(ER_X_BAD_PROJECTION, "Invalid empty projection list for grouping");

  m_builder.put("SELECT ");
  add_document_object(m_msg.projection(), &Find_statement_builder::add_document_primary_projection_item);
  m_builder.put(" FROM (");
  m_builder.put("SELECT ");
  add_table_projection(m_msg.projection());
  m_builder.put(" FROM ");
  add_table(m_msg.collection());
  add_filter(m_msg.criteria());
  add_grouping(m_msg.grouping());
  add_order(m_msg.order());
  add_limit(m_msg.limit(), false);
  m_builder.put(") AS ").put(DERIVED_TABLE_NAME);
  add_grouping_criteria(m_msg.grouping_criteria());
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

  add_document_object(projection, &Find_statement_builder::add_document_projection_item);
}


void xpl::Find_statement_builder::add_document_object(const Projection_list &projection,
                                                      const Object_item_adder &adder) const
{
  m_builder.put("JSON_OBJECT(")
      .put_list(projection, boost::bind(adder, this, _1))
      .put(") AS doc");
}


void xpl::Find_statement_builder::add_document_projection_item(const Projection &item) const
{
  if (!item.has_alias())
    throw ngs::Error_code(ER_X_PROJ_BAD_KEY_NAME, "Invalid projection target name");

  m_builder.put_quote(item.alias()).put(", ").gen(item.source());
}


void xpl::Find_statement_builder::add_document_primary_projection_item(const Projection &item) const
{
  if (!item.has_alias())
    throw ngs::Error_code(ER_X_PROJ_BAD_KEY_NAME, "Invalid projection target name");

  m_builder.put_quote(item.alias()).put(", ")
      .put(DERIVED_TABLE_NAME).dot().put_identifier(item.alias());
}


void xpl::Find_statement_builder::add_grouping(const Grouping_list &group) const
{
  if (group.size() > 0)
    m_builder.put(" GROUP BY ").put_list(group);
}


void xpl::Find_statement_builder::add_grouping_criteria(const Grouping_criteria &criteria) const
{
  if (criteria.IsInitialized())
    m_builder.put(" HAVING ").gen(criteria);
}
