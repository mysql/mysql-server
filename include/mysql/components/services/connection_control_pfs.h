/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef MYSQL_OPTION_TRACKER_H
#define MYSQL_OPTION_TRACKER_H

#include <mysql/components/service.h>

/**
  @ingroup group_components_services_inventory

  Option tracker registration and deregistration services

  This is a service that will allow registering an option.
  Each option has a name. The name is UTF8mb4 and is unique in
  the list.
  Manipulating the option list is an "expesive" operation since there
  is a global lock involved.

  Each code container (a component or a plugin) should register its
  options during its initialization and should unregister them during
  its deinitialization.
*/
BEGIN_SERVICE_DEFINITION(mysql_failed_attempts_tracker)

/**
  Define an option. Adds an option definition.

  If another option of the same name exists, the definition fails

  @param option            The name of the option, UTF8mb4. Must be unique.
  @param container         The container name. UTF8mb4
                            Please prefix with "plugin_" for plugins.
  @param is_enabled        non-0 if the option is marked as enabled, 0 otherwise
  @retval false success
  @retval true failure
*/
DECLARE_METHOD(void, define, (const char *userhost));
/**
  Undefine an option.

  Fails if no option is defined with the same name

  @param option            The name of the option, US ASCII
  @retval false success
  @retval true failure
*/
DECLARE_BOOL_METHOD(undefine, (const char *userhost));

END_SERVICE_DEFINITION(mysql_failed_attempts_tracker)

#endif /* MYSQL_OPTION_TRACKER_H */
