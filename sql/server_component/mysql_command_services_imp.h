/* Copyright (c) 2022, Oracle and/or its affiliates.

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

#ifndef MYSQL_COMMAND_SERVICES_IMP_H
#define MYSQL_COMMAND_SERVICES_IMP_H

#include <include/my_inttypes.h>
#include <include/mysql.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/mysql_command_consumer.h>
#include <mysql/components/services/mysql_command_services.h>
#include <mysql/components/services/mysql_current_thread_reader.h>
#include <mysql/service_srv_session_bits.h>

// Use an internal MySQL server user
#define MYSQL_SESSION_USER "mysql.session"
#define MYSQL_SYS_HOST "localhost"

struct Mysql_handle {
  MYSQL *mysql = nullptr;
};

struct Mysql_res_handle {
  MYSQL_RES *mysql_res = nullptr;
};

/**
  This structure is used by mysql command service.
  session_svc and is_thd_associated is set in cssm_begin_connect()
  command_consumer_services is set by
  mysql_service_mysql_command_options::set() api,
  if it is nullptr then the default mysql_test_consumer services will be set.
  data is allocated in mysql_text_consumer_factory_v1 service start() api,
  and this data is retrived by csi_read_rows().
  This information is used by mysql_text_consumer service apis.
  mcs_thd, mcs_protocol, mcs_user_name, mcs_password, mcs_tcpip_port,
  mcs_db and mcs_client_flag will be set by mysql_service_mysql_command_options
  set() api.
*/
struct mysql_command_service_extn {
  MYSQL_SESSION session_svc;
  bool is_thd_associated; /* this is used to free session_svc,
                             if it was allocated. */
  MYSQL_DATA *data = nullptr;
  void *command_consumer_services = nullptr;
  SRV_CTX_H *consumer_srv_data = nullptr;
  MYSQL_THD mcs_thd = nullptr;
  const char *mcs_protocol = nullptr;
  const char *mcs_user_name = nullptr;
  const char *mcs_host_name = nullptr;
  const char *mcs_password = nullptr;
  int mcs_tcpip_port;
  const char *mcs_db = nullptr;
  uint32_t mcs_client_flag = 0;
};

#define MYSQL_COMMAND_SERVICE_EXTN(H)                    \
  reinterpret_cast<struct mysql_command_service_extn *>( \
      MYSQL_EXTENSION_PTR(H)->mcs_extn)

/**
  An implementation of mysql command services apis.
*/
class mysql_command_services_imp {
 public:
  /* mysql_command_factory service apis */
  static DEFINE_BOOL_METHOD(init, (MYSQL_H * mysql_h));
  static DEFINE_BOOL_METHOD(connect, (MYSQL_H mysql_h));
  static DEFINE_BOOL_METHOD(reset, (MYSQL_H mysql_h));
  static DEFINE_BOOL_METHOD(close, (MYSQL_H mysql_h));
  static DEFINE_BOOL_METHOD(commit, (MYSQL_H mysql_h));
  static DEFINE_BOOL_METHOD(autocommit, (MYSQL_H mysql_h, bool mode));
  static DEFINE_BOOL_METHOD(rollback, (MYSQL_H mysql_h));

  /* mysql_command_options service apis */
  static DEFINE_BOOL_METHOD(set, (MYSQL_H mysql, int option, const void *arg));
  static DEFINE_BOOL_METHOD(get, (MYSQL_H mysql, int option, const void *arg));

  /* mysql_command_query service apis */
  static DEFINE_BOOL_METHOD(query, (MYSQL_H mysql, const char *stmt_str,
                                    unsigned long length));
  static DEFINE_BOOL_METHOD(affected_rows, (MYSQL_H mysql, uint64_t *rows));

  /* mysql_command_query_result service apis */
  static DEFINE_BOOL_METHOD(store_result,
                            (MYSQL_H mysql, MYSQL_RES_H *mysql_res));
  static DEFINE_BOOL_METHOD(use_result,
                            (MYSQL_H mysql, MYSQL_RES_H *mysql_res));
  static DEFINE_BOOL_METHOD(free_result, (MYSQL_RES_H mysql_res));
  static DEFINE_BOOL_METHOD(more_results, (MYSQL_H mysql));
  static DEFINE_METHOD(int, next_result, (MYSQL_H mysql));
  static DEFINE_BOOL_METHOD(result_metadata, (MYSQL_RES_H res_h));
  static DEFINE_BOOL_METHOD(fetch_row, (MYSQL_RES_H res_h, MYSQL_ROW_H *row));
  static DEFINE_BOOL_METHOD(fetch_lengths, (MYSQL_RES_H res_h, ulong **length));

  /* mysql_command_field_info service apis */
  static DEFINE_BOOL_METHOD(fetch_field,
                            (MYSQL_RES_H res_h, MYSQL_FIELD_H *field_h));
  static DEFINE_BOOL_METHOD(num_fields,
                            (MYSQL_RES_H res_h, unsigned int *num_fields));
  static DEFINE_BOOL_METHOD(fetch_fields,
                            (MYSQL_RES_H res_h, MYSQL_FIELD_H **fields_h));
  static DEFINE_BOOL_METHOD(field_count,
                            (MYSQL_H mysql_h, unsigned int *num_fields));

  /* mysql_command_error_info service apis */
  static DEFINE_BOOL_METHOD(sql_errno, (MYSQL_H mysql_h, unsigned int *err_no));
  static DEFINE_BOOL_METHOD(sql_error, (MYSQL_H mysql_h, char **errmsg));
  static DEFINE_BOOL_METHOD(sql_state,
                            (MYSQL_H mysql_h, char **sqlstate_errmsg));
};

/** This is a wrapper class of all the mysql_text_consumer services refs */
class mysql_command_consumer_refs {
 public:
  SERVICE_TYPE(mysql_text_consumer_factory_v1) * factory_srv;
  SERVICE_TYPE(mysql_text_consumer_metadata_v1) * metadata_srv;
  SERVICE_TYPE(mysql_text_consumer_row_factory_v1) * row_factory_srv;
  SERVICE_TYPE(mysql_text_consumer_error_v1) * error_srv;
  SERVICE_TYPE(mysql_text_consumer_get_null_v1) * get_null_srv;
  SERVICE_TYPE(mysql_text_consumer_get_integer_v1) * get_integer_srv;
  SERVICE_TYPE(mysql_text_consumer_get_longlong_v1) * get_longlong_srv;
  SERVICE_TYPE(mysql_text_consumer_get_decimal_v1) * get_decimal_srv;
  SERVICE_TYPE(mysql_text_consumer_get_double_v1) * get_double_srv;
  SERVICE_TYPE(mysql_text_consumer_get_date_time_v1) * get_date_time_srv;
  SERVICE_TYPE(mysql_text_consumer_get_string_v1) * get_string_srv;
  SERVICE_TYPE(mysql_text_consumer_client_capabilities_v1) *
      client_capabilities_srv;
};

#endif /* MYSQL_COMMAND_SERVICES_IMP_H */
