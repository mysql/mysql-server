/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef RPL_ASYNC_CONN_FAILOVER_DELETE_MANAGED_UDF_H
#define RPL_ASYNC_CONN_FAILOVER_DELETE_MANAGED_UDF_H

#include "sql/udf_service_impl.h"
#include "sql/udf_service_util.h"

class Rpl_async_conn_failover_delete_managed : public Udf_service_impl {
 private:
  Udf_charset_service m_charset_service;
  static constexpr const char *m_udf_name =
      "asynchronous_connection_failover_delete_managed";
  bool m_initialized{false};

 public:
  Rpl_async_conn_failover_delete_managed() = default;
  ~Rpl_async_conn_failover_delete_managed() override = default;

  /**
    Initialize variables, acquires the mysql_service_mysql_udf_metadata from the
    registry service and register the Asynchronous Connection Failover's UDFs.
    If there is an error registering any UDF, all installed UDFs are
    unregistered.

    @retval true if there was an error
    @retval false if all UDFs were registered
   */
  bool init() override;

  /**
    Release the udf_metadata service.

    @retval false  Success
    @retval true   Failure. service could not be released
  */
  bool deinit();

  /**
    Deletes managed network configuration details

    @param[in] init_id     UDF_INIT structure
    @param[in] args       UDF_ARGS structure containing argument details
    @param[out] result    error message
    @param[out] length    error message length
    @param[out] is_null   if result is null
    @param[in,out] error  error code

    @return error message
  */
  static char *delete_managed(UDF_INIT *init_id, UDF_ARGS *args, char *result,
                              unsigned long *length, unsigned char *is_null,
                              unsigned char *error);

  /**
    Initialize and verifies UDF's arguments. Also sets argument and result
    charset.

    @param[in] init_id     UDF_INIT structure
    @param[in] args       UDF_ARGS structure containing argument details
    @param[out] message   error message

    @return True if error, false otherwise.
  */
  static bool delete_managed_init(UDF_INIT *init_id, UDF_ARGS *args,
                                  char *message);

  /**
    Deinitialize variables initialized during init function.

    @param[in] init_id     UDF_INIT structure
  */
  static void delete_managed_deinit(UDF_INIT *init_id);
};
#endif /* RPL_ASYNC_CONN_FAILOVER_DELETE_MANAGED_UDF_H */
