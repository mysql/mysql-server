/* Copyright (c) 2006, 2015, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef SQL_UNION_INCLUDED
#define SQL_UNION_INCLUDED

#include "my_global.h"          // ulong
#include "sql_class.h"          // Query_result_interceptor

struct LEX;

typedef class st_select_lex_unit SELECT_LEX_UNIT;

class Query_result_union :public Query_result_interceptor
{
  Temp_table_param tmp_table_param;
public:
  TABLE *table;
  bool is_union_mixed_with_union_all; // Mark the mixed operation

  Query_result_union() :table(0),
  is_union_mixed_with_union_all(false){}
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  /**
    Do prepare() and prepare2() if they have been postponed until
    column type information is computed (used by Query_result_union_direct).

    @param types Column types

    @return false on success, true on failure
  */
  virtual bool postponed_prepare(List<Item> &types)
  { return false; }
  bool send_data(List<Item> &items);
  bool send_eof();
  virtual bool flush();
  void cleanup();
  bool create_result_table(THD *thd, List<Item> *column_types,
                           bool is_distinct, ulonglong options,
                           const char *alias, bool bit_fields_as_long,
                           bool create_table);
  friend bool TABLE_LIST::create_derived(THD *thd);
};

#endif /* SQL_UNION_INCLUDED */
