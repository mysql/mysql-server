/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <mysql.h>
#include <mysql/components/component_implementation.h>
#include <mysql/components/my_service.h>
#include <mysql/components/services/mysql_command_services.h>
#include <mysql/components/services/security_context.h>
#include <mysql/components/services/udf_registration.h>
#include <mysql/service_srv_session_info.h>
#include <stdio.h>
#include <string>

REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_thd_security_context, thd_security_ctx);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_account_database_security_context_lookup,
                                account_db_security_ctx_lookup);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_security_context_options,
                                security_ctx_options);
REQUIRES_SERVICE_PLACEHOLDER_AS(udf_registration, udf_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_factory, cmd_factory_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_options, cmd_options_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_query, cmd_query_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_query_result,
                                cmd_query_result_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_field_info, cmd_field_info_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_error_info, cmd_error_info_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_field_metadata,
                                cmd_field_meta_srv);

BEGIN_COMPONENT_PROVIDES(test_mysql_command_services)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(test_mysql_command_services)
REQUIRES_SERVICE_AS(udf_registration, udf_srv),
    REQUIRES_SERVICE_AS(mysql_thd_security_context, thd_security_ctx),
    REQUIRES_SERVICE_AS(mysql_account_database_security_context_lookup,
                        account_db_security_ctx_lookup),
    REQUIRES_SERVICE_AS(mysql_security_context_options, security_ctx_options),
    REQUIRES_SERVICE_AS(mysql_command_factory, cmd_factory_srv),
    REQUIRES_SERVICE_AS(mysql_command_options, cmd_options_srv),
    REQUIRES_SERVICE_AS(mysql_command_query, cmd_query_srv),
    REQUIRES_SERVICE_AS(mysql_command_query_result, cmd_query_result_srv),
    REQUIRES_SERVICE_AS(mysql_command_field_info, cmd_field_info_srv),
    REQUIRES_SERVICE_AS(mysql_command_error_info, cmd_error_info_srv),
    REQUIRES_SERVICE_AS(mysql_command_field_metadata, cmd_field_meta_srv),
    END_COMPONENT_REQUIRES();

MYSQL_H mysql_h = nullptr;
MYSQL_LEX_CSTRING user;
MYSQL_LEX_CSTRING host;

static char *test_mysql_command_services_udf(UDF_INIT *, UDF_ARGS *args,
                                             char *result,
                                             unsigned long *length,
                                             unsigned char *,
                                             unsigned char *error) {
  *error = 1;
  if (args->arg_count == 0) {
    return nullptr;
  }

  MYSQL_RES_H mysql_res = nullptr;
  MYSQL_ROW_H row = nullptr;
  MYSQL_FIELD_H *fields_h = nullptr;
  MYSQL_FIELD_H field_h = nullptr;
  unsigned int field_count;
  uint64_t row_count = 0;
  unsigned int num_column = 0;
  std::string result_set;
  unsigned int err_no;
  char *sqlstate_errmsg[50];

  //  Execute the SQL specified in the argument.
  if (cmd_factory_srv->init(&mysql_h)) {
    return nullptr;
  }
  if (mysql_h) {
    if (cmd_factory_srv->connect(mysql_h)) {
      return nullptr;
    }
  } else {
    return nullptr;
  }

  std::string query(args->args[0], args->lengths[0]);

  if (cmd_query_srv->query(mysql_h, query.data(), query.length())) {
    cmd_error_info_srv->sql_error(mysql_h, &result);
    *length = strlen(result);
    goto err;
  }

  cmd_query_result_srv->store_result(mysql_h, &mysql_res);
  if (mysql_res) {
    if (cmd_query_srv->affected_rows(mysql_h, &row_count)) {
      result = nullptr;
      goto err;
    }
    if (cmd_field_info_srv->num_fields(mysql_res, &num_column)) {
      result = nullptr;
      goto err;
    }
    if (cmd_field_info_srv->field_count(mysql_h, &field_count)) {
      result = nullptr;
      goto err;
    }

    if (field_count > 0) {
      if (cmd_field_info_srv->fetch_field(mysql_res, &field_h)) {
        result = nullptr;
        goto err;
      }
      if (cmd_field_info_srv->fetch_fields(mysql_res, &fields_h)) {
        result = nullptr;
        goto err;
      }

      const char *field_name = nullptr, *table_name = nullptr,
                 *db_name = nullptr;
      if (cmd_field_meta_srv->get(field_h, MYSQL_COMMAND_FIELD_METADATA_NAME,
                                  &field_name) ||
          !field_name) {
        result = nullptr;
        goto err;
      }
      if (cmd_field_meta_srv->get(
              field_h, MYSQL_COMMAND_FIELD_METADATA_TABLE_NAME, &table_name)) {
        result = nullptr;
        goto err;
      }
      if (cmd_field_meta_srv->get(
              field_h, MYSQL_COMMAND_FIELD_METADATA_TABLE_DB_NAME, &db_name)) {
        result = nullptr;
        goto err;
      }
    }

    for (uint64_t i = 0; i < row_count; i++) {
      if (cmd_query_result_srv->fetch_row(mysql_res, &row)) {
        result = nullptr;
        goto err;
      }
      ulong *length = nullptr;
      if (cmd_query_result_srv->fetch_lengths(mysql_res, &length)) {
        result = nullptr;
        goto err;
      }
      for (unsigned int j = 0; j < num_column; j++) {
        result_set += row[j];
      }
    }
    /* The caller has the buffer limit, and the size is of MAX_FIELD_WIDTH size
       so we are truncating the result of the query output if it has more date
    */
    strncpy(
        result,
        reinterpret_cast<char *>(const_cast<char *>(result_set.c_str())),
        (result_set.length() < *length) ? result_set.length() : (*length - 1));
    *length =
        (result_set.length() < *length) ? result_set.length() : (*length - 1);
    result[*length] = '\0';
  } else {
    cmd_error_info_srv->sql_error(mysql_h, &result);
    cmd_error_info_srv->sql_errno(mysql_h, &err_no);
    cmd_error_info_srv->sql_state(mysql_h, sqlstate_errmsg);
    *length = strlen(result);
  }
err:
  *error = 0;
  cmd_query_result_srv->free_result(mysql_res);
  cmd_factory_srv->close(mysql_h);
  return result;
}

static char *test_mysql_command_services_apis_udf(UDF_INIT *, UDF_ARGS *args,
                                                  char *result,
                                                  unsigned long *length,
                                                  unsigned char *,
                                                  unsigned char *error) {
  *error = 1;
  if (args->arg_count > 0) {
    return nullptr;
  }
  MYSQL_RES_H mysql_res = nullptr;
  MYSQL_ROW_H row = nullptr;
  uint64_t row_count = 0;
  unsigned int num_column = 0;
  std::string result_set;

  //  Execute the SQL specified in the argument.
  if (cmd_factory_srv->init(&mysql_h)) {
    return nullptr;
  }
  if (mysql_h) {
    if (cmd_factory_srv->connect(mysql_h)) {
      return nullptr;
    }
  } else {
    return nullptr;
  }

  if (cmd_factory_srv->reset(mysql_h)) {
    goto err;
  }

  /* set AUTOCOMMIT to OFF */
  if (cmd_factory_srv->autocommit(mysql_h, false)) {
    goto err;
  }

  {
    std::string query("DROP TABLE IF EXISTS test.my_demo_transaction");

    if (cmd_query_srv->query(mysql_h, query.data(), query.length())) {
      *length = strlen(result);
      goto err;
    }
  }

  /* To get the mysql option value */
  void *option_val;
  cmd_options_srv->get(mysql_h, MYSQL_OPT_MAX_ALLOWED_PACKET, &option_val);

  {
    std::string query(
        "CREATE TABLE test.my_demo_transaction( "
        "col1 int , col2 varchar(30))");

    if (cmd_query_srv->query(mysql_h, query.data(), query.length())) {
      goto err;
    }
  }

  {
    std::string query(
        "INSERT INTO test.my_demo_transaction VALUES(10, 'mysql-1')");

    if (cmd_query_srv->query(mysql_h, query.data(), query.length())) {
      goto err;
    }
  }

  /* Commiting the transaction */
  if (cmd_factory_srv->commit(mysql_h)) {
    goto err;
  }

  /* now insert the second row, and roll back the transaction */
  {
    std::string query(
        "INSERT INTO test.my_demo_transaction VALUES(20, 'mysql-2')");

    if (cmd_query_srv->query(mysql_h, query.data(), query.length())) {
      goto err;
    }
  }

  /* Commiting the transaction */
  if (cmd_factory_srv->rollback(mysql_h)) {
    goto err;
  }

  {
    std::string query("SELECT * from  test.my_demo_transaction");

    if (cmd_query_srv->query(mysql_h, query.data(), query.length())) {
      goto err;
    }
  }

  cmd_query_result_srv->store_result(mysql_h, &mysql_res);
  if (mysql_res) {
    if (cmd_query_srv->affected_rows(mysql_h, &row_count)) {
      result = nullptr;
      goto err;
    }
    if (cmd_field_info_srv->num_fields(mysql_res, &num_column)) {
      result = nullptr;
      goto err;
    }

    for (uint64_t i = 0; i < row_count; i++) {
      if (cmd_query_result_srv->fetch_row(mysql_res, &row)) {
        result = nullptr;
        goto err;
      }
      ulong *length = nullptr;
      cmd_query_result_srv->fetch_lengths(mysql_res, &length);
      for (unsigned int j = 0; j < num_column; j++) {
        result_set += row[j];
      }
    }
    cmd_query_result_srv->more_results(mysql_h);
    cmd_query_result_srv->next_result(mysql_h);
    cmd_query_result_srv->result_metadata(mysql_res);
    /* The caller has the buffer limit, and the size is of MAX_FIELD_WIDTH size
       so we are truncating the result of the query output if it has more date
    */
    strncpy(
        result,
        reinterpret_cast<char *>(const_cast<char *>(result_set.c_str())),
        (result_set.length() < *length) ? result_set.length() : (*length - 1));
    *length =
        (result_set.length() < *length) ? result_set.length() : (*length - 1);
    result[*length] = '\0';
  }
  *error = 0;
err:
  cmd_query_result_srv->free_result(mysql_res);
  cmd_factory_srv->close(mysql_h);
  return result;
}

static mysql_service_status_t init() {
  Udf_func_string udf1 = test_mysql_command_services_udf;
  if (udf_srv->udf_register("test_mysql_command_services_udf", STRING_RESULT,
                            reinterpret_cast<Udf_func_any>(udf1), nullptr,
                            nullptr)) {
    fprintf(stderr, "Can't register the test_mysql_command_services_udf UDF\n");
    return 1;
  }
  Udf_func_string udf2 = test_mysql_command_services_apis_udf;
  if (udf_srv->udf_register("test_mysql_command_services_apis_udf",
                            STRING_RESULT, reinterpret_cast<Udf_func_any>(udf2),
                            nullptr, nullptr)) {
    fprintf(stderr,
            "Can't register the test_mysql_command_services_apis_udf UDF\n");
    return 1;
  }
  return 0;
}

static mysql_service_status_t deinit() {
  int was_present = 0;
  if (udf_srv->udf_unregister("test_mysql_command_services_udf", &was_present))
    fprintf(stderr,
            "Can't unregister the test_mysql_command_services_udf UDF\n");
  if (udf_srv->udf_unregister("test_mysql_command_services_apis_udf",
                              &was_present))
    fprintf(stderr,
            "Can't unregister the test_mysql_command_services_apis_udf UDF\n");
  return 0; /* success */
}

BEGIN_COMPONENT_METADATA(test_mysql_command_services)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_property", "1"),
    END_COMPONENT_METADATA();

DECLARE_COMPONENT(test_mysql_command_services,
                  "mysql:test_mysql_command_services")
init, deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_mysql_command_services)
    END_DECLARE_LIBRARY_COMPONENTS
