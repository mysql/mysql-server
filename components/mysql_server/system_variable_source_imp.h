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

#ifndef SYSTEM_VARIABLE_SOURCE_IMP_H
#define SYSTEM_VARIABLE_SOURCE_IMP_H

#include <mysql/components/services/system_variable_source.h>
#include <mysql/components/service_implementation.h>

/**
  An implementation of the service method to give the source of given
  system variable.
*/

class mysql_system_variable_source_imp
{
public:
  /**
    Get source information of given system variable.

    @param [in] name Name of system variable
    @param [in] length Name length of system variable
    @param [out]  source Source of system variable
    @return Status of performance operation
    @retval false Success
    @retval true Failure
  */

  static DEFINE_BOOL_METHOD(get,
  (const char* name, unsigned int length,
     enum enum_variable_source* source));

};
#endif /* SYSTEM_VARIABLE_SOURCE_IMP_H */
