/*
Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#pragma once

#include <cstddef>
#include "my_getopt.h"  // my_option
#include "mysql.h"      // MYSQL

class Client_program_options {
 public:
  virtual ~Client_program_options() = default;
  virtual bool init(int *argc_ptr, char ***argv_ptr) noexcept = 0;
  virtual bool apply(MYSQL *mysql) noexcept = 0;
  virtual bool connect(MYSQL *mysql, unsigned long client_flag) noexcept = 0;
  const char *get_last_error() noexcept { return last_error; }
  void clear_last_error() { last_error = nullptr; }

  static Client_program_options *create(
      const char *section_name, const char *copyright,
      const char *extra_args = nullptr, const my_option *opts = nullptr,
      size_t nopts = 0,
      bool (*get_one_option_user_arg)(int optid, const struct my_option *opt,
                                      char *argument) = nullptr);

 protected:
  const char *last_error{nullptr};
  inline static Client_program_options *singleton{nullptr};
  Client_program_options() = default;
  Client_program_options(const Client_program_options &) = delete;
  Client_program_options &operator=(const Client_program_options &) = delete;
  Client_program_options(Client_program_options &&) = delete;
  Client_program_options &operator=(Client_program_options &&) = delete;
};