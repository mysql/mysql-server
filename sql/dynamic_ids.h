/* Copyright (c) 2010, 2014, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DYNAMIC_ID_H

#define DYNAMIC_ID_H

#include <my_sys.h>
#include <sql_string.h>
#include "prealloced_array.h"

class Server_ids
{
public:
  Server_ids();
  ~Server_ids() { };

  Prealloced_array<ulong, 16> dynamic_ids;

  bool pack_dynamic_ids(String *buffer);
  bool unpack_dynamic_ids(char *param_dynamic_ids);
};

#endif
