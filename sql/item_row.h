/* Copyright (C) 2000 MySQL AB

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

class Item_row: public Item
{
  Item **items;
  table_map used_tables_cache;
  uint arg_count;
  bool array_holder;
  bool const_item_cache;
  bool with_null;
public:
  Item_row(List<Item> &);
  Item_row(Item_row *item):
    Item(),
    items(item->items),
    used_tables_cache(item->used_tables_cache),
    arg_count(item->arg_count),
    array_holder(0), 
    const_item_cache(item->const_item_cache),
    with_null(0)
  {}

  enum Type type() const { return ROW_ITEM; };
  void illegal_method_call(const char *);
  bool is_null() { return null_value; }
  void make_field(Send_field *)
  {
    illegal_method_call((const char*)"make_field");
  };
  double val()
  {
    illegal_method_call((const char*)"val");
    return 0;
  };
  longlong val_int()
  {
    illegal_method_call((const char*)"val_int");
    return 0;
  };
  String *val_str(String *)
  {
    illegal_method_call((const char*)"val_str");
    return 0;
  };
  bool fix_fields(THD *thd, TABLE_LIST *tables, Item **ref);
  void split_sum_func(THD *thd, Item **ref_pointer_array, List<Item> &fields);
  table_map used_tables() const { return used_tables_cache; };
  bool const_item() const { return const_item_cache; };
  enum Item_result result_type() const { return ROW_RESULT; }
  void update_used_tables();
  void print(String *str);

  bool walk(Item_processor processor, byte *arg);

  uint cols() { return arg_count; }
  Item* el(uint i) { return items[i]; }
  Item** addr(uint i) { return items + i; }
  bool check_cols(uint c);
  bool null_inside() { return with_null; };
  void bring_value();
};
