#ifndef ITEM_CMPFUNC_INCLUDED
#define ITEM_CMPFUNC_INCLUDED

/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

/* compare and test functions */

#include <string.h>
#include <sys/types.h>

#include "binary_log_types.h"
#include "extra/regex/my_regex.h"  // my_regex_t
#include "m_ctype.h"
#include "my_alloc.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "my_table_map.h"
#include "my_time.h"
#include "mysql/udf_registration_types.h"
#include "mysql_time.h"
#include "sql/enum_query_type.h"
#include "sql/item.h"
#include "sql/item_func.h"       // Item_int_func
#include "sql/item_row.h"        // Item_row
#include "sql/mem_root_array.h"  // Mem_root_array
#include "sql/my_decimal.h"
#include "sql/parse_tree_node_base.h"
#include "sql/sql_const.h"
#include "sql/sql_list.h"
#include "sql/table.h"
#include "sql_string.h"
#include "template_utils.h"  // down_cast

class Arg_comparator;
class Field;
class Item_in_subselect;
class Item_subselect;
class Item_sum_hybrid;
class Json_scalar_holder;
class Json_wrapper;
class PT_item_list;
class SELECT_LEX;
class THD;
struct MY_BITMAP;

typedef int (Arg_comparator::*arg_cmp_func)();

class Arg_comparator {
  Item **a, **b;
  arg_cmp_func func;
  Item_result_field *owner;
  Arg_comparator *comparators;  // used only for compare_row()
  uint16 comparator_count;
  double precision;
  /* Fields used in DATE/DATETIME comparison. */
  Item *a_cache, *b_cache;  // Cached values of a and b items
  bool is_nulls_eq;         // true <=> compare for the EQUAL_FUNC
  bool set_null;            // true <=> set owner->null_value
                            //   when one of arguments is NULL.
  longlong (*get_value_a_func)(THD *thd, Item ***item_arg, Item **cache_arg,
                               const Item *warn_item, bool *is_null);
  longlong (*get_value_b_func)(THD *thd, Item ***item_arg, Item **cache_arg,
                               const Item *warn_item, bool *is_null);
  bool try_year_cmp_func(Item_result type);
  static bool get_date_from_const(Item *date_arg, Item *str_arg,
                                  ulonglong *value);
  /**
    Only used by compare_json() in the case where a JSON value is
    compared to an SQL value. This member points to pre-allocated
    memory that can be used instead of the heap when converting the
    SQL value to a JSON value.
  */
  Json_scalar_holder *json_scalar;

 public:
  DTCollation cmp_collation;
  /* Allow owner function to use string buffers. */
  String value1, value2;

  Arg_comparator()
      : comparators(0),
        comparator_count(0),
        a_cache(0),
        b_cache(0),
        set_null(true),
        get_value_a_func(0),
        get_value_b_func(0),
        json_scalar(0) {}
  Arg_comparator(Item **a1, Item **a2)
      : a(a1),
        b(a2),
        comparators(0),
        comparator_count(0),
        a_cache(0),
        b_cache(0),
        set_null(true),
        get_value_a_func(0),
        get_value_b_func(0),
        json_scalar(0) {}

  bool set_compare_func(Item_result_field *owner, Item_result type);
  bool set_cmp_func(Item_result_field *owner_arg, Item **a1, Item **a2,
                    Item_result type);

  bool set_cmp_func(Item_result_field *owner_arg, Item **a1, Item **a2,
                    bool set_null_arg);

  inline int compare() { return (this->*func)(); }

  int compare_string();         // compare args[0] & args[1]
  int compare_binary_string();  // compare args[0] & args[1]
  int compare_real();           // compare args[0] & args[1]
  int compare_decimal();        // compare args[0] & args[1]
  int compare_int_signed();     // compare args[0] & args[1]
  int compare_int_signed_unsigned();
  int compare_int_unsigned_signed();
  int compare_int_unsigned();
  int compare_time_packed();
  int compare_e_time_packed();
  int compare_row();              // compare args[0] & args[1]
  int compare_e_string();         // compare args[0] & args[1]
  int compare_e_binary_string();  // compare args[0] & args[1]
  int compare_e_real();           // compare args[0] & args[1]
  int compare_e_decimal();        // compare args[0] & args[1]
  int compare_e_int();            // compare args[0] & args[1]
  int compare_e_int_diff_signedness();
  int compare_e_row();  // compare args[0] & args[1]
  int compare_real_fixed();
  int compare_e_real_fixed();
  int compare_datetime();  // compare args[0] & args[1] as DATETIMEs
  int compare_json();

  static bool can_compare_as_dates(Item *a, Item *b, ulonglong *const_val_arg);

  Item **cache_converted_constant(THD *thd, Item **value, Item **cache,
                                  Item_result type);
  void set_datetime_cmp_func(Item_result_field *owner_arg, Item **a1,
                             Item **b1);
  static arg_cmp_func comparator_matrix[5][2];
  inline bool is_owner_equal_func() {
    return (owner->type() == Item::FUNC_ITEM &&
            ((Item_func *)owner)->functype() == Item_func::EQUAL_FUNC);
  }
  void cleanup();
  /*
    Set correct cmp_context if items would be compared as INTs.
  */
  inline void set_cmp_context_for_datetime() {
    DBUG_ASSERT(func == &Arg_comparator::compare_datetime);
    if ((*a)->is_temporal()) (*a)->cmp_context = INT_RESULT;
    if ((*b)->is_temporal()) (*b)->cmp_context = INT_RESULT;
  }
  friend class Item_func;
};

class Item_bool_func : public Item_int_func {
 public:
  Item_bool_func() : Item_int_func(), m_created_by_in2exists(false) {}
  explicit Item_bool_func(const POS &pos)
      : Item_int_func(pos), m_created_by_in2exists(false) {}

  Item_bool_func(Item *a) : Item_int_func(a), m_created_by_in2exists(false) {}
  Item_bool_func(const POS &pos, Item *a)
      : Item_int_func(pos, a), m_created_by_in2exists(false) {}

  Item_bool_func(Item *a, Item *b)
      : Item_int_func(a, b), m_created_by_in2exists(false) {}
  Item_bool_func(const POS &pos, Item *a, Item *b)
      : Item_int_func(pos, a, b), m_created_by_in2exists(false) {}

  Item_bool_func(THD *thd, Item_bool_func *item)
      : Item_int_func(thd, item),
        m_created_by_in2exists(item->m_created_by_in2exists) {}
  bool is_bool_func() const override { return true; }
  bool resolve_type(THD *) override {
    max_length = 1;
    return false;
  }
  uint decimal_precision() const override { return 1; }
  bool created_by_in2exists() const override { return m_created_by_in2exists; }
  void set_created_by_in2exists() { m_created_by_in2exists = true; }

 private:
  /**
    True <=> this item was added by IN->EXISTS subquery transformation, and
    should thus be deleted if we switch to materialization.
  */
  bool m_created_by_in2exists;
};

/**
  Abstract Item class, to represent <code>X IS [NOT] (TRUE | FALSE)</code>
  boolean predicates.
*/

class Item_func_truth : public Item_bool_func {
 public:
  bool val_bool() override;
  longlong val_int() override;
  bool resolve_type(THD *) override;
  void print(String *str, enum_query_type query_type) override;

 protected:
  Item_func_truth(const POS &pos, Item *a, bool a_value, bool a_affirmative)
      : Item_bool_func(pos, a), value(a_value), affirmative(a_affirmative) {}

  ~Item_func_truth() {}

 private:
  /**
    True for <code>X IS [NOT] TRUE</code>,
    false for <code>X IS [NOT] FALSE</code> predicates.
  */
  const bool value;
  /**
    True for <code>X IS Y</code>, false for <code>X IS NOT Y</code> predicates.
  */
  const bool affirmative;
};

/**
  This Item represents a <code>X IS TRUE</code> boolean predicate.
*/

class Item_func_istrue final : public Item_func_truth {
 public:
  Item_func_istrue(const POS &pos, Item *a)
      : Item_func_truth(pos, a, true, true) {}
  ~Item_func_istrue() {}
  const char *func_name() const override { return "istrue"; }
};

/**
  This Item represents a <code>X IS NOT TRUE</code> boolean predicate.
*/

class Item_func_isnottrue final : public Item_func_truth {
 public:
  Item_func_isnottrue(const POS &pos, Item *a)
      : Item_func_truth(pos, a, true, false) {}
  ~Item_func_isnottrue() {}
  const char *func_name() const override { return "isnottrue"; }
};

/**
  This Item represents a <code>X IS FALSE</code> boolean predicate.
*/

class Item_func_isfalse final : public Item_func_truth {
 public:
  Item_func_isfalse(const POS &pos, Item *a)
      : Item_func_truth(pos, a, false, true) {}
  ~Item_func_isfalse() {}
  const char *func_name() const override { return "isfalse"; }
};

/**
  This Item represents a <code>X IS NOT FALSE</code> boolean predicate.
*/

class Item_func_isnotfalse final : public Item_func_truth {
 public:
  Item_func_isnotfalse(const POS &pos, Item *a)
      : Item_func_truth(pos, a, false, false) {}
  ~Item_func_isnotfalse() {}
  const char *func_name() const override { return "isnotfalse"; }
};

static const int UNKNOWN = -1;

/*
  Item_in_optimizer(left_expr, Item_in_subselect(...))

  Item_in_optimizer is used to wrap an instance of Item_in_subselect. This
  class does the following:
   - Evaluate the left expression and store it in Item_cache_* object (to
     avoid re-evaluating it many times during subquery execution)
   - Shortcut the evaluation of "NULL IN (...)" to NULL in the cases where we
     don't care if the result is NULL or FALSE.

   args[1] keeps a reference to the Item_in_subselect object.

   args[0] is a copy of Item_in_subselect's left expression and should be
   kept equal also after resolving.

  NOTE
    It is not quite clear why the above listed functionality should be
    placed into a separate class called 'Item_in_optimizer'.
*/

class Item_in_optimizer final : public Item_bool_func {
 private:
  Item_cache *cache;
  bool save_cache;
  /*
    Stores the value of "NULL IN (SELECT ...)" for uncorrelated subqueries:
      UNKNOWN - "NULL in (SELECT ...)" has not yet been evaluated
      FALSE   - result is FALSE
      TRUE    - result is NULL
  */
  int result_for_null_param;

 public:
  Item_in_optimizer(Item *a, Item_in_subselect *b)
      : Item_bool_func(a, reinterpret_cast<Item *>(b)),
        cache(0),
        save_cache(0),
        result_for_null_param(UNKNOWN) {
    set_subquery();
  }
  bool fix_fields(THD *, Item **) override;
  bool fix_left(THD *thd, Item **ref);
  void fix_after_pullout(SELECT_LEX *parent_select,
                         SELECT_LEX *removed_select) override;
  bool is_null() override;
  longlong val_int() override;
  void cleanup() override;
  const char *func_name() const override { return "<in_optimizer>"; }
  Item_cache **get_cache() { return &cache; }
  void keep_top_level_cache();
  Item *transform(Item_transformer transformer, uchar *arg) override;
  void replace_argument(THD *thd, Item **oldpp, Item *newp) override;
};

/// Abstract factory interface for creating comparison predicates.
class Comp_creator {
 public:
  virtual ~Comp_creator() {}
  virtual Item_bool_func *create(Item *a, Item *b) const = 0;

  /// This interface is only used by Item_allany_subselect.
  virtual const char *symbol(bool invert) const = 0;
  virtual bool eqne_op() const = 0;
  virtual bool l_op() const = 0;
};

/// Abstract base class for the comparison operators =, <> and <=>.
class Linear_comp_creator : public Comp_creator {
 public:
  virtual Item_bool_func *create(Item *a, Item *b) const;
  virtual bool eqne_op() const { return true; }
  virtual bool l_op() const { return false; }

 protected:
  /**
    Creates only an item tree node, without attempting to rewrite row
    constructors.
    @see create()
  */
  virtual Item_bool_func *create_scalar_predicate(Item *a, Item *b) const = 0;

  /// Combines a list of conditions <code>exp op exp</code>.
  virtual Item_bool_func *combine(List<Item> list) const = 0;
};

class Eq_creator : public Linear_comp_creator {
 public:
  virtual const char *symbol(bool invert) const { return invert ? "<>" : "="; }

 protected:
  virtual Item_bool_func *create_scalar_predicate(Item *a, Item *b) const;
  virtual Item_bool_func *combine(List<Item> list) const;
};

class Equal_creator : public Linear_comp_creator {
 public:
  virtual const char *symbol(bool invert MY_ATTRIBUTE((unused))) const {
    // This will never be called with true.
    DBUG_ASSERT(!invert);
    return "<=>";
  }

 protected:
  virtual Item_bool_func *create_scalar_predicate(Item *a, Item *b) const;
  virtual Item_bool_func *combine(List<Item> list) const;
};

class Ne_creator : public Linear_comp_creator {
 public:
  virtual const char *symbol(bool invert) const { return invert ? "=" : "<>"; }

 protected:
  virtual Item_bool_func *create_scalar_predicate(Item *a, Item *b) const;
  virtual Item_bool_func *combine(List<Item> list) const;
};

class Gt_creator : public Comp_creator {
 public:
  Gt_creator() {}          /* Remove gcc warning */
  virtual ~Gt_creator() {} /* Remove gcc warning */
  virtual Item_bool_func *create(Item *a, Item *b) const;
  virtual const char *symbol(bool invert) const { return invert ? "<=" : ">"; }
  virtual bool eqne_op() const { return 0; }
  virtual bool l_op() const { return 0; }
};

class Lt_creator : public Comp_creator {
 public:
  Lt_creator() {}          /* Remove gcc warning */
  virtual ~Lt_creator() {} /* Remove gcc warning */
  virtual Item_bool_func *create(Item *a, Item *b) const;
  virtual const char *symbol(bool invert) const { return invert ? ">=" : "<"; }
  virtual bool eqne_op() const { return 0; }
  virtual bool l_op() const { return 1; }
};

class Ge_creator : public Comp_creator {
 public:
  Ge_creator() {}          /* Remove gcc warning */
  virtual ~Ge_creator() {} /* Remove gcc warning */
  virtual Item_bool_func *create(Item *a, Item *b) const;
  virtual const char *symbol(bool invert) const { return invert ? "<" : ">="; }
  virtual bool eqne_op() const { return 0; }
  virtual bool l_op() const { return 0; }
};

class Le_creator : public Comp_creator {
 public:
  Le_creator() {}          /* Remove gcc warning */
  virtual ~Le_creator() {} /* Remove gcc warning */
  virtual Item_bool_func *create(Item *a, Item *b) const;
  virtual const char *symbol(bool invert) const { return invert ? ">" : "<="; }
  virtual bool eqne_op() const { return 0; }
  virtual bool l_op() const { return 1; }
};

class Item_bool_func2 : public Item_bool_func { /* Bool with 2 string args */
 private:
  bool convert_constant_arg(THD *thd, Item *field, Item **item,
                            bool *converted);

 protected:
  Arg_comparator cmp;
  bool abort_on_null;

 public:
  Item_bool_func2(Item *a, Item *b)
      : Item_bool_func(a, b), cmp(tmp_arg, tmp_arg + 1), abort_on_null(false) {}

  Item_bool_func2(const POS &pos, Item *a, Item *b)
      : Item_bool_func(pos, a, b),
        cmp(tmp_arg, tmp_arg + 1),
        abort_on_null(false) {}

  bool resolve_type(THD *) override;
  bool set_cmp_func() {
    return cmp.set_cmp_func(this, tmp_arg, tmp_arg + 1, true);
  }
  optimize_type select_optimize() const override { return OPTIMIZE_OP; }
  virtual enum Functype rev_functype() const { return UNKNOWN_FUNC; }
  bool have_rev_func() const override { return rev_functype() != UNKNOWN_FUNC; }

  void print(String *str, enum_query_type query_type) override {
    Item_func::print_op(str, query_type);
  }

  bool is_null() override { return args[0]->is_null() || args[1]->is_null(); }
  const CHARSET_INFO *compare_collation() const override {
    return cmp.cmp_collation.collation;
  }
  void top_level_item() override { abort_on_null = true; }
  void cleanup() override {
    Item_bool_func::cleanup();
    cmp.cleanup();
  }

  friend class Arg_comparator;
};

class Item_bool_rowready_func2 : public Item_bool_func2 {
 public:
  Item_bool_rowready_func2(Item *a, Item *b) : Item_bool_func2(a, b) {
    allowed_arg_cols = 0;  // Fetch this value from first argument
  }
  Item_bool_rowready_func2(const POS &pos, Item *a, Item *b)
      : Item_bool_func2(pos, a, b) {
    allowed_arg_cols = 0;  // Fetch this value from first argument
  }

  Item *neg_transformer(THD *thd) override;
  virtual Item *negated_item();
  bool subst_argument_checker(uchar **) override { return true; }
};

/**
  XOR inherits from Item_bool_func2 because it is not optimized yet.
  Later, when XOR is optimized, it needs to inherit from
  Item_cond instead. See WL#5800.
*/
class Item_func_xor final : public Item_bool_func2 {
 public:
  Item_func_xor(Item *i1, Item *i2) : Item_bool_func2(i1, i2) {}
  Item_func_xor(const POS &pos, Item *i1, Item *i2)
      : Item_bool_func2(pos, i1, i2) {}

  enum Functype functype() const override { return XOR_FUNC; }
  const char *func_name() const override { return "xor"; }
  longlong val_int() override;
  void top_level_item() override {}
  Item *neg_transformer(THD *thd) override;

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;
};

class Item_func_not : public Item_bool_func {
 public:
  Item_func_not(Item *a) : Item_bool_func(a) {}
  Item_func_not(const POS &pos, Item *a) : Item_bool_func(pos, a) {}

  longlong val_int() override;
  enum Functype functype() const override { return NOT_FUNC; }
  const char *func_name() const override { return "not"; }
  Item *neg_transformer(THD *) override;
  void print(String *str, enum_query_type query_type) override;

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;
};

class Item_maxmin_subselect;
class JOIN;

/*
  trigcond<param>(arg) ::= param? arg : true

  The class Item_func_trig_cond is used for guarded predicates
  which are employed only for internal purposes.
  A guarded predicate is an object consisting of an a regular or
  a guarded predicate P and a pointer to a boolean guard variable g.
  A guarded predicate P/g is evaluated to true if the value of the
  guard g is false, otherwise it is evaluated to the same value that
  the predicate P: val(P/g)= g ? val(P):true.
  Guarded predicates allow us to include predicates into a conjunction
  conditionally. Currently they are utilized for pushed down predicates
  in queries with outer join operations.

  In the future, probably, it makes sense to extend this class to
  the objects consisting of three elements: a predicate P, a pointer
  to a variable g and a firing value s with following evaluation
  rule: val(P/g,s)= g==s? val(P) : true. It will allow us to build only
  one item for the objects of the form P/g1/g2...

  Objects of this class are built only for query execution after
  the execution plan has been already selected. That's why this
  class needs only val_int out of generic methods.

  Current uses of Item_func_trig_cond objects:
   - To wrap selection conditions when executing outer joins
   - To wrap condition that is pushed down into subquery
*/

class Item_func_trig_cond final : public Item_bool_func {
 public:
  enum enum_trig_type {
    /**
      This trigger type deactivates join conditions when a row has been
      NULL-complemented. For example, in t1 LEFT JOIN t2, the join condition
      can be tested on t2's row only if that row is not NULL-complemented.
    */
    IS_NOT_NULL_COMPL,

    /**
      This trigger type deactivates predicated from WHERE condition when no
      row satisfying the join condition has been found. For Example, in t1
      LEFT JOIN t2, the where condition pushed to t2 can be tested only after
      at least one t2 row has been produced, which may be a NULL-complemented
      row.
    */
    FOUND_MATCH,

    /**
       In IN->EXISTS subquery transformation, new predicates are added:
       WHERE inner_field=outer_field OR inner_field IS NULL,
       as well as
       HAVING inner_field IS NOT NULL,
       are disabled if outer_field is a NULL value
    */
    OUTER_FIELD_IS_NOT_NULL
  };

 private:
  /** Pointer to trigger variable */
  bool *trig_var;
  /// Optional: JOIN of table which is the source of trig_var
  const JOIN *m_join;
  /// Optional: if join!=NULL: index of table
  plan_idx m_idx;
  /** Type of trig_var; for printing */
  enum_trig_type trig_type;

 public:
  /**
     @param a             the item for @<condition@>
     @param f             pointer to trigger variable
     @param join          if a table's property is the source of 'f', JOIN
     which owns this table; NULL otherwise.
     @param idx           if join!=NULL: index of this table in the
     JOIN_TAB/QEP_TAB array. NO_PLAN_IDX otherwise.
     @param trig_type_arg type of 'f'
  */
  Item_func_trig_cond(Item *a, bool *f, JOIN *join, plan_idx idx,
                      enum_trig_type trig_type_arg)
      : Item_bool_func(a),
        trig_var(f),
        m_join(join),
        m_idx(idx),
        trig_type(trig_type_arg) {}
  longlong val_int() override;
  enum Functype functype() const override { return TRIG_COND_FUNC; };
  /// '@<if@>', to distinguish from the if() SQL function
  const char *func_name() const override { return "<if>"; };
  /// Get range of inner tables spanned by associated outer join operation
  void get_table_range(TABLE_LIST **first_table, TABLE_LIST **last_table);
  bool fix_fields(THD *thd, Item **ref) override {
    if (Item_bool_func::fix_fields(thd, ref)) return true;
    add_trig_func_tables();
    return false;
  }
  void add_trig_func_tables() {
    if (trig_type == IS_NOT_NULL_COMPL || trig_type == FOUND_MATCH) {
      DBUG_ASSERT(m_join != nullptr);
      // Make this function dependent on the range of inner tables
      TABLE_LIST *first_table, *last_table;
      get_table_range(&first_table, &last_table);
      used_tables_cache |= last_table->map() | ((last_table->map() - 1) &
                                                ~(first_table->map() - 1));
    } else if (trig_type == OUTER_FIELD_IS_NOT_NULL) {
      used_tables_cache |= OUTER_REF_TABLE_BIT;
    }
  }
  void update_used_tables() override {
    Item_bool_func::update_used_tables();
    add_trig_func_tables();
  }
  bool *get_trig_var() { return trig_var; }
  void print(String *str, enum_query_type query_type) override;
};

class Item_func_not_all : public Item_func_not {
  /* allow to check presence of values in max/min optimization */
  Item_sum_hybrid *test_sum_item;
  Item_maxmin_subselect *test_sub_item;
  Item_subselect *subselect;

  bool abort_on_null;

 public:
  bool show;

  Item_func_not_all(Item *a)
      : Item_func_not(a),
        test_sum_item(0),
        test_sub_item(0),
        subselect(0),
        abort_on_null(0),
        show(0) {}
  void top_level_item() override { abort_on_null = true; }
  bool is_top_level_item() const { return abort_on_null; }
  longlong val_int() override;
  enum Functype functype() const override { return NOT_ALL_FUNC; }
  const char *func_name() const override { return "<not>"; }
  void print(String *str, enum_query_type query_type) override;
  void set_sum_test(Item_sum_hybrid *item) { test_sum_item = item; };
  void set_sub_test(Item_maxmin_subselect *item) { test_sub_item = item; };
  void set_subselect(Item_subselect *item) { subselect = item; }
  table_map not_null_tables() const override {
    /*
      See handling of not_null_tables_cache in
      Item_in_optimizer::fix_fields().

      This item is the result of a transformation from an ALL clause
      such as
          left-expr < ALL(subquery)
      into
          <not>(left-expr >= (subquery)

      An inequality usually rejects NULLs from both operands, so the
      not_null_tables() of the inequality is the union of the
      null-rejecting tables of both operands. However, since this is a
      transformed ALL clause that should return true if the subquery
      is empty (even if left-expr is NULL), it is not null rejecting
      for left-expr. The not null tables mask for left-expr should be
      removed, leaving only the null-rejecting tables of the
      subquery. Item_subselect::not_null_tables() always returns 0 (no
      null-rejecting tables). Therefore, always return 0.
    */
    return 0;
  }
  bool empty_underlying_subquery();
  Item *neg_transformer(THD *) override;
};

class Item_func_nop_all final : public Item_func_not_all {
 public:
  Item_func_nop_all(Item *a) : Item_func_not_all(a) {}
  longlong val_int() override;
  const char *func_name() const override { return "<nop>"; }
  table_map not_null_tables() const override { return not_null_tables_cache; }
  Item *neg_transformer(THD *thd) override;
};

class Item_func_eq : public Item_bool_rowready_func2 {
 public:
  Item_func_eq(Item *a, Item *b) : Item_bool_rowready_func2(a, b) {}
  Item_func_eq(const POS &pos, Item *a, Item *b)
      : Item_bool_rowready_func2(pos, a, b) {}
  longlong val_int() override;
  enum Functype functype() const override { return EQ_FUNC; }
  enum Functype rev_functype() const override { return EQ_FUNC; }
  cond_result eq_cmp_result() const override { return COND_TRUE; }
  const char *func_name() const override { return "="; }
  Item *negated_item() override;
  bool equality_substitution_analyzer(uchar **) override { return true; }
  Item *equality_substitution_transformer(uchar *arg) override;
  bool gc_subst_analyzer(uchar **) override { return true; }

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;
};

class Item_func_equal final : public Item_bool_rowready_func2 {
 public:
  Item_func_equal(Item *a, Item *b) : Item_bool_rowready_func2(a, b) {
    null_on_null = false;
  }
  Item_func_equal(const POS &pos, Item *a, Item *b)
      : Item_bool_rowready_func2(pos, a, b) {
    null_on_null = false;
  }
  longlong val_int() override;
  bool resolve_type(THD *thd) override;
  enum Functype functype() const override { return EQUAL_FUNC; }
  enum Functype rev_functype() const override { return EQUAL_FUNC; }
  cond_result eq_cmp_result() const override { return COND_TRUE; }
  const char *func_name() const override { return "<=>"; }
  Item *neg_transformer(THD *) override { return nullptr; }

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;
};

class Item_func_ge final : public Item_bool_rowready_func2 {
 public:
  Item_func_ge(Item *a, Item *b) : Item_bool_rowready_func2(a, b){};
  longlong val_int() override;
  enum Functype functype() const override { return GE_FUNC; }
  enum Functype rev_functype() const override { return LE_FUNC; }
  cond_result eq_cmp_result() const override { return COND_TRUE; }
  const char *func_name() const override { return ">="; }
  Item *negated_item() override;
  bool gc_subst_analyzer(uchar **) override { return true; }

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;
};

class Item_func_gt final : public Item_bool_rowready_func2 {
 public:
  Item_func_gt(Item *a, Item *b) : Item_bool_rowready_func2(a, b){};
  longlong val_int() override;
  enum Functype functype() const override { return GT_FUNC; }
  enum Functype rev_functype() const override { return LT_FUNC; }
  cond_result eq_cmp_result() const override { return COND_FALSE; }
  const char *func_name() const override { return ">"; }
  Item *negated_item() override;
  bool gc_subst_analyzer(uchar **) override { return true; }

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;
};

class Item_func_le final : public Item_bool_rowready_func2 {
 public:
  Item_func_le(Item *a, Item *b) : Item_bool_rowready_func2(a, b){};
  longlong val_int() override;
  enum Functype functype() const override { return LE_FUNC; }
  enum Functype rev_functype() const override { return GE_FUNC; }
  cond_result eq_cmp_result() const override { return COND_TRUE; }
  const char *func_name() const override { return "<="; }
  Item *negated_item() override;
  bool gc_subst_analyzer(uchar **) override { return true; }

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;
};

class Item_func_lt final : public Item_bool_rowready_func2 {
 public:
  Item_func_lt(Item *a, Item *b) : Item_bool_rowready_func2(a, b) {}
  longlong val_int() override;
  enum Functype functype() const override { return LT_FUNC; }
  enum Functype rev_functype() const override { return GT_FUNC; }
  cond_result eq_cmp_result() const override { return COND_FALSE; }
  const char *func_name() const override { return "<"; }
  Item *negated_item() override;
  bool gc_subst_analyzer(uchar **) override { return true; }

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;
};

class Item_func_ne final : public Item_bool_rowready_func2 {
 public:
  Item_func_ne(Item *a, Item *b) : Item_bool_rowready_func2(a, b) {}
  longlong val_int() override;
  enum Functype functype() const override { return NE_FUNC; }
  cond_result eq_cmp_result() const override { return COND_FALSE; }
  optimize_type select_optimize() const override { return OPTIMIZE_KEY; }
  const char *func_name() const override { return "<>"; }
  Item *negated_item() override;

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;
};

/*
  The class Item_func_opt_neg is defined to factor out the functionality
  common for the classes Item_func_between and Item_func_in. The objects
  of these classes can express predicates or there negations.
  The alternative approach would be to create pairs Item_func_between,
  Item_func_notbetween and Item_func_in, Item_func_notin.

*/

class Item_func_opt_neg : public Item_int_func {
 public:
  bool negated;    /* <=> the item represents NOT <func> */
  bool pred_level; /* <=> [NOT] <func> is used on a predicate level */
 public:
  Item_func_opt_neg(const POS &pos, Item *a, Item *b, Item *c, bool is_negation)
      : Item_int_func(pos, a, b, c), negated(0), pred_level(0) {
    if (is_negation) negate();
  }
  Item_func_opt_neg(const POS &pos, PT_item_list *list, bool is_negation)
      : Item_int_func(pos, list), negated(0), pred_level(0) {
    if (is_negation) negate();
  }

 public:
  inline void negate() { negated = !negated; }
  inline void top_level_item() override { pred_level = 1; }
  bool is_top_level_item() const { return pred_level; }
  Item *neg_transformer(THD *) override {
    negated = !negated;
    return this;
  }
  bool eq(const Item *item, bool binary_cmp) const override;
  bool subst_argument_checker(uchar **) override { return true; }
};

class Item_func_between final : public Item_func_opt_neg {
  DTCollation cmp_collation;

 public:
  Item_result cmp_type;
  String value0, value1, value2;
  /* true <=> arguments will be compared as dates. */
  bool compare_as_dates_with_strings;
  bool compare_as_temporal_dates;
  bool compare_as_temporal_times;

  /* Comparators used for DATE/DATETIME comparison. */
  Arg_comparator ge_cmp, le_cmp;
  Item_func_between(const POS &pos, Item *a, Item *b, Item *c, bool is_negation)
      : Item_func_opt_neg(pos, a, b, c, is_negation),
        compare_as_dates_with_strings(false),
        compare_as_temporal_dates(false),
        compare_as_temporal_times(false) {}
  longlong val_int() override;
  optimize_type select_optimize() const override { return OPTIMIZE_KEY; }
  enum Functype functype() const override { return BETWEEN; }
  const char *func_name() const override { return "between"; }
  bool fix_fields(THD *, Item **) override;
  void fix_after_pullout(SELECT_LEX *parent_select,
                         SELECT_LEX *removed_select) override;
  bool resolve_type(THD *) override;
  void print(String *str, enum_query_type query_type) override;
  bool is_bool_func() const override { return true; }
  const CHARSET_INFO *compare_collation() const override {
    return cmp_collation.collation;
  }
  uint decimal_precision() const override { return 1; }
  bool gc_subst_analyzer(uchar **) override { return true; }

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;
};

class Item_func_strcmp final : public Item_bool_func2 {
 public:
  Item_func_strcmp(const POS &pos, Item *a, Item *b)
      : Item_bool_func2(pos, a, b) {}
  longlong val_int() override;
  optimize_type select_optimize() const override { return OPTIMIZE_NONE; }
  const char *func_name() const override { return "strcmp"; }

  void print(String *str, enum_query_type query_type) override {
    Item_func::print(str, query_type);
  }
  bool resolve_type(THD *thd) override {
    if (Item_bool_func2::resolve_type(thd)) return true;
    fix_char_length(2);  // returns "1" or "0" or "-1"
    return false;
  }
};

struct interval_range {
  Item_result type;
  double dbl;
  my_decimal dec;
};

class Item_func_interval final : public Item_int_func {
  typedef Item_int_func super;

  Item_row *row;
  bool use_decimal_comparison;
  interval_range *intervals;

 public:
  Item_func_interval(const POS &pos, MEM_ROOT *mem_root, Item *expr1,
                     Item *expr2, class PT_item_list *opt_expr_list = NULL)
      : super(pos, alloc_row(pos, mem_root, expr1, expr2, opt_expr_list)),
        row(down_cast<Item_row *>(args[0])),
        intervals(0) {
    allowed_arg_cols = 0;  // Fetch this value from first argument
  }

  bool itemize(Parse_context *pc, Item **res) override;
  longlong val_int() override;
  bool resolve_type(THD *) override;
  const char *func_name() const override { return "interval"; }
  uint decimal_precision() const override { return 2; }
  void print(String *str, enum_query_type query_type) override;

 private:
  // Runs in CTOR init list, cannot access *this as Item_func_interval
  static Item_row *alloc_row(const POS &pos, MEM_ROOT *mem_root, Item *expr1,
                             Item *expr2, class PT_item_list *opt_expr_list);
};

class Item_func_coalesce : public Item_func_numhybrid {
 protected:
  Item_func_coalesce(const POS &pos, Item *a, Item *b)
      : Item_func_numhybrid(pos, a, b) {
    null_on_null = false;
  }
  Item_func_coalesce(const POS &pos, Item *a) : Item_func_numhybrid(pos, a) {
    null_on_null = false;
  }

 public:
  Item_func_coalesce(const POS &pos, PT_item_list *list)
      : Item_func_numhybrid(pos, list) {
    null_on_null = false;
  }
  double real_op() override;
  longlong int_op() override;
  String *str_op(String *) override;
  /**
    Get the result of COALESCE as a JSON value.
    @param[in,out] wr   the result value holder
  */
  bool val_json(Json_wrapper *wr) override;
  bool date_op(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) override;
  bool time_op(MYSQL_TIME *ltime) override;
  my_decimal *decimal_op(my_decimal *) override;
  bool resolve_type(THD *) override;
  void set_numeric_type() override {}
  enum Item_result result_type() const override { return hybrid_type; }
  const char *func_name() const override { return "coalesce"; }
};

class Item_func_ifnull final : public Item_func_coalesce {
 protected:
  bool field_type_defined;

 public:
  Item_func_ifnull(const POS &pos, Item *a, Item *b)
      : Item_func_coalesce(pos, a, b) {}
  double real_op() override;
  longlong int_op() override;
  String *str_op(String *str) override;
  bool date_op(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) override;
  bool time_op(MYSQL_TIME *ltime) override;
  my_decimal *decimal_op(my_decimal *) override;
  bool val_json(Json_wrapper *result) override;
  bool resolve_type(THD *) override;
  const char *func_name() const override { return "ifnull"; }
  Field *tmp_table_field(TABLE *table) override;
  uint decimal_precision() const override;
};

/**
   ANY_VALUE(expr) is like expr except that it is not checked by
   aggregate_check logic. It serves as a solution for users who want to
   bypass this logic.
*/
class Item_func_any_value final : public Item_func_coalesce {
 public:
  Item_func_any_value(const POS &pos, Item *a) : Item_func_coalesce(pos, a) {}
  const char *func_name() const override { return "any_value"; }
  bool aggregate_check_group(uchar *arg) override;
  bool aggregate_check_distinct(uchar *arg) override;
};

class Item_func_if final : public Item_func {
  enum Item_result cached_result_type;

 public:
  Item_func_if(Item *a, Item *b, Item *c)
      : Item_func(a, b, c), cached_result_type(INT_RESULT) {
    null_on_null = false;
  }
  Item_func_if(const POS &pos, Item *a, Item *b, Item *c)
      : Item_func(pos, a, b, c), cached_result_type(INT_RESULT) {
    null_on_null = false;
  }

  double val_real() override;
  longlong val_int() override;
  String *val_str(String *str) override;
  my_decimal *val_decimal(my_decimal *) override;
  bool val_json(Json_wrapper *wr) override;
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) override;
  bool get_time(MYSQL_TIME *ltime) override;
  enum Item_result result_type() const override { return cached_result_type; }
  bool fix_fields(THD *, Item **) override;
  bool resolve_type(THD *) override;
  void fix_after_pullout(SELECT_LEX *parent_select,
                         SELECT_LEX *removed_select) override;
  uint decimal_precision() const override;
  const char *func_name() const override { return "if"; }
};

class Item_func_nullif final : public Item_bool_func2 {
  enum Item_result cached_result_type;

 public:
  Item_func_nullif(const POS &pos, Item *a, Item *b)
      : Item_bool_func2(pos, a, b), cached_result_type(INT_RESULT) {
    null_on_null = false;
  }
  double val_real() override;
  longlong val_int() override;
  String *val_str(String *str) override;
  my_decimal *val_decimal(my_decimal *) override;
  bool val_json(Json_wrapper *wr) override;
  Item_result result_type() const override { return cached_result_type; }
  bool resolve_type(THD *thd) override;
  uint decimal_precision() const override {
    return args[0]->decimal_precision();
  }
  const char *func_name() const override { return "nullif"; }

  void print(String *str, enum_query_type query_type) override {
    Item_func::print(str, query_type);
  }

  bool is_null() override;
  /**
    This is a workaround for the broken inheritance hierarchy: this should
    inherit from Item_func instead of Item_bool_func2
  */
  bool is_bool_func() const override { return false; }
};

/* Functions to handle the optimized IN */

/* A vector of values of some type  */

class in_vector {
 public:
  const uint count;  ///< Original size of the vector
  uint used_count;   ///< The actual size of the vector (NULL may be ignored)

  /**
    See Item_func_in::resolve_type() for why we need both
    count and used_count.
   */
  explicit in_vector(uint elements) : count(elements), used_count(elements) {}

  virtual ~in_vector() {}
  virtual void set(uint pos, Item *item) = 0;
  virtual uchar *get_value(Item *item) = 0;

  /**
    Shrinks the IN-list array, to fit actual usage.
   */
  virtual void shrink_array(size_t n) = 0;

  /**
    Sorts the IN-list array, so we can do efficient lookup with binary_search.
   */
  virtual void sort() = 0;

  /**
    Calls (the virtual) get_value, i.e. item->val_int() or item->val_str() etc.
    and then calls find_value() if the value is non-null.
    @param  item to evaluate, and lookup in the IN-list.
    @return true if item was found.
   */
  bool find_item(Item *item);

  /**
    Does a binary_search in the 'base' array for the input 'value'
    @param  value to lookup in the IN-list.
    @return true if value was found.
   */
  virtual bool find_value(const void *value) const = 0;

  /*
    Create an instance of Item_{type} (e.g. Item_decimal) constant object
    which type allows it to hold an element of this vector without any
    conversions.
    The purpose of this function is to be able to get elements of this
    vector in form of Item_xxx constants without creating Item_xxx object
    for every array element you get (i.e. this implements "FlyWeight" pattern)
  */
  virtual Item *create_item() const { return nullptr; }

  /*
    Store the value at position #pos into provided item object
    SYNOPSIS
      value_to_item()
        pos   Index of value to store
        item  Constant item to store value into. The item must be of the same
              type that create_item() returns.
  */
  virtual void value_to_item(uint pos MY_ATTRIBUTE((unused)),
                             Item *item MY_ATTRIBUTE((unused))) {}

  /* Compare values number pos1 and pos2 for equality */
  virtual bool compare_elems(uint pos1, uint pos2) const = 0;

  virtual Item_result result_type() const = 0;
};

class in_string final : public in_vector {
  char buff[STRING_BUFFER_USUAL_SIZE];
  String tmp;
  Mem_root_array<String> base_objects;
  // String objects are not sortable, sort pointers instead.
  Mem_root_array<String *> base_pointers;

  qsort2_cmp compare;
  const CHARSET_INFO *collation;

 public:
  in_string(THD *thd, uint elements, qsort2_cmp cmp_func,
            const CHARSET_INFO *cs);
  void set(uint pos, Item *item) override;
  uchar *get_value(Item *item) override;
  Item *create_item() const override { return new Item_string(collation); }
  void value_to_item(uint pos, Item *item) override {
    String *str = base_pointers[pos];
    item->str_value = *str;
  }
  Item_result result_type() const override { return STRING_RESULT; }

  void shrink_array(size_t n) override { base_pointers.resize(n); }

  void sort() override;
  bool find_value(const void *value) const override;
  bool compare_elems(uint pos1, uint pos2) const override;
};

class in_longlong : public in_vector {
 public:
  struct packed_longlong {
    longlong val;
    longlong unsigned_flag;
  };

 protected:
  /*
    Here we declare a temporary variable (tmp) of the same type as the
    elements of this vector. tmp is used in finding if a given value is in
    the list.
  */
  packed_longlong tmp;

  Mem_root_array<packed_longlong> base;

 public:
  in_longlong(THD *thd, uint elements);
  void set(uint pos, Item *item) override;
  uchar *get_value(Item *item) override;

  Item *create_item() const override {
    /*
      We've created a signed INT, this may not be correct in the
      general case (see BUG#19342).
    */
    return new Item_int((longlong)0);
  }
  void value_to_item(uint pos, Item *item) override {
    ((Item_int *)item)->value = base[pos].val;
    ((Item_int *)item)->unsigned_flag = (bool)base[pos].unsigned_flag;
  }
  Item_result result_type() const override { return INT_RESULT; }

  void shrink_array(size_t n) override { base.resize(n); }

  void sort() override;
  bool find_value(const void *value) const override;
  bool compare_elems(uint pos1, uint pos2) const override;
};

class in_datetime_as_longlong final : public in_longlong {
 public:
  in_datetime_as_longlong(THD *thd, uint elements)
      : in_longlong(thd, elements){};
  Item *create_item() const override {
    return new Item_temporal(MYSQL_TYPE_DATETIME, 0LL);
  }
  void set(uint pos, Item *item) override;
  uchar *get_value(Item *item) override;
};

class in_time_as_longlong final : public in_longlong {
 public:
  in_time_as_longlong(THD *thd, uint elements) : in_longlong(thd, elements){};
  Item *create_item() const override {
    return new Item_temporal(MYSQL_TYPE_TIME, 0LL);
  }
  void set(uint pos, Item *item) override;
  uchar *get_value(Item *item) override;
};

/*
  Class to represent a vector of constant DATE/DATETIME values.
  Values are obtained with help of the get_datetime_value() function.
  If the left item is a constant one then its value is cached in the
  lval_cache variable.
*/
class in_datetime final : public in_longlong {
 public:
  /* An item used to issue warnings. */
  Item *warn_item;
  /* Cache for the left item. */
  Item *lval_cache;

  in_datetime(THD *thd_arg, Item *warn_item_arg, uint elements)
      : in_longlong(thd_arg, elements),
        warn_item(warn_item_arg),
        lval_cache(0){};
  void set(uint pos, Item *item) override;
  uchar *get_value(Item *item) override;

  Item *create_item() const override {
    return new Item_temporal(MYSQL_TYPE_DATETIME, 0LL);
  }
};

class in_double final : public in_vector {
  double tmp;
  Mem_root_array<double> base;

 public:
  in_double(THD *thd, uint elements);
  void set(uint pos, Item *item) override;
  uchar *get_value(Item *item) override;
  Item *create_item() const override { return new Item_float(0.0, 0); }
  void value_to_item(uint pos, Item *item) override {
    ((Item_float *)item)->value = base[pos];
  }
  Item_result result_type() const override { return REAL_RESULT; }

  void shrink_array(size_t n) override { base.resize(n); }

  void sort() override;
  bool find_value(const void *value) const override;
  bool compare_elems(uint pos1, uint pos2) const override;
};

class in_decimal final : public in_vector {
  my_decimal val;
  Mem_root_array<my_decimal> base;

 public:
  in_decimal(THD *thd, uint elements);
  void set(uint pos, Item *item) override;
  uchar *get_value(Item *item) override;
  Item *create_item() const override { return new Item_decimal(0, false); }
  void value_to_item(uint pos, Item *item) override {
    my_decimal *dec = &base[pos];
    Item_decimal *item_dec = (Item_decimal *)item;
    item_dec->set_decimal_value(dec);
  }
  Item_result result_type() const override { return DECIMAL_RESULT; }

  void shrink_array(size_t n) override { base.resize(n); }

  void sort() override;
  bool find_value(const void *value) const override;
  bool compare_elems(uint pos1, uint pos2) const override;
};

/*
** Classes for easy comparing of non const items
*/

class cmp_item {
 public:
  cmp_item() {}
  virtual ~cmp_item() {}
  virtual void store_value(Item *item) = 0;
  /**
     @returns result (true, false or UNKNOWN) of
     "stored argument's value <> item's value"
  */
  virtual int cmp(Item *item) = 0;
  // for optimized IN with row
  virtual int compare(const cmp_item *item) const = 0;

  /**
    Find the appropriate comparator for the given type.

    @param result_type  Used to find the appropriate comparator.
    @param item         Item object used to distinguish temporal types.
    @param cs           Charset

    @return
      New cmp_item_xxx object.
  */
  static cmp_item *get_comparator(Item_result result_type, const Item *item,
                                  const CHARSET_INFO *cs);
  virtual cmp_item *make_same() = 0;
  virtual void store_value_by_template(cmp_item *, Item *item) {
    store_value(item);
  }
};

/// cmp_item which stores a scalar (i.e. non-ROW).
class cmp_item_scalar : public cmp_item {
 protected:
  bool m_null_value;  ///< If stored value is NULL
  void set_null_value(bool nv) { m_null_value = nv; }
};

class cmp_item_string final : public cmp_item_scalar {
 private:
  String *value_res;
  char value_buff[STRING_BUFFER_USUAL_SIZE];
  String value;
  const CHARSET_INFO *cmp_charset;

 public:
  cmp_item_string(const CHARSET_INFO *cs)
      : value(value_buff, sizeof(value_buff), cs), cmp_charset(cs) {}

  virtual int compare(const cmp_item *ci) const {
    const cmp_item_string *l_cmp = down_cast<const cmp_item_string *>(ci);
    return sortcmp(value_res, l_cmp->value_res, cmp_charset);
  }

  virtual void store_value(Item *item) {
    String *res = item->val_str(&value);
    if (res && (res != &value || !res->is_alloced())) {
      // 'res' may point in item's transient internal data, so make a copy
      value.copy(*res);
    }
    value_res = &value;
    set_null_value(item->null_value);
  }

  virtual int cmp(Item *arg) {
    char buff[STRING_BUFFER_USUAL_SIZE];
    String tmp(buff, sizeof(buff), cmp_charset);
    String *res = arg->val_str(&tmp);
    if (m_null_value || arg->null_value) return UNKNOWN;
    if (value_res && res)
      return sortcmp(value_res, res, cmp_charset) != 0;
    else if (!value_res && !res)
      return false;
    else
      return true;
  }
  virtual cmp_item *make_same();
};

class cmp_item_int final : public cmp_item_scalar {
  longlong value;

 public:
  cmp_item_int() {} /* Remove gcc warning */
  void store_value(Item *item) override {
    value = item->val_int();
    set_null_value(item->null_value);
  }
  int cmp(Item *arg) override {
    const bool rc = value != arg->val_int();
    return (m_null_value || arg->null_value) ? UNKNOWN : rc;
  }
  int compare(const cmp_item *ci) const override {
    const cmp_item_int *l_cmp = down_cast<const cmp_item_int *>(ci);
    return (value < l_cmp->value) ? -1 : ((value == l_cmp->value) ? 0 : 1);
  }
  cmp_item *make_same() override;
};

/*
  Compare items of temporal type.
  Values are obtained with: get_datetime_value() (DATE/DATETIME/TIMESTAMP) and
                            get_time_value() (TIME).
  If the left item is a constant one then its value is cached in the
  lval_cache variable.
*/
class cmp_item_datetime : public cmp_item_scalar {
  longlong value;

 public:
  /* Item used for issuing warnings. */
  const Item *warn_item;
  /* Cache for the left item. */
  Item *lval_cache;
  /// Distinguish between DATE/DATETIME/TIMESTAMP and TIME
  bool has_date;

  cmp_item_datetime(const Item *warn_item_arg);
  void store_value(Item *item) override;
  int cmp(Item *arg) override;
  int compare(const cmp_item *ci) const override;
  cmp_item *make_same() override;
};

class cmp_item_real : public cmp_item_scalar {
  double value;

 public:
  cmp_item_real() {} /* Remove gcc warning */
  void store_value(Item *item) override {
    value = item->val_real();
    set_null_value(item->null_value);
  }
  int cmp(Item *arg) override {
    const bool rc = value != arg->val_real();
    return (m_null_value || arg->null_value) ? UNKNOWN : rc;
  }
  int compare(const cmp_item *ci) const override {
    const cmp_item_real *l_cmp = down_cast<const cmp_item_real *>(ci);
    return (value < l_cmp->value) ? -1 : ((value == l_cmp->value) ? 0 : 1);
  }
  cmp_item *make_same() override;
};

class cmp_item_decimal : public cmp_item_scalar {
  my_decimal value;

 public:
  cmp_item_decimal() {} /* Remove gcc warning */
  void store_value(Item *item);
  int cmp(Item *arg);
  int compare(const cmp_item *c) const;
  cmp_item *make_same();
};

/**
  CASE ... WHEN ... THEN ... END function implementation.

  When there is no expression between CASE and the first WHEN
  (the CASE expression) then this function simple checks all WHEN expressions
  one after another. When some WHEN expression evaluated to TRUE then the
  value of the corresponding THEN expression is returned.

  When the CASE expression is specified then it is compared to each WHEN
  expression individually. When an equal WHEN expression is found
  corresponding THEN expression is returned.
  In order to do correct comparisons several comparators are used. One for
  each result type. Different result types that are used in particular
  CASE ... END expression are collected in the resolve_type() member
  function and only comparators for there result types are used.
*/

class Item_func_case final : public Item_func {
  typedef Item_func super;

  int first_expr_num, else_expr_num;
  enum Item_result cached_result_type, left_result_type;
  String tmp_value;
  uint ncases;
  Item_result cmp_type;
  DTCollation cmp_collation;
  cmp_item *cmp_items[5]; /* For all result types */
  cmp_item *case_item;

 public:
  Item_func_case(const POS &pos, List<Item> &list, Item *first_expr_arg,
                 Item *else_expr_arg)
      : super(pos),
        first_expr_num(-1),
        else_expr_num(-1),
        cached_result_type(INT_RESULT),
        left_result_type(INT_RESULT),
        case_item(0) {
    null_on_null = false;
    ncases = list.elements;
    if (first_expr_arg) {
      first_expr_num = list.elements;
      list.push_back(first_expr_arg);
    }
    if (else_expr_arg) {
      else_expr_num = list.elements;
      list.push_back(else_expr_arg);
    }
    set_arguments(list, true);
    memset(&cmp_items, 0, sizeof(cmp_items));
  }
  double val_real() override;
  longlong val_int() override;
  String *val_str(String *) override;
  my_decimal *val_decimal(my_decimal *) override;
  bool val_json(Json_wrapper *wr) override;
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) override;
  bool get_time(MYSQL_TIME *ltime) override;
  bool fix_fields(THD *thd, Item **ref) override;
  bool resolve_type(THD *) override;
  uint decimal_precision() const override;
  enum Item_result result_type() const override { return cached_result_type; }
  const char *func_name() const override { return "case"; }
  void print(String *str, enum_query_type query_type) override;
  Item *find_item(String *str);
  const CHARSET_INFO *compare_collation() const override {
    return cmp_collation.collation;
  }
  void cleanup() override;
};

/**
  in_expr [NOT] IN (in_value_list).

  The current implementation distinguishes 2 cases:
  1) all items in in_value_list are constants and have the same
    result type. This case is handled by in_vector class.
  2) otherwise Item_func_in employs several cmp_item objects to perform
    comparisons of in_expr and an item from in_value_list. One cmp_item
    object for each result type. Different result types are collected in the
    resolve_type() member function by means of collect_cmp_types() function.
*/
class Item_func_in final : public Item_func_opt_neg {
 public:
  /// An array of values, created when the bisection lookup method is used
  in_vector *array;
  /**
    If there is some NULL among @<in value list@>, during a val_int() call; for
    example
    IN ( (1,(3,'col')), ... ), where 'col' is a column which evaluates to
    NULL.
  */
  bool have_null;
  /**
    Set to true by resolve_type() if the IN list contains a
    dependent subquery, in which case condition filtering will not be
    calculated for this item.
  */
  bool dep_subq_in_list;
  Item_result left_result_type;
  cmp_item *cmp_items[6]; /* One cmp_item for each result type */
  DTCollation cmp_collation;

  Item_func_in(const POS &pos, PT_item_list *list, bool is_negation)
      : Item_func_opt_neg(pos, list, is_negation),
        array(NULL),
        have_null(false),
        dep_subq_in_list(false) {
    memset(&cmp_items, 0, sizeof(cmp_items));
    allowed_arg_cols = 0;  // Fetch this value from first argument
  }
  longlong val_int() override;
  bool fix_fields(THD *, Item **) override;
  void fix_after_pullout(SELECT_LEX *parent_select,
                         SELECT_LEX *removed_select) override;
  bool resolve_type(THD *) override;
  uint decimal_precision() const override { return 1; }

  /**
    Cleanup data and comparator arrays.

    @note Used during regular cleanup and to free arrays after GC substitution.
    @see substitute_gc().
  */
  void cleanup_arrays() {
    uint i;
    destroy(array);
    array = 0;
    for (i = 0; i <= (uint)DECIMAL_RESULT + 1; i++) {
      destroy(cmp_items[i]);
      cmp_items[i] = 0;
    }
  }

  void cleanup() override {
    DBUG_ENTER("Item_func_in::cleanup");
    Item_int_func::cleanup();
    cleanup_arrays();
    DBUG_VOID_RETURN;
  }
  optimize_type select_optimize() const override { return OPTIMIZE_KEY; }
  void print(String *str, enum_query_type query_type) override;
  enum Functype functype() const override { return IN_FUNC; }
  const char *func_name() const override { return " IN "; }
  bool is_bool_func() const override { return true; }
  const CHARSET_INFO *compare_collation() const override {
    return cmp_collation.collation;
  }
  bool gc_subst_analyzer(uchar **) override { return true; }

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;

 private:
  /**
     Usable if @<in value list@> is made only of constants. Returns true if one
     of these constants contains a NULL. Example:
     IN ( (-5, (12,NULL)), ... ).
  */
  bool list_contains_null();
  /**
    Utility function to help calculate the total filtering effect of
    IN predicates. This function calculates the filtering effect from
    a single field (or field reference) on the left hand side of the
    expression.

    @param fieldref          Field (or field reference) on left hand side of
                             IN, i.e., this function should be called for
                             each fi in "(f1,...,fn) IN (values)"
    @param filter_for_table  The table we are calculating filter effect for
    @param fields_to_ignore  Fields in 'filter_for_table' that should not
                             be part of the filter calculation. The filtering
                             effect of these fields are already part of the
                             calculation somehow (e.g. because there is a
                             predicate "col = <const>", and the optimizer
                             has decided to do ref access on 'col').
    @param rows_in_table     The number of rows in table 'filter_for_table'

    @return                  the filtering effect (between 0 and 1) 'the_field'
                             participates with in this IN predicate.
  */
  float get_single_col_filtering_effect(Item_ident *fieldref,
                                        table_map filter_for_table,
                                        const MY_BITMAP *fields_to_ignore,
                                        double rows_in_table);
};

class cmp_item_row : public cmp_item {
  cmp_item **comparators;
  uint n;

 public:
  cmp_item_row() : comparators(0), n(0) {}
  ~cmp_item_row();
  void store_value(Item *item);
  bool alloc_comparators(Item *item);
  int cmp(Item *arg);
  int compare(const cmp_item *arg) const;
  cmp_item *make_same();
  void store_value_by_template(cmp_item *tmpl, Item *);
  friend bool Item_func_in::resolve_type(THD *thd);
};

class in_row final : public in_vector {
  cmp_item_row tmp;
  Mem_root_array<cmp_item_row> base_objects;
  // Sort pointers, rather than objects.
  Mem_root_array<cmp_item_row *> base_pointers;

 public:
  in_row(THD *thd, uint elements);
  void set(uint pos, Item *item) override;
  uchar *get_value(Item *item) override;
  friend bool Item_func_in::resolve_type(THD *thd);
  Item_result result_type() const override { return ROW_RESULT; }

  void shrink_array(size_t n) override { base_pointers.resize(n); }

  void sort() override;
  bool find_value(const void *value) const override;
  bool compare_elems(uint pos1, uint pos2) const override;
};

/* Functions used by where clause */

class Item_func_isnull : public Item_bool_func {
 protected:
  longlong cached_value;

 public:
  Item_func_isnull(Item *a) : Item_bool_func(a) { null_on_null = false; }
  Item_func_isnull(const POS &pos, Item *a) : Item_bool_func(pos, a) {
    null_on_null = false;
  }
  longlong val_int() override;
  enum Functype functype() const override { return ISNULL_FUNC; }
  bool resolve_type(THD *thd) override;
  const char *func_name() const override { return "isnull"; }
  /* Optimize case of not_null_column IS NULL */
  void update_used_tables() override;

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;
  optimize_type select_optimize() const override { return OPTIMIZE_NULL; }
  Item *neg_transformer(THD *thd) override;
  const CHARSET_INFO *compare_collation() const override {
    return args[0]->collation.collation;
  }
};

/* Functions used by HAVING for rewriting IN subquery */

/*
  This is like IS NOT NULL but it also remembers if it ever has
  encountered a NULL; it remembers this in the "was_null" property of the
  "owner" item.
*/
class Item_is_not_null_test final : public Item_func_isnull {
  Item_in_subselect *owner;

 public:
  Item_is_not_null_test(Item_in_subselect *ow, Item *a)
      : Item_func_isnull(a), owner(ow) {}
  enum Functype functype() const override { return ISNOTNULLTEST_FUNC; }
  longlong val_int() override;
  const char *func_name() const override { return "<is_not_null_test>"; }
  void update_used_tables() override;
  /**
    We add RAND_TABLE_BIT to prevent moving this item from HAVING to WHERE.

    @retval Always RAND_TABLE_BIT
  */
  table_map get_initial_pseudo_tables() const override {
    return RAND_TABLE_BIT;
  }
};

class Item_func_isnotnull final : public Item_bool_func {
  bool abort_on_null;

 public:
  Item_func_isnotnull(Item *a) : Item_bool_func(a), abort_on_null(0) {}
  Item_func_isnotnull(const POS &pos, Item *a)
      : Item_bool_func(pos, a), abort_on_null(0) {}

  longlong val_int() override;
  enum Functype functype() const override { return ISNOTNULL_FUNC; }
  bool resolve_type(THD *) override {
    max_length = 1;
    maybe_null = false;
    return false;
  }
  const char *func_name() const override { return "isnotnull"; }
  optimize_type select_optimize() const override { return OPTIMIZE_NULL; }
  table_map not_null_tables() const override {
    return abort_on_null ? not_null_tables_cache : 0;
  }
  Item *neg_transformer(THD *thd) override;
  void print(String *str, enum_query_type query_type) override;
  const CHARSET_INFO *compare_collation() const override {
    return args[0]->collation.collation;
  }
  void top_level_item() override { abort_on_null = true; }

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;
};

class Item_func_like final : public Item_bool_func2 {
  typedef Item_bool_func2 super;

  Item *escape_item;

  bool escape_used_in_parsing;

  bool escape_evaluated;  ///< Tells if the escape clause has been evaluated.
  bool eval_escape_clause(THD *thd);

 public:
  int escape;

  Item_func_like(Item *a, Item *b, Item *escape_arg, bool escape_used)
      : Item_bool_func2(a, b),
        escape_item(escape_arg),
        escape_used_in_parsing(escape_used),
        escape_evaluated(false) {}
  Item_func_like(const POS &pos, Item *a, Item *b, Item *opt_escape_arg)
      : super(pos, a, b),
        escape_item(opt_escape_arg),
        escape_used_in_parsing(opt_escape_arg != NULL),
        escape_evaluated(false) {}

  bool itemize(Parse_context *pc, Item **res) override;

  longlong val_int() override;
  enum Functype functype() const override { return LIKE_FUNC; }
  optimize_type select_optimize() const override;
  cond_result eq_cmp_result() const override { return COND_TRUE; }
  const char *func_name() const override { return "like"; }
  bool fix_fields(THD *thd, Item **ref) override;
  void cleanup() override;
  /**
    @retval true non default escape char specified
                 using "expr LIKE pat ESCAPE 'escape_char'" syntax
  */
  bool escape_was_used_in_parsing() const { return escape_used_in_parsing; }

  /**
    Has the escape clause been evaluated? It only needs to be evaluated
    once per execution, since we require it to be constant during execution.
    The escape member has a valid value if and only if this function returns
    true.
  */
  bool escape_is_evaluated() const { return escape_evaluated; }

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;

 private:
  /**
    The method updates covering keys depending on the
    length of wild string prefix.

    @param thd Pointer to THD object.

    @retval true if error happens during wild string prefix claculation,
            false otherwise.
  */
  bool check_covering_prefix_keys(THD *thd);
};

class Item_cond : public Item_bool_func {
  typedef Item_bool_func super;

 protected:
  List<Item> list;
  bool abort_on_null;

 public:
  /* Item_cond() is only used to create top level items */
  Item_cond() : Item_bool_func(), abort_on_null(1) {}
  Item_cond(Item *i1, Item *i2) : Item_bool_func(), abort_on_null(0) {
    list.push_back(i1);
    list.push_back(i2);
  }
  Item_cond(const POS &pos, Item *i1, Item *i2)
      : Item_bool_func(pos), abort_on_null(0) {
    list.push_back(i1);
    list.push_back(i2);
  }

  Item_cond(THD *thd, Item_cond *item);
  Item_cond(List<Item> &nlist)
      : Item_bool_func(), list(nlist), abort_on_null(0) {}
  bool add(Item *item) {
    DBUG_ASSERT(item);
    return list.push_back(item);
  }
  bool add_at_head(Item *item) {
    DBUG_ASSERT(item);
    return list.push_front(item);
  }
  void add_at_head(List<Item> *nlist) {
    DBUG_ASSERT(nlist->elements);
    list.prepend(nlist);
  }

  bool itemize(Parse_context *pc, Item **res) override;

  bool fix_fields(THD *, Item **ref) override;
  void fix_after_pullout(SELECT_LEX *parent_select,
                         SELECT_LEX *removed_select) override;

  Type type() const override { return COND_ITEM; }
  List<Item> *argument_list() { return &list; }
  bool eq(const Item *item, bool binary_cmp) const override;
  table_map used_tables() const override { return used_tables_cache; }
  void update_used_tables() override;
  void print(String *str, enum_query_type query_type) override;
  void split_sum_func(THD *thd, Ref_item_array ref_item_array,
                      List<Item> &fields) override;
  void top_level_item() override { abort_on_null = true; }
  void copy_andor_arguments(THD *thd, Item_cond *item);
  bool walk(Item_processor processor, enum_walk walk, uchar *arg) override;
  Item *transform(Item_transformer transformer, uchar *arg) override;
  void traverse_cond(Cond_traverser, void *arg, traverse_order order) override;
  void neg_arguments(THD *thd);
  bool subst_argument_checker(uchar **) override { return true; }
  Item *compile(Item_analyzer analyzer, uchar **arg_p,
                Item_transformer transformer, uchar *arg_t) override;

  bool equality_substitution_analyzer(uchar **) override { return true; }
};

/*
  The class Item_equal is used to represent conjunctions of equality
  predicates of the form field1 = field2, and field=const in where
  conditions and on expressions.

  All equality predicates of the form field1=field2 contained in a
  conjunction are substituted for a sequence of items of this class.
  An item of this class Item_equal(f1,f2,...fk) represents a
  multiple equality f1=f2=...=fk.

  If a conjunction contains predicates f1=f2 and f2=f3, a new item of
  this class is created Item_equal(f1,f2,f3) representing the multiple
  equality f1=f2=f3 that substitutes the above equality predicates in
  the conjunction.
  A conjunction of the predicates f2=f1 and f3=f1 and f3=f2 will be
  substituted for the item representing the same multiple equality
  f1=f2=f3.
  An item Item_equal(f1,f2) can appear instead of a conjunction of
  f2=f1 and f1=f2, or instead of just the predicate f1=f2.

  An item of the class Item_equal inherits equalities from outer
  conjunctive levels.

  Suppose we have a where condition of the following form:
  WHERE f1=f2 AND f3=f4 AND f3=f5 AND ... AND (...OR (f1=f3 AND ...)).
  In this case:
    f1=f2 will be substituted for Item_equal(f1,f2);
    f3=f4 and f3=f5  will be substituted for Item_equal(f3,f4,f5);
    f1=f3 will be substituted for Item_equal(f1,f2,f3,f4,f5);

  An object of the class Item_equal can contain an optional constant
  item c. Then it represents a multiple equality of the form
  c=f1=...=fk.

  Objects of the class Item_equal are used for the following:

  1. An object Item_equal(t1.f1,...,tk.fk) allows us to consider any
  pair of tables ti and tj as joined by an equi-condition.
  Thus it provide us with additional access paths from table to table.

  2. An object Item_equal(t1.f1,...,tk.fk) is applied to deduce new
  SARGable predicates:
    f1=...=fk AND P(fi) => f1=...=fk AND P(fi) AND P(fj).
  It also can give us additional index scans and can allow us to
  improve selectivity estimates.

  3. An object Item_equal(t1.f1,...,tk.fk) is used to optimize the
  selected execution plan for the query: if table ti is accessed
  before the table tj then in any predicate P in the where condition
  the occurrence of tj.fj is substituted for ti.fi. This can allow
  an evaluation of the predicate at an earlier step.

  When feature 1 is supported they say that join transitive closure
  is employed.
  When feature 2 is supported they say that search argument transitive
  closure is employed.
  Both features are usually supported by preprocessing original query and
  adding additional predicates.
  We do not just add predicates, we rather dynamically replace some
  predicates that can not be used to access tables in the investigated
  plan for those, obtained by substitution of some fields for equal fields,
  that can be used.

  Prepared Statements/Stored Procedures note: instances of class
  Item_equal are created only at the time a PS/SP is executed and
  are deleted in the end of execution. All changes made to these
  objects need not be registered in the list of changes of the parse
  tree and do not harm PS/SP re-execution.

  Item equal objects are employed only at the optimize phase. Usually they are
  not supposed to be evaluated.  Yet in some cases we call the method val_int()
  for them. We have to take care of restricting the predicate such an
  object represents f1=f2= ...=fn to the projection of known fields fi1=...=fik.
*/
class Item_equal final : public Item_bool_func {
  List<Item_field> fields; /* list of equal field items                    */
  Item *const_item;        /* optional constant item equal to fields items */
  cmp_item *eval_item;
  Arg_comparator cmp;
  bool cond_false;
  bool compare_as_dates;

 public:
  inline Item_equal()
      : Item_bool_func(), const_item(0), eval_item(0), cond_false(0) {}
  Item_equal(Item_field *f1, Item_field *f2);
  Item_equal(Item *c, Item_field *f);
  Item_equal(Item_equal *item_equal);
  virtual ~Item_equal() { destroy(eval_item); }

  inline Item *get_const() { return const_item; }
  bool compare_const(THD *thd, Item *c);
  bool add(THD *thd, Item *c, Item_field *f);
  bool add(THD *thd, Item *c);
  void add(Item_field *f);
  uint members();
  bool contains(Field *field);
  /**
    Get the first field of multiple equality, use for semantic checking.

    @retval First field in the multiple equality.
  */
  Item_field *get_first() { return fields.head(); }
  Item_field *get_subst_item(const Item_field *field);
  bool merge(THD *thd, Item_equal *item);
  bool update_const(THD *thd);
  enum Functype functype() const override { return MULT_EQUAL_FUNC; }
  longlong val_int() override;
  const char *func_name() const override { return "multiple equal"; }
  optimize_type select_optimize() const override { return OPTIMIZE_EQUAL; }
  /**
    Order field items in multiple equality according to a sorting criteria.

    The function perform ordering of the field items in the Item_equal
    object according to the criteria determined by the cmp callback parameter.
    If cmp(item_field1,item_field2,arg)<0 than item_field1 must be
    placed after item_field2.

    The function sorts field items by the exchange sort algorithm.
    The list of field items is looked through and whenever two neighboring
    members follow in a wrong order they are swapped. This is performed
    again and again until we get all members in a right order.

    @param compare      function to compare field item
  */
  template <typename Node_cmp_func>
  void sort(Node_cmp_func compare) {
    fields.sort(compare);
  }
  friend class Item_equal_iterator;
  bool resolve_type(THD *) override;
  bool fix_fields(THD *thd, Item **ref) override;
  void update_used_tables() override;
  bool walk(Item_processor processor, enum_walk walk, uchar *arg) override;
  Item *transform(Item_transformer transformer, uchar *arg) override;
  void print(String *str, enum_query_type query_type) override;
  const CHARSET_INFO *compare_collation() const override {
    return fields.head()->collation.collation;
  }

  bool equality_substitution_analyzer(uchar **) override { return true; }

  Item *equality_substitution_transformer(uchar *arg) override;

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;

 private:
  void check_covering_prefix_keys();
};

class COND_EQUAL {
 public:
  uint max_members;               /* max number of members the current level
                                     list and all lower level lists */
  COND_EQUAL *upper_levels;       /* multiple equalities of upper and levels */
  List<Item_equal> current_level; /* list of multiple equalities of
                                     the current and level           */
  COND_EQUAL() { upper_levels = 0; }
};

class Item_equal_iterator : public List_iterator_fast<Item_field> {
 public:
  inline Item_equal_iterator(Item_equal &item_equal)
      : List_iterator_fast<Item_field>(item_equal.fields) {}
  inline Item_field *operator++(int) {
    Item_field *item = (*(List_iterator_fast<Item_field> *)this)++;
    return item;
  }
  inline void rewind(void) { List_iterator_fast<Item_field>::rewind(); }
};

class Item_cond_and final : public Item_cond {
 public:
  COND_EQUAL cond_equal; /* contains list of Item_equal objects for
                            the current and level and reference
                            to multiple equalities of upper and levels */
  Item_cond_and() : Item_cond() {}

  Item_cond_and(Item *i1, Item *i2) : Item_cond(i1, i2) {}
  Item_cond_and(const POS &pos, Item *i1, Item *i2) : Item_cond(pos, i1, i2) {}

  Item_cond_and(THD *thd, Item_cond_and *item) : Item_cond(thd, item) {}
  Item_cond_and(List<Item> &list_arg) : Item_cond(list_arg) {}
  enum Functype functype() const override { return COND_AND_FUNC; }
  longlong val_int() override;
  const char *func_name() const override { return "and"; }
  Item *copy_andor_structure(THD *thd) override {
    Item_cond_and *item;
    if ((item = new Item_cond_and(thd, this)))
      item->copy_andor_arguments(thd, this);
    return item;
  }
  Item *neg_transformer(THD *thd) override;
  bool gc_subst_analyzer(uchar **) override { return true; }

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;
};

class Item_cond_or final : public Item_cond {
 public:
  Item_cond_or() : Item_cond() {}

  Item_cond_or(Item *i1, Item *i2) : Item_cond(i1, i2) {}
  Item_cond_or(const POS &pos, Item *i1, Item *i2) : Item_cond(pos, i1, i2) {}

  Item_cond_or(THD *thd, Item_cond_or *item) : Item_cond(thd, item) {}
  Item_cond_or(List<Item> &list_arg) : Item_cond(list_arg) {}
  enum Functype functype() const override { return COND_OR_FUNC; }
  longlong val_int() override;
  const char *func_name() const override { return "or"; }
  Item *copy_andor_structure(THD *thd) override {
    Item_cond_or *item;
    if ((item = new Item_cond_or(thd, this)))
      item->copy_andor_arguments(thd, this);
    return item;
  }
  Item *neg_transformer(THD *thd) override;
  bool gc_subst_analyzer(uchar **) override { return true; }

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;
};

/* Some useful inline functions */

inline Item *and_conds(Item *a, Item *b) {
  if (!b) return a;
  if (!a) return b;
  return new Item_cond_and(a, b);
}

longlong get_datetime_value(THD *thd, Item ***item_arg, Item **cache_arg,
                            const Item *warn_item, bool *is_null);

bool get_mysql_time_from_str(THD *thd, String *str, timestamp_type warn_type,
                             const char *warn_name, MYSQL_TIME *l_time);
/*
  These need definitions from this file but the variables are defined
  in mysqld.h. The variables really belong in this component, but for
  the time being we leave them in mysqld.cc to avoid merge problems.
*/
extern Eq_creator eq_creator;
extern Equal_creator equal_creator;
extern Ne_creator ne_creator;
extern Gt_creator gt_creator;
extern Lt_creator lt_creator;
extern Ge_creator ge_creator;
extern Le_creator le_creator;

#endif /* ITEM_CMPFUNC_INCLUDED */
