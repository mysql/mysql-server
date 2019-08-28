#ifndef SQL_CRYPT_INCLUDED
#define SQL_CRYPT_INCLUDED

/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


#include "sql_alloc.h"
#include "mysql_com.h"                          /* rand_struct */

class SQL_CRYPT :public Sql_alloc
{
  struct rand_struct rand,org_rand;
  char decode_buff[256],encode_buff[256];
  uint shift;
 public:
  SQL_CRYPT() {}
  SQL_CRYPT(ulong *seed)
  {
    init(seed);
  }
  ~SQL_CRYPT() {}
  void init(ulong *seed);
  void reinit() { shift=0; rand=org_rand; }
  void encode(char *str, uint length);
  void decode(char *str, uint length);
};

#endif /* SQL_CRYPT_INCLUDED */
