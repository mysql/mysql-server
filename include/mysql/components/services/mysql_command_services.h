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

#ifndef MYSQL_COMMAND_SERVICE_H
#define MYSQL_COMMAND_SERVICE_H

#include <mysql/components/service.h>
#include <mysql/components/services/mysql_string.h>
#include <stdint.h>

/* Command service provides mysql query service apis */

DEFINE_SERVICE_HANDLE(MYSQL_H);
DEFINE_SERVICE_HANDLE(MYSQL_RES_H);
DEFINE_SERVICE_HANDLE(MYSQL_FIELD_H);

#define MYSQL_ROW_H char **

/**
 This enum is used in mysql_service_mysql_command_options service to set the
 provided option similar to mysql_option api.
 @note Please take care that MYSQL_COMMAND_CONSUMER_SERVICE value should be
 greaterthan the last enum value of @ref mysql_option enum.
*/
enum mysql_command_option {
  MYSQL_TEXT_CONSUMER_FACTORY = 1024,  // Make sure this number should be
                                       // greaterthan enum mysql_option last
                                       // option value.
  MYSQL_TEXT_CONSUMER_METADATA,
  MYSQL_TEXT_CONSUMER_ROW_FACTORY,
  MYSQL_TEXT_CONSUMER_ERROR,
  MYSQL_TEXT_CONSUMER_GET_NULL,
  MYSQL_TEXT_CONSUMER_GET_INTEGER,
  MYSQL_TEXT_CONSUMER_GET_LONGLONG,
  MYSQL_TEXT_CONSUMER_GET_DECIMAL,
  MYSQL_TEXT_CONSUMER_GET_DOUBLE,
  MYSQL_TEXT_CONSUMER_GET_DATE_TIME,
  MYSQL_TEXT_CONSUMER_GET_STRING,
  MYSQL_TEXT_CONSUMER_CLIENT_CAPABILITIES,
  MYSQL_COMMAND_LOCAL_THD_HANDLE,
  MYSQL_COMMAND_PROTOCOL,
  MYSQL_COMMAND_USER_NAME,
  MYSQL_COMMAND_HOST_NAME,
  MYSQL_COMMAND_TCPIP_PORT
};

/**
  @ingroup group_components_services_inventory

  A service that provides the apis for mysql command init, info, connect,
  reset, close, commit, auto_commit and rollback.

*/
BEGIN_SERVICE_DEFINITION(mysql_command_factory)

/**
  Calls mysql_init() api to Gets or initializes a MYSQL_H structure

  @param[out] mysql_h Prepared mysql object from mysql_init call.

  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(init, (MYSQL_H * mysql_h));

/**
  Calls mysql_real_connect api to connects to a MySQL server.

  @param[in] mysql_h A valid mysql object.

  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(connect, (MYSQL_H mysql_h));

/**
  Calls mysql_reset_connection api to resets the connection to
  clear session state.

  @param[in] mysql_h A valid mysql object.

  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(reset, (MYSQL_H mysql_h));

/**
  Calls mysql_close api to closes a server connection.

  @param[in] mysql_h A valid mysql object.

  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(close, (MYSQL_H mysql_h));

/**
  Calls mysql_commit api to commits the transaction.

  @param[in] mysql_h A valid mysql object.

  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(commit, (MYSQL_H mysql_h));

/**
  Calls mysql_autocommit api to toggles autocommit mode on/off.

  @param[in] mysql_h A valid mysql object.
  @param[in] mode Sets autocommit mode on if mode is 1, off
          if mode is 0.

  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(autocommit, (MYSQL_H mysql_h, bool mode));

/**
  Calls mysql_rollback api to rolls back the transaction.

  @param[in] mysql_h A valid mysql object.

  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(rollback, (MYSQL_H mysql_h));
END_SERVICE_DEFINITION(mysql_command_factory)

/**
  @ingroup group_components_services_inventory

  A service that provides the apis for mysql command get_option and set_option.
*/
BEGIN_SERVICE_DEFINITION(mysql_command_options)

/**
  Calls mysql_options api to sets connect options for connection-establishment
  functions such as real_connect().

  @param[in] mysql A valid mysql object.
  @param[in] option The option argument is the option that you
             want to set.
  @param[in] arg The arg argument is the value
             for the option.

--------------+-------------------------------+--------------------------------+
     Type     |     Option                    |Explanation                     |
--------------+-------------------------------+--------------------------------+
const char *  |MYSQL_COMMAND_CONSUMER_SERVICE |The service (implementation)    |
              |                               |name/prefix to look for in the  |
              |                               |registry and direct all the     |
              |                               |calls to.                       |
--------------+-------------------------------+--------------------------------+
MYSQL_THD     |MYSQL_COMMAND_LOCAL_THD_HANDLE |The THD to run the query in.    |
              |                               |If null a new internal THD will |
              |                               |be created.                     |
--------------+-------------------------------+--------------------------------+
const char *  |MYSQL_COMMAND_PROTOCOL         |Could be valid socket meaning co|
              |                               |nnect to remote server, could be|
              |                               |"local"(default) meaning connect|
              |                               |to the current server.          |
--------------+-------------------------------+--------------------------------+
const char *  |MYSQL_COMMAND_USER_NAME        |The user name to send to the    |
              |                               |server/set into the thread's    |
              |                               |security context.               |
--------------+-------------------------------+--------------------------------+
const char *  |MYSQL_COMMAND_HOST_NAME        |The host name to use to         |
              |                               |connect/set into the thread's   |
              |                               |security context.               |
--------------+-------------------------------+--------------------------------+
int           |MYSQL_COMMAND_TCPIP_PORT       |The port to use to connect.     |
--------------+-------------------------------+--------------------------------+

  @note For the other mysql client options it calls the mysql_options api.

  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(set, (MYSQL_H mysql, int option, const void *arg));

/**
  Calls mysql_get_option api to returns the value of a mysql_options() option.

  @param[in] mysql A valid mysql object.
  @param[in] option The option argument is the option that you
          want to get.
  @param[out] arg The arg argument is the value
          for the option to store.

  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(get, (MYSQL_H mysql, int option, const void *arg));

END_SERVICE_DEFINITION(mysql_command_options)

/**
  @ingroup group_components_services_inventory

  A service that provides the apis for mysql command query and
  affected_rows.
*/
BEGIN_SERVICE_DEFINITION(mysql_command_query)

/**
  Calls mysql_real_query api to executes an SQL query specified
  as a counted string.

  @param[in] mysql A valid mysql object.
  @param[in] stmt_str SQL statement which has a query.
  @param[in] length A string length bytes long.

  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(query, (MYSQL_H mysql, const char *stmt_str,
                            unsigned long length));

/**
  Calls mysql_affected_rows api to return the number of rows
  changed/deleted/inserted by the last UPDATE,DELETE or INSERT query.

  @param[in]  mysql A valid mysql object.
  @param[out] *rows Number of rows affected, for SELECT stmt it tells
              about number of rows present.

  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(affected_rows, (MYSQL_H mysql, uint64_t *rows));
END_SERVICE_DEFINITION(mysql_command_query)

/**
  @ingroup group_components_services_inventory

  A service that provides the apis for mysql command, store_result,
  free_result, more_results, next_result, result_metadata and fetch_row.
*/
BEGIN_SERVICE_DEFINITION(mysql_command_query_result)

/**
  Calls mysql_store_result api to retrieves a complete result set.

  @param[in]  mysql A valid mysql object.
  @param[out] *mysql_res An mysql result object to get the result
              set.

  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(store_result, (MYSQL_H mysql, MYSQL_RES_H *mysql_res));

/**
  Calls mysql_free_result api to frees memory used by a result set.

  @param[in] mysql_res An mysql result object to free the result
              set.

  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(free_result, (MYSQL_RES_H mysql_res));

/**
  Calls mysql_more_results api to checks whether any more results exist.

  @param[in]  mysql A valid mysql object.

  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(more_results, (MYSQL_H mysql));

/**
  Calls mysql_next_result api to returns/initiates the next result
  in multiple-result executions.

  @param[in]  mysql A valid mysql object.

  @retval -1 no more results
  @retval >0 error
  @retval 0  if yes more results exits(keep looping)
*/
DECLARE_METHOD(int, next_result, (MYSQL_H mysql));

/**
  Calls mysql_result_metadata api to whether a result set has metadata.

  @param[in]  res_h An mysql result object to get the metadata
              info.

  @retval true    failure metadata_info not present.
  @retval false   success metadata_info present.
*/
DECLARE_BOOL_METHOD(result_metadata, (MYSQL_RES_H res_h));

/**
  Calls mysql_fetch_row api to fetches the next row from the result set.

  @param[in]  res_h An mysql result object to fetch a row from
              the result set.
  @param[out] *row To store the fetched row with server's charset.

  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(fetch_row, (MYSQL_RES_H res_h, MYSQL_ROW_H *row));

/**
  Calls mysql_fetch_lengths api to Returns the lengths of all columns
  in the current row.

  @param[in]  res_h An mysql result object to fetch a row from
              the result set.
  @param[out] *length lengths of all columns.

  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(fetch_lengths, (MYSQL_RES_H res_h, ulong **length));
END_SERVICE_DEFINITION(mysql_command_query_result)

/**
  @ingroup group_components_services_inventory

  A service that provides the apis for mysql command field info, fetch_field,
  num_fields, fetch_fields and field_count.
*/
BEGIN_SERVICE_DEFINITION(mysql_command_field_info)

/**
  Calls mysql_fetch_field api to returns the type of next table field.

  @param[in]  res_h An mysql result object to return the next table
              field.
  @param[out] *field_h Stores the definition of one column of a result
              set as a MYSQL_FIELD structure

  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(fetch_field, (MYSQL_RES_H res_h, MYSQL_FIELD_H *field_h));

/**
  Calls mysql_num_fields api to returns the number of columns in a result set.

  @param[in]  res_h A valid mysql result set object.
  @param[out] *num_fields Stores the number of columns in the result set.
  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(num_fields, (MYSQL_RES_H res_h, unsigned int *num_fields));

/**
  Calls mysql_fetch_fields api to returns an array of all field structures.

  @param[in]  res_h A valid mysql result set object.
  @param[out] **fields_h Stores the array of all fields for a result set.
  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(fetch_fields,
                    (MYSQL_RES_H res_h, MYSQL_FIELD_H **fields_h));

/**
  Calls mysql_field_count api to returns the number of columns for the most
  resent statement.

  @param[in]  mysql_h A valid mysql handle object.
  @param[out] *num_fields Stores the number of columns for the last stmt.
  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(field_count, (MYSQL_H mysql_h, unsigned int *num_fields));
END_SERVICE_DEFINITION(mysql_command_field_info)

/**
  @ingroup group_components_services_inventory

  A service that provides the apis for mysql command error info, mysql_errno,
  error, sqlstate.
*/
BEGIN_SERVICE_DEFINITION(mysql_command_error_info)

/**
  Calls mysql_errno api to return the number of most recently invoked mysql
  function.

  @param[in]  mysql_h A valid mysql handle object.
  @param[out] *err_no Stores the error number of last mysql function.
  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(sql_errno, (MYSQL_H mysql_h, unsigned int *err_no));

/**
  Calls mysql_error api to return the error message of most recently invoked
  mysql function.

  @param[in]  mysql_h A valid mysql handle object.
  @param[out] *errmsg Stores the error message of last mysql function.
  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(sql_error, (MYSQL_H mysql_h, char **errmsg));

/**
  Calls mysql_sqlstate api to return the SQLSTATE error code for the last error.

  @param[in]  mysql_h A valid mysql handle object.
  @param[out] *sqlstate_errmsg Stores the SQLSTATE error message of the most
              recently executed SQL stmt.
  @retval true    failure
  @retval false   success
*/
DECLARE_BOOL_METHOD(sql_state, (MYSQL_H mysql_h, char **sqlstate_errmsg));
END_SERVICE_DEFINITION(mysql_command_error_info)

#endif /* MYSQL_COMMAND_SERVICE_H */
