/*
   Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef CLIENT_QUERY_ATTRIBUTES_H
#define CLIENT_QUERY_ATTRIBUTES_H

#include "mysql.h"

class client_query_attributes {
 public:
  client_query_attributes() = default;
  ~client_query_attributes() { clear(); }
  bool push_param(char *name, char *value);
  int set_params(MYSQL *mysql);

  void clear(MYSQL *mysql = nullptr);

 private:
  /* 32 should be enough for everybody */
  static constexpr int max_count = 32;
  const char *names[max_count];
  MYSQL_BIND values[max_count];
  unsigned count{0};
};

extern client_query_attributes *global_attrs;

#endif