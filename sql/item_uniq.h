/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Compability file ; This file only contains dummy functions */

#ifdef __GNUC__
#pragma interface
#endif

#include <queues.h>

class Item_func_unique_users :public Item_real_func
{
public:
  Item_func_unique_users(Item *name_arg,int start,int end,List<Item> &list)
    :Item_real_func(list) {}
  double val() { DBUG_ASSERT(fixed == 1); return 0.0; }
  void fix_length_and_dec() { decimals=0; max_length=6; }
  void print(String *str) { str->append("0.0", 3); }
};


class Item_sum_unique_users :public Item_sum_num
{
public:
  Item_sum_unique_users(Item *name_arg,int start,int end,Item *item_arg)
    :Item_sum_num(item_arg) {}
  Item_sum_unique_users(THD *thd, Item_sum_unique_users *item)
    :Item_sum_num(thd, item) {}
  double val() { DBUG_ASSERT(fixed == 1); return 0.0; }  
  enum Sumfunctype sum_func () const {return UNIQUE_USERS_FUNC;}
  void clear() {}
  bool add() { return 0; }
  void reset_field() {}
  void update_field() {}
  bool fix_fields(THD *thd, TABLE_LIST *tlist, Item **ref)
  {
    DBUG_ASSERT(fixed == 0);
    fixed= 1;
    return FALSE;
  }
  Item *copy_or_same(THD* thd)
  {
    return new Item_sum_unique_users(thd, this);
  }
  void print(String *str) { str->append("0.0", 3); }
};
