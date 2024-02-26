/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

/*
  This is a test executable which does nothing, except verify that symbols are
  visible. See the accompanying cmake rules.
  Instantiated Value objects are bad, running this executable will fail.
 */
#include <stdlib.h>

#include <string>

#include "sql-common/json_binary.h"

int main() {
  json_binary::Value value = json_binary::parse_binary(nullptr, 0);

  bool is_valid = value.is_valid();
  json_binary::Value::enum_type enum_type = value.type();

  value = json_binary::Value(json_binary::Value::OBJECT, nullptr, 0, 1, false);

  json_binary::Value elt1 = value.element(1);
  json_binary::Value key1 = value.key(1);

  size_t sz1 = value.lookup_index("foo", 3);
  size_t sz2 = value.lookup_index(std::string("foo"));

  bool has_space = value.has_space(0, 0, nullptr);

  json_binary::Value null_value =
      json_binary::Value(json_binary::Value::LITERAL_NULL);

  json_binary::Value int_value =
      json_binary::Value(json_binary::Value::INT, 42);

  json_binary::Value double_value = json_binary::Value(0.0);

  json_binary::Value string_value = json_binary::Value("foo", 3);

  std::string string_buf;
  string_value.to_std_string(&string_buf, [] { assert(false); });
  string_value.to_pretty_std_string(&string_buf, [] { assert(false); });
  return 0;
}
