/*
   Copyright (c) 2012, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "multi_option.h"

void Multi_option::add_value(char *value, bool clear) {
  if (option_values == nullptr) {
    option_values = reinterpret_cast<Multi_option_container *>(my_malloc(
        PSI_NOT_INSTRUMENTED, sizeof(Multi_option_container), MYF(MY_WME)));
    // in a rare case when the allocation fails
    if (option_values == nullptr) return;
    new (option_values) Multi_option_container(PSI_NOT_INSTRUMENTED);
  } else if (clear)
    option_values->clear();
  option_values->emplace_back(value);
}

void Multi_option::set_mysql_options(MYSQL *mysql, mysql_option option) {
  if (option_values)
    for (auto const &init_command : *option_values)
      mysql_options(mysql, option, init_command);
}

void Multi_option::free() {
  if (option_values != nullptr) {
    option_values->~Multi_option_container();
    my_free(option_values);
    option_values = nullptr;
  }
}
