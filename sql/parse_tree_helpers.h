/* Copyright (c) 2013, 2022, Oracle and/or its affiliates.

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

#ifndef PARSE_TREE_HELPERS_INCLUDED
#define PARSE_TREE_HELPERS_INCLUDED

#include <assert.h>
#include <sys/types.h>  // TODO: replace with cstdint
#include <new>

#include "lex_string.h"
#include "m_ctype.h"

#include "my_inttypes.h"  // TODO: replace with cstdint
#include "mysql_time.h"
#include "sql/item.h"
#include "sql/item_func.h"       // Item etc.
#include "sql/parse_location.h"  // POS
#include "sql/parse_tree_node_base.h"
#include "sql/resourcegroups/resource_group_basic_types.h"  // resourcegroups::Range
#include "sql/set_var.h"                                    // enum_var_type
#include "sql/sql_error.h"
#include "sql/sql_list.h"

class PT_query_expression_body;
class PT_query_primary;
class String;
class THD;
class my_decimal;
enum class Set_operator;
struct Column_parse_context;
struct MEM_ROOT;
struct handlerton;

template <typename Element_type>
class Mem_root_array;

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
class Parse_tree_item : public Item {
 public:
  explicit Parse_tree_item(const POS &pos) : Item(pos) {}

  enum Type type() const override { return INVALID_ITEM; }
  double val_real() override {
    assert(0);
    return 0;
  }
  longlong val_int() override {
    assert(0);
    return 0;
  }
  String *val_str(String *) override {
    assert(0);
    return nullptr;
  }
  my_decimal *val_decimal(my_decimal *) override {
    assert(0);
    return nullptr;
  }
  bool get_date(MYSQL_TIME *, uint) override {
    assert(0);
    return false;
  }
  bool get_time(MYSQL_TIME *) override {
    assert(0);
    return false;
  }
};

/**
  Wrapper class for an Item list head, used to allocate Item lists in the parser
  in a context-independent way
*/
class PT_item_list : public Parse_tree_node {
  typedef Parse_tree_node super;

 public:
  PT_item_list() : value(*THR_MALLOC) {}

  mem_root_deque<Item *> value;

  bool contextualize(Parse_context *pc) override {
    if (super::contextualize(pc)) return true;
    for (Item *&item : value) {
      if (item->itemize(pc, &item)) return true;
    }
    return false;
  }

  bool is_empty() const { return value.empty(); }
  uint elements() const { return value.size(); }

  bool push_back(Item *item) {
    /*
     Item may be NULL in case of OOM: just ignore it and check thd->is_error()
     in the caller code.
    */
    if (item == nullptr) return true;
    value.push_back(item);
    return false;
  }

  bool push_front(Item *item) {
    /*
     Item may be NULL in case of OOM: just ignore it and check thd->is_error()
     in the caller code.
    */
    if (item == nullptr) return true;
    value.push_front(item);
    return false;
  }

  Item *pop_front() {
    assert(!is_empty());
    Item *ret = value.front();
    value.pop_front();
    return ret;
  }

  Item *operator[](uint index) const { return value[index]; }
};

/**
  Contextualize a Mem_root_array of parse tree nodes of the type PTN

  @tparam Context       Parse context.
  @tparam Array         Array of parse tree nodes.

  @param[in,out] pc     Parse context.
  @param[in,out] array  Array of nodes to contextualize.

  @return false on success.
*/
template <typename Context, typename Array>
bool contextualize_array(Context *pc, Array *array) {
  for (auto it : *array) {
    if (pc->thd->lex->will_contextualize && it->contextualize(pc)) return true;
  }
  return false;
}

/**
  Helper function to imitate dynamic_cast for Item_cond hierarchy.

  Template parameter @p To is the destination type (@c Item_cond_and etc.)
  Template parameter @p Tag is the Functype tag to compare from->functype() with

  @param from   source item

  @return typecasted item of the type To or NULL
*/
template <class To, Item_func::Functype Tag>
To *item_cond_cast(Item *const from) {
  return ((from->type() == Item::COND_ITEM &&
           static_cast<Item_func *>(from)->functype() == Tag)
              ? static_cast<To *>(from)
              : nullptr);
}

/**
  Flatten associative operators at parse time

  This function flattens AND and OR operators at parse time if applicable,
  otherwise it creates new Item_cond_and or Item_cond_or respectively.

  Template parameter @p Class is @c Item_cond_and or @c Item_cond_or
  Template parameter @p Tag is @c COND_AND_FUNC (for @c Item_cond_and) or @c
  COND_OR_FUNC otherwise

  @param mem_root       MEM_ROOT
  @param pos            parse location
  @param left           left argument of the operator
  @param right          right argument of the operator

  @return resulting parse tree Item
*/
template <class Class, Item_func::Functype Tag>
Item *flatten_associative_operator(MEM_ROOT *mem_root, const POS &pos,
                                   Item *left, Item *right) {
  if (left == nullptr || right == nullptr) return nullptr;
  Class *left_func = item_cond_cast<Class, Tag>(left);
  Class *right_func = item_cond_cast<Class, Tag>(right);
  if (left_func) {
    if (right_func) {
      // (X1 op X2) op (Y1 op Y2) ==> op (X1, X2, Y1, Y2)
      right_func->add_at_head(left_func->argument_list());
      return right;
    } else {
      // (X1 op X2) op Y ==> op (X1, X2, Y)
      left_func->add(right);
      return left;
    }
  } else if (right_func) {
    //  X op (Y1 op Y2) ==> op (X, Y1, Y2)
    right_func->add_at_head(left);
    return right;
  } else {
    /* X op Y */
    return new (mem_root) Class(pos, left, right);
  }
}

Item_splocal *create_item_for_sp_var(THD *thd, LEX_CSTRING name,
                                     class sp_variable *spv,
                                     const char *query_start_ptr,
                                     const char *start, const char *end);

LEX_CSTRING make_string(THD *thd, const char *start_ptr, const char *end_ptr);
void sp_create_assignment_lex(THD *thd, const char *option_ptr);
bool sp_create_assignment_instr(THD *thd, const char *expr_end_ptr);
bool resolve_engine(THD *thd, const LEX_CSTRING &name, bool is_temp_table,
                    bool strict, handlerton **ret);
bool apply_privileges(
    THD *thd, const Mem_root_array<class PT_role_or_privilege *> &privs);

inline bool is_identifier(const char *str, const char *ident) {
  return !my_strcasecmp(system_charset_info, str, ident);
}

inline bool is_identifier(const LEX_STRING &str, const char *ident) {
  return is_identifier(str.str, ident);
}

bool validate_vcpu_range(const resourcegroups::Range &range);
bool validate_resource_group_priority(THD *thd, int *priority,
                                      const LEX_CSTRING &name,
                                      const resourcegroups::Type &type);
bool check_resource_group_support();
bool check_resource_group_name_len(const LEX_CSTRING &name,
                                   Sql_condition::enum_severity_level severity);

void move_cf_appliers(Parse_context *tddlpc, Column_parse_context *cpc);

#endif /* PARSE_TREE_HELPERS_INCLUDED */
