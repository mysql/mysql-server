/* Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#ifndef SQL_INITIALIZE_H
#define SQL_INITIALIZE_H 1

#include <string>

#include "sql/bootstrap_impl.h"

class Compiled_in_command_iterator : public bootstrap::Command_iterator {
 public:
  Compiled_in_command_iterator() = default;
  virtual ~Compiled_in_command_iterator() = default;
  bool begin(void) override;
  int next(std::string &query) override;
  void report_error_details(log_function_t log) override;
  void end(void) override;

 private:
  int m_cmds_ofs{0};
  int m_cmd_ofs{0};
};

extern bool opt_initialize_insecure;
bool initialize_create_data_directory(const char *data_home);
extern bool mysql_initialize_directory_freshly_created;

/* Declarations below are for unit testing. */
extern bool generate_password(char *password, int size);

#define ALLOWED_PWD_UPCHARS "QWERTYUIOPASDFGHJKLZXCVBNM"
#define ALLOWED_PWD_LOWCHARS "qwertyuiopasdfghjklzxcvbnm"
#define ALLOWED_PWD_NUMCHARS "1234567890"
#define ALLOWED_PWD_SYMCHARS ",.-+*;:_!#%&/()=?><"

static constexpr const char g_allowed_pwd_chars[] =
    ALLOWED_PWD_LOWCHARS ALLOWED_PWD_SYMCHARS ALLOWED_PWD_UPCHARS
        ALLOWED_PWD_NUMCHARS;
static constexpr const char g_upper_case_chars[] = ALLOWED_PWD_UPCHARS;
static constexpr const char g_lower_case_chars[] = ALLOWED_PWD_LOWCHARS;
static constexpr const char g_numeric_chars[] = ALLOWED_PWD_NUMCHARS;
static constexpr const char g_special_chars[] = ALLOWED_PWD_SYMCHARS;

#endif /* SQL_INITIALIZE_H */
