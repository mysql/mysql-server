/* Copyright (c) 2022, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

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
#include <mysql/components/services/bits/mysql_thd_attributes_bits.h>
#include <mysql/components/services/mysql_command_consumer.h>
#include <mysql/components/services/mysql_command_services.h>
#include <mysql/components/services/mysql_command_session_state.h>
#include <mysql/components/services/mysql_thd_attributes.h>
#include <mysql/components/services/udf_registration.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "scope_guard.h"

REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_thd_attributes, thd_attributes_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(udf_registration, udf_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_factory, cmd_factory_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_thread, cmd_thread_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_options, cmd_options_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_query, cmd_query_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_session_state,
                                cmd_session_state_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_query_result,
                                cmd_query_result_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_field_info, cmd_field_info_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_error_info, cmd_error_info_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_command_field_metadata,
                                cmd_field_meta_srv);

namespace session_state_client_capabilities_imp {

DEFINE_METHOD(void, client_capabilities,
              (SRV_CTX_H srv_ctx_h [[maybe_unused]],
               unsigned long *capabilities)) {
  if (capabilities == nullptr) return;

  *capabilities = CLIENT_PROTOCOL_41 | CLIENT_SESSION_TRACK |
                  CLIENT_DEPRECATE_EOF | CLIENT_MULTI_RESULTS;
}

}  // namespace session_state_client_capabilities_imp

BEGIN_SERVICE_IMPLEMENTATION(test_mysql_command_services,
                             mysql_text_consumer_client_capabilities_v1)
session_state_client_capabilities_imp::client_capabilities
END_SERVICE_IMPLEMENTATION();

BEGIN_COMPONENT_PROVIDES(test_mysql_command_services)
PROVIDES_SERVICE(test_mysql_command_services,
                 mysql_text_consumer_client_capabilities_v1),
    END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(test_mysql_command_services)
REQUIRES_SERVICE_AS(udf_registration, udf_srv),
    REQUIRES_SERVICE_AS(mysql_thd_attributes, thd_attributes_srv),
    REQUIRES_SERVICE_AS(mysql_command_factory, cmd_factory_srv),
    REQUIRES_SERVICE_AS(mysql_command_thread, cmd_thread_srv),
    REQUIRES_SERVICE_AS(mysql_command_options, cmd_options_srv),
    REQUIRES_SERVICE_AS(mysql_command_query, cmd_query_srv),
    REQUIRES_SERVICE_AS(mysql_command_session_state, cmd_session_state_srv),
    REQUIRES_SERVICE_AS(mysql_command_query_result, cmd_query_result_srv),
    REQUIRES_SERVICE_AS(mysql_command_field_info, cmd_field_info_srv),
    REQUIRES_SERVICE_AS(mysql_command_error_info, cmd_error_info_srv),
    REQUIRES_SERVICE_AS(mysql_command_field_metadata, cmd_field_meta_srv),
    END_COMPONENT_REQUIRES();

MYSQL_H mysql_h = nullptr;
static bool get_session_track_type_arg(
    UDF_ARGS *args, unsigned int arg_index,
    mysql_command_session_state_type *tracker_type) {
  static const std::map<std::string, mysql_command_session_state_type>
      session_track_type_names{
          {"SESSION_TRACK_SYSTEM_VARIABLES",
           MYSQL_COMMAND_SESSION_TRACK_SYSTEM_VARIABLES},
          {"SESSION_TRACK_SCHEMA", MYSQL_COMMAND_SESSION_TRACK_SCHEMA},
          {"SESSION_TRACK_STATE_CHANGE",
           MYSQL_COMMAND_SESSION_TRACK_STATE_CHANGE},
          {"SESSION_TRACK_GTIDS", MYSQL_COMMAND_SESSION_TRACK_GTIDS},
          {"SESSION_TRACK_TRANSACTION_CHARACTERISTICS",
           MYSQL_COMMAND_SESSION_TRACK_TRANSACTION_CHARACTERISTICS},
          {"SESSION_TRACK_TRANSACTION_STATE",
           MYSQL_COMMAND_SESSION_TRACK_TRANSACTION_STATE},
      };

  if (args == nullptr || args->args[arg_index] == nullptr ||
      tracker_type == nullptr) {
    return true;
  }

  const std::string name(args->args[arg_index], args->lengths[arg_index]);
  const auto it = session_track_type_names.find(name);
  if (it == session_track_type_names.end()) return true;

  *tracker_type = it->second;
  return false;
}

static bool execute_query_and_discard_result(MYSQL_H mysql_h,
                                             const std::string &query,
                                             char **errmsg,
                                             unsigned long *errmsg_length) {
  MYSQL_RES_H mysql_res = nullptr;

  if (cmd_query_srv->query(mysql_h, query.data(), query.length())) {
    cmd_error_info_srv->sql_error(mysql_h, errmsg);
    *errmsg_length = (*errmsg != nullptr) ? strlen(*errmsg) : 0;
    return true;
  }

  bool done = false;
  while (!done) {
    unsigned int field_count = 0;

    if (cmd_field_info_srv->field_count(mysql_h, &field_count)) {
      cmd_error_info_srv->sql_error(mysql_h, errmsg);
      *errmsg_length = (*errmsg != nullptr) ? strlen(*errmsg) : 0;
      return true;
    }

    if (field_count > 0) {
      if (cmd_query_result_srv->store_result(mysql_h, &mysql_res)) {
        cmd_error_info_srv->sql_error(mysql_h, errmsg);
        *errmsg_length = (*errmsg != nullptr) ? strlen(*errmsg) : 0;
        return true;
      }

      cmd_query_result_srv->free_result(mysql_res);
      mysql_res = nullptr;
    }

    done = cmd_query_result_srv->more_results(mysql_h);
    if (done) return false;

    if (cmd_query_result_srv->next_result(mysql_h) != 0) {
      cmd_error_info_srv->sql_error(mysql_h, errmsg);
      *errmsg_length = (*errmsg != nullptr) ? strlen(*errmsg) : 0;
      return true;
    }
  }

  return false;
}

static void store_string_result(char *result, unsigned long *length,
                                const std::string &value) {
  const size_t max_copy_length =
      (*length == 0)
          ? 0
          : std::min(value.length(), static_cast<size_t>(*length - 1));

  if (max_copy_length > 0) {
    strncpy(result, value.c_str(), max_copy_length);
  }
  *length = static_cast<unsigned long>(max_copy_length);
  result[*length] = '\0';
}

static void execute_query_and_ignore_failure(MYSQL_H mysql_h,
                                             const std::string &query) {
  MYSQL_RES_H mysql_res = nullptr;
  auto reset_da_if_needed = [] {
    uint16_t da_status = STATUS_DA_EMPTY;
    thd_attributes_srv->set(nullptr, "da_status", &da_status);
  };

  if (cmd_query_srv->query(mysql_h, query.data(), query.length())) {
    reset_da_if_needed();
    return;
  }

  bool done = false;
  while (!done) {
    unsigned int field_count = 0;

    if (cmd_field_info_srv->field_count(mysql_h, &field_count)) {
      reset_da_if_needed();
      return;
    }

    if (field_count > 0) {
      if (cmd_query_result_srv->store_result(mysql_h, &mysql_res)) {
        reset_da_if_needed();
        return;
      }

      cmd_query_result_srv->free_result(mysql_res);
      mysql_res = nullptr;
    }

    done = cmd_query_result_srv->more_results(mysql_h);
    if (done) return;

    if (cmd_query_result_srv->next_result(mysql_h) != 0) {
      reset_da_if_needed();
      return;
    }
  }
}

static bool set_session_track_capable_consumer(MYSQL_H mysql_h) {
  return cmd_options_srv->set(
      mysql_h, MYSQL_TEXT_CONSUMER_CLIENT_CAPABILITIES,
      "mysql_text_consumer_client_capabilities_v1.test_mysql_command_services");
}

static bool connect_session_state_test_handle(
    MYSQL_H *session_mysql_h, bool session_track_capable_consumer,
    bool session_track_client_flag = false) {
  if (cmd_factory_srv->init(session_mysql_h) || *session_mysql_h == nullptr) {
    return true;
  }
  if (session_track_client_flag) {
    const uint32_t client_flags = CLIENT_SESSION_TRACK;
    if (cmd_options_srv->set(*session_mysql_h, MYSQL_COMMAND_CLIENT_FLAGS,
                             &client_flags)) {
      cmd_factory_srv->close(*session_mysql_h);
      *session_mysql_h = nullptr;
      return true;
    }
  }
  if (cmd_factory_srv->connect(*session_mysql_h)) {
    cmd_factory_srv->close(*session_mysql_h);
    *session_mysql_h = nullptr;
    return true;
  }
  if (session_track_capable_consumer &&
      set_session_track_capable_consumer(*session_mysql_h)) {
    cmd_factory_srv->close(*session_mysql_h);
    *session_mysql_h = nullptr;
    return true;
  }

  return false;
}

static void collect_session_state(MYSQL_H mysql_h,
                                  mysql_command_session_state_type tracker_type,
                                  std::string *session_state) {
  MYSQL_COMMAND_SESSION_STATE_ITERATOR_H iterator = nullptr;

  session_state->clear();

  if (cmd_session_state_srv->init(mysql_h, tracker_type, &iterator) !=
      MYSQL_COMMAND_SESSION_STATE_OK) {
    return;
  }

  while (true) {
    size_t data_length = 0;
    mysql_command_session_state_status status =
        cmd_session_state_srv->get_next(iterator, nullptr, 0, &data_length);
    if (status == MYSQL_COMMAND_SESSION_STATE_END) break;
    if (status != MYSQL_COMMAND_SESSION_STATE_BUFFER_TOO_SMALL) break;

    std::vector<char> data(data_length == 0 ? 1 : data_length);
    status = cmd_session_state_srv->get_next(iterator, data.data(), data.size(),
                                             &data_length);
    if (status != MYSQL_COMMAND_SESSION_STATE_OK) break;

    if (!session_state->empty()) *session_state += "|";
    session_state->append(data.data(), data_length);
  }

  cmd_session_state_srv->deinit(iterator);
}

static char *run_session_state_query_udf(
    UDF_ARGS *args, char *result, unsigned long *length, unsigned char *error,
    bool session_track_capable_consumer,
    bool session_track_client_flag = false) {
  *error = 1;
  *result = '\0';

  if ((args->arg_count != 2U && args->arg_count != 3U) ||
      args->arg_type[args->arg_count - 1U] != STRING_RESULT ||
      args->arg_type[args->arg_count - 2U] != STRING_RESULT ||
      (args->arg_count == 3U && args->arg_type[0] != STRING_RESULT)) {
    return nullptr;
  }

  MYSQL_H session_mysql_h = nullptr;
  char *output = result;
  std::string setup_query;
  std::string query;
  std::string session_state;

  mysql_command_session_state_type tracker_type;
  if (get_session_track_type_arg(args, args->arg_count - 1U, &tracker_type)) {
    return nullptr;
  }

  if (args->arg_count == 3U) {
    setup_query.assign(args->args[0], args->lengths[0]);
    query.assign(args->args[1], args->lengths[1]);
  } else {
    query.assign(args->args[0], args->lengths[0]);
  }

  if (connect_session_state_test_handle(&session_mysql_h,
                                        session_track_capable_consumer,
                                        session_track_client_flag)) {
    return nullptr;
  }

  if (!setup_query.empty() &&
      execute_query_and_discard_result(session_mysql_h, setup_query, &output,
                                       length)) {
    goto err;
  }

  if (execute_query_and_discard_result(session_mysql_h, query, &output,
                                       length)) {
    goto err;
  }

  collect_session_state(session_mysql_h, tracker_type, &session_state);
  store_string_result(result, length, session_state);
  output = result;

err:
  *error = 0;
  cmd_factory_srv->close(session_mysql_h);
  return output;
}

static void append_row_field(std::string &result_set, MYSQL_ROW_H row,
                             const ulong *lengths, unsigned int column) {
  if (row[column] == nullptr) {
    result_set += "NULL";
    return;
  }
  result_set.append(row[column], lengths[column]);
}

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

  /* reset to empty as a start */
  *result = 0;

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
  std::size_t number_of_query_executions{1U};
  if (args->arg_count > 1U && args->arg_type[1] == INT_RESULT) {
    number_of_query_executions = *reinterpret_cast<long long *>(args->args[1]);
  }

  for (std::size_t u{0U}; u < number_of_query_executions; ++u) {
    result_set.clear();
    // It is OK to call free_result() with nullptr MYSQL_RES_H.
    cmd_query_result_srv->free_result(mysql_res);
    mysql_res = nullptr;

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
        if (cmd_field_meta_srv->get(field_h,
                                    MYSQL_COMMAND_FIELD_METADATA_TABLE_NAME,
                                    &table_name)) {
          result = nullptr;
          goto err;
        }
        if (cmd_field_meta_srv->get(field_h,
                                    MYSQL_COMMAND_FIELD_METADATA_TABLE_DB_NAME,
                                    &db_name)) {
          result = nullptr;
          goto err;
        }
      }

      for (uint64_t i = 0; i < row_count; i++) {
        if (cmd_query_result_srv->fetch_row(mysql_res, &row)) {
          result = nullptr;
          goto err;
        }
        ulong *field_lengths = nullptr;
        if (cmd_query_result_srv->fetch_lengths(mysql_res, &field_lengths)) {
          result = nullptr;
          goto err;
        }
        for (unsigned int j = 0; j < num_column; j++) {
          append_row_field(result_set, row, field_lengths, j);
        }
      }
      /* The caller has the buffer limit, and the size is of MAX_FIELD_WIDTH
        size so we are truncating the result of the query output if it has more
        date
      */
      if (u == 0U) {
        /* Make sure we return results from the very first execution */
        strncpy(
            result,
            reinterpret_cast<char *>(const_cast<char *>(result_set.c_str())),
            (result_set.length() < *length) ? result_set.length()
                                            : (*length - 1));
        *length = (result_set.length() < *length) ? result_set.length()
                                                  : (*length - 1);
        result[*length] = '\0';
      }
    } else {
      if (u == 0U) {
        cmd_error_info_srv->sql_error(mysql_h, &result);
        cmd_error_info_srv->sql_errno(mysql_h, &err_no);
        cmd_error_info_srv->sql_state(mysql_h, sqlstate_errmsg);
        *length = strlen(result);
      }
    }
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
  {
    void *option_val = nullptr;
    cmd_options_srv->get(mysql_h, MYSQL_OPT_MAX_ALLOWED_PACKET, &option_val);
  }

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
      ulong *field_lengths = nullptr;
      if (cmd_query_result_srv->fetch_lengths(mysql_res, &field_lengths)) {
        result = nullptr;
        goto err;
      }
      for (unsigned int j = 0; j < num_column; j++) {
        append_row_field(result_set, row, field_lengths, j);
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

static long long test_mysql_command_services_error_code_udf(
    UDF_INIT *, UDF_ARGS *args, unsigned char *is_null, unsigned char *error) {
  *error = 1;
  if (args->arg_count < 1 || args->arg_type[0] != STRING_RESULT) return 0;

  std::string query(args->args[0], args->lengths[0]);
  MYSQL_H mysql_h = nullptr;
  unsigned int err_no = 0;

  //  Execute the SQL specified in the argument.
  if (cmd_factory_srv->init(&mysql_h)) {
    return 0;
  }
  if (mysql_h) {
    if (cmd_factory_srv->connect(mysql_h)) {
      return 0;
    }
  } else {
    return 0;
  }

  cmd_query_srv->query(mysql_h, query.data(), query.length());
  cmd_error_info_srv->sql_errno(mysql_h, &err_no);

  *error = 0;
  *is_null = 0;

  cmd_factory_srv->close(mysql_h);

  // Return the err_no or 0 in case of error
  return static_cast<long long>(err_no);
}

static char *test_mysql_command_services_row_semantics_udf(
    UDF_INIT *, UDF_ARGS *args, char *result, unsigned long *length,
    unsigned char *is_null, unsigned char *error) {
  *is_null = 0;
  *error = 1;
  if (args->arg_count != 0) {
    return nullptr;
  }

  MYSQL_H local_mysql_h = nullptr;
  MYSQL_RES_H mysql_res = nullptr;
  MYSQL_ROW_H row = nullptr;
  ulong *field_lengths = nullptr;
  const char query[] = "SELECT NULL, 'NULL', UNHEX('610062')";
  unsigned int num_columns = 0;
  const char binary_value[] = {'a', '\0', 'b'};
  char binary_length[32];
  bool string_null_matches = false;
  bool binary_value_matches = false;
  std::string result_set;

  auto cleanup = create_scope_guard([&mysql_res, &local_mysql_h] {
    cmd_query_result_srv->free_result(mysql_res);
    if (local_mysql_h != nullptr) cmd_factory_srv->close(local_mysql_h);
  });

  if (cmd_factory_srv->init(&local_mysql_h) || local_mysql_h == nullptr) {
    return nullptr;
  }
  if (cmd_factory_srv->connect(local_mysql_h)) {
    return nullptr;
  }

  if (cmd_query_srv->query(local_mysql_h, query, strlen(query))) {
    return nullptr;
  }

  if (cmd_query_result_srv->store_result(local_mysql_h, &mysql_res) ||
      mysql_res == nullptr) {
    return nullptr;
  }

  if (cmd_field_info_srv->num_fields(mysql_res, &num_columns) ||
      num_columns != 3) {
    return nullptr;
  }

  if (cmd_query_result_srv->fetch_row(mysql_res, &row) || row == nullptr) {
    return nullptr;
  }

  if (cmd_query_result_srv->fetch_lengths(mysql_res, &field_lengths) ||
      field_lengths == nullptr) {
    return nullptr;
  }

  snprintf(binary_length, sizeof(binary_length), "%lu", field_lengths[2]);
  string_null_matches = row[1] != nullptr && field_lengths[1] == 4 &&
                        memcmp(row[1], "NULL", 4) == 0;
  binary_value_matches =
      row[2] != nullptr && field_lengths[2] == sizeof(binary_value) &&
      memcmp(row[2], binary_value, sizeof(binary_value)) == 0;

  result_set = "null=";
  result_set += (row[0] == nullptr) ? "yes" : "no";
  result_set += ",string_NULL=";
  result_set += string_null_matches ? "yes" : "no";
  result_set += ",binary_length=";
  result_set += binary_length;
  result_set += ",binary_value=";
  result_set += binary_value_matches ? "yes" : "no";
  strncpy(
      result, result_set.c_str(),
      (result_set.length() < *length) ? result_set.length() : (*length - 1));
  *length =
      (result_set.length() < *length) ? result_set.length() : (*length - 1);
  result[*length] = '\0';
  *error = 0;
  return result;
}

static char *test_mysql_command_services_authenticate_udf(
    UDF_INIT *, UDF_ARGS *args, char *result, unsigned long *length,
    unsigned char *is_null, unsigned char *error) {
  *is_null = 0;
  *error = 1;

  if ((args->arg_count != 5 && args->arg_count != 6) ||
      args->arg_type[0] != STRING_RESULT ||
      args->arg_type[1] != STRING_RESULT ||
      args->arg_type[2] != STRING_RESULT || args->arg_type[3] != INT_RESULT ||
      args->arg_type[4] != STRING_RESULT || args->args[0] == nullptr ||
      args->args[1] == nullptr || args->args[2] == nullptr ||
      args->args[3] == nullptr ||
      (args->arg_count == 6 &&
       (args->arg_type[5] != STRING_RESULT || args->args[5] == nullptr))) {
    return nullptr;
  }

  MYSQL_H authenticated_mysql_h = nullptr;
  MYSQL_RES_H mysql_res = nullptr;
  const std::string user(args->args[0], args->lengths[0]);
  const std::string password(args->args[1], args->lengths[1]);
  const std::string host(args->args[2], args->lengths[2]);
  const long long port_arg = *reinterpret_cast<long long *>(args->args[3]);
  const std::string socket = args->args[4] == nullptr
                                 ? std::string()
                                 : std::string(args->args[4], args->lengths[4]);
  const std::string mode =
      args->arg_count == 6 ? std::string(args->args[5], args->lengths[5]) : "";

  auto cleanup = create_scope_guard([&mysql_res, &authenticated_mysql_h] {
    cmd_query_result_srv->free_result(mysql_res);
    if (authenticated_mysql_h != nullptr)
      cmd_factory_srv->close(authenticated_mysql_h);
  });

  const auto authenticate = [&]() -> std::string {
    if (port_arg < 0 ||
        port_arg > static_cast<long long>(std::numeric_limits<int>::max())) {
      return "SERVICE_ERROR";
    }
    const int port = static_cast<int>(port_arg);
    int connect_port = port;
    const char *connect_password = password.c_str();
    const bool retry_after_validation_failure = mode == "retry_validation";
    const bool retry_after_authentication_failure =
        mode == "retry_authentication";
    const bool same_handle_retry =
        retry_after_validation_failure || retry_after_authentication_failure;
    const bool authenticate_after_embedded_failure =
        mode == "after_embedded_failure";

    if (retry_after_validation_failure) {
      connect_port =
          port == std::numeric_limits<int>::max() ? port - 1 : port + 1;
    } else if (retry_after_authentication_failure) {
      connect_password = "wrong-password";
    }

    if (cmd_factory_srv->init(&authenticated_mysql_h) ||
        authenticated_mysql_h == nullptr) {
      return "SERVICE_ERROR";
    }

    if (authenticate_after_embedded_failure) {
      constexpr char missing_user[] = "mcs_auth_missing";
      if (cmd_options_srv->set(authenticated_mysql_h, MYSQL_COMMAND_USER_NAME,
                               missing_user)) {
        return "SERVICE_ERROR";
      }
      if (!cmd_factory_srv->connect(authenticated_mysql_h)) {
        return "EMBEDDED_CONNECT_SUCCEEDED";
      }
    } else if (mode == "local_thd") {
      if (cmd_options_srv->set(authenticated_mysql_h,
                               MYSQL_COMMAND_LOCAL_THD_HANDLE, nullptr)) {
        return "SERVICE_ERROR";
      }
    } else if (mode == "consumer") {
      if (cmd_options_srv->set(authenticated_mysql_h,
                               MYSQL_TEXT_CONSUMER_FACTORY, nullptr)) {
        return "SERVICE_ERROR";
      }
    } else if (mode == "no_lock") {
      const bool no_lock_registry = true;
      if (cmd_options_srv->set(authenticated_mysql_h, MYSQL_NO_LOCK_REGISTRY,
                               &no_lock_registry)) {
        return "SERVICE_ERROR";
      }
    } else if (!mode.empty() && !same_handle_retry &&
               !authenticate_after_embedded_failure) {
      return "SERVICE_ERROR";
    }

    if (cmd_options_srv->set(authenticated_mysql_h, MYSQL_COMMAND_USER_NAME,
                             user.c_str()) ||
        cmd_options_srv->set(authenticated_mysql_h, MYSQL_COMMAND_PASSWORD,
                             connect_password) ||
        cmd_options_srv->set(authenticated_mysql_h, MYSQL_COMMAND_HOST_NAME,
                             host.c_str()) ||
        cmd_options_srv->set(authenticated_mysql_h, MYSQL_COMMAND_TCPIP_PORT,
                             &connect_port) ||
        (!socket.empty() &&
         cmd_options_srv->set(authenticated_mysql_h, MYSQL_COMMAND_PROTOCOL,
                              socket.c_str()))) {
      return "SERVICE_ERROR";
    }

    const auto connection_error = [&]() -> std::string {
      unsigned int error_number = 0;
      if (cmd_error_info_srv->sql_errno(authenticated_mysql_h, &error_number)) {
        return "SERVICE_ERROR";
      }
      return "ERROR:" + std::to_string(error_number);
    };

    if (same_handle_retry) {
      if (!cmd_factory_srv->connect(authenticated_mysql_h)) {
        return "FIRST_CONNECT_SUCCEEDED";
      }

      const std::string first_error = connection_error();
      const std::string expected_error =
          retry_after_validation_failure ? "ERROR:2034" : "ERROR:1045";
      if (first_error != expected_error) {
        return "FIRST_" + first_error;
      }

      if (!cmd_factory_srv->connect(authenticated_mysql_h)) {
        return "RETRY_CONNECTED";
      }
      const std::string retry_error = connection_error();
      if (retry_error != "ERROR:2034") {
        return "RETRY_" + retry_error;
      }

      if (!cmd_options_srv->set(authenticated_mysql_h, MYSQL_COMMAND_PASSWORD,
                                password.c_str())) {
        return "RETRY_SET_SUCCEEDED";
      }
      return connection_error();
    }

    if (cmd_factory_srv->connect(authenticated_mysql_h)) {
      return connection_error();
    }

    constexpr const char query[] = "SELECT CURRENT_USER()";
    if (cmd_query_srv->query(authenticated_mysql_h, query, strlen(query)) ||
        cmd_query_result_srv->store_result(authenticated_mysql_h, &mysql_res) ||
        mysql_res == nullptr) {
      return connection_error();
    }

    MYSQL_ROW_H row = nullptr;
    ulong *field_lengths = nullptr;
    if (cmd_query_result_srv->fetch_row(mysql_res, &row) || row == nullptr ||
        row[0] == nullptr ||
        cmd_query_result_srv->fetch_lengths(mysql_res, &field_lengths) ||
        field_lengths == nullptr) {
      return "SERVICE_ERROR";
    }

    return std::string(row[0], field_lengths[0]);
  };

  const std::string output = authenticate();
  store_string_result(result, length, output);
  *error = 0;
  return result;
}

// Run in thread + failed connect + cleanup
static long long test_mysql_command_services_explicit_connect_fail_cleanup_udf(
    UDF_INIT *, UDF_ARGS *, unsigned char *is_null, unsigned char *error) {
  *is_null = 0;
  *error = 1;

  constexpr const char *bad_user = "no_such_user";
  constexpr const char *host = "localhost";

  long long ret_err = -1;

  // Spawn a thread, since the cmd thread must run off the server's main session
  // thread
  std::thread worker([&] {
    MYSQL_H mysql_h = nullptr;

    if (cmd_thread_srv->init() != 0) return;

    do {
      if (cmd_factory_srv->init(&mysql_h) != 0 || mysql_h == nullptr) break;

      // Set the options
      if (cmd_options_srv->set(mysql_h, MYSQL_NO_LOCK_REGISTRY,
                               reinterpret_cast<void *>(1)) != 0 ||
          cmd_options_srv->set(mysql_h, MYSQL_COMMAND_USER_NAME, bad_user) !=
              0 ||
          cmd_options_srv->set(mysql_h, MYSQL_COMMAND_HOST_NAME, host) != 0) {
        cmd_factory_srv->close(mysql_h);
        mysql_h = nullptr;
        break;  // Failed setting the options, exit
      }

      // Expect failure: wrong user
      ret_err = (cmd_factory_srv->connect(mysql_h) != 0) ? 1 : 0;

      // Always close the handle even on failure
      cmd_factory_srv->close(mysql_h);
      mysql_h = nullptr;
    } while (false);

    cmd_thread_srv->end();
  });

  worker.join();

  *error = (ret_err == -1)
               ? 1
               : 0;  // only mark UDF-level error if we never tried connect
  return ret_err;
}

static bool mcs_client_flags_noop_update_affected_rows(
    bool set_client_found_rows, uint64_t *affected_rows) {
  // Simple UPDATE that matches one row but changes no rows. Without
  // CLIENT_FOUND_ROWS it should report 0 affected rows, otherwise, if set it
  // should report 1
  constexpr const char *noop_update_matching_row =
      "UPDATE test.mcs_client_flags SET c1 = 1 WHERE c1 = 1";

  MYSQL_H mysql_h = nullptr;
  const uint32_t client_flags = CLIENT_FOUND_ROWS;
  uint32_t actual_client_flags = 0;

  if (cmd_factory_srv->init(&mysql_h) != 0 || mysql_h == nullptr) return true;

  auto close_mysql_h =
      create_scope_guard([&] { cmd_factory_srv->close(mysql_h); });

  const uint32_t expected_client_flags =
      set_client_found_rows ? client_flags : 0;

  // Verify that MYSQL_COMMAND_CLIENT_FLAGS stores the value before connect()
  if (set_client_found_rows &&
      cmd_options_srv->set(mysql_h, MYSQL_COMMAND_CLIENT_FLAGS,
                           &client_flags) != 0) {
    return true;
  }

  if (cmd_options_srv->get(mysql_h, MYSQL_COMMAND_CLIENT_FLAGS,
                           &actual_client_flags) != 0 ||
      actual_client_flags != expected_client_flags) {
    return true;
  }

  if (cmd_factory_srv->connect(mysql_h) != 0) return true;

  if (cmd_query_srv->query(mysql_h, noop_update_matching_row,
                           strlen(noop_update_matching_row)) != 0) {
    return true;
  }

  if (cmd_query_srv->affected_rows(mysql_h, affected_rows) != 0) return true;

  return false;
}

static long long test_mysql_command_services_client_flags_udf(
    UDF_INIT *, UDF_ARGS *args, unsigned char *is_null, unsigned char *error) {
  *is_null = 0;
  *error = 0;

  if (args->arg_count > 0) {
    *error = 1;
    return 0;
  }

  uint64_t default_affected_rows = 0;
  uint64_t found_rows_affected_rows = 0;

  // Run an UPDATE that matches one row but changes nothing
  if (mcs_client_flags_noop_update_affected_rows(false,
                                                 &default_affected_rows) ||
      mcs_client_flags_noop_update_affected_rows(true,
                                                 &found_rows_affected_rows)) {
    *error = 1;
    return 0;
  }

  // Expectation is that without CLIENT_FOUND_ROWS set it should report 0
  // affected rows, otherwise 1
  return (default_affected_rows == 0 && found_rows_affected_rows == 1) ? 1 : 0;
}

static char *test_mysql_command_session_state_udf(UDF_INIT *, UDF_ARGS *args,
                                                  char *result,
                                                  unsigned long *length,
                                                  unsigned char *,
                                                  unsigned char *error) {
  return run_session_state_query_udf(args, result, length, error, true);
}

static char *test_mysql_command_session_state_default_consumer_udf(
    UDF_INIT *, UDF_ARGS *args, char *result, unsigned long *length,
    unsigned char *, unsigned char *error) {
  return run_session_state_query_udf(args, result, length, error, false);
}

static char *test_mysql_command_session_state_client_flags_udf(
    UDF_INIT *, UDF_ARGS *args, char *result, unsigned long *length,
    unsigned char *, unsigned char *error) {
  return run_session_state_query_udf(args, result, length, error, false, true);
}

static char *test_mysql_command_session_state_sequence_udf(
    UDF_INIT *, UDF_ARGS *args, char *result, unsigned long *length,
    unsigned char *, unsigned char *error) {
  *error = 1;
  *result = '\0';

  if ((args->arg_count != 3U && args->arg_count != 4U) ||
      args->arg_type[args->arg_count - 1U] != STRING_RESULT ||
      args->arg_type[args->arg_count - 2U] != STRING_RESULT ||
      args->arg_type[args->arg_count - 3U] != STRING_RESULT ||
      (args->arg_count == 4U && args->arg_type[0] != STRING_RESULT)) {
    return nullptr;
  }

  MYSQL_H session_mysql_h = nullptr;
  char *output = result;
  std::string setup_query;
  std::string first_query;
  std::string second_query;
  std::string session_state;

  mysql_command_session_state_type tracker_type;
  if (get_session_track_type_arg(args, args->arg_count - 1U, &tracker_type)) {
    return nullptr;
  }

  if (args->arg_count == 4U) {
    setup_query.assign(args->args[0], args->lengths[0]);
    first_query.assign(args->args[1], args->lengths[1]);
    second_query.assign(args->args[2], args->lengths[2]);
  } else {
    first_query.assign(args->args[0], args->lengths[0]);
    second_query.assign(args->args[1], args->lengths[1]);
  }

  if (cmd_factory_srv->init(&session_mysql_h) || session_mysql_h == nullptr) {
    return nullptr;
  }
  if (cmd_factory_srv->connect(session_mysql_h)) {
    cmd_factory_srv->close(session_mysql_h);
    return nullptr;
  }
  if (set_session_track_capable_consumer(session_mysql_h)) {
    cmd_factory_srv->close(session_mysql_h);
    return nullptr;
  }

  if (!setup_query.empty() &&
      execute_query_and_discard_result(session_mysql_h, setup_query, &output,
                                       length)) {
    goto err;
  }

  if (execute_query_and_discard_result(session_mysql_h, first_query, &output,
                                       length)) {
    goto err;
  }

  execute_query_and_ignore_failure(session_mysql_h, second_query);

  collect_session_state(session_mysql_h, tracker_type, &session_state);
  store_string_result(result, length, session_state);
  output = result;

err:
  *error = 0;
  cmd_factory_srv->close(session_mysql_h);
  return output;
}

static long long test_mysql_command_session_error_info_success_clears_udf(
    UDF_INIT *, UDF_ARGS *args, unsigned char *is_null, unsigned char *error) {
  static constexpr char failing_query[] = "SELECT * FROM test.no_such_table";
  static constexpr char success_query[] = "SELECT 1";

  *error = 1;
  *is_null = 0;

  if (args->arg_count != 0U) {
    return 0;
  }

  MYSQL_H session_mysql_h = nullptr;
  char errbuf[MYSQL_ERRMSG_SIZE] = {0};
  char *errmsg = errbuf;
  unsigned long errmsg_length = sizeof(errbuf);
  char error_info_buf[MYSQL_ERRMSG_SIZE] = {0};
  char *error_info = error_info_buf;
  char *sqlstate = nullptr;
  unsigned int err_no = 0;
  long long ret = 0;

  if (cmd_factory_srv->init(&session_mysql_h) || session_mysql_h == nullptr ||
      cmd_factory_srv->connect(session_mysql_h)) {
    goto err;
  }

  execute_query_and_ignore_failure(session_mysql_h, failing_query);

  if (execute_query_and_discard_result(session_mysql_h, success_query, &errmsg,
                                       &errmsg_length) ||
      cmd_error_info_srv->sql_errno(session_mysql_h, &err_no) ||
      cmd_error_info_srv->sql_error(session_mysql_h, &error_info) ||
      cmd_error_info_srv->sql_state(session_mysql_h, &sqlstate)) {
    goto err;
  }

  ret = (err_no == 0 && error_info[0] == '\0' && sqlstate != nullptr &&
         strcmp(sqlstate, "00000") == 0)
            ? 1
            : 0;
  *error = 0;

err:
  if (session_mysql_h != nullptr) {
    cmd_factory_srv->close(session_mysql_h);
  }
  return ret;
}

static long long test_mysql_command_session_state_error_reset_udf(
    UDF_INIT *, UDF_ARGS *args, unsigned char *is_null, unsigned char *error) {
  *error = 1;
  *is_null = 0;

  if (args->arg_count != 4U || args->arg_type[0] != STRING_RESULT ||
      args->arg_type[1] != STRING_RESULT ||
      args->arg_type[2] != STRING_RESULT ||
      args->arg_type[3] != STRING_RESULT) {
    return 0;
  }

  const std::string setup_query(args->args[0], args->lengths[0]);
  const std::string first_query(args->args[1], args->lengths[1]);
  const std::string second_query(args->args[2], args->lengths[2]);
  mysql_command_session_state_type tracker_type;
  if (get_session_track_type_arg(args, 3, &tracker_type)) {
    return 0;
  }

  long long ret = -1;

  std::thread worker([&] {
    MYSQL_H session_mysql_h = nullptr;
    char *errmsg = nullptr;
    unsigned long errmsg_length = 0;

    if (cmd_thread_srv->init() != 0) return;

    do {
      if (cmd_factory_srv->init(&session_mysql_h) || session_mysql_h == nullptr)
        break;
      if (cmd_factory_srv->connect(session_mysql_h)) break;
      if (set_session_track_capable_consumer(session_mysql_h)) break;

      if (execute_query_and_discard_result(session_mysql_h, setup_query,
                                           &errmsg, &errmsg_length)) {
        break;
      }
      if (execute_query_and_discard_result(session_mysql_h, first_query,
                                           &errmsg, &errmsg_length)) {
        break;
      }

      execute_query_and_ignore_failure(session_mysql_h, second_query);

      std::string session_state;
      collect_session_state(session_mysql_h, tracker_type, &session_state);
      ret = session_state.empty() ? 1 : 0;
    } while (false);

    if (session_mysql_h != nullptr) cmd_factory_srv->close(session_mysql_h);
    cmd_thread_srv->end();
  });

  worker.join();

  if (ret == -1) return 0;

  *error = 0;
  return ret;
}

static long long test_mysql_command_session_state_reset_udf(
    UDF_INIT *, UDF_ARGS *args, unsigned char *is_null, unsigned char *error) {
  *error = 1;
  *is_null = 0;

  if (args->arg_count != 3U || args->arg_type[0] != STRING_RESULT ||
      args->arg_type[1] != STRING_RESULT ||
      args->arg_type[2] != STRING_RESULT) {
    return 0;
  }

  MYSQL_H session_mysql_h = nullptr;
  char *errmsg = nullptr;
  unsigned long errmsg_length = 0;
  const std::string setup_query(args->args[0], args->lengths[0]);
  const std::string query(args->args[1], args->lengths[1]);
  mysql_command_session_state_type tracker_type;
  if (get_session_track_type_arg(args, 2, &tracker_type)) {
    return 0;
  }
  long long ret = 0;

  if (cmd_factory_srv->init(&session_mysql_h) || session_mysql_h == nullptr) {
    return 0;
  }
  if (cmd_factory_srv->connect(session_mysql_h)) {
    goto err;
  }
  if (set_session_track_capable_consumer(session_mysql_h)) {
    goto err;
  }

  if (execute_query_and_discard_result(session_mysql_h, setup_query, &errmsg,
                                       &errmsg_length)) {
    goto err;
  }
  if (execute_query_and_discard_result(session_mysql_h, query, &errmsg,
                                       &errmsg_length)) {
    goto err;
  }
  if (cmd_factory_srv->reset(session_mysql_h)) {
    goto err;
  }

  {
    std::string session_state;
    collect_session_state(session_mysql_h, tracker_type, &session_state);
    ret = session_state.empty() ? 1 : 0;
  }

  *error = 0;

err:
  cmd_factory_srv->close(session_mysql_h);
  return ret;
}

static long long test_mysql_command_session_state_local_thd_udf(
    UDF_INIT *, UDF_ARGS *args, unsigned char *is_null, unsigned char *error) {
  *error = 1;
  *is_null = 0;

  if (args->arg_count != 0U) {
    return 0;
  }

  MYSQL_H session_mysql_h = nullptr;
  char *errmsg = nullptr;
  unsigned long errmsg_length = 0;
  long long ret = 0;

  if (cmd_factory_srv->init(&session_mysql_h) || session_mysql_h == nullptr ||
      cmd_options_srv->set(session_mysql_h, MYSQL_COMMAND_LOCAL_THD_HANDLE,
                           nullptr) ||
      cmd_factory_srv->connect(session_mysql_h)) {
    goto err;
  }
  if (set_session_track_capable_consumer(session_mysql_h)) {
    goto err;
  }

  if (execute_query_and_discard_result(
          session_mysql_h, "SET @@session.session_track_state_change=ON",
          &errmsg, &errmsg_length) ||
      execute_query_and_discard_result(
          session_mysql_h, "SET @test_mysql_command_services_local_state=1",
          &errmsg, &errmsg_length)) {
    goto err;
  }

  {
    std::string session_state;

    /*
      Commands executed on a command-service local THD populate the MYSQL_H
      session-state cache before the tracker state is consumed.
    */
    collect_session_state(session_mysql_h,
                          MYSQL_COMMAND_SESSION_TRACK_STATE_CHANGE,
                          &session_state);
    ret = (session_state == "1") ? 1 : 0;
  }

  if (ret == 0 || execute_query_and_discard_result(session_mysql_h, "SELECT 1",
                                                   &errmsg, &errmsg_length)) {
    goto err;
  }

  {
    std::string session_state;

    /*
      A later command-service local THD command that produces no tracker
      changes must clear the cache.
    */
    collect_session_state(session_mysql_h,
                          MYSQL_COMMAND_SESSION_TRACK_STATE_CHANGE,
                          &session_state);
    ret = session_state.empty() ? 1 : 0;
  }

  *error = 0;

err:
  if (session_mysql_h != nullptr) {
    cmd_factory_srv->close(session_mysql_h);
  }
  return ret;
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

  Udf_func_longlong udf3 = test_mysql_command_services_error_code_udf;
  if (udf_srv->udf_register("test_mysql_command_services_error_code_udf",
                            INT_RESULT, reinterpret_cast<Udf_func_any>(udf3),
                            nullptr, nullptr)) {
    fprintf(
        stderr,
        "Can't register the test_mysql_command_services_error_code_udf UDF\n");
    return 1;
  }

  Udf_func_longlong udf4 =
      test_mysql_command_services_explicit_connect_fail_cleanup_udf;
  if (udf_srv->udf_register(
          "test_mysql_command_services_explicit_connect_fail_cleanup_udf",
          INT_RESULT, reinterpret_cast<Udf_func_any>(udf4), nullptr, nullptr)) {
    fprintf(
        stderr,
        "Can't register "
        "test_mysql_command_services_explicit_connect_fail_cleanup_udf UDF\n");
    return 1;
  }

  Udf_func_longlong udf5 = test_mysql_command_services_client_flags_udf;
  if (udf_srv->udf_register("test_mysql_command_services_client_flags_udf",
                            INT_RESULT, reinterpret_cast<Udf_func_any>(udf5),
                            nullptr, nullptr)) {
    fprintf(
        stderr,
        "Can't register test_mysql_command_services_client_flags_udf UDF\n");
    return 1;
  }

  Udf_func_string udf6 = test_mysql_command_session_state_udf;
  if (udf_srv->udf_register("test_mysql_command_session_state_udf",
                            STRING_RESULT, reinterpret_cast<Udf_func_any>(udf6),
                            nullptr, nullptr)) {
    fprintf(stderr,
            "Can't register the test_mysql_command_session_state_udf UDF\n");
    return 1;
  }

  Udf_func_string udf7 = test_mysql_command_session_state_default_consumer_udf;
  if (udf_srv->udf_register("test_mysql_command_session_state_default_consumer_"
                            "udf",
                            STRING_RESULT, reinterpret_cast<Udf_func_any>(udf7),
                            nullptr, nullptr)) {
    fprintf(stderr,
            "Can't register the "
            "test_mysql_command_session_state_default_consumer_udf UDF\n");
    return 1;
  }

  Udf_func_string udf_session_state_client_flags =
      test_mysql_command_session_state_client_flags_udf;
  if (udf_srv->udf_register(
          "test_mysql_command_session_state_client_flags_udf", STRING_RESULT,
          reinterpret_cast<Udf_func_any>(udf_session_state_client_flags),
          nullptr, nullptr)) {
    fprintf(stderr,
            "Can't register the "
            "test_mysql_command_session_state_client_flags_udf UDF\n");
    return 1;
  }

  Udf_func_string udf8 = test_mysql_command_session_state_sequence_udf;
  if (udf_srv->udf_register("test_mysql_command_session_state_sequence_udf",
                            STRING_RESULT, reinterpret_cast<Udf_func_any>(udf8),
                            nullptr, nullptr)) {
    fprintf(stderr,
            "Can't register the test_mysql_command_session_state_sequence_udf "
            "UDF\n");
    return 1;
  }

  Udf_func_longlong udf9 = test_mysql_command_session_state_error_reset_udf;
  if (udf_srv->udf_register("test_mysql_command_session_state_error_reset_udf",
                            INT_RESULT, reinterpret_cast<Udf_func_any>(udf9),
                            nullptr, nullptr)) {
    fprintf(stderr,
            "Can't register the "
            "test_mysql_command_session_state_error_reset_udf UDF\n");
    return 1;
  }
  Udf_func_longlong udf10 = test_mysql_command_session_state_reset_udf;
  if (udf_srv->udf_register("test_mysql_command_session_state_reset_udf",
                            INT_RESULT, reinterpret_cast<Udf_func_any>(udf10),
                            nullptr, nullptr)) {
    fprintf(stderr,
            "Can't register the "
            "test_mysql_command_session_state_reset_udf UDF\n");
    return 1;
  }
  Udf_func_longlong udf11 = test_mysql_command_session_state_local_thd_udf;
  if (udf_srv->udf_register("test_mysql_command_session_state_local_thd_udf",
                            INT_RESULT, reinterpret_cast<Udf_func_any>(udf11),
                            nullptr, nullptr)) {
    fprintf(stderr,
            "Can't register the "
            "test_mysql_command_session_state_local_thd_udf UDF\n");
    return 1;
  }
  Udf_func_longlong udf12 =
      test_mysql_command_session_error_info_success_clears_udf;
  if (udf_srv->udf_register(
          "test_mysql_command_session_error_info_success_clears_udf",
          INT_RESULT, reinterpret_cast<Udf_func_any>(udf12), nullptr,
          nullptr)) {
    fprintf(stderr,
            "Can't register the "
            "test_mysql_command_session_error_info_success_clears_udf "
            "UDF\n");
    return 1;
  }

  Udf_func_string udf_row_semantics =
      test_mysql_command_services_row_semantics_udf;
  if (udf_srv->udf_register("test_mysql_command_services_row_semantics_udf",
                            STRING_RESULT,
                            reinterpret_cast<Udf_func_any>(udf_row_semantics),
                            nullptr, nullptr)) {
    fprintf(stderr,
            "Can't register the "
            "test_mysql_command_services_row_semantics_udf UDF\n");
    return 1;
  }

  Udf_func_string udf_authenticate =
      test_mysql_command_services_authenticate_udf;
  if (udf_srv->udf_register(
          "test_mysql_command_services_authenticate_udf", STRING_RESULT,
          reinterpret_cast<Udf_func_any>(udf_authenticate), nullptr, nullptr)) {
    fprintf(stderr,
            "Can't register the "
            "test_mysql_command_services_authenticate_udf UDF\n");
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
  if (udf_srv->udf_unregister("test_mysql_command_services_error_code_udf",
                              &was_present))
    fprintf(stderr,
            "Can't unregister the test_mysql_command_services_error_code_udf "
            "UDF\n");
  if (udf_srv->udf_unregister(
          "test_mysql_command_services_explicit_connect_fail_cleanup_udf",
          &was_present))
    fprintf(
        stderr,
        "Can't unregister "
        "test_mysql_command_services_explicit_connect_fail_cleanup_udf UDF\n");
  if (udf_srv->udf_unregister("test_mysql_command_services_client_flags_udf",
                              &was_present))
    fprintf(stderr,
            "Can't unregister test_mysql_command_services_client_flags_udf "
            "UDF\n");
  if (udf_srv->udf_unregister("test_mysql_command_session_state_udf",
                              &was_present))
    fprintf(stderr,
            "Can't unregister the test_mysql_command_session_state_udf UDF\n");
  if (udf_srv->udf_unregister(
          "test_mysql_command_session_state_default_consumer_udf",
          &was_present))
    fprintf(stderr,
            "Can't unregister the "
            "test_mysql_command_session_state_default_consumer_udf UDF\n");
  if (udf_srv->udf_unregister(
          "test_mysql_command_session_state_client_flags_udf", &was_present))
    fprintf(stderr,
            "Can't unregister the "
            "test_mysql_command_session_state_client_flags_udf UDF\n");
  if (udf_srv->udf_unregister("test_mysql_command_session_state_sequence_udf",
                              &was_present))
    fprintf(stderr,
            "Can't unregister the "
            "test_mysql_command_session_state_sequence_udf UDF\n");
  if (udf_srv->udf_unregister(
          "test_mysql_command_session_state_error_reset_udf", &was_present))
    fprintf(stderr,
            "Can't unregister the "
            "test_mysql_command_session_state_error_reset_udf UDF\n");
  if (udf_srv->udf_unregister("test_mysql_command_session_state_reset_udf",
                              &was_present))
    fprintf(stderr,
            "Can't unregister the "
            "test_mysql_command_session_state_reset_udf UDF\n");
  if (udf_srv->udf_unregister("test_mysql_command_session_state_local_thd_udf",
                              &was_present))
    fprintf(stderr,
            "Can't unregister the "
            "test_mysql_command_session_state_local_thd_udf UDF\n");
  if (udf_srv->udf_unregister(
          "test_mysql_command_session_error_info_success_clears_udf",
          &was_present))
    fprintf(stderr,
            "Can't unregister the "
            "test_mysql_command_session_error_info_success_clears_udf "
            "UDF\n");
  if (udf_srv->udf_unregister("test_mysql_command_services_row_semantics_udf",
                              &was_present))
    fprintf(stderr,
            "Can't unregister the "
            "test_mysql_command_services_row_semantics_udf UDF\n");
  if (udf_srv->udf_unregister("test_mysql_command_services_authenticate_udf",
                              &was_present))
    fprintf(stderr,
            "Can't unregister the "
            "test_mysql_command_services_authenticate_udf UDF\n");
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
