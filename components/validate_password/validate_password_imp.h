/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef VALIDATE_PASSWORD_IMP_H
#define VALIDATE_PASSWORD_IMP_H

#include <mysql/components/component_implementation.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/component_status_var_service.h>
#include <mysql/components/services/component_sys_var_service.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/components/services/mysql_string.h>
#include <mysql/components/services/psi_memory_service.h>
#include <mysql/components/services/security_context.h>
#include <mysql/components/services/validate_password.h>

extern REQUIRES_SERVICE_PLACEHOLDER(registry);
extern REQUIRES_SERVICE_PLACEHOLDER(log_builtins);
extern REQUIRES_SERVICE_PLACEHOLDER(log_builtins_string);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_string_factory);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_string_case);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_string_converter);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_string_iterator);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_string_ctype);
extern REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_register);
extern REQUIRES_SERVICE_PLACEHOLDER(component_sys_variable_unregister);
extern REQUIRES_SERVICE_PLACEHOLDER(status_variable_registration);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_thd_security_context);
extern REQUIRES_SERVICE_PLACEHOLDER(mysql_security_context_options);
extern REQUIRES_SERVICE_PLACEHOLDER(psi_memory_v1);

/**
  An implementation of the password_validation_service to validate password and
  to get its strength.
*/
class validate_password_imp {
 public:
  /**
    Validates the strength of given password.

    @sa validate_password::validate()
  */
  static DEFINE_BOOL_METHOD(validate, (void *thd, my_h_string password));

  /**
    Gets the password strength between (0-100)

    @sa validate_password::get_strength()
  */
  static DEFINE_BOOL_METHOD(get_strength, (void *thd, my_h_string password,
                                           unsigned int *strength));
};
#endif /* VALIDATE_PASSWORD_IMP_H */
