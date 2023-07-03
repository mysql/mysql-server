/*
 * Copyright (c) 2020, 2023, Oracle and/or its affiliates.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */
#ifndef PLUGIN_TEST_SERVICE_SQL_API_HELPER_TEST_CONTEXT_H_
#define PLUGIN_TEST_SERVICE_SQL_API_HELPER_TEST_CONTEXT_H_

#include <string>

#include "mysqld_error.h"  // NOLINT(build/include_subdir)

#include "mysql/components/services/log_builtins.h"
#include "plugin/test_service_sql_api/helper/conversions.h"
#include "plugin/test_service_sql_api/helper/test_logger.h"

class Test_context {
 public:
  Test_context(const char *log_file, void *plugin_handler)
      : m_logger(log_file),
        m_separator(s_separator_length, '='),
        m_plugin_handler(plugin_handler) {}

  template <typename... Args>
  void log_test_line(const Args &... args) {
    log_test(utils::to_string(args...), "\n");
  }

  template <typename... Args>
  void log_test(const Args &... args) {
    m_logger.print_to_file(utils::to_string(args...));
  }

  void separator() { log_test_line(m_separator); }

  void separator(const char separator_character) {
    log_test_line(std::string(s_separator_length, separator_character));
  }

  template <typename... Args>
  void log_error(const Args &... args) {
    auto text = utils::to_string(args...);

    LogPluginErr(ERROR_LEVEL, ER_LOG_PRINTF_MSG, text.c_str());
  }

  void *get_plugin_handler() { return m_plugin_handler; }

 private:
  static const int s_separator_length{73};
  Test_logger m_logger;
  std::string m_separator;
  void *m_plugin_handler{nullptr};
};

#endif  // PLUGIN_TEST_SERVICE_SQL_API_HELPER_TEST_CONTEXT_H_
