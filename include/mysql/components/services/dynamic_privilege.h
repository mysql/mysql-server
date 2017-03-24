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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

#ifndef DYNAMIC_PRIVILEGE_H
#define DYNAMIC_PRIVILEGE_H
#include <mysql/components/service.h>
#include <stddef.h>

DEFINE_SERVICE_HANDLE(Security_context_handle);

BEGIN_SERVICE_DEFINITION(dynamic_privilege_register)
  DECLARE_BOOL_METHOD(register_privilege, (const char *, size_t));
  DECLARE_BOOL_METHOD(unregister_privilege, (const char *, size_t));
END_SERVICE_DEFINITION(dynamic_privilege_register)


BEGIN_SERVICE_DEFINITION(global_grants_check)
  DECLARE_BOOL_METHOD(has_global_grant,
                      (Security_context_handle, const char *, size_t));
END_SERVICE_DEFINITION(global_grants_check)

#endif /* DYNAMIC_PRIVILEGE_H */

