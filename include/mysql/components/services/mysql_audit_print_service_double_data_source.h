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

#ifndef MYSQL_AUDIT_PRINT_SERVICE_DOUBLE_DATA_SOURCE_H
#define MYSQL_AUDIT_PRINT_SERVICE_DOUBLE_DATA_SOURCE_H

#include <mysql/components/service.h>
#include <mysql/components/services/bits/thd.h>
#include <mysql/components/services/mysql_string.h>

/**
  @ingroup group_components_services_inventory

  Audit print service allows to obtain data of the double type associated with
  the THD object, which keeps various data of the user session. It also allows
  to obtain data not necessarily bound with the user session.

  Currently, following data is supported:

  - Query time     ("query_time")     Query execution time [seconds].
  - Rows sent      ("rows_sent")      Row count sent to the client as a result.
  - Rows examined  ("rows_examined")  Row count accessed during the query.
  - Bytes received ("bytes_received") Byte count received from the client.
  - Bytes sent     ("bytes_sent")     Byte count sent to the client.

  @section double_service_init Initialization

  The service can be instantiated using the registry service with the
  "mysql_audit_print_service_double_data_source" name.

  @code
  SERVICE_TYPE(registry) *registry = mysql_plugin_registry_acquire();
  my_service<SERVICE_TYPE(mysql_audit_print_service_double_data_source)>
  svc("mysql_audit_print_service_double_data_source", registry);
  if (svc.is_valid()) {
    // The service is ready to be used
  }
  @endcode

  @section double_service_query_time Query Time

  Query Time represents query execution time in seconds.

  @code
  double value;

  svc->get(m_thd, "query_time", &value);
  @endcode
*/
BEGIN_SERVICE_DEFINITION(mysql_audit_print_service_double_data_source)

/**
  Get data value.

  @param      thd   Session THD object.
  @param      name  Name of the data value to be retrieved.
  @param[out] out   Out value pointer. Must not be nullptr.

  @return
    @retval FALSE Succeeded.
    @retval TRUE  Failed.
*/
DECLARE_BOOL_METHOD(get, (MYSQL_THD thd, my_h_string name, double *out));

END_SERVICE_DEFINITION(mysql_audit_print_service_double_data_source)

#endif /* MYSQL_AUDIT_PRINT_SERVICE_DOUBLE_DATA_SOURCE_H */
