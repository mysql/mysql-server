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
  bool array_holder;
  table_map tables;
  uint arg_count;
  Item **items;
public:
  Item_row(List<Item> &);
  Item_row(Item_row *item):
    Item(), array_holder(0), tables(item->tables), arg_count(item->arg_count),
    items(item->items)
  {}

  ~Item_row()
  {
    if(array_holder && items)
      sql_element_free(items);
  }

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
  table_map used_tables() const { return tables; };
  enum Item_result result_type() const { return ROW_RESULT; }

  virtual uint cols() { return arg_count; }
  virtual Item* el(uint i) { return items[i]; }
  virtual bool check_cols(uint c);
};
