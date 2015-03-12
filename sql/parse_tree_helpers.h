/* Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PARSE_TREE_HELPERS_INCLUDED
#define PARSE_TREE_HELPERS_INCLUDED

#include "item_func.h"      // Item etc.
#include "set_var.h"        // enum_var_type

typedef class st_select_lex SELECT_LEX;

/**
  Base class for parse-time Item objects

  Parse-time Item objects are placeholders for real Item objects: in some
  cases it is not easy or even possible to decide what exact Item class object
  we need to allocate in the parser. Parse-time Item objects are intended
  to defer real Item object allocation to the contextualization phase (see
  the Item::itemize() function).

  This wrapper class overrides abstract virtual functions of the parent
  class with dummy wrappers to make C++ compiler happy.
*/
class Parse_tree_item : public Item
{
public:
  explicit Parse_tree_item(const POS &pos) : Item(pos) {}

  virtual enum Type type() const { return INVALID_ITEM; }
  virtual double val_real() { DBUG_ASSERT(0); return 0; }
  virtual longlong val_int() { DBUG_ASSERT(0); return 0; }
  virtual String *val_str(String *) { DBUG_ASSERT(0); return NULL; }
  virtual my_decimal *val_decimal(my_decimal *) { DBUG_ASSERT(0); return NULL; }
  virtual bool get_date(MYSQL_TIME *, uint) { DBUG_ASSERT(0); return false; }
  virtual bool get_time(MYSQL_TIME *) { DBUG_ASSERT(0); return false; }
};


/**
  Wrapper class for an Item list head, used to allocate Item lists in the parser
  in a context-independent way
*/
class PT_item_list : public Parse_tree_node
{
  typedef Parse_tree_node super;

public:
  List<Item> value;

  virtual bool contextualize(Parse_context *pc)
  {
    if (super::contextualize(pc))
      return true;
    List_iterator<Item> it(value);
    Item *item;
    while ((item= it++))
    {
      if (item->itemize(pc, &item))
        return true;
      it.replace(item);
    }
    return false;
  }

  bool is_empty() const { return value.is_empty(); }
  uint elements() const { return value.elements; }


  bool push_back(Item *item)
  {
    /*
     Item may be NULL in case of OOM: just ignore it and check thd->is_error()
     in the caller code.
    */
    return item == NULL || value.push_back(item);
  }

  bool push_front(Item *item)
  {
    /*
     Item may be NULL in case of OOM: just ignore it and check thd->is_error()
     in the caller code.
    */
    return item == NULL || value.push_front(item);
  }

  Item *pop_front()
  {
    DBUG_ASSERT(!is_empty());
    return value.pop();
  }
};


/**
  Helper function to imitate dynamic_cast for Item_cond hierarchy

  @param To     destination type (Item_cond_and etc.)
  @param Tag    Functype tag to compare from->functype() with
  @param from   source item

  @return typecasted item of the type To or NULL
*/
template<class To, Item_func::Functype Tag>
To *item_cond_cast(Item * const from)
{
  return ((from->type() == Item::COND_ITEM &&
           static_cast<Item_func *>(from)->functype() == Tag) ?
          static_cast<To *>(from) : NULL);
}


/**
  Flatten associative operators at parse time

  This function flattens AND and OR operators at parse time if applicable,
  otherwise it creates new Item_cond_and or Item_cond_or respectively.

  @param Class  Item_cond_and or Item_cond_or
  @param Tag    COND_AND_FUNC (for Item_cond_and) or COND_OR_FUNC otherwise

  @param mem_root       MEM_ROOT
  @param pos            parse location
  @param left           left argument of the operator
  @param right          right argument of the operator

  @return resulting parse tree Item
*/
template<class Class, Item_func::Functype Tag>
Item *flatten_associative_operator(MEM_ROOT *mem_root, const POS &pos,
                                   Item *left, Item *right)
{
  if (left == NULL || right == NULL)
    return NULL;
  Class *left_func= item_cond_cast<Class, Tag>(left);
  Class *right_func= item_cond_cast<Class, Tag>(right);
  if (left_func)
  {
    if (right_func)
    {
      // (X1 op X2) op (Y1 op Y2) ==> op (X1, X2, Y1, Y2)
      right_func->add_at_head(left_func->argument_list());
      return right;
    }
    else
    {
      // (X1 op X2) op Y ==> op (X1, X2, Y)
      left_func->add(right);
      return left;
    }
  }
  else if (right_func)
  {
    //  X op (Y1 op Y2) ==> op (X, Y1, Y2)
    right_func->add_at_head(left);
    return right;
  }
  else
  {
    /* X op Y */
    return new (mem_root) Class(pos, left, right);
  }
}


Item_splocal* create_item_for_sp_var(THD *thd,
                                     LEX_STRING name,
                                     class sp_variable *spv,
                                     const char *query_start_ptr,
                                     const char *start,
                                     const char *end);

bool setup_select_in_parentheses(SELECT_LEX *);
void my_syntax_error(const char *s);


bool find_sys_var_null_base(THD *thd, struct sys_var_with_base *tmp);
bool set_system_variable(THD *thd, struct sys_var_with_base *tmp,
                         enum enum_var_type var_type, Item *val);
LEX_STRING make_string(THD *thd, const char *start_ptr, const char *end_ptr);
bool set_trigger_new_row(Parse_context *pc,
                         LEX_STRING trigger_field_name,
                         Item *expr_item,
                         LEX_STRING expr_query);
void sp_create_assignment_lex(THD *thd, const char *option_ptr);
bool sp_create_assignment_instr(THD *thd, const char *expr_end_ptr);

#endif /* PARSE_TREE_HELPERS_INCLUDED */
