#ifndef ITEM_CMPFUNC_INCLUDED
#define ITEM_CMPFUNC_INCLUDED

/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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


/* compare and test functions */

#include "mem_root_array.h"  // Mem_root_array
#include "my_regex.h"        // my_regex_t
#include "item_func.h"       // Item_int_func
#include "item_row.h"        // Item_row
#include "template_utils.h"  // down_cast

class Arg_comparator;
class Item_sum_hybrid;
class Item_row;
struct Json_scalar_holder;

typedef int (Arg_comparator::*arg_cmp_func)();

typedef int (*Item_field_cmpfunc)(Item_field *f1, Item_field *f2, void *arg); 

class Arg_comparator: public Sql_alloc
{
  Item **a, **b;
  arg_cmp_func func;
  Item_result_field *owner;
  Arg_comparator *comparators;   // used only for compare_row()
  uint16 comparator_count;
  double precision;
  /* Fields used in DATE/DATETIME comparison. */
  Item *a_cache, *b_cache;         // Cached values of a and b items
  bool is_nulls_eq;                // TRUE <=> compare for the EQUAL_FUNC
  bool set_null;                   // TRUE <=> set owner->null_value
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

  Arg_comparator(): comparators(0), comparator_count(0),
    a_cache(0), b_cache(0), set_null(TRUE),
    get_value_a_func(0), get_value_b_func(0), json_scalar(0)
  {}
  Arg_comparator(Item **a1, Item **a2): a(a1), b(a2),
    comparators(0), comparator_count(0),
    a_cache(0), b_cache(0), set_null(TRUE),
    get_value_a_func(0), get_value_b_func(0), json_scalar(0)
  {}

  int set_compare_func(Item_result_field *owner, Item_result type);
  inline int set_compare_func(Item_result_field *owner_arg)
  {
    return set_compare_func(owner_arg, item_cmp_type((*a)->result_type(),
                                                     (*b)->result_type()));
  }
  int set_cmp_func(Item_result_field *owner_arg,
                   Item **a1, Item **a2,
                   Item_result type);

  int set_cmp_func(Item_result_field *owner_arg,
                   Item **a1, Item **a2, bool set_null_arg);

  inline int compare() { return (this->*func)(); }

  int compare_string();		 // compare args[0] & args[1]
  int compare_binary_string();	 // compare args[0] & args[1]
  int compare_real();            // compare args[0] & args[1]
  int compare_decimal();         // compare args[0] & args[1]
  int compare_int_signed();      // compare args[0] & args[1]
  int compare_int_signed_unsigned();
  int compare_int_unsigned_signed();
  int compare_int_unsigned();
  int compare_time_packed();
  int compare_e_time_packed();
  int compare_row();             // compare args[0] & args[1]
  int compare_e_string();	 // compare args[0] & args[1]
  int compare_e_binary_string(); // compare args[0] & args[1]
  int compare_e_real();          // compare args[0] & args[1]
  int compare_e_decimal();       // compare args[0] & args[1]
  int compare_e_int();           // compare args[0] & args[1]
  int compare_e_int_diff_signedness();
  int compare_e_row();           // compare args[0] & args[1]
  int compare_real_fixed();
  int compare_e_real_fixed();
  int compare_datetime();        // compare args[0] & args[1] as DATETIMEs
  int compare_json();

  static bool can_compare_as_dates(Item *a, Item *b, ulonglong *const_val_arg);

  Item** cache_converted_constant(THD *thd, Item **value, Item **cache,
                                  Item_result type);
  void set_datetime_cmp_func(Item_result_field *owner_arg, Item **a1, Item **b1);
  static arg_cmp_func comparator_matrix [5][2];
  inline bool is_owner_equal_func()
  {
    return (owner->type() == Item::FUNC_ITEM &&
           ((Item_func*)owner)->functype() == Item_func::EQUAL_FUNC);
  }
  void cleanup();
  /*
    Set correct cmp_context if items would be compared as INTs.
  */
  inline void set_cmp_context_for_datetime()
  {
    DBUG_ASSERT(func == &Arg_comparator::compare_datetime);
    if ((*a)->is_temporal())
      (*a)->cmp_context= INT_RESULT;
    if ((*b)->is_temporal())
      (*b)->cmp_context= INT_RESULT;
  }
  friend class Item_func;
};

class Item_bool_func :public Item_int_func
{
public:
  Item_bool_func() : Item_int_func(), m_created_by_in2exists(false) {}
  explicit Item_bool_func(const POS &pos)
  : Item_int_func(pos), m_created_by_in2exists(false)
  {}

  Item_bool_func(Item *a) : Item_int_func(a),
    m_created_by_in2exists(false)  {}
  Item_bool_func(const POS &pos, Item *a) : Item_int_func(pos, a),
    m_created_by_in2exists(false)  {}

  Item_bool_func(Item *a,Item *b) : Item_int_func(a,b),
    m_created_by_in2exists(false)  {}
  Item_bool_func(const POS &pos, Item *a,Item *b) : Item_int_func(pos, a,b),
    m_created_by_in2exists(false)  {}

  Item_bool_func(THD *thd, Item_bool_func *item) : Item_int_func(thd, item),
    m_created_by_in2exists(item->m_created_by_in2exists) {}
  bool is_bool_func() { return 1; }
  void fix_length_and_dec() { decimals=0; max_length=1; }
  uint decimal_precision() const { return 1; }
  virtual bool created_by_in2exists() const { return m_created_by_in2exists; }
  void set_created_by_in2exists() { m_created_by_in2exists= true; }
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

class Item_func_truth : public Item_bool_func
{
public:
  virtual bool val_bool();
  virtual longlong val_int();
  virtual void fix_length_and_dec();
  virtual void print(String *str, enum_query_type query_type);

protected:
  Item_func_truth(const POS &pos, Item *a, bool a_value, bool a_affirmative)
  : Item_bool_func(pos, a), value(a_value), affirmative(a_affirmative)
  {}

  ~Item_func_truth()
  {}
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

class Item_func_istrue : public Item_func_truth
{
public:
  Item_func_istrue(const POS &pos, Item *a)
    : Item_func_truth(pos, a, true, true)
  {}
  ~Item_func_istrue() {}
  virtual const char* func_name() const { return "istrue"; }
};


/**
  This Item represents a <code>X IS NOT TRUE</code> boolean predicate.
*/

class Item_func_isnottrue : public Item_func_truth
{
public:
  Item_func_isnottrue(const POS &pos, Item *a)
    : Item_func_truth(pos, a, true, false)
  {}
  ~Item_func_isnottrue() {}
  virtual const char* func_name() const { return "isnottrue"; }
};


/**
  This Item represents a <code>X IS FALSE</code> boolean predicate.
*/

class Item_func_isfalse : public Item_func_truth
{
public:
  Item_func_isfalse(const POS &pos, Item *a)
    : Item_func_truth(pos, a, false, true)
  {}
  ~Item_func_isfalse() {}
  virtual const char* func_name() const { return "isfalse"; }
};


/**
  This Item represents a <code>X IS NOT FALSE</code> boolean predicate.
*/

class Item_func_isnotfalse : public Item_func_truth
{
public:
  Item_func_isnotfalse(const POS &pos, Item *a)
    : Item_func_truth(pos, a, false, false)
  {}
  ~Item_func_isnotfalse() {}
  virtual const char* func_name() const { return "isnotfalse"; }
};


class Item_cache;
#define UNKNOWN ((my_bool)-1)


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

class Item_in_optimizer: public Item_bool_func
{
private:
  Item_cache *cache;
  bool save_cache;
  /* 
    Stores the value of "NULL IN (SELECT ...)" for uncorrelated subqueries:
      UNKNOWN - "NULL in (SELECT ...)" has not yet been evaluated
      FALSE   - result is FALSE
      TRUE    - result is NULL
  */
  my_bool result_for_null_param;
public:
  Item_in_optimizer(Item *a, Item_in_subselect *b):
    Item_bool_func(a, reinterpret_cast<Item *>(b)), cache(0),
    save_cache(0), result_for_null_param(UNKNOWN)
  { with_subselect= TRUE; }
  bool fix_fields(THD *, Item **);
  bool fix_left(THD *thd, Item **ref);
  void fix_after_pullout(st_select_lex *parent_select,
                         st_select_lex *removed_select);
  bool is_null();
  longlong val_int();
  void cleanup();
  const char *func_name() const { return "<in_optimizer>"; }
  Item_cache **get_cache() { return &cache; }
  void keep_top_level_cache();
  Item *transform(Item_transformer transformer, uchar *arg);
  void replace_argument(THD *thd, Item **oldpp, Item *newp);
};

/// Abstract factory interface for creating comparison predicates.
class Comp_creator
{
public:
  virtual ~Comp_creator() {}
  virtual Item_bool_func* create(Item *a, Item *b) const = 0;

  /// This interface is only used by Item_allany_subselect.
  virtual const char* symbol(bool invert) const = 0;
  virtual bool eqne_op() const = 0;
  virtual bool l_op() const = 0;
};

/// Abstract base class for the comparison operators =, <> and <=>.
class Linear_comp_creator :public Comp_creator
{
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

class Eq_creator :public Linear_comp_creator
{
public:
  virtual const char* symbol(bool invert) const { return invert ? "<>" : "="; }

protected:
  virtual Item_bool_func *create_scalar_predicate(Item *a, Item *b) const;
  virtual Item_bool_func *combine(List<Item> list) const;
};

class Equal_creator :public Linear_comp_creator
{
public:
  virtual const char* symbol(bool invert) const
  {
    // This will never be called with true.
    DBUG_ASSERT(!invert);
    return "<=>";
  }

protected:
  virtual Item_bool_func *create_scalar_predicate(Item *a, Item *b) const;
  virtual Item_bool_func *combine(List<Item> list) const;
};

class Ne_creator :public Linear_comp_creator
{
public:
  virtual const char* symbol(bool invert) const { return invert ? "=" : "<>"; }

protected:
  virtual Item_bool_func *create_scalar_predicate(Item *a, Item *b) const;
  virtual Item_bool_func *combine(List<Item> list) const;
};

class Gt_creator :public Comp_creator
{
public:
  Gt_creator() {}                             /* Remove gcc warning */
  virtual ~Gt_creator() {}                    /* Remove gcc warning */
  virtual Item_bool_func* create(Item *a, Item *b) const;
  virtual const char* symbol(bool invert) const { return invert? "<=" : ">"; }
  virtual bool eqne_op() const { return 0; }
  virtual bool l_op() const { return 0; }
};

class Lt_creator :public Comp_creator
{
public:
  Lt_creator() {}                             /* Remove gcc warning */
  virtual ~Lt_creator() {}                    /* Remove gcc warning */
  virtual Item_bool_func* create(Item *a, Item *b) const;
  virtual const char* symbol(bool invert) const { return invert? ">=" : "<"; }
  virtual bool eqne_op() const { return 0; }
  virtual bool l_op() const { return 1; }
};

class Ge_creator :public Comp_creator
{
public:
  Ge_creator() {}                             /* Remove gcc warning */
  virtual ~Ge_creator() {}                    /* Remove gcc warning */
  virtual Item_bool_func* create(Item *a, Item *b) const;
  virtual const char* symbol(bool invert) const { return invert? "<" : ">="; }
  virtual bool eqne_op() const { return 0; }
  virtual bool l_op() const { return 0; }
};

class Le_creator :public Comp_creator
{
public:
  Le_creator() {}                             /* Remove gcc warning */
  virtual ~Le_creator() {}                    /* Remove gcc warning */
  virtual Item_bool_func* create(Item *a, Item *b) const;
  virtual const char* symbol(bool invert) const { return invert? ">" : "<="; }
  virtual bool eqne_op() const { return 0; }
  virtual bool l_op() const { return 1; }
};

class Item_bool_func2 :public Item_bool_func
{						/* Bool with 2 string args */
private:
  bool convert_constant_arg(THD *thd, Item *field, Item **item);
protected:
  Arg_comparator cmp;
  bool abort_on_null;

public:
  Item_bool_func2(Item *a,Item *b)
    :Item_bool_func(a,b), cmp(tmp_arg, tmp_arg+1), abort_on_null(FALSE) {}

  Item_bool_func2(const POS &pos, Item *a,Item *b)
    :Item_bool_func(pos, a,b), cmp(tmp_arg, tmp_arg+1), abort_on_null(FALSE)
  {}

  void fix_length_and_dec();
  int set_cmp_func()
  {
    return cmp.set_cmp_func(this, tmp_arg, tmp_arg+1, TRUE);
  }
  optimize_type select_optimize() const { return OPTIMIZE_OP; }
  virtual enum Functype rev_functype() const { return UNKNOWN_FUNC; }
  bool have_rev_func() const { return rev_functype() != UNKNOWN_FUNC; }

  virtual inline void print(String *str, enum_query_type query_type)
  {
    Item_func::print_op(str, query_type);
  }

  bool is_null() { return MY_TEST(args[0]->is_null() || args[1]->is_null()); }
  const CHARSET_INFO *compare_collation()
  { return cmp.cmp_collation.collation; }
  void top_level_item() { abort_on_null= TRUE; }
  void cleanup()
  {
    Item_bool_func::cleanup();
    cmp.cleanup();
  }

  friend class  Arg_comparator;
};

class Item_bool_rowready_func2 :public Item_bool_func2
{
public:
  Item_bool_rowready_func2(Item *a, Item *b) :Item_bool_func2(a, b)
  {
    allowed_arg_cols= 0;  // Fetch this value from first argument
  }
  Item_bool_rowready_func2(const POS &pos, Item *a, Item *b)
    : Item_bool_func2(pos, a, b)
  {
    allowed_arg_cols= 0;  // Fetch this value from first argument
  }

  Item *neg_transformer(THD *thd);
  virtual Item *negated_item();
  bool subst_argument_checker(uchar **arg) { return TRUE; }
};

/**
  XOR inherits from Item_bool_func2 because it is not optimized yet.
  Later, when XOR is optimized, it needs to inherit from
  Item_cond instead. See WL#5800. 
*/
class Item_func_xor :public Item_bool_func2
{
public:
  Item_func_xor(Item *i1, Item *i2) :Item_bool_func2(i1, i2) {}
  Item_func_xor(const POS &pos, Item *i1, Item *i2)
    : Item_bool_func2(pos, i1, i2)
  {}

  enum Functype functype() const { return XOR_FUNC; }
  const char *func_name() const { return "xor"; }
  longlong val_int();
  void top_level_item() {}
  Item *neg_transformer(THD *thd);

  float get_filtering_effect(table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table);
};

class Item_func_not :public Item_bool_func
{
public:
  Item_func_not(Item *a) :Item_bool_func(a) {}
  Item_func_not(const POS &pos, Item *a) :Item_bool_func(pos, a) {}

  longlong val_int();
  enum Functype functype() const { return NOT_FUNC; }
  const char *func_name() const { return "not"; }
  Item *neg_transformer(THD *thd);
  virtual void print(String *str, enum_query_type query_type);

  float get_filtering_effect(table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table);
};

class Item_maxmin_subselect;
class JOIN;

/*
  trigcond<param>(arg) ::= param? arg : TRUE

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

class Item_func_trig_cond: public Item_bool_func
{
public:
  enum enum_trig_type
  {
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
     @param a             the item for <condition>
     @param f             pointer to trigger variable
     @param join          if a table's property is the source of 'f', JOIN
     which owns this table; NULL otherwise.
     @param idx           if join!=NULL: index of this table in the
     JOIN_TAB/QEP_TAB array. NO_PLAN_IDX otherwise.
     @param trig_type_arg type of 'f'
  */
  Item_func_trig_cond(Item *a, bool *f, JOIN *join, plan_idx idx,
                      enum_trig_type trig_type_arg)
  : Item_bool_func(a), trig_var(f), m_join(join), m_idx(idx),
    trig_type(trig_type_arg)
  {}
  longlong val_int();
  enum Functype functype() const { return TRIG_COND_FUNC; };
  /// '<if>', to distinguish from the if() SQL function
  const char *func_name() const { return "<if>"; };
  bool const_item() const { return FALSE; }
  bool *get_trig_var() { return trig_var; }
  /* The following is needed for ICP: */
  table_map used_tables() const { return args[0]->used_tables(); }
  void print(String *str, enum_query_type query_type);
};


class Item_func_not_all :public Item_func_not
{
  /* allow to check presence of values in max/min optimization */
  Item_sum_hybrid *test_sum_item;
  Item_maxmin_subselect *test_sub_item;
  Item_subselect *subselect;

  bool abort_on_null;
public:
  bool show;

  Item_func_not_all(Item *a)
    :Item_func_not(a), test_sum_item(0), test_sub_item(0), subselect(0),
     abort_on_null(0), show(0)
    {}
  virtual void top_level_item() { abort_on_null= 1; }
  bool is_top_level_item() const { return abort_on_null; }
  longlong val_int();
  enum Functype functype() const { return NOT_ALL_FUNC; }
  const char *func_name() const { return "<not>"; }
  virtual void print(String *str, enum_query_type query_type);
  void set_sum_test(Item_sum_hybrid *item) { test_sum_item= item; };
  void set_sub_test(Item_maxmin_subselect *item) { test_sub_item= item; };
  void set_subselect(Item_subselect *item) { subselect= item; }
  table_map not_null_tables() const
  {
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
  Item *neg_transformer(THD *thd);
};


class Item_func_nop_all :public Item_func_not_all
{
public:

  Item_func_nop_all(Item *a) :Item_func_not_all(a) {}
  longlong val_int();
  const char *func_name() const { return "<nop>"; }
  table_map not_null_tables() const { return not_null_tables_cache; }
  Item *neg_transformer(THD *thd);
};


class Item_func_eq :public Item_bool_rowready_func2
{
public:
  Item_func_eq(Item *a,Item *b) :
    Item_bool_rowready_func2( a, b)
  {}
  Item_func_eq(const POS &pos, Item *a,Item *b) :
    Item_bool_rowready_func2(pos, a, b)
  {}
  longlong val_int();
  enum Functype functype() const { return EQ_FUNC; }
  enum Functype rev_functype() const { return EQ_FUNC; }
  cond_result eq_cmp_result() const { return COND_TRUE; }
  const char *func_name() const { return "="; }
  Item *negated_item();
  virtual bool equality_substitution_analyzer(uchar **arg) { return true; }
  virtual Item* equality_substitution_transformer(uchar *arg);
  bool gc_subst_analyzer(uchar **arg) { return true; }

  float get_filtering_effect(table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table);
};

class Item_func_equal :public Item_bool_rowready_func2
{
public:
  Item_func_equal(Item *a,Item *b) :Item_bool_rowready_func2(a,b) {};
  Item_func_equal(const POS &pos, Item *a,Item *b)
    : Item_bool_rowready_func2(pos, a,b)
  {};

  longlong val_int();
  void fix_length_and_dec();
  table_map not_null_tables() const { return 0; }
  enum Functype functype() const { return EQUAL_FUNC; }
  enum Functype rev_functype() const { return EQUAL_FUNC; }
  cond_result eq_cmp_result() const { return COND_TRUE; }
  const char *func_name() const { return "<=>"; }
  Item *neg_transformer(THD *thd) { return 0; }

  float get_filtering_effect(table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table);
};


class Item_func_ge :public Item_bool_rowready_func2
{
public:
  Item_func_ge(Item *a,Item *b) :Item_bool_rowready_func2(a,b) {};
  longlong val_int();
  enum Functype functype() const { return GE_FUNC; }
  enum Functype rev_functype() const { return LE_FUNC; }
  cond_result eq_cmp_result() const { return COND_TRUE; }
  const char *func_name() const { return ">="; }
  Item *negated_item();
  bool gc_subst_analyzer(uchar **arg) { return true; }

  float get_filtering_effect(table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table);
};

class Item_func_gt :public Item_bool_rowready_func2
{
public:
  Item_func_gt(Item *a,Item *b) :Item_bool_rowready_func2(a,b) {};
  longlong val_int();
  enum Functype functype() const { return GT_FUNC; }
  enum Functype rev_functype() const { return LT_FUNC; }
  cond_result eq_cmp_result() const { return COND_FALSE; }
  const char *func_name() const { return ">"; }
  Item *negated_item();
  bool gc_subst_analyzer(uchar **arg) { return true; }

  float get_filtering_effect(table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table);
};


class Item_func_le :public Item_bool_rowready_func2
{
public:
  Item_func_le(Item *a,Item *b) :Item_bool_rowready_func2(a,b) {};
  longlong val_int();
  enum Functype functype() const { return LE_FUNC; }
  enum Functype rev_functype() const { return GE_FUNC; }
  cond_result eq_cmp_result() const { return COND_TRUE; }
  const char *func_name() const { return "<="; }
  Item *negated_item();
  bool gc_subst_analyzer(uchar **arg) { return true; }

  float get_filtering_effect(table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table);
};


class Item_func_lt :public Item_bool_rowready_func2
{
public:
  Item_func_lt(Item *a,Item *b) :Item_bool_rowready_func2(a,b) {}
  longlong val_int();
  enum Functype functype() const { return LT_FUNC; }
  enum Functype rev_functype() const { return GT_FUNC; }
  cond_result eq_cmp_result() const { return COND_FALSE; }
  const char *func_name() const { return "<"; }
  Item *negated_item();
  bool gc_subst_analyzer(uchar **arg) { return true; }

  float get_filtering_effect(table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table);
};


class Item_func_ne :public Item_bool_rowready_func2
{
public:
  Item_func_ne(Item *a,Item *b) :Item_bool_rowready_func2(a,b) {}
  longlong val_int();
  enum Functype functype() const { return NE_FUNC; }
  cond_result eq_cmp_result() const { return COND_FALSE; }
  optimize_type select_optimize() const { return OPTIMIZE_KEY; } 
  const char *func_name() const { return "<>"; }
  Item *negated_item();

  float get_filtering_effect(table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table);
};


/*
  The class Item_func_opt_neg is defined to factor out the functionality
  common for the classes Item_func_between and Item_func_in. The objects
  of these classes can express predicates or there negations.
  The alternative approach would be to create pairs Item_func_between,
  Item_func_notbetween and Item_func_in, Item_func_notin.

*/

class Item_func_opt_neg :public Item_int_func
{
public:
  bool negated;     /* <=> the item represents NOT <func> */
  bool pred_level;  /* <=> [NOT] <func> is used on a predicate level */
public:
  Item_func_opt_neg(const POS &pos, Item *a, Item *b, Item *c, bool is_negation)
    :Item_int_func(pos, a, b, c), negated(0), pred_level(0) 
  {
    if (is_negation)
      negate();
  }
  Item_func_opt_neg(const POS &pos, PT_item_list *list, bool is_negation)
    :Item_int_func(pos, list), negated(0), pred_level(0)
  {
    if (is_negation)
      negate();
  }
public:
  inline void negate() { negated= !negated; }
  inline void top_level_item() { pred_level= 1; }
  bool is_top_level_item() const { return pred_level; }
  Item *neg_transformer(THD *thd)
  {
    negated= !negated;
    return this;
  }
  bool eq(const Item *item, bool binary_cmp) const;
  bool subst_argument_checker(uchar **arg) { return TRUE; }
};


class Item_func_between :public Item_func_opt_neg
{
  DTCollation cmp_collation;
public:
  Item_result cmp_type;
  String value0,value1,value2;
  /* TRUE <=> arguments will be compared as dates. */
  bool compare_as_dates_with_strings;
  bool compare_as_temporal_dates;
  bool compare_as_temporal_times;
  
  /* Comparators used for DATE/DATETIME comparison. */
  Arg_comparator ge_cmp, le_cmp;
  Item_func_between(const POS &pos, Item *a, Item *b, Item *c, bool is_negation)
    :Item_func_opt_neg(pos, a, b, c, is_negation),
    compare_as_dates_with_strings(FALSE),
    compare_as_temporal_dates(FALSE),
    compare_as_temporal_times(FALSE) {}
  longlong val_int();
  optimize_type select_optimize() const { return OPTIMIZE_KEY; }
  enum Functype functype() const   { return BETWEEN; }
  const char *func_name() const { return "between"; }
  bool fix_fields(THD *, Item **);
  void fix_after_pullout(st_select_lex *parent_select,
                         st_select_lex *removed_select);
  void fix_length_and_dec();
  virtual void print(String *str, enum_query_type query_type);
  bool is_bool_func() { return 1; }
  const CHARSET_INFO *compare_collation() { return cmp_collation.collation; }
  uint decimal_precision() const { return 1; }
  bool gc_subst_analyzer(uchar **arg) { return true; }

  float get_filtering_effect(table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table);
};


class Item_func_strcmp :public Item_bool_func2
{
public:
  Item_func_strcmp(const POS &pos, Item *a, Item *b) :Item_bool_func2(pos, a, b)
  {}
  longlong val_int();
  optimize_type select_optimize() const { return OPTIMIZE_NONE; }
  const char *func_name() const { return "strcmp"; }

  virtual inline void print(String *str, enum_query_type query_type)
  {
    Item_func::print(str, query_type);
  }
  void fix_length_and_dec()
  {
    Item_bool_func2::fix_length_and_dec();
    fix_char_length(2); // returns "1" or "0" or "-1"
  }
};


struct interval_range
{
  Item_result type;
  double dbl;
  my_decimal dec;
};

class Item_func_interval :public Item_int_func
{
  typedef Item_int_func super;

  Item_row *row;
  my_bool use_decimal_comparison;
  interval_range *intervals;
public:
  Item_func_interval(const POS &pos, MEM_ROOT *mem_root, Item *expr1,
                     Item *expr2, class PT_item_list *opt_expr_list= NULL)
    :super(pos, alloc_row(pos, mem_root, expr1, expr2, opt_expr_list)),
     intervals(0)
  {
    allowed_arg_cols= 0;    // Fetch this value from first argument
  }

  virtual bool itemize(Parse_context *pc, Item **res);
  longlong val_int();
  void fix_length_and_dec();
  const char *func_name() const { return "interval"; }
  uint decimal_precision() const { return 2; }
  void print(String *str, enum_query_type query_type);

private:
  Item_row *alloc_row(const POS &pos, MEM_ROOT *mem_root, Item *expr1,
                      Item *expr2, class PT_item_list *opt_expr_list);
};


class Item_func_coalesce :public Item_func_numhybrid
{
protected:
  enum_field_types cached_field_type;
  Item_func_coalesce(const POS &pos, Item *a, Item *b)
    : Item_func_numhybrid(pos, a, b)
  {}
  Item_func_coalesce(const POS &pos, Item *a)
    : Item_func_numhybrid(pos, a)
  {}
public:
  Item_func_coalesce(const POS &pos, PT_item_list *list);
  double real_op();
  longlong int_op();
  String *str_op(String *);
  /**
    Get the result of COALESCE as a JSON value.
    @param[in,out] wr   the result value holder
  */
  bool val_json(Json_wrapper *wr);
  bool date_op(MYSQL_TIME *ltime, my_time_flags_t fuzzydate);
  bool time_op(MYSQL_TIME *ltime);
  my_decimal *decimal_op(my_decimal *);
  void fix_length_and_dec();
  void find_num_type() {}
  enum Item_result result_type () const { return hybrid_type; }
  const char *func_name() const { return "coalesce"; }
  table_map not_null_tables() const { return 0; }
  enum_field_types field_type() const { return cached_field_type; }
};


class Item_func_ifnull :public Item_func_coalesce
{
protected:
  bool field_type_defined;
public:
  Item_func_ifnull(const POS &pos, Item *a, Item *b)
    : Item_func_coalesce(pos, a, b)
  {}
  double real_op();
  longlong int_op();
  String *str_op(String *str);
  bool date_op(MYSQL_TIME *ltime, my_time_flags_t fuzzydate);
  bool time_op(MYSQL_TIME *ltime);
  my_decimal *decimal_op(my_decimal *);
  bool val_json(Json_wrapper *result);
  void fix_length_and_dec();
  const char *func_name() const { return "ifnull"; }
  Field *tmp_table_field(TABLE *table);
  uint decimal_precision() const;
};


/**
   ANY_VALUE(expr) is like expr except that it is not checked by
   aggregate_check logic. It serves as a solution for users who want to
   bypass this logic.
*/
class Item_func_any_value :public Item_func_coalesce
{
public:
  Item_func_any_value(const POS &pos, Item *a) :Item_func_coalesce(pos, a) {}
  const char *func_name() const { return "any_value"; }
  bool aggregate_check_group(uchar *arg);
  bool aggregate_check_distinct(uchar *arg);
};


class Item_func_if :public Item_func
{
  enum Item_result cached_result_type;
  enum_field_types cached_field_type;
public:
  Item_func_if(Item *a,Item *b,Item *c)
    :Item_func(a,b,c), cached_result_type(INT_RESULT)
  {}
  Item_func_if(const POS &pos, Item *a,Item *b,Item *c)
    :Item_func(pos, a,b,c), cached_result_type(INT_RESULT)
  {}

  double val_real();
  longlong val_int();
  String *val_str(String *str);
  my_decimal *val_decimal(my_decimal *);
  bool val_json(Json_wrapper *wr);
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate);
  bool get_time(MYSQL_TIME *ltime);
  enum Item_result result_type () const { return cached_result_type; }
  enum_field_types field_type() const { return cached_field_type; }
  bool fix_fields(THD *, Item **);
  void fix_length_and_dec();
  void fix_after_pullout(st_select_lex *parent_select,
                         st_select_lex *removed_select);
  uint decimal_precision() const;
  const char *func_name() const { return "if"; }
private:
  void cache_type_info(Item *source);
};


class Item_func_nullif :public Item_bool_func2
{
  enum Item_result cached_result_type;
public:
  Item_func_nullif(const POS &pos, Item *a, Item *b)
    :Item_bool_func2(pos, a, b), cached_result_type(INT_RESULT)
  {}
  double val_real();
  longlong val_int();
  String *val_str(String *str);
  my_decimal *val_decimal(my_decimal *);
  enum Item_result result_type () const { return cached_result_type; }
  void fix_length_and_dec();
  uint decimal_precision() const { return args[0]->decimal_precision(); }
  const char *func_name() const { return "nullif"; }

  virtual inline void print(String *str, enum_query_type query_type)
  {
    Item_func::print(str, query_type);
  }

  table_map not_null_tables() const { return 0; }
  bool is_null();
};


/* Functions to handle the optimized IN */


/* A vector of values of some type  */

class in_vector :public Sql_alloc
{
public:
  const uint count;  ///< Original size of the vector
  uint used_count;   ///< The actual size of the vector (NULL may be ignored)

  /**
    See Item_func_in::fix_length_and_dec for why we need both
    count and used_count.
   */
  explicit in_vector(uint elements)
    : count(elements), used_count(elements)
  {}

  virtual ~in_vector() {}
  virtual void set(uint pos,Item *item)=0;
  virtual uchar *get_value(Item *item)=0;

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
  virtual Item* create_item() { return NULL; }
  
  /*
    Store the value at position #pos into provided item object
    SYNOPSIS
      value_to_item()
        pos   Index of value to store
        item  Constant item to store value into. The item must be of the same
              type that create_item() returns.
  */
  virtual void value_to_item(uint pos, Item *item) { }
  
  /* Compare values number pos1 and pos2 for equality */
  virtual bool compare_elems(uint pos1, uint pos2) const = 0;

  virtual Item_result result_type()= 0;
};

class in_string :public in_vector
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  String tmp;
  // DTOR is not trivial, but we manage memory ourselves.
  Mem_root_array<String, true> base_objects;
  // String objects are not sortable, sort pointers instead.
  Mem_root_array<String*, true> base_pointers;

  qsort2_cmp compare;
  const CHARSET_INFO *collation;
public:
  in_string(THD *thd,
            uint elements, qsort2_cmp cmp_func, const CHARSET_INFO *cs);
  ~in_string();
  void set(uint pos,Item *item);
  uchar *get_value(Item *item);
  Item* create_item()
  { 
    return new Item_string(collation);
  }
  void value_to_item(uint pos, Item *item)
  {    
    String *str= base_pointers[pos];
    Item_string *to= (Item_string*)item;
    to->str_value= *str;
  }
  Item_result result_type() { return STRING_RESULT; }

  virtual void shrink_array(size_t n) { base_pointers.resize(n); }

  virtual void sort();
  virtual bool find_value(const void *value) const;
  virtual bool compare_elems(uint pos1, uint pos2) const;
};

class in_longlong :public in_vector
{
public:
  struct packed_longlong 
  {
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

  Mem_root_array<packed_longlong, true> base;

public:
  in_longlong(THD *thd, uint elements);
  void set(uint pos,Item *item);
  uchar *get_value(Item *item);
  
  Item* create_item()
  { 
    /* 
      We're created a signed INT, this may not be correct in 
      general case (see BUG#19342).
    */
    return new Item_int((longlong)0);
  }
  void value_to_item(uint pos, Item *item)
  {
    ((Item_int*) item)->value= base[pos].val;
    ((Item_int*) item)->unsigned_flag= (my_bool)
      base[pos].unsigned_flag;
  }
  Item_result result_type() { return INT_RESULT; }

  virtual void shrink_array(size_t n) { base.resize(n); }

  virtual void sort();
  virtual bool find_value(const void *value) const;
  virtual bool compare_elems(uint pos1, uint pos2) const;
};


class in_datetime_as_longlong :public in_longlong
{
public:
  in_datetime_as_longlong(THD *thd, uint elements)
    : in_longlong(thd, elements)
  {};
  Item *create_item()
  {
    return new Item_temporal(MYSQL_TYPE_DATETIME, 0LL);
  }
  void set(uint pos, Item *item);
  uchar *get_value(Item *item);
};


class in_time_as_longlong :public in_longlong
{
public:
  in_time_as_longlong(THD *thd, uint elements)
    : in_longlong(thd, elements)
  {};
  Item *create_item()
  {
    return new Item_temporal(MYSQL_TYPE_TIME, 0LL);
  }
  void set(uint pos, Item *item);
  uchar *get_value(Item *item);
};


/*
  Class to represent a vector of constant DATE/DATETIME values.
  Values are obtained with help of the get_datetime_value() function.
  If the left item is a constant one then its value is cached in the
  lval_cache variable.
*/
class in_datetime :public in_longlong
{
public:
  /* An item used to issue warnings. */
  Item *warn_item;
  /* Cache for the left item. */
  Item *lval_cache;

  in_datetime(THD *thd_arg, Item *warn_item_arg, uint elements)
    : in_longlong(thd_arg, elements), warn_item(warn_item_arg),
      lval_cache(0)
  {};
  void set(uint pos,Item *item);
  uchar *get_value(Item *item);

  Item* create_item()
  { 
    return new Item_temporal(MYSQL_TYPE_DATETIME, (longlong) 0);
  }
};


class in_double :public in_vector
{
  double tmp;
  Mem_root_array<double, true> base;
public:
  in_double(THD *thd, uint elements);
  void set(uint pos,Item *item);
  uchar *get_value(Item *item);
  Item *create_item()
  { 
    return new Item_float(0.0, 0);
  }
  void value_to_item(uint pos, Item *item)
  {
    ((Item_float*)item)->value= base[pos];
  }
  Item_result result_type() { return REAL_RESULT; }

  virtual void shrink_array(size_t n) { base.resize(n); }

  virtual void sort();
  virtual bool find_value(const void *value) const;
  virtual bool compare_elems(uint pos1, uint pos2) const;
};


class in_decimal :public in_vector
{
  my_decimal val;
  Mem_root_array<my_decimal, true> base;
public:
  in_decimal(THD *thd, uint elements);
  void set(uint pos, Item *item);
  uchar *get_value(Item *item);
  Item *create_item()
  { 
    return new Item_decimal(0, FALSE);
  }
  void value_to_item(uint pos, Item *item)
  {
    my_decimal *dec= &base[pos];
    Item_decimal *item_dec= (Item_decimal*)item;
    item_dec->set_decimal_value(dec);
  }
  Item_result result_type() { return DECIMAL_RESULT; }

  virtual void shrink_array(size_t n) { base.resize(n); }

  virtual void sort();
  virtual bool find_value(const void *value) const;
  virtual bool compare_elems(uint pos1, uint pos2) const;
};


/*
** Classes for easy comparing of non const items
*/

class cmp_item :public Sql_alloc
{
public:
  cmp_item() {}
  virtual ~cmp_item() {}
  virtual void store_value(Item *item)= 0;
  /**
     @returns result (TRUE, FALSE or UNKNOWN) of
     "stored argument's value <> item's value"
  */
  virtual int cmp(Item *item)= 0;
  // for optimized IN with row
  virtual int compare(const cmp_item *item) const= 0;

  /**
    Find the appropriate comparator for the given type.

    @param result_type  Used to find the appropriate comparator.
    @param item         Item object used to distinguish temporal types.
    @param cs           Charset

    @return
      New cmp_item_xxx object.
  */
  static cmp_item* get_comparator(Item_result result_type, const Item *item,
                                  const CHARSET_INFO *cs);
  virtual cmp_item *make_same()= 0;
  virtual void store_value_by_template(cmp_item *tmpl, Item *item)
  {
    store_value(item);
  }
};

/// cmp_item which stores a scalar (i.e. non-ROW).
class cmp_item_scalar : public cmp_item
{
protected:
  bool m_null_value;                            ///< If stored value is NULL
  void set_null_value(bool nv) { m_null_value= nv; }
};

class cmp_item_string : public cmp_item_scalar
{
private:
  String *value_res;
  char value_buff[STRING_BUFFER_USUAL_SIZE];
  String value;
  const CHARSET_INFO *cmp_charset;
public:
  cmp_item_string (const CHARSET_INFO *cs)
    : value(value_buff, sizeof(value_buff), cs), cmp_charset(cs)
  {}

  virtual int compare(const cmp_item *ci) const
  {
    const cmp_item_string *l_cmp= down_cast<const cmp_item_string*>(ci);
    return sortcmp(value_res, l_cmp->value_res, cmp_charset);
  }

  virtual void store_value(Item *item)
  {
    String *res= item->val_str(&value);
    if (res && (res != &value || !res->is_alloced()))
    {
      // 'res' may point in item's transient internal data, so make a copy
      value.copy(*res);
    }
    value_res= &value;
    set_null_value(item->null_value);
  }

  virtual int cmp(Item *arg)
  {
    char buff[STRING_BUFFER_USUAL_SIZE];
    String tmp(buff, sizeof(buff), cmp_charset);
    String *res= arg->val_str(&tmp);
    if (m_null_value || arg->null_value)
      return UNKNOWN;
    if (value_res && res)
      return sortcmp(value_res, res, cmp_charset) != 0;
    else if (!value_res && !res)
      return FALSE;
    else
      return TRUE;
  }
  virtual cmp_item *make_same();
};


class cmp_item_int : public cmp_item_scalar
{
  longlong value;
public:
  cmp_item_int() {}                           /* Remove gcc warning */
  void store_value(Item *item)
  {
    value= item->val_int();
    set_null_value(item->null_value);
  }
  int cmp(Item *arg)
  {
    const bool rc= value != arg->val_int();
    return (m_null_value || arg->null_value) ? UNKNOWN : rc;
  }
  int compare(const cmp_item *ci) const
  {
    const cmp_item_int *l_cmp= down_cast<const cmp_item_int*>(ci);
    return (value < l_cmp->value) ? -1 : ((value == l_cmp->value) ? 0 : 1);
  }
  cmp_item *make_same();
};

/*
  Compare items of temporal type.
  Values are obtained with: get_datetime_value() (DATE/DATETIME/TIMESTAMP) and
                            get_time_value() (TIME).
  If the left item is a constant one then its value is cached in the
  lval_cache variable.
*/
class cmp_item_datetime : public cmp_item_scalar
{
  longlong value;
public:
  /* Item used for issuing warnings. */
  const Item *warn_item;
  /* Cache for the left item. */
  Item *lval_cache;
  /// Distinguish between DATE/DATETIME/TIMESTAMP and TIME
  bool has_date;

  cmp_item_datetime(const Item *warn_item_arg);
  void store_value(Item *item);
  int cmp(Item *arg);
  int compare(const cmp_item *ci) const;
  cmp_item *make_same();
};

class cmp_item_real : public cmp_item_scalar
{
  double value;
public:
  cmp_item_real() {}                          /* Remove gcc warning */
  void store_value(Item *item)
  {
    value= item->val_real();
    set_null_value(item->null_value);
  }
  int cmp(Item *arg)
  {
    const bool rc= value != arg->val_real();
    return (m_null_value || arg->null_value) ? UNKNOWN : rc;
  }
  int compare(const cmp_item *ci) const
  {
    const cmp_item_real *l_cmp= down_cast<const cmp_item_real*>(ci);
    return (value < l_cmp->value)? -1 : ((value == l_cmp->value) ? 0 : 1);
  }
  cmp_item *make_same();
};


class cmp_item_decimal : public cmp_item_scalar
{
  my_decimal value;
public:
  cmp_item_decimal() {}                       /* Remove gcc warning */
  void store_value(Item *item);
  int cmp(Item *arg);
  int compare(const cmp_item *c) const;
  cmp_item *make_same();
};


/*
  The class Item_func_case is the CASE ... WHEN ... THEN ... END function
  implementation.

  When there is no expression between CASE and the first WHEN 
  (the CASE expression) then this function simple checks all WHEN expressions
  one after another. When some WHEN expression evaluated to TRUE then the
  value of the corresponding THEN expression is returned.

  When the CASE expression is specified then it is compared to each WHEN
  expression individually. When an equal WHEN expression is found
  corresponding THEN expression is returned.
  In order to do correct comparisons several comparators are used. One for
  each result type. Different result types that are used in particular
  CASE ... END expression are collected in the fix_length_and_dec() member
  function and only comparators for there result types are used.
*/

class Item_func_case :public Item_func
{
  typedef Item_func super;

  int first_expr_num, else_expr_num;
  enum Item_result cached_result_type, left_result_type;
  String tmp_value;
  uint ncases;
  Item_result cmp_type;
  DTCollation cmp_collation;
  enum_field_types cached_field_type;
  cmp_item *cmp_items[5]; /* For all result types */
  cmp_item *case_item;
public:
  Item_func_case(const POS &pos, List<Item> &list, Item *first_expr_arg,
                 Item *else_expr_arg)
    : super(pos), first_expr_num(-1), else_expr_num(-1),
    cached_result_type(INT_RESULT), left_result_type(INT_RESULT), case_item(0)
  {
    ncases= list.elements;
    if (first_expr_arg)
    {
      first_expr_num= list.elements;
      list.push_back(first_expr_arg);
    }
    if (else_expr_arg)
    {
      else_expr_num= list.elements;
      list.push_back(else_expr_arg);
    }
    set_arguments(list, true);
    memset(&cmp_items, 0, sizeof(cmp_items));
  }
  double val_real();
  longlong val_int();
  String *val_str(String *);
  my_decimal *val_decimal(my_decimal *);
  bool val_json(Json_wrapper *wr);
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate);
  bool get_time(MYSQL_TIME *ltime);
  bool fix_fields(THD *thd, Item **ref);
  void fix_length_and_dec();
  uint decimal_precision() const;
  table_map not_null_tables() const { return 0; }
  enum Item_result result_type () const { return cached_result_type; }
  enum_field_types field_type() const { return cached_field_type; }
  const char *func_name() const { return "case"; }
  virtual void print(String *str, enum_query_type query_type);
  Item *find_item(String *str);
  const CHARSET_INFO *compare_collation() { return cmp_collation.collation; }
  void cleanup();
};

/*
  The Item_func_in class implements
  in_expr IN (<in value list>)
  and
  in_expr NOT IN (<in value list>)

  The current implementation distinguishes 2 cases:
  1) all items in <in value list> are constants and have the same
    result type. This case is handled by in_vector class.
  2) otherwise Item_func_in employs several cmp_item objects to perform
    comparisons of in_expr and an item from <in value list>. One cmp_item
    object for each result type. Different result types are collected in the
    fix_length_and_dec() member function by means of collect_cmp_types()
    function.
*/
class Item_func_in :public Item_func_opt_neg
{
public:
  /// An array of values, created when the bisection lookup method is used
  in_vector *array;
  /**
    If there is some NULL among <in value list>, during a val_int() call; for
    example
    IN ( (1,(3,'col')), ... ), where 'col' is a column which evaluates to
    NULL.
  */
  bool have_null;
  /**
    Set to true by fix_length_and_dec() if the IN list contains a
    dependent subquery, in which case condition filtering will not be
    calculated for this item.
  */
  bool dep_subq_in_list;
  Item_result left_result_type;
  cmp_item *cmp_items[6]; /* One cmp_item for each result type */
  DTCollation cmp_collation;

  Item_func_in(const POS &pos, PT_item_list *list, bool is_negation)
    :Item_func_opt_neg(pos, list, is_negation), array(NULL),
    have_null(false), dep_subq_in_list(false)
  {
    memset(&cmp_items, 0, sizeof(cmp_items));
    allowed_arg_cols= 0;  // Fetch this value from first argument
  }
  longlong val_int();
  bool fix_fields(THD *, Item **);
  void fix_after_pullout(st_select_lex *parent_select,
                         st_select_lex *removed_select);
  void fix_length_and_dec();
  uint decimal_precision() const { return 1; }

  /**
    Cleanup data and comparator arrays.

    @note Used during regular cleanup and to free arrays after GC substitution.
    @see substitute_gc().
  */
  void cleanup_arrays()
  {
    uint i;
    delete array;
    array= 0;
    for (i= 0; i <= (uint)DECIMAL_RESULT + 1; i++)
    {
      delete cmp_items[i];
      cmp_items[i]= 0;
    }
  }

  void cleanup()
  {
    DBUG_ENTER("Item_func_in::cleanup");
    Item_int_func::cleanup();
    cleanup_arrays();
    DBUG_VOID_RETURN;
  }
  optimize_type select_optimize() const
    { return OPTIMIZE_KEY; }
  virtual void print(String *str, enum_query_type query_type);
  enum Functype functype() const { return IN_FUNC; }
  const char *func_name() const { return " IN "; }
  bool is_bool_func() { return 1; }
  const CHARSET_INFO *compare_collation() { return cmp_collation.collation; }
  bool gc_subst_analyzer(uchar **arg) { return true; }

  float get_filtering_effect(table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table);
private:
  /**
     Usable if <in value list> is made only of constants. Returns true if one
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

class cmp_item_row :public cmp_item
{
  cmp_item **comparators;
  uint n;
public:
  cmp_item_row(): comparators(0), n(0) {}
  ~cmp_item_row();
  void store_value(Item *item);
  void alloc_comparators(Item *item);
  int cmp(Item *arg);
  int compare(const cmp_item *arg) const;
  cmp_item *make_same();
  void store_value_by_template(cmp_item *tmpl, Item *);
  friend void Item_func_in::fix_length_and_dec();
};


class in_row :public in_vector
{
  cmp_item_row tmp;
  // DTOR is not trivial, but we manage memory ourselves.
  Mem_root_array<cmp_item_row, true> base_objects;
  // Sort pointers, rather than objects.
  Mem_root_array<cmp_item_row*, true> base_pointers;
public:
  in_row(THD *thd, uint elements, Item *);
  ~in_row();
  void set(uint pos,Item *item);
  uchar *get_value(Item *item);
  friend void Item_func_in::fix_length_and_dec();
  Item_result result_type() { return ROW_RESULT; }

  virtual void shrink_array(size_t n) { base_pointers.resize(n); }

  virtual void sort();
  virtual bool find_value(const void *value) const;
  virtual bool compare_elems(uint pos1, uint pos2) const;
};

/* Functions used by where clause */

class Item_func_isnull :public Item_bool_func
{
protected:
  longlong cached_value;
public:
  Item_func_isnull(Item *a) :Item_bool_func(a) {}
  Item_func_isnull(const POS &pos, Item *a) :Item_bool_func(pos, a) {}

  longlong val_int();
  enum Functype functype() const { return ISNULL_FUNC; }
  void fix_length_and_dec()
  {
    decimals=0; max_length=1; maybe_null=0;
    update_used_tables();
  }
  const char *func_name() const { return "isnull"; }
  /* Optimize case of not_null_column IS NULL */
  virtual void update_used_tables()
  {
    if (!args[0]->maybe_null)
    {
      used_tables_cache= 0;			/* is always false */
      const_item_cache= 1;
      cached_value= (longlong) 0;
    }
    else
    {
      args[0]->update_used_tables();
      with_subselect= args[0]->has_subquery();
      with_stored_program= args[0]->has_stored_program();

      if ((const_item_cache= !(used_tables_cache= args[0]->used_tables()) &&
           !with_subselect && !with_stored_program))
      {
	/* Remember if the value is always NULL or never NULL */
	cached_value= (longlong) args[0]->is_null();
      }
    }
  }

  float get_filtering_effect(table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table);
  table_map not_null_tables() const { return 0; }
  optimize_type select_optimize() const { return OPTIMIZE_NULL; }
  Item *neg_transformer(THD *thd);
  const CHARSET_INFO *compare_collation()
  { return args[0]->collation.collation; }
};

/* Functions used by HAVING for rewriting IN subquery */

class Item_in_subselect;

/* 
  This is like IS NOT NULL but it also remembers if it ever has
  encountered a NULL; it remembers this in the "was_null" property of the
  "owner" item.
*/
class Item_is_not_null_test :public Item_func_isnull
{
  Item_in_subselect* owner;
public:
  Item_is_not_null_test(Item_in_subselect* ow, Item *a)
    :Item_func_isnull(a), owner(ow)
  {}
  enum Functype functype() const { return ISNOTNULLTEST_FUNC; }
  longlong val_int();
  const char *func_name() const { return "<is_not_null_test>"; }
  void update_used_tables();
  /**
    We add RAND_TABLE_BIT to prevent moving this item from HAVING to WHERE.
     
    @retval Always RAND_TABLE_BIT
  */
  table_map get_initial_pseudo_tables() const { return RAND_TABLE_BIT; }
};


class Item_func_isnotnull :public Item_bool_func
{
  bool abort_on_null;
public:
  Item_func_isnotnull(Item *a) :Item_bool_func(a), abort_on_null(0) {}
  Item_func_isnotnull(const POS &pos, Item *a)
    : Item_bool_func(pos, a), abort_on_null(0)
  {}

  longlong val_int();
  enum Functype functype() const { return ISNOTNULL_FUNC; }
  void fix_length_and_dec()
  {
    decimals=0; max_length=1; maybe_null=0;
  }
  const char *func_name() const { return "isnotnull"; }
  optimize_type select_optimize() const { return OPTIMIZE_NULL; }
  table_map not_null_tables() const
  { return abort_on_null ? not_null_tables_cache : 0; }
  Item *neg_transformer(THD *thd);
  virtual void print(String *str, enum_query_type query_type);
  const CHARSET_INFO *compare_collation()
  { return args[0]->collation.collation; }
  void top_level_item() { abort_on_null=1; }

  float get_filtering_effect(table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table);
};


class Item_func_like :public Item_bool_func2
{
  typedef Item_bool_func2 super;

  // Boyer-Moore data
  bool        can_do_bm;	// pattern is '%abcd%' case
  const char* pattern;
  int         pattern_len;

  // Boyer-Moore buffers, *this is owner
  int* bmGs; //   good suffix shift table, size is pattern_len + 1
  int* bmBc; // bad character shift table, size is alphabet_size

  void bm_compute_suffixes(int* suff);
  void bm_compute_good_suffix_shifts(int* suff);
  void bm_compute_bad_character_shifts();
  bool bm_matches(const char* text, size_t text_len) const;
  enum { alphabet_size = 256 };

  Item *escape_item;
  
  bool escape_used_in_parsing;

  bool escape_evaluated;  ///< Tells if the escape clause has been evaluated.
  bool eval_escape_clause(THD *thd);

public:
  int escape;

  Item_func_like(Item *a,Item *b, Item *escape_arg, bool escape_used)
    :Item_bool_func2(a,b), can_do_bm(false), pattern(0), pattern_len(0), 
     bmGs(0), bmBc(0), escape_item(escape_arg),
     escape_used_in_parsing(escape_used), escape_evaluated(false) {}
  Item_func_like(const POS &pos, Item *a, Item *b, Item *opt_escape_arg)
    :super(pos, a, b), can_do_bm(false), pattern(0), pattern_len(0), 
     bmGs(0), bmBc(0), escape_item(opt_escape_arg),
     escape_used_in_parsing(opt_escape_arg != NULL), escape_evaluated(false)
  {}

  virtual bool itemize(Parse_context *pc, Item **res);

  longlong val_int();
  enum Functype functype() const { return LIKE_FUNC; }
  optimize_type select_optimize() const;
  cond_result eq_cmp_result() const { return COND_TRUE; }
  const char *func_name() const { return "like"; }
  bool fix_fields(THD *thd, Item **ref);
  void cleanup();
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

  float get_filtering_effect(table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table);
};


class Item_func_regex :public Item_bool_func
{
  my_regex_t preg;
  bool regex_compiled;
  bool regex_is_const;
  String prev_regexp;
  DTCollation cmp_collation;
  const CHARSET_INFO *regex_lib_charset;
  int regex_lib_flags;
  String conv;
  int regcomp(bool send_error);
public:
  Item_func_regex(const POS &pos, Item *a,Item *b) :Item_bool_func(pos, a,b),
    regex_compiled(0),regex_is_const(0) {}
  void cleanup();
  longlong val_int();
  bool fix_fields(THD *thd, Item **ref);
  const char *func_name() const { return "regexp"; }

  virtual inline void print(String *str, enum_query_type query_type)
  {
    print_op(str, query_type);
  }

  const CHARSET_INFO *compare_collation() { return cmp_collation.collation; }
};


class Item_cond :public Item_bool_func
{
  typedef Item_bool_func super;

protected:
  List<Item> list;
  bool abort_on_null;

public:
  /* Item_cond() is only used to create top level items */
  Item_cond(): Item_bool_func(), abort_on_null(1)
  { const_item_cache=0; }

  Item_cond(Item *i1,Item *i2)
    :Item_bool_func(), abort_on_null(0)
  {
    list.push_back(i1);
    list.push_back(i2);
  }
  Item_cond(const POS &pos, Item *i1, Item *i2)
    :Item_bool_func(pos), abort_on_null(0)
  {
    list.push_back(i1);
    list.push_back(i2);
  }

  Item_cond(THD *thd, Item_cond *item);
  Item_cond(List<Item> &nlist)
    :Item_bool_func(), list(nlist), abort_on_null(0) {}
  bool add(Item *item)
  {
    DBUG_ASSERT(item);
    return list.push_back(item);
  }
  bool add_at_head(Item *item)
  {
    DBUG_ASSERT(item);
    return list.push_front(item);
  }
  void add_at_head(List<Item> *nlist)
  {
    DBUG_ASSERT(nlist->elements);
    list.prepand(nlist);
  }

  virtual bool itemize(Parse_context *pc, Item **res);

  bool fix_fields(THD *, Item **ref);
  void fix_after_pullout(st_select_lex *parent_select,
                         st_select_lex *removed_select);

  enum Type type() const { return COND_ITEM; }
  List<Item>* argument_list() { return &list; }
  bool eq(const Item *item, bool binary_cmp) const;
  table_map used_tables() const { return used_tables_cache; }
  void update_used_tables();
  virtual void print(String *str, enum_query_type query_type);
  void split_sum_func(THD *thd, Ref_ptr_array ref_pointer_array,
                      List<Item> &fields);
  void top_level_item() { abort_on_null=1; }
  void copy_andor_arguments(THD *thd, Item_cond *item);
  bool walk(Item_processor processor, enum_walk walk, uchar *arg);
  Item *transform(Item_transformer transformer, uchar *arg);
  void traverse_cond(Cond_traverser, void *arg, traverse_order order);
  void neg_arguments(THD *thd);
  enum_field_types field_type() const { return MYSQL_TYPE_LONGLONG; }
  bool subst_argument_checker(uchar **arg) { return TRUE; }
  Item *compile(Item_analyzer analyzer, uchar **arg_p,
                Item_transformer transformer, uchar *arg_t);

  virtual bool equality_substitution_analyzer(uchar **arg) { return true; }
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
struct st_join_table;

class Item_equal: public Item_bool_func
{
  List<Item_field> fields; /* list of equal field items                    */
  Item *const_item;        /* optional constant item equal to fields items */
  cmp_item *eval_item;
  Arg_comparator cmp;
  bool cond_false;
  bool compare_as_dates;
public:
  inline Item_equal()
    : Item_bool_func(), const_item(0), eval_item(0), cond_false(0)
  { const_item_cache=0 ;}
  Item_equal(Item_field *f1, Item_field *f2);
  Item_equal(Item *c, Item_field *f);
  Item_equal(Item_equal *item_equal);
  virtual ~Item_equal()
  {
    delete eval_item;
  }

  inline Item* get_const() { return const_item; }
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
  Item_field* get_first() { return fields.head(); }
  Item_field* get_subst_item(const Item_field *field);
  bool merge(THD *thd, Item_equal *item);
  bool update_const(THD *thd);
  enum Functype functype() const { return MULT_EQUAL_FUNC; }
  longlong val_int(); 
  const char *func_name() const { return "multiple equal"; }
  optimize_type select_optimize() const { return OPTIMIZE_EQUAL; }
  void sort(Item_field_cmpfunc compare, void *arg);
  friend class Item_equal_iterator;
  void fix_length_and_dec();
  bool fix_fields(THD *thd, Item **ref);
  void update_used_tables();
  bool walk(Item_processor processor, enum_walk walk, uchar *arg);
  Item *transform(Item_transformer transformer, uchar *arg);
  virtual void print(String *str, enum_query_type query_type);
  const CHARSET_INFO *compare_collation() 
  { return fields.head()->collation.collation; }

  virtual bool equality_substitution_analyzer(uchar **arg) { return true; }

  virtual Item* equality_substitution_transformer(uchar *arg);

  float get_filtering_effect(table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table);
}; 

class COND_EQUAL: public Sql_alloc
{
public:
  uint max_members;               /* max number of members the current level
                                     list and all lower level lists */ 
  COND_EQUAL *upper_levels;       /* multiple equalities of upper and levels */
  List<Item_equal> current_level; /* list of multiple equalities of 
                                     the current and level           */
  COND_EQUAL()
  { 
    upper_levels= 0;
  }
};


class Item_equal_iterator : public List_iterator_fast<Item_field>
{
public:
  inline Item_equal_iterator(Item_equal &item_equal) 
    :List_iterator_fast<Item_field> (item_equal.fields)
  {}
  inline Item_field* operator++(int)
  { 
    Item_field *item= (*(List_iterator_fast<Item_field> *) this)++;
    return  item;
  }
  inline void rewind(void) 
  { 
    List_iterator_fast<Item_field>::rewind();
  }
};

class Item_cond_and :public Item_cond
{
public:
  COND_EQUAL cond_equal;  /* contains list of Item_equal objects for 
                             the current and level and reference
                             to multiple equalities of upper and levels */  
  Item_cond_and() :Item_cond() {}

  Item_cond_and(Item *i1,Item *i2) :Item_cond(i1,i2) {}
  Item_cond_and(const POS &pos, Item *i1, Item *i2) :Item_cond(pos, i1, i2) {}

  Item_cond_and(THD *thd, Item_cond_and *item) :Item_cond(thd, item) {}
  Item_cond_and(List<Item> &list_arg): Item_cond(list_arg) {}
  enum Functype functype() const { return COND_AND_FUNC; }
  longlong val_int();
  const char *func_name() const { return "and"; }
  Item* copy_andor_structure(THD *thd)
  {
    Item_cond_and *item;
    if ((item= new Item_cond_and(thd, this)))
      item->copy_andor_arguments(thd, this);
    return item;
  }
  Item *neg_transformer(THD *thd);
  bool gc_subst_analyzer(uchar **arg) { return true; }

  float get_filtering_effect(table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table);
};


class Item_cond_or :public Item_cond
{
public:
  Item_cond_or() :Item_cond() {}

  Item_cond_or(Item *i1,Item *i2) :Item_cond(i1,i2) {}
  Item_cond_or(const POS &pos, Item *i1,Item *i2) :Item_cond(pos, i1, i2) {}

  Item_cond_or(THD *thd, Item_cond_or *item) :Item_cond(thd, item) {}
  Item_cond_or(List<Item> &list_arg): Item_cond(list_arg) {}
  enum Functype functype() const { return COND_OR_FUNC; }
  longlong val_int();
  const char *func_name() const { return "or"; }
  Item* copy_andor_structure(THD *thd)
  {
    Item_cond_or *item;
    if ((item= new Item_cond_or(thd, this)))
      item->copy_andor_arguments(thd, this);
    return item;
  }
  Item *neg_transformer(THD *thd);
  bool gc_subst_analyzer(uchar **arg) { return true; }

  float get_filtering_effect(table_map filter_for_table,
                             table_map read_tables,
                             const MY_BITMAP *fields_to_ignore,
                             double rows_in_table);
};

/* Some useful inline functions */

inline Item *and_conds(Item *a, Item *b)
{
  if (!b) return a;
  if (!a) return b;
  return new Item_cond_and(a, b);
}


Item *and_expressions(Item *a, Item *b, Item **org_item);

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
