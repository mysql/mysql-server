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

#ifndef X_TESTS_DRIVER_MYSQLX_CHARSET_H_
#define X_TESTS_DRIVER_MYSQLX_CHARSET_H_

#include <cstdint>
#include <string>


namespace xcl {

class Charset {
 public:
  static std::string charset_name_from_id(uint32_t id);
  static std::string collation_name_from_id(uint32_t id);
  static uint32_t id_from_collation_name(const std::string& collation_name);

 private:
  typedef struct {
    uint32_t id;
    std::string name;
    std::string collation;
  } Charset_entry;

  static const Charset_entry m_charsets_info[];

  static std::string field_from_id(uint32_t id,
                                   std::string Charset_entry::*field);
};

}  // namespace xcl

#endif  //  X_TESTS_DRIVER_MYSQLX_CHARSET_H_
