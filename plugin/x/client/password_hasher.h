/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef X_CLIENT_PASSWORD_HASHER_H_
#define X_CLIENT_PASSWORD_HASHER_H_

#include <cstdint>
#include <string>


namespace xcl {
namespace password_hasher {

char *octet2hex(char *to, const char *str, size_t len);
std::string generate_user_salt();
std::string scramble(const char *message, const char *password);
bool check_scramble_mysql41_hash(const char *scramble_arg,
                                 const char *message,
                                 const uint8_t *hash_stage2);
std::string get_password_from_salt(const std::string &hash_stage2);

}  // namespace password_hasher
}  // namespace xcl

#endif  // X_CLIENT_PASSWORD_HASHER_H_
