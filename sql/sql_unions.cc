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


/* UNION  of select's */

/* UNION's  were introduced by Monty and Sinisa <sinisa@mysql.com> */


#include "mysql_priv.h"


/* Union  of selects */


int mysql_union(THD *thd,LEX *lex,uint no_of_selects) 
{
  SELECT_LEX *sl, *for_order=&lex->select_lex; uint no=0; int res=0;
  List<Item> fields;     TABLE *table=(TABLE *)NULL; TABLE_LIST *resulting=(TABLE_LIST *)NULL;
  for (;for_order->next;for_order=for_order->next);
  ORDER *some_order = (ORDER *)for_order->order_list.first;
  for (sl=&lex->select_lex;sl;sl=sl->next, no++)
  {
    TABLE_LIST *tables=(TABLE_LIST*) sl->table_list.first;
    if (!no) // First we do CREATE from SELECT
    {
      select_create *result;
      lex->create_info.options=HA_LEX_CREATE_TMP_TABLE;
      if ((result=new select_create(tables->db ? tables->db : thd->db,
				    NULL, &lex->create_info,
				    lex->create_list,
				    lex->key_list,
				    sl->item_list,DUP_IGNORE)))
      {
	res=mysql_select(thd,tables,sl->item_list,
			 sl->where,
			 sl->ftfunc_list,
			 (ORDER*) NULL,
			 (ORDER*) sl->group_list.first,
			 sl->having,
			 (ORDER*) some_order,
			 sl->options | thd->options,
			 result);
	if (res) 
	{
	  result->abort();
	  delete result;
	  return res;
	}
	table=result->table;
	List_iterator<Item> it(*(result->fields));
	Item *item;
	while ((item= it++))
	  fields.push_back(item);
	delete result;
	if (!reopen_table(table)) return 1;
	if (!(resulting = (TABLE_LIST *) thd->calloc(sizeof(TABLE_LIST))))
	  return 1;
	resulting->db=tables->db ? tables->db : thd->db;
	resulting->real_name=table->real_name;
	resulting->name=table->table_name;
	resulting->table=table;
      }
      else
	return -1;
    }
    else // Then we do INSERT from SELECT
    {
      select_result *result;
      if ((result=new select_insert(table, &fields, DUP_IGNORE)))
      {
	res=mysql_select(thd,tables,sl->item_list,
			 sl->where,
                         sl->ftfunc_list,
			 (ORDER*) some_order,
			 (ORDER*) sl->group_list.first,
			 sl->having,
			 (ORDER*) NULL,
			 sl->options | thd->options,
			 result);
	delete result;
	if (res) return 1;
      }
      else
	return -1;
    }
  }
  select_result *result;
  List<Item> item_list;
  List<Item_func_match> ftfunc_list;
  ftfunc_list.empty();
  if (item_list.push_back(new Item_field(NULL,NULL,"*")))
    return -1;
  if (lex->exchange)
  {
    if (lex->exchange->dumpfile)
    {
      if (!(result=new select_dump(lex->exchange)))
	return -1;
    }
    else
    {
      if (!(result=new select_export(lex->exchange)))
	return -1;
    }
  }
  else if (!(result=new select_send()))
    return -1;
  else
  {
    res=mysql_select(thd,resulting,item_list,
		     NULL,
		     ftfunc_list,
		     (ORDER*) NULL,
		     (ORDER*) NULL,
		     NULL,
		     (ORDER*) NULL,
		     thd->options,
		     result);
    if (res)
      result->abort();
  }
  delete result;
  return res;
}
