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

#ifndef VALIDATE_PASSWORD_SERVICE_H
#define VALIDATE_PASSWORD_SERVICE_H

#include <mysql/components/service.h>
#include <mysql/components/services/mysql_string.h>

/**
  This component defines interface that the server uses to enforce a
  password policy.

  The policy is enfoced through two methods
  1) validate_password_imp::validate() that answers the question of whether
     this password is good enough or not.

  2) validate_password_imp::get_strength() that can be used by password
     changing UIs to display a password strength meter in the range of [0-100]
     as the user enters a password.
*/
BEGIN_SERVICE_DEFINITION(validate_password)
  /**
    Checks if a password is valid by the password policy.

    @param thd MYSQL THD object
    @param password Given Password
    @return Status of performed operation
    @return false success (valid password)
    @return true failure (invalid password)
  */
  DECLARE_BOOL_METHOD(validate,
                      (void *thd, my_h_string password));

  /**
    Calculates the strength of a password in the scale of 0 to 100.

    @param thd MYSQL THD object
    @param password Given Password
    @param [out] strength pointer to handle the strength of the given password.
                 in the range of [0-100], where 0 is week password and
                 100 is strong password
    @return Status of performed operation
    @return false success
    @return true failure
  */
  DECLARE_BOOL_METHOD(get_strength,
                      (void *thd, my_h_string password,
                       unsigned int *strength));

END_SERVICE_DEFINITION(validate_password)

#endif /* VALIDATE_PASSWORD_SERVICE_H */
