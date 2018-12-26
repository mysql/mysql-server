/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_SRC_ADMIN_CMD_HANDLER_H_
#define PLUGIN_X_SRC_ADMIN_CMD_HANDLER_H_

#include <google/protobuf/repeated_field.h>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

#include "plugin/x/ngs/include/ngs/error_code.h"
#include "plugin/x/ngs/include/ngs/interface/sql_session_interface.h"
#include "plugin/x/ngs/include/ngs/protocol_fwd.h"

namespace xpl {
class Session;
class Session_options;

class Admin_command_handler {
 public:
  class Command_arguments {
   public:
    using Argument_list = std::vector<std::string>;
    using List = ::google::protobuf::RepeatedPtrField<::Mysqlx::Datatypes::Any>;
    using Argument_name_list = std::initializer_list<const char *const>;
    static const char *const PLACEHOLDER;

    virtual ~Command_arguments() {}
    virtual Command_arguments &string_arg(Argument_name_list name,
                                          std::string *ret_value,
                                          const bool optional = false) = 0;
    virtual Command_arguments &string_list(Argument_name_list name,
                                           std::vector<std::string> *ret_value,
                                           const bool optional = false) = 0;
    virtual Command_arguments &sint_arg(Argument_name_list name,
                                        int64_t *ret_value,
                                        const bool optional = false) = 0;
    virtual Command_arguments &uint_arg(Argument_name_list name,
                                        uint64_t *ret_value,
                                        const bool optional = false) = 0;
    virtual Command_arguments &bool_arg(Argument_name_list name,
                                        bool *ret_value,
                                        const bool optional = false) = 0;
    virtual Command_arguments &docpath_arg(Argument_name_list name,
                                           std::string *ret_value,
                                           const bool optional = false) = 0;
    virtual Command_arguments &object_list(
        Argument_name_list name, std::vector<Command_arguments *> *ret_value,
        const bool optional = false, unsigned expected_members_count = 3) = 0;

    virtual bool is_end() const = 0;
    virtual const ngs::Error_code &end() = 0;
    virtual const ngs::Error_code &error() const = 0;
  };

  explicit Admin_command_handler(Session *session);

  ngs::Error_code execute(const std::string &name_space,
                          const std::string &command, Command_arguments *args);

  static const char *const MYSQLX_NAMESPACE;

 protected:
  using Argument_list = Command_arguments::Argument_list;
  using Value_list = Command_arguments::List;

  ngs::Error_code ping(const std::string &name_space, Command_arguments *args);

  ngs::Error_code list_clients(const std::string &name_space,
                               Command_arguments *args);
  ngs::Error_code kill_client(const std::string &name_space,
                              Command_arguments *args);

  ngs::Error_code create_collection(const std::string &name_space,
                                    Command_arguments *args);
  ngs::Error_code drop_collection(const std::string &name_space,
                                  Command_arguments *args);
  ngs::Error_code ensure_collection(const std::string &name_space,
                                    Command_arguments *args);

  ngs::Error_code create_collection_index(const std::string &name_space,
                                          Command_arguments *args);
  ngs::Error_code drop_collection_index(const std::string &name_space,
                                        Command_arguments *args);

  ngs::Error_code list_objects(const std::string &name_space,
                               Command_arguments *args);

  ngs::Error_code enable_notices(const std::string &name_space,
                                 Command_arguments *args);
  ngs::Error_code disable_notices(const std::string &name_space,
                                  Command_arguments *args);
  ngs::Error_code list_notices(const std::string &name_space,
                               Command_arguments *args);

  using Method_ptr = ngs::Error_code (Admin_command_handler::*)(
      const std::string &name_space, Command_arguments *args);
  static const struct Command_handler
      : private std::map<std::string, Method_ptr> {
    Command_handler();
    ngs::Error_code execute(Admin_command_handler *admin,
                            const std::string &name_space,
                            const std::string &command,
                            Command_arguments *args) const;
  } m_command_handler;

  Session *m_session;
};

#define DOC_MEMBER_REGEX                                                     \
  R"(\\$((\\*{2})?(\\[([[:digit:]]+|\\*)\\]|\\.([[:alpha:]_\\$][[:alnum:]_)" \
  R"(\\$]*|\\*|\\".*\\")))*)"

}  // namespace xpl

#endif  // PLUGIN_X_SRC_ADMIN_CMD_HANDLER_H_
