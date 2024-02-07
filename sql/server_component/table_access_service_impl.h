/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#ifndef TABLE_ACCESS_SERVICE_IMPL_INCLUDED
#define TABLE_ACCESS_SERVICE_IMPL_INCLUDED

#include <mysql/components/component_implementation.h>
#include <mysql/components/service.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/table_access_service.h>

extern SERVICE_TYPE(table_access_factory_v1)
    SERVICE_IMPLEMENTATION(mysql_server, table_access_factory_v1);

extern SERVICE_TYPE(table_access_v1)
    SERVICE_IMPLEMENTATION(mysql_server, table_access_v1);

extern SERVICE_TYPE(table_access_index_v1)
    SERVICE_IMPLEMENTATION(mysql_server, table_access_index_v1);

extern SERVICE_TYPE(table_access_scan_v1)
    SERVICE_IMPLEMENTATION(mysql_server, table_access_scan_v1);

extern SERVICE_TYPE(table_access_update_v1)
    SERVICE_IMPLEMENTATION(mysql_server, table_access_update_v1);

extern SERVICE_TYPE(field_access_nullability_v1)
    SERVICE_IMPLEMENTATION(mysql_server, field_access_nullability_v1);

extern SERVICE_TYPE(field_integer_access_v1)
    SERVICE_IMPLEMENTATION(mysql_server, field_integer_access_v1);

extern SERVICE_TYPE(field_varchar_access_v1)
    SERVICE_IMPLEMENTATION(mysql_server, field_varchar_access_v1);

extern SERVICE_TYPE(field_any_access_v1)
    SERVICE_IMPLEMENTATION(mysql_server, field_any_access_v1);

#endif /* TABLE_ACCESS_SERVICE_IMPL_INCLUDED */
