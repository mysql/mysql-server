/* Copyright (c) 2000, 2017, Alibaba and/or its affiliates. All rights reserved.

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

#ifndef SQL_ITEM_SEQUENCE_FUNC_INCLUDED
#define SQL_ITEM_SEQUENCE_FUNC_INCLUDED

#include "sql/item_func.h"

/**
  Implementation of sequence function: NEXTVAL()
*/
class Item_func_nextval : public Item_int_func {
 protected:
  THD *m_thd;
  TABLE_LIST *table_list;

 public:
  Item_func_nextval(THD *thd, TABLE_LIST *table)
      : Item_int_func(), m_thd(thd), table_list(table) {}

  longlong val_int();
  const char *func_name() const { return "nextval"; }

  void fix_length_and_dec() {
    unsigned_flag = 1;
    max_length = MAX_BIGINT_WIDTH;
    maybe_null = 1;
  }
  bool const_item() const { return 0; }
};

/**
  Implementation of sequence function: CURRVAL()
*/
class Item_func_currval : public Item_int_func {
 protected:
  THD *m_thd;
  TABLE_LIST *table_list;

 public:
  Item_func_currval(THD *thd, TABLE_LIST *table)
      : Item_int_func(), m_thd(thd), table_list(table) {}

  longlong val_int();
  const char *func_name() const { return "currval"; }
  void fix_length_and_dec() {
    unsigned_flag = 1;
    max_length = MAX_BIGINT_WIDTH;
    maybe_null = 1;
  }

  bool const_item() const { return 0; }
};

#endif
