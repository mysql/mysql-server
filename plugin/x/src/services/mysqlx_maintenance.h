/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef MYSQLX_MAINTENANCE_H
#define MYSQLX_MAINTENANCE_H

#include <mysql/components/service.h>
#include <mysql/components/service_implementation.h>

BEGIN_SERVICE_DEFINITION(mysqlx_maintenance)
DECLARE_BOOL_METHOD(reset_global_status_variables, ());
END_SERVICE_DEFINITION(mysqlx_maintenance)

extern SERVICE_TYPE_NO_CONST(mysqlx_maintenance)
    SERVICE_IMPLEMENTATION(mysql_server, mysqlx_maintenance);

#endif  // MYSQLX_MAINTENANCE_H
