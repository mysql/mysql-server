/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _PASSWORD_HASHER_H_
#define _PASSWORD_HASHER_H_

#include <string>
#include <stdint.h>

class Password_hasher
{
public:
  static std::string generate_user_salt();
  static std::string compute_password_hash(const std::string &password);
  static std::string scramble(const char *message, const char *password);
  static bool        check_scramble_mysql41_hash(const char *scramble_arg, const char *message, const uint8_t *hash_stage2);
  static std::string get_salt_from_password(const std::string &password_hash);
  static std::string get_password_from_salt(const std::string &hash_stage2);

private:
  static void hex2octet(char *to, const char *str, size_t len);
  static char *octet2hex(char *to, const char *str, size_t len);
  static void compute_two_stage_mysql41_hash(const char *password, size_t pass_len, uint8_t *hash_stage1, uint8_t *hash_stage2);
  static void my_crypt(char *to, const uint8_t *s1, const uint8_t *s2, size_t len);

  static const char *_dig_vec_upper;
};

#endif
