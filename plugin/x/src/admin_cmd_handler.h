/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#include <initializer_list>
#include <map>
#include <string>
#include <vector>

#include "my_compiler.h"  // NOLINT(build/include_subdir)
MY_COMPILER_DIAGNOSTIC_PUSH()
// Suppress warning C4251 'type' : class 'type1' needs to have dll-interface
// to be used by clients of class 'type2'
MY_COMPILER_MSVC_DIAGNOSTIC_IGNORE(4251)
#include <google/protobuf/repeated_field.h>  // NOLINT(build/include_order)
MY_COMPILER_DIAGNOSTIC_POP()

#include "plugin/x/src/admin_cmd_collection_handler.h"
#include "plugin/x/src/interface/admin_command_arguments.h"
#include "plugin/x/src/interface/sql_session.h"
#include "plugin/x/src/ngs/error_code.h"

namespace xpl {

class Admin_command_handler {
 public:
  using Command_arguments = iface::Admin_command_arguments;

  explicit Admin_command_handler(iface::Session *session);

  ngs::Error_code execute(const std::string &command, Command_arguments *args);

  static const char *const k_mysqlx_namespace;

 protected:
  using Argument_list = Command_arguments::Argument_list;
  using Value_list = Command_arguments::List;
  using Argument_appearance = Command_arguments::Appearance_type;

  ngs::Error_code ping(Command_arguments *args);

  ngs::Error_code list_clients(Command_arguments *args);
  ngs::Error_code kill_client(Command_arguments *args);

  /**
    Create a new collection.

    Create a collection with a given name in the specified schema. If collection
    already exists an error is returned. 'create_collection' command supports
    additional options. Validation option consists of two fields - schema and
    level. Validation level field is optional and can be used to change the
    state of validation operations. Validation schema field is required (if
    validation option is supplied) and can be used to create constraint on
    a given validation schema for newly created collection. Update
    Mysqlx_stmt_create_collection variable on each call.

    @param[in] args        list of arguments for create_collection command.
                           Allowed fields are:
                            - name (string, required) - name of created
                              collection.
                            - schema (string, required) - name of collection's
                              schema.
                            - options (object, optional) - additional options
                              that will be used for collection. Possible fields:
                              - validation (object, optional) - validation
                                details. Possible fields:
                                - schema (object, required) - validation schema
                                  that may be used when operating on a
                                  collection.
                                - level (string, optional) - enables or disables
                                  validation, default value is "strict".

    @return Result of creating a collection
      @retval == ER_TABLE_EXISTS_ERROR - specified collection already exists.
      @retval == ER_X_CMD_NUM_ARGUMENTS - required argument is missing.
      @retval == ER_X_CMD_INVALID_ARGUMENT - unknown argument provided.
      @retval == ER_X_BAD_SCHEMA - invalid schema name.
      @retval == ER_X_BAD_TABLE - invalid table name.
      @retval == ER_X_CMD_ARGUMENT_VALUE - invalid validation level value.
      @retval == ER_X_COLLECTION_OPTION_DOESNT_EXISTS - invalid validation
                 option field.
  */
  ngs::Error_code create_collection(Command_arguments *args);
  /**
    Drop existing collection.

    Drop a collection with a given name in the specified schema. If collection
    do not exist an error is returned. Update Mysqlx_stmt_drop_collection
    variable on each call.

    @param[in] args        list of arguments for drop_collection command.
                           Allowed fields are:
                            - name (string, required) - name of dropped
                              collection.
                            - schema (string, required) - name of collection's
                              schema.

    @return Result of dropping a collection
      @retval == ER_X_BAD_TABLE_ERROR - collection do not exist.
      @retval == ER_X_CMD_NUM_ARGUMENTS - required argument is missing.
      @retval == ER_X_CMD_INVALID_ARGUMENT - unknown argument provided.
      @retval == ER_X_BAD_SCHEMA - invalid schema name.
      @retval == ER_X_BAD_TABLE - invalid table name.
  */
  ngs::Error_code drop_collection(Command_arguments *args);
  /**
    Ensure that a collection exists.

    Ensure that a collection with a given name in the specified schema exists.
    If not then create a new collection. 'ensure_collection' command supports
    additional options. Validation option consists of two fields - schema and
    level. Validation level field is optional and can be used to change the
    state of validation operations. Validation schema field is required (if
    validation option is supplied) and can be used to create constraint on
    a given validation schema for newly created collection. Update
    Mysqlx_stmt_ensure_collection variable on each call.

    @param[in] args        list of arguments for ensure_collection command.
                           Allowed fields are:
                            - name (string, required) - name of created
                              collection.
                            - schema (string, required) - name of collection's
                              schema.
                            - options (object, optional) - additional options
                              that will be used for collection. Possible fields:
                              - validation (object, optional) - validation
                                details. Possible fields:
                                - schema (object, required) - validation schema
                                  that may be used when operating on a
                                  collection.
                                - level (string, optional) - enables or disables
                                  validation, default value is "strict".

    @return Result of creating a collection
      @retval == ER_X_CMD_NUM_ARGUMENTS - required argument is missing.
      @retval == ER_X_CMD_INVALID_ARGUMENT - unknown argument provided.
      @retval == ER_X_BAD_SCHEMA - invalid schema name.
      @retval == ER_X_BAD_TABLE - invalid table name.
      @retval == ER_X_CMD_ARGUMENT_VALUE - invalid validation level value.
      @retval == ER_X_COLLECTION_OPTION_DOESNT_EXISTS - invalid validation
                 option field.
  */
  ngs::Error_code ensure_collection(Command_arguments *args);
  /**
    Modify options for a specified collection.

    Command used to modify collection options. It can be used to modify
    validation schema of a collection or enable/disable validation. Update
    Mysqlx_stmt_modify_collection_options variable on each call.

    @param[in] args        list of arguments for modify_collection_options
                           command. Allowed fields are:
                            - name (string, required) - name of collection.
                            - schema (string, required) - name of collection's
                              schema.
                            - options (object, optional) - additional options
                              that will be used for collection. Possible fields:
                              - validation (object, optional) - validation
                                details. Possible fields:
                                - schema (object, optional) - validation schema
                                  that may be used when operating on a
                                  collection.
                                - level (string, optional) - state of validation
                                  operations.

    @return Result of modifying a collection options
      @retval == ER_X_INVALID_NAMESPACE - statement used in an unsupported
                 namespace.
      @retval == ER_X_CMD_NUM_ARGUMENTS - required argument is missing.
      @retval == ER_X_CMD_INVALID_ARGUMENT - unknown argument provided.
      @retval == ER_X_BAD_SCHEMA - invalid schema name.
      @retval == ER_X_BAD_TABLE - invalid table name.
      @retval == ER_X_CMD_ARGUMENT_VALUE - invalid validation field value.
      @retval == ER_X_COLLECTION_OPTION_DOESNT_EXISTS - invalid validation
                 option field.
      @retval == ER_X_DOCUMENT_DOESNT_MATCH_EXPECTED_SCHEMA - existing
                 collection data violates validation schema constraints.
  */
  ngs::Error_code modify_collection_options(Command_arguments *args);
  /**
    Get options for a specified collection.

    Return a JSON containing requested collection options. Update
    Mysqlx_stmt_get_collection_options variable on each call.

    @param[in] args        list of arguments for get_collection_options command.
                           Allowed fields are:
                            - name (string, required) - name of collection.
                            - schema (string, required) - name of collection's
                              schema.
                            - options (array, required) - options that will be
                              fetched from a collection.

    @return Result of fetching a collection options
      @retval == ER_X_INVALID_NAMESPACE - statement used in an unsupported
                 namespace.
      @retval == ER_X_CMD_NUM_ARGUMENTS - required argument is missing.
      @retval == ER_X_CMD_INVALID_ARGUMENT - unknown argument provided.
      @retval == ER_X_BAD_SCHEMA - invalid schema name.
      @retval == ER_X_BAD_TABLE - invalid table name.
      @retval == ER_TABLEACCESS_DENIED_ERROR - user has no access to collection.
      @retval == ER_NO_SUCH_TABLE - collection do not exist.
      @retval == ER_X_COLLECTION_OPTION_DOESNT_EXISTS - invalid collection
                 option field.
  */
  ngs::Error_code get_collection_options(Command_arguments *args);

  ngs::Error_code create_collection_index(Command_arguments *args);
  ngs::Error_code drop_collection_index(Command_arguments *args);

  ngs::Error_code list_objects(Command_arguments *args);

  ngs::Error_code enable_notices(Command_arguments *args);
  ngs::Error_code disable_notices(Command_arguments *args);
  ngs::Error_code list_notices(Command_arguments *args);

  using Method_ptr =
      ngs::Error_code (Admin_command_handler::*)(Command_arguments *args);
  static const struct Command_handler
      : private std::map<std::string, Method_ptr> {
    Command_handler();
    ngs::Error_code execute(Admin_command_handler *admin,
                            const std::string &command,
                            Command_arguments *args) const;
  } m_command_handler;

  iface::Session *m_session;
  Admin_command_collection_handler m_collection_handler;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_ADMIN_CMD_HANDLER_H_
