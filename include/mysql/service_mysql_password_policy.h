/* Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_SERVICE_MYSQL_PLUGIN_AUTH_INCLUDED
#define MYSQL_SERVICE_MYSQL_PLUGIN_AUTH_INCLUDED

/**
  @file include/mysql/service_mysql_plugin_auth.h
  This service provides functions to validatete password, check for strength
  of password based on common policy.

  SYNOPSIS
  my_validate_password_policy()    - function to validate password
                                     based on defined policy
  const char*                        buffer holding the password value
  unsigned int                       buffer length

  my_calculate_password_strength() - function to calculate strength
                                     of the password based on the policies defined.
  const char*                        buffer holding the password value
  unsigned int                       buffer length

  Both the service function returns 0 on SUCCESS and 1 incase input password does not
  match against the policy rules defined.
*/

#ifdef __cplusplus
extern "C" {
#endif

extern struct mysql_password_policy_service_st {
  int (*my_validate_password_policy_func)(const char *, unsigned int);
  int (*my_calculate_password_strength_func)(const char *, unsigned int);
} *mysql_password_policy_service;

#ifdef MYSQL_DYNAMIC_PLUGIN

#define my_validate_password_policy(buffer, length) \
  mysql_password_policy_service->my_validate_password_policy_func(buffer, length)
#define my_calculate_password_strength(buffer, length) \
  mysql_password_policy_service->my_calculate_password_strength_func(buffer, length)

#else

int my_validate_password_policy(const char *, unsigned int);
int my_calculate_password_strength(const char *, unsigned int);

#endif

#ifdef __cplusplus
}
#endif

#endif
