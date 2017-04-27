#ifndef ITEM_SUM_INCLUDED
#define ITEM_SUM_INCLUDED

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


/* classes for sum functions */

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <sys/types.h>
#include <utility>          // std::forward

#include "binary_log_types.h"
#include "enum_query_type.h"
#include "item.h"           // Item_result_field
#include "item_func.h"      // Item_int_func
#include "json_dom.h"       // Json_wrapper
#include "m_ctype.h"
#include "m_string.h"
#include "mem_root_array.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_decimal.h"
#include "my_inttypes.h"
#include "my_macros.h"
#include "my_table_map.h"
#include "my_time.h"
#include "my_tree.h"        // TREE
#include "mysql_com.h"
#include "parse_tree_node_base.h"
#include "sql_alloc.h"      // Sql_alloc
#include "sql_const.h"
#include "sql_string.h"
#include "sql_udf.h"        // udf_handler
#include "system_variables.h"
#include "table.h"
#include "template_utils.h"

class Field;
class Item_sum;
class PT_item_list;
class PT_order_list;
class THD;
class Temp_table_param;

/**
  The abstract base class for the Aggregator_* classes.
  It implements the data collection functions (setup/add/clear)
  as either pass-through to the real functionality or
  as collectors into an Unique (for distinct) structure.

  Note that update_field/reset_field are not in that
  class, because they're simply not called when
  GROUP BY/DISTINCT can be handled with help of index on grouped 
  fields (quick_group is false);
*/

class Aggregator : public Sql_alloc
{
  friend class Item_sum;
  friend class Item_sum_sum;
  friend class Item_sum_count;
  friend class Item_sum_avg;

  /* 
    All members are protected as this class is not usable outside of an 
    Item_sum descendant.
  */
protected:
  /* the aggregate function class to act on */
  Item_sum *item_sum;

public:
  Aggregator (Item_sum *arg): item_sum(arg) {}
  virtual ~Aggregator () {}                   /* Keep gcc happy */

  enum Aggregator_type { SIMPLE_AGGREGATOR, DISTINCT_AGGREGATOR }; 
  virtual Aggregator_type Aggrtype() = 0;

  /**
    Called before adding the first row. 
    Allocates and sets up the internal aggregation structures used, 
    e.g. the Unique instance used to calculate distinct.
  */
  virtual bool setup(THD *) = 0;

  /**
    Called when we need to wipe out all the data from the aggregator :
    all the values acumulated and all the state.
    Cleans up the internal structures and resets them to their initial state.
  */
  virtual void clear() = 0;

  /**
    Called when there's a new value to be aggregated.
    Updates the internal state of the aggregator to reflect the new value.
  */
  virtual bool add() = 0;

  /**
    Called when there are no more data and the final value is to be retrieved.
    Finalises the state of the aggregator, so the final result can be retrieved.
  */
  virtual void endup() = 0;

  /** Decimal value of being-aggregated argument */
  virtual my_decimal *arg_val_decimal(my_decimal * value) = 0;
  /** Floating point value of being-aggregated argument */
  virtual double arg_val_real() = 0;
  /**
    NULLness of being-aggregated argument.

    @param use_null_value Optimization: to determine if the argument is NULL
    we must, in the general case, call is_null() on it, which itself might
    call val_*() on it, which might be costly. If you just have called
    arg_val*(), you can pass use_null_value=true; this way, arg_is_null()
    might avoid is_null() and instead do a cheap read of the Item's null_value
    (updated by arg_val*()).
  */
  virtual bool arg_is_null(bool use_null_value) = 0;
};


class SELECT_LEX;

/**
  Class Item_sum is the base class used for special expressions that SQL calls
  'set functions'. These expressions are formed with the help of aggregate
  functions such as SUM, MAX, GROUP_CONCAT etc.

 GENERAL NOTES

  A set function cannot be used in all positions where expressions are accepted.
  There are some quite explicable restrictions for the use of set functions.

  In the query:

    SELECT AVG(b) FROM t1 WHERE SUM(b) > 20 GROUP by a

  the set function AVG(b) is valid, while the usage of SUM(b) is invalid.
  A WHERE condition must contain expressions that can be evaluated for each row
  of the table. Yet the expression SUM(b) can be evaluated only for each group
  of rows with the same value of column a.
  In the query:

    SELECT AVG(b) FROM t1 WHERE c > 30 GROUP BY a HAVING SUM(b) > 20

  both set function expressions AVG(b) and SUM(b) are valid.

  We can say that in a query without nested selects an occurrence of a
  set function in an expression of the SELECT list or/and in the HAVING
  clause is valid, while in the WHERE clause, FROM clause or GROUP BY clause
  it is invalid.

  The general rule to detect whether a set function is valid in a query with
  nested subqueries is much more complicated.

  Consider the following query:

    SELECT t1.a FROM t1 GROUP BY t1.a
      HAVING t1.a > ALL (SELECT t2.c FROM t2 WHERE SUM(t1.b) < t2.c).

  The set function SUM(b) is used here in the WHERE clause of the subquery.
  Nevertheless it is valid since it is contained in the HAVING clause of the
  outer query. The expression SUM(t1.b) is evaluated for each group defined
 in the main query, not for groups of the subquery.

  The problem of finding the query where to aggregate a particular
  set function is not so simple as it seems to be.

  In the query: 
    SELECT t1.a FROM t1 GROUP BY t1.a
     HAVING t1.a > ALL(SELECT t2.c FROM t2 GROUP BY t2.c
                         HAVING SUM(t1.a) < t2.c)

  the set function can be evaluated in both the outer and the inner query block.
  If we evaluate SUM(t1.a) for the outer query then we get the value of t1.a
  multiplied by the cardinality of a group in table t1. In this case,
  SUM(t1.a) is used as a constant value in each correlated subquery.
  But SUM(t1.a) can also be evaluated for the inner query.
  In this case t1.a will be a constant value for each correlated subquery and
  summation is performed for each group of table t2.
  (Here it makes sense to remind that the query

    SELECT c FROM t GROUP BY a HAVING SUM(1) < a 

  is quite valid in our SQL).

  So depending on what query block we assign the set function to we
  can get different results.

  The general rule to detect the query block Q where a set function will be
  aggregated (evaluated) can be formulated as follows.

  Reference: SQL2011 @<set function specification@> syntax rules 6 and 7.

  Consider a set function S(E) where E is an expression which contains
  column references C1, ..., Cn. Resolve all column references Ci against
  the query block Qi containing the set function S(E). Let Q be the innermost
  query block of all query blocks Qi. (It should be noted here that S(E)
  in no way can be aggregated in the query block containing the subquery Q,
  otherwise S(E) would refer to at least one unbound column reference).
  If S(E) is used in a construct of Q where set functions are allowed then
  we aggregate S(E) in Q.
  Otherwise:
  - if ANSI SQL mode is enabled (MODE_ANSI), then report an error.
  - otherwise, look for the innermost query block containing S(E) of those
    where usage of S(E) is allowed. The place of aggregation depends on which
    clause the subquery is contained within; It will be different when
    contained in a WHERE clause versus in the select list or in HAVING clause.

  Let's demonstrate how this rule is applied to the following queries.

  1. SELECT t1.a FROM t1 GROUP BY t1.a
       HAVING t1.a > ALL(SELECT t2.b FROM t2 GROUP BY t2.b
                           HAVING t2.b > ALL(SELECT t3.c FROM t3 GROUP BY t3.c
                                                HAVING SUM(t1.a+t2.b) < t3.c))
  For this query the set function SUM(t1.a+t2.b) contains t1.a and t2.b
  with t1.a defined in the outermost query, and t2.b defined for its
  subquery. The set function is contained in the HAVING clause of the subquery
  and can be evaluated in this subquery.

  2. SELECT t1.a FROM t1 GROUP BY t1.a
       HAVING t1.a > ALL(SELECT t2.b FROM t2
                           WHERE t2.b > ALL (SELECT t3.c FROM t3 GROUP BY t3.c
                                               HAVING SUM(t1.a+t2.b) < t3.c))
  The set function SUM(t1.a+t2.b) is contained in the WHERE clause of the second
  query block - the outermost query block where t1.a and t2.b are defined.
  If we evaluate the function in this subquery we violate the context rules.
  So we evaluate the function in the third query block (over table t3) where it
  is used under the HAVING clause; if in ANSI SQL mode, an error is thrown.

  3. SELECT t1.a FROM t1 GROUP BY t1.a
       HAVING t1.a > ALL(SELECT t2.b FROM t2
                           WHERE t2.b > ALL (SELECT t3.c FROM t3 
                                               WHERE SUM(t1.a+t2.b) < t3.c))
  In this query, evaluation of SUM(t1.a+t2.b) is not valid neither in the second
  nor in the third query block.

  Set functions can generally not be nested. In the query

    SELECT t1.a from t1 GROUP BY t1.a HAVING AVG(SUM(t1.b)) > 20

  the expression SUM(b) is not valid, even though it is contained inside
  a HAVING clause.
  However, it is acceptable in the query:

    SELECT t.1 FROM t1 GROUP BY t1.a HAVING SUM(t1.b) > 20.

  An argument of a set function does not have to be a simple column reference
  as seen in examples above. This can be a more complex expression

    SELECT t1.a FROM t1 GROUP BY t1.a HAVING SUM(t1.b+1) > 20.

  The expression SUM(t1.b+1) has clear semantics in this context:
  we sum up the values of t1.b+1 where t1.b varies for all values within a
  group of rows that contain the same t1.a value.

  A set function for an outer query yields a constant value within a subquery.
  So the semantics of the query

    SELECT t1.a FROM t1 GROUP BY t1.a
      HAVING t1.a IN (SELECT t2.c FROM t2 GROUP BY t2.c
                        HAVING AVG(t2.c+SUM(t1.b)) > 20)

  is still clear. For a group of rows with the same value for t1.a, calculate
  the value of SUM(t1.b) as 's'. This value is substituted in the subquery:

    SELECT t2.c FROM t2 GROUP BY t2.c HAVING AVG(t2.c+s)

  By the same reason the following query with a subquery 

    SELECT t1.a FROM t1 GROUP BY t1.a
      HAVING t1.a IN (SELECT t2.c FROM t2 GROUP BY t2.c
                        HAVING AVG(SUM(t1.b)) > 20)
  is also valid.

 IMPLEMENTATION NOTES

  The member base_select contains a reference to the query block that the
  set function is contained within.

  The member aggr_select contains a reference to the query block where the
  set function is aggregated.

  The field max_aggr_level holds the maximum of the nest levels of the
  unbound column references contained in the set function. A column reference
  is unbound within a set function if it is not bound by any subquery
  used as a subexpression in this function. A column reference is bound by
  a subquery if it is a reference to the column by which the aggregation
  of some set function that is used in the subquery is calculated.
  For the set function used in the query

    SELECT t1.a FROM t1 GROUP BY t1.a
      HAVING t1.a > ALL(SELECT t2.b FROM t2 GROUP BY t2.b
                          HAVING t2.b > ALL(SELECT t3.c FROM t3 GROUP BY t3.c
                                              HAVING SUM(t1.a+t2.b) < t3.c))

  the value of max_aggr_level is equal to 1 since t1.a is bound in the main
  query, and t2.b is bound by the first subquery whose nest level is 1.
  Obviously a set function cannot be aggregated in a subquery whose
  nest level is less than max_aggr_level. (Yet it can be aggregated in the
  subqueries whose nest level is greater than max_aggr_level.)
  In the query
    SELECT t1.a FROM t1 HAVING AVG(t1.a+(SELECT MIN(t2.c) FROM t2))

  the value of the max_aggr_level for the AVG set function is 0 since
  the reference t2.c is bound in the subquery.

  If a set function contains no column references (like COUNT(*)),
  max_aggr_level is -1.

  The field 'max_sum_func_level' is to contain the maximum of the
  nest levels of the set functions that are used as subexpressions of
  the arguments of the given set function, but not aggregated in any
  subquery within this set function. A nested set function s1 can be
  used within set function s0 only if s1.max_sum_func_level <
  s0.max_sum_func_level. Set function s1 is considered as nested
  for set function s0 if s1 is not calculated in any subquery
  within s0.

  A set function that is used as a subexpression in an argument of another
  set function refers to the latter via the field 'in_sum_func'.

  The condition imposed on the usage of set functions are checked when
  we traverse query subexpressions with the help of the recursive method
  fix_fields. When we apply this method to an object of the class
  Item_sum, first, on the descent, we call the method init_sum_func_check
  that initialize members used at checking. Then, on the ascent, we
  call the method check_sum_func that validates the set function usage
  and reports an error if it is invalid.
  The method check_sum_func serves to link the items for the set functions
  that are aggregated in the containing query blocks. Circular chains of such
  functions are attached to the corresponding SELECT_LEX structures
  through the field inner_sum_func_list.

  Exploiting the fact that the members mentioned above are used in one
  recursive function we could have allocated them on the thread stack.
  Yet we don't do it now.
  
  It is assumed that the nesting level of subqueries does not exceed 63
  (valid nesting levels are stored in a 64-bit bitmap called nesting_map).
  The assumption is enforced in LEX::new_query().
*/

class Item_sum :public Item_result_field
{
  typedef Item_result_field super;

  friend class Aggregator_distinct;
  friend class Aggregator_simple;

protected:
  /**
    Aggregator class instance. Not set initially. Allocated only after
    it is determined if the incoming data are already distinct.
  */
  Aggregator *aggr;

private:
  /**
    Used in making ROLLUP. Set for the ROLLUP copies of the original
    Item_sum and passed to create_tmp_field() to cause it to work
    over the temp table buffer that is referenced by
    Item_result_field::result_field.
  */
  bool force_copy_fields;

  /**
    Indicates how the aggregate function was specified by the parser :
     true if it was written as AGGREGATE(DISTINCT),
     false if it was AGGREGATE()
  */
  bool with_distinct;

public:

  bool has_force_copy_fields() const { return force_copy_fields; }
  bool has_with_distinct()     const { return with_distinct; }

  enum Sumfunctype
  {
    COUNT_FUNC,          // COUNT
    COUNT_DISTINCT_FUNC, // COUNT (DISTINCT)
    SUM_FUNC,            // SUM
    SUM_DISTINCT_FUNC,   // SUM (DISTINCT)
    AVG_FUNC,            // AVG
    AVG_DISTINCT_FUNC,   // AVG (DISTINCT)
    MIN_FUNC,            // MIN
    MAX_FUNC,            // MAX
    STD_FUNC,            // STD/STDDEV/STDDEV_POP
    VARIANCE_FUNC,       // VARIANCE/VAR_POP and VAR_SAMP
    SUM_BIT_FUNC,        // BIT_AND, BIT_OR and BIT_XOR
    UDF_SUM_FUNC,        // user defined functions
    GROUP_CONCAT_FUNC,   // GROUP_CONCAT
    JSON_AGG_FUNC,       // JSON_ARRAYAGG and JSON_OBJECTAGG
  };

  Item **ref_by;  ///< pointer to a ref to the object used to register it
  Item_sum *next; ///< next in the circular chain of registered objects
  Item_sum *in_sum_func;   ///< the containing set function if any
  SELECT_LEX *base_select; ///< query block where function is placed
  SELECT_LEX *aggr_select; ///< query block where function is aggregated
  int8 max_aggr_level;     ///< max level of unbound column references
  int8 max_sum_func_level; ///< max level of aggregation for contained functions
  bool quick_group;        ///< If incremental update of fields

protected:  
  uint arg_count;
  Item **args, *tmp_args[2];
  table_map used_tables_cache;
  bool forced_const;
  static ulonglong ram_limitation(THD *thd);

public:  

  void mark_as_sum_func();
  void mark_as_sum_func(SELECT_LEX *);
  Item_sum(const POS &pos)
    :super(pos), next(NULL), quick_group(true), arg_count(0),
     forced_const(false)
  {
    init_aggregator();
  }


  Item_sum(Item *a)
    :next(NULL), quick_group(true), arg_count(1), args(tmp_args),
     forced_const(false)
  {
    args[0]=a;
    mark_as_sum_func();
    init_aggregator();
  }
  Item_sum(const POS &pos, Item *a)
    :super(pos), next(NULL), quick_group(true), arg_count(1), args(tmp_args),
     forced_const(false)
  {
    args[0]=a;
    init_aggregator();
  }

  Item_sum(const POS &pos, Item *a, Item *b)
    :super(pos), next(nullptr), quick_group(true), arg_count(2), args(tmp_args),
     forced_const(false)
  {
    args[0]= a;
    args[1]= b;
    init_aggregator();
  }

  Item_sum(const POS &pos, PT_item_list *opt_list);

  /// Copy constructor, need to perform subqueries with temporary tables
  Item_sum(THD *thd, Item_sum *item);

  bool itemize(Parse_context *pc, Item **res) override;
  Type type() const override { return SUM_FUNC_ITEM; }
  virtual enum Sumfunctype sum_func() const= 0;
  virtual void fix_after_pullout(SELECT_LEX*,
                     SELECT_LEX *removed_select MY_ATTRIBUTE((unused))) override
  {
    // Just make sure we are not aggregating into a context that is merged up.
    DBUG_ASSERT(base_select != removed_select &&
                aggr_select != removed_select);
  }

  /**
    Resets the aggregate value to its default and aggregates the current
    value of its attribute(s).
  */  
  inline bool reset_and_add() 
  { 
    aggregator_clear(); 
    return aggregator_add(); 
  };

  /*
    Called when new group is started and results are being saved in
    a temporary table. Similarly to reset_and_add() it resets the 
    value to its default and aggregates the value of its 
    attribute(s), but must also store it in result_field. 
    This set of methods (result_item(), reset_field, update_field()) of
    Item_sum is used only if quick_group is not null. Otherwise
    copy_or_same() is used to obtain a copy of this item.
  */
  virtual void reset_field()=0;
  /*
    Called for each new value in the group, when temporary table is in use.
    Similar to add(), but uses temporary table field to obtain current value,
    Updated value is then saved in the field.
  */
  virtual void update_field()=0;
  virtual bool keep_field_type() const { return 0; }
  bool resolve_type(THD *) override;
  virtual Item *result_item(Field *field)
    { return new Item_field(field); }
  table_map used_tables() const override { return used_tables_cache; }
  void update_used_tables() override;
  bool is_null() override { return null_value; }
  void make_const()
  {
    used_tables_cache= 0; 
    forced_const= true;
  }
  bool const_item() const override { return forced_const; }
  bool const_during_execution() const override { return false; }
  void print(String *str, enum_query_type query_type) override;
  void fix_num_length_and_dec();
  bool eq(const Item *item, bool binary_cmp) const override;
  /**
    Mark an aggregate as having no rows.

    This function is called by the execution engine to assign 'NO ROWS
    FOUND' value to an aggregate item, when the underlying result set
    has no rows. Such value, in a general case, may be different from
    the default value of the item after 'clear()': e.g. a numeric item
    may be initialized to 0 by clear() and to NULL by
    no_rows_in_result().
  */
  void no_rows_in_result() override
  {
    set_aggregator(with_distinct ?
                   Aggregator::DISTINCT_AGGREGATOR :
                   Aggregator::SIMPLE_AGGREGATOR);
    aggregator_clear();
  }
  virtual void make_unique() { force_copy_fields= true; }
  virtual Field *create_tmp_field(bool group, TABLE *table);
  bool walk(Item_processor processor, enum_walk walk, uchar *arg) override;
  bool clean_up_after_removal(uchar *arg) override;
  bool aggregate_check_group(uchar *arg) override;
  bool aggregate_check_distinct(uchar *arg) override;
  bool init_sum_func_check(THD *thd);
  bool check_sum_func(THD *thd, Item **ref);

  Item *get_arg(uint i) { return args[i]; }
  Item *set_arg(uint i, THD *thd, Item *new_val);
  uint get_arg_count() const { return arg_count; }
  /// @todo delete this when we no longer support temporary transformations
  Item **get_arg_ptr(uint i) { return &args[i]; }

  /* Initialization of distinct related members */
  void init_aggregator()
  {
    aggr= NULL;
    with_distinct= false;
    force_copy_fields= false;
  }

  /**
    Called to initialize the aggregator.
  */

  inline bool aggregator_setup(THD *thd) { return aggr->setup(thd); };

  /**
    Called to cleanup the aggregator.
  */

  inline void aggregator_clear() { aggr->clear(); }

  /**
    Called to add value to the aggregator.
  */

  inline bool aggregator_add() { return aggr->add(); };

  /* stores the declared DISTINCT flag (from the parser) */
  void set_distinct(bool distinct)
  {
    with_distinct= distinct;
    quick_group= !with_distinct;
  }

  /*
    Set the type of aggregation : DISTINCT or not.

    May be called multiple times.
  */

  int set_aggregator(Aggregator::Aggregator_type aggregator);

  virtual void clear()= 0;
  virtual bool add()= 0;
  virtual bool setup(THD*) { return false; }

  void cleanup() override;
};


class Unique;


/**
 The distinct aggregator. 
 Implements AGGFN (DISTINCT ..)
 Collects all the data into an Unique (similarly to what Item_sum_distinct 
 does currently) and then (if applicable) iterates over the list of 
 unique values and pumps them back into its object
*/

class Aggregator_distinct : public Aggregator
{
  friend class Item_sum_sum;

  /* 
    flag to prevent consecutive runs of endup(). Normally in endup there are 
    expensive calculations (like walking the distinct tree for example) 
    which we must do only once if there are no data changes.
    We can re-use the data for the second and subsequent val_xxx() calls.
    endup_done set to TRUE also means that the calculated values for
    the aggregate functions are correct and don't need recalculation.
  */
  bool endup_done;

  /*
    Used depending on the type of the aggregate function and the presence of
    blob columns in it:
    - For COUNT(DISTINCT) and no blob fields this points to a real temporary
      table. It's used as a hash table.
    - For AVG/SUM(DISTINCT) or COUNT(DISTINCT) with blob fields only the
      in-memory data structure of a temporary table is constructed.
      It's used by the Field classes to transform data into row format.
  */
  TABLE *table;
  
  /*
    An array of field lengths on row allocated and used only for 
    COUNT(DISTINCT) with multiple columns and no blobs. Used in 
    Aggregator_distinct::composite_key_cmp (called from Unique to compare 
    nodes
  */
  uint32 *field_lengths;

  /*
    Used in conjunction with 'table' to support the access to Field classes 
    for COUNT(DISTINCT). Needed by copy_fields()/copy_funcs().
  */
  Temp_table_param *tmp_table_param;
  
  /*
    If there are no blobs in the COUNT(DISTINCT) arguments, we can use a tree,
    which is faster than heap table. In that case, we still use the table
    to help get things set up, but we insert nothing in it. 
    For AVG/SUM(DISTINCT) we always use this tree (as it takes a single 
    argument) to get the distinct rows.
  */
  Unique *tree;

  /* 
    The length of the temp table row. Must be a member of the class as it
    gets passed down to simple_raw_key_cmp () as a compare function argument
    to Unique. simple_raw_key_cmp () is used as a fast comparison function 
    when the entire row can be binary compared.
  */  
  uint tree_key_length;

  enum Const_distinct{
    NOT_CONST= 0,
    /**
      Set to true if the result is known to be always NULL.
      If set deactivates creation and usage of the temporary table (in the
      'table' member) and the Unique instance (in the 'tree' member) as well as
      the calculation of the final value on the first call to
      @c Item_sum::val_xxx(),
      @c Item_avg::val_xxx(),
      @c Item_count::val_xxx().
     */
    CONST_NULL,
    /**
      Set to true if count distinct is on only const items. Distinct on a const
      value will always be the constant itself. And count distinct of the same
      would always be 1. Similar to CONST_NULL, it avoids creation of temporary
      table and the Unique instance.
     */
    CONST_NOT_NULL
  } const_distinct;

  /**
    When feeding back the data in endup() from Unique/temp table back to
    Item_sum::add() methods we must read the data from Unique (and not
    recalculate the functions that are given as arguments to the aggregate
    function.
    This flag is to tell the arg_*() methods to take the data from the Unique
    instead of calling the relevant val_..() method.
  */
  bool use_distinct_values;

public:
  Aggregator_distinct (Item_sum *sum) :
    Aggregator(sum), table(NULL), tmp_table_param(NULL), tree(NULL),
    const_distinct(NOT_CONST), use_distinct_values(false) {}
  virtual ~Aggregator_distinct ();
  Aggregator_type Aggrtype() override { return DISTINCT_AGGREGATOR; }

  bool setup(THD *) override;
  void clear() override;
  bool add() override;
  void endup() override;
  my_decimal *arg_val_decimal(my_decimal * value) override;
  double arg_val_real() override;
  bool arg_is_null(bool use_null_value) override;

  bool unique_walk_function(void *element);
  static int composite_key_cmp(const void* arg, const void* a, const void* b);
};


/**
  The pass-through aggregator. 
  Implements AGGFN (DISTINCT ..) by knowing it gets distinct data on input. 
  So it just pumps them back to the Item_sum descendant class.
*/
class Aggregator_simple : public Aggregator
{
public:

  Aggregator_simple (Item_sum *sum) :
    Aggregator(sum) {}
  Aggregator_type Aggrtype() override { return Aggregator::SIMPLE_AGGREGATOR; }

  bool setup(THD *thd) override { return item_sum->setup(thd); }
  void clear() override { item_sum->clear(); }
  bool add() override { return item_sum->add(); }
  void endup() override {};
  my_decimal *arg_val_decimal(my_decimal * value) override;
  double arg_val_real() override;
  bool arg_is_null(bool use_null_value) override;
};


class Item_sum_num :public Item_sum
{
protected:
  /*
   val_xxx() functions may be called several times during the execution of a 
   query. Derived classes that require extensive calculation in val_xxx()
   maintain cache of aggregate value. This variable governs the validity of 
   that cache.
  */
  bool is_evaluated;
public:
  Item_sum_num(const POS &pos, Item *item_par) 
    :Item_sum(pos, item_par), is_evaluated(false)
  {}

  Item_sum_num(const POS &pos, PT_item_list *list) 
    :Item_sum(pos, list), is_evaluated(false)
  {}

  Item_sum_num(THD *thd, Item_sum_num *item) 
    :Item_sum(thd, item),is_evaluated(item->is_evaluated) {}
  bool fix_fields(THD *, Item **) override;
  longlong val_int() override
  {
    DBUG_ASSERT(fixed == 1);
    return (longlong) rint(val_real());             /* Real as default */
  }
  String *val_str(String *str) override;
  my_decimal *val_decimal(my_decimal *) override;
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) override
  {
    return get_date_from_numeric(ltime, fuzzydate); /* Decimal or real */
  }
  bool get_time(MYSQL_TIME *ltime) override
  {
    return get_time_from_numeric(ltime); /* Decimal or real */
  }
  void reset_field() override;
};


class Item_sum_int :public Item_sum_num
{
public:
  Item_sum_int(const POS &pos, Item *item_par) :Item_sum_num(pos, item_par)
  { set_data_type_longlong(); }
  Item_sum_int(const POS &pos, PT_item_list *list) :Item_sum_num(pos, list)
  { set_data_type_longlong(); }
  Item_sum_int(THD *thd, Item_sum_int *item) :Item_sum_num(thd, item)
  { set_data_type_longlong(); }
  bool resolve_type(THD*) override
  {
    maybe_null= false;
    for (uint i= 0; i < arg_count; i++)
    {
       maybe_null|= args[i]->maybe_null;
    }
    null_value= FALSE;
    return false;
  }
  double val_real() override
  { DBUG_ASSERT(fixed); return static_cast<double>(val_int()); }
  String *val_str(String *str) override;
  my_decimal *val_decimal(my_decimal *) override;
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) override
  {
    return get_date_from_int(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime) override
  {
    return get_time_from_int(ltime);
  }
  enum Item_result result_type() const override { return INT_RESULT; }
};


class Item_sum_sum :public Item_sum_num
{
protected:
  Item_result hybrid_type;
  double sum;
  my_decimal dec_buffs[2];
  uint curr_dec_buff;
  bool resolve_type(THD *) override;

public:
  Item_sum_sum(const POS &pos, Item *item_par, bool distinct)
    :Item_sum_num(pos, item_par)
  {
    set_distinct(distinct);
  }

  Item_sum_sum(THD *thd, Item_sum_sum *item);
  enum Sumfunctype sum_func() const override
  {
    return has_with_distinct() ? SUM_DISTINCT_FUNC : SUM_FUNC;
  }
  void clear() override;
  bool add() override;
  double val_real() override;
  longlong val_int() override;
  String *val_str(String *str) override;
  my_decimal *val_decimal(my_decimal *) override;
  enum Item_result result_type() const override { return hybrid_type; }
  void reset_field() override;
  void update_field() override;
  void no_rows_in_result() override {}
  const char *func_name() const override
  {
    return has_with_distinct() ? "sum(distinct " : "sum("; 
  }
  Item *copy_or_same(THD *thd) override;
};


class Item_sum_count :public Item_sum_int
{
  longlong count;

  friend class Aggregator_distinct;

  void clear() override;
  bool add() override;
  void cleanup() override;

  public:
  Item_sum_count(const POS &pos, Item *item_par)
    :Item_sum_int(pos, item_par),count(0)
  {}

  /**
    Constructs an instance for COUNT(DISTINCT)

    @param pos  Position of token in the parser.
    @param list A list of the arguments to the aggregate function

    This constructor is called by the parser only for COUNT (DISTINCT).
  */

  Item_sum_count(const POS &pos, PT_item_list *list)
    :Item_sum_int(pos, list), count(0)
  {
    set_distinct(true);
  }
  Item_sum_count(THD *thd, Item_sum_count *item)
    :Item_sum_int(thd, item), count(item->count)
  {}
  enum Sumfunctype sum_func() const override
  {
    return has_with_distinct() ? COUNT_DISTINCT_FUNC : COUNT_FUNC;
  }
  bool resolve_type(THD*) override
  {
    maybe_null= false;
    null_value= FALSE;
    return false;
  }
  void no_rows_in_result() override { count= 0; }
  void make_const(longlong count_arg)
  {
    count=count_arg;
    Item_sum::make_const();
  }
  longlong val_int() override;
  void reset_field() override;
  void update_field() override;
  const char *func_name() const override
  {
    return has_with_distinct() ? "count(distinct " : "count(";
  }
  Item *copy_or_same(THD *thd) override;
};


/* Item to get the value of a stored sum function */

class Item_sum_avg;
class Item_sum_bit;


/**
  This is used in connection which a parent Item_sum:
  - which can produce different result types (is "hybrid")
  - which stores function's value into a temporary table's column (one
  row per group).
  - which stores in the column some internal piece of information which should
  not be returned to the user, so special implementations are needed.
*/
class Item_sum_hybrid_field: public Item_result_field
{
protected:
  /// The tmp table's column containing the value of the set function.
  Field *field;
  /// Stores the Item's result type.
  Item_result hybrid_type;
public:
  enum Item_result result_type() const override { return hybrid_type; }
  bool mark_field_in_map(uchar *arg) override
  {
    /*
      Filesort (find_all_keys) over a temporary table collects the columns it
      needs.
    */
    return Item::mark_field_in_map(pointer_cast<Mark_field *>(arg), field);
  }
};


/**
  Common abstract class for:
    Item_avg_field
    Item_variance_field
*/
class Item_sum_num_field: public Item_sum_hybrid_field
{
public:
  longlong val_int() override
  {
    /* can't be fix_fields()ed */
    return (longlong) rint(val_real());
  }
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) override
  {
    return get_date_from_numeric(ltime, fuzzydate); /* Decimal or real */
  }
  bool get_time(MYSQL_TIME *ltime) override
  {
    return get_time_from_numeric(ltime); /* Decimal or real */
  }
  bool is_null() override
  {
    /*
      TODO : Implement error handling for this function as
      update_null_value() can return error.
    */
    (void) update_null_value();
    return null_value;
  }
};


class Item_avg_field :public Item_sum_num_field
{
public:
  uint f_precision, f_scale, dec_bin_size;
  uint prec_increment;
  Item_avg_field(Item_result res_type, Item_sum_avg *item);
  enum Type type() const override { return FIELD_AVG_ITEM; }
  double val_real() override;
  my_decimal *val_decimal(my_decimal *) override;
  String *val_str(String *) override;
  bool resolve_type(THD *) override { return false; }
  const char *func_name() const override
  { DBUG_ASSERT(0); return "avg_field"; }
};


/// This is used in connection with an Item_sum_bit, @see Item_sum_hybrid_field
class Item_sum_bit_field :public Item_sum_hybrid_field
{
protected:
  ulonglong reset_bits;
public:
  Item_sum_bit_field(Item_result res_type, Item_sum_bit *item,
                     ulonglong reset_bits);
  longlong val_int() override;
  double val_real() override;
  my_decimal *val_decimal(my_decimal *) override;
  String *val_str(String *) override;
  bool resolve_type(THD *) override { return false; }
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) override;
  bool get_time(MYSQL_TIME *ltime) override;
  enum Type type() const override { return FIELD_BIT_ITEM; }
  const char *func_name() const override
  { DBUG_ASSERT(0); return "sum_bit_field"; }
};


/// Common abstraction for Item_sum_json_array and Item_sum_json_object
class Item_sum_json : public Item_sum
{
protected:
  /// String used when reading JSON binary values or JSON text values.
  String m_value;
  /// String used for converting JSON text values to utf8mb4 charset.
  String m_conversion_buffer;
  /// Wrapper around the container (object/array) which accumulates the value.
  Json_wrapper m_wrapper;

public:
  /**
    Construct an Item_sum_json instance.
    @param args  arguments to forward to Item_sum's constructor
  */
  template <typename... Args>
  Item_sum_json(Args&&... args) : Item_sum(std::forward<Args>(args)...)
  {
    set_data_type_json();
  }

  bool fix_fields(THD *thd, Item **pItem) override;
  enum Sumfunctype sum_func() const override { return JSON_AGG_FUNC; }
  Item_result result_type() const override { return STRING_RESULT; }

  double val_real() override;
  longlong val_int() override;
  String *val_str(String *str) override;
  bool val_json(Json_wrapper *wr) override;
  my_decimal *val_decimal(my_decimal *decimal_buffer) override;
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) override;
  bool get_time(MYSQL_TIME *ltime) override;

  void reset_field() override;
  void update_field() override;
};


/// Implements aggregation of values into an array.
class Item_sum_json_array final : public Item_sum_json
{
  /// Accumulates the final value.
  Json_array m_json_array;
public:
  Item_sum_json_array(THD *thd, Item_sum *item)
    : Item_sum_json(thd, item) { }
  Item_sum_json_array(const POS &pos, Item *a)
    : Item_sum_json(pos, a) { }
  const char *func_name() const override { return "json_arrayagg("; }
  void clear() override;
  bool add() override;
  Item *copy_or_same(THD* thd) override;
};


/// Implements aggregation of values into an object.
class Item_sum_json_object final : public Item_sum_json
{
  /// Accumulates the final value.
  Json_object m_json_object;
  /// Buffer used to get the value of the key.
  String m_tmp_key_value;
public:
  Item_sum_json_object(THD *thd, Item_sum *item)
    : Item_sum_json(thd, item) { }
  Item_sum_json_object(const POS &pos, Item *a, Item *b)
    : Item_sum_json(pos, a, b) { }
  const char *func_name() const override { return "json_objectagg("; }
  void clear() override;
  bool add() override;
  Item *copy_or_same(THD* thd) override;
};


class Item_sum_avg final : public Item_sum_sum
{
public:
  ulonglong count;
  uint prec_increment;
  uint f_precision, f_scale, dec_bin_size;

  Item_sum_avg(const POS &pos, Item *item_par, bool distinct) 
    :Item_sum_sum(pos, item_par, distinct), count(0) 
  {}

  Item_sum_avg(THD *thd, Item_sum_avg *item)
    :Item_sum_sum(thd, item), count(item->count),
    prec_increment(item->prec_increment) {}

  bool resolve_type(THD *thd) override;
  enum Sumfunctype sum_func() const override
  {
    return has_with_distinct() ? AVG_DISTINCT_FUNC : AVG_FUNC;
  }
  void clear() override;
  bool add() override;
  double val_real() override;
  // In SPs we might force the "wrong" type with select into a declare variable
  longlong val_int() override { return (longlong) rint(val_real()); }
  my_decimal *val_decimal(my_decimal *) override;
  String *val_str(String *str) override;
  void reset_field() override;
  void update_field() override;
  Item *result_item(Field*) override
  { return new Item_avg_field(hybrid_type, this); }
  void no_rows_in_result() override {}
  const char *func_name() const override
  {
    return has_with_distinct() ? "avg(distinct " : "avg(";
  }
  Item *copy_or_same(THD *thd) override;
  Field *create_tmp_field(bool group, TABLE *table) override;
  void cleanup() override
  {
    count= 0;
    Item_sum_sum::cleanup();
  }
};

class Item_sum_variance;

class Item_variance_field : public Item_sum_num_field
{
protected:
  uint sample;
public:
  Item_variance_field(Item_sum_variance *item);
  enum Type type() const override {return FIELD_VARIANCE_ITEM; }
  double val_real() override;
  String *val_str(String *str) override
  { return val_string_from_real(str); }
  my_decimal *val_decimal(my_decimal *dec_buf) override
  { return val_decimal_from_real(dec_buf); }
  bool resolve_type(THD *) override { return false; }
  const char *func_name() const override
  { DBUG_ASSERT(0); return "variance_field"; }
};


/*
  variance(a) =

  =  sum (ai - avg(a))^2 / count(a) )
  =  sum (ai^2 - 2*ai*avg(a) + avg(a)^2) / count(a)
  =  (sum(ai^2) - sum(2*ai*avg(a)) + sum(avg(a)^2))/count(a) = 
  =  (sum(ai^2) - 2*avg(a)*sum(a) + count(a)*avg(a)^2)/count(a) = 
  =  (sum(ai^2) - 2*sum(a)*sum(a)/count(a) + count(a)*sum(a)^2/count(a)^2 )/count(a) = 
  =  (sum(ai^2) - 2*sum(a)^2/count(a) + sum(a)^2/count(a) )/count(a) = 
  =  (sum(ai^2) - sum(a)^2/count(a))/count(a)

But, this falls prey to catastrophic cancellation.  Instead, use the recurrence formulas

  M_{1} = x_{1}, ~ M_{k} = M_{k-1} + (x_{k} - M_{k-1}) / k newline 
  S_{1} = 0, ~ S_{k} = S_{k-1} + (x_{k} - M_{k-1}) times (x_{k} - M_{k}) newline
  for 2 <= k <= n newline
  ital variance = S_{n} / (n-1)

*/

class Item_sum_variance : public Item_sum_num
{
  bool resolve_type(THD *) override;

public:
  Item_result hybrid_type;
  double recurrence_m, recurrence_s;    /* Used in recurrence relation. */
  ulonglong count;
  uint f_precision0, f_scale0;
  uint f_precision1, f_scale1;
  uint dec_bin_size0, dec_bin_size1;
  uint sample;
  uint prec_increment;

  Item_sum_variance(const POS &pos, Item *item_par, uint sample_arg)
    :Item_sum_num(pos, item_par),
     hybrid_type(REAL_RESULT), count(0), sample(sample_arg)
  {}

  Item_sum_variance(THD *thd, Item_sum_variance *item);
  enum Sumfunctype sum_func() const override { return VARIANCE_FUNC; }
  void clear() override;
  bool add() override;
  double val_real() override;
  my_decimal *val_decimal(my_decimal *) override;
  void reset_field() override;
  void update_field() override;
  Item *result_item(Field *) override
  { return new Item_variance_field(this); }
  void no_rows_in_result() override {}
  const char *func_name() const override
    { return sample ? "var_samp(" : "variance("; }
  Item *copy_or_same(THD* thd) override;
  Field *create_tmp_field(bool group, TABLE *table) override;
  enum Item_result result_type() const override { return REAL_RESULT; }
  void cleanup() override
  {
    count= 0;
    Item_sum_num::cleanup();
  }
};

class Item_sum_std;

class Item_std_field final : public Item_variance_field
{
public:
  Item_std_field(Item_sum_std *item);
  enum Type type() const override { return FIELD_STD_ITEM; }
  double val_real() override;
  my_decimal *val_decimal(my_decimal *) override;
  enum Item_result result_type () const override { return REAL_RESULT; }
  const char *func_name() const override { DBUG_ASSERT(0); return "std_field";}
};

/*
   standard_deviation(a) = sqrt(variance(a))
*/

class Item_sum_std : public Item_sum_variance
{
  public:
  Item_sum_std(const POS &pos, Item *item_par, uint sample_arg)
    :Item_sum_variance(pos, item_par, sample_arg)
  {}

  Item_sum_std(THD *thd, Item_sum_std *item)
    :Item_sum_variance(thd, item)
    {}
  enum Sumfunctype sum_func() const override { return STD_FUNC; }
  double val_real() override;
  Item *result_item(Field*) override
    { return new Item_std_field(this); }
  const char *func_name() const override { return "std("; }
  Item *copy_or_same(THD* thd) override;
  enum Item_result result_type () const override { return REAL_RESULT; }
};

// This class is a string or number function depending on num_func
class Arg_comparator;

class Item_sum_hybrid : public Item_sum
{
protected:
  Item_cache *value, *arg_cache;
  Arg_comparator *cmp;
  Item_result hybrid_type;
  int cmp_sign;
  bool was_values;  // Set if we have found at least one row (for max/min only)

  public:
  Item_sum_hybrid(Item *item_par,int sign)
    :Item_sum(item_par), value(0), arg_cache(0), cmp(0),
    hybrid_type(INT_RESULT), cmp_sign(sign), was_values(true)
  { collation.set(&my_charset_bin); }
  Item_sum_hybrid(const POS &pos, Item *item_par,int sign)
    :Item_sum(pos, item_par), value(0), arg_cache(0), cmp(0),
    hybrid_type(INT_RESULT), cmp_sign(sign), was_values(true)
  { collation.set(&my_charset_bin); }

  Item_sum_hybrid(THD *thd, Item_sum_hybrid *item)
    :Item_sum(thd, item), value(item->value), arg_cache(0),
    hybrid_type(item->hybrid_type),
    cmp_sign(item->cmp_sign), was_values(item->was_values)
  { }
  bool fix_fields(THD *, Item **) override;
  bool setup_hybrid(Item *item, Item *value_arg);
  void clear() override;
  double val_real() override;
  longlong val_int() override;
  longlong val_time_temporal() override;
  longlong val_date_temporal() override;
  my_decimal *val_decimal(my_decimal *) override;
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) override;
  bool get_time(MYSQL_TIME *ltime) override;
  void reset_field() override;
  String *val_str(String *) override;
  bool val_json(Json_wrapper *wr) override;
  bool keep_field_type() const override { return 1; }
  enum Item_result result_type () const override { return hybrid_type; }
  void update_field() override;
  void min_max_update_str_field();
  void min_max_update_temporal_field();
  void min_max_update_real_field();
  void min_max_update_int_field();
  void min_max_update_decimal_field();
  void cleanup() override;
  bool any_value() { return was_values; }
  void no_rows_in_result() override;
  Field *create_tmp_field(bool group, TABLE *table) override;
};


class Item_sum_min final : public Item_sum_hybrid
{
public:
  Item_sum_min(Item *item_par) :Item_sum_hybrid(item_par,1) {}
  Item_sum_min(const POS &pos, Item *item_par) :Item_sum_hybrid(pos, item_par,1)
  {}

  Item_sum_min(THD *thd, Item_sum_min *item) :Item_sum_hybrid(thd, item) {}
  enum Sumfunctype sum_func() const override { return MIN_FUNC; }

  bool add() override;
  const char *func_name() const override { return "min("; }
  Item *copy_or_same(THD* thd) override;
};


class Item_sum_max final : public Item_sum_hybrid
{
public:
  Item_sum_max(Item *item_par) :Item_sum_hybrid(item_par,-1) {}
  Item_sum_max(const POS &pos, Item *item_par)
    :Item_sum_hybrid(pos, item_par, -1)
  {}

  Item_sum_max(THD *thd, Item_sum_max *item) :Item_sum_hybrid(thd, item) {}
  enum Sumfunctype sum_func() const override {return MAX_FUNC;}

  bool add() override;
  const char *func_name() const override { return "max("; }
  Item *copy_or_same(THD* thd) override;
};


/**
  Base class used to implement BIT_AND, BIT_OR and BIT_XOR set functions.
 */
class Item_sum_bit :public Item_sum
{
protected:
  /// Stores the neutral element for function
  ulonglong reset_bits;
  /// Stores the result value for the INT_RESULT
  ulonglong bits;
  /// Stores the result value for the STRING_RESULT
  String value_buff;
  /// Stores the Item's result type. Can only be INT_RESULT or STRING_RESULT
  Item_result hybrid_type;
  /// Buffer used to avoid String allocation in the constructor
  const char initial_value_buff_storage[1] = {0};
public:
  Item_sum_bit(const POS &pos, Item *item_par, ulonglong reset_arg)
    :Item_sum(pos, item_par), reset_bits(reset_arg), bits(reset_arg),
     value_buff(initial_value_buff_storage, 1, &my_charset_bin)
  {}

  /// Copy constructor, used for executing subqueries with temporary tables
  Item_sum_bit(THD *thd, Item_sum_bit *item):
    Item_sum(thd, item), reset_bits(item->reset_bits), bits(item->bits),
    value_buff(initial_value_buff_storage, 1, &my_charset_bin),
    hybrid_type(item->hybrid_type)
  {
    /**
      This constructor should only be called during the Optimize stage.
      Asserting that the item was not evaluated yet.
    */
    DBUG_ASSERT(item->value_buff.length() == 1);
    DBUG_ASSERT(item->bits == item->reset_bits);
  }

  Item *result_item(Field *) override
  { return new Item_sum_bit_field(hybrid_type, this, reset_bits); }

  enum Sumfunctype sum_func() const override { return SUM_BIT_FUNC; }
  enum Item_result result_type() const override { return hybrid_type; }
  void clear() override;
  longlong val_int() override;
  double val_real() override;
  String *val_str(String *str) override;
  my_decimal *val_decimal(my_decimal *decimal_value) override;
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) override;
  bool get_time(MYSQL_TIME *ltime) override;
  void reset_field() override;
  void update_field() override;
  bool resolve_type(THD *) override;
  bool fix_fields(THD *thd, Item **ref) override;
  void cleanup() override
  {
    bits= reset_bits;
    Item_sum::cleanup();
  }
  template<class Char_op, class Int_op>
  bool eval_op(Char_op char_op, Int_op int_op);
};


class Item_sum_or final: public Item_sum_bit
{
public:
  Item_sum_or(const POS &pos, Item *item_par) :Item_sum_bit(pos, item_par,0LL)
  {}

  Item_sum_or(THD *thd, Item_sum_or *item) :Item_sum_bit(thd, item) {}
  bool add() override;
  const char *func_name() const override { return "bit_or("; }
  Item *copy_or_same(THD* thd) override;
};


class Item_sum_and final : public Item_sum_bit
{
  public:
  Item_sum_and(const POS &pos, Item *item_par)
    :Item_sum_bit(pos, item_par, ULLONG_MAX)
  {}

  Item_sum_and(THD *thd, Item_sum_and *item) :Item_sum_bit(thd, item) {}
  bool add() override;
  const char *func_name() const override { return "bit_and("; }
  Item *copy_or_same(THD* thd) override;
};

class Item_sum_xor final : public Item_sum_bit
{
  public:
  Item_sum_xor(const POS &pos, Item *item_par)
    :Item_sum_bit(pos, item_par, 0LL)
  {}

  Item_sum_xor(THD *thd, Item_sum_xor *item) :Item_sum_bit(thd, item) {}
  bool add() override;
  const char *func_name() const override { return "bit_xor("; }
  Item *copy_or_same(THD* thd) override;
};


/*
  User defined aggregates
*/

class Item_udf_sum : public Item_sum
{
  typedef Item_sum super;
protected:
  udf_handler udf;

public:
  Item_udf_sum(const POS &pos, udf_func *udf_arg, PT_item_list *opt_list)
    :Item_sum(pos, opt_list), udf(udf_arg)
  { quick_group= false;}
  Item_udf_sum(THD *thd, Item_udf_sum *item)
    :Item_sum(thd, item), udf(item->udf)
  { udf.not_original= true; }

  bool itemize(Parse_context *pc, Item **res) override;
  const char *func_name() const override { return udf.name(); }
  bool fix_fields(THD *thd, Item **ref) override
  {
    DBUG_ASSERT(fixed == 0);

    if (init_sum_func_check(thd))
      return true;

    fixed= 1;
    if (udf.fix_fields(thd, this, this->arg_count, this->args))
      return true;

    return check_sum_func(thd, ref);
  }
  enum Sumfunctype sum_func() const override { return UDF_SUM_FUNC; }

  void clear() override;
  bool add() override;
  void reset_field() override {};
  void update_field() override {};
  void cleanup() override;
  void print(String *str, enum_query_type query_type) override;
};


class Item_sum_udf_float final : public Item_udf_sum
{
 public:
  Item_sum_udf_float(const POS &pos, udf_func *udf_arg, PT_item_list *opt_list)
    :Item_udf_sum(pos, udf_arg, opt_list)
  {}
  Item_sum_udf_float(THD *thd, Item_sum_udf_float *item)
    :Item_udf_sum(thd, item) {}
  longlong val_int() override
  {
    DBUG_ASSERT(fixed == 1);
    return (longlong) rint(Item_sum_udf_float::val_real());
  }
  double val_real() override;
  String *val_str(String*str) override;
  my_decimal *val_decimal(my_decimal *) override;
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) override
  {
    return get_date_from_real(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime) override
  {
    return get_time_from_real(ltime);
  }
  bool resolve_type(THD *) override
  {
    set_data_type(MYSQL_TYPE_DOUBLE);
    fix_num_length_and_dec();
    return false;
   }
  Item *copy_or_same(THD* thd) override;
};


class Item_sum_udf_int final : public Item_udf_sum
{
public:
  Item_sum_udf_int(const POS &pos, udf_func *udf_arg, PT_item_list *opt_list)
    :Item_udf_sum(pos, udf_arg, opt_list)
  {}
  Item_sum_udf_int(THD *thd, Item_sum_udf_int *item)
    :Item_udf_sum(thd, item) {}
  longlong val_int() override;
  double val_real() override
    { DBUG_ASSERT(fixed == 1); return (double) Item_sum_udf_int::val_int(); }
  String *val_str(String*str) override;
  my_decimal *val_decimal(my_decimal *) override;
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) override
  {
    return get_date_from_int(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime) override
  {
    return get_time_from_int(ltime);
  }
  enum Item_result result_type () const override { return INT_RESULT; }
  bool resolve_type(THD *) override
  {
    set_data_type_longlong();
    return false;
  }
  Item *copy_or_same(THD* thd) override;
};


class Item_sum_udf_str final : public Item_udf_sum
{
public:
  Item_sum_udf_str(const POS &pos, udf_func *udf_arg, PT_item_list *opt_list)
    :Item_udf_sum(pos, udf_arg, opt_list)
  {}
  Item_sum_udf_str(THD *thd, Item_sum_udf_str *item)
    :Item_udf_sum(thd, item) {}
  String *val_str(String *) override;
  double val_real() override
  {
    int err_not_used;
    char *end_not_used;
    String *res;
    res=val_str(&str_value);
    return res ? my_strntod(res->charset(),(char*) res->ptr(),res->length(),
			    &end_not_used, &err_not_used) : 0.0;
  }
  longlong val_int() override
  {
    int err_not_used;
    char *end;
    String *res;
    const CHARSET_INFO *cs;

    if (!(res= val_str(&str_value)))
      return 0;                                 /* Null value */
    cs= res->charset();
    end= (char*) res->ptr()+res->length();
    return cs->cset->strtoll10(cs, res->ptr(), &end, &err_not_used);
  }
  my_decimal *val_decimal(my_decimal *dec) override;
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) override
  {
    return get_date_from_string(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime) override
  {
    return get_time_from_string(ltime);
  }
  enum Item_result result_type () const override { return STRING_RESULT; }
  bool resolve_type(THD *) override;
  Item *copy_or_same(THD* thd) override;
};


class Item_sum_udf_decimal final : public Item_udf_sum
{
public:
  Item_sum_udf_decimal(const POS &pos,
                       udf_func *udf_arg, PT_item_list *opt_list)
    :Item_udf_sum(pos, udf_arg, opt_list)
  {}
  Item_sum_udf_decimal(THD *thd, Item_sum_udf_decimal *item)
    :Item_udf_sum(thd, item) {}
  String *val_str(String *) override;
  double val_real() override;
  longlong val_int() override;
  my_decimal *val_decimal(my_decimal *) override;
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) override
  {
    return get_date_from_decimal(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime) override
  {
    return get_time_from_decimal(ltime);
  }
  enum Item_result result_type () const override { return DECIMAL_RESULT; }
  bool resolve_type(THD *) override
  {
    set_data_type(MYSQL_TYPE_NEWDECIMAL);
    fix_num_length_and_dec();
    return false;
   }
  Item *copy_or_same(THD* thd) override;
};


C_MODE_START
int group_concat_key_cmp_with_distinct(const void* arg, const void* key1,
                                       const void* key2);
int group_concat_key_cmp_with_order(const void* arg, const void* key1,
                                    const void* key2);
int dump_leaf_key(void* key_arg,
                  element_count count MY_ATTRIBUTE((unused)),
                  void* item_arg);
C_MODE_END

class Item_func_group_concat final : public Item_sum
{
  typedef Item_sum super;

  Temp_table_param *tmp_table_param;
  String result;
  String *separator;
  TREE tree_base;
  TREE *tree;

  /**
     If DISTINCT is used with this GROUP_CONCAT, this member is used to filter
     out duplicates. 
     @see Item_func_group_concat::setup
     @see Item_func_group_concat::add
     @see Item_func_group_concat::clear
   */
  Unique *unique_filter;
  TABLE *table;
  Mem_root_array<ORDER> order_array;
  Name_resolution_context *context;
  /** The number of ORDER BY items. */
  uint arg_count_order;
  /** The number of selected items, aka the expr list. */
  uint arg_count_field;
  uint row_count;
  bool distinct;
  bool warning_for_row;
  bool always_null;
  bool force_copy_fields;
  /** True if result has been written to output buffer. */
  bool m_result_finalized;
  /*
    Following is 0 normal object and pointer to original one for copy
    (to correctly free resources)
  */
  Item_func_group_concat *original;

  friend int group_concat_key_cmp_with_distinct(const void* arg,
                                                const void* key1,
                                                const void* key2);
  friend int group_concat_key_cmp_with_order(const void* arg,
                                             const void* key1,
					     const void* key2);
  friend int dump_leaf_key(void* key_arg,
                           element_count count MY_ATTRIBUTE((unused)),
			   void* item_arg);

public:
  Item_func_group_concat(const POS &pos,
                         bool is_distinct, PT_item_list *select_list,
                         PT_order_list *opt_order_list, String *separator);

  Item_func_group_concat(THD *thd, Item_func_group_concat *item);
  ~Item_func_group_concat();

  bool itemize(Parse_context *pc, Item **res) override;
  void cleanup() override;

  enum Sumfunctype sum_func() const override { return GROUP_CONCAT_FUNC; }
  const char *func_name() const override { return "group_concat"; }
  Item_result result_type() const override { return STRING_RESULT; }
  Field *make_string_field(TABLE *table_arg) override;
  void clear() override;
  bool add() override;
  void reset_field() override { DBUG_ASSERT(0); }        // not used
  void update_field() override { DBUG_ASSERT(0); }       // not used
  bool fix_fields(THD *,Item **) override;
  bool setup(THD *thd) override;
  void make_unique() override;
  double val_real() override
  {
    String *res;  res=val_str(&str_value);
    return res ? my_atof(res->c_ptr()) : 0.0;
  }
  longlong val_int() override
  {
    String *res;
    char *end_ptr;
    int error;
    if (!(res= val_str(&str_value)))
      return (longlong) 0;
    end_ptr= (char*) res->ptr()+ res->length();
    return my_strtoll10(res->ptr(), &end_ptr, &error);
  }
  my_decimal *val_decimal(my_decimal *decimal_value) override
  {
    return val_decimal_from_string(decimal_value);
  }
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) override
  {
    return get_date_from_string(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime) override
  {
    return get_time_from_string(ltime);
  }
  String* val_str(String* str) override;
  Item *copy_or_same(THD* thd) override;
  void no_rows_in_result() override {}
  void print(String *str, enum_query_type query_type) override;
  bool change_context_processor(uchar *cntx) override
  {
    context= reinterpret_cast<Name_resolution_context *>(cntx);
    return false;
  }
};

/**
  Class for implementation of the GROUPING function. The GROUPING
  function distinguishes super-aggregate rows from regular grouped
  rows. GROUP BY extensions such as ROLLUP and CUBE produce
  super-aggregate rows where the set of all values is represented
  by null. Using the GROUPING function, you can distinguish a null
  representing the set of all values in a super-aggregate row from
  a NULL in a regular row.
*/
class Item_func_grouping: public Item_int_func
{
public:
  Item_func_grouping(const POS &pos, PT_item_list *a): Item_int_func(pos,a) {}
  const char * func_name() const override { return "grouping"; }
  enum Functype functype() const override { return GROUPING_FUNC; }
  longlong val_int() override;
  bool aggregate_check_group(uchar *arg) override;
  bool fix_fields(THD *thd, Item **ref) override;
  void update_used_tables() override
  {
    const bool aggregated= has_aggregation();
    Item_int_func::update_used_tables();
    if (aggregated)
      set_aggregation();
  }
  void cleanup() override;
};

#endif /* ITEM_SUM_INCLUDED */
