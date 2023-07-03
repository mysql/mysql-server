#ifndef ITEM_CMPFUNC_INCLUDED
#define ITEM_CMPFUNC_INCLUDED

/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.

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

#include <assert.h>
#include <sys/types.h>

#include <cstring>
#include <memory>

#include "field_types.h"
#include "my_alloc.h"
#include "my_compiler.h"

#include "my_inttypes.h"
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
#include "sql/parse_location.h"  // POS
#include "sql/sql_const.h"
#include "sql/sql_list.h"
#include "sql/table.h"
#include "sql_string.h"
#include "template_utils.h"  // down_cast

class Arg_comparator;
class Field;
class Item_eq_base;
class Item_in_subselect;
class Item_subselect;
class Item_sum_hybrid;
class Json_scalar_holder;
class Json_wrapper;
class PT_item_list;
class Query_block;
class THD;
struct CHARSET_INFO;
struct MY_BITMAP;
struct Parse_context;

Item *make_condition(Parse_context *pc, Item *item);

typedef int (Arg_comparator::*arg_cmp_func)();

/// A class that represents a join condition in a hash join. The class holds an
/// equality condition, as well as a pre-calculated bitmap of the used tables
/// (Item::used_tables()) for each side of the condition.
///
/// The class also contains one Item for each side of the condition. In most
/// cases, the Item is only a pointer to the left/right Item of the join
/// condition. But for certain data types (DECIMAL, DOUBLE(M, N), FLOAT(M, N)),
/// the Item might be a typecast. Either way, the caller should use these Items
/// when i.e. reading the values from the join condition, so that the values are
/// read in the right data type context. See the comments for
/// Item_eq_base::create_cast_if_needed for more details around this.
class HashJoinCondition {
 public:
  HashJoinCondition(Item_eq_base *join_condition, MEM_ROOT *mem_root);

  Item_eq_base *join_condition() const { return m_join_condition; }

  Item *left_extractor() const { return m_left_extractor; }
  Item *right_extractor() const { return m_right_extractor; }
  bool left_uses_any_table(table_map tables) const {
    return (m_left_used_tables & tables) != 0;
  }

  bool right_uses_any_table(table_map tables) const {
    return (m_right_used_tables & tables) != 0;
  }

  size_t max_character_length() const { return m_max_character_length; }

  bool store_full_sort_key() const { return m_store_full_sort_key; }

  /// Returns true if this join condition evaluates to TRUE if both
  /// operands are NULL.
  bool null_equals_null() const { return m_null_equals_null; }

 private:
  Item_eq_base *m_join_condition;
  Item *m_left_extractor;
  Item *m_right_extractor;

  // Item::used_tables() is heavily used during the join to determine which side
  // of the condition we are to read the value from, so caching the result of
  // used_tables() gives a nice speedup.
  const table_map m_left_used_tables;
  const table_map m_right_used_tables;

  // The maximum number of characters among the two arguments. This is
  // especially relevant when we have a PAD SPACE collation and the SQL mode
  // PAD_CHAR_TO_FULL_LENGTH enabled, since we will have to pad the shortest
  // argument to the same length as the longest argument.
  const size_t m_max_character_length{0};

  // Normally, we store the full sort key for the condition as key in the hash
  // table. However, if the string is very long, or we have a PAD SPACE
  // collation, this could result in huge sort keys. If we detect that this
  // could happen in the worst case, we store just a hash in the key instead (so
  // we hash the hash). If so, we have to do a recheck afterwards, in order to
  // guard against hash collisions.
  bool m_store_full_sort_key;

  // True if NULL is considered equal to NULL, and not as UNKNOWN.
  bool m_null_equals_null;
};

class Arg_comparator {
  Item **left{nullptr};
  Item **right{nullptr};
  arg_cmp_func func{nullptr};
  Item_result_field *owner{nullptr};
  Arg_comparator *comparators{nullptr};  // used only for compare_row()
  uint16 comparator_count{0};
  double precision{0.0};
  /* Fields used in DATE/DATETIME comparison. */
  Item *left_cache{nullptr};  // Cached values of "left" and "right" items
  Item *right_cache{nullptr};
  bool set_null{true};  // true <=> set owner->null_value
                        //   when one of arguments is NULL.

  bool try_year_cmp_func(Item_result type);
  static bool get_date_from_const(Item *date_arg, Item *str_arg,
                                  ulonglong *const_value);
  /**
    Only used by compare_json() in the case where a JSON value is
    compared to an SQL value. This member points to pre-allocated
    memory that can be used instead of the heap when converting the
    SQL value to a JSON value.
  */
  Json_scalar_holder *json_scalar{nullptr};

 public:
  DTCollation cmp_collation;
  /* Allow owner function to use string buffers. */
  String value1, value2;

  Arg_comparator() = default;

  Arg_comparator(Item **left, Item **right) : left(left), right(right) {}

  bool set_compare_func(Item_result_field *owner, Item_result type);
  bool set_cmp_func(Item_result_field *owner_arg, Item **left, Item **right,
                    Item_result type);

  bool set_cmp_func(Item_result_field *owner_arg, Item **left, Item **right,
                    bool set_null_arg);

  bool set_cmp_func(Item_result_field *owner_arg, Item **left, Item **right,
                    bool set_null_arg, Item_result type);
  /**
     Comparison function are expected to operate on arguments having the
     same data types. Since MySQL has very loosened up rules, it accepts
     all kind of arguments which the standard SQL does not allow, and then it
     converts the arguments internally to ones usable in the comparison.
     This function transforms these internal conversions to explicit CASTs
     so that the internally executed query becomes compatible with the standard
     At the moment nodes are injected only for comparisons between:

        1) temporal types and numeric data types: in which case the
        comparison is normally done as DOUBLE, so the arguments which are not
        floating point, will get converted to DOUBLE, and also for

        2) comparisons between temporal types: in which case the
        comparison happens as DATETIME if the arguments have different data
        types, so in this case the temporal arguments that are not DATETIME
        will get wrapped in a CAST to DATETIME.

     WL#12108; This function will limit itself to comparison between regular
     functions, aggregation functions and fields, all of which are constant
     for execution (so this excludes stored procedures, stored functions, GC,
     user defined functions, as well as literals).
     For const arguments, see type conversions done in fold_condition.

     @return false if successful, true otherwise
  */
  bool inject_cast_nodes();

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
  int compare_row();  // compare args[0] & args[1]
  int compare_real_fixed();
  int compare_datetime();  // compare args[0] & args[1] as DATETIMEs
  int compare_json();
  bool compare_null_values();

  static bool can_compare_as_dates(const Item *a, const Item *b);

  void set_datetime_cmp_func(Item_result_field *owner_arg, Item **a1,
                             Item **b1);
  static arg_cmp_func comparator_matrix[5];
  void cleanup();
  /*
    Set correct cmp_context if items would be compared as INTs.
  */
  inline void set_cmp_context_for_datetime() {
    assert(func == &Arg_comparator::compare_datetime);
    if ((*left)->is_temporal()) (*left)->cmp_context = INT_RESULT;
    if ((*right)->is_temporal()) (*right)->cmp_context = INT_RESULT;
  }

  Item_result get_compare_type() const { return m_compare_type; }

  uint get_child_comparator_count() const { return comparator_count; }

  Arg_comparator *get_child_comparators() const { return comparators; }

  /// @returns true if the class has decided that values should be extracted
  ///   from the Items using function pointers set up by this class.
  bool use_custom_value_extractors() const {
    return get_value_a_func != nullptr;
  }

  // Read the value from one of the Items (decided by "left_argument"), using
  // the function pointers that this class has set up. This can happen for DATE,
  // TIME, DATETIME and YEAR values, and the returned value is a temporal value
  // in packed format.
  longlong extract_value_from_argument(THD *thd, Item *item, bool left_argument,
                                       bool *is_null) const;

  Item **get_left_ptr() const { return left; }
  Item *get_right() const { return *right; }

 private:
  /// A function pointer that is used for retrieving the value from argument
  /// "left". This function is only used when we are comparing in a datetime
  /// context, and it retrieves the value as a DATE, TIME, DATETIME or YEAR,
  /// depending on the comparison context.
  ///
  /// @param thd thread handle. Used to retrieve the SQL mode among other things
  /// @param item_arg the item to retrieve the value from
  /// @param cache_arg a pointer to an Item where we can cache the value
  ///   from "item_arg". Can be nullptr
  /// @param warn_item if raising an conversion warning, the warning gets the
  ///   data type and item name from this item
  /// @param is_null whether or not "item_arg" returned SQL NULL
  ///
  /// @returns a DATE/TIME/YEAR/DATETIME value, in packed format
  longlong (*get_value_a_func)(THD *thd, Item ***item_arg, Item **cache_arg,
                               const Item *warn_item, bool *is_null){nullptr};

  // This function does the same as "get_value_a_func", except that it returns
  // the value from the argument "right" (the right side of the comparison).
  longlong (*get_value_b_func)(THD *thd, Item ***item_arg, Item **cache_arg,
                               const Item *warn_item, bool *is_null){nullptr};

  // The data type that is used when comparing the two Items. I.e., if the type
  // is INT_RESULT, we call val_int() on both sides and compare those.
  Item_result m_compare_type{INVALID_RESULT};
};

class Item_bool_func : public Item_int_func {
 protected:
  Item_bool_func() : Item_int_func() { set_data_type_bool(); }
  explicit Item_bool_func(const POS &pos) : Item_int_func(pos) {
    set_data_type_bool();
  }

  explicit Item_bool_func(Item *a) : Item_int_func(a) { set_data_type_bool(); }
  Item_bool_func(const POS &pos, Item *a) : Item_int_func(pos, a) {
    set_data_type_bool();
  }

  Item_bool_func(Item *a, Item *b, Item *c) : Item_int_func(a, b, c) {
    set_data_type_bool();
  }
  Item_bool_func(Item *a, Item *b) : Item_int_func(a, b) {
    set_data_type_bool();
  }
  Item_bool_func(const POS &pos, Item *a, Item *b) : Item_int_func(pos, a, b) {
    set_data_type_bool();
  }
  Item_bool_func(const POS &pos, Item *a, Item *b, Item *c)
      : Item_int_func(pos, a, b, c) {
    set_data_type_bool();
  }

  Item_bool_func(THD *thd, Item_bool_func *item)
      : Item_int_func(thd, item),
        m_created_by_in2exists(item->m_created_by_in2exists) {
    set_data_type_bool();
  }

 public:
  bool is_bool_func() const override { return true; }
  bool resolve_type(THD *thd) override {
    max_length = 1;
    return Item_int_func::resolve_type(thd);
  }
  uint decimal_precision() const override { return 1; }
  bool created_by_in2exists() const override { return m_created_by_in2exists; }
  void set_created_by_in2exists() { m_created_by_in2exists = true; }

  static const char *bool_transform_names[10];
  /**
    Array that transforms a boolean test according to another.
    First dimension is existing value, second dimension is test to apply
  */
  static const Bool_test bool_transform[10][8];

 private:
  /**
    True <=> this item was added by IN->EXISTS subquery transformation, and
    should thus be deleted if we switch to materialization.
  */
  bool m_created_by_in2exists{false};
};

/**
  A predicate that is "always true" or "always false". To be used as a
  standalone condition or as part of conditions, together with other condition
  and predicate objects.
  Mostly used when generating conditions internally.
*/
class Item_func_bool_const : public Item_bool_func {
 public:
  Item_func_bool_const() : Item_bool_func() {
    max_length = 1;
    used_tables_cache = 0;
    not_null_tables_cache = 0;
    fixed = true;
  }
  explicit Item_func_bool_const(const POS &pos) : Item_bool_func(pos) {
    max_length = 1;
    used_tables_cache = 0;
    not_null_tables_cache = 0;
    fixed = true;
  }
  bool fix_fields(THD *, Item **) override { return false; }
  bool basic_const_item() const override { return true; }
  void cleanup() override { result_field = nullptr; }
};

/// A predicate that is "always true".

class Item_func_true : public Item_func_bool_const {
 public:
  Item_func_true() : Item_func_bool_const() {}
  explicit Item_func_true(const POS &pos) : Item_func_bool_const(pos) {}
  const char *func_name() const override { return "true"; }
  bool val_bool() override { return true; }
  longlong val_int() override { return 1; }
  void print(const THD *, String *str, enum_query_type) const override {
    str->append("true");
  }
  enum Functype functype() const override { return TRUE_FUNC; }
};

/// A predicate that is "always false".

class Item_func_false : public Item_func_bool_const {
 public:
  Item_func_false() : Item_func_bool_const() {}
  explicit Item_func_false(const POS &pos) : Item_func_bool_const(pos) {}
  const char *func_name() const override { return "false"; }
  bool val_bool() override { return false; }
  longlong val_int() override { return 0; }
  void print(const THD *, String *str, enum_query_type) const override {
    str->append("false");
  }
  enum Functype functype() const override { return FALSE_FUNC; }
};

/**
  Item class, to represent <code>X IS [NOT] (TRUE | FALSE)</code>
  boolean predicates.
*/
class Item_func_truth final : public Item_bool_func {
  typedef Item_bool_func super;

 public:
  longlong val_int() override;
  bool resolve_type(THD *) override;
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
  Item *truth_transformer(THD *, Bool_test test) override {
    truth_test = super::bool_transform[truth_test][test];
    return this;
  }
  const char *func_name() const override {
    return super::bool_transform_names[truth_test];
  }
  enum Functype functype() const override { return ISTRUTH_FUNC; }

  Item_func_truth(const POS &pos, Item *a, Bool_test truth_test)
      : super(pos, a), truth_test(truth_test) {
    null_on_null = false;
    switch (truth_test) {
      case BOOL_IS_TRUE:
      case BOOL_IS_FALSE:
      case BOOL_NOT_TRUE:
      case BOOL_NOT_FALSE:
        break;
      default:
        assert(false);
    }
  }
  Item_func_truth(Item *a, Bool_test truth_test)
      : super(a), truth_test(truth_test) {
    null_on_null = false;
    switch (truth_test) {
      case BOOL_IS_TRUE:
      case BOOL_IS_FALSE:
      case BOOL_NOT_TRUE:
      case BOOL_NOT_FALSE:
        break;
      default:
        assert(false);
    }
  }
  void apply_is_true() override {
    /*
      This item cannot produce NULL result. But, if the upper item confuses
      NULL and FALSE, we can do as if NULL input caused a NULL result when it
      actually causes a FALSE result.
    */
    switch (truth_test) {
      case BOOL_IS_TRUE:
      case BOOL_IS_FALSE:
        null_on_null = true;
      default:
        break;
    }
  }

 protected:
  Bool_test truth_test;  ///< The value we're testing for.
};

static const int UNKNOWN = -1;

/*
  Item_in_optimizer(Item_in_subselect(...))

  Item_in_optimizer is used to wrap an instance of Item_in_subselect. This
  class does the following:
   - Evaluate the left expression and store it in Item_cache_* object (to
     avoid re-evaluating it many times during subquery execution)
   - Shortcut the evaluation of "NULL IN (...)" to NULL in the cases where we
     don't care if the result is NULL or FALSE.

   args[0] keeps a reference to the Item_in_subselect object.

  NOTE
    It is not quite clear why the above listed functionality should be
    placed into a separate class called 'Item_in_optimizer'.
*/

class Item_in_optimizer final : public Item_bool_func {
 private:
  Item_cache *cache{nullptr};
  /**
    Stores the value of "NULL IN (SELECT ...)" for uncorrelated subqueries:
      UNKNOWN - "NULL in (SELECT ...)" has not yet been evaluated
      FALSE   - result is FALSE
      TRUE    - result is NULL
  */
  int result_for_null_param{UNKNOWN};

 public:
  Item_in_optimizer(Item_in_subselect *item)
      : Item_bool_func(pointer_cast<Item *>(item)) {
    set_subquery();
  }
  bool fix_fields(THD *, Item **) override;
  bool fix_left(THD *thd, Item **ref);
  void fix_after_pullout(Query_block *parent_query_block,
                         Query_block *removed_query_block) override;
  void split_sum_func(THD *thd, Ref_item_array ref_item_array,
                      mem_root_deque<Item *> *fields) override;
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
  bool is_null() override;
  longlong val_int() override;
  void cleanup() override;
  const char *func_name() const override { return "<in_optimizer>"; }
  Item_cache **get_cache() { return &cache; }
  void update_used_tables() override;
};

/// Abstract factory interface for creating comparison predicates.
class Comp_creator {
 public:
  virtual ~Comp_creator() = default;
  virtual Item_bool_func *create(Item *a, Item *b) const = 0;

  /// This interface is only used by Item_allany_subselect.
  virtual const char *symbol(bool invert) const = 0;
  virtual bool eqne_op() const = 0;
  virtual bool l_op() const = 0;
};

/// Abstract base class for the comparison operators =, <> and <=>.
class Linear_comp_creator : public Comp_creator {
 public:
  Item_bool_func *create(Item *a, Item *b) const override;
  bool eqne_op() const override { return true; }
  bool l_op() const override { return false; }

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
  const char *symbol(bool invert) const override { return invert ? "<>" : "="; }

 protected:
  Item_bool_func *create_scalar_predicate(Item *a, Item *b) const override;
  Item_bool_func *combine(List<Item> list) const override;
};

class Equal_creator : public Linear_comp_creator {
 public:
  const char *symbol(bool invert [[maybe_unused]]) const override {
    // This will never be called with true.
    assert(!invert);
    return "<=>";
  }

 protected:
  Item_bool_func *create_scalar_predicate(Item *a, Item *b) const override;
  Item_bool_func *combine(List<Item> list) const override;
};

class Ne_creator : public Linear_comp_creator {
 public:
  const char *symbol(bool invert) const override { return invert ? "=" : "<>"; }

 protected:
  Item_bool_func *create_scalar_predicate(Item *a, Item *b) const override;
  Item_bool_func *combine(List<Item> list) const override;
};

class Gt_creator : public Comp_creator {
 public:
  Item_bool_func *create(Item *a, Item *b) const override;
  const char *symbol(bool invert) const override { return invert ? "<=" : ">"; }
  bool eqne_op() const override { return false; }
  bool l_op() const override { return false; }
};

class Lt_creator : public Comp_creator {
 public:
  Item_bool_func *create(Item *a, Item *b) const override;
  const char *symbol(bool invert) const override { return invert ? ">=" : "<"; }
  bool eqne_op() const override { return false; }
  bool l_op() const override { return true; }
};

class Ge_creator : public Comp_creator {
 public:
  Item_bool_func *create(Item *a, Item *b) const override;
  const char *symbol(bool invert) const override { return invert ? "<" : ">="; }
  bool eqne_op() const override { return false; }
  bool l_op() const override { return false; }
};

class Le_creator : public Comp_creator {
 public:
  Item_bool_func *create(Item *a, Item *b) const override;
  const char *symbol(bool invert) const override { return invert ? ">" : "<="; }
  bool eqne_op() const override { return false; }
  bool l_op() const override { return true; }
};

/// Base class for functions that usually take two arguments, which are possibly
/// strings, and perform some kind of comparison on the two arguments and return
/// a boolean. The functions may take more than two arguments (for example, LIKE
/// takes an optional third argument in the ESCAPE clause), but all of the
/// functions perform a comparison between the first two arguments, and extra
/// arguments are modifiers that affect how the comparison is performed.
class Item_bool_func2 : public Item_bool_func {
 private:
  bool convert_constant_arg(THD *thd, Item *field, Item **item,
                            bool *converted);

 protected:
  Arg_comparator cmp;
  bool abort_on_null{false};

  Item_bool_func2(Item *a, Item *b)
      : Item_bool_func(a, b), cmp(args, args + 1) {}

  Item_bool_func2(Item *a, Item *b, Item *c)
      : Item_bool_func(a, b, c), cmp(args, args + 1) {}

  Item_bool_func2(const POS &pos, Item *a, Item *b)
      : Item_bool_func(pos, a, b), cmp(args, args + 1) {}

  Item_bool_func2(const POS &pos, Item *a, Item *b, Item *c)
      : Item_bool_func(pos, a, b, c), cmp(args, args + 1) {}

 public:
  bool resolve_type(THD *) override;
  /// Sets up a comparator of the correct type based on the type of the
  /// function's arguments. Also sets up caches to hold constant values
  /// converted to the type expected by the comparator. See
  /// Arg_comparator::set_cmp_func().
  virtual bool set_cmp_func() {
    return cmp.set_cmp_func(this, args, args + 1, is_nullable());
  }
  optimize_type select_optimize(const THD *) override { return OPTIMIZE_OP; }
  /// @returns an operator REV_OP so that "B REV_OP A" is equivalent to
  /// "A this_operator B".
  virtual enum Functype rev_functype() const { return UNKNOWN_FUNC; }
  bool have_rev_func() const override { return rev_functype() != UNKNOWN_FUNC; }

  void print(const THD *thd, String *str,
             enum_query_type query_type) const override {
    Item_func::print_op(thd, str, query_type);
  }

  bool is_null() override { return args[0]->is_null() || args[1]->is_null(); }
  const CHARSET_INFO *compare_collation() const override {
    return cmp.cmp_collation.collation;
  }
  Item_result compare_type() const { return cmp.get_compare_type(); }
  void apply_is_true() override { abort_on_null = true; }
  /// Treat UNKNOWN result like FALSE because callers see no difference
  bool ignore_unknown() const { return abort_on_null; }
  void cleanup() override {
    Item_bool_func::cleanup();
    cmp.cleanup();
  }
  const Arg_comparator *get_comparator() const { return &cmp; }
  Item *replace_scalar_subquery(uchar *) override;
  friend class Arg_comparator;
};

/**
  Item_func_comparison is a class for comparison functions that take two
  arguments and return a boolean result.
  It is a common class for the regular comparison operators (=, <>, <, <=,
  >, >=) as well as the special <=> equality operator.
*/
class Item_func_comparison : public Item_bool_func2 {
 public:
  Item_func_comparison(Item *a, Item *b) : Item_bool_func2(a, b) {
    allowed_arg_cols = 0;  // Fetch this value from first argument
  }
  Item_func_comparison(const POS &pos, Item *a, Item *b)
      : Item_bool_func2(pos, a, b) {
    allowed_arg_cols = 0;  // Fetch this value from first argument
  }

  Item *truth_transformer(THD *, Bool_test) override;
  virtual Item *negated_item();
  bool subst_argument_checker(uchar **) override { return true; }
  bool is_null() override;

  bool cast_incompatible_args(uchar *) override;
};

/**
  XOR inherits from Item_bool_func2 because it is not optimized yet.
  Later, when XOR is optimized, it needs to inherit from
  Item_cond instead. See WL#5800.
*/
class Item_func_xor final : public Item_bool_func2 {
  typedef Item_bool_func2 super;

 public:
  Item_func_xor(Item *i1, Item *i2) : Item_bool_func2(i1, i2) {}
  Item_func_xor(const POS &pos, Item *i1, Item *i2)
      : Item_bool_func2(pos, i1, i2) {}

  enum Functype functype() const override { return XOR_FUNC; }
  const char *func_name() const override { return "xor"; }
  bool itemize(Parse_context *pc, Item **res) override;
  longlong val_int() override;
  void apply_is_true() override {}
  Item *truth_transformer(THD *, Bool_test) override;

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
  Item *truth_transformer(THD *, Bool_test) override;
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;
};

/**
  Wrapper class when MATCH function is used in WHERE clause.
  The MATCH clause can be used as a function returning a floating point value
  in the SELECT list or in the WHERE clause. However, it may also be used as
  a boolean function in the WHERE clause, where it has different semantics than
  when used together with a comparison operator. With a comparison operator,
  the match operation is performed with ranking. To preserve this behavior,
  the Item_func_match object is wrapped inside an object of class
  Item_func_match_predicate, which effectively transforms the function into
  a predicate. The overridden functions implemented in this class generally
  forward all evaluation to the underlying object.
*/
class Item_func_match_predicate final : public Item_bool_func {
 public:
  explicit Item_func_match_predicate(Item *a) : Item_bool_func(a) {}

  longlong val_int() override;
  enum Functype functype() const override { return MATCH_FUNC; }
  const char *func_name() const override { return "match"; }
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override {
    args[0]->print(thd, str, query_type);
  }

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override {
    return args[0]->get_filtering_effect(thd, filter_for_table, read_tables,
                                         fields_to_ignore, rows_in_table);
  }
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
  Item_func_trig_cond(Item *a, bool *f, const JOIN *join, plan_idx idx,
                      enum_trig_type trig_type_arg)
      : Item_bool_func(a),
        trig_var(f),
        m_join(join),
        m_idx(idx),
        trig_type(trig_type_arg) {}
  longlong val_int() override;
  enum Functype functype() const override { return TRIG_COND_FUNC; }
  /// '@<if@>', to distinguish from the if() SQL function
  const char *func_name() const override { return "<if>"; }
  /// Get range of inner tables spanned by associated outer join operation
  void get_table_range(Table_ref **first_table, Table_ref **last_table) const;
  /// Get table_map of inner tables spanned by associated outer join operation
  table_map get_inner_tables() const;
  bool fix_fields(THD *thd, Item **ref) override {
    if (Item_bool_func::fix_fields(thd, ref)) return true;
    add_trig_func_tables();
    return false;
  }
  void add_trig_func_tables() {
    if (trig_type == IS_NOT_NULL_COMPL || trig_type == FOUND_MATCH) {
      assert(m_join != nullptr);
      // Make this function dependent on the inner tables
      used_tables_cache |= get_inner_tables();
    } else if (trig_type == OUTER_FIELD_IS_NOT_NULL) {
      used_tables_cache |= OUTER_REF_TABLE_BIT;
    }
  }
  void update_used_tables() override {
    Item_bool_func::update_used_tables();
    add_trig_func_tables();
  }
  void fix_after_pullout(Query_block *parent_query_block,
                         Query_block *removed_query_block) override {
    Item_bool_func::fix_after_pullout(parent_query_block, removed_query_block);
    add_trig_func_tables();
  }
  const JOIN *get_join() const { return m_join; }
  enum enum_trig_type get_trig_type() const { return trig_type; }
  bool *get_trig_var() { return trig_var; }
  enum_trig_type get_trig_type() { return trig_type; }
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
  plan_idx idx() const { return m_idx; }

  bool contains_only_equi_join_condition() const override;
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
        test_sum_item(nullptr),
        test_sub_item(nullptr),
        subselect(nullptr),
        abort_on_null(false),
        show(false) {}
  void apply_is_true() override { abort_on_null = true; }
  /// Treat UNKNOWN result like FALSE because callers see no difference
  bool ignore_unknown() const { return abort_on_null; }
  longlong val_int() override;
  enum Functype functype() const override { return NOT_ALL_FUNC; }
  const char *func_name() const override { return "<not>"; }
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
  void set_sum_test(Item_sum_hybrid *item) { test_sum_item = item; }
  void set_sub_test(Item_maxmin_subselect *item) { test_sub_item = item; }
  void set_subselect(Item_subselect *item) { subselect = item; }
  table_map not_null_tables() const override {
    /*
      See handling of not_null_tables_cache in
      Item_in_optimizer::fix_fields().

      This item is the result of a transformation from an ALL clause
      such as
          left-expr < ALL(subquery)
      into
          <not>(left-expr >= ANY(subquery)

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
  Item *truth_transformer(THD *, Bool_test) override;
};

class Item_func_nop_all final : public Item_func_not_all {
 public:
  Item_func_nop_all(Item *a) : Item_func_not_all(a) {}
  longlong val_int() override;
  const char *func_name() const override { return "<nop>"; }
  table_map not_null_tables() const override { return not_null_tables_cache; }
  Item *truth_transformer(THD *, Bool_test) override;
};

/**
  Base class for the equality comparison operators = and <=>.

  Both of these operators can be used to construct a key for a hash join, as
  both represent an equality, only differing in how NULL values are handled. The
  common code for constructing hash join keys is located in this class.
*/
class Item_eq_base : public Item_func_comparison {
 protected:
  Item_eq_base(Item *a, Item *b) : Item_func_comparison(a, b) {}
  Item_eq_base(const POS &pos, Item *a, Item *b)
      : Item_func_comparison(pos, a, b) {}

 public:
  bool contains_only_equi_join_condition() const final;

  /// Read the value from the join condition, and append it to the output vector
  /// "join_key_buffer". The function will determine which side of the condition
  /// to read the value from by using the bitmap "tables".
  ///
  /// @param thd the thread handler
  /// @param tables a bitmap that marks the tables that are involved in the join
  /// @param join_condition an instance containing the join condition together
  ///   with some pre-calculated values
  /// @param[out] join_key_buffer a buffer where the value from the join
  ///   condition will be appended
  /// @param is_multi_column_key true if the hash join key has multiple columns
  ///   (that is, the hash join condition is a conjunction)
  ///
  /// @returns true if this is an ordinary equality (=) predicate and the value
  /// evaluated to NULL, or false otherwise.
  bool append_join_key_for_hash_join(THD *thd, table_map tables,
                                     const HashJoinCondition &join_condition,
                                     bool is_multi_column_key,
                                     String *join_key_buffer) const;

  /// Wrap the argument in a typecast, if needed.
  ///
  /// When computing a hash of the join value during a hash join, we want to
  /// create a hash value that is memcmp-able. This is quite straightforward
  /// for most data types, but it can be tricky for some types. For the
  /// straightforward cases, this function just returns the argument it was
  /// given in. For the complex cases, the function returns the given argument,
  /// wrapped in a typecast node. Which typecast node it is wrapped in is
  /// determined by the comparison context of this equality condition. The
  /// comparison context is given by the member "cmp"; a comparator class that
  /// is set up during query resolving.
  ///
  /// @param mem_root the MEM_ROOT where the typecast node is allocated
  /// @param argument the argument that we might wrap in a typecast. This is
  ///   either the left or the right side of the Item_eq_base
  ///
  /// @returns either the argument it was given, or the argument wrapped in a
  ///   typecast
  Item *create_cast_if_needed(MEM_ROOT *mem_root, Item *argument) const;

  /// If this equality originally came from a multi-equality, this documents
  /// which one it came from (otherwise nullptr). It is used during planning:
  /// For selectivity estimates and for not pushing down the same multi-equality
  /// to the same join more than once (see IsBadJoinForCondition()).
  ///
  /// This is used only in the hypergraph optimizer; the pre-hypergraph
  /// optimizer uses COND_EQUAL to find this instead.
  ///
  /// It is always nullptr in Item_func_equal objects, as such objects are never
  /// created from multiple equalities.
  Item_equal *source_multiple_equality = nullptr;
};

/**
  Implements the comparison operator equals (=)
*/
class Item_func_eq final : public Item_eq_base {
 public:
  Item_func_eq(Item *a, Item *b) : Item_eq_base(a, b) {}
  Item_func_eq(const POS &pos, Item *a, Item *b) : Item_eq_base(pos, a, b) {}
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

  /// See if this is a condition where any of the arguments refers to a field
  /// that is outside the bits marked by 'left_side_tables' and
  /// 'right_side_tables'.
  ///
  /// This is a situation that can happen during equality propagation in the
  /// optimization phase. Consider the following query:
  ///
  ///   SELECT * FROM t1 LEFT JOIN
  ///     (t2 LEFT JOIN t3 ON t3.i = t2.i) ON t2.i = t1.i;
  ///
  /// The optimizer will see that t1.i = t2.i = t3.i. Furthermore, it will
  /// replace one side of this condition with a field from a table that is as
  /// early in the join order as possible. However, this will break queries
  /// executed in the iterator executor. The above query will end up with
  /// something like this after optimization:
  ///
  ///       Left hash join <--- t1.i = t2.i
  ///       |            |
  ///      t1     Left hash join  <--- t1.i = t3.i
  ///             |            |
  ///             t2           t3
  ///
  /// Note that 't2.i = t3.i' has been rewritten to 't1.i = t3.i'. When
  /// evaluating the join between t2 and t3, t1 is outside our reach!
  /// To overcome this, we must reverse the changes done by the equality
  /// propagation. It is possible to do so because during equality propagation,
  /// we save a list of all of the fields that were considered equal.
  /// If we are asked to replace ("replace" set to true), arguments of this
  /// function are replaced with an equal field. If we are not replacing, we
  /// set "found" to "true" if an equal field is found, "false" otherwise.
  void ensure_multi_equality_fields_are_available(table_map left_side_tables,
                                                  table_map right_side_tables,
                                                  bool replace, bool *found);
};

/**
  The <=> operator evaluates the same as

    a IS NULL || b IS NULL ? a IS NULL == b IS NULL : a = b

  a <=> b is equivalent to the standard operation a IS NOT DISTINCT FROM b.

  Notice that the result is TRUE or FALSE, and never UNKNOWN.
*/
class Item_func_equal final : public Item_eq_base {
 public:
  Item_func_equal(Item *a, Item *b) : Item_eq_base(a, b) {
    null_on_null = false;
  }
  Item_func_equal(const POS &pos, Item *a, Item *b) : Item_eq_base(pos, a, b) {
    null_on_null = false;
  }
  // Needs null value propagated to parent, even though operator is not nullable
  bool set_cmp_func() override {
    return cmp.set_cmp_func(this, args, args + 1, true);
  }
  longlong val_int() override;
  bool resolve_type(THD *thd) override;
  enum Functype functype() const override { return EQUAL_FUNC; }
  enum Functype rev_functype() const override { return EQUAL_FUNC; }
  cond_result eq_cmp_result() const override { return COND_TRUE; }
  const char *func_name() const override { return "<=>"; }
  Item *truth_transformer(THD *, Bool_test) override { return nullptr; }
  bool is_null() override { return false; }

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;
};

/**
  Implements the comparison operator greater than or equals (>=)
*/
class Item_func_ge final : public Item_func_comparison {
 public:
  Item_func_ge(Item *a, Item *b) : Item_func_comparison(a, b) {}
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

/**
  Implements the comparison operator greater than (>)
*/
class Item_func_gt final : public Item_func_comparison {
 public:
  Item_func_gt(Item *a, Item *b) : Item_func_comparison(a, b) {}
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

/**
  Implements the comparison operator less than or equals (<=)
*/
class Item_func_le final : public Item_func_comparison {
 public:
  Item_func_le(Item *a, Item *b) : Item_func_comparison(a, b) {}
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

/**
  Internal function used by subquery to derived transformation to check
  if a subquery is scalar. We model it to check if the count is greater than
  1 using Item_func_gt.
*/

class Item_func_reject_if : public Item_bool_func {
 public:
  Item_func_reject_if(Item *a) : Item_bool_func(a) {}
  longlong val_int() override;
  const char *func_name() const override { return "reject_if"; }
  /// Redefine to avoid pushing into derived table
  bool is_valid_for_pushdown(uchar *arg [[maybe_unused]]) override {
    return true;
  }
  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;
  /**
    We add RAND_TABLE_BIT to prevent moving this item from the JOIN condition:
    it might raise an error too early: only if the join condition succeeds is
    it relevant and should be evaluated. Cf.
    Query_block::decorrelate_derived_scalar_subquery_post

    @return Always RAND_TABLE_BIT
  */
  table_map get_initial_pseudo_tables() const override {
    return RAND_TABLE_BIT;
  }
};

/**
  Implements the comparison operator less than (<)
*/
class Item_func_lt final : public Item_func_comparison {
 public:
  Item_func_lt(Item *a, Item *b) : Item_func_comparison(a, b) {}
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

/**
  Implements the comparison operator not equals (<>)
*/
class Item_func_ne final : public Item_func_comparison {
 public:
  Item_func_ne(Item *a, Item *b) : Item_func_comparison(a, b) {}
  longlong val_int() override;
  enum Functype functype() const override { return NE_FUNC; }
  enum Functype rev_functype() const override { return NE_FUNC; }
  cond_result eq_cmp_result() const override { return COND_FALSE; }
  optimize_type select_optimize(const THD *) override { return OPTIMIZE_KEY; }
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
  of these classes can express predicates or their negations.
  The alternative approach would be to create pairs Item_func_between,
  Item_func_notbetween and Item_func_in, Item_func_notin.

*/

class Item_func_opt_neg : public Item_int_func {
 public:
  bool negated;    /* <=> the item represents NOT <func> */
  bool pred_level; /* <=> [NOT] <func> is used on a predicate level */
 public:
  Item_func_opt_neg(const POS &pos, Item *a, Item *b, Item *c, bool is_negation)
      : Item_int_func(pos, a, b, c), negated(false), pred_level(false) {
    if (is_negation) negate();
  }
  Item_func_opt_neg(const POS &pos, PT_item_list *list, bool is_negation)
      : Item_int_func(pos, list), negated(false), pred_level(false) {
    if (is_negation) negate();
  }

 public:
  inline void negate() { negated = !negated; }
  inline void apply_is_true() override { pred_level = true; }
  bool ignore_unknown() const { return pred_level; }
  Item *truth_transformer(THD *, Bool_test test) override {
    if (test != BOOL_NEGATED) return nullptr;
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
  optimize_type select_optimize(const THD *) override { return OPTIMIZE_KEY; }
  enum Functype functype() const override { return BETWEEN; }
  const char *func_name() const override { return "between"; }
  bool fix_fields(THD *, Item **) override;
  void fix_after_pullout(Query_block *parent_query_block,
                         Query_block *removed_query_block) override;
  bool resolve_type(THD *) override;
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
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
  void update_used_tables() override;

  void update_not_null_tables() {
    // not_null_tables_cache == union(T1(e),T1(e1),T1(e2))
    if (pred_level && !negated) return;

    /// not_null_tables_cache == union(T1(e), intersection(T1(e1),T1(e2)))
    not_null_tables_cache =
        args[0]->not_null_tables() |
        (args[1]->not_null_tables() & args[2]->not_null_tables());
  }
};

class Item_func_strcmp final : public Item_bool_func2 {
 public:
  Item_func_strcmp(const POS &pos, Item *a, Item *b)
      : Item_bool_func2(pos, a, b) {}
  longlong val_int() override;
  optimize_type select_optimize(const THD *) override { return OPTIMIZE_NONE; }
  const char *func_name() const override { return "strcmp"; }
  enum Functype functype() const override { return STRCMP_FUNC; }

  void print(const THD *thd, String *str,
             enum_query_type query_type) const override {
    Item_func::print(thd, str, query_type);
  }
  // We derive (indirectly) from Item_bool_func, but this is not a true boolean.
  // Override length and unsigned_flag set by set_data_type_bool().
  bool resolve_type(THD *thd) override {
    if (Item_bool_func2::resolve_type(thd)) return true;
    fix_char_length(2);  // returns "1" or "0" or "-1"
    unsigned_flag = false;
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
                     Item *expr2, class PT_item_list *opt_expr_list = nullptr)
      : super(pos, alloc_row(pos, mem_root, expr1, expr2, opt_expr_list)),
        row(down_cast<Item_row *>(args[0])),
        intervals(nullptr) {
    allowed_arg_cols = 0;  // Fetch this value from first argument
  }

  bool itemize(Parse_context *pc, Item **res) override;
  longlong val_int() override;
  bool resolve_type(THD *) override;
  const char *func_name() const override { return "interval"; }
  uint decimal_precision() const override { return 2; }
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
  void update_used_tables() override;

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
  Item_func_coalesce(Item *a) : Item_func_numhybrid(a) { null_on_null = false; }

 public:
  Item_func_coalesce(const POS &pos, PT_item_list *list)
      : Item_func_numhybrid(pos, list) {
    null_on_null = false;
  }
  enum_field_types default_data_type() const override {
    return MYSQL_TYPE_VARCHAR;
  }
  Item_func_coalesce(Item *a, Item *b) : Item_func_numhybrid(a, b) {
    null_on_null = false;
  }
  TYPELIB *get_typelib() const override;
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
  bool resolve_type(THD *thd) override;
  bool resolve_type_inner(THD *thd) override;
  void set_numeric_type() override {}
  enum Item_result result_type() const override { return hybrid_type; }
  const char *func_name() const override { return "coalesce"; }
  enum Functype functype() const override { return COALESCE_FUNC; }
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
  const char *func_name() const override { return "ifnull"; }
  Field *tmp_table_field(TABLE *table) override;
};

/**
   ANY_VALUE(expr) is like expr except that it is not checked by
   aggregate_check logic. It serves as a solution for users who want to
   bypass this logic.
*/
class Item_func_any_value final : public Item_func_coalesce {
 public:
  Item_func_any_value(const POS &pos, Item *a) : Item_func_coalesce(pos, a) {}
  Item_func_any_value(Item *a) : Item_func_coalesce(a) {}
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
  enum_field_types default_data_type() const override {
    return MYSQL_TYPE_VARCHAR;
  }
  bool resolve_type(THD *thd) override;
  bool resolve_type_inner(THD *thd) override;
  void fix_after_pullout(Query_block *parent_query_block,
                         Query_block *removed_query_block) override;
  TYPELIB *get_typelib() const override;
  const char *func_name() const override { return "if"; }
  enum Functype functype() const override { return IF_FUNC; }
  void update_used_tables() override;

  ///< T1(IF(e,e1,e2)) = intersection(T1(e1),T1(e2))
  void update_not_null_tables() {
    not_null_tables_cache =
        (args[1]->not_null_tables() & args[2]->not_null_tables());
  }
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
  enum_field_types default_data_type() const override {
    return MYSQL_TYPE_VARCHAR;
  }
  bool resolve_type(THD *thd) override;
  bool resolve_type_inner(THD *thd) override;
  const char *func_name() const override { return "nullif"; }
  enum Functype functype() const override { return NULLIF_FUNC; }

  // No, we should NOT inherit from Item_bool_func2
  uint decimal_precision() const override { return Item::decimal_precision(); }

  void print(const THD *thd, String *str,
             enum_query_type query_type) const override {
    Item_func::print(thd, str, query_type);
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
 private:
  const uint m_size;  ///< Size of the vector
 public:
  uint m_used_size{0};  ///< The actual size of the vector (NULL may be ignored)

  /**
    See Item_func_in::resolve_type() for why we need both
    count and used_count.
   */
  explicit in_vector(uint elements) : m_size(elements) {}

  virtual ~in_vector() = default;

  /**
    Calls item->val_int() or item->val_str() etc.
    and then does binary_search if the value is non-null.
    @param  item to evaluate, and lookup in the IN-list.
    @return true if evaluated value of the item was found.
   */
  virtual bool find_item(Item *item) = 0;

  /**
    Create an instance of Item_{type} (e.g. Item_decimal) constant object
    which type allows it to hold an element of this vector without any
    conversions.
    The purpose of this function is to be able to get elements of this
    vector in form of Item_xxx constants without creating Item_xxx object
    for every array element you get (i.e. this implements "FlyWeight" pattern)

    @param mem_root  Where to allocate the Item.
  */
  virtual Item_basic_constant *create_item(MEM_ROOT *mem_root) const = 0;

  /**
    Store the value at position #pos into provided item object

    @param pos   Index of value to store
    @param item  Constant item to store value into. The item must be of the same
                 type that create_item() returns.
  */
  virtual void value_to_item(uint pos, Item_basic_constant *item) const = 0;

  /** Compare values number pos1 and pos2 for equality */
  virtual bool compare_elems(uint pos1, uint pos2) const = 0;

  virtual bool is_row_result() const { return false; }

  /**
    Fill the vector by evaluating the items passed as arguments.
    Note that null values are skipped so the vector may end up containing
    fewer elements than the number of items.
    The vector is sorted so that it can be used for binary search.

    @param items       Items to evaluate
    @param item_count  Number of items

    @return true if any null values was found, false otherwise.
  */
  bool fill(Item **items, uint item_count);

 private:
  virtual void set(uint pos, Item *item) = 0;

  /// Sort the IN-list array, so we can do efficient lookup with binary_search.
  virtual void sort_array() = 0;
};

class in_string final : public in_vector {
  char buff[STRING_BUFFER_USUAL_SIZE];
  String tmp;
  Mem_root_array<String> base_objects;
  // String objects are not sortable, sort pointers instead.
  Mem_root_array<String *> base_pointers;
  const CHARSET_INFO *collation;

 public:
  in_string(MEM_ROOT *mem_root, uint elements, const CHARSET_INFO *cs);
  Item_basic_constant *create_item(MEM_ROOT *mem_root) const override {
    return new (mem_root) Item_string(collation);
  }
  void value_to_item(uint pos, Item_basic_constant *item) const override {
    item->set_str_value(base_pointers[pos]);
  }
  bool find_item(Item *item) override;
  bool compare_elems(uint pos1, uint pos2) const override;

 private:
  void set(uint pos, Item *item) override;
  void sort_array() override;
};

class in_longlong : public in_vector {
 public:
  struct packed_longlong {
    longlong val;
    bool unsigned_flag;
  };

 protected:
  Mem_root_array<packed_longlong> base;

 public:
  in_longlong(MEM_ROOT *mem_root, uint elements)
      : in_vector(elements), base(mem_root, elements) {}
  Item_basic_constant *create_item(MEM_ROOT *mem_root) const override {
    /*
      We've created a signed INT, this may not be correct in the
      general case (see BUG#19342).
    */
    return new (mem_root) Item_int(0LL);
  }
  void value_to_item(uint pos, Item_basic_constant *item) const override {
    down_cast<Item_int *>(item)->value = base[pos].val;
    item->unsigned_flag = base[pos].unsigned_flag;
  }
  bool find_item(Item *item) override;
  bool compare_elems(uint pos1, uint pos2) const override;

 private:
  void set(uint pos, Item *item) override { val_item(item, &base[pos]); }
  void sort_array() override;
  virtual void val_item(Item *item, packed_longlong *result);
};

class in_datetime_as_longlong final : public in_longlong {
 public:
  in_datetime_as_longlong(MEM_ROOT *mem_root, uint elements)
      : in_longlong(mem_root, elements) {}
  Item_basic_constant *create_item(MEM_ROOT *mem_root) const override {
    return new (mem_root) Item_temporal(MYSQL_TYPE_DATETIME, 0LL);
  }

 private:
  void val_item(Item *item, packed_longlong *result) override;
};

class in_time_as_longlong final : public in_longlong {
 public:
  in_time_as_longlong(MEM_ROOT *mem_root, uint elements)
      : in_longlong(mem_root, elements) {}
  Item_basic_constant *create_item(MEM_ROOT *mem_root) const override {
    return new (mem_root) Item_temporal(MYSQL_TYPE_TIME, 0LL);
  }

 private:
  void val_item(Item *item, packed_longlong *result) override;
};

/*
  Class to represent a vector of constant DATE/DATETIME values.
  Values are obtained with help of the get_datetime_value() function.
*/
class in_datetime final : public in_longlong {
  /// An item used to issue warnings.
  Item *warn_item;

 public:
  in_datetime(MEM_ROOT *mem_root, Item *warn_item_arg, uint elements)
      : in_longlong(mem_root, elements), warn_item(warn_item_arg) {}
  Item_basic_constant *create_item(MEM_ROOT *mem_root) const override {
    return new (mem_root) Item_temporal(MYSQL_TYPE_DATETIME, 0LL);
  }

 private:
  void set(uint pos, Item *item) override;
  void val_item(Item *item, packed_longlong *result) override;
};

class in_double final : public in_vector {
  Mem_root_array<double> base;

 public:
  in_double(MEM_ROOT *mem_root, uint elements)
      : in_vector(elements), base(mem_root, elements) {}
  Item_basic_constant *create_item(MEM_ROOT *mem_root) const override {
    return new (mem_root) Item_float(0.0, 0);
  }
  void value_to_item(uint pos, Item_basic_constant *item) const override {
    down_cast<Item_float *>(item)->value = base[pos];
  }
  bool find_item(Item *item) override;
  bool compare_elems(uint pos1, uint pos2) const override;

 private:
  void set(uint pos, Item *item) override;
  void sort_array() override;
};

class in_decimal final : public in_vector {
  Mem_root_array<my_decimal> base;

 public:
  in_decimal(MEM_ROOT *mem_root, uint elements)
      : in_vector(elements), base(mem_root, elements) {}
  Item_basic_constant *create_item(MEM_ROOT *mem_root) const override {
    return new (mem_root) Item_decimal(0, false);
  }
  void value_to_item(uint pos, Item_basic_constant *item) const override {
    down_cast<Item_decimal *>(item)->set_decimal_value(&base[pos]);
  }
  bool find_item(Item *item) override;
  bool compare_elems(uint pos1, uint pos2) const override;

 private:
  void set(uint pos, Item *item) override;
  void sort_array() override;
};

/*
** Classes for easy comparing of non const items
*/

class cmp_item {
 public:
  cmp_item() = default;
  virtual ~cmp_item() = default;
  /**
    Allocate comparator objects for each value object, based on the template
    comparator objects. Only implemented for derived class cmp_item_row.

    @param mem_root mem_root for allocation.
    @param tmpl     The template item object.
    @param arg      The value item.

    @returns false if success, true if error.
  */
  virtual bool allocate_value_comparators(MEM_ROOT *mem_root, cmp_item *tmpl,
                                          Item *arg);
  virtual void store_value(Item *item) = 0;
  /**
     @returns result (true, false or UNKNOWN) of
     "stored argument's value <> item's value"
  */
  virtual int cmp(Item *item) = 0;
  // for optimized IN with row
  virtual int compare(const cmp_item *item) const = 0;

  /**
    Create an appropriate comparator for the given type.

    @param thd          Session handle.
    @param result_type  Used to find the appropriate comparator.
    @param item         Item object used to distinguish temporal types.
    @param cs           Charset

    @returns new cmp_item_xxx object, or nullptr if error.
  */
  static cmp_item *new_comparator(THD *thd, Item_result result_type, Item *item,
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
  const String *value_res;
  StringBuffer<STRING_BUFFER_USUAL_SIZE> value;
  const CHARSET_INFO *cmp_charset;

 public:
  cmp_item_string(const CHARSET_INFO *cs) : value(cs), cmp_charset(cs) {}

  int compare(const cmp_item *ci) const override {
    const cmp_item_string *l_cmp = down_cast<const cmp_item_string *>(ci);
    return sortcmp(value_res, l_cmp->value_res, cmp_charset);
  }

  void store_value(Item *item) override {
    String *res = eval_string_arg(cmp_charset, item, &value);
    if (res && (res != &value || !res->is_alloced())) {
      // 'res' may point in item's transient internal data, so make a copy
      value.copy(*res);
    }
    value_res = &value;
    set_null_value(item->null_value);
  }

  int cmp(Item *arg) override;
  cmp_item *make_same() override;
};

class cmp_item_json final : public cmp_item_scalar {
 private:
  /// Cached JSON value to look up
  unique_ptr_destroy_only<Json_wrapper> m_value;
  /// Cache for the value above
  unique_ptr_destroy_only<Json_scalar_holder> m_holder;
  /// String buffer
  String m_str_value;

 public:
  /**
    Construct a cmp_item_json object.
    @param wrapper a Json_wrapper for holding the JSON value in the comparison
    @param holder  pre-alloced memory for creating JSON scalar values without
                   using the heap
  */
  cmp_item_json(unique_ptr_destroy_only<Json_wrapper> wrapper,
                unique_ptr_destroy_only<Json_scalar_holder> holder);
  ~cmp_item_json() override;

  int compare(const cmp_item *ci) const override;
  void store_value(Item *item) override;
  int cmp(Item *arg) override;
  cmp_item *make_same() override;
};

class cmp_item_int final : public cmp_item_scalar {
  longlong value;

 public:
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
*/
class cmp_item_datetime : public cmp_item_scalar {
  longlong value;

 public:
  /* Item used for issuing warnings. */
  const Item *warn_item;
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
  void store_value(Item *item) override;
  int cmp(Item *arg) override;
  int compare(const cmp_item *c) const override;
  cmp_item *make_same() override;
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
  Item_func_case(const POS &pos, mem_root_deque<Item *> *list,
                 Item *first_expr_arg, Item *else_expr_arg)
      : super(pos),
        first_expr_num(-1),
        else_expr_num(-1),
        cached_result_type(INT_RESULT),
        left_result_type(INT_RESULT),
        case_item(nullptr) {
    null_on_null = false;
    ncases = list->size();
    if (first_expr_arg) {
      first_expr_num = list->size();
      list->push_back(first_expr_arg);
    }
    if (else_expr_arg) {
      else_expr_num = list->size();
      list->push_back(else_expr_arg);
    }
    set_arguments(list, true);
    memset(&cmp_items, 0, sizeof(cmp_items));
  }
  ~Item_func_case() override;
  int get_first_expr_num() const { return first_expr_num; }
  int get_else_expr_num() const { return else_expr_num; }
  double val_real() override;
  longlong val_int() override;
  String *val_str(String *) override;
  my_decimal *val_decimal(my_decimal *) override;
  bool val_json(Json_wrapper *wr) override;
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) override;
  bool get_time(MYSQL_TIME *ltime) override;
  bool fix_fields(THD *thd, Item **ref) override;
  enum_field_types default_data_type() const override {
    return MYSQL_TYPE_VARCHAR;
  }
  bool resolve_type(THD *thd) override;
  bool resolve_type_inner(THD *thd) override;
  TYPELIB *get_typelib() const override;
  enum Item_result result_type() const override { return cached_result_type; }
  const char *func_name() const override { return "case"; }
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
  Item *find_item(String *str);
  const CHARSET_INFO *compare_collation() const override {
    return cmp_collation.collation;
  }
  enum Functype functype() const override { return CASE_FUNC; }
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
  /// An array of const values, created when the bisection lookup method is used
  in_vector *m_const_array{nullptr};
  /**
    If there is some NULL among @<in value list@>, during a val_int() call; for
    example
    IN ( (1,(3,'col')), ... ), where 'col' is a column which evaluates to
    NULL.
  */
  bool have_null{false};
  /// Set to true when values in const array are populated
  bool m_populated{false};

 private:
  /// Set to true if all values in IN-list are const
  bool m_values_are_const{true};
  /// Set to true if const array must be repopulated per execution.
  bool m_need_populate{false};
  /**
    Set to true by resolve_type() if the IN list contains a
    dependent subquery, in which case condition filtering will not be
    calculated for this item.
  */
  bool dep_subq_in_list{false};
  /// True until start of 2nd call to resolve_type()
  bool first_resolve_call{true};

  Item_result left_result_type;
  cmp_item *cmp_items[6]; /* One cmp_item for each result type */
  DTCollation cmp_collation;

 public:
  Item_func_in(const POS &pos, PT_item_list *list, bool is_negation)
      : Item_func_opt_neg(pos, list, is_negation) {
    memset(&cmp_items, 0, sizeof(cmp_items));
    allowed_arg_cols = 0;  // Fetch this value from first argument
  }
  ~Item_func_in() override;
  longlong val_int() override;
  bool fix_fields(THD *, Item **) override;
  void fix_after_pullout(Query_block *parent_query_block,
                         Query_block *removed_query_block) override;
  bool resolve_type(THD *) override;
  void update_used_tables() override;
  uint decimal_precision() const override { return 1; }

  /**
    Populate values for bisection with fresh values, should be called once
    per execution.

    @param thd     Thread handler

    @returns false if success, true if error
  */
  bool populate_bisection(THD *thd);
  void cleanup() override;
  optimize_type select_optimize(const THD *) override { return OPTIMIZE_KEY; }
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
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

  void update_not_null_tables() {
    // not_null_tables_cache == union(T1(e),union(T1(ei)))
    if (pred_level && negated) return;

    not_null_tables_cache = ~(table_map)0;

    ///< not_null_tables_cache = union(T1(e),intersection(T1(ei)))
    Item **arg_end = args + arg_count;
    for (Item **arg = args + 1; arg != arg_end; arg++)
      not_null_tables_cache &= (*arg)->not_null_tables();
    not_null_tables_cache |= args[0]->not_null_tables();
  }

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
  void cleanup_arrays();  ///< Helper function for this common task
};

class cmp_item_row : public cmp_item {
  cmp_item **comparators{nullptr};
  uint n;

  // Only used for Mem_root_array::resize()
  cmp_item_row() : n(0) {}

  friend class Mem_root_array_YY<cmp_item_row>;

 public:
  cmp_item_row(THD *thd, Item *item) : n(item->cols()) {
    allocate_template_comparators(thd, item);
  }
  ~cmp_item_row() override;

  cmp_item_row(cmp_item_row &&other)
      : comparators(other.comparators), n(other.n) {
    other.comparators = nullptr;
    other.n = 0;
  }

  bool allocate_value_comparators(MEM_ROOT *mem_root, cmp_item *tmpl,
                                  Item *arg) override;
  void store_value(Item *item) override;
  int cmp(Item *arg) override;
  int compare(const cmp_item *arg) const override;
  cmp_item *make_same() override;
  void store_value_by_template(cmp_item *tmpl, Item *) override;

 private:
  /**
    Allocate comparator objects for the LHS argument to IN, used as template
    for the value comparators.

    @param thd    Session handle
    @param item   Item to allocate comparator objects for, left-hand IN operand

    @returns false if success, true if error.
  */
  bool allocate_template_comparators(THD *thd, Item *item);
};

class in_row final : public in_vector {
  unique_ptr_destroy_only<cmp_item_row> tmp;
  Mem_root_array<cmp_item_row> base_objects;
  // Sort pointers, rather than objects.
  Mem_root_array<cmp_item_row *> base_pointers;

 public:
  in_row(MEM_ROOT *mem_root, uint elements, cmp_item_row *cmp);
  bool is_row_result() const override { return true; }
  /**
    Allocate extra objects for evaluation

    @param mem_root  Memory root for allocation.
    @param lhs       The left-hand side object of the IN predicate.
    @param arg_count Number of arguments on the right-hand side of the predicate

    @returns false if success, true if error.
  */
  bool allocate(MEM_ROOT *mem_root, Item *lhs, uint arg_count);
  bool find_item(Item *item) override;
  bool compare_elems(uint pos1, uint pos2) const override;

  Item_basic_constant *create_item(MEM_ROOT *) const override {
    assert(false);
    return nullptr;
  }
  void value_to_item(uint, Item_basic_constant *) const override {
    assert(false);
  }

 private:
  void set(uint pos, Item *item) override;
  void sort_array() override;
};

/* Functions used by where clause */

class Item_func_isnull : public Item_bool_func {
  typedef Item_bool_func super;

  bool cache_used = false;
  bool cached_value;

 public:
  Item_func_isnull(Item *a) : super(a) { null_on_null = false; }
  Item_func_isnull(const POS &pos, Item *a) : super(pos, a) {
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
  optimize_type select_optimize(const THD *) override { return OPTIMIZE_NULL; }
  Item *truth_transformer(THD *, Bool_test test) override;
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
  const CHARSET_INFO *compare_collation() const override {
    return args[0]->collation.collation;
  }
  bool fix_fields(THD *thd, Item **ref) override;
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
  bool resolve_type(THD *thd) override;
  void update_used_tables() override;
  /**
    We add RAND_TABLE_BIT to prevent moving this item from HAVING to WHERE.

    @retval Always RAND_TABLE_BIT
  */
  table_map get_initial_pseudo_tables() const override {
    return RAND_TABLE_BIT;
  }
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override {
    Item_bool_func::print(thd, str, query_type);
  }
};

class Item_func_isnotnull final : public Item_bool_func {
 public:
  Item_func_isnotnull(Item *a) : Item_bool_func(a) { null_on_null = false; }
  Item_func_isnotnull(const POS &pos, Item *a) : Item_bool_func(pos, a) {
    null_on_null = false;
  }

  longlong val_int() override;
  enum Functype functype() const override { return ISNOTNULL_FUNC; }
  bool resolve_type(THD *thd) override {
    set_nullable(false);
    return Item_bool_func::resolve_type(thd);
  }
  const char *func_name() const override { return "isnotnull"; }
  optimize_type select_optimize(const THD *) override { return OPTIMIZE_NULL; }
  Item *truth_transformer(THD *, Bool_test test) override;
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
  const CHARSET_INFO *compare_collation() const override {
    return args[0]->collation.collation;
  }
  void apply_is_true() override {
    null_on_null = true;
  }  // Same logic as for Item_func_truth's function
  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;
};

class Item_func_like final : public Item_bool_func2 {
  /// True if escape clause is const (a literal)
  bool escape_is_const = false;
  /// Tells if the escape clause has been evaluated.
  bool escape_evaluated = false;
  bool eval_escape_clause(THD *thd);
  /// The escape character (0 if no escape character).
  int m_escape;

 public:
  Item_func_like(Item *a, Item *b) : Item_bool_func2(a, b) {}
  Item_func_like(Item *a, Item *b, Item *escape_arg)
      : Item_bool_func2(a, b, escape_arg) {
    assert(escape_arg != nullptr);
  }
  Item_func_like(const POS &pos, Item *a, Item *b, Item *escape_arg)
      : Item_bool_func2(pos, a, b, escape_arg) {
    assert(escape_arg != nullptr);
  }
  Item_func_like(const POS &pos, Item *a, Item *b)
      : Item_bool_func2(pos, a, b) {}

  longlong val_int() override;
  enum Functype functype() const override { return LIKE_FUNC; }
  optimize_type select_optimize(const THD *thd) override;
  /// Result may be not equal with equal inputs if ESCAPE character is present
  cond_result eq_cmp_result() const override { return COND_OK; }
  const char *func_name() const override { return "like"; }
  bool fix_fields(THD *thd, Item **ref) override;
  bool resolve_type(THD *) override;
  void cleanup() override;
  Item *replace_scalar_subquery(uchar *) override;
  // Overridden because Item_bool_func2::print() doesn't print the ESCAPE
  // clause.
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
  /**
    @retval true non default escape char specified
                 using "expr LIKE pat ESCAPE 'escape_char'" syntax
  */
  bool escape_was_used_in_parsing() const { return arg_count > 2; }

  /// Returns the escape character.
  int escape() const {
    assert(escape_is_evaluated());
    return m_escape;
  }

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

    @retval true if error happens during wild string prefix calculation,
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
  Item_cond() : Item_bool_func(), abort_on_null(true) {}
  Item_cond(Item *i1, Item *i2) : Item_bool_func(), abort_on_null(false) {
    list.push_back(i1);
    list.push_back(i2);
  }
  Item_cond(const POS &pos, Item *i1, Item *i2)
      : Item_bool_func(pos), abort_on_null(false) {
    list.push_back(i1);
    list.push_back(i2);
  }

  Item_cond(THD *thd, Item_cond *item);
  Item_cond(List<Item> &nlist)
      : Item_bool_func(), list(nlist), abort_on_null(false) {}
  bool add(Item *item) {
    assert(item);
    return list.push_back(item);
  }
  bool add_at_head(Item *item) {
    assert(item);
    return list.push_front(item);
  }
  void add_at_head(List<Item> *nlist) {
    assert(nlist->elements);
    list.prepend(nlist);
  }

  bool itemize(Parse_context *pc, Item **res) override;

  bool fix_fields(THD *, Item **ref) override;
  void fix_after_pullout(Query_block *parent_query_block,
                         Query_block *removed_query_block) override;

  Type type() const override { return COND_ITEM; }
  List<Item> *argument_list() { return &list; }
  bool eq(const Item *item, bool binary_cmp) const override;
  table_map used_tables() const override { return used_tables_cache; }
  void update_used_tables() override;
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
  void split_sum_func(THD *thd, Ref_item_array ref_item_array,
                      mem_root_deque<Item *> *fields) override;
  void apply_is_true() override { abort_on_null = true; }
  void copy_andor_arguments(THD *thd, Item_cond *item);
  bool walk(Item_processor processor, enum_walk walk, uchar *arg) override;
  Item *transform(Item_transformer transformer, uchar *arg) override;
  void traverse_cond(Cond_traverser, void *arg, traverse_order order) override;
  bool truth_transform_arguments(THD *thd, Bool_test test);
  bool subst_argument_checker(uchar **) override { return true; }
  Item *compile(Item_analyzer analyzer, uchar **arg_p,
                Item_transformer transformer, uchar *arg_t) override;
  bool remove_const_conds(THD *thd, Item *item, Item **new_item);
  /// Treat UNKNOWN result like FALSE because callers see no difference
  bool ignore_unknown() const { return abort_on_null; }
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
  /// List of equal field items.
  List<Item_field> fields;
  /// Optional constant item equal to all the field items.
  Item *m_const_arg{nullptr};
  /// Helper for comparing the fields.
  cmp_item *eval_item{nullptr};
  /// Helper for comparing constants.
  Arg_comparator cmp;
  /// Flag set to true if the equality is known to be always false.
  bool cond_false{false};
  /// Should constants be compared as datetimes?
  bool compare_as_dates{false};

 public:
  ~Item_equal() override;

  Item_equal(Item_field *f1, Item_field *f2);
  Item_equal(Item *c, Item_field *f);
  explicit Item_equal(Item_equal *item_equal);

  Item *const_arg() const { return m_const_arg; }
  void set_const_arg(Item *c) { m_const_arg = c; }
  bool compare_const(THD *thd, Item *c);
  bool add(THD *thd, Item *c, Item_field *f);
  bool add(THD *thd, Item *c);
  void add(Item_field *f);
  uint members();
  bool contains(const Field *field) const;
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
  optimize_type select_optimize(const THD *) override { return OPTIMIZE_EQUAL; }
  bool cast_incompatible_args(uchar *) override {
    // Multiple equality nodes (Item_equal) should have been
    // converted back to simple equalities (Item_func_eq) by
    // substitute_for_best_equal_field before cast nodes are injected.
    assert(false);
    return false;
  }
  bool contains_only_equi_join_condition() const override {
    return const_arg() == nullptr;
  }

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

  // A class to iterate over fields without exposing fields directly.
  class FieldProxy {
   public:
    explicit FieldProxy(Item_equal *item) : m_fields(&item->fields) {}
    List_STL_Iterator<Item_field> begin() { return m_fields->begin(); }
    List_STL_Iterator<Item_field> end() { return m_fields->end(); }
    List_STL_Iterator<const Item_field> begin() const {
      return m_fields->cbegin();
    }
    List_STL_Iterator<const Item_field> end() const { return m_fields->cend(); }
    List_STL_Iterator<const Item_field> cbegin() const {
      return m_fields->cbegin();
    }
    List_STL_Iterator<const Item_field> cend() const {
      return m_fields->cend();
    }

   private:
    List<Item_field> *m_fields;
  };
  class ConstFieldProxy {
   public:
    explicit ConstFieldProxy(const Item_equal *item)
        : m_fields(&item->fields) {}
    List_STL_Iterator<const Item_field> begin() const {
      return m_fields->cbegin();
    }
    List_STL_Iterator<const Item_field> end() const { return m_fields->cend(); }
    List_STL_Iterator<const Item_field> cbegin() const {
      return m_fields->cbegin();
    }
    List_STL_Iterator<const Item_field> cend() const {
      return m_fields->cend();
    }
    size_t size() const { return m_fields->size(); }

   private:
    const List<Item_field> *m_fields;
  };
  FieldProxy get_fields() { return FieldProxy(this); }
  ConstFieldProxy get_fields() const { return ConstFieldProxy(this); }

  bool resolve_type(THD *) override;
  bool fix_fields(THD *thd, Item **ref) override;
  void update_used_tables() override;
  bool walk(Item_processor processor, enum_walk walk, uchar *arg) override;
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
  bool eq(const Item *item, bool binary_cmp) const override;
  const CHARSET_INFO *compare_collation() const override {
    return fields.head()->collation.collation;
  }

  bool equality_substitution_analyzer(uchar **) override { return true; }

  Item *equality_substitution_transformer(uchar *arg) override;

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;
  Item *m_const_folding[2];  ///< temporary area used for constant folding

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
  COND_EQUAL() { upper_levels = nullptr; }
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
  Item *truth_transformer(THD *, Bool_test) override;
  bool gc_subst_analyzer(uchar **) override { return true; }

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;

  bool contains_only_equi_join_condition() const override;
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
  Item *truth_transformer(THD *, Bool_test) override;
  bool gc_subst_analyzer(uchar **) override { return true; }

  float get_filtering_effect(THD *thd, table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table) override;
};

/// Builds condition: (a AND b) IS TRUE
inline Item *and_conds(Item *a, Item *b) {
  if (!b) return a;
  if (!a) return b;

  Item *item = new Item_cond_and(a, b);
  if (item == nullptr) return nullptr;
  item->apply_is_true();
  return item;
}

longlong get_datetime_value(THD *thd, Item ***item_arg, Item ** /* cache_arg */,
                            const Item *warn_item, bool *is_null);

// TODO: the next two functions should be moved to sql_time.{h,cc}
bool get_mysql_time_from_str_no_warn(THD *thd, String *str, MYSQL_TIME *l_time,
                                     MYSQL_TIME_STATUS *status);

bool get_mysql_time_from_str(THD *thd, String *str,
                             enum_mysql_timestamp_type warn_type,
                             const char *warn_name, MYSQL_TIME *l_time);

// Helper function to ensure_multi_equality_fields_are_available().
// Finds and adjusts (if "replace" is set to true) an "Item_field" in a
// function with an equal field in the available tables. For more
// details look at FindEqualField().
void find_and_adjust_equal_fields(Item *item, table_map available_tables,
                                  bool replace, bool *found);

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

/// Returns true if the item is a conjunction.
inline bool IsAnd(const Item *item) {
  return item->type() == Item::COND_ITEM &&
         down_cast<const Item_cond *>(item)->functype() ==
             Item_func::COND_AND_FUNC;
}

/**
  Calls "func" on each term in "condition" if it's a conjunction (and
  recursively on any conjunction directly contained in it, thereby flattening
  nested AND structures). Otherwise, calls "func" on "condition". It aborts and
  returns true as soon as a call to "func" returns true.
 */
template <class Func>
bool WalkConjunction(Item *condition, Func func) {
  if (condition == nullptr) {
    return false;
  } else if (IsAnd(condition)) {
    for (Item &item : *down_cast<Item_cond_and *>(condition)->argument_list()) {
      if (WalkConjunction(&item, func)) {
        return true;
      }
    }
    return false;
  } else {
    return func(condition);
  }
}

#endif /* ITEM_CMPFUNC_INCLUDED */
