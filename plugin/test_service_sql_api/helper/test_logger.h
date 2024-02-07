/*
 * Copyright (c) 2020, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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
#ifndef PLUGIN_TEST_SERVICE_SQL_API_HELPER_TEST_LOGGER_H_
#define PLUGIN_TEST_SERVICE_SQL_API_HELPER_TEST_LOGGER_H_

#include <string>

#include "my_io.h"   // NOLINT(build/include_subdir)
#include "my_sys.h"  // NOLINT(build/include_subdir)

class Test_logger {
 public:
  explicit Test_logger(const char *log_name) {
    char filename[FN_REFLEN];

    fn_format(filename, log_name, "", ".log",
              MY_REPLACE_EXT | MY_UNPACK_FILENAME);

    my_delete(filename, 0);

    m_out_file = my_open(filename, O_CREAT | O_RDWR, MYF(0));
  }

  ~Test_logger() { my_close(m_out_file, MYF(0)); }

  void print_to_file(const std::string &text) const {
    my_write(m_out_file, reinterpret_cast<const uchar *>(text.c_str()),
             text.length(), MYF(0));
  }

 private:
  File m_out_file;
};

#endif  // PLUGIN_TEST_SERVICE_SQL_API_HELPER_TEST_LOGGER_H_
