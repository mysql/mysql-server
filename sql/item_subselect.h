#ifndef ITEM_SUBSELECT_INCLUDED
#define ITEM_SUBSELECT_INCLUDED

/* Copyright (c) 2002, 2018, Oracle and/or its affiliates. All rights reserved.

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

/* subselect Item */

#include <stddef.h>
#include <sys/types.h>

#include "binary_log_types.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_table_map.h"
#include "my_time.h"
#include "mysql/udf_registration_types.h"
#include "mysql_time.h"
#include "sql/enum_query_type.h"
#include "sql/item.h"  // Item_result_field
#include "sql/parse_tree_node_base.h"

class Comp_creator;
class Field;
class Item_func_not_all;
class Item_in_optimizer;
class JOIN;
class Json_wrapper;
class PT_subquery;
class QEP_TAB;
class Query_result_interceptor;
class Query_result_subquery;
class SELECT_LEX;
class SELECT_LEX_UNIT;
class String;
class THD;
class Temp_table_param;
class my_decimal;
class subselect_engine;
struct TABLE_LIST;
template <class T>
class List;

/**
  Convenience typedef used in this file, and further used by any files
  including this file.

  @retval NULL In case of semantic errors.
*/
typedef Comp_creator *(*chooser_compare_func_creator)(bool invert);

/* base class for subselects */

class Item_subselect : public Item_result_field {
  typedef Item_result_field super;

 private:
  bool value_assigned; /* value already assigned to subselect */
  /**
      Whether or not execution of this subselect has been traced by
      optimizer tracing already. If optimizer trace option
      REPEATED_SUBSELECT is disabled, this is used to disable tracing
      after the first one.
  */
  bool traced_before;

 public:
  /*
    Used inside Item_subselect::fix_fields() according to this scenario:
      > Item_subselect::fix_fields
        > engine->prepare
          > query_block->prepare
            (Here we realize we need to do the rewrite and set
             substitution= some new Item, eg. Item_in_optimizer )
          < query_block->prepare
        < engine->prepare
        *ref= substitution;
      < Item_subselect::fix_fields
  */
  Item *substitution;

 public:
  /* unit of subquery */
  SELECT_LEX_UNIT *unit;
  /**
     If !=NO_PLAN_IDX: this Item is in the condition attached to the JOIN_TAB
     having this index in the parent JOIN.
  */
  int in_cond_of_tab;

  /// EXPLAIN needs read-only access to the engine
  const subselect_engine *get_engine_for_explain() const { return engine; }

 protected:
  /* engine that perform execution of subselect (single select or union) */
  subselect_engine *engine;
  /* old engine if engine was changed */
  subselect_engine *old_engine;
  /* cache of used external tables */
  table_map used_tables_cache;
  /* allowed number of columns (1 for single value subqueries) */
  uint max_columns;
  /* where subquery is placed */
  enum_parsing_context parsing_place;
  /* work with 'substitution' */
  bool have_to_be_excluded;

 public:
  /* subquery is transformed */
  bool changed;

  enum trans_res { RES_OK, RES_REDUCE, RES_ERROR };
  enum subs_type {
    UNKNOWN_SUBS,
    SINGLEROW_SUBS,
    EXISTS_SUBS,
    IN_SUBS,
    ALL_SUBS,
    ANY_SUBS
  };

  Item_subselect();
  explicit Item_subselect(const POS &pos);

 private:
  /// Accumulate properties from underlying query expression
  void accumulate_properties();
  /// Accumulate properties from underlying query block
  void accumulate_properties(SELECT_LEX *select);
  /// Accumulate properties from a selected expression within a query block.
  void accumulate_expression(Item *item);
  /// Accumulate properties from a condition or GROUP/ORDER within a query
  /// block.
  void accumulate_condition(Item *item);
  /// Accumulate properties from a join condition within a query block.
  void accumulate_join_condition(List<TABLE_LIST> *tables);

 public:
  /// Accumulate used tables
  void accumulate_used_tables(table_map add_tables) {
    used_tables_cache |= add_tables;
  }

  virtual subs_type substype() { return UNKNOWN_SUBS; }

  /*
    We need this method, because some compilers do not allow 'this'
    pointer in constructor initialization list, but we need to pass a pointer
    to subselect Item class to Query_result_interceptor's constructor.
  */
  void init(SELECT_LEX *select, Query_result_subquery *result);

  ~Item_subselect();
  void cleanup() override;
  virtual void reset() { null_value = 1; }
  virtual trans_res select_transformer(SELECT_LEX *select) = 0;
  bool assigned() const { return value_assigned; }
  void assigned(bool a) { value_assigned = a; }
  enum Type type() const override;
  bool is_null() override {
    /*
      TODO : Implement error handling for this function as
      update_null_value() can return error.
    */
    (void)update_null_value();
    return null_value;
  }
  bool fix_fields(THD *thd, Item **ref) override;
  void fix_after_pullout(SELECT_LEX *parent_select,
                         SELECT_LEX *removed_select) override;
  virtual bool exec();
  bool resolve_type(THD *) override;
  table_map used_tables() const override { return used_tables_cache; }
  table_map not_null_tables() const override { return 0; }
  Item *get_tmp_table_item(THD *thd) override;
  void update_used_tables() override;
  void print(String *str, enum_query_type query_type) override;
  virtual bool have_guarded_conds() { return false; }
  bool change_engine(subselect_engine *eng) {
    old_engine = engine;
    engine = eng;
    return eng == 0;
  }

  /*
    True if this subquery has been already evaluated. Implemented only for
    single select and union subqueries only.
  */
  bool is_evaluated() const;
  bool is_uncacheable() const;

  /*
    Used by max/min subquery to initialize value presence registration
    mechanism. Engine call this method before rexecution query.
  */
  virtual void reset_value_registration() {}
  enum_parsing_context place() { return parsing_place; }
  bool walk_body(Item_processor processor, enum_walk walk, uchar *arg);
  bool walk(Item_processor processor, enum_walk walk, uchar *arg) override;
  bool explain_subquery_checker(uchar **arg) override;
  bool inform_item_in_cond_of_tab(uchar *arg) override;
  bool clean_up_after_removal(uchar *arg) override;

  const char *func_name() const override {
    DBUG_ASSERT(0);
    return "subselect";
  }

  bool check_function_as_value_generator(uchar *args) override {
    Check_function_as_value_generator_parameters *func_arg =
        pointer_cast<Check_function_as_value_generator_parameters *>(args);
    func_arg->err_code = func_arg->get_unnamed_function_error_code();
    return true;
  }

  friend class Query_result_interceptor;
  friend class Item_in_optimizer;
  friend bool Item_field::fix_fields(THD *, Item **);
  friend int Item_field::fix_outer_field(THD *, Field **, Item **);
  friend bool Item_ref::fix_fields(THD *, Item **);
  friend void Item_ident::fix_after_pullout(SELECT_LEX *parent_select,
                                            SELECT_LEX *removed_selec);
  friend void mark_select_range_as_dependent(THD *thd, SELECT_LEX *last_select,
                                             SELECT_LEX *current_sel,
                                             Field *found_field,
                                             Item *found_item,
                                             Item_ident *resolved_item);

 private:
  bool subq_opt_away_processor(uchar *arg) override;
};

/* single value subselect */

class Item_singlerow_subselect : public Item_subselect {
 protected:
  Item_cache *value, **row;
  bool no_rows;  ///< @c no_rows_in_result
 public:
  Item_singlerow_subselect(SELECT_LEX *select_lex);
  Item_singlerow_subselect()
      : Item_subselect(), value(0), row(0), no_rows(false) {}

  void cleanup() override;
  subs_type substype() override { return SINGLEROW_SUBS; }

  void reset() override;
  trans_res select_transformer(SELECT_LEX *select) override;
  void store(uint i, Item *item);
  double val_real() override;
  longlong val_int() override;
  String *val_str(String *) override;
  my_decimal *val_decimal(my_decimal *) override;
  bool val_json(Json_wrapper *result) override;
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) override;
  bool get_time(MYSQL_TIME *ltime) override;
  bool val_bool() override;
  enum Item_result result_type() const override;
  bool resolve_type(THD *) override;

  /*
    Mark the subquery as having no rows.
    If there are aggregate functions (in the outer query),
    we need to generate a NULL row. @c return_zero_rows().
  */
  void no_rows_in_result() override;

  uint cols() const override;
  /**
    @note that this returns the i-th element of the SELECT list.
    To check for nullability, look at this->maybe_null and not
    element_index[i]->maybe_null, since the selected expressions are
    always NULL if the subquery is empty.
  */
  Item *element_index(uint i) override {
    return reinterpret_cast<Item *>(row[i]);
  }
  Item **addr(uint i) override { return (Item **)row + i; }
  bool check_cols(uint c) override;
  bool null_inside() override;
  void bring_value() override;

  /**
    This method is used to implement a special case of semantic tree
    rewriting, mandated by a SQL:2003 exception in the specification.
    The only caller of this method is handle_sql2003_note184_exception(),
    see the code there for more details.
    Note that this method breaks the object internal integrity, by
    removing it's association with the corresponding SELECT_LEX,
    making this object orphan from the parse tree.
    No other method, beside the destructor, should be called on this
    object, as it is now invalid.
    @return the SELECT_LEX structure that was given in the constructor.
  */
  SELECT_LEX *invalidate_and_restore_select_lex();

  friend class Query_result_scalar_subquery;
};

/* used in static ALL/ANY optimization */
class Item_maxmin_subselect final : public Item_singlerow_subselect {
 protected:
  bool max;
  bool was_values;  // Set if we have found at least one row
 public:
  Item_maxmin_subselect(THD *thd, Item_subselect *parent,
                        SELECT_LEX *select_lex, bool max, bool ignore_nulls);
  void print(String *str, enum_query_type query_type) override;
  void cleanup() override;
  bool any_value() { return was_values; }
  void register_value() { was_values = true; }
  void reset_value_registration() override { was_values = false; }
};

/* exists subselect */

class Item_exists_subselect : public Item_subselect {
  typedef Item_subselect super;

 protected:
  bool value; /* value of this item (boolean: exists/not-exists) */

 public:
  /**
    The method chosen to execute the predicate, currently used for IN, =ANY
    and EXISTS predicates.
  */
  enum enum_exec_method {
    EXEC_UNSPECIFIED,  ///< No execution method specified yet.
    EXEC_SEMI_JOIN,    ///< Predicate is converted to semi-join nest.
    /// IN was converted to correlated EXISTS, and this is a final decision.
    EXEC_EXISTS,
    /**
       Decision between EXEC_EXISTS and EXEC_MATERIALIZATION is not yet taken.
       IN was temporarily converted to correlated EXISTS.
       All descendants of Item_in_subselect must go through this method
       before they can reach EXEC_EXISTS.
    */
    EXEC_EXISTS_OR_MAT,
    /// Predicate executed via materialization, and this is a final decision.
    EXEC_MATERIALIZATION
  };
  enum_exec_method exec_method;
  /// Priority of this predicate in the convert-to-semi-join-nest process.
  int sj_convert_priority;
  /// True if this predicate is chosen for semi-join transformation
  bool sj_chosen;
  /**
    Used by subquery optimizations to keep track about where this subquery
    predicate is located, and whether it is a candidate for transformation.
      (TABLE_LIST*) 1   - the predicate is an AND-part of the WHERE
      join nest pointer - the predicate is an AND-part of ON expression
                          of a join nest
      NULL              - for all other locations. It also means that the
                          predicate is not a candidate for transformation.
    See also THD::emb_on_expr_nest.
  */
  TABLE_LIST *embedding_join_nest;

  Item_exists_subselect(SELECT_LEX *select);

  Item_exists_subselect()
      : Item_subselect(),
        value(false),
        exec_method(EXEC_UNSPECIFIED),
        sj_convert_priority(0),
        sj_chosen(false),
        embedding_join_nest(NULL) {}

  explicit Item_exists_subselect(const POS &pos)
      : super(pos),
        value(false),
        exec_method(EXEC_UNSPECIFIED),
        sj_convert_priority(0),
        sj_chosen(false),
        embedding_join_nest(NULL) {}

  trans_res select_transformer(SELECT_LEX *) override {
    exec_method = EXEC_EXISTS;
    return RES_OK;
  }
  subs_type substype() override { return EXISTS_SUBS; }
  void reset() override { value = 0; }

  enum Item_result result_type() const override { return INT_RESULT; }
  longlong val_int() override;
  double val_real() override;
  String *val_str(String *) override;
  my_decimal *val_decimal(my_decimal *) override;
  bool val_bool() override;
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) override {
    return get_date_from_int(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime) override { return get_time_from_int(ltime); }
  bool resolve_type(THD *thd) override;
  void print(String *str, enum_query_type query_type) override;

  friend class Query_result_exists_subquery;
  friend class subselect_indexsubquery_engine;
};

/**
  Representation of IN subquery predicates of the form
  "left_expr IN (SELECT ...)".

  @details
  This class has:
   - A "subquery execution engine" (as a subclass of Item_subselect) that allows
     it to evaluate subqueries. (and this class participates in execution by
     having was_null variable where part of execution result is stored.
   - Transformation methods (todo: more on this).

  This class is not used directly, it is "wrapped" into Item_in_optimizer
  which provides some small bits of subquery evaluation.
*/

class Item_in_subselect : public Item_exists_subselect {
  typedef Item_exists_subselect super;

 public:
  Item *left_expr;

 protected:
  /**
    Cache of the left operand of the subquery predicate. Allocated in the
    runtime memory root, for each execution, thus need not be freed.
  */
  List<Cached_item> *left_expr_cache;
  bool left_expr_cache_filled;  ///< Whether left_expr_cache holds a value
  /** The need for expr cache may be optimized away, @sa init_left_expr_cache.
   */
  bool need_expr_cache;

 private:
  /**
    In the case of

       x COMP_OP (SELECT1 UNION SELECT2 ...)

    - the subquery transformation is done on SELECT1; this requires wrapping
      'x' with more Item layers, and injecting that in a condition in SELECT1.

    - the same transformation is done on SELECT2; but the wrapped 'x' doesn't
      need to be created again, the one created for SELECT1 could be reused

    - to achieve this, the wrapped 'x' is stored in member
      'm_injected_left_expr' when it is created for SELECT1, and is later
      reused for SELECT2.

    This will refer to a cached value which is reevaluated once for each
    candidate row, cf. setup in #single_value_transformer.
  */
  Item_ref *m_injected_left_expr;

  /**
    Pointer to the created Item_in_optimizer; it is stored for the same
    reasons as 'm_injected_left_expr'.
  */
  Item_in_optimizer *optimizer;
  bool was_null;

 protected:
  bool abort_on_null;

 private:
  /**
     This bundles several pieces of information useful when doing the
     IN->EXISTS transform. If this transform has not been done, pointer is
     NULL.
  */
  struct In2exists_info {
    /**
       True: if IN->EXISTS has been done and has added a condition to the
       subquery's WHERE clause.
    */
    bool added_to_where;
    /**
       True: if subquery was dependent (correlated) before IN->EXISTS
       was done.
    */
    bool dependent_before;
    /**
       True: if subquery was dependent (correlated) after IN->EXISTS
       was done.
    */
    bool dependent_after;
  } * in2exists_info;

  Item *remove_in2exists_conds(Item *conds);
  bool mark_as_outer(Item *left_row, size_t col);

 public:
  /* Used to trigger on/off conditions that were pushed down to subselect */
  bool *pushed_cond_guards;

  Item_func_not_all *upper_item;  // point on NOT/NOP before ALL/SOME subquery

 private:
  PT_subquery *pt_subselect;

 public:
  bool in2exists_added_to_where() const {
    return in2exists_info && in2exists_info->added_to_where;
  }

  /// Is reliable only if IN->EXISTS has been done.
  bool dependent_before_in2exists() const {
    return in2exists_info->dependent_before;
  }

  bool *get_cond_guard(int i) {
    return pushed_cond_guards ? pushed_cond_guards + i : NULL;
  }
  void set_cond_guard_var(int i, bool v) {
    if (pushed_cond_guards) pushed_cond_guards[i] = v;
  }
  bool have_guarded_conds() override { return pushed_cond_guards != nullptr; }

  Item_in_subselect(Item *left_expr, SELECT_LEX *select_lex);
  Item_in_subselect(const POS &pos, Item *left_expr,
                    PT_subquery *pt_subquery_arg);

  Item_in_subselect()
      : Item_exists_subselect(),
        left_expr(NULL),
        left_expr_cache(NULL),
        left_expr_cache_filled(false),
        need_expr_cache(true),
        m_injected_left_expr(NULL),
        optimizer(NULL),
        was_null(false),
        abort_on_null(false),
        in2exists_info(NULL),
        pushed_cond_guards(NULL),
        upper_item(NULL) {}

  bool itemize(Parse_context *pc, Item **res) override;

  void cleanup() override;
  subs_type substype() override { return IN_SUBS; }
  void reset() override {
    value = 0;
    null_value = 0;
    was_null = 0;
  }
  trans_res select_transformer(SELECT_LEX *select) override;
  trans_res select_in_like_transformer(SELECT_LEX *select, Comp_creator *func);
  trans_res single_value_transformer(SELECT_LEX *select, Comp_creator *func);
  trans_res row_value_transformer(SELECT_LEX *select);
  trans_res single_value_in_to_exists_transformer(SELECT_LEX *select,
                                                  Comp_creator *func);
  trans_res row_value_in_to_exists_transformer(SELECT_LEX *select);
  bool walk(Item_processor processor, enum_walk walk, uchar *arg) override;
  bool exec() override;
  longlong val_int() override;
  double val_real() override;
  String *val_str(String *) override;
  my_decimal *val_decimal(my_decimal *) override;
  bool val_bool() override;
  void top_level_item() override { abort_on_null = 1; }
  bool is_top_level_item() const { return abort_on_null; }

  bool test_limit();
  void print(String *str, enum_query_type query_type) override;
  bool fix_fields(THD *thd, Item **ref) override;
  void fix_after_pullout(SELECT_LEX *parent_select,
                         SELECT_LEX *removed_select) override;
  bool init_left_expr_cache();

  /**
     Once the decision to use IN->EXISTS has been taken, performs some last
     steps of this transformation.
  */
  bool finalize_exists_transform(SELECT_LEX *select);
  /**
     Once the decision to use materialization has been taken, performs some
     last steps of this transformation.
  */
  bool finalize_materialization_transform(JOIN *join);

  friend class Item_ref_null_helper;
  friend class Item_is_not_null_test;
  friend class Item_in_optimizer;
  friend class subselect_indexsubquery_engine;
  friend class subselect_hash_sj_engine;
};

/// ALL/ANY/SOME subselect.
class Item_allany_subselect final : public Item_in_subselect {
 public:
  chooser_compare_func_creator func_creator;
  Comp_creator *func;
  bool all;

  Item_allany_subselect(Item *left_expr, chooser_compare_func_creator fc,
                        SELECT_LEX *select, bool all);

  // only ALL subquery has upper not
  subs_type substype() override { return all ? ALL_SUBS : ANY_SUBS; }
  trans_res select_transformer(SELECT_LEX *select) override;
  void print(String *str, enum_query_type query_type) override;
};

class subselect_engine {
 protected:
  Query_result_interceptor *result; /* results storage class */
  Item_subselect *item;             /* item, that use this engine */
  enum Item_result res_type;        /* type of results */
  enum_field_types res_field_type;  /* column type of the results */
  /**
    True if at least one of the columns returned by the subquery may
    be null, or if a single-row subquery may return zero rows.
  */
  bool maybe_null;

 public:
  enum enum_engine_type {
    ABSTRACT_ENGINE,
    SINGLE_SELECT_ENGINE,
    UNION_ENGINE,
    INDEXSUBQUERY_ENGINE,
    HASH_SJ_ENGINE
  };

  subselect_engine(Item_subselect *si, Query_result_interceptor *res)
      : result(res),
        item(si),
        res_type(STRING_RESULT),
        res_field_type(MYSQL_TYPE_VAR_STRING),
        maybe_null(false) {}
  virtual ~subselect_engine() {}  // to satisfy compiler
  /**
    Cleanup engine after complete query execution, free all resources.
  */
  virtual void cleanup() = 0;

  /// Sets "thd" for 'result'. Should be called before prepare()
  void set_thd_for_result();
  virtual bool prepare() = 0;
  virtual void fix_length_and_dec(Item_cache **row) = 0;
  /*
    Execute the engine

    SYNOPSIS
      exec()

    DESCRIPTION
      Execute the engine. The result of execution is subquery value that is
      either captured by previously set up Query_result-based 'sink' or
      stored somewhere by the exec() method itself.

    RETURN
      0 - OK
      1 - Either an execution error, or the engine was "changed", and the
          caller should call exec() again for the new engine.
  */
  virtual bool exec() = 0;
  virtual uint cols() const = 0; /* return number of columns in select */
  virtual uint8 uncacheable() const = 0; /* query is uncacheable */
  virtual enum Item_result type() const { return res_type; }
  virtual enum_field_types field_type() const { return res_field_type; }
  virtual void exclude() = 0;
  bool may_be_null() const { return maybe_null; }
  virtual table_map upper_select_const_tables() const = 0;
  static table_map calc_const_tables(TABLE_LIST *);
  virtual void print(String *str, enum_query_type query_type) = 0;
  virtual bool change_query_result(Item_subselect *si,
                                   Query_result_subquery *result) = 0;
  virtual enum_engine_type engine_type() const { return ABSTRACT_ENGINE; }
#ifndef DBUG_OFF
  /**
     @returns the internal Item. Defined only in debug builds, because should
     be used only for debug asserts.
  */
  const Item_subselect *get_item() const { return item; }
#endif

 protected:
  void set_row(List<Item> &item_list, Item_cache **row, bool never_empty);
};

class subselect_single_select_engine final : public subselect_engine {
 private:
  SELECT_LEX *select_lex; /* corresponding select_lex */
 public:
  subselect_single_select_engine(SELECT_LEX *select,
                                 Query_result_interceptor *result,
                                 Item_subselect *item);
  void cleanup() override;
  bool prepare() override;
  void fix_length_and_dec(Item_cache **row) override;
  bool exec() override;
  uint cols() const override;
  uint8 uncacheable() const override;
  void exclude() override;
  table_map upper_select_const_tables() const override;
  void print(String *str, enum_query_type query_type) override;
  bool change_query_result(Item_subselect *si,
                           Query_result_subquery *result) override;
  enum_engine_type engine_type() const override { return SINGLE_SELECT_ENGINE; }

  friend class subselect_hash_sj_engine;
  friend class Item_in_subselect;
};

class subselect_union_engine final : public subselect_engine {
 public:
  subselect_union_engine(SELECT_LEX_UNIT *u, Query_result_interceptor *result,
                         Item_subselect *item);
  void cleanup() override;
  bool prepare() override;
  void fix_length_and_dec(Item_cache **row) override;
  bool exec() override;
  uint cols() const override;
  uint8 uncacheable() const override;
  void exclude() override;
  table_map upper_select_const_tables() const override;
  void print(String *str, enum_query_type query_type) override;
  bool change_query_result(Item_subselect *si,
                           Query_result_subquery *result) override;
  enum_engine_type engine_type() const override { return UNION_ENGINE; }

 private:
  SELECT_LEX_UNIT *unit; /* corresponding unit structure */
};

/**
  A subquery execution engine that evaluates the subquery by doing index
  lookups in a single table's index.

  This engine is used to resolve subqueries in forms

    outer_expr IN (SELECT tbl.key FROM tbl WHERE subq_where)

  or, row-based:

    (oe1, .. oeN) IN (SELECT key_part1, ... key_partK
                      FROM tbl WHERE subqwhere)

  i.e. the subquery is a single table SELECT without GROUP BY, aggregate
  functions, etc.
*/
class subselect_indexsubquery_engine : public subselect_engine {
 protected:
  /// Table which is read, using one of eq_ref, ref, ref_or_null.
  QEP_TAB *tab;
  Item *cond;     /* The WHERE condition of subselect */
  ulonglong hash; /* Hash value calculated by copy_ref_key, when needed. */
 private:
  /*
    The "having" clause. This clause (further referred to as "artificial
    having") was inserted by subquery transformation code. It contains
    Item(s) that have a side-effect: they record whether the subquery has
    produced a row with NULL certain components. We need to use it for cases
    like
      (oe1, oe2) IN (SELECT t.key, t.no_key FROM t1)
    where we do index lookup on t.key=oe1 but need also to check if there
    was a row such that t.no_key IS NULL.
  */
  Item *having;

 public:
  subselect_indexsubquery_engine(QEP_TAB *tab_arg, Item_subselect *subs,
                                 Item *where, Item *having_arg)
      : subselect_engine(subs, 0),
        tab(tab_arg),
        cond(where),
        having(having_arg) {}
  bool exec() override;
  void print(String *str, enum_query_type query_type) override;
  enum_engine_type engine_type() const override { return INDEXSUBQUERY_ENGINE; }
  void cleanup() override {}
  bool prepare() override;
  void fix_length_and_dec(Item_cache **row) override;
  uint cols() const override { return 1; }
  uint8 uncacheable() const override { return UNCACHEABLE_DEPENDENT; }
  void exclude() override;
  table_map upper_select_const_tables() const override { return 0; }
  bool change_query_result(Item_subselect *si,
                           Query_result_subquery *result) override;
  bool scan_table();
  void copy_ref_key(bool *require_scan, bool *convert_error);
};

/*
  This function is actually defined in sql_parse.cc, but it depends on
  chooser_compare_func_creator defined in this file.
 */
Item *all_any_subquery_creator(Item *left_expr,
                               chooser_compare_func_creator cmp, bool all,
                               SELECT_LEX *select);

inline bool Item_subselect::is_uncacheable() const {
  return engine->uncacheable();
}

/**
  Compute an IN predicate via a hash semi-join. The subquery is materialized
  during the first evaluation of the IN predicate. The IN predicate is executed
  via the functionality inherited from subselect_indexsubquery_engine.
*/

class subselect_hash_sj_engine final : public subselect_indexsubquery_engine {
 private:
  /* TRUE if the subquery was materialized into a temp table. */
  bool is_materialized;
  /**
     Existence of inner NULLs in materialized table:
     By design, other values than IRRELEVANT_OR_FALSE are possible only if the
     subquery has only one inner expression.
  */
  enum nulls_exist {
    /// none, or they don't matter
    NEX_IRRELEVANT_OR_FALSE = 0,
    /// they matter, and we don't know yet if they exists
    NEX_UNKNOWN = 1,
    /// they matter, and we know there exists at least one.
    NEX_TRUE = 2
  };
  enum nulls_exist mat_table_has_nulls;
  /*
    The old engine already chosen at parse time and stored in permanent memory.
    Through this member we can re-create and re-prepare the join object
    used to materialize the subquery for each execution of a prepared
    statement. We also reuse the functionality of
    subselect_single_select_engine::[prepare | cols].
  */
  subselect_single_select_engine *materialize_engine;
  /* Temp table context of the outer select's JOIN. */
  Temp_table_param *tmp_param;

 public:
  subselect_hash_sj_engine(Item_subselect *in_predicate,
                           subselect_single_select_engine *old_engine)
      : subselect_indexsubquery_engine(NULL, in_predicate, NULL, NULL),
        is_materialized(false),
        materialize_engine(old_engine),
        tmp_param(NULL) {}
  ~subselect_hash_sj_engine();

  bool setup(List<Item> *tmp_columns);
  void cleanup() override;
  bool prepare() override { return materialize_engine->prepare(); }
  bool exec() override;
  void print(String *str, enum_query_type query_type) override;
  uint cols() const override { return materialize_engine->cols(); }
  enum_engine_type engine_type() const override { return HASH_SJ_ENGINE; }

  const QEP_TAB *get_qep_tab() const { return tab; }
};
#endif /* ITEM_SUBSELECT_INCLUDED */
