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

#include "components/mysql_server/system_variable_source_imp.h"
#include "components/mysql_server/server_component.h"
#include "set_var.h"

void mysql_comp_system_variable_source_init()
{
  return;
}

/**
  Get source of given system variable.

  @param [in] name Name of system variable
  @param [in] length Name length of system variable
  @param [out]  source Source of system variable
  @return Status of performance operation
  @retval false Success
  @retval true Failure
*/
DEFINE_BOOL_METHOD(mysql_system_variable_source_imp::get,
  (const char* name, unsigned int length,
     enum enum_variable_source* source))
{
  try
  {
    return get_sysvar_source(name, length, source);
  }
  catch (...)
  {
    mysql_components_handle_std_exception(__func__);
  }
  return true;
}

