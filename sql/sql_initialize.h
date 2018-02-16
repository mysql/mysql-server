/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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
  Compiled_in_command_iterator() : is_active(false) {}
  virtual ~Compiled_in_command_iterator() { end(); }
  void begin(void);
  int next(std::string &query, int *read_error, int *iterator_type);
  void end(void);

 private:
  bool is_active;
};

extern bool opt_initialize_insecure;
bool initialize_create_data_directory(const char *data_home);

#endif /* SQL_INITIALIZE_H */
