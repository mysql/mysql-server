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

class Delete_statement_builder: public Crud_statement_builder
{
public:
  typedef ::Mysqlx::Crud::Delete Delete;

  explicit Delete_statement_builder(const Expression_generator &gen)
      : Crud_statement_builder(gen) {}

  void build(const Delete &msg) const;
};

} // namespace xpl

#endif // DELETE_STATEMENT_BUILDER_H_
