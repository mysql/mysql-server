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

/* subselect Item */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

struct st_select_lex;
class JOIN;
class select_subselect;

/* simple (not depended of covered select ) subselect */

class Item_subselect :public Item
{
protected:
  longlong int_value;
  double real_value;
  my_bool executed; /* simple subselect is executed */
  my_bool optimized; /* simple subselect is optimized */
  my_bool error; /* error in query */
  enum Item_result res_type;

  int exec();
  void assign_null() 
  {
    null_value= 1;
    int_value= 0;
    real_value= 0;
    max_length= 4;
    res_type= STRING_RESULT;
  }
public:
  st_select_lex *select_lex;
  JOIN *join;
  select_subselect *result;

  Item_subselect(THD *thd, st_select_lex *select_lex);
  Item_subselect(Item_subselect *item)
  {
    null_value= item->null_value;
    int_value= item->int_value;
    real_value= item->real_value;
    max_length= item->max_length;
    decimals= item->decimals;
    res_type= item->res_type;
    executed= item->executed;
    select_lex= item->select_lex;
    join= item->join;
    result= item->result;
    name= item->name;
    error= item->error;
  }
  enum Type type() const;
  double val ();
  longlong val_int ();
  String *val_str (String *);
  bool is_null() { return null_value; }
  void make_field (Send_field *);
  bool fix_fields(THD *thd, TABLE_LIST *tables);
  Item *new_item() { return new Item_subselect(this); }
  enum Item_result result_type() const { return res_type; }

  friend class select_subselect;
};


