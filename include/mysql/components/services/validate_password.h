/* Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#ifndef VALIDATE_PASSWORD_SERVICE_H
#define VALIDATE_PASSWORD_SERVICE_H

#include <mysql/components/service.h>
#include <mysql/components/services/mysql_string.h>

/**
  @ingroup group_components_services_inventory

  Interfaces to enforce a password policy.

  The policy is enfoced through two methods
  1) validate() that answers the question of whether this password is good
     enough or not.

  2) get_strength() that can be used by password changing UIs to display
     a password strength meter in the range of [0-100] as the user enters
     a password.

  @code
    REQUIRES_SERVICE(validate_password);
    bool validate_password(THD *thd, const char *password,
                           unsigned int password_length) {
      String password_string;
      password_string.set(password, password_length, &my_charset_utf8mb3_bin);
      if (mysql_service_validate_password->validate(thd, password_string)) {
        // Emit error that password does not adhere to policy criteria
        return true;
      }
      return false;
    }

    unsigned int get_password_strength(THD *thd, const char *password,
                                       unsigned int password_length) {
      String password_string;
      password_string.set(password, password_length, &my_charset_utf8mb3_bin);
      unsigned int strength = 0;
      if (mysql_service_validate_password->get_strength(thd, password_string,
                                                    &strength)) {
        return 0;
      }
      return strength;
    }
  @endcode
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
DECLARE_BOOL_METHOD(validate, (void *thd, my_h_string password));

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
                    (void *thd, my_h_string password, unsigned int *strength));

END_SERVICE_DEFINITION(validate_password)

/**
  @ingroup group_components_services_inventory

  Service to enforce that new password contains N different characters
  compared to existing password.

  @code
  REQUIRES_SERVICE(validate_password_changed_characters)
  bool compare_passwords(const char *current_password,
                         unsigned int current_password_length,
                         const char * new_oassword,
                         unsigned int new_password_length) {
    String current_password_string, new_password_string;
    current_password_string.assign(current_password, current_password_length,
                                   &my_charset_utf8mb3_bin);
    new_password_string.assign(new_password, new_password_length,
                               &my_charset_utf8mb3_bin);
    unsigned int min_required = 0, changed = 0;
    if (mysql_service_validate_password_changed_characters->validate(
          current_password_string, new_password_string,
          &min_required, &changed)) {
      // Raise error that min_required characters should be changed
      return true;
    }
    return false;
  }
  @endcode
*/

BEGIN_SERVICE_DEFINITION(validate_password_changed_characters)

/**
  Validate if number of changed characters matches the pre-configured
  criteria

  @param [in]  current_password Current password
  @param [in]  new_password     New password
  @param [out] minimum_required Minimum required number of changed characters
  @param [out] changed          Actual number of changed characters

  @returns Result of validation
    @retval false Success
        @retval true  Error
*/
DECLARE_BOOL_METHOD(validate,
                    (my_h_string current_password, my_h_string new_password,
                     uint *minimum_required, uint *changed));

END_SERVICE_DEFINITION(validate_password_changed_characters)

#endif /* VALIDATE_PASSWORD_SERVICE_H */
