#ifndef ITEM_SUBSELECT_INCLUDED
#define ITEM_SUBSELECT_INCLUDED

/* Copyright (c) 2002, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/// Implements classes that represent subqueries and predicates containing
/// table subqueries.

#include <assert.h>
#include <sys/types.h>

#include <cstddef>
#include <memory>  // unique_ptr
#include <vector>

#include "field_types.h"  // enum_field_types
#include "my_alloc.h"     // Destroy_only

#include "my_inttypes.h"
#include "my_table_map.h"
#include "my_time.h"
#include "mysql/udf_registration_types.h"
#include "mysql_time.h"
#include "sql/comp_creator.h"
#include "sql/enum_query_type.h"
#include "sql/item.h"                    // Item_result_field
#include "sql/iterators/row_iterator.h"  // IWYU pragma: keep
#include "sql/parse_location.h"          // POS
#include "sql/parse_tree_node_base.h"
#include "sql/sql_const.h"
#include "sql/sql_opt_exec_shared.h"
#include "template_utils.h"

class Comp_creator;
class Field;
class Item_func_not_all;
class Item_in_optimizer;
class JOIN;
class Json_wrapper;
class PT_subquery;
class Query_result_interceptor;
class Query_result_subquery;
class Query_result_union;
class Query_block;
class Query_expression;
class String;
class THD;
class Temp_table_param;
class my_decimal;
class subselect_indexsubquery_engine;
struct AccessPath;
class Table_ref;

template <class T>
class List;

/// Base class that is common to all subqueries and subquery predicates

class Item_subselect : public Item_result_field {
  typedef Item_result_field super;

 public:
  Query_expression *query_expr() const { return m_query_expr; }

  /**
     If !=NO_PLAN_IDX: this Item is in the condition attached to the JOIN_TAB
     having this index in the parent JOIN.
  */
  int in_cond_of_tab{NO_PLAN_IDX};

  // For EXPLAIN.
  enum enum_engine_type { OTHER_ENGINE, INDEXSUBQUERY_ENGINE, HASH_SJ_ENGINE };
  enum_engine_type engine_type() const;

  // For EXPLAIN. Only valid if engine_type() == HASH_SJ_ENGINE.
  const TABLE *get_table() const;
  const Index_lookup &index_lookup() const;
  join_type get_join_type() const;

  void create_iterators(THD *thd);
  virtual AccessPath *root_access_path() const { return nullptr; }

  enum Subquery_type {
    SCALAR_SUBQUERY,  ///< Scalar or row subquery
    EXISTS_SUBQUERY,  ///< [NOT] EXISTS subquery predicate
    IN_SUBQUERY,      ///< [NOT] IN subquery predicate
    ALL_SUBQUERY,     ///< comp-op ALL quantified comparison predicate
    ANY_SUBQUERY      ///< comp-op ANY quantified comparison predicate
  };

  /// Accumulate used tables
  void accumulate_used_tables(table_map add_tables) {
    m_used_tables_cache |= add_tables;
    m_subquery_used_tables |= add_tables;
  }

  /// Return whether this subquery references any tables in the directly
  /// containing query block, i.e. whether there are outer references to the
  /// containing block inside the subquery.
  bool contains_outer_references() const {
    return (subquery_used_tables() & ~PSEUDO_TABLE_BITS) != 0;
  }

  virtual Subquery_type subquery_type() const = 0;

  /**
    @returns whether this subquery is a single column scalar subquery.
             (Note that scalar and row subqueries are both represented as
              scalar subqueries, this function can be used to distinguish them)
  */
  virtual bool is_single_column_scalar_subquery() const { return false; }

  void cleanup() override;
  /**
    Reset state after a single execution of a subquery, useful when a
    dependent subquery must be evaluated multiple times for varying values
    of the outer references.
    This is a lighter cleanup procedure than the one provided by cleanup().
  */
  virtual void reset() {
    m_value_assigned = false;
    null_value = true;
  }
  bool is_value_assigned() const { return m_value_assigned; }
  void set_value_assigned() { m_value_assigned = true; }
  void reset_value_assigned() { m_value_assigned = false; }
  enum Type type() const override;
  bool is_null() override { return update_null_value() || null_value; }
  bool fix_fields(THD *thd, Item **ref) override;
  void fix_after_pullout(Query_block *parent_query_block,
                         Query_block *removed_query_block) override;
  virtual bool exec(THD *thd);
  table_map used_tables() const override { return m_used_tables_cache; }
  table_map not_null_tables() const override { return 0; }
  /// @returns used tables for subquery (excluding any left-hand expression)
  table_map subquery_used_tables() const { return m_subquery_used_tables; }
  Item *get_tmp_table_item(THD *thd) override;
  void update_used_tables() override;
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;

  void set_indexsubquery_engine(subselect_indexsubquery_engine *eng) {
    indexsubquery_engine = eng;
  }

  /*
    True if this subquery has been already evaluated. Implemented only for
    single select and union subqueries only.
  */
  bool is_evaluated() const;
  bool is_uncacheable() const;

  /**
    Used by max/min subquery to initialize value presence registration
    mechanism. Engine calls this function before re-execution of subquery.
  */
  virtual void reset_has_values() {}
  /**
    @returns the "place" where this subquery is located in the simply
             containing query block.
  */
  enum_parsing_context place() { return m_parsing_place; }

  bool walk(Item_processor processor, enum_walk walk, uchar *arg) override;
  bool explain_subquery_checker(uchar **arg) override;
  bool inform_item_in_cond_of_tab(uchar *arg) override;
  bool clean_up_after_removal(uchar *arg) override;

  bool check_function_as_value_generator(uchar *args) override {
    Check_function_as_value_generator_parameters *func_arg =
        pointer_cast<Check_function_as_value_generator_parameters *>(args);
    func_arg->err_code = func_arg->get_unnamed_function_error_code();
    return true;
  }

  /// argument used by walk method collect_scalar_subqueries ("css")
  struct Collect_subq_info {
    ///< accumulated all subq (or aggregates) found
    std::vector<Item_subselect *> list;
    Query_block *m_query_block{nullptr};
    Collect_subq_info(Query_block *owner) : m_query_block(owner) {}
    bool contains(Query_expression *candidate) {
      for (auto sq : list) {
        if (sq->m_query_expr == candidate) return true;
      }
      return false;
    }
  };

  bool collect_subqueries(uchar *) override;
  Item *replace_item_field(uchar *arg) override;
  Item *replace_item_view_ref(uchar *arg) override;

 protected:
  Item_subselect() : Item_result_field() { set_subquery(); }
  explicit Item_subselect(const POS &pos) : super(pos) { set_subquery(); }
  /**
    Bind this subquery object with the supplied query expression.
    As part of transformations, this function may re-bind the subquery to
    a different query expression.

    @param qe  supplied query expression
  */
  void bind(Query_expression *qe);

  /// The query expression of the subquery
  Query_expression *m_query_expr{nullptr};  // Actual value set in construction

  /// The query result object associated with the query expression
  Query_result_interceptor *m_query_result{nullptr};

  /// Only relevant for Item_in_subselect; optimized structure used for
  /// execution in place of running the entire subquery.
  subselect_indexsubquery_engine *indexsubquery_engine{nullptr};

  /// cache of used tables
  table_map m_used_tables_cache{0};
  /// cache of used tables from subquery only (not including LHS of IN subquery)
  table_map m_subquery_used_tables{0};

  /// allowed number of columns (1 for scalar subqueries)
  uint m_max_columns{0};  // Irrelevant value, actually set during construction
  /// where subquery is placed
  enum_parsing_context m_parsing_place{CTX_NONE};

 private:
  bool subq_opt_away_processor(uchar *arg) override;

  /// Accumulate properties from underlying query expression
  void accumulate_properties();
  /// Accumulate properties from underlying query block
  void accumulate_properties(Query_block *select);
  /// Accumulate properties from a selected expression within a query block.
  void accumulate_expression(Item *item);
  /// Accumulate properties from a condition or GROUP/ORDER within a query
  /// block.
  void accumulate_condition(Item *item);

  /// True if value has been assigned to subquery
  bool m_value_assigned{false};
  /**
    Whether or not execution of this subquery has been traced by
    optimizer tracing already. If optimizer trace option
    REPEATED_SUBSELECT is disabled, this is used to disable tracing
    after the first one.
  */
  bool m_traced_before{false};
};

/// Class that represents scalar subquery and row subquery

class Item_singlerow_subselect : public Item_subselect {
 public:
  Item_singlerow_subselect(Query_block *query_block);
  Item_singlerow_subselect() : Item_subselect() {}

  bool fix_fields(THD *thd, Item **ref) override;
  void cleanup() override;
  Subquery_type subquery_type() const override { return SCALAR_SUBQUERY; }
  bool create_row(const mem_root_deque<Item *> &item_list, Item_cache **row,
                  bool possibly_empty);

  bool is_single_column_scalar_subquery() const override;

  void reset() override;
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
    To check for nullability, look at this->is_nullable() and not
    element_index[i]->is_nullable(), since the selected expressions are
    always NULL if the subquery is empty.
  */
  Item *element_index(uint i) override { return m_row[i]; }
  Item **addr(uint i) override { return pointer_cast<Item **>(m_row) + i; }
  bool check_cols(uint c) override;
  bool null_inside() override;
  void bring_value() override;

  bool collect_scalar_subqueries(uchar *) override;
  virtual bool is_maxmin() const { return false; }

  /**
    Argument for walk method replace_scalar_subquery
  */
  struct Scalar_subquery_replacement {
    ///< subquery to be replaced with field from derived table
    Item_singlerow_subselect *m_target;
    ///< The derived table of the transform
    TABLE *m_derived;
    ///< the replacement field
    Field *m_field;
    ///< The transformed query block.
    Query_block *m_outer_query_block;
    ///< The immediately surrounding query block. This will be the transformed
    ///< block or a subquery of it
    Query_block *m_inner_query_block;
    ///< True if subquery's selected item contains a COUNT aggregate
    bool m_add_coalesce{false};
    ///< Presence of HAVING clause in subquery: Only relevant if
    ///< \c m_add_coalesce is true
    bool m_add_having_compensation{false};
    ///< Index of field holding value of having clause in derived table's list
    ///< of fields. Only relevant if \c m_add_coalesce is true
    uint m_having_idx{0};

    Scalar_subquery_replacement(Item_singlerow_subselect *target,
                                TABLE *derived, Field *field,
                                Query_block *select, bool add_coalesce,
                                bool add_having_compensation, uint having_idx)
        : m_target(target),
          m_derived(derived),
          m_field(field),
          m_outer_query_block(select),
          m_inner_query_block(select),
          m_add_coalesce(add_coalesce),
          m_add_having_compensation(add_having_compensation),
          m_having_idx(having_idx) {}
  };

  Item *replace_scalar_subquery(uchar *arge) override;
  /**
    This method is used to implement a special case of semantic tree
    rewriting, mandated by a SQL:2003 exception in the specification.
    The only caller of this method is handle_sql2003_note184_exception(),
    see the code there for more details.
    Note that this method breaks the object internal integrity, by
    removing it's association with the corresponding Query_block,
    making this object orphan from the parse tree.
    No other method, beside the destructor, should be called on this
    object, as it is now invalid.
    @return the Query_block structure that was given in the constructor.
  */
  Query_block *invalidate_and_restore_query_block();
  std::optional<ContainedSubquery> get_contained_subquery(
      const Query_block *outer_query_block) override;
  friend class Query_result_scalar_subquery;

 private:
  /// Value cache of a scalar subquery
  Item_cache *m_value{nullptr};
  /**
    Value cache for a row or scalar subquery. In case of a scalar subquery,
    m_row points directly at m_value. In case of a row subquery, m_row
    is an array of pointers to individual Item_cache objects.
  */
  Item_cache **m_row{nullptr};
  bool m_no_rows{false};  ///< @c no_rows_in_result
};

/* used in static ALL/ANY optimization */
class Item_maxmin_subselect final : public Item_singlerow_subselect {
 public:
  /**
    Create an Item for use in transformation of quantified comparison
    predicates that can be evaluated using MAX or MIN aggregation.

    @param parent      The quantified comparison predicate that is transformed.
    @param query_block The query block representing the inner part of the subq.
    @param max_arg     Whether the aggregation is for MAX or MIN.
  */
  Item_maxmin_subselect(Item_subselect *parent, Query_block *query_block,
                        bool max_arg);
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
  void cleanup() override;
  bool has_values() const { return m_has_values; }
  void register_value() { m_has_values = true; }
  void reset_has_values() override { m_has_values = false; }
  bool is_maxmin() const override { return true; }

 private:
  bool m_max;                ///< True if performing MAX, false if MIN
  bool m_has_values{false};  ///< Set if we have found at least one row
};

/// Classes that represent predicates over table subqueries:
/// [NOT] EXISTS, [NOT] IN, ANY/SOME and ALL

/**
  Strategy which will be used to handle this subquery: flattening to a
  semi-join, conversion to a derived table, rewrite of IN to EXISTS...
  Sometimes the strategy is first only a candidate, then the real decision
  happens in a second phase. Other times the first decision is final.
 */
enum class Subquery_strategy : int {
  /// Nothing decided yet
  UNSPECIFIED,
  /// Candidate for rewriting IN(subquery) to EXISTS, or subquery
  /// materialization
  CANDIDATE_FOR_IN2EXISTS_OR_MAT,
  /// Candidate for semi-join flattening
  CANDIDATE_FOR_SEMIJOIN,
  /// Candidate for rewriting to joined derived table
  CANDIDATE_FOR_DERIVED_TABLE,
  /// Semi-join flattening
  SEMIJOIN,
  /// Rewrite to joined derived table
  DERIVED_TABLE,
  /// Evaluate as EXISTS subquery (possibly after rewriting from another type)
  SUBQ_EXISTS,
  /// Subquery materialization (HASH_SJ_ENGINE)
  SUBQ_MATERIALIZATION,
  /// Subquery has been deleted, probably because it was always false
  DELETED,
};

class Item_exists_subselect : public Item_subselect {
  typedef Item_subselect super;

 public:
  /**
    Create an Item that represents an EXISTS subquery predicate, or any
    quantified comparison predicate that uses the same base class.

    @param query_block First query block of query expression representing
                       the contained subquery.
  */
  explicit Item_exists_subselect(Query_block *query_block);

  Item_exists_subselect() : Item_subselect() {}

  explicit Item_exists_subselect(const POS &pos) : super(pos) {}

  // The left-hand expression of the subquery predicate. Unused with EXISTS
  Item *left_expr{nullptr};

  /// Priority of this predicate in the convert-to-semi-join-nest process.
  int sj_convert_priority{0};
  /// Execution strategy chosen for this Item
  Subquery_strategy strategy{Subquery_strategy::UNSPECIFIED};
  /// Used by the transformation to derived table
  enum_condition_context outer_condition_context{enum_condition_context::ANDS};

  /**
    Used by subquery optimizations to keep track about where this subquery
    predicate is located, and whether it is a candidate for transformation.
      (Table_ref*) 1 - the predicate is an AND-part of the WHERE
      join nest pointer    - the predicate is an AND-part of ON expression
                             of a join nest
      NULL                 - for all other locations. It also means that the
                             predicate is not a candidate for transformation.
    See also THD::emb_on_expr_nest.

    As for the second case above (the join nest pointer), note that this value
    may change if scalar subqueries are transformed to derived tables,
    cf. transform_scalar_subqueries_to_join_with_derived, due to the need to
    build new join nests. The change is performed in Query_block::nest_derived.
  */
  Table_ref *embedding_join_nest{nullptr};

  void notify_removal() override { strategy = Subquery_strategy::DELETED; }

  /*
    Transform subquery predicate to a form that can be executed, e.g.
    as an EXISTS subquery, or a MAX or MIN aggregation on the subquery.

    This is a virtual function that has implementations for EXISTS, IN
    and quantified comparison subquery predicates.

    @param thd              Thread handle
    @param[out] transformed Points to transformed representation of the object
                            if unchanged: No transformation was performed

    @returns false if success, true if error
  */
  virtual bool transformer(THD *thd, Item **transformed);

  Subquery_type subquery_type() const override { return EXISTS_SUBQUERY; }
  bool is_bool_func() const override { return true; }
  void reset() override {
    reset_value_assigned();
    m_value = false;
  }

  enum Item_result result_type() const override { return INT_RESULT; }
  /*
    The item is
    ([NOT] IN/EXISTS) [ IS [NOT] TRUE|FALSE ]
  */
  enum Bool_test value_transform = BOOL_IDENTITY;
  bool with_is_op() const {
    switch (value_transform) {
      case BOOL_IS_TRUE:
      case BOOL_IS_FALSE:
      case BOOL_NOT_TRUE:
      case BOOL_NOT_FALSE:
        return true;
      default:
        return false;
    }
  }
  /// True if the IS TRUE/FALSE wasn't explicit in the query
  bool implicit_is_op = false;
  Item *truth_transformer(THD *, enum Bool_test test) override;
  bool translate(bool &null_v, bool v);
  void apply_is_true() override {
    const bool had_is = with_is_op();
    truth_transformer(nullptr, BOOL_IS_TRUE);
    if (!had_is && value_transform == BOOL_IS_TRUE)
      implicit_is_op = true;  // needn't be written by EXPLAIN
  }
  /// True if the Item has decided that it can do antijoin
  bool can_do_aj = false;
  bool choose_semijoin_or_antijoin();
  bool fix_fields(THD *thd, Item **ref) override;
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
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;

  friend class Query_result_exists_subquery;

 protected:
  bool is_semijoin_candidate(THD *thd);
  bool is_derived_candidate(THD *thd);

  /// value of this item (boolean: exists/not-exists)
  bool m_value{false};

  /**
    True if naked IN is allowed to exchange FALSE for UNKNOWN.
    Because this is about the naked IN, there is no public ignore_unknown(),
    intentionally, so that callers don't get it wrong.
  */
  bool abort_on_null{false};
};

/**
  Representation of IN subquery predicates of the form
  "left_expr IN (SELECT ...)".

  @details
  This class has:
   - A "subquery execution engine" (as a subclass of Item_subselect) that allows
     it to evaluate subqueries. (and this class participates in execution by
     having m_was_null variable where part of execution result is stored.
   - Transformation methods (todo: more on this).

  This class is not used directly, it is "wrapped" into Item_in_optimizer
  which provides some small bits of subquery evaluation.
*/

class Item_in_subselect : public Item_exists_subselect {
  typedef Item_exists_subselect super;

 public:
  Item_in_subselect(Item *left_expr, Query_block *query_block);
  Item_in_subselect(const POS &pos, Item *left_expr,
                    PT_subquery *pt_subquery_arg);

  Item_in_subselect() : Item_exists_subselect(), pt_subselect(nullptr) {}

  bool do_itemize(Parse_context *pc, Item **res) override;

  void cleanup() override;
  Subquery_type subquery_type() const override { return IN_SUBQUERY; }

  void reset() override {
    m_value = false;
    null_value = false;
    m_was_null = false;
  }
  bool transformer(THD *thd, Item **transformed) override;
  bool quantified_comp_transformer(THD *thd, Comp_creator *func,
                                   Item **transformed);
  bool single_value_transformer(THD *thd, Comp_creator *func,
                                Item **transformed);
  bool row_value_transformer(THD *thd, Item **transformed);
  bool single_value_in_to_exists_transformer(THD *thd, Query_block *select,
                                             Comp_creator *func);
  bool row_value_in_to_exists_transformer(THD *thd, Query_block *select,
                                          Item_in_optimizer *optimizer);
  bool subquery_allows_materialization(THD *thd, Query_block *query_block,
                                       const Query_block *outer);
  bool walk(Item_processor processor, enum_walk walk, uchar *arg) override;
  Item *transform(Item_transformer transformer, uchar *arg) override;
  Item *compile(Item_analyzer analyzer, uchar **arg_p,
                Item_transformer transformer, uchar *arg_t) override;

  bool exec(THD *thd) override;
  longlong val_int() override;
  double val_real() override;
  String *val_str(String *) override;
  my_decimal *val_decimal(my_decimal *) override;
  bool val_bool() override;
  bool test_limit();
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;
  void fix_after_pullout(Query_block *parent_query_block,
                         Query_block *removed_query_block) override;
  void update_used_tables() override;
  bool init_left_expr_cache(THD *thd);

  /**
     Once the decision to use IN->EXISTS has been taken, performs some last
     steps of this transformation.
  */
  bool finalize_exists_transform(THD *thd, Query_block *select);
  /**
     Once the decision to use materialization has been taken, performs some
     last steps of this transformation.
  */
  bool finalize_materialization_transform(THD *thd, JOIN *join);
  AccessPath *root_access_path() const override;
  std::optional<ContainedSubquery> get_contained_subquery(
      const Query_block *outer_query_block) override;

  bool in2exists_added_to_where() const {
    return m_in2exists_info != nullptr && m_in2exists_info->added_to_where;
  }

  /// Is reliable only if IN->EXISTS has been done.
  bool dependent_before_in2exists() const {
    return m_in2exists_info->dependent_before;
  }

  bool *get_cond_guard(int i) const {
    return m_pushed_cond_guards != nullptr ? m_pushed_cond_guards + i : nullptr;
  }
  void set_cond_guard_var(int i, bool v) const {
    if (m_pushed_cond_guards != nullptr) m_pushed_cond_guards[i] = v;
  }

  friend class Item_ref_null_helper;
  friend class Item_is_not_null_test;
  friend class Item_in_optimizer;
  friend class subselect_indexsubquery_engine;
  friend class subselect_hash_sj_engine;

  /// Used to trigger on/off conditions that were pushed down to subquery
  bool *m_pushed_cond_guards{nullptr};

  /// Point on NOT/NOP before ALL/SOME subquery
  Item_func_not_all *m_upper_item{nullptr};

 protected:
  /**
    Cache of the left operand of the subquery predicate. Allocated in the
    runtime memory root, for each execution, thus need not be freed.
  */
  List<Cached_item> *m_left_expr_cache{nullptr};

  /// Whether m_left_expr_cache holds a value
  bool m_left_expr_cache_filled{false};

  /// The need for expr cache may be optimized away, @sa init_left_expr_cache.
  bool need_expr_cache{true};

 private:
  bool mark_as_outer(Item *left_row, size_t col);

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
  Item_ref *m_injected_left_expr{nullptr};

  bool m_was_null{false};

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
  } *m_in2exists_info{nullptr};

  PT_subquery *pt_subselect;

  bool val_bool_naked();
};

/**
  Class that represents a quantified comparison predicate.

  expression comp-op ANY ( subquery )
  expression comp-op ALL ( subquery )

  Note that the special cases = ANY (aka IN) and <> ALL (aka NOT IN)
  are handled by the base class Item_in_subselect.
*/
class Item_allany_subselect final : public Item_in_subselect {
 public:
  Item_allany_subselect(Item *left_expr, chooser_compare_func_creator fc,
                        Query_block *select, bool all);

  Subquery_type subquery_type() const override {
    return m_all ? ALL_SUBQUERY : ANY_SUBQUERY;
  }
  bool transformer(THD *thd, Item **transformed) override;
  void print(const THD *thd, String *str,
             enum_query_type query_type) const override;

  chooser_compare_func_creator m_func_creator;
  Comp_creator *m_func;
  bool m_all;
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
class subselect_indexsubquery_engine {
 protected:
  Query_result_union *result = nullptr; /* results storage class */
  /// Table which is read, using one of eq_ref, ref, ref_or_null.
  TABLE *table{nullptr};
  Table_ref *table_ref{nullptr};
  Index_lookup ref;
  join_type type{JT_UNKNOWN};
  Item *m_cond;      ///< The WHERE condition of  the subquery
  ulonglong m_hash;  ///< Hash value calculated by RefIterator, when needed.
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
  Item *m_having;

  Item_in_subselect *item; /* item that uses this engine */

 public:
  enum enum_engine_type { INDEXSUBQUERY_ENGINE, HASH_SJ_ENGINE };

  subselect_indexsubquery_engine(TABLE *table, Table_ref *table_ref,
                                 const Index_lookup &ref,
                                 enum join_type join_type,
                                 Item_in_subselect *subs, Item *where,
                                 Item *having)
      : table(table),
        table_ref(table_ref),
        ref(ref),
        type(join_type),
        m_cond(where),
        m_having(having),
        item(subs) {}
  virtual ~subselect_indexsubquery_engine() = default;
  virtual bool exec(THD *thd);
  virtual void print(const THD *thd, String *str, enum_query_type query_type);
  virtual enum_engine_type engine_type() const { return INDEXSUBQUERY_ENGINE; }
  virtual void cleanup() {}
  virtual void create_iterators(THD *) {}
};

/*
  This function is actually defined in sql_parse.cc, but it depends on
  chooser_compare_func_creator defined in this file.
 */
Item *all_any_subquery_creator(Item *left_expr,
                               chooser_compare_func_creator cmp, bool all,
                               Query_block *select);

/**
  Compute an IN predicate via a hash semi-join. The subquery is materialized
  during the first evaluation of the IN predicate. The IN predicate is executed
  via the functionality inherited from subselect_indexsubquery_engine.
*/

class subselect_hash_sj_engine final : public subselect_indexsubquery_engine {
 private:
  /* true if the subquery was materialized into a temp table. */
  bool is_materialized;
  // true if we know for sure that there are zero rows in the table.
  // Set only after is_materialized is true.
  bool has_zero_rows = false;
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
  Query_expression *const m_query_expr;
  unique_ptr_destroy_only<RowIterator> m_iterator;
  AccessPath *m_root_access_path;

  /// Saved result object, must be restored after use
  Query_result_interceptor *saved_result{nullptr};

 public:
  subselect_hash_sj_engine(Item_in_subselect *in_predicate,
                           Query_expression *query_expr)
      : subselect_indexsubquery_engine(nullptr, nullptr, {}, JT_UNKNOWN,
                                       in_predicate, nullptr, nullptr),
        is_materialized(false),
        m_query_expr(query_expr) {}
  ~subselect_hash_sj_engine() override;

  bool setup(THD *thd, const mem_root_deque<Item *> &tmp_columns);
  void cleanup() override;
  bool exec(THD *thd) override;
  void print(const THD *thd, String *str, enum_query_type query_type) override;
  enum_engine_type engine_type() const override { return HASH_SJ_ENGINE; }

  TABLE *get_table() const { return table; }
  const Index_lookup &index_lookup() const { return ref; }
  enum join_type get_join_type() const { return type; }
  AccessPath *root_access_path() const { return m_root_access_path; }
  void create_iterators(THD *thd) override;
};

/**
  Removes every predicate injected by IN->EXISTS.

  This function is different from others:
  - it wants to remove all traces of IN->EXISTS (for
  materialization)
  - remove_subq_pushed_predicates() and remove_additional_cond() want to
  remove only the conditions of IN->EXISTS which index lookup already
  satisfies (they are just an optimization).

  If there are no in2exists conditions, it will return the exact same
  pointer. If it returns a new Item, the old Item is left alone, so it
  can be reused in other settings.

  @param conds  Condition; may be nullptr.
  @returns      new condition
 */
Item *remove_in2exists_conds(Item *conds);

/// @returns whether the item is a quantified comparison predicate
bool is_quantified_comp_predicate(Item *item);

#endif /* ITEM_SUBSELECT_INCLUDED */
