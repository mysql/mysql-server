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


/*
  OLAP implementation by Sinisa Milivojevic <sinisa@mysql.com>
  Inspired by code submitted by Srilakshmi <lakshmi@gdit.iiit.net>

  The ROLLUP code in this file has to be complitely rewritten as it's
  not good enough to satisfy the goals of MySQL.

  In 4.1 we will replace this with a working, superior implementation
  of ROLLUP.
*/

#ifdef DISABLED_UNTIL_REWRITTEN_IN_4_1

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_select.h"


/****************************************************************************
  Functions that recursively actually creates new SELECT's
  Returns 0 if OK, 1 if error, -1 if error already printed to client
****************************************************************************/


static int make_new_olap_select(LEX *lex, SELECT_LEX *select_lex, List<Item> new_fields)
{
  THD	*thd=current_thd;
  Item *item, *new_item;
  Item_null *constant= new Item_null("ALL");

  SELECT_LEX *new_select = (SELECT_LEX *) thd->memdup((char*) select_lex, sizeof(*select_lex));
  if (!new_select)
    return 1;
  lex->last_selects->next=new_select;
  new_select->linkage=OLAP_TYPE;
  new_select->olap=NON_EXISTING_ONE;
  new_select->group_list.elements=0;
  new_select->group_list.first=(byte *)0;
  new_select->group_list.next=(byte **)&new_select->group_list.first;
  List<Item> privlist;
  
  List_iterator<Item> list_it(select_lex->item_list);
  List_iterator<Item> new_it(new_fields);
    
  while ((item=list_it++))
  {
    bool not_found= TRUE;
    if (item->type()==Item::FIELD_ITEM)
    {
      Item_field *iif = (Item_field *)item;
      new_it.rewind();
      while ((new_item=new_it++))
      {
	if (new_item->type()==Item::FIELD_ITEM && 
	    !strcmp(((Item_field*)new_item)->table_name,iif->table_name) &&
	    !strcmp(((Item_field*)new_item)->field_name,iif->field_name))
	{
	  not_found= 0;
	  ((Item_field*)new_item)->db_name=iif->db_name;
	  Item_field *new_one=new Item_field(iif->db_name, iif->table_name, iif->field_name);
	  privlist.push_back(new_one);
	  if (add_to_list(new_select->group_list,new_one,1))
	    return 1;
	  break;
	}
      }
    }
    if (not_found)
    {
      if (item->type() == Item::FIELD_ITEM)
	privlist.push_back(constant);
      else
	privlist.push_back((Item*)thd->memdup((char *)item,item->size_of()));
    }
  }
  new_select->item_list=privlist;

  lex->last_selects = new_select;
  return 0;
}

/****************************************************************************
  Functions that recursively creates combinations of queries for OLAP
  Returns 0 if OK, 1 if error, -1 if error already printed to client
****************************************************************************/

static int  olap_combos(List<Item> old_fields, List<Item> new_fields, Item *item, LEX *lex, 
			      SELECT_LEX *select_lex, int position, int selection, int num_fields, 
			      int num_new_fields)
{
  int sl_return = 0;
  if (position == num_new_fields)
  {
    if (item)
      new_fields.push_front(item);
    sl_return = make_new_olap_select(lex, select_lex, new_fields);
  }
  else
  {
    if (item)
      new_fields.push_front(item);
    while ((num_fields - num_new_fields >= selection - position) && !sl_return)
    {
      item = old_fields.pop();
      sl_return = olap_combos(old_fields, new_fields, item, lex, select_lex, position+1, ++selection, num_fields, num_new_fields);
    }
  }
  return sl_return;
}


/****************************************************************************
  Top level function for converting OLAP clauses to multiple selects
  This is also a place where clauses treatment depends on OLAP type 
  Returns 0 if OK, 1 if error, -1 if error already printed to client
****************************************************************************/

int handle_olaps(LEX *lex, SELECT_LEX *select_lex)
{
  List<Item> item_list_copy, new_item_list;
  item_list_copy.empty();
  new_item_list.empty();
  int count=select_lex->group_list.elements;
  int sl_return=0;


  lex->last_selects=select_lex;

  for (ORDER *order=(ORDER *)select_lex->group_list.first ; order ; order=order->next)
    item_list_copy.push_back(*(order->item));

  List<Item>	all_fields(select_lex->item_list);


  if (setup_tables(lex->thd, (TABLE_LIST *)select_lex->table_list.first
                   &select_lex->where, &select_lex->leaf_tables, 0) ||
      setup_fields(lex->thd, 0, (TABLE_LIST *)select_lex->table_list.first,
		   select_lex->item_list, 1, &all_fields,1) ||
      setup_fields(lex->thd, 0, (TABLE_LIST *)select_lex->table_list.first,
		   item_list_copy, 1, &all_fields, 1))
    return -1;

  if (select_lex->olap == CUBE_TYPE)
  {
    for ( int i=count-1; i>=0 && !sl_return; i--)
      sl_return=olap_combos(item_list_copy, new_item_list, (Item *)0, lex, select_lex, 0, 0, count, i);
  }
  else if (select_lex->olap == ROLLUP_TYPE)
  {
    for ( int i=count-1; i>=0 && !sl_return; i--)
    {
      Item *item;
      item_list_copy.pop();
      List_iterator<Item> it(item_list_copy);
      new_item_list.empty();
      while ((item = it++))
	new_item_list.push_front(item);
      sl_return=make_new_olap_select(lex, select_lex, new_item_list);
    }
  }
  else
    sl_return=1; // impossible
  return sl_return;
}

#endif /* DISABLED_UNTIL_REWRITTEN_IN_4_1 */
