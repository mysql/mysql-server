/* Copyright (c) 2006, 2024, Oracle and/or its affiliates.

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

#ifndef PASSWORD_INCLUDED
#define PASSWORD_INCLUDED

/**
  @file include/password.h
*/

#include "config.h"

#if !defined(WITHOUT_MYSQL_NATIVE_PASSWORD) || \
    WITHOUT_MYSQL_NATIVE_PASSWORD == 0
#include <stddef.h>
#include <sys/types.h>
#include "my_macros.h"
void my_make_scrambled_password_sha1(char *to, const char *password,
                                     size_t pass_len);
/*
  These functions are used for authentication by client and
  implemented in sql-common/mysql_native_password_client.cc
*/

void make_scrambled_password(char *to, const char *password);
void scramble(char *to, const char *message, const char *password);
bool check_scramble(const unsigned char *reply, const char *message,
                    const unsigned char *hash_stage2);
void get_salt_from_password(unsigned char *res, const char *password);
void make_password_from_salt(char *to, const unsigned char *hash_stage2);
#endif

#endif /* PASSWORD_INCLUDED */
