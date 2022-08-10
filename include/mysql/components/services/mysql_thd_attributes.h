/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

  @section Initialization

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

  @section Query Digest Text

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
*/
BEGIN_SERVICE_DEFINITION(mysql_thd_attributes)

/**
  Get THD attribute.

  Currently, following attributes are supported:

  - Query Digest Text ("query_digest" of the returned my_h_string type).

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
