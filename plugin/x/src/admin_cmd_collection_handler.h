/*
 * Copyright (c) 2019, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_ADMIN_CMD_COLLECTION_HANDLER_H_
#define PLUGIN_X_SRC_ADMIN_CMD_COLLECTION_HANDLER_H_

#include <map>
#include <string>

#include "plugin/x/src/admin_cmd_arguments.h"
#include "plugin/x/src/interface/admin_command_arguments.h"
#include "plugin/x/src/interface/sql_session.h"
#include "plugin/x/src/ngs/error_code.h"

namespace xpl {

class Admin_command_collection_handler {
 public:
  using Command_arguments = iface::Admin_command_arguments;

  Admin_command_collection_handler(iface::Session *session,
                                   const char *const mysqlx_namespace);

  ngs::Error_code create_collection(Command_arguments *args);
  ngs::Error_code drop_collection(Command_arguments *args);
  ngs::Error_code ensure_collection(Command_arguments *args);
  ngs::Error_code modify_collection_options(Command_arguments *args);
  ngs::Error_code get_collection_options(Command_arguments *args);

 private:
  using Argument_appearance = Command_arguments::Appearance_type;

  bool is_collection(const std::string &schema, const std::string &name);

  ngs::Error_code get_validation_info(
      const Command_arguments::Object &validation,
      Command_arguments::Any *validation_schema, bool *enforce) const;

  std::string generate_constraint_name(const std::string &coll_name) const;

  ngs::Error_code check_if_collection_exists_and_is_accessible(
      const std::string &schema, const std::string &collection);

  ngs::Error_code create_collection_impl(
      iface::Sql_session *da, const std::string &schema,
      const std::string &name,
      const Command_arguments::Object &validation) const;

  ngs::Error_code check_schema(const Command_arguments::Any &validation_schema,
                               std::string *schema) const;

  ngs::Error_code modify_collection_validation(
      const std::string &schema, const std::string &collection,
      const Command_arguments::Object &validation);

  class Collection_option_handler {
   public:
    using Collection_option_method_ptr = ngs::Error_code (
        Collection_option_handler::*)(const std::string &schema,
                                      const std::string &collection);

   public:
    Collection_option_handler(
        Admin_command_collection_handler *cmd_collection_handler,
        iface::Session *session);

    ngs::Error_code dispatch(const std::string &option,
                             const std::string &schema,
                             const std::string &collection);

    bool contains_handler(const std::string &option) const {
      return m_dispatcher.count(option) != 0;
    }

   private:
    ngs::Error_code get_validation_option(const std::string &schema,
                                          const std::string &collection);

    std::string create_validation_json(std::string validation_schema_raw,
                                       std::string validation_level);
    void send_validation_option_json(const std::string &validation_json);

   private:
    Admin_command_collection_handler *m_cmd_collection_handler;
    iface::Session *m_session;
    const std::map<std::string, Collection_option_method_ptr> m_dispatcher;
  };

 private:
  iface::Session *m_session;
  const char *const k_mysqlx_namespace;
  Collection_option_handler m_collection_option_handler{this, m_session};
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_ADMIN_CMD_COLLECTION_HANDLER_H_
