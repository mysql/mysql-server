/* Copyright (c) 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_GLOBAL_VARIABLE_ATTRIBUTES_SERVICE_IMP_H
#define MYSQL_GLOBAL_VARIABLE_ATTRIBUTES_SERVICE_IMP_H

#include <mysql/components/services/mysql_global_variable_attributes_service.h>
#include <mysql/plugin.h>

/**
  @file sql/server_component/mysql_global_variable_attributes_service_imp.h
  The server implementation of global variable attributes service.
*/
extern SERVICE_TYPE(mysql_global_variable_attributes)
    SERVICE_IMPLEMENTATION(mysql_server, mysql_global_variable_attributes);
extern SERVICE_TYPE(mysql_global_variable_attributes_iterator)
    SERVICE_IMPLEMENTATION(mysql_server,
                           mysql_global_variable_attributes_iterator);

#endif /* MYSQL_GLOBAL_VARIABLE_ATTRIBUTES_SERVICE_IMP_H */
