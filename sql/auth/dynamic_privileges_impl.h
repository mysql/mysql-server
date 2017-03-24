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
#ifndef DYNAMIC_PRIVILEGES_IMPL_H
#define DYNAMIC_PRIVILEGES_IMPL_H
#include <mysql/components/service.h>
#include <mysql/components/service_implementation.h>
#include <stddef.h>

DEFINE_SERVICE_HANDLE(Security_context_handle);

/**
  Interface implementation for registering and checking global dynamic
  privileges.
*/
class dynamic_privilege_services_impl
{
public:

  static DEFINE_BOOL_METHOD(register_privilege,
    (const char *privilege_str, size_t privilege_str_len));

  /**
    Unregister a previously registered privilege object identifier so that it no
    longer can be used in GRANT statements.
    @param privilege_str Privilege object ID
    @param privilege_str_len The length of the string (not including \0)
    @return Error state
       @retval true Operation was not successful
       @retval false Success
  */
  static DEFINE_BOOL_METHOD(unregister_privilege,
    (const char *privilege_str, size_t privilege_str_len));

  /**
    Check if the supplied security context has the specified privilege identifier
    granted to it.
    @return
       @retval true The privilege was granted.
       @retval false Access is defined - no such privilege.
  */
  static DEFINE_BOOL_METHOD(has_global_grant,
    (Security_context_handle, const char *privilege_str,
     size_t privilege_str_len));
private:

};
bool dynamic_privilege_init(void);
#endif /* MYSQL_SERVER_DYNAMIC_LOADER_PATH_FILTER_H */

