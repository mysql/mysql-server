/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

#ifndef DYNAMIC_PRIVILEGE_H
#define DYNAMIC_PRIVILEGE_H
#include <mysql/components/service.h>
#include <stddef.h>

DEFINE_SERVICE_HANDLE(Security_context_handle);

/**
  @ingroup group_components_services_inventory

  A service to register and unregister dynamic privileges

  Use it to add new privileges to the dynamic list.

  Privilege names are in ASCII.

  @sa @ref mysql_audit_api_connection_imp
*/
BEGIN_SERVICE_DEFINITION(dynamic_privilege_register)
DECLARE_BOOL_METHOD(register_privilege,
                    (const char *priv_name, size_t priv_name_len));
DECLARE_BOOL_METHOD(unregister_privilege,
                    (const char *priv_name, size_t priv_name_len));
END_SERVICE_DEFINITION(dynamic_privilege_register)

BEGIN_SERVICE_DEFINITION(global_grants_check)
DECLARE_BOOL_METHOD(has_global_grant,
                    (Security_context_handle, const char *priv_name,
                     size_t priv_name_len));
END_SERVICE_DEFINITION(global_grants_check)

/**
  @ingroup group_components_services_inventory

  A service to register and unregister dynamic privileges as deprecated.

  Privilege names are in ASCII.

  @sa @ref mysql_audit_api_connection_imp
*/
BEGIN_SERVICE_DEFINITION(dynamic_privilege_deprecation)
DECLARE_BOOL_METHOD(add, (const char *priv_name, size_t priv_name_len));
DECLARE_BOOL_METHOD(remove, (const char *priv_name, size_t priv_name_len));
END_SERVICE_DEFINITION(dynamic_privilege_deprecation)

#endif /* DYNAMIC_PRIVILEGE_H */
