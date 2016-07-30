/* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_INITIALIZE_H
#define SQL_INITIALIZE_H 1

#include "bootstrap_impl.h"

class Compiled_in_command_iterator : public Command_iterator
{
public:
  Compiled_in_command_iterator() : is_active(false)
  {}
  virtual ~Compiled_in_command_iterator()
  {
    end();
  }
  void begin(void);
  int next(std::string &query, int *read_error, int *iterator_type);
  void end(void);
private:
  bool is_active;
};

extern my_bool opt_initialize_insecure;
bool initialize_create_data_directory(const char *data_home);

#endif /* SQL_INITIALIZE_H */
