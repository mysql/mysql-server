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

#ifndef _XPL_CRUD_CMD_HANDLER_H_
#define _XPL_CRUD_CMD_HANDLER_H_

#include "ngs/protocol_fwd.h"
#include "ngs/error_code.h"
#include "query_string_builder.h"

#include <google/protobuf/repeated_field.h>

#include <map>

namespace ngs
{
  class Protocol_encoder;
}

namespace xpl
{
class Sql_data_context;
class Session_options;
class Session_status_variables;

class Crud_command_handler
{
public:
  Crud_command_handler(Sql_data_context &da, Session_options &options, Session_status_variables &status_variables);

  ngs::Error_code execute_crud_insert(ngs::Protocol_encoder &proto,
                                      const Mysqlx::Crud::Insert &msg);

  ngs::Error_code execute_crud_update(ngs::Protocol_encoder &proto,
                                      const Mysqlx::Crud::Update &msg);

  ngs::Error_code execute_crud_find(ngs::Protocol_encoder &proto,
                                      const Mysqlx::Crud::Find &msg);

  ngs::Error_code execute_crud_delete(ngs::Protocol_encoder &proto,
                                      const Mysqlx::Crud::Delete &msg);

private:
  Sql_data_context &m_da;
  Session_options &m_options;
  Query_string_builder m_qb;
  Session_status_variables &m_status_variables;
};

} // namespace xpl

#endif // _XPL_CRUD_CMD_HANDLER_H_
