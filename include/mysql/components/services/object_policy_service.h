/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#ifndef OBJECT_POLICY_SERVICE_H_INCLUDED
#define OBJECT_POLICY_SERVICE_H_INCLUDED

#include <mysql/components/service.h>
#include <mysql/components/services/mysql_current_thread_reader.h>
#include <mysql/components/services/mysql_string.h>

#include <mysql/components/services/bits/object_policy_bits.h>

BEGIN_SERVICE_DEFINITION(column_masking_policy_management)

/**
    Create a new policy

    @param [in]   thd                         Current thread
    @param [in]   policy_name                 Name of the policy
    @param [in]   argument                    Name of the argument used as
                                              placeholder for column
    @param [in]   extra_information           Extra information
    @param [out]  message_buffer              Buffer used to pass information

    @returns Status of the operation
      @retval 0 Success
      @retval 1 Failure
  */
DECLARE_BOOL_METHOD(create,
                    (MYSQL_THD thd, my_h_string policy_name, bool replace,
                     my_h_string expression, my_h_string argument,
                     my_h_string extra_information,
                     my_h_string message_buffer));

/**
  Remove an existing policy

  @param [in]   thd                        Current thread
  @param [in]   policy_name                Name of the policy
  @param [out]  message_buffer             Buffer used to pass information

  @returns Status of the operation
    @retval 0 Success
    @retval 1 Failure
*/
DECLARE_BOOL_METHOD(drop, (MYSQL_THD thd, my_h_string policy_name,
                           my_h_string message_buffer));
END_SERVICE_DEFINITION(column_masking_policy_management)

BEGIN_SERVICE_DEFINITION(column_masking_policy_retrieval)

/**
  Get policy details

  @param [in]   thd                         Current thread
  @param [out]  expression                  Expression in UTF8MB4
  @param [out]  argument                    Placeholder name
  @param [out]  extra_information           Extra information
  @param [out]  message_buffer              Buffer used to pass information

  @returns Status of the operation
    @retval 0 Success
    @retval 1 Failure
*/
DECLARE_BOOL_METHOD(get, (MYSQL_THD thd, my_h_string policy_name,
                          my_h_string expression, my_h_string argument,
                          my_h_string extra_information,
                          my_h_string message_buffer));
END_SERVICE_DEFINITION(column_masking_policy_retrieval)
#endif /* OBJECT_POLICY_SERVICE_H_INCLUDED */
