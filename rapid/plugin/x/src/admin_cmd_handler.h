/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
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

#ifndef _ADMIN_CMD_HANDLER_H_
#define _ADMIN_CMD_HANDLER_H_

#include <string>
#include "ngs/protocol_encoder.h"
#include "ngs_common/protocol_protobuf.h"

namespace xpl
{
  class Session;
  class Sql_data_context;
  class Session_options;

  class Admin_command_handler
  {
  public:
    typedef ::google::protobuf::RepeatedPtrField< ::Mysqlx::Datatypes::Any > Argument_list;
    typedef ngs::Error_code (*Command_handler_ptr)(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args);

    static ngs::Error_code execute(Session &session, Sql_data_context &da, Session_options &options, const std::string &command, const Argument_list &args);

  private:
    typedef std::map<std::string, Command_handler_ptr> Command_handler_map;

    static Command_handler_map m_command_handlers;
    static struct Command_handler_map_init
    {
      Command_handler_map_init();
    } m_command_handler_init;

  private:
    static ngs::Error_code ping(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args);

    static ngs::Error_code list_clients(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args);
    static ngs::Error_code kill_client(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args);

    static ngs::Error_code create_collection(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args);
    static ngs::Error_code create_collection_index(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args);

    static ngs::Error_code drop_collection_or_table(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args);
    static ngs::Error_code drop_collection_index(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args);

    static ngs::Error_code list_objects(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args);

    static ngs::Error_code enable_notices(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args);
    static ngs::Error_code disable_notices(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args);
    static ngs::Error_code list_notices(Session &session, Sql_data_context &da, Session_options &options, const Argument_list &args);

  };
}

#endif
