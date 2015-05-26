/* Copyright (c) 2006, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef SQL_DO_INCLUDED
#define SQL_DO_INCLUDED

#include "sql_class.h"
 
class THD;
struct LEX;

class Query_result_do :public Query_result
{
public:
  Query_result_do(THD *thd): Query_result() {}
  bool send_result_set_metadata(List<Item> &list, uint flags) { return false; }
  bool send_data(List<Item> &items);
  bool send_eof();
  virtual bool check_simple_select() const { return false; }
  void abort_result_set() {}
  virtual void cleanup() {}
};

bool mysql_do(THD *thd, LEX *lex);

#endif /* SQL_DO_INCLUDED */
