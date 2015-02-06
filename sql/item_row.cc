/* Copyright (c) 2002, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "item_row.h"

#include "sql_class.h"  // THD

Item_row::Item_row(const POS &pos, Item *head, List<Item> &tail):
  super(pos), used_tables_cache(0), not_null_tables_cache(0),
  const_item_cache(1), with_null(0)
{

  //TODO: think placing 2-3 component items in item (as it done for function)
  arg_count= 1 + tail.elements;
  items= (Item**) sql_alloc(sizeof(Item*)*arg_count);
  if (items == NULL)
  {
    arg_count= 0;
    return; // OOM
  }
  items[0]= head;
  List_iterator<Item> li(tail);
  uint i= 1;
  Item *item;
  while ((item= li++))
  {
    items[i]= item;
    i++;    
  }
}

Item_row::Item_row(Item *head, List<Item> &tail):
  used_tables_cache(0), not_null_tables_cache(0),
  const_item_cache(1), with_null(0)
{

  //TODO: think placing 2-3 component items in item (as it done for function)
  arg_count= 1 + tail.elements;
  items= (Item**) sql_alloc(sizeof(Item*)*arg_count);
  if (items == NULL)
  {
    arg_count= 0;
    return; // OOM
  }
  items[0]= head;
  List_iterator<Item> li(tail);
  uint i= 1;
  Item *item;
  while ((item= li++))
  {
    items[i]= item;
    i++;    
  }
}

bool Item_row::itemize(Parse_context *pc, Item **res)
{
  if (skip_itemize(res))
    return false;
  if (super::itemize(pc, res))
    return true;
  for (uint i= 0; i < arg_count; i++)
  {
    if (items[i]->itemize(pc, &items[i]))
      return true;
  }
  return false;
}


void Item_row::illegal_method_call(const char *method)
{
  DBUG_ENTER("Item_row::illegal_method_call");
  DBUG_PRINT("error", ("!!! %s method was called for row item", method));
  DBUG_ASSERT(0);
  my_error(ER_OPERAND_COLUMNS, MYF(0), 1);
  DBUG_VOID_RETURN;
}

bool Item_row::fix_fields(THD *thd, Item **ref)
{
  DBUG_ASSERT(fixed == 0);
  null_value= 0;
  maybe_null= 0;
  Item **arg, **arg_end;
  for (arg= items, arg_end= items+arg_count; arg != arg_end ; arg++)
  {
    if ((!(*arg)->fixed && (*arg)->fix_fields(thd, arg)))
      return TRUE;
    // we can't assign 'item' before, because fix_fields() can change arg
    Item *item= *arg;
    used_tables_cache |= item->used_tables();
    const_item_cache&= item->const_item() && !with_null;
    not_null_tables_cache|= item->not_null_tables();

    if (const_item_cache)
    {
      if (item->cols() > 1)
	with_null|= item->null_inside();
      else
      {
	if (item->is_null())
          with_null|= 1;
      }
    }
    maybe_null|= item->maybe_null;
    with_sum_func|= item->with_sum_func;
    with_subselect|= item->has_subquery();
  }
  fixed= 1;
  return FALSE;
}


void Item_row::cleanup()
{
  DBUG_ENTER("Item_row::cleanup");

  Item::cleanup();
  /* Reset to the original values */
  used_tables_cache= 0;
  const_item_cache= 1;
  with_null= 0;

  DBUG_VOID_RETURN;
}


void Item_row::split_sum_func(THD *thd, Ref_ptr_array ref_pointer_array,
                              List<Item> &fields)
{
  Item **arg, **arg_end;
  for (arg= items, arg_end= items+arg_count; arg != arg_end ; arg++)
    (*arg)->split_sum_func2(thd, ref_pointer_array, fields, arg, TRUE);
}


void Item_row::update_used_tables()
{
  used_tables_cache= 0;
  const_item_cache= true;
  with_subselect= false;
  with_stored_program= false;
  for (uint i= 0; i < arg_count; i++)
  {
    items[i]->update_used_tables();
    used_tables_cache|= items[i]->used_tables();
    const_item_cache&= items[i]->const_item();
    with_subselect|= items[i]->has_subquery();
    with_stored_program|= items[i]->has_stored_program();
  }
}

void Item_row::fix_after_pullout(st_select_lex *parent_select,
                                 st_select_lex *removed_select)
{
  used_tables_cache= 0;
  not_null_tables_cache= 0;
  const_item_cache= true;
  for (uint i= 0; i < arg_count; i++)
  {
    items[i]->fix_after_pullout(parent_select, removed_select);
    used_tables_cache|= items[i]->used_tables();
    not_null_tables_cache|= items[i]->not_null_tables();
    const_item_cache&= items[i]->const_item();
  }
}

bool Item_row::check_cols(uint c)
{
  if (c != arg_count)
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), c);
    return 1;
  }
  return 0;
}

void Item_row::print(String *str, enum_query_type query_type)
{
  str->append('(');
  for (uint i= 0; i < arg_count; i++)
  {
    if (i)
      str->append(',');
    items[i]->print(str, query_type);
  }
  str->append(')');
}


bool Item_row::walk(Item_processor processor, enum_walk walk, uchar *arg)
{
  if ((walk & WALK_PREFIX) && (this->*processor)(arg))
    return true;

  for (uint i= 0; i < arg_count; i++)
  {
    if (items[i]->walk(processor, walk, arg))
      return true;
  }
  return (walk & WALK_POSTFIX) && (this->*processor)(arg);
}


Item *Item_row::transform(Item_transformer transformer, uchar *arg)
{
  DBUG_ASSERT(!current_thd->stmt_arena->is_stmt_prepare());

  for (uint i= 0; i < arg_count; i++)
  {
    Item *new_item= items[i]->transform(transformer, arg);
    if (!new_item)
      return 0;

    /*
      THD::change_item_tree() should be called only if the tree was
      really transformed, i.e. when a new item has been created.
      Otherwise we'll be allocating a lot of unnecessary memory for
      change records at each execution.
    */
    if (items[i] != new_item)
      current_thd->change_item_tree(&items[i], new_item);
  }
  return (this->*transformer)(arg);
}

void Item_row::bring_value()
{
  for (uint i= 0; i < arg_count; i++)
    items[i]->bring_value();
}
