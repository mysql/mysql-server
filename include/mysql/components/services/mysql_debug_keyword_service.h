/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef DEBUG_KEYWORD_SERVICE_GUARD
#define DEBUG_KEYWORD_SERVICE_GUARD

#include "mysql/components/service.h"

/**
  @ingroup group_components_services_inventory

  A service to check if debug keyword status.

  The debug keywords are used to implement various functionality for debugging.
  See my_dbug.h for more info. One of the functionality is @ref DBUG_EXECUTE_IF.

  This service lookup_debug_keyword() is used to implement @ref DBUG_EXECUTE_IF
  for use in components. The @ref DBUG_EXECUTE_IF is implemented in
  util/debug_execute_if.h

  The usage remains the same way as used in server. E.g.,
        DBUG_EXECUTE_IF("debug point", { ...code... return false; });
*/
BEGIN_SERVICE_DEFINITION(mysql_debug_keyword_service)

/**
  Check if debug keyword is enabled.

  @param[in]  name  The debug keyword to check if its enabled.
                    The memory for char* argument is managed by service invoker.
  @retval true    failure
  @retval false   success

  @return true if keyword is enabled.
*/
DECLARE_BOOL_METHOD(lookup_debug_keyword, (const char *name));

END_SERVICE_DEFINITION(mysql_debug_keyword_service)

#endif  // DEBUG_KEYWORD_SERVICE_GUARD