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
#include "sql_select.h"

/* Union  of selects */


int mysql_union(THD *thd,LEX *lex,uint no_of_selects) 
{
  SELECT_LEX *sl, *for_order=&lex->select_lex;  int res=0;
  TABLE *table=(TABLE *)NULL; TABLE_LIST *resulting=(TABLE_LIST *)NULL;
  for (;for_order->next;for_order=for_order->next);
  ORDER *some_order = (ORDER *)for_order->order_list.first;
	List<Item> list;
	List_iterator<Item> it(lex->select_lex.item_list);
	Item *item;
	TABLE_LIST *s=(TABLE_LIST*) lex->select_lex.table_list.first;
	while ((item= it++))
		if (list.push_back(item))
			return -1;
	if (setup_fields(thd,s,list,0,0))
		return -1;
	TMP_TABLE_PARAM *tmp_table_param= new TMP_TABLE_PARAM;
  count_field_types(tmp_table_param,list,0);
	tmp_table_param->end_write_records= HA_POS_ERROR; tmp_table_param->copy_field=0;
  tmp_table_param->copy_field_count=tmp_table_param->field_count=
    tmp_table_param->sum_func_count= tmp_table_param->func_count=0;
  if (!(table=create_tmp_table(thd, tmp_table_param, list, (ORDER*) 0, !lex->union_option,
			       0, 0, lex->select_lex.options | thd->options)))
    return 1;
	if (!(resulting = (TABLE_LIST *) thd->calloc(sizeof(TABLE_LIST))))
		return 1;
	resulting->db=s->db ? s->db : thd->db;
	resulting->real_name=table->real_name;
	resulting->name=table->table_name;
	resulting->table=table;

  for (sl=&lex->select_lex;sl;sl=sl->next)
  {
    TABLE_LIST *tables=(TABLE_LIST*) sl->table_list.first;
		select_insert *result;
		if ((result=new select_insert(table,&list, DUP_IGNORE, true)))
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
			if (res) 
				return res;
		}
		else
			return -1;
	}
  select_result *result;
  List<Item_func_match> ftfunc_list;
  ftfunc_list.empty();
  if (lex->exchange)
  {
    if (lex->exchange->dumpfile)
      result=new select_dump(lex->exchange);
    else
      result=new select_export(lex->exchange);
  }
  else result=new select_send();
  if (result)
  {
    res=mysql_select(thd,resulting,list,
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
		delete result;
  }
	else
		res=-1;
  return res;
}
