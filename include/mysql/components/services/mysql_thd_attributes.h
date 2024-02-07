/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_THD_ATTRIBUTES_H
#define MYSQL_THD_ATTRIBUTES_H

#include <mysql/components/service.h>
#include <mysql/components/services/bits/thd.h>

/**
  @ingroup group_components_services_inventory

  THD Attributes service allows to obtain data associated with the THD
  object, which keeps various attributes of the user session.

  Currently, following attributes are supported:

  - Query Digest Text
  - Query Text
  - Host or IP
  - Schema
  - Command String ("command" of the return mysql_cstring_with_length type)
  - SQL Command String ("sql_command" of the return mysql_cstring_with_length
  type)
  - Query Character Set ("query_charset" of the return mysql_cstring_with_length
  type)

  @section thd_attributes_init Initialization

  The service can be instantiated using the registry service with the
  "mysql_thd_attributes" name.

  @code
  SERVICE_TYPE(registry) *registry = mysql_plugin_registry_acquire();
  my_service<SERVICE_TYPE(mysql_thd_attributes)>
                                         svc("mysql_thd_attributes", registry);
  if (svc.is_valid()) {
    // The service is ready to be used
  }
  @endcode

  @section thd_attributes_query_digest_text Query Digest Text

  Query Digest represents converted SQL statement to normalized form. The code
  below demonstrates how query digest can be obtained from the service.

  @code
  my_h_string str;

  mysql_thd_attributes->get(m_thd, "query_digest",
                                   reinterpret_cast<void *>(&str));
  @endcode

  The buffer can be fetched using the code below:

  @code
  char buf[1024]; // buffer must be big enough to store the digest

  mysql_string_converter->convert_to_buffer(str, buf, sizeof(buf), "utf8mb3");
  @endcode

  After the string content has been copied into another buffer, it must be
  destroyed:

  @code
  mysql_string_factory->destroy(str);
  @endcode

  @section thd_attributes_query_text Query Text

  Query text represents complete SQL text currently being executed within a
  given session. The example code is identical to the one for Query Digest, the
  only difference is that attribute name "sql_text" should be used in this
  case.

  @section thd_attributes_host_or_ip Host or IP

  Host or IP text contains host or IP (if host unavailable) belonging to a
  client associated with a given session. The example code is identical to the
  one for Query Digest, the only difference is that attribute name "host_or_ip"
  should be used in this case.

  @section thd_attributes_schema Schema

  Schema text contains a name of database currently in use by a
  client associated with a given session. The example code is identical to the
  one for Query Digest, the only difference is that attribute name "schema"
  should be used in this case.
*/

BEGIN_SERVICE_DEFINITION(mysql_thd_attributes)

/**
  Get THD attribute.

  Currently, following attributes are supported:

  - Query Digest Text ("query_digest" of the returned my_h_string type)
  - Query Text ("sql_text" of the returned my_h_string type)
  - Host or IP ("host_or_ip" of the returned my_h_string type)
  - Schema ("schema" of the returned my_h_string type)
  - Is Upgrade Thread ("is_upgrade_thread" of the returned bool type)
  - Is Init File System Thread ("is_init_file_thread" of the returned bool type)
  - Command String ("command" of the return mysql_cstring_with_length type)
  - SQL Command String ("sql_command" of the return mysql_cstring_with_length
  type)
  - Query Character Set ("query_charset" of the return mysql_cstring_with_length
  type)
  - status variable ("thd_status" of the returned uint16 type). if session is
    OK, session is killed, query is killed or timeout
  - time-zone name variable ("time_zone_name" of the returned
    mysql_cstring_with_length type)
  - Query execution status ("da_status" of the returned uint16 type).

  @param      thd           Session THD object.
  @param      name          Name of the attribute to be set.
  @param[out] inout_pvalue  Iterator pointer.

  @return
    @retval FALSE Succeeded.
    @retval TRUE  Failed.
*/
DECLARE_BOOL_METHOD(get, (MYSQL_THD thd, const char *name, void *inout_pvalue));

/**
  Set THD attribute.

  Currently the implementation does not support setting of any attribute.

  @param      thd           Session THD object.
  @param      name          Name of the attribute to be set.
  @param[out] inout_pvalue  Iterator pointer.

  @return The function always fail and return TRUE value.
*/
DECLARE_BOOL_METHOD(set, (MYSQL_THD thd, const char *name, void *inout_pvalue));

END_SERVICE_DEFINITION(mysql_thd_attributes)

#endif /* MYSQL_THD_ATTRIBUTES_H */
