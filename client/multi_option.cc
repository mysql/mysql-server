/*
   Copyright (c) 2012, 2023, Oracle and/or its affiliates.
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
