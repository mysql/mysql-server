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

#ifndef DELETE_STATEMENT_BUILDER_H_
#define DELETE_STATEMENT_BUILDER_H_

#include "statement_builder.h"

namespace xpl
{

class Delete_statement_builder: public Statement_builder
{
public:
  typedef ::Mysqlx::Crud::Delete Delete;

  Delete_statement_builder(const Delete &msg, Query_string_builder &qb);

protected:
  virtual void add_statement() const;

  const Delete &m_msg;
};

} // namespace xpl

#endif // DELETE_STATEMENT_BUILDER_H_
