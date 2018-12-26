/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

/*
  TODO:
  Fix that MAYBE_KEY are stored in the tree so that we can detect use
  of full hash keys for queries like:

  select s.id, kws.keyword_id from sites as s,kws where s.id=kws.site_id and kws.keyword_id in (204,205);

*/

// Needed by the unit tests
#ifndef OPT_RANGE_CC_INCLUDED
#define OPT_RANGE_CC_INCLUDED

/*
  This file contains:

  RangeAnalysisModule  
    A module that accepts a condition, index (or partitioning) description, 
    and builds lists of intervals (in index/partitioning space), such that 
    all possible records that match the condition are contained within the 
    intervals.
    The entry point for the range analysis module is get_mm_tree()
    (mm=min_max) function.
    
    The lists are returned in form of complicated structure of interlinked
    SEL_TREE/SEL_IMERGE/SEL_ARG objects.
    See quick_range_seq_next, find_used_partitions for examples of how to walk 
    this structure.
    All direct "users" of this module are located within this file, too.


  PartitionPruningModule
    A module that accepts a partitioned table, condition, and finds which
    partitions we will need to use in query execution. Search down for
    "PartitionPruningModule" for description.
    The module has single entry point - prune_partitions() function.


  Range/index_merge/groupby-minmax optimizer module  
    A module that accepts a table, condition, and returns 
     - a QUICK_*_SELECT object that can be used to retrieve rows that match
       the specified condition, or a "no records will match the condition" 
       statement.

    The module entry points are
      test_quick_select()
      get_quick_select_for_ref()


  Record retrieval code for range/index_merge/groupby-min-max.
    Implementations of QUICK_*_SELECT classes.

  KeyTupleFormat
  ~~~~~~~~~~~~~~
  The code in this file (and elsewhere) makes operations on key value tuples.
  Those tuples are stored in the following format:
  
  The tuple is a sequence of key part values. The length of key part value
  depends only on its type (and not depends on the what value is stored)
  
    KeyTuple: keypart1-data, keypart2-data, ...
  
  The value of each keypart is stored in the following format:
  
    keypart_data: [isnull_byte] keypart-value-bytes

  If a keypart may have a NULL value (key_part->field->real_maybe_null() can
  be used to check this), then the first byte is a NULL indicator with the 
  following valid values:
    1  - keypart has NULL value.
    0  - keypart has non-NULL value.

  <questionable-statement> If isnull_byte==1 (NULL value), then the following
  keypart->length bytes must be 0.
  </questionable-statement>

  keypart-value-bytes holds the value. Its format depends on the field type.
  The length of keypart-value-bytes may or may not depend on the value being
  stored. The default is that length is static and equal to 
  KEY_PART_INFO::length.
  
  Key parts with (key_part_flag & HA_BLOB_PART) have length depending of the 
  value:
  
     keypart-value-bytes: value_length value_bytes

  The value_length part itself occupies HA_KEY_BLOB_LENGTH=2 bytes.

  See key_copy() and key_restore() for code to move data between index tuple
  and table record

  CAUTION: the above description is only sergefp's understanding of the 
           subject and may omit some details.
*/

#include "opt_range.h"

#include "item_sum.h"            // Item_sum
#include "key.h"                 // is_key_used
#include "log.h"                 // sql_print_error
#include "opt_statistics.h"      // guess_rec_per_key
#include "opt_trace.h"           // Opt_trace_array
#include "partition_info.h"      // partition_info
#include "sql_partition.h"       // HA_USE_AUTO_PARTITION
#include "sql_base.h"            // free_io_cache
#include "sql_class.h"           // THD
#include "sql_opt_exec_shared.h" // QEP_shared_owner
#include "sql_optimizer.h"       // JOIN
#include "sql_parse.h"           // check_stack_overrun
#include "uniques.h"             // Unique
#include "opt_hints.h"           // hint_key_state
#include "mysys_err.h"           // EE_CAPACITY_EXCEEDED

using std::min;
using std::max;

/*
  Convert double value to #rows. Currently this does floor(), and we
  might consider using round() instead.
*/
#define double2rows(x) ((ha_rows)(x))

static int sel_cmp(Field *f,uchar *a,uchar *b,uint8 a_flag,uint8 b_flag);

static uchar is_null_string[2]= {1,0};

class RANGE_OPT_PARAM;

/**
  Error handling class for range optimizer. We handle only out of memory
  error here. This is to give a hint to the user to
  raise range_optimizer_max_mem_size if required.
  Warning for the memory error is pushed only once. The consequent errors
  will be ignored.
*/
class Range_optimizer_error_handler : public Internal_error_handler
{
public:
  Range_optimizer_error_handler()
    : m_has_errors(false), m_is_mem_error(false)
  {}

  virtual bool handle_condition(THD *thd,
                                uint sql_errno,
                                const char* sqlstate,
                                Sql_condition::enum_severity_level *level,
                                const char* msg)
  {
    if (*level == Sql_condition::SL_ERROR)
    {
      m_has_errors= true;
      /* Out of memory error is reported only once. Return as handled */
      if (m_is_mem_error && sql_errno == EE_CAPACITY_EXCEEDED)
        return true;
      if (sql_errno == EE_CAPACITY_EXCEEDED)
      {
        m_is_mem_error= true;
        /* Convert the error into a warning. */
        *level= Sql_condition::SL_WARNING;
        push_warning_printf(
                       thd, Sql_condition::SL_WARNING,
                       ER_CAPACITY_EXCEEDED,
                       ER_THD(thd, ER_CAPACITY_EXCEEDED),
                       (ulonglong)thd->variables.range_optimizer_max_mem_size,
                       "range_optimizer_max_mem_size",
                       ER_THD(thd, ER_CAPACITY_EXCEEDED_IN_RANGE_OPTIMIZER));
        return true;
      }
    }
    return false;
  }

  bool has_errors() const { return m_has_errors; }
private:
  bool m_has_errors;
  bool m_is_mem_error;
};

/*
  A construction block of the SEL_ARG-graph.
  
  The following description only covers graphs of SEL_ARG objects with 
  sel_arg->type==KEY_RANGE:

  One SEL_ARG object represents an "elementary interval" in form
  
      min_value <=?  table.keypartX  <=? max_value
  
  The interval is a non-empty interval of any kind: with[out] minimum/maximum
  bound, [half]open/closed, single-point interval, etc.

  1. SEL_ARG GRAPH STRUCTURE
  
  SEL_ARG objects are linked together in a graph. The meaning of the graph
  is better demostrated by an example:
  
     tree->keys[i]
      | 
      |             $              $
      |    part=1   $     part=2   $    part=3
      |             $              $
      |  +-------+  $   +-------+  $   +--------+
      |  | kp1<1 |--$-->| kp2=5 |--$-->| kp3=10 |
      |  +-------+  $   +-------+  $   +--------+
      |      |      $              $       |
      |      |      $              $   +--------+
      |      |      $              $   | kp3=12 | 
      |      |      $              $   +--------+ 
      |  +-------+  $              $   
      \->| kp1=2 |--$--------------$-+ 
         +-------+  $              $ |   +--------+
             |      $              $  ==>| kp3=11 |
         +-------+  $              $ |   +--------+
         | kp1=3 |--$--------------$-+       |
         +-------+  $              $     +--------+
             |      $              $     | kp3=14 |
            ...     $              $     +--------+
 
  The entire graph is partitioned into "interval lists".

  An interval list is a sequence of ordered disjoint intervals over
  the same key part. SEL_ARG are linked via "next" and "prev" pointers
  with NULL as sentinel.

    In the example pic, there are 4 interval lists: 
    "kp<1 OR kp1=2 OR kp1=3", "kp2=5", "kp3=10 OR kp3=12", "kp3=11 OR kp3=13".
    The vertical lines represent SEL_ARG::next/prev pointers.
    
  Additionally, all intervals in the list form a red-black (RB) tree,
  linked via left/right/parent pointers with null_element as sentinel. The
  red-black tree root SEL_ARG object will be further called "root of the
  interval list".

  A red-black tree with 7 SEL_ARGs will look similar to what is shown
  below. Left/right/parent pointers are shown while next pointers go from a
  node with number X to the node with number X+1 (and prev in the
  opposite direction):

                         Root
                        +---+
                        | 4 |
                        +---+
                   left/     \ right
                    __/       \__
                   /             \
              +---+               +---+
              | 2 |               | 6 |
              +---+               +---+
        left /     \ right  left /     \ right
            |       |           |       |
        +---+       +---+   +---+       +---+
        | 1 |       | 3 |   | 5 |       | 7 |
        +---+       +---+   +---+       +---+

  In this tree,
    * node1->prev == node7->next == NULL
    * node1->left == node1->right ==
      node3->left == ... node7->right == &null_element

  In an interval list, each member X may have SEL_ARG::next_key_part pointer
  pointing to the root of another interval list Y. The pointed interval list
  must cover a key part with greater number (i.e. Y->part > X->part).
    
    In the example pic, the next_key_part pointers are represented by
    horisontal lines.

  2. SEL_ARG GRAPH SEMANTICS

  It represents a condition in a special form (we don't have a name for it ATM)
  The SEL_ARG::next/prev is "OR", and next_key_part is "AND".
  
  For example, the picture represents the condition in form:
   (kp1 < 1 AND kp2=5 AND (kp3=10 OR kp3=12)) OR 
   (kp1=2 AND (kp3=11 OR kp3=14)) OR 
   (kp1=3 AND (kp3=11 OR kp3=14))

  In red-black tree form:

                     +-------+                 +--------+
                     | kp1=2 |.................| kp3=14 |
                     +-------+                 +--------+
                      /     \                     /
             +---------+    +-------+     +--------+
             | kp1 < 1 |    | kp1=3 |     | kp3=11 |
             +---------+    +-------+     +--------+
                 .               .
            ......               .......
            .                          .
        +-------+                  +--------+
        | kp2=5 |                  | kp3=14 |
        +-------+                  +--------+
            .                        /
            .                   +--------+
       (root of R-B tree        | kp3=11 |
        for "kp3={10|12}")      +--------+


  Where / and \ denote left and right pointers and ... denotes
  next_key_part pointers to the root of the R-B tree of intervals for
  consecutive key parts.

  3. SEL_ARG GRAPH USE

  Use get_mm_tree() to construct SEL_ARG graph from WHERE condition.
  Then walk the SEL_ARG graph and get a list of dijsoint ordered key
  intervals (i.e. intervals in form
  
   (constA1, .., const1_K) < (keypart1,.., keypartK) < (constB1, .., constB_K)

  Those intervals can be used to access the index. The uses are in:
   - check_quick_select() - Walk the SEL_ARG graph and find an estimate of
                            how many table records are contained within all
                            intervals.
   - get_quick_select()   - Walk the SEL_ARG, materialize the key intervals,
                            and create QUICK_RANGE_SELECT object that will
                            read records within these intervals.

  4. SPACE COMPLEXITY NOTES 

    SEL_ARG graph is a representation of an ordered disjoint sequence of
    intervals over the ordered set of index tuple values.

    For multi-part keys, one can construct a WHERE expression such that its
    list of intervals will be of combinatorial size. Here is an example:
     
      (keypart1 IN (1,2, ..., n1)) AND 
      (keypart2 IN (1,2, ..., n2)) AND 
      (keypart3 IN (1,2, ..., n3))
    
    For this WHERE clause the list of intervals will have n1*n2*n3 intervals
    of form
     
      (keypart1, keypart2, keypart3) = (k1, k2, k3), where 1 <= k{i} <= n{i}
    
    SEL_ARG graph structure aims to reduce the amount of required space by
    "sharing" the elementary intervals when possible (the pic at the
    beginning of this comment has examples of such sharing). The sharing may 
    prevent combinatorial blowup:

      There are WHERE clauses that have combinatorial-size interval lists but
      will be represented by a compact SEL_ARG graph.
      Example:
        (keypartN IN (1,2, ..., n1)) AND 
        ...
        (keypart2 IN (1,2, ..., n2)) AND 
        (keypart1 IN (1,2, ..., n3))

    but not in all cases:

    - There are WHERE clauses that do have a compact SEL_ARG-graph
      representation but get_mm_tree() and its callees will construct a
      graph of combinatorial size.
      Example:
        (keypart1 IN (1,2, ..., n1)) AND 
        (keypart2 IN (1,2, ..., n2)) AND 
        ...
        (keypartN IN (1,2, ..., n3))

    - There are WHERE clauses for which the minimal possible SEL_ARG graph
      representation will have combinatorial size.
      Example:
        By induction: Let's take any interval on some keypart in the middle:

           kp15=c0
        
        Then let's AND it with this interval 'structure' from preceding and
        following keyparts:

          (kp14=c1 AND kp16=c3) OR keypart14=c2) (*)
        
        We will obtain this SEL_ARG graph:
 
             kp14     $      kp15      $      kp16
                      $                $
         +---------+  $   +---------+  $   +---------+
         | kp14=c1 |--$-->| kp15=c0 |--$-->| kp16=c3 |
         +---------+  $   +---------+  $   +---------+
              |       $                $              
         +---------+  $   +---------+  $             
         | kp14=c2 |--$-->| kp15=c0 |  $             
         +---------+  $   +---------+  $             
                      $                $
                      
       Note that we had to duplicate "kp15=c0" and there was no way to avoid
       that. 
       The induction step: AND the obtained expression with another "wrapping"
       expression like (*).
       When the process ends because of the limit on max. number of keyparts 
       we'll have:

         WHERE clause length  is O(3*#max_keyparts)
         SEL_ARG graph size   is O(2^(#max_keyparts/2))

       (it is also possible to construct a case where instead of 2 in 2^n we
        have a bigger constant, e.g. 4, and get a graph with 4^(31/2)= 2^31
        nodes)

    We avoid consuming too much memory by setting a limit on the number of
    SEL_ARG object we can construct during one range analysis invocation.
*/

class SEL_ARG :public Sql_alloc
{
public:
  uint8 min_flag,max_flag,maybe_flag;
  uint8 part;					// Which key part
  uint8 maybe_null;
  /**
    The rtree index interval to scan, undefined unless
    SEL_ARG::min_flag == GEOM_FLAG.
   */
  enum ha_rkey_function rkey_func_flag;
  /* 
    Number of children of this element in the RB-tree, plus 1 for this
    element itself.
  */
  uint16 elements;
  /**
    Valid only for elements which are RB-tree roots: Number of
    references to this SEL_ARG tree. References may be from
    SEL_ARG::next_key_part of SEL_ARGs from earlier keyparts or
    SEL_TREE::keys[i].

    The SEL_ARGs are re-used in a lazy-copy manner based on this
    reference counting.
  */
  ulong use_count;

  Field *field;
  uchar *min_value,*max_value;			// Pointer to range

  /*
    eq_tree(), first(), last() etc require that left == right == NULL
    if the type is MAYBE_KEY. Todo: fix this so SEL_ARGs without R-B
    children are handled consistently. See related WL#5894.
   */
  SEL_ARG *left,*right;   /* R-B tree children */
  SEL_ARG *next,*prev;    /* Links for bi-directional interval list */
  SEL_ARG *parent;        /* R-B tree parent */
  /*
    R-B tree root of intervals covering keyparts consecutive to this
    SEL_ARG. See documentation of SEL_ARG GRAPH semantics for details.
  */
  SEL_ARG *next_key_part; 
  enum leaf_color { BLACK,RED } color;

  /**
    Used to indicate if the range predicate for an index is always
    true/false, depends on values from other tables or can be
    evaluated as is.
  */
  enum Type {
    /** The range predicate for this index is always false. */
    IMPOSSIBLE,
    /** The range predicate for this index is always true.*/
    ALWAYS,
    /** 
      There is a range predicate that refers to another table. The
      range access method cannot be used on this index unless that
      other table is earlier in the join sequence. The bit
      representing the index is set in SQL_SELECT::needed_reg to
      notify the join optimizer that there is a table dependency.
      After deciding on join order, the optimizer may chose to rerun
      the range optimizer for tables with such dependencies.
    */
    MAYBE_KEY,
    /**
      There is a range condition that can be used on this index. The
      range conditions for this index in stored in the SEL_ARG tree.
    */
    KEY_RANGE
  } type;

  SEL_ARG() {}
  SEL_ARG(SEL_ARG &);
  SEL_ARG(Field *,const uchar *, const uchar *);
  SEL_ARG(Field *field, uint8 part, uchar *min_value, uchar *max_value,
	  uint8 min_flag, uint8 max_flag, uint8 maybe_flag);
  /*
    Used to construct MAYBE_KEY and IMPOSSIBLE SEL_ARGs. left and
    right is NULL, so this ctor must not be used to create other
    SEL_ARG types. See todo for left/right pointers.
  */
  SEL_ARG(enum Type type_arg)
    :min_flag(0), part(0), rkey_func_flag(HA_READ_INVALID), elements(1),
    use_count(1), left(NULL), right(NULL),
    next_key_part(0), color(BLACK), type(type_arg)
  {
    DBUG_ASSERT(type_arg == MAYBE_KEY || type_arg == IMPOSSIBLE);
  }
  /**
    returns true if a range predicate is equal. Use all_same()
    to check for equality of all the predicates on this keypart.
  */
  inline bool is_same(const SEL_ARG *arg) const
  {
    if (type != arg->type || part != arg->part)
      return false;
    if (type != KEY_RANGE)
      return true;
    return cmp_min_to_min(arg) == 0 && cmp_max_to_max(arg) == 0;
  }
  /**
    returns true if all the predicates in the keypart tree are equal
  */
  bool all_same(const SEL_ARG *arg) const
  {
    if (type != arg->type || part != arg->part)
      return false;
    if (type != KEY_RANGE)
      return true;
    if (arg == this)
      return true;
    const SEL_ARG *cmp_arg= arg->first();
    const SEL_ARG *cur_arg= first();
    for (; cur_arg && cmp_arg && cur_arg->is_same(cmp_arg);
         cur_arg= cur_arg->next, cmp_arg= cmp_arg->next) ;
    if (cur_arg || cmp_arg)
      return false;
    return true;
  }
  inline void merge_flags(SEL_ARG *arg) { maybe_flag|=arg->maybe_flag; }
  inline void maybe_smaller() { maybe_flag=1; }
  /* Return true iff it's a single-point null interval */
  inline bool is_null_interval() { return maybe_null && max_value[0] == 1; } 
  inline int cmp_min_to_min(const SEL_ARG* arg) const
  {
    return sel_cmp(field,min_value, arg->min_value, min_flag, arg->min_flag);
  }
  inline int cmp_min_to_max(const SEL_ARG* arg) const
  {
    return sel_cmp(field,min_value, arg->max_value, min_flag, arg->max_flag);
  }
  inline int cmp_max_to_max(const SEL_ARG* arg) const
  {
    return sel_cmp(field,max_value, arg->max_value, max_flag, arg->max_flag);
  }
  inline int cmp_max_to_min(const SEL_ARG* arg) const
  {
    return sel_cmp(field,max_value, arg->min_value, max_flag, arg->min_flag);
  }
  SEL_ARG *clone_and(SEL_ARG* arg, MEM_ROOT *mem_root)
  {						// Get overlapping range
    uchar *new_min,*new_max;
    uint8 flag_min,flag_max;
    if (cmp_min_to_min(arg) >= 0)
    {
      new_min=min_value; flag_min=min_flag;
    }
    else
    {
      new_min=arg->min_value; flag_min=arg->min_flag; /* purecov: deadcode */
    }
    if (cmp_max_to_max(arg) <= 0)
    {
      new_max=max_value; flag_max=max_flag;
    }
    else
    {
      new_max=arg->max_value; flag_max=arg->max_flag;
    }
    return new (mem_root) SEL_ARG(field, part, new_min, new_max, flag_min, flag_max,
		       MY_TEST(maybe_flag && arg->maybe_flag));
  }
  SEL_ARG *clone_first(SEL_ARG *arg, MEM_ROOT *mem_root)
  {						// min <= X < arg->min
    return new (mem_root) SEL_ARG(field,part, min_value, arg->min_value,
		       min_flag, arg->min_flag & NEAR_MIN ? 0 : NEAR_MAX,
		       maybe_flag | arg->maybe_flag);
  }
  SEL_ARG *clone_last(SEL_ARG *arg, MEM_ROOT *mem_root)
  {						// min <= X <= key_max
    return new (mem_root) SEL_ARG(field, part, min_value, arg->max_value,
		       min_flag, arg->max_flag, maybe_flag | arg->maybe_flag);
  }
  SEL_ARG *clone(RANGE_OPT_PARAM *param, SEL_ARG *new_parent, SEL_ARG **next);

  bool copy_min(SEL_ARG* arg)
  {						// Get overlapping range
    if (cmp_min_to_min(arg) > 0)
    {
      min_value=arg->min_value; min_flag=arg->min_flag;
      if ((max_flag & NO_MAX_RANGE) && (min_flag & NO_MIN_RANGE))
	return 1;				// Full range
    }
    maybe_flag|=arg->maybe_flag;
    return 0;
  }
  bool copy_max(SEL_ARG* arg)
  {						// Get overlapping range
    if (cmp_max_to_max(arg) <= 0)
    {
      max_value=arg->max_value; max_flag=arg->max_flag;
      if ((max_flag & NO_MAX_RANGE) && (min_flag & NO_MIN_RANGE))
	return 1;				// Full range
    }
    maybe_flag|=arg->maybe_flag;
    return 0;
  }

  void copy_min_to_min(SEL_ARG *arg)
  {
    min_value=arg->min_value; min_flag=arg->min_flag;
  }
  void copy_min_to_max(SEL_ARG *arg)
  {
    max_value=arg->min_value;
    max_flag=arg->min_flag & NEAR_MIN ? 0 : NEAR_MAX;
  }
  void copy_max_to_min(SEL_ARG *arg)
  {
    min_value=arg->max_value;
    min_flag=arg->max_flag & NEAR_MAX ? 0 : NEAR_MIN;
  }

  /**
    Set spatial index range scan parameters. This object will be used to do
    spatial index range scan after this call.

    @param rkey_func The scan function to perform. It must be one of the
                     spatial index specific scan functions.
   */
  void set_gis_index_read_function(const enum ha_rkey_function rkey_func)
  {
    DBUG_ASSERT(rkey_func >= HA_READ_MBR_CONTAIN &&
                rkey_func <= HA_READ_MBR_EQUAL);
    min_flag= GEOM_FLAG;
    rkey_func_flag= rkey_func;
    max_flag= NO_MAX_RANGE;
  }

  /* returns a number of keypart values (0 or 1) appended to the key buffer */
  int store_min(uint length, uchar **min_key,uint min_key_flag)
  {
    /* "(kp1 > c1) AND (kp2 OP c2) AND ..." -> (kp1 > c1) */
    if ((min_flag & GEOM_FLAG) ||
        (!(min_flag & NO_MIN_RANGE) &&
	!(min_key_flag & (NO_MIN_RANGE | NEAR_MIN))))
    {
      if (maybe_null && *min_value)
      {
	**min_key=1;
	memset(*min_key+1, 0, length-1);
      }
      else
	memcpy(*min_key,min_value,length);
      (*min_key)+= length;
      return 1;
    }
    return 0;
  }
  /* returns a number of keypart values (0 or 1) appended to the key buffer */
  int store_max(uint length, uchar **max_key, uint max_key_flag)
  {
    if (!(max_flag & NO_MAX_RANGE) &&
	!(max_key_flag & (NO_MAX_RANGE | NEAR_MAX)))
    {
      if (maybe_null && *max_value)
      {
	**max_key=1;
	memset(*max_key+1, 0, length-1);
      }
      else
	memcpy(*max_key,max_value,length);
      (*max_key)+= length;
      return 1;
    }
    return 0;
  }

  /*
    Returns a number of keypart values appended to the key buffer
    for min key and max key. This function is used by both Range
    Analysis and Partition pruning. For partition pruning we have
    to ensure that we don't store also subpartition fields. Thus
    we have to stop at the last partition part and not step into
    the subpartition fields. For Range Analysis we set last_part
    to MAX_KEY which we should never reach.
  */
  int store_min_key(KEY_PART *key,
                    uchar **range_key,
                    uint *range_key_flag,
                    uint last_part)
  {
    SEL_ARG *key_tree= first();
    uint res= key_tree->store_min(key[key_tree->part].store_length,
                                  range_key, *range_key_flag);
    *range_key_flag|= key_tree->min_flag;
    
    if (key_tree->next_key_part &&
	key_tree->next_key_part->type == SEL_ARG::KEY_RANGE &&
        key_tree->part != last_part &&
	key_tree->next_key_part->part == key_tree->part+1 &&
	!(*range_key_flag & (NO_MIN_RANGE | NEAR_MIN)))
      res+= key_tree->next_key_part->store_min_key(key,
                                                   range_key,
                                                   range_key_flag,
                                                   last_part);
    return res;
  }

  /* returns a number of keypart values appended to the key buffer */
  int store_max_key(KEY_PART *key,
                    uchar **range_key,
                    uint *range_key_flag,
                    uint last_part)
  {
    SEL_ARG *key_tree= last();
    uint res=key_tree->store_max(key[key_tree->part].store_length,
                                 range_key, *range_key_flag);
    (*range_key_flag)|= key_tree->max_flag;
    if (key_tree->next_key_part &&
	key_tree->next_key_part->type == SEL_ARG::KEY_RANGE &&
        key_tree->part != last_part &&
	key_tree->next_key_part->part == key_tree->part+1 &&
	!(*range_key_flag & (NO_MAX_RANGE | NEAR_MAX)))
      res+= key_tree->next_key_part->store_max_key(key,
                                                   range_key,
                                                   range_key_flag,
                                                   last_part);
    return res;
  }

  SEL_ARG *insert(SEL_ARG *key);
  SEL_ARG *tree_delete(SEL_ARG *key);
  SEL_ARG *find_range(SEL_ARG *key);
  SEL_ARG *rb_insert(SEL_ARG *leaf);
  friend SEL_ARG *rb_delete_fixup(SEL_ARG *root,SEL_ARG *key, SEL_ARG *par);
#ifndef DBUG_OFF
  friend int test_rb_tree(SEL_ARG *element,SEL_ARG *parent);
#endif
  bool test_use_count(SEL_ARG *root);
  SEL_ARG *first();
  const SEL_ARG *first() const;
  SEL_ARG *last();
  void make_root();
  inline bool simple_key()
  {
    return !next_key_part && elements == 1;
  }
  /**
    Update use_count of all SEL_ARG trees for later keyparts to
    reflect that this SEL_ARG tree is now referred to 'count' more
    times than it used to be (either through SEL_TREE::keys[] or
    SEL_ARG::next_key_part pointers).

    This function does NOT update use_count of the current SEL_ARG
    object.

    @param count The number of additional references to this SEL_ARG
                 tree.

    @todo consider refactoring this function to also increase
          use_count of 'this' instead of incrementing use_count only
          on later keyparts.
  */
  void increment_use_count(long count)
  {
    /*
      Increment use_count for all SEL_ARG trees referenced via
      next_key_part from any SEL_ARG in this tree.
    */
    for (SEL_ARG *cur_selarg= first();
         cur_selarg;
         cur_selarg= cur_selarg->next)
    {
      if (cur_selarg->next_key_part)
      {
        cur_selarg->next_key_part->use_count+= count;
        cur_selarg->next_key_part->increment_use_count(count);
      }
    }
  }

  /**
    Update use count for SEL_ARG's next_key_part.
    This function does NOT update use_count of the current
    SEL_ARG object.

    Primarily used for reducing reference count of next_key_part of a
    node when removed from SEL_ARG tree during tree merge operations.

    @param count The number of additional references to this SEL_ARG
                 tree.
  */
  void increment_next_key_part_use_count(long count)
  {
    if (next_key_part)
    {
      next_key_part->use_count+= count;
      next_key_part->increment_use_count(count);
    }
  }

  void free_tree()
  {
    for (SEL_ARG *pos=first(); pos ; pos=pos->next)
      if (pos->next_key_part)
      {
	pos->next_key_part->use_count--;
	pos->next_key_part->free_tree();
      }
  }

  inline SEL_ARG **parent_ptr()
  {
    return parent->left == this ? &parent->left : &parent->right;
  }


  /*
    Check if this SEL_ARG object represents a single-point interval

    SYNOPSIS
      is_singlepoint()
    
    DESCRIPTION
      Check if this SEL_ARG object (not tree) represents a single-point
      interval, i.e. if it represents a "keypart = const" or 
      "keypart IS NULL".

    RETURN
      TRUE   This SEL_ARG object represents a singlepoint interval
      FALSE  Otherwise
  */

  bool is_singlepoint() const
  {
    /* 
      Check for NEAR_MIN ("strictly less") and NO_MIN_RANGE (-inf < field) 
      flags, and the same for right edge.
    */
    if (min_flag || max_flag)
      return FALSE;
    uchar *min_val= min_value;
    uchar *max_val= max_value;

    if (maybe_null)
    {
      /* First byte is a NULL value indicator */
      if (*min_val != *max_val)
        return FALSE;

      if (*min_val)
        return TRUE; /* This "x IS NULL" */
      min_val++;
      max_val++;
    }
    return !field->key_cmp(min_val, max_val);
  }
  SEL_ARG *clone_tree(RANGE_OPT_PARAM *param);
};

/**
  Helper function to compare two SEL_ARG's.
*/
static bool all_same(const SEL_ARG *sa1, const SEL_ARG *sa2)
{
  if (sa1 == NULL && sa2 == NULL)
    return true;
  if ((sa1 != NULL && sa2 == NULL) || (sa1 == NULL && sa2 != NULL))
    return false;
  return sa1->all_same(sa2);
}

class SEL_IMERGE;


class SEL_TREE :public Sql_alloc
{
public:
  /**
    Starting an effort to document this field:

    IMPOSSIBLE: if keys[i]->type == SEL_ARG::IMPOSSIBLE for some i,
      then type == SEL_TREE::IMPOSSIBLE. Rationale: if the predicate for
      one of the indexes is always false, then the full predicate is also
      always false.

    ALWAYS: if either (keys[i]->type == SEL_ARG::ALWAYS) or 
      (keys[i] == NULL) for all i, then type == SEL_TREE::ALWAYS. 
      Rationale: the range access method will not be able to filter
      out any rows when there are no range predicates that can be used
      to filter on any index.

    KEY: There are range predicates that can be used on at least one
      index.
      
    KEY_SMALLER: There are range predicates that can be used on at
      least one index. In addition, there are predicates that cannot
      be directly utilized by range access on key parts in the same
      index. These unused predicates makes it probable that the row
      estimate for range access on this index is too pessimistic.
  */
  enum Type { IMPOSSIBLE, ALWAYS, MAYBE, KEY, KEY_SMALLER } type;

  SEL_TREE(enum Type type_arg, MEM_ROOT *root, size_t num_keys)
    : type(type_arg), keys(root, num_keys), n_ror_scans(0)
  { }
  SEL_TREE(MEM_ROOT *root, size_t num_keys) :
    type(KEY), keys(root, num_keys), n_ror_scans(0)
  { }
  /**
    Constructor that performs deep-copy of the SEL_ARG trees in
    'keys[]' and the index merge alternatives in 'merges'.

    @param arg     The SEL_TREE to copy
    @param param   Parameters for range analysis
  */
  SEL_TREE(SEL_TREE *arg, RANGE_OPT_PARAM *param);
  /*
    Possible ways to read rows using a single index because the
    conditions of the query consists of single-index conjunctions:

       (ranges_for_idx_1) AND (ranges_for_idx_2) AND ...

    The SEL_ARG graph for each non-NULL element in keys[] may consist
    of many single-index ranges (disjunctions), so ranges_for_idx_1
    may e.g. be:

       "idx_field1 = 1 OR (idx_field1 > 5 AND idx_field2 = 10)"

    assuming that index1 is a composite index covering
    (idx_field1,...,idx_field2,..)

    Index merge intersection intersects ranges on SEL_ARGs from two or
    more indexes.

    Note: there may exist SEL_TREE objects with sel_tree->type=KEY and
    keys[i]=0 for all i. (SergeyP: it is not clear whether there is any
    merit in range analyzer functions (e.g. get_mm_parts) returning a
    pointer to such SEL_TREE instead of NULL)
  */
  Mem_root_array<SEL_ARG *, true> keys;
  key_map keys_map;        /* bitmask of non-NULL elements in keys */

  /*
    Possible ways to read rows using Index merge (sort) union.

    Each element in 'merges' consists of multi-index disjunctions,
    which means that Index merge (sort) union must be applied to read
    rows. The nodes in the 'merges' list forms a conjunction of such
    multi-index disjunctions.

    The list is non-empty only if type==KEY.
  */
  List<SEL_IMERGE> merges;

  /* The members below are filled/used only after get_mm_tree is done */
  key_map ror_scans_map;   /* bitmask of ROR scan-able elements in keys */
  uint    n_ror_scans;     /* number of set bits in ror_scans_map */

  struct st_ror_scan_info **ror_scans;     /* list of ROR key scans */
  struct st_ror_scan_info **ror_scans_end; /* last ROR scan */
  /* Note that #records for each key scan is stored in table->quick_rows */
};

class RANGE_OPT_PARAM
{
public:
  THD	*thd;   /* Current thread handle */
  TABLE *table; /* Table being analyzed */
  Item *cond;   /* Used inside get_mm_tree(). */
  table_map prev_tables;
  table_map read_tables;
  table_map current_table; /* Bit of the table being analyzed */

  /* Array of parts of all keys for which range analysis is performed */
  KEY_PART *key_parts;
  KEY_PART *key_parts_end;
  MEM_ROOT *mem_root; /* Memory that will be freed when range analysis completes */
  MEM_ROOT *old_root; /* Memory that will last until the query end */
  /*
    Number of indexes used in range analysis (In SEL_TREE::keys only first
    #keys elements are not empty)
  */
  uint keys;
  
  /* 
    If true, the index descriptions describe real indexes (and it is ok to
    call field->optimize_range(real_keynr[...], ...).
    Otherwise index description describes fake indexes, like a partitioning
    expression.
  */
  bool using_real_indexes;
  
  /*
    Aggressively remove "scans" that do not have conditions on first
    keyparts. Such scans are usable when doing partition pruning but not
    regular range optimization.
  */
  bool remove_jump_scans;
  
  /*
    used_key_no -> table_key_no translation table. Only makes sense if
    using_real_indexes==TRUE
  */
  uint real_keynr[MAX_KEY];

  /*
    Used to store 'current key tuples', in both range analysis and
    partitioning (list) analysis
  */
  uchar min_key[MAX_KEY_LENGTH+MAX_FIELD_WIDTH],
    max_key[MAX_KEY_LENGTH+MAX_FIELD_WIDTH];

  bool force_default_mrr;
  /** 
    Whether index statistics or index dives should be used when
    estimating the number of rows in an equality range. If true, index
    statistics is used for these indexes.
  */
  bool use_index_statistics;

  /// Error handler for this param.

  Range_optimizer_error_handler error_handler;

  bool has_errors() const  { return (error_handler.has_errors()); }

  virtual ~RANGE_OPT_PARAM() {}

};

class PARAM : public RANGE_OPT_PARAM
{
public:
  KEY_PART *key[MAX_KEY]; /* First key parts of keys used in the query */
  longlong baseflag;
  uint max_key_part;
  /* Number of ranges in the last checked tree->key */
  uint range_count;

  bool quick;				// Don't calulate possible keys

  uint fields_bitmap_size;
  MY_BITMAP needed_fields;    /* bitmask of fields needed by the query */
  MY_BITMAP tmp_covered_fields;

  key_map *needed_reg; /* ptr to needed_reg argument of test_quick_select() */

  // Buffer for index_merge cost estimates.
  Unique::Imerge_cost_buf_type imerge_cost_buff;

  /* TRUE if last checked tree->key can be used for ROR-scan */
  bool is_ror_scan;
  /* Number of ranges in the last checked tree->key */
  uint n_ranges;

  /* 
     The sort order the range access method must be able
     to provide. Three-value logic: asc/desc/don't care
  */
  ORDER::enum_order order_direction;

  /// Control whether the various index merge strategies are allowed
  bool index_merge_allowed;
  bool index_merge_union_allowed;
  bool index_merge_sort_union_allowed;
  bool index_merge_intersect_allowed;
};

class TABLE_READ_PLAN;
  class TRP_RANGE;
  class TRP_ROR_INTERSECT;
  class TRP_ROR_UNION;
  class TRP_INDEX_MERGE;
  class TRP_GROUP_MIN_MAX;

struct st_ror_scan_info;

static SEL_TREE * get_mm_parts(RANGE_OPT_PARAM *param,
                               Item_func *cond_func,Field *field,
                               Item_func::Functype type,Item *value,
                               Item_result cmp_type);
static SEL_ARG *get_mm_leaf(RANGE_OPT_PARAM *param,Item *cond_func,Field *field,
			    KEY_PART *key_part,
			    Item_func::Functype type,Item *value);
static SEL_TREE *get_mm_tree(RANGE_OPT_PARAM *param,Item *cond);

static bool is_key_scan_ror(PARAM *param, uint keynr, uint nparts);
static ha_rows check_quick_select(PARAM *param, uint idx, bool index_only,
                                  SEL_ARG *tree, bool update_tbl_stats, 
                                  uint *mrr_flags, uint *bufsize,
                                  Cost_estimate *cost);
QUICK_RANGE_SELECT *get_quick_select(PARAM *param,uint index,
                                     SEL_ARG *key_tree, uint mrr_flags, 
                                     uint mrr_buf_size, MEM_ROOT *alloc);
static TRP_RANGE *get_key_scans_params(PARAM *param, SEL_TREE *tree,
                                       bool index_read_must_be_used,
                                       bool update_tbl_stats,
                                       const Cost_estimate *cost_est);
static
TRP_ROR_INTERSECT *get_best_ror_intersect(const PARAM *param, SEL_TREE *tree,
                                          const Cost_estimate *cost_est);
static
TABLE_READ_PLAN *get_best_disjunct_quick(PARAM *param, SEL_IMERGE *imerge,
                                         const Cost_estimate *cost_est);
static
TRP_GROUP_MIN_MAX *get_best_group_min_max(PARAM *param, SEL_TREE *tree,
                                          const Cost_estimate *cost_est);
#ifndef DBUG_OFF
static void print_sel_tree(PARAM *param, SEL_TREE *tree, key_map *tree_map,
                           const char *msg);
static void print_ror_scans_arr(TABLE *table, const char *msg,
                                struct st_ror_scan_info **start,
                                struct st_ror_scan_info **end);
static void print_quick(QUICK_SELECT_I *quick, const key_map *needed_reg);
#endif

static void append_range_all_keyparts(Opt_trace_array *range_trace,
                                      String *range_string,
                                      String *range_so_far,
                                      SEL_ARG *keypart_root,
                                      const KEY_PART_INFO *key_parts,
                                      const bool print_full);
static inline void dbug_print_tree(const char *tree_name,
                                   SEL_TREE *tree,
                                   const RANGE_OPT_PARAM *param);

static inline void print_tree(String *out,
                              const char *tree_name,
                              SEL_TREE *tree,
                              const RANGE_OPT_PARAM *param,
                              const bool print_full) MY_ATTRIBUTE((unused));

void append_range(String *out,
                  const KEY_PART_INFO *key_parts,
                  const uchar *min_key, const uchar *max_key,
                  const uint flag);

static SEL_TREE *tree_and(RANGE_OPT_PARAM *param,SEL_TREE *tree1,SEL_TREE *tree2);
static SEL_TREE *tree_or(RANGE_OPT_PARAM *param,SEL_TREE *tree1,SEL_TREE *tree2);
/*
  A null_sel_tree is used in get_func_mm_tree_from_in_predicate to pass
  as an argument to tree_or. It is used only to influence the return
  value from tree_or function.
*/

static MEM_ROOT null_root;
static SEL_TREE null_sel_tree(SEL_TREE::IMPOSSIBLE, &null_root, 0);


static SEL_ARG *sel_add(SEL_ARG *key1,SEL_ARG *key2);
static SEL_ARG *key_or(RANGE_OPT_PARAM *param, SEL_ARG *key1, SEL_ARG *key2);
static SEL_ARG *key_and(RANGE_OPT_PARAM *param, SEL_ARG *key1, SEL_ARG *key2,
                        uint clone_flag);
static bool get_range(SEL_ARG **e1,SEL_ARG **e2,SEL_ARG *root1);
bool get_quick_keys(PARAM *param,QUICK_RANGE_SELECT *quick,KEY_PART *key,
                    SEL_ARG *key_tree, uchar *min_key,uint min_key_flag,
                    uchar *max_key,uint max_key_flag);
static bool eq_tree(SEL_ARG* a,SEL_ARG *b);
static bool eq_ranges_exceeds_limit(SEL_ARG *keypart_root, uint* count, 
                                    uint limit);

static SEL_ARG null_element(SEL_ARG::IMPOSSIBLE);
static bool null_part_in_key(KEY_PART *key_part, const uchar *key,
                             uint length);
bool sel_trees_can_be_ored(SEL_TREE *tree1, SEL_TREE *tree2, RANGE_OPT_PARAM* param);


/*
  SEL_IMERGE is a list of possible ways to do index merge, i.e. it is
  a condition in the following form:
   (t_1||t_2||...||t_N) && (next)

  where all t_i are SEL_TREEs, next is another SEL_IMERGE and no pair
  (t_i,t_j) contains SEL_ARGS for the same index.

  SEL_TREE contained in SEL_IMERGE always has merges=NULL.

  This class relies on memory manager to do the cleanup.
*/

class SEL_IMERGE : public Sql_alloc
{
  enum { PREALLOCED_TREES= 10};
public:
  SEL_TREE *trees_prealloced[PREALLOCED_TREES];
  SEL_TREE **trees;             /* trees used to do index_merge   */
  SEL_TREE **trees_next;        /* last of these trees            */
  SEL_TREE **trees_end;         /* end of allocated space         */

  SEL_ARG  ***best_keys;        /* best keys to read in SEL_TREEs */

  SEL_IMERGE() :
    trees(&trees_prealloced[0]),
    trees_next(trees),
    trees_end(trees + PREALLOCED_TREES)
  {}
  SEL_IMERGE (SEL_IMERGE *arg, RANGE_OPT_PARAM *param);
  int or_sel_tree(RANGE_OPT_PARAM *param, SEL_TREE *tree);
  int or_sel_tree_with_checks(RANGE_OPT_PARAM *param, SEL_TREE *new_tree);
  int or_sel_imerge_with_checks(RANGE_OPT_PARAM *param, SEL_IMERGE* imerge);
};


/*
  Add SEL_TREE to this index_merge without any checks,

  NOTES
    This function implements the following:
      (x_1||...||x_N) || t = (x_1||...||x_N||t), where x_i, t are SEL_TREEs

  RETURN
     0 - OK
    -1 - Out of memory.
*/

int SEL_IMERGE::or_sel_tree(RANGE_OPT_PARAM *param, SEL_TREE *tree)
{
  if (trees_next == trees_end)
  {
    const int realloc_ratio= 2;		/* Double size for next round */
    uint old_elements= static_cast<uint>(trees_end - trees);
    uint old_size= sizeof(SEL_TREE**) * old_elements;
    uint new_size= old_size * realloc_ratio;
    SEL_TREE **new_trees;
    if (!(new_trees= (SEL_TREE**)alloc_root(param->mem_root, new_size)))
      return -1;
    memcpy(new_trees, trees, old_size);
    trees=      new_trees;
    trees_next= trees + old_elements;
    trees_end=  trees + old_elements * realloc_ratio;
  }
  *(trees_next++)= tree;
  return 0;
}


/*
  Perform OR operation on this SEL_IMERGE and supplied SEL_TREE new_tree,
  combining new_tree with one of the trees in this SEL_IMERGE if they both
  have SEL_ARGs for the same key.

  SYNOPSIS
    or_sel_tree_with_checks()
      param    PARAM from test_quick_select
      new_tree SEL_TREE with type KEY or KEY_SMALLER.

  NOTES
    This does the following:
    (t_1||...||t_k)||new_tree =
     either
       = (t_1||...||t_k||new_tree)
     or
       = (t_1||....||(t_j|| new_tree)||...||t_k),

     where t_i, y are SEL_TREEs.
    new_tree is combined with the first t_j it has a SEL_ARG on common
    key with. As a consequence of this, choice of keys to do index_merge
    read may depend on the order of conditions in WHERE part of the query.

  RETURN
    0  OK
    1  One of the trees was combined with new_tree to SEL_TREE::ALWAYS,
       and (*this) should be discarded.
   -1  An error occurred.
*/

int SEL_IMERGE::or_sel_tree_with_checks(RANGE_OPT_PARAM *param, SEL_TREE *new_tree)
{
  DBUG_ENTER("SEL_IMERGE::or_sel_tree_with_checks");
  for (SEL_TREE** tree = trees;
       tree != trees_next;
       tree++)
  {
    if (sel_trees_can_be_ored(*tree, new_tree, param))
    {
      *tree = tree_or(param, *tree, new_tree);
      if (!*tree)
        DBUG_RETURN(1);
      if (((*tree)->type == SEL_TREE::MAYBE) ||
          ((*tree)->type == SEL_TREE::ALWAYS))
        DBUG_RETURN(1);
      /* SEL_TREE::IMPOSSIBLE is impossible here */
      DBUG_RETURN(0);
    }
  }

  /* New tree cannot be combined with any of existing trees. */
  const int ret= or_sel_tree(param, new_tree);
  DBUG_RETURN(ret);
}


/*
  Perform OR operation on this index_merge and supplied index_merge list.

  RETURN
    0 - OK
    1 - One of conditions in result is always TRUE and this SEL_IMERGE
        should be discarded.
   -1 - An error occurred
*/

int SEL_IMERGE::or_sel_imerge_with_checks(RANGE_OPT_PARAM *param, SEL_IMERGE* imerge)
{
  for (SEL_TREE** tree= imerge->trees;
       tree != imerge->trees_next;
       tree++)
  {
    if (or_sel_tree_with_checks(param, *tree))
      return 1;
  }
  return 0;
}


SEL_TREE::SEL_TREE(SEL_TREE *arg, RANGE_OPT_PARAM *param):
  Sql_alloc(), keys(param->mem_root, param->keys), n_ror_scans(0)
{
  keys_map= arg->keys_map;
  type= arg->type;
  for (uint idx= 0; idx < param->keys; idx++)
  {
    if (arg->keys[idx])
    {
      keys[idx]= arg->keys[idx]->clone_tree(param);
      if (!keys[idx])
        break;
      keys[idx]->use_count++;
      keys[idx]->increment_use_count(1);
    }
    else
      keys[idx]= NULL;
  }

  List_iterator<SEL_IMERGE> it(arg->merges);
  for (SEL_IMERGE *el= it++; el; el= it++)
  {
    SEL_IMERGE *merge= new (param->mem_root) SEL_IMERGE(el, param);
    if (!merge || merge->trees == merge->trees_next ||
        param->has_errors())
    {
      merges.empty();
      return;
    }
    merges.push_back (merge);
  }

  /*
    SEL_TREEs are only created by get_mm_tree() (and functions called
    by get_mm_tree()). Index intersection is checked after
    get_mm_tree() has constructed all ranges. In other words, there
    should not be any ROR scans to copy when this ctor is called.
  */
  DBUG_ASSERT(n_ror_scans == 0);
}


SEL_IMERGE::SEL_IMERGE (SEL_IMERGE *arg, RANGE_OPT_PARAM *param) : Sql_alloc()
{
  uint elements= static_cast<uint>(arg->trees_end - arg->trees);
  if (elements > PREALLOCED_TREES)
  {
    uint size= elements * sizeof (SEL_TREE **);
    if (!(trees= (SEL_TREE **)alloc_root(param->mem_root, size)))
      goto mem_err;
  }
  else
    trees= &trees_prealloced[0];

  trees_next= trees;
  trees_end= trees + elements;

  for (SEL_TREE **tree = trees, **arg_tree= arg->trees; tree < trees_end; 
       tree++, arg_tree++)
  {
    if (!(*tree= new (param->mem_root) SEL_TREE(*arg_tree, param)) ||
        param->has_errors())
      goto mem_err;
  }

  return;

mem_err:
  trees= &trees_prealloced[0];
  trees_next= trees;
  trees_end= trees;
}


/*
  Perform AND operation on two index_merge lists and store result in *im1.
*/

inline void imerge_list_and_list(List<SEL_IMERGE> *im1, List<SEL_IMERGE> *im2)
{
  im1->concat(im2);
}


/*
  Perform OR operation on 2 index_merge lists, storing result in first list.

  NOTES
    The following conversion is implemented:
     (a_1 &&...&& a_N)||(b_1 &&...&& b_K) = AND_i,j(a_i || b_j) =>
      => (a_1||b_1).

    i.e. all conjuncts except the first one are currently dropped.
    This is done to avoid producing N*K ways to do index_merge.

    If (a_1||b_1) produce a condition that is always TRUE, NULL is returned
    and index_merge is discarded (while it is actually possible to try
    harder).

    As a consequence of this, choice of keys to do index_merge read may depend
    on the order of conditions in WHERE part of the query.

  RETURN
    0     OK, result is stored in *im1
    other Error, both passed lists are unusable
*/

int imerge_list_or_list(RANGE_OPT_PARAM *param,
                        List<SEL_IMERGE> *im1,
                        List<SEL_IMERGE> *im2)
{
  SEL_IMERGE *imerge= im1->head();
  im1->empty();
  im1->push_back(imerge);

  return imerge->or_sel_imerge_with_checks(param, im2->head());
}


/*
  Perform OR operation on index_merge list and key tree.

  RETURN
    false     OK, result is stored in *im1.
    true      Error
*/

static bool imerge_list_or_tree(RANGE_OPT_PARAM *param,
                                List<SEL_IMERGE> *im1,
                                SEL_TREE *tree)
{
  DBUG_ENTER("imerge_list_or_tree");
  SEL_IMERGE *imerge;
  List_iterator<SEL_IMERGE> it(*im1);
  
  uint remaining_trees= im1->elements;
  while ((imerge= it++))
  {
    SEL_TREE *or_tree;
    /*
      Need to make a copy of 'tree' for all but the last OR operation
      because or_sel_tree_with_checks() may change it.
    */
    if (--remaining_trees == 0)
      or_tree= tree;
    else
    {
      or_tree= new (param->mem_root) SEL_TREE (tree, param);
      if (!or_tree || param->has_errors())
        DBUG_RETURN(true);
      if (or_tree->keys_map.is_clear_all() && or_tree->merges.is_empty())
        DBUG_RETURN(false);
    }

    int result_or= imerge->or_sel_tree_with_checks(param, or_tree);
    if (result_or == 1)
      it.remove();
    else if (result_or == -1)
      DBUG_RETURN(true);
  }
  DBUG_ASSERT(remaining_trees == 0);
  DBUG_RETURN(im1->is_empty());
}


#undef index					// Fix for Unixware 7

QUICK_SELECT_I::QUICK_SELECT_I()
  :max_used_key_length(0),
   used_key_parts(0)
{}

void QUICK_SELECT_I::trace_quick_description(Opt_trace_context *trace)
{
  Opt_trace_object range_trace(trace, "range_details");

  String range_info;
  range_info.set_charset(system_charset_info);
  add_info_string(&range_info);
  range_trace.add_utf8("used_index", range_info.ptr(), range_info.length());
}

QUICK_RANGE_SELECT::QUICK_RANGE_SELECT(THD *thd, TABLE *table, uint key_nr,
                                       bool no_alloc, MEM_ROOT *parent_alloc,
                                       bool *create_error)
  :ranges(key_memory_Quick_ranges), free_file(0), cur_range(NULL), last_range(0),
   mrr_flags(0), mrr_buf_size(0), mrr_buf_desc(NULL),
   dont_free(0)
{
  my_bitmap_map *bitmap;
  DBUG_ENTER("QUICK_RANGE_SELECT::QUICK_RANGE_SELECT");

  in_ror_merged_scan= 0;
  index= key_nr;
  head=  table;
  key_part_info= head->key_info[index].key_part;

  /* 'thd' is not accessible in QUICK_RANGE_SELECT::reset(). */
  mrr_buf_size= thd->variables.read_rnd_buff_size;

  if (!no_alloc && !parent_alloc)
  {
    // Allocates everything through the internal memroot
    init_sql_alloc(key_memory_quick_range_select_root,
                   &alloc, thd->variables.range_alloc_block_size, 0);
    thd->mem_root= &alloc;
  }
  else
    memset(&alloc, 0, sizeof(alloc));
  file= head->file;
  record= head->record[0];

  /* Allocate a bitmap for used columns (Q: why not on MEM_ROOT?) */
  if (!(bitmap= (my_bitmap_map*) my_malloc(key_memory_my_bitmap_map,
                                           head->s->column_bitmap_size,
                                           MYF(MY_WME))))
  {
    column_bitmap.bitmap= 0;
    *create_error= 1;
  }
  else
    bitmap_init(&column_bitmap, bitmap, head->s->fields, FALSE);
  DBUG_VOID_RETURN;
}


void QUICK_RANGE_SELECT::need_sorted_output()
{
  mrr_flags |= HA_MRR_SORTED;
}


int QUICK_RANGE_SELECT::init()
{
  DBUG_ENTER("QUICK_RANGE_SELECT::init");

  if (file->inited)
    file->ha_index_or_rnd_end();
  DBUG_RETURN(FALSE);
}


void QUICK_RANGE_SELECT::range_end()
{
  if (file->inited)
    file->ha_index_or_rnd_end();
}


QUICK_RANGE_SELECT::~QUICK_RANGE_SELECT()
{
  DBUG_ENTER("QUICK_RANGE_SELECT::~QUICK_RANGE_SELECT");
  if (!dont_free)
  {
    /* file is NULL for CPK scan on covering ROR-intersection */
    if (file) 
    {
      range_end();
      if (free_file)
      {
        DBUG_PRINT("info", ("Freeing separate handler %p (free: %d)", file,
                            free_file));
        file->ha_external_lock(current_thd, F_UNLCK);
        file->ha_close();
        delete file;
      }
    }
    free_root(&alloc,MYF(0));
    my_free(column_bitmap.bitmap);
  }
  my_free(mrr_buf_desc);
  DBUG_VOID_RETURN;
}


QUICK_INDEX_MERGE_SELECT::QUICK_INDEX_MERGE_SELECT(THD *thd_param,
                                                   TABLE *table)
  :unique(NULL), pk_quick_select(NULL), thd(thd_param)
{
  DBUG_ENTER("QUICK_INDEX_MERGE_SELECT::QUICK_INDEX_MERGE_SELECT");
  index= MAX_KEY;
  head= table;

  init_sql_alloc(key_memory_quick_index_merge_root,
                 &alloc, thd->variables.range_alloc_block_size, 0);
  DBUG_VOID_RETURN;
}

int QUICK_INDEX_MERGE_SELECT::init()
{
  DBUG_ENTER("QUICK_INDEX_MERGE_SELECT::init");
  DBUG_RETURN(0);
}

int QUICK_INDEX_MERGE_SELECT::reset()
{
  DBUG_ENTER("QUICK_INDEX_MERGE_SELECT::reset");
  const int retval= read_keys_and_merge();
  DBUG_RETURN(retval);
}

bool
QUICK_INDEX_MERGE_SELECT::push_quick_back(QUICK_RANGE_SELECT *quick_sel_range)
{
  /*
    Save quick_select that does scan on clustered primary key as it will be
    processed separately.
  */
  if (head->file->primary_key_is_clustered() &&
      quick_sel_range->index == head->s->primary_key)
    pk_quick_select= quick_sel_range;
  else
    return quick_selects.push_back(quick_sel_range);
  return 0;
}

QUICK_INDEX_MERGE_SELECT::~QUICK_INDEX_MERGE_SELECT()
{
  List_iterator_fast<QUICK_RANGE_SELECT> quick_it(quick_selects);
  QUICK_RANGE_SELECT* quick;
  DBUG_ENTER("QUICK_INDEX_MERGE_SELECT::~QUICK_INDEX_MERGE_SELECT");
  delete unique;
  quick_it.rewind();
  while ((quick= quick_it++))
    quick->file= NULL;
  quick_selects.delete_elements();
  delete pk_quick_select;
  /* It's ok to call the next two even if they are already deinitialized */
  end_read_record(&read_record);
  free_io_cache(head);
  free_root(&alloc,MYF(0));
  DBUG_VOID_RETURN;
}


QUICK_ROR_INTERSECT_SELECT::QUICK_ROR_INTERSECT_SELECT(THD *thd_param,
                                                       TABLE *table,
                                                       bool retrieve_full_rows,
                                                       MEM_ROOT *parent_alloc)
  : cpk_quick(NULL), thd(thd_param), need_to_fetch_row(retrieve_full_rows),
    scans_inited(FALSE)
{
  index= MAX_KEY;
  head= table;
  record= head->record[0];
  if (!parent_alloc)
    init_sql_alloc(key_memory_quick_ror_intersect_select_root,
                   &alloc, thd->variables.range_alloc_block_size, 0);
  else
    memset(&alloc, 0, sizeof(MEM_ROOT));
  last_rowid= (uchar*) alloc_root(parent_alloc? parent_alloc : &alloc,
                                  head->file->ref_length);
}


/*
  Do post-constructor initialization.
  SYNOPSIS
    QUICK_ROR_INTERSECT_SELECT::init()

  RETURN
    0      OK
    other  Error code
*/

int QUICK_ROR_INTERSECT_SELECT::init()
{
  DBUG_ENTER("QUICK_ROR_INTERSECT_SELECT::init");
 /* Check if last_rowid was successfully allocated in ctor */
  DBUG_RETURN(!last_rowid);
}


/*
  Initialize this quick select to be a ROR-merged scan.

  SYNOPSIS
    QUICK_RANGE_SELECT::init_ror_merged_scan()
      reuse_handler If TRUE, use head->file, otherwise create a separate
                    handler object

  NOTES
    This function creates and prepares for subsequent use a separate handler
    object if it can't reuse head->file. The reason for this is that during
    ROR-merge several key scans are performed simultaneously, and a single
    handler is only capable of preserving context of a single key scan.

    In ROR-merge the quick select doing merge does full records retrieval,
    merged quick selects read only keys.

  RETURN
    0  ROR child scan initialized, ok to use.
    1  error
*/

int QUICK_RANGE_SELECT::init_ror_merged_scan(bool reuse_handler)
{
  handler *save_file= file, *org_file;
  THD *thd;
  MY_BITMAP * const save_read_set= head->read_set;
  MY_BITMAP * const save_write_set= head->write_set;
  DBUG_ENTER("QUICK_RANGE_SELECT::init_ror_merged_scan");

  in_ror_merged_scan= 1;
  mrr_flags|= HA_MRR_SORTED;
  if (reuse_handler)
  {
    DBUG_PRINT("info", ("Reusing handler %p", file));
    if (init() || reset())
    {
      DBUG_RETURN(1);
    }
    head->column_bitmaps_set(&column_bitmap, &column_bitmap);
    file->extra(HA_EXTRA_SECONDARY_SORT_ROWID);
    goto end;
  }

  /* Create a separate handler object for this quick select */
  if (free_file)
  {
    /* already have own 'handler' object. */
    DBUG_RETURN(0);
  }

  thd= head->in_use;
  if (!(file= head->file->clone(head->s->normalized_path.str, thd->mem_root)))
  {
    /* 
      Manually set the error flag. Note: there seems to be quite a few
      places where a failure could cause the server to "hang" the client by
      sending no response to a query. ATM those are not real errors because 
      the storage engine calls in question happen to never fail with the 
      existing storage engines. 
    */
    my_error(ER_OUT_OF_RESOURCES, MYF(0)); /* purecov: inspected */
    /* Caller will free the memory */
    goto failure;  /* purecov: inspected */
  }

  head->column_bitmaps_set(&column_bitmap, &column_bitmap);

  if (file->ha_external_lock(thd, F_RDLCK))
    goto failure;

  if (init() || reset())
  {
    file->ha_external_lock(thd, F_UNLCK);
    file->ha_close();
    goto failure;
  }
  free_file= TRUE;
  last_rowid= file->ref;
  file->extra(HA_EXTRA_SECONDARY_SORT_ROWID);

end:
  /*
    We are only going to read key fields and call position() on 'file'
    The following sets head->tmp_set to only use this key and then updates
    head->read_set and head->write_set to use this bitmap.
    The now bitmap is stored in 'column_bitmap' which is used in ::get_next()
  */
  org_file= head->file;
  head->file= file;
  /* We don't have to set 'head->keyread' here as the 'file' is unique */
  if (!head->no_keyread)
    head->mark_columns_used_by_index(index);
  head->prepare_for_position();
  head->file= org_file;
  bitmap_copy(&column_bitmap, head->read_set);

  /*
    We have prepared a column_bitmap which get_next() will use. To do this we
    used TABLE::read_set/write_set as playground; restore them to their
    original value to not pollute other scans.
  */
  head->column_bitmaps_set(save_read_set, save_write_set);
  bitmap_clear_all(&head->tmp_set);

  DBUG_RETURN(0);

failure:
  head->column_bitmaps_set(save_read_set, save_write_set);
  delete file;
  file= save_file;
  DBUG_RETURN(1);
}


/*
  Initialize this quick select to be a part of a ROR-merged scan.
  SYNOPSIS
    QUICK_ROR_INTERSECT_SELECT::init_ror_merged_scan()
      reuse_handler If TRUE, use head->file, otherwise create separate
                    handler object.
  RETURN
    0     OK
    other error code
*/
int QUICK_ROR_INTERSECT_SELECT::init_ror_merged_scan(bool reuse_handler)
{
  int error;
  List_iterator_fast<QUICK_RANGE_SELECT> quick_it(quick_selects);
  QUICK_RANGE_SELECT* quick;
  DBUG_ENTER("QUICK_ROR_INTERSECT_SELECT::init_ror_merged_scan");

  /* Initialize all merged "children" quick selects */
  DBUG_ASSERT(!need_to_fetch_row || reuse_handler);
  if (!need_to_fetch_row && reuse_handler)
  {
    quick= quick_it++;
    /*
      There is no use of this->file. Use it for the first of merged range
      selects.
    */
    int error= quick->init_ror_merged_scan(TRUE);
    if (error)
      DBUG_RETURN(error);
    quick->file->extra(HA_EXTRA_KEYREAD_PRESERVE_FIELDS);
  }
  while ((quick= quick_it++))
  {
#ifndef DBUG_OFF
    const MY_BITMAP * const save_read_set= quick->head->read_set;
    const MY_BITMAP * const save_write_set= quick->head->write_set;
#endif
    if ((error= quick->init_ror_merged_scan(FALSE)))
      DBUG_RETURN(error);
    quick->file->extra(HA_EXTRA_KEYREAD_PRESERVE_FIELDS);
    // Sets are shared by all members of "quick_selects" so must not change
    DBUG_ASSERT(quick->head->read_set == save_read_set);
    DBUG_ASSERT(quick->head->write_set == save_write_set);
    /* All merged scans share the same record buffer in intersection. */
    quick->record= head->record[0];
  }

  /* Prepare for ha_rnd_pos calls if needed. */
  if (need_to_fetch_row && (error= head->file->ha_rnd_init(false)))
  {
    DBUG_PRINT("error", ("ROR index_merge rnd_init call failed"));
    DBUG_RETURN(error);
  }
  DBUG_RETURN(0);
}


/*
  Initialize quick select for row retrieval.
  SYNOPSIS
    reset()
  RETURN
    0      OK
    other  Error code
*/

int QUICK_ROR_INTERSECT_SELECT::reset()
{
  DBUG_ENTER("QUICK_ROR_INTERSECT_SELECT::reset");
  if (!scans_inited && init_ror_merged_scan(TRUE))
    DBUG_RETURN(1);
  scans_inited= TRUE;
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  QUICK_RANGE_SELECT *quick;
  while ((quick= it++))
    quick->reset();
  DBUG_RETURN(0);
}


/*
  Add a merged quick select to this ROR-intersection quick select.

  SYNOPSIS
    QUICK_ROR_INTERSECT_SELECT::push_quick_back()
      quick Quick select to be added. The quick select must return
            rows in rowid order.
  NOTES
    This call can only be made before init() is called.

  RETURN
    FALSE OK
    TRUE  Out of memory.
*/

bool
QUICK_ROR_INTERSECT_SELECT::push_quick_back(QUICK_RANGE_SELECT *quick)
{
  return quick_selects.push_back(quick);
}

QUICK_ROR_INTERSECT_SELECT::~QUICK_ROR_INTERSECT_SELECT()
{
  DBUG_ENTER("QUICK_ROR_INTERSECT_SELECT::~QUICK_ROR_INTERSECT_SELECT");
  quick_selects.delete_elements();
  delete cpk_quick;
  free_root(&alloc,MYF(0));
  if (need_to_fetch_row && head->file->inited)
    head->file->ha_rnd_end();
  DBUG_VOID_RETURN;
}


QUICK_ROR_UNION_SELECT::QUICK_ROR_UNION_SELECT(THD *thd_param,
                                               TABLE *table)
  : queue(Quick_ror_union_less(this),
          Malloc_allocator<PSI_memory_key>(PSI_INSTRUMENT_ME)),
    thd(thd_param), scans_inited(FALSE)
{
  index= MAX_KEY;
  head= table;
  rowid_length= table->file->ref_length;
  record= head->record[0];
  init_sql_alloc(key_memory_quick_ror_union_select_root,
                 &alloc, thd->variables.range_alloc_block_size, 0);
  thd_param->mem_root= &alloc;
}


/*
  Do post-constructor initialization.
  SYNOPSIS
    QUICK_ROR_UNION_SELECT::init()

  RETURN
    0      OK
    other  Error code
*/

int QUICK_ROR_UNION_SELECT::init()
{
  DBUG_ENTER("QUICK_ROR_UNION_SELECT::init");
  if (queue.reserve(quick_selects.elements))
  {
    DBUG_RETURN(1);
  }

  if (!(cur_rowid= (uchar*) alloc_root(&alloc, 2*head->file->ref_length)))
    DBUG_RETURN(1);
  prev_rowid= cur_rowid + head->file->ref_length;
  DBUG_RETURN(0);
}


/*
  Initialize quick select for row retrieval.
  SYNOPSIS
    reset()

  RETURN
    0      OK
    other  Error code
*/

int QUICK_ROR_UNION_SELECT::reset()
{
  QUICK_SELECT_I *quick;
  int error;
  DBUG_ENTER("QUICK_ROR_UNION_SELECT::reset");
  have_prev_rowid= FALSE;
  if (!scans_inited)
  {
    List_iterator_fast<QUICK_SELECT_I> it(quick_selects);
    while ((quick= it++))
    {
      if (quick->init_ror_merged_scan(FALSE))
        DBUG_RETURN(1);
    }
    scans_inited= TRUE;
  }
  queue.clear();
  /*
    Initialize scans for merged quick selects and put all merged quick
    selects into the queue.
  */
  List_iterator_fast<QUICK_SELECT_I> it(quick_selects);
  while ((quick= it++))
  {
    if ((error= quick->reset()))
      DBUG_RETURN(error);
    if ((error= quick->get_next()))
    {
      if (error == HA_ERR_END_OF_FILE)
        continue;
      DBUG_RETURN(error);
    }
    quick->save_last_pos();
    queue.push(quick);
  }

  /* Prepare for ha_rnd_pos calls. */
  if (head->file->inited && (error= head->file->ha_rnd_end()))
  {
    DBUG_PRINT("error", ("ROR index_merge rnd_end call failed"));
    DBUG_RETURN(error);
  }
  if ((error= head->file->ha_rnd_init(false)))
  {
    DBUG_PRINT("error", ("ROR index_merge rnd_init call failed"));
    DBUG_RETURN(error);
  }

  DBUG_RETURN(0);
}


bool
QUICK_ROR_UNION_SELECT::push_quick_back(QUICK_SELECT_I *quick_sel_range)
{
  return quick_selects.push_back(quick_sel_range);
}

QUICK_ROR_UNION_SELECT::~QUICK_ROR_UNION_SELECT()
{
  DBUG_ENTER("QUICK_ROR_UNION_SELECT::~QUICK_ROR_UNION_SELECT");
  quick_selects.delete_elements();
  if (head->file->inited)
    head->file->ha_rnd_end();
  free_root(&alloc,MYF(0));
  DBUG_VOID_RETURN;
}


QUICK_RANGE::QUICK_RANGE()
  :min_key(0),max_key(0),min_length(0),max_length(0),
  flag(NO_MIN_RANGE | NO_MAX_RANGE), rkey_func_flag(HA_READ_INVALID),
  min_keypart_map(0), max_keypart_map(0)
{}

QUICK_RANGE::QUICK_RANGE(const uchar *min_key_arg, uint min_length_arg,
                         key_part_map min_keypart_map_arg,
                         const uchar *max_key_arg, uint max_length_arg,
                         key_part_map max_keypart_map_arg, uint flag_arg,
                         enum ha_rkey_function rkey_func_flag_arg)
  :min_key(NULL),
  max_key(NULL),
  min_length((uint16) min_length_arg),
  max_length((uint16) max_length_arg),
  flag((uint16) flag_arg),
  rkey_func_flag(rkey_func_flag_arg),
  min_keypart_map(min_keypart_map_arg),
  max_keypart_map(max_keypart_map_arg)
{
  min_key= static_cast<uchar*>(sql_memdup(min_key_arg, min_length_arg + 1));
  max_key= static_cast<uchar*>(sql_memdup(max_key_arg, max_length_arg + 1));
  // If we get is_null_string as argument, the memdup is undefined behavior.
  DBUG_ASSERT(min_key_arg != is_null_string);
  DBUG_ASSERT(max_key_arg != is_null_string);
}

SEL_ARG::SEL_ARG(SEL_ARG &arg)
  :Sql_alloc(),
  min_flag(arg.min_flag),
  max_flag(arg.max_flag),
  maybe_flag(arg.maybe_flag),
  part(arg.part),
  maybe_null(arg.maybe_null),
  rkey_func_flag(arg.rkey_func_flag),
  elements(1),
  use_count(1),
  field(arg.field),
  min_value(arg.min_value),
  max_value(arg.max_value),
  left(&null_element),
  right(&null_element),
  next(NULL),
  prev(NULL),
  next_key_part(arg.next_key_part),
  type(arg.type)
{
  DBUG_ASSERT(arg.type != MAYBE_KEY);  // Would need left=right=NULL
}


inline void SEL_ARG::make_root()
{
  left=right= &null_element;
  color=BLACK;
  next=prev= NULL;
  use_count=0; elements=1;
}

SEL_ARG::SEL_ARG(Field *f,const uchar *min_value_arg,
                 const uchar *max_value_arg)
  :min_flag(0), max_flag(0), maybe_flag(0), part(0),
  maybe_null(f->real_maybe_null()), rkey_func_flag(HA_READ_INVALID),
  elements(1), use_count(1), field(f),
  min_value(const_cast<uchar *>(min_value_arg)),
  max_value(const_cast<uchar *>(max_value_arg)),
  left(&null_element), right(&null_element),
  next(NULL), prev(NULL),
  next_key_part(0), color(BLACK), type(KEY_RANGE)
{}

SEL_ARG::SEL_ARG(Field *field_,uint8 part_,
                 uchar *min_value_, uchar *max_value_,
		 uint8 min_flag_,uint8 max_flag_,uint8 maybe_flag_)
  :min_flag(min_flag_),max_flag(max_flag_),maybe_flag(maybe_flag_), part(part_),
  maybe_null(field_->real_maybe_null()),
  rkey_func_flag(HA_READ_INVALID), elements(1),use_count(1),
  field(field_), min_value(min_value_), max_value(max_value_),
  left(&null_element), right(&null_element),
  next(NULL), prev(NULL), next_key_part(0), color(BLACK), type(KEY_RANGE)
{}

SEL_ARG *SEL_ARG::clone(RANGE_OPT_PARAM *param, SEL_ARG *new_parent, 
                        SEL_ARG **next_arg)
{
  SEL_ARG *tmp;

  if (param->has_errors())
    return 0;

  if (type != KEY_RANGE)
  {
    if (!(tmp= new (param->mem_root) SEL_ARG(type)))
      return 0;					// out of memory
    tmp->prev= *next_arg;			// Link into next/prev chain
    (*next_arg)->next=tmp;
    (*next_arg)= tmp;
    tmp->part= this->part;
  }
  else
  {
    if (!(tmp= new (param->mem_root) SEL_ARG(field,part, min_value,max_value,
                                             min_flag, max_flag, maybe_flag)))
      return 0;					// OOM
    tmp->parent=new_parent;
    tmp->next_key_part=next_key_part;
    if (left != &null_element)
      if (!(tmp->left=left->clone(param, tmp, next_arg)))
	return 0;				// OOM

    tmp->prev= *next_arg;			// Link into next/prev chain
    (*next_arg)->next=tmp;
    (*next_arg)= tmp;

    if (right != &null_element)
      if (!(tmp->right= right->clone(param, tmp, next_arg)))
	return 0;				// OOM
  }
  increment_use_count(1);
  tmp->color= color;
  tmp->elements= this->elements;
  return tmp;
}

/**
  This gives the first SEL_ARG in the interval list, and the minimal element
  in the red-black tree

  @return
  SEL_ARG   first SEL_ARG in the interval list
*/
SEL_ARG *SEL_ARG::first()
{
  SEL_ARG *next_arg=this;
  if (!next_arg->left)
    return 0;					// MAYBE_KEY
  while (next_arg->left != &null_element)
    next_arg=next_arg->left;
  return next_arg;
}

const SEL_ARG *SEL_ARG::first() const
{
  return const_cast<SEL_ARG*>(this)->first();
}

SEL_ARG *SEL_ARG::last()
{
  SEL_ARG *next_arg=this;
  if (!next_arg->right)
    return 0;					// MAYBE_KEY
  while (next_arg->right != &null_element)
    next_arg=next_arg->right;
  return next_arg;
}


/*
  Check if a compare is ok, when one takes ranges in account
  Returns -2 or 2 if the ranges where 'joined' like  < 2 and >= 2
*/

static int sel_cmp(Field *field, uchar *a, uchar *b, uint8 a_flag,
                   uint8 b_flag)
{
  int cmp;
  /* First check if there was a compare to a min or max element */
  if (a_flag & (NO_MIN_RANGE | NO_MAX_RANGE))
  {
    if ((a_flag & (NO_MIN_RANGE | NO_MAX_RANGE)) ==
	(b_flag & (NO_MIN_RANGE | NO_MAX_RANGE)))
      return 0;
    return (a_flag & NO_MIN_RANGE) ? -1 : 1;
  }
  if (b_flag & (NO_MIN_RANGE | NO_MAX_RANGE))
    return (b_flag & NO_MIN_RANGE) ? 1 : -1;

  if (field->real_maybe_null())			// If null is part of key
  {
    if (*a != *b)
    {
      return *a ? -1 : 1;
    }
    if (*a)
      goto end;					// NULL where equal
    a++; b++;					// Skip NULL marker
  }
  cmp=field->key_cmp(a , b);
  if (cmp) return cmp < 0 ? -1 : 1;		// The values differed

  // Check if the compared equal arguments was defined with open/closed range
 end:
  if (a_flag & (NEAR_MIN | NEAR_MAX))
  {
    if ((a_flag & (NEAR_MIN | NEAR_MAX)) == (b_flag & (NEAR_MIN | NEAR_MAX)))
      return 0;
    if (!(b_flag & (NEAR_MIN | NEAR_MAX)))
      return (a_flag & NEAR_MIN) ? 2 : -2;
    return (a_flag & NEAR_MIN) ? 1 : -1;
  }
  if (b_flag & (NEAR_MIN | NEAR_MAX))
    return (b_flag & NEAR_MIN) ? -2 : 2;
  return 0;					// The elements where equal
}


SEL_ARG *SEL_ARG::clone_tree(RANGE_OPT_PARAM *param)
{
  SEL_ARG tmp_link,*next_arg,*root;
  next_arg= &tmp_link;
  if (!(root= clone(param, (SEL_ARG *) 0, &next_arg)) ||
      (param && param->has_errors()))
    return 0;
  next_arg->next=0;				// Fix last link
  tmp_link.next->prev=0;			// Fix first link
  if (root)					// If not OOM
    root->use_count= 0;
  return root;
}


/*
  Table rows retrieval plan. Range optimizer creates QUICK_SELECT_I-derived
  objects from table read plans.
*/
class TABLE_READ_PLAN
{
public:
  /*
    Plan read cost, with or without cost of full row retrieval, depending
    on plan creation parameters.
  */
  Cost_estimate cost_est;
  ha_rows records; /* estimate of #rows to be examined */

  /*
    If TRUE, the scan returns rows in rowid order. This is used only for
    scans that can be both ROR and non-ROR.
  */
  bool is_ror;

  /*
    Create quick select for this plan.
    SYNOPSIS
     make_quick()
       param               Parameter from test_quick_select
       retrieve_full_rows  If TRUE, created quick select will do full record
                           retrieval.
       parent_alloc        Memory pool to use, if any.

    NOTES
      retrieve_full_rows is ignored by some implementations.

    RETURN
      created quick select
      NULL on any error.
  */
  virtual QUICK_SELECT_I *make_quick(PARAM *param,
                                     bool retrieve_full_rows,
                                     MEM_ROOT *parent_alloc=NULL) = 0;

  /* Table read plans are allocated on MEM_ROOT and are never deleted */
  static void *operator new(size_t size, MEM_ROOT *mem_root)
  { return alloc_root(mem_root, size); }
  static void operator delete(void *ptr,size_t size) { TRASH(ptr, size); }
  static void operator delete(void *ptr, MEM_ROOT *mem_root) { /* Never called */ }
  virtual ~TABLE_READ_PLAN() {}               /* Remove gcc warning */

  /**
     Add basic info for this TABLE_READ_PLAN to the optimizer trace.

     @param param        Parameters for range analysis of this table
     @param trace_object The optimizer trace object the info is appended to
   */
  virtual void trace_basic_info(const PARAM *param,
                                Opt_trace_object *trace_object) const = 0;
};

/*
  Plan for a QUICK_RANGE_SELECT scan.
  TRP_RANGE::make_quick ignores retrieve_full_rows parameter because
  QUICK_RANGE_SELECT doesn't distinguish between 'index only' scans and full
  record retrieval scans.
*/

class TRP_RANGE : public TABLE_READ_PLAN
{
public:
  /**
    Root of red-black tree for intervals over key fields to be used in
    "range" method retrieval. See SEL_ARG graph description.
  */
  SEL_ARG *key;
  uint     key_idx; /* key number in PARAM::key and PARAM::real_keynr*/
  uint     mrr_flags; 
  uint     mrr_buf_size;

  TRP_RANGE(SEL_ARG *key_arg, uint idx_arg, uint mrr_flags_arg)
   : key(key_arg), key_idx(idx_arg), mrr_flags(mrr_flags_arg)
  {}
  virtual ~TRP_RANGE() {}                     /* Remove gcc warning */

  QUICK_SELECT_I *make_quick(PARAM *param, bool retrieve_full_rows,
                             MEM_ROOT *parent_alloc)
  {
    DBUG_ENTER("TRP_RANGE::make_quick");
    QUICK_RANGE_SELECT *quick;
    if ((quick= get_quick_select(param, key_idx, key, mrr_flags, mrr_buf_size,
                                 parent_alloc)))
    {
      quick->records= records;
      quick->cost_est= cost_est;
    }
    DBUG_RETURN(quick);
  }

  void trace_basic_info(const PARAM *param,
                        Opt_trace_object *trace_object) const;
};

void TRP_RANGE::trace_basic_info(const PARAM *param,
                                 Opt_trace_object *trace_object) const
{
#ifdef OPTIMIZER_TRACE
  DBUG_ASSERT(param->using_real_indexes);
  const uint keynr_in_table= param->real_keynr[key_idx];

  const KEY &cur_key= param->table->key_info[keynr_in_table];
  const KEY_PART_INFO *key_part= cur_key.key_part;

  trace_object->add_alnum("type", "range_scan").
    add_utf8("index", cur_key.name).add("rows", records);

  Opt_trace_array trace_range(&param->thd->opt_trace, "ranges");

  // TRP_RANGE should not be created if there are no range intervals
  DBUG_ASSERT(key);

  String range_info;
  range_info.set_charset(system_charset_info);
  append_range_all_keyparts(&trace_range, NULL, &range_info,
                            key, key_part, false);
#endif
}


typedef struct st_ror_scan_info
{
  uint      idx;      ///< # of used key in param->keys
  uint      keynr;    ///< # of used key in table
  ha_rows   records;  ///< estimate of # records this scan will return

  /** Set of intervals over key fields that will be used for row retrieval. */
  SEL_ARG   *sel_arg;

  /** Fields used in the query and covered by this ROR scan. */
  MY_BITMAP covered_fields;
  /**
    Fields used in the query that are a) covered by this ROR scan and
    b) not already covered by ROR scans ordered earlier in the merge
    sequence.
  */
  MY_BITMAP covered_fields_remaining;
  /** #fields in covered_fields_remaining (caching of bitmap_bits_set()) */
  uint      num_covered_fields_remaining;

  /**
    Cost of reading all index records with values in sel_arg intervals set
    (assuming there is no need to access full table records)
  */
  Cost_estimate index_read_cost;
} ROR_SCAN_INFO;

/* Plan for QUICK_ROR_INTERSECT_SELECT scan. */

class TRP_ROR_INTERSECT : public TABLE_READ_PLAN
{
public:
  TRP_ROR_INTERSECT() {}                      /* Remove gcc warning */
  virtual ~TRP_ROR_INTERSECT() {}             /* Remove gcc warning */
  QUICK_SELECT_I *make_quick(PARAM *param, bool retrieve_full_rows,
                             MEM_ROOT *parent_alloc);

  /* Array of pointers to ROR range scans used in this intersection */
  struct st_ror_scan_info **first_scan;
  struct st_ror_scan_info **last_scan; /* End of the above array */
  struct st_ror_scan_info *cpk_scan;  /* Clustered PK scan, if there is one */
  bool is_covering; /* TRUE if no row retrieval phase is necessary */
  Cost_estimate index_scan_cost; /* SUM(cost(index_scan)) */

  void trace_basic_info(const PARAM *param,
                        Opt_trace_object *trace_object) const;
};

void TRP_ROR_INTERSECT::trace_basic_info(const PARAM *param,
                                         Opt_trace_object *trace_object) const
{
#ifdef OPTIMIZER_TRACE
  trace_object->add_alnum("type", "index_roworder_intersect").
    add("rows", records).
    add("cost", cost_est).
    add("covering", is_covering).
    add("clustered_pk_scan", cpk_scan != NULL);

  Opt_trace_context * const trace= &param->thd->opt_trace;
  Opt_trace_array ota(trace, "intersect_of");
  for (st_ror_scan_info **cur_scan= first_scan;
       cur_scan != last_scan;
       cur_scan++)
  {
    const KEY &cur_key= param->table->key_info[(*cur_scan)->keynr];
    const KEY_PART_INFO *key_part= cur_key.key_part;

    Opt_trace_object trace_isect_idx(trace);
    trace_isect_idx.add_alnum("type", "range_scan").
      add_utf8("index", cur_key.name).add("rows", (*cur_scan)->records);

    Opt_trace_array trace_range(trace, "ranges");
    for (const SEL_ARG *current= (*cur_scan)->sel_arg;
         current;
         current= current->next)
    {
      String range_info;
      range_info.set_charset(system_charset_info);
      for (const SEL_ARG *part= current;
           part;
           part= part->next_key_part)
      {
        const KEY_PART_INFO *cur_key_part= key_part + part->part;
        append_range(&range_info, cur_key_part,
                     part->min_value, part->max_value,
                     part->min_flag | part->max_flag);
      }
      trace_range.add_utf8(range_info.ptr(), range_info.length());
    }
  }
#endif
}

/*
  Plan for QUICK_ROR_UNION_SELECT scan.
  QUICK_ROR_UNION_SELECT always retrieves full rows, so retrieve_full_rows
  is ignored by make_quick.
*/

class TRP_ROR_UNION : public TABLE_READ_PLAN
{
public:
  TRP_ROR_UNION() {}                          /* Remove gcc warning */
  virtual ~TRP_ROR_UNION() {}                 /* Remove gcc warning */
  QUICK_SELECT_I *make_quick(PARAM *param, bool retrieve_full_rows,
                             MEM_ROOT *parent_alloc);
  TABLE_READ_PLAN **first_ror; /* array of ptrs to plans for merged scans */
  TABLE_READ_PLAN **last_ror;  /* end of the above array */

  void trace_basic_info(const PARAM *param,
                        Opt_trace_object *trace_object) const;
};

void TRP_ROR_UNION::trace_basic_info(const PARAM *param,
                                     Opt_trace_object *trace_object) const
{
#ifdef OPTIMIZER_TRACE
  Opt_trace_context * const trace= &param->thd->opt_trace;
  trace_object->add_alnum("type", "index_roworder_union");
  Opt_trace_array ota(trace, "union_of");
  for (TABLE_READ_PLAN **current= first_ror;
       current != last_ror;
       current++)
  {
    Opt_trace_object trp_info(trace);
    (*current)->trace_basic_info(param, &trp_info);
  }
#endif
}

/*
  Plan for QUICK_INDEX_MERGE_SELECT scan.
  QUICK_ROR_INTERSECT_SELECT always retrieves full rows, so retrieve_full_rows
  is ignored by make_quick.
*/

class TRP_INDEX_MERGE : public TABLE_READ_PLAN
{
public:
  TRP_INDEX_MERGE() {}                        /* Remove gcc warning */
  virtual ~TRP_INDEX_MERGE() {}               /* Remove gcc warning */
  QUICK_SELECT_I *make_quick(PARAM *param, bool retrieve_full_rows,
                             MEM_ROOT *parent_alloc);
  TRP_RANGE **range_scans; /* array of ptrs to plans of merged scans */
  TRP_RANGE **range_scans_end; /* end of the array */

  void trace_basic_info(const PARAM *param,
                        Opt_trace_object *trace_object) const;
};

void TRP_INDEX_MERGE::trace_basic_info(const PARAM *param,
                                       Opt_trace_object *trace_object) const
{
#ifdef OPTIMIZER_TRACE
  Opt_trace_context * const trace= &param->thd->opt_trace;
  trace_object->add_alnum("type", "index_merge");
  Opt_trace_array ota(trace, "index_merge_of");
  for (TRP_RANGE **current= range_scans;
       current != range_scans_end;
       current++)
  {
    Opt_trace_object trp_info(trace);
    (*current)->trace_basic_info(param, &trp_info);
  }
#endif
}

/*
  Plan for a QUICK_GROUP_MIN_MAX_SELECT scan. 
*/

class TRP_GROUP_MIN_MAX : public TABLE_READ_PLAN
{
private:
  bool have_min;             ///< TRUE if there is a MIN function
  bool have_max;             ///< TRUE if there is a MAX function
  /**
    TRUE if there is an aggregate distinct function, e.g.
    "COUNT(DISTINCT x)"
   */
  bool have_agg_distinct;
  /**
    The key_part of the only field used by all MIN/MAX functions.
    Note that TRP_GROUP_MIN_MAX is not used if there are MIN/MAX
    functions on more than one field.
  */
  KEY_PART_INFO *min_max_arg_part;
  uint group_prefix_len;    ///< Length of all key parts in the group prefix
  uint used_key_parts;      ///< Number of index key parts used for access
  uint group_key_parts;     ///< Number of index key parts in the group prefix
  KEY *index_info;          ///< The index chosen for data access
  uint index;               ///< The id of the chosen index
  uchar key_infix[MAX_KEY_LENGTH];  ///< Constants from equality predicates
  uint key_infix_len;       ///< Length of key_infix
  SEL_TREE *range_tree;     ///< Represents all range predicates in the query
  SEL_ARG  *index_tree;     ///< The sub-tree corresponding to index_info
  uint param_idx;           ///< Index of used key in param->key
  bool is_index_scan;       ///< Use index_next() instead of random read
public:
  /** Number of records selected by the ranges in index_tree. */
  ha_rows quick_prefix_records;
public:

  void trace_basic_info(const PARAM *param,
                        Opt_trace_object *trace_object) const;

  TRP_GROUP_MIN_MAX(bool have_min_arg, bool have_max_arg, 
                    bool have_agg_distinct_arg,
                    KEY_PART_INFO *min_max_arg_part_arg,
                    uint group_prefix_len_arg, uint used_key_parts_arg,
                    uint group_key_parts_arg, KEY *index_info_arg,
                    uint index_arg, uint key_infix_len_arg,
                    uchar *key_infix_arg,
                    SEL_TREE *tree_arg, SEL_ARG *index_tree_arg,
                    uint param_idx_arg, ha_rows quick_prefix_records_arg)
  : have_min(have_min_arg), have_max(have_max_arg),
    have_agg_distinct(have_agg_distinct_arg),
    min_max_arg_part(min_max_arg_part_arg),
    group_prefix_len(group_prefix_len_arg), used_key_parts(used_key_parts_arg),
    group_key_parts(group_key_parts_arg), index_info(index_info_arg),
    index(index_arg), key_infix_len(key_infix_len_arg), range_tree(tree_arg),
    index_tree(index_tree_arg), param_idx(param_idx_arg), is_index_scan(FALSE),
    quick_prefix_records(quick_prefix_records_arg)
    {
      if (key_infix_len)
        memcpy(this->key_infix, key_infix_arg, key_infix_len);
    }
  virtual ~TRP_GROUP_MIN_MAX() {}             /* Remove gcc warning */

  QUICK_SELECT_I *make_quick(PARAM *param, bool retrieve_full_rows,
                             MEM_ROOT *parent_alloc);
  void use_index_scan() { is_index_scan= TRUE; }
};

void TRP_GROUP_MIN_MAX::trace_basic_info(const PARAM *param,
                                         Opt_trace_object *trace_object) const
{
#ifdef OPTIMIZER_TRACE
  trace_object->add_alnum("type", "index_group").
    add_utf8("index", index_info->name);
  if (min_max_arg_part)
    trace_object->add_utf8("group_attribute",
                           min_max_arg_part->field->field_name);
  else
    trace_object->add_null("group_attribute");
  trace_object->add("min_aggregate", have_min).
    add("max_aggregate", have_max).
    add("distinct_aggregate", have_agg_distinct).
    add("rows", records).
    add("cost", cost_est);

  const KEY_PART_INFO *key_part= index_info->key_part;
  Opt_trace_context * const trace= &param->thd->opt_trace;
  {
    Opt_trace_array trace_keyparts(trace, "key_parts_used_for_access");
    for (uint partno= 0; partno < used_key_parts; partno++)
    {
      const KEY_PART_INFO *cur_key_part= key_part + partno;
      trace_keyparts.add_utf8(cur_key_part->field->field_name);
    }
  }
  Opt_trace_array trace_range(trace, "ranges");

  // can have group quick without ranges
  if (index_tree)
  {
    String range_info;
    range_info.set_charset(system_charset_info);
    append_range_all_keyparts(&trace_range, NULL,
                              &range_info, index_tree, key_part, false);
  }
#endif
}

/*
  Fill param->needed_fields with bitmap of fields used in the query.
  SYNOPSIS
    fill_used_fields_bitmap()
      param Parameter from test_quick_select function.

  NOTES
    Clustered PK members are not put into the bitmap as they are implicitly
    present in all keys (and it is impossible to avoid reading them).
  RETURN
    0  Ok
    1  Out of memory.
*/

static int fill_used_fields_bitmap(PARAM *param)
{
  TABLE *table= param->table;
  my_bitmap_map *tmp;
  uint pk;
  param->tmp_covered_fields.bitmap= 0;
  param->fields_bitmap_size= table->s->column_bitmap_size;
  if (!(tmp= (my_bitmap_map*) alloc_root(param->mem_root,
                                  param->fields_bitmap_size)) ||
      bitmap_init(&param->needed_fields, tmp, table->s->fields, FALSE))
    return 1;

  bitmap_copy(&param->needed_fields, table->read_set);
  bitmap_union(&param->needed_fields, table->write_set);

  pk= param->table->s->primary_key;
  if (pk != MAX_KEY && param->table->file->primary_key_is_clustered())
  {
    /* The table uses clustered PK and it is not internally generated */
    KEY_PART_INFO *key_part= param->table->key_info[pk].key_part;
    KEY_PART_INFO *key_part_end=
      key_part + param->table->key_info[pk].user_defined_key_parts;
    for (;key_part != key_part_end; ++key_part)
      bitmap_clear_bit(&param->needed_fields, key_part->fieldnr-1);
  }
  return 0;
}


/*
  Test if a key can be used in different ranges, and create the QUICK
  access method (range, index merge etc) that is estimated to be
  cheapest unless table/index scan is even cheaper (exception: @see
  parameter force_quick_range).

  SYNOPSIS
    test_quick_select()
      thd               Current thread
      keys_to_use       Keys to use for range retrieval
      prev_tables       Tables assumed to be already read when the scan is
                        performed (but not read at the moment of this call)
      limit             Query limit
      force_quick_range Prefer to use range (instead of full table scan) even
                        if it is more expensive.
      interesting_order The sort order the range access method must be able
                        to provide. Three-value logic: asc/desc/don't care
      needed_reg        this info is used in make_join_select() even if there is no quick!
      quick[out]        Calculated QUICK, or NULL
  NOTES
    Updates the following:
      needed_reg - Bits for keys with may be used if all prev regs are read

    In the table struct the following information is updated:
      quick_keys           - Which keys can be used
      quick_rows           - How many rows the key matches
      quick_condition_rows - E(# rows that will satisfy the table condition)

  IMPLEMENTATION
    quick_condition_rows value is obtained as follows:
      
      It is a minimum of E(#output rows) for all considered table access
      methods (range and index_merge accesses over various indexes).
    
    The obtained value is not a true E(#rows that satisfy table condition)
    but rather a pessimistic estimate. To obtain a true E(#...) one would
    need to combine estimates of various access methods, taking into account
    correlations between sets of rows they will return.
    
    For example, if values of tbl.key1 and tbl.key2 are independent (a right
    assumption if we have no information about their correlation) then the
    correct estimate will be:
    
      E(#rows("tbl.key1 < c1 AND tbl.key2 < c2")) = 
      = E(#rows(tbl.key1 < c1)) / total_rows(tbl) * E(#rows(tbl.key2 < c2)

    which is smaller than 
      
       MIN(E(#rows(tbl.key1 < c1), E(#rows(tbl.key2 < c2)))

    which is currently produced.

  TODO
   * Change the value returned in quick_condition_rows from a pessimistic
     estimate to true E(#rows that satisfy table condition). 
     (we can re-use some of E(#rows) calcuation code from index_merge/intersection 
      for this)
   
   * Check if this function really needs to modify keys_to_use, and change the
     code to pass it by reference if it doesn't.

   * In addition to force_quick_range other means can be (an usually are) used
     to make this function prefer range over full table scan. Figure out if
     force_quick_range is really needed.

  RETURN
   -1 if impossible select (i.e. certainly no rows will be selected)
    0 if can't use quick_select
    1 if found usable ranges and quick select has been successfully created.

  @note After this call, caller may decide to really use the returned QUICK,
  by calling QEP_TAB::set_quick() and updating tab->type() if appropriate.

*/
int test_quick_select(THD *thd, key_map keys_to_use,
                      table_map prev_tables,
                      ha_rows limit, bool force_quick_range,
                      const ORDER::enum_order interesting_order,
                      const QEP_shared_owner *tab,
                      Item *cond,
                      key_map *needed_reg,
                      QUICK_SELECT_I **quick)
{
  DBUG_ENTER("test_quick_select");

  *quick= NULL;
  needed_reg->clear_all();

  if (keys_to_use.is_clear_all())
    DBUG_RETURN(0);

  table_map const_tables, read_tables;
  if (tab->join())
  {
    const_tables= tab->join()->found_const_table_map;
    read_tables= tab->join()->is_executed() ?
      // in execution, range estimation is done for each row, so can access previous tables
      (tab->prefix_tables() & ~tab->added_tables()) :
      const_tables;
  }
  else
    const_tables= read_tables= 0;

  DBUG_PRINT("enter",("keys_to_use: %lu  prev_tables: %lu  const_tables: %lu",
		      (ulong) keys_to_use.to_ulonglong(), (ulong) prev_tables,
		      (ulong) const_tables));

  const Cost_model_server *const cost_model= thd->cost_model();
  TABLE *const head= tab->table();
  ha_rows records= head->file->stats.records;
  if (!records)
    records++;					/* purecov: inspected */
  double scan_time=
    cost_model->row_evaluate_cost(static_cast<double>(records)) + 1;
  Cost_estimate cost_est= head->file->table_scan_cost();
  cost_est.add_io(1.1);
  cost_est.add_cpu(scan_time);
  if (head->force_index)
  {
    scan_time= DBL_MAX;
    cost_est.set_max_cost();
  }
  if (limit < records)
  {
    cost_est.reset();
    // Force to use index
    cost_est.add_io(head->cost_model()->page_read_cost(
      static_cast<double>(records)) + 1);
    cost_est.add_cpu(scan_time);
  }
  else if (cost_est.total_cost() <= 2.0 && !force_quick_range)
    DBUG_RETURN(0);				/* No need for quick select */

  Opt_trace_context * const trace= &thd->opt_trace;
  Opt_trace_object trace_range(trace, "range_analysis");
  Opt_trace_object(trace, "table_scan").
    add("rows", head->file->stats.records).
    add("cost", cost_est);

  keys_to_use.intersect(head->keys_in_use_for_query);
  if (!keys_to_use.is_clear_all())
  {
    MEM_ROOT alloc;
    SEL_TREE *tree= NULL;
    KEY_PART *key_parts;
    KEY *key_info;
    PARAM param;

    /*
      Use the 3 multiplier as range optimizer allocates big PARAM structure
      and may evaluate a subquery expression
      TODO During the optimization phase we should evaluate only inexpensive
           single-lookup subqueries.
    */
    if (check_stack_overrun(thd, 3*STACK_MIN_SIZE + sizeof(PARAM), NULL))
      DBUG_RETURN(0);                           // Fatal error flag is set

    /* set up parameter that is passed to all functions */
    param.thd= thd;
    param.baseflag= head->file->ha_table_flags();
    param.prev_tables=prev_tables | const_tables;
    param.read_tables=read_tables;
    param.current_table= head->pos_in_table_list->map();
    param.table=head;
    param.keys=0;
    param.mem_root= &alloc;
    param.old_root= thd->mem_root;
    param.needed_reg= needed_reg;
    param.imerge_cost_buff.reset();
    param.using_real_indexes= TRUE;
    param.remove_jump_scans= TRUE;
    param.force_default_mrr= (interesting_order == ORDER::ORDER_DESC);
    param.order_direction= interesting_order;
    param.use_index_statistics= false;
    /*
      Set index_merge_allowed from OPTIMIZER_SWITCH_INDEX_MERGE.
      Notice also that OPTIMIZER_SWITCH_INDEX_MERGE disables all
      index merge sub strategies.
    */
    param.index_merge_allowed=
      thd->optimizer_switch_flag(OPTIMIZER_SWITCH_INDEX_MERGE);
    param.index_merge_union_allowed=
      param.index_merge_allowed &&
      thd->optimizer_switch_flag(OPTIMIZER_SWITCH_INDEX_MERGE_UNION);
    param.index_merge_sort_union_allowed=
      param.index_merge_allowed &&
      thd->optimizer_switch_flag(OPTIMIZER_SWITCH_INDEX_MERGE_SORT_UNION);
    param.index_merge_intersect_allowed=
      param.index_merge_allowed &&
      thd->optimizer_switch_flag(OPTIMIZER_SWITCH_INDEX_MERGE_INTERSECT);

    thd->no_errors=1;				// Don't warn about NULL
    init_sql_alloc(key_memory_test_quick_select_exec,
                   &alloc, thd->variables.range_alloc_block_size, 0);
    set_memroot_max_capacity(&alloc,
                             thd->variables.range_optimizer_max_mem_size);
    set_memroot_error_reporting(&alloc, true);
    thd->push_internal_handler(&param.error_handler);
    if (!(param.key_parts= (KEY_PART*) alloc_root(&alloc,
                                                  sizeof(KEY_PART)*
                                                  head->s->key_parts)) ||
        fill_used_fields_bitmap(&param))
    {
      thd->no_errors=0;
      thd->pop_internal_handler();
      free_root(&alloc,MYF(0));			// Return memory & allocator
      DBUG_RETURN(0);				// Can't use range
    }
    key_parts= param.key_parts;
    thd->mem_root= &alloc;

    {
      Opt_trace_array trace_idx(trace,
                                "potential_range_indexes",
                                Opt_trace_context::RANGE_OPTIMIZER);
      /*
        Make an array with description of all key parts of all table keys.
        This is used in get_mm_parts function.
      */
      key_info= head->key_info;
      for (uint idx= 0 ; idx < head->s->keys ; idx++, key_info++)
      {
        Opt_trace_object trace_idx_details(trace);
        trace_idx_details.add_utf8("index", key_info->name);
        KEY_PART_INFO *key_part_info;

        if (!keys_to_use.is_set(idx))
        {
          trace_idx_details.add("usable", false).
            add_alnum("cause", "not_applicable");
          continue;
        }

        if (hint_key_state(thd, head, idx, NO_RANGE_HINT_ENUM, 0))
        {
          trace_idx_details.add("usable", false).
            add_alnum("cause", "no_range_optimization hint");
          continue;
        }

        if (key_info->flags & HA_FULLTEXT)
        {
          trace_idx_details.add("usable", false).
            add_alnum("cause", "fulltext");
          continue;    // ToDo: ft-keys in non-ft ranges, if possible   SerG
        }

        trace_idx_details.add("usable", true);

        param.key[param.keys]=key_parts;
        key_part_info= key_info->key_part;
        Opt_trace_array trace_keypart(trace, "key_parts");
        for (uint part=0 ; part < actual_key_parts(key_info) ;
             part++, key_parts++, key_part_info++)
        {
          key_parts->key=          param.keys;
          key_parts->part=         part;
          key_parts->length=       key_part_info->length;
          key_parts->store_length= key_part_info->store_length;
          key_parts->field=        key_part_info->field;
          key_parts->null_bit=     key_part_info->null_bit;
          key_parts->image_type =
            (part < key_info->user_defined_key_parts &&
             key_info->flags & HA_SPATIAL) ? Field::itMBR : Field::itRAW;
          /* Only HA_PART_KEY_SEG is used */
          key_parts->flag=         (uint8) key_part_info->key_part_flag;
          trace_keypart.add_utf8(key_parts->field->field_name);
        }
        param.real_keynr[param.keys++]=idx;
      }
    }
    param.key_parts_end=key_parts;

    /* Calculate cost of full index read for the shortest covering index */
    if (!head->covering_keys.is_clear_all())
    {
      int key_for_use= find_shortest_key(head, &head->covering_keys);
      Cost_estimate key_read_time=
        param.table->file->index_scan_cost(key_for_use, 1,
                                           static_cast<double>(records));
      key_read_time.add_cpu(cost_model->row_evaluate_cost(
        static_cast<double>(records)));

      bool chosen= false;
      if (key_read_time < cost_est)
      {
        cost_est= key_read_time;
        chosen= true;
      }

      Opt_trace_object trace_cov(trace,
                                 "best_covering_index_scan",
                                 Opt_trace_context::RANGE_OPTIMIZER);
      trace_cov.add_utf8("index", head->key_info[key_for_use].name).
        add("cost", key_read_time).add("chosen", chosen);
      if (!chosen)
        trace_cov.add_alnum("cause", "cost");
    }

    TABLE_READ_PLAN *best_trp= NULL;
    TRP_GROUP_MIN_MAX *group_trp;
    Cost_estimate best_cost= cost_est;

    if (cond)
    {
      {
        Opt_trace_array trace_setup_cond(trace, "setup_range_conditions");
        tree= get_mm_tree(&param, cond);
      }
      if (tree)
      {
        if (tree->type == SEL_TREE::IMPOSSIBLE)
        {
          trace_range.add("impossible_range", true);
          records=0L;                      /* Return -1 from this function. */
          cost_est.reset();
          cost_est.add_io(static_cast<double>(HA_POS_ERROR));
          goto free_mem;
        }
        /*
          If the tree can't be used for range scans, proceed anyway, as we
          can construct a group-min-max quick select
        */
        if (tree->type != SEL_TREE::KEY && tree->type != SEL_TREE::KEY_SMALLER)
        {
          trace_range.add("range_scan_possible", false);
          if (tree->type == SEL_TREE::ALWAYS)
            trace_range.add_alnum("cause", "condition_always_true");

          tree= NULL;
        }
      }
    }

    /*
      Try to construct a QUICK_GROUP_MIN_MAX_SELECT.
      Notice that it can be constructed no matter if there is a range tree.
    */
    group_trp= get_best_group_min_max(&param, tree, &best_cost);
    if (group_trp)
    {
      param.table->quick_condition_rows= min(group_trp->records,
                                             head->file->stats.records);
      Opt_trace_object grp_summary(trace,
                                   "best_group_range_summary",
                                   Opt_trace_context::RANGE_OPTIMIZER);
      if (unlikely(trace->is_started()))
        group_trp->trace_basic_info(&param, &grp_summary);
      if (group_trp->cost_est < best_cost)
      {
        grp_summary.add("chosen", true);
        best_trp= group_trp;
        best_cost= best_trp->cost_est;
      }
      else
        grp_summary.add("chosen", false).add_alnum("cause", "cost");
    }

    if (tree)
    {
      /*
        It is possible to use a range-based quick select (but it might be
        slower than 'all' table scan).
      */
      dbug_print_tree("final_tree", tree, &param);

      {
        /*
          Calculate cost of single index range scan and possible
          intersections of these
        */
        Opt_trace_object trace_range(trace,
                                     "analyzing_range_alternatives",
                                     Opt_trace_context::RANGE_OPTIMIZER);
        TRP_RANGE         *range_trp;
        TRP_ROR_INTERSECT *rori_trp;

        /* Get best 'range' plan and prepare data for making other plans */
        if ((range_trp= get_key_scans_params(&param, tree, FALSE, TRUE,
                                             &best_cost)))
        {
          best_trp= range_trp;
          best_cost= best_trp->cost_est;
        }

        /*
          Simultaneous key scans and row deletes on several handler
          objects are not allowed so don't use ROR-intersection for
          table deletes. Also, ROR-intersection cannot return rows in
          descending order
        */
        if ((thd->lex->sql_command != SQLCOM_DELETE) && 
            param.index_merge_allowed &&
            interesting_order != ORDER::ORDER_DESC)
        {
          /*
            Get best non-covering ROR-intersection plan and prepare data for
            building covering ROR-intersection.
          */
          if ((rori_trp= get_best_ror_intersect(&param, tree, &best_cost)))
          {
            best_trp= rori_trp;
            best_cost= best_trp->cost_est;
          }
        }
      }

      // Here we calculate cost of union index merge
      if (!tree->merges.is_empty())
      {
        // Cannot return rows in descending order.
        if (param.index_merge_allowed &&
            interesting_order != ORDER::ORDER_DESC &&
            param.table->file->stats.records)
        {
          /* Try creating index_merge/ROR-union scan. */
          SEL_IMERGE *imerge;
          TABLE_READ_PLAN *best_conj_trp= NULL, *new_conj_trp= NULL;
          List_iterator_fast<SEL_IMERGE> it(tree->merges);
          Opt_trace_array trace_idx_merge(trace,
                                          "analyzing_index_merge_union",
                                          Opt_trace_context::RANGE_OPTIMIZER);
          while ((imerge= it++))
          {
            new_conj_trp= get_best_disjunct_quick(&param, imerge,
                                                  &best_cost);
            if (new_conj_trp)
              set_if_smaller(param.table->quick_condition_rows,
                             new_conj_trp->records);
            if (!best_conj_trp ||
                (new_conj_trp &&
                 new_conj_trp->cost_est < best_conj_trp->cost_est))
            {
              best_conj_trp= new_conj_trp;
            }
          }
          if (best_conj_trp)
            best_trp= best_conj_trp;
        }
      }
    }

    thd->mem_root= param.old_root;

    /* If we got a read plan, create a quick select from it. */
    if (best_trp)
    {
      QUICK_SELECT_I *qck;
      records= best_trp->records;
      if (!(qck= best_trp->make_quick(&param, TRUE)) || qck->init())
        qck= NULL;
      *quick= qck;
    }

free_mem:
    thd->pop_internal_handler();
    if (unlikely(*quick && trace->is_started() && best_trp))
    {
      // best_trp cannot be NULL if quick is set, done to keep fortify happy
      Opt_trace_object trace_range_summary(trace,
                                           "chosen_range_access_summary");
      {
        Opt_trace_object trace_range_plan(trace,
                                          "range_access_plan");
        best_trp->trace_basic_info(&param, &trace_range_plan);
      }
      trace_range_summary.add("rows_for_plan", (*quick)->records).
        add("cost_for_plan", (*quick)->cost_est).
        add("chosen", true);
    }

    free_root(&alloc,MYF(0));			// Return memory & allocator
    thd->mem_root= param.old_root;
    thd->no_errors=0;

    DBUG_EXECUTE("info", print_quick(*quick, needed_reg););
  }


  /*
    Assume that if the user is using 'limit' we will only need to scan
    limit rows if we are using a key
  */
  DBUG_RETURN(records ? MY_TEST(*quick) : -1);
}

/****************************************************************************
 * Partition pruning module
 ****************************************************************************/

/*
  PartitionPruningModule

  This part of the code does partition pruning. Partition pruning solves the
  following problem: given a query over partitioned tables, find partitions
  that we will not need to access (i.e. partitions that we can assume to be
  empty) when executing the query.
  The set of partitions to prune doesn't depend on which query execution
  plan will be used to execute the query.
  
  HOW IT WORKS
  
  Partition pruning module makes use of RangeAnalysisModule. The following
  examples show how the problem of partition pruning can be reduced to the 
  range analysis problem:
  
  EXAMPLE 1
    Consider a query:
    
      SELECT * FROM t1 WHERE (t1.a < 5 OR t1.a = 10) AND t1.a > 3 AND t1.b='z'
    
    where table t1 is partitioned using PARTITION BY RANGE(t1.a).  An apparent
    way to find the used (i.e. not pruned away) partitions is as follows:
    
    1. analyze the WHERE clause and extract the list of intervals over t1.a
       for the above query we will get this list: {(3 < t1.a < 5), (t1.a=10)}

    2. for each interval I
       {
         find partitions that have non-empty intersection with I;
         mark them as used;
       }
       
  EXAMPLE 2
    Suppose the table is partitioned by HASH(part_func(t1.a, t1.b)). Then
    we need to:

    1. Analyze the WHERE clause and get a list of intervals over (t1.a, t1.b).
       The list of intervals we'll obtain will look like this:
       ((t1.a, t1.b) = (1,'foo')),
       ((t1.a, t1.b) = (2,'bar')), 
       ((t1,a, t1.b) > (10,'zz'))
       
    2. for each interval I 
       {
         if (the interval has form "(t1.a, t1.b) = (const1, const2)" )
         {
           calculate HASH(part_func(t1.a, t1.b));
           find which partition has records with this hash value and mark
             it as used;
         }
         else
         {
           mark all partitions as used; 
           break;
         }
       }

   For both examples the step #1 is exactly what RangeAnalysisModule could
   be used to do, if it was provided with appropriate index description
   (array of KEY_PART structures). 
   In example #1, we need to provide it with description of index(t1.a), 
   in example #2, we need to provide it with description of index(t1.a, t1.b).
   
   These index descriptions are further called "partitioning index
   descriptions". Note that it doesn't matter if such indexes really exist,
   as range analysis module only uses the description.
   
   Putting it all together, partitioning module works as follows:
   
   prune_partitions() {
     call create_partition_index_description();

     call get_mm_tree(); // invoke the RangeAnalysisModule
     
     // analyze the obtained interval list and get used partitions 
     call find_used_partitions();
  }

*/

struct st_part_prune_param;
struct st_part_opt_info;

typedef void (*mark_full_part_func)(partition_info*, uint32);

/*
  Partition pruning operation context
*/
typedef struct st_part_prune_param
{
  RANGE_OPT_PARAM range_param; /* Range analyzer parameters */

  /***************************************************************
   Following fields are filled in based solely on partitioning 
   definition and not modified after that:
   **************************************************************/
  partition_info *part_info; /* Copy of table->part_info */
  /* Function to get partition id from partitioning fields only */
  get_part_id_func get_top_partition_id_func;
  /* Function to mark a partition as used (w/all subpartitions if they exist)*/
  mark_full_part_func mark_full_partition_used;
 
  /* Partitioning 'index' description, array of key parts */
  KEY_PART *key;
  
  /*
    Number of fields in partitioning 'index' definition created for
    partitioning (0 if partitioning 'index' doesn't include partitioning
    fields)
  */
  uint part_fields;
  uint subpart_fields; /* Same as above for subpartitioning */
  
  /* 
    Number of the last partitioning field keypart in the index, or -1 if
    partitioning index definition doesn't include partitioning fields.
  */
  int last_part_partno;
  int last_subpart_partno; /* Same as above for supartitioning */

  /*
    is_part_keypart[i] == test(keypart #i in partitioning index is a member
                               used in partitioning)
    Used to maintain current values of cur_part_fields and cur_subpart_fields
  */
  my_bool *is_part_keypart;
  /* Same as above for subpartitioning */
  my_bool *is_subpart_keypart;

  my_bool ignore_part_fields; /* Ignore rest of partioning fields */

  /***************************************************************
   Following fields form find_used_partitions() recursion context:
   **************************************************************/
  SEL_ARG **arg_stack;     /* "Stack" of SEL_ARGs */
  SEL_ARG **arg_stack_end; /* Top of the stack    */
  /* Number of partitioning fields for which we have a SEL_ARG* in arg_stack */
  uint cur_part_fields;
  /* Same as cur_part_fields, but for subpartitioning */
  uint cur_subpart_fields;

  /* Iterator to be used to obtain the "current" set of used partitions */
  PARTITION_ITERATOR part_iter;

  /* Initialized bitmap of num_subparts size */
  MY_BITMAP subparts_bitmap;

  uchar *cur_min_key;
  uchar *cur_max_key;

  uint cur_min_flag, cur_max_flag;
} PART_PRUNE_PARAM;

static bool create_partition_index_description(PART_PRUNE_PARAM *prune_par);
static int find_used_partitions(PART_PRUNE_PARAM *ppar, SEL_ARG *key_tree);
static int find_used_partitions_imerge(PART_PRUNE_PARAM *ppar,
                                       SEL_IMERGE *imerge);
static int find_used_partitions_imerge_list(PART_PRUNE_PARAM *ppar,
                                            List<SEL_IMERGE> &merges);
static void mark_all_partitions_as_used(partition_info *part_info);

#ifndef DBUG_OFF
static void print_partitioning_index(KEY_PART *parts, KEY_PART *parts_end);
static void dbug_print_segment_range(SEL_ARG *arg, KEY_PART *part);
static void dbug_print_singlepoint_range(SEL_ARG **start, uint num);
#endif


/**
  Perform partition pruning for a given table and condition.

  @param      thd            Thread handle
  @param      table          Table to perform partition pruning for
  @param      pprune_cond    Condition to use for partition pruning
  
  @note This function assumes that lock_partitions are setup when it
  is invoked. The function analyzes the condition, finds partitions that
  need to be used to retrieve the records that match the condition, and 
  marks them as used by setting appropriate bit in part_info->read_partitions
  In the worst case all partitions are marked as used. If the table is not
  yet locked, it will also unset bits in part_info->lock_partitions that is
  not set in read_partitions.

  This function returns promptly if called for non-partitioned table.

  @return Operation status
    @retval true  Failure
    @retval false Success
*/

bool prune_partitions(THD *thd, TABLE *table, Item *pprune_cond)
{
  partition_info *part_info = table->part_info;
  DBUG_ENTER("prune_partitions");

  /*
    If the prepare stage already have completed pruning successfully,
    it is no use of running prune_partitions() again on the same condition.
    Since it will not be able to prune anything more than the previous call
    from the prepare step.
  */
  if (part_info && part_info->is_pruning_completed)
    DBUG_RETURN(false);

  table->all_partitions_pruned_away= false;

  if (!part_info)
    DBUG_RETURN(FALSE); /* not a partitioned table */

  if (table->s->db_type()->partition_flags() & HA_USE_AUTO_PARTITION &&
      part_info->is_auto_partitioned)
    DBUG_RETURN(false); /* Should not prune auto partitioned table */

  if (!pprune_cond)
  {
    mark_all_partitions_as_used(part_info);
    DBUG_RETURN(FALSE);
  }
  
  /* No need to continue pruning if there is no more partitions to prune! */
  if (bitmap_is_clear_all(&part_info->lock_partitions))
    bitmap_clear_all(&part_info->read_partitions);
  if (bitmap_is_clear_all(&part_info->read_partitions))
  {
    table->all_partitions_pruned_away= true;
    DBUG_RETURN(false);
  }

  PART_PRUNE_PARAM prune_param;
  MEM_ROOT alloc;
  RANGE_OPT_PARAM  *range_par= &prune_param.range_param;
  my_bitmap_map *old_sets[2];

  prune_param.part_info= part_info;
  init_sql_alloc(key_memory_prune_partitions_exec,
                 &alloc, thd->variables.range_alloc_block_size, 0);
  set_memroot_max_capacity(&alloc, thd->variables.range_optimizer_max_mem_size);
  set_memroot_error_reporting(&alloc, true);
  thd->push_internal_handler(&range_par->error_handler);
  range_par->mem_root= &alloc;
  range_par->old_root= thd->mem_root;

  if (create_partition_index_description(&prune_param))
  {
    mark_all_partitions_as_used(part_info);
    thd->pop_internal_handler();
    free_root(&alloc,MYF(0));		// Return memory & allocator
    DBUG_RETURN(FALSE);
  }
  
  dbug_tmp_use_all_columns(table, old_sets, 
                           table->read_set, table->write_set);
  range_par->thd= thd;
  range_par->table= table;
  /* range_par->cond doesn't need initialization */
  range_par->prev_tables= range_par->read_tables= 0;
  range_par->current_table= table->pos_in_table_list->map();

  range_par->keys= 1; // one index
  range_par->using_real_indexes= FALSE;
  range_par->remove_jump_scans= FALSE;
  range_par->real_keynr[0]= 0;

  thd->no_errors=1;				// Don't warn about NULL
  thd->mem_root=&alloc;

  bitmap_clear_all(&part_info->read_partitions);

  prune_param.key= prune_param.range_param.key_parts;
  SEL_TREE *tree;
  int res;

  tree= get_mm_tree(range_par, pprune_cond);
  if (!tree)
    goto all_used;

  if (tree->type == SEL_TREE::IMPOSSIBLE)
  {
    /* Cannot improve the pruning any further. */
    part_info->is_pruning_completed= true;
    goto end;
  }

  if (tree->type != SEL_TREE::KEY && tree->type != SEL_TREE::KEY_SMALLER)
    goto all_used;

  if (tree->merges.is_empty())
  {
    /* Range analysis has produced a single list of intervals. */
    prune_param.arg_stack_end= prune_param.arg_stack;
    prune_param.cur_part_fields= 0;
    prune_param.cur_subpart_fields= 0;
    
    prune_param.cur_min_key= prune_param.range_param.min_key;
    prune_param.cur_max_key= prune_param.range_param.max_key;
    prune_param.cur_min_flag= prune_param.cur_max_flag= 0;

    init_all_partitions_iterator(part_info, &prune_param.part_iter);
    if (!tree->keys[0] || (-1 == (res= find_used_partitions(&prune_param,
                                                            tree->keys[0]))))
      goto all_used;
  }
  else
  {
    if (tree->merges.elements == 1)
    {
      /* 
        Range analysis has produced a "merge" of several intervals lists, a 
        SEL_TREE that represents an expression in form         
          sel_imerge = (tree1 OR tree2 OR ... OR treeN)
        that cannot be reduced to one tree. This can only happen when 
        partitioning index has several keyparts and the condition is OR of
        conditions that refer to different key parts. For example, we'll get
        here for "partitioning_field=const1 OR subpartitioning_field=const2"
      */
      if (-1 == (res= find_used_partitions_imerge(&prune_param,
                                                  tree->merges.head())))
        goto all_used;
    }
    else
    {
      /* 
        Range analysis has produced a list of several imerges, i.e. a
        structure that represents a condition in form 
        imerge_list= (sel_imerge1 AND sel_imerge2 AND ... AND sel_imergeN)
        This is produced for complicated WHERE clauses that range analyzer
        can't really analyze properly.
      */
      if (-1 == (res= find_used_partitions_imerge_list(&prune_param,
                                                       tree->merges)))
        goto all_used;
    }
  }
  
  /*
    If the condition can be evaluated now, we are done with pruning.

    During the prepare phase, before locking, subqueries and stored programs
    are not evaluated. So we need to run prune_partitions() a second time in
    the optimize phase to prune partitions for reading, when subqueries and
    stored programs may be evaluated.
  */
  if (pprune_cond->can_be_evaluated_now())
    part_info->is_pruning_completed= true;
  goto end;

all_used:
  mark_all_partitions_as_used(prune_param.part_info);
end:
  thd->pop_internal_handler();
  dbug_tmp_restore_column_maps(table->read_set, table->write_set, old_sets);
  thd->no_errors=0;
  thd->mem_root= range_par->old_root;
  free_root(&alloc,MYF(0));			// Return memory & allocator
  /* If an error occurred we can return failure after freeing the memroot. */
  if (thd->is_error())
  {
    DBUG_RETURN(true);
  }
  /*
    Must be a subset of the locked partitions.
    lock_partitions contains the partitions marked by explicit partition
    selection (... t PARTITION (pX) ...) and we must only use partitions
    within that set.
  */
  bitmap_intersect(&prune_param.part_info->read_partitions,
                   &prune_param.part_info->lock_partitions);
  /*
    If not yet locked, also prune partitions to lock if not UPDATEing
    partition key fields. This will also prune lock_partitions if we are under
    LOCK TABLES, so prune away calls to start_stmt().
    TODO: enhance this prune locking to also allow pruning of
    'UPDATE t SET part_key = const WHERE cond_is_prunable' so it adds
    a lock for part_key partition.
  */
  if (!thd->lex->is_query_tables_locked() &&
      !partition_key_modified(table, table->write_set))
  {
    bitmap_copy(&prune_param.part_info->lock_partitions,
                &prune_param.part_info->read_partitions);
  }
  if (bitmap_is_clear_all(&(prune_param.part_info->read_partitions)))
    table->all_partitions_pruned_away= true;
  DBUG_RETURN(false);
}


/*
  Store field key image to table record

  SYNOPSIS
    store_key_image_to_rec()
      field  Field which key image should be stored
      ptr    Field value in key format
      len    Length of the value, in bytes

  DESCRIPTION
    Copy the field value from its key image to the table record. The source
    is the value in key image format, occupying len bytes in buffer pointed
    by ptr. The destination is table record, in "field value in table record"
    format.
*/

void store_key_image_to_rec(Field *field, uchar *ptr, uint len)
{
  /* Do the same as print_key_value() does */
  my_bitmap_map *old_map;

  if (field->real_maybe_null())
  {
    if (*ptr)
    {
      field->set_null();
      return;
    }
    field->set_notnull();
    ptr++;
  }    
  old_map= dbug_tmp_use_all_columns(field->table,
                                    field->table->write_set);
  field->set_key_image(ptr, len); 
  dbug_tmp_restore_column_map(field->table->write_set, old_map);
}


/*
  For SEL_ARG* array, store sel_arg->min values into table record buffer

  SYNOPSIS
    store_selargs_to_rec()
      ppar   Partition pruning context
      start  Array of SEL_ARG* for which the minimum values should be stored
      num    Number of elements in the array

  DESCRIPTION
    For each SEL_ARG* interval in the specified array, store the left edge
    field value (sel_arg->min, key image format) into the table record.
*/

static void store_selargs_to_rec(PART_PRUNE_PARAM *ppar, SEL_ARG **start,
                                 int num)
{
  KEY_PART *parts= ppar->range_param.key_parts;
  for (SEL_ARG **end= start + num; start != end; start++)
  {
    SEL_ARG *sel_arg= (*start);
    store_key_image_to_rec(sel_arg->field, sel_arg->min_value,
                           parts[sel_arg->part].length);
  }
}


/* Mark a partition as used in the case when there are no subpartitions */
static void mark_full_partition_used_no_parts(partition_info* part_info,
                                              uint32 part_id)
{
  DBUG_ENTER("mark_full_partition_used_no_parts");
  DBUG_PRINT("enter", ("Mark partition %u as used", part_id));
  bitmap_set_bit(&part_info->read_partitions, part_id);
  DBUG_VOID_RETURN;
}


/* Mark a partition as used in the case when there are subpartitions */
static void mark_full_partition_used_with_parts(partition_info *part_info,
                                                uint32 part_id)
{
  uint32 start= part_id * part_info->num_subparts;
  uint32 end=   start + part_info->num_subparts; 
  DBUG_ENTER("mark_full_partition_used_with_parts");

  for (; start != end; start++)
  {
    DBUG_PRINT("info", ("1:Mark subpartition %u as used", start));
    bitmap_set_bit(&part_info->read_partitions, start);
  }
  DBUG_VOID_RETURN;
}

/*
  Find the set of used partitions for List<SEL_IMERGE>
  SYNOPSIS
    find_used_partitions_imerge_list
      ppar      Partition pruning context.
      key_tree  Intervals tree to perform pruning for.
      
  DESCRIPTION
    List<SEL_IMERGE> represents "imerge1 AND imerge2 AND ...". 
    The set of used partitions is an intersection of used partitions sets
    for imerge_{i}.
    We accumulate this intersection in a separate bitmap.
 
  RETURN 
    See find_used_partitions()
*/

static int find_used_partitions_imerge_list(PART_PRUNE_PARAM *ppar,
                                            List<SEL_IMERGE> &merges)
{
  MY_BITMAP all_merges;
  uint bitmap_bytes;
  my_bitmap_map *bitmap_buf;
  uint n_bits= ppar->part_info->read_partitions.n_bits;
  bitmap_bytes= bitmap_buffer_size(n_bits);
  if (!(bitmap_buf= (my_bitmap_map*) alloc_root(ppar->range_param.mem_root,
                                                bitmap_bytes)))
  {
    /*
      Fallback, process just the first SEL_IMERGE. This can leave us with more
      partitions marked as used then actually needed.
    */
    return find_used_partitions_imerge(ppar, merges.head());
  }
  bitmap_init(&all_merges, bitmap_buf, n_bits, FALSE);
  bitmap_set_prefix(&all_merges, n_bits);

  List_iterator<SEL_IMERGE> it(merges);
  SEL_IMERGE *imerge;
  while ((imerge=it++))
  {
    int res= find_used_partitions_imerge(ppar, imerge);
    if (!res)
    {
      /* no used partitions on one ANDed imerge => no used partitions at all */
      return 0;
    }

    if (res != -1)
      bitmap_intersect(&all_merges, &ppar->part_info->read_partitions);

    if (bitmap_is_clear_all(&all_merges))
      return 0;

    bitmap_clear_all(&ppar->part_info->read_partitions);
  }
  memcpy(ppar->part_info->read_partitions.bitmap, all_merges.bitmap,
         bitmap_bytes);
  return 1;
}


/*
  Find the set of used partitions for SEL_IMERGE structure
  SYNOPSIS
    find_used_partitions_imerge()
      ppar      Partition pruning context.
      key_tree  Intervals tree to perform pruning for.
      
  DESCRIPTION
    SEL_IMERGE represents "tree1 OR tree2 OR ...". The implementation is
    trivial - just use mark used partitions for each tree and bail out early
    if for some tree_{i} all partitions are used.
 
  RETURN 
    See find_used_partitions().
*/

static
int find_used_partitions_imerge(PART_PRUNE_PARAM *ppar, SEL_IMERGE *imerge)
{
  int res= 0;
  for (SEL_TREE **ptree= imerge->trees; ptree < imerge->trees_next; ptree++)
  {
    ppar->arg_stack_end= ppar->arg_stack;
    ppar->cur_part_fields= 0;
    ppar->cur_subpart_fields= 0;
    
    ppar->cur_min_key= ppar->range_param.min_key;
    ppar->cur_max_key= ppar->range_param.max_key;
    ppar->cur_min_flag= ppar->cur_max_flag= 0;

    init_all_partitions_iterator(ppar->part_info, &ppar->part_iter);
    SEL_ARG *key_tree= (*ptree)->keys[0];
    if (!key_tree || (-1 == (res |= find_used_partitions(ppar, key_tree))))
      return -1;
  }
  return res;
}


/*
  Collect partitioning ranges for the SEL_ARG tree and mark partitions as used

  SYNOPSIS
    find_used_partitions()
      ppar      Partition pruning context.
      key_tree  SEL_ARG range tree to perform pruning for

  DESCRIPTION
    This function 
      * recursively walks the SEL_ARG* tree collecting partitioning "intervals"
      * finds the partitions one needs to use to get rows in these intervals
      * marks these partitions as used.
    The next session desribes the process in greater detail.
 
  IMPLEMENTATION
    TYPES OF RESTRICTIONS THAT WE CAN OBTAIN PARTITIONS FOR    
    We can find out which [sub]partitions to use if we obtain restrictions on 
    [sub]partitioning fields in the following form:
    1.  "partition_field1=const1 AND ... AND partition_fieldN=constN"
    1.1  Same as (1) but for subpartition fields

    If partitioning supports interval analysis (i.e. partitioning is a
    function of a single table field, and partition_info::
    get_part_iter_for_interval != NULL), then we can also use condition in
    this form:
    2.  "const1 <=? partition_field <=? const2"
    2.1  Same as (2) but for subpartition_field

    INFERRING THE RESTRICTIONS FROM SEL_ARG TREE
    
    The below is an example of what SEL_ARG tree may represent:
    
    (start)
     |                           $
     |   Partitioning keyparts   $  subpartitioning keyparts
     |                           $
     |     ...          ...      $
     |      |            |       $
     | +---------+  +---------+  $  +-----------+  +-----------+
     \-| par1=c1 |--| par2=c2 |-----| subpar1=c3|--| subpar2=c5|
       +---------+  +---------+  $  +-----------+  +-----------+
            |                    $        |             |
            |                    $        |        +-----------+ 
            |                    $        |        | subpar2=c6|
            |                    $        |        +-----------+ 
            |                    $        |
            |                    $  +-----------+  +-----------+
            |                    $  | subpar1=c4|--| subpar2=c8|
            |                    $  +-----------+  +-----------+
            |                    $         
            |                    $
       +---------+               $  +------------+  +------------+
       | par1=c2 |------------------| subpar1=c10|--| subpar2=c12|
       +---------+               $  +------------+  +------------+
            |                    $
           ...                   $

    The up-down connections are connections via SEL_ARG::left and
    SEL_ARG::right. A horizontal connection to the right is the
    SEL_ARG::next_key_part connection.
    
    find_used_partitions() traverses the entire tree via recursion on
     * SEL_ARG::next_key_part (from left to right on the picture)
     * SEL_ARG::left|right (up/down on the pic). Left-right recursion is
       performed for each depth level.
    
    Recursion descent on SEL_ARG::next_key_part is used to accumulate (in
    ppar->arg_stack) constraints on partitioning and subpartitioning fields.
    For the example in the above picture, one of stack states is:
      in find_used_partitions(key_tree = "subpar2=c5") (***)
      in find_used_partitions(key_tree = "subpar1=c3")
      in find_used_partitions(key_tree = "par2=c2")   (**)
      in find_used_partitions(key_tree = "par1=c1")
      in prune_partitions(...)
    We apply partitioning limits as soon as possible, e.g. when we reach the
    depth (**), we find which partition(s) correspond to "par1=c1 AND par2=c2",
    and save them in ppar->part_iter.
    When we reach the depth (***), we find which subpartition(s) correspond to
    "subpar1=c3 AND subpar2=c5", and then mark appropriate subpartitions in
    appropriate subpartitions as used.
    
    It is possible that constraints on some partitioning fields are missing.
    For the above example, consider this stack state:
      in find_used_partitions(key_tree = "subpar2=c12") (***)
      in find_used_partitions(key_tree = "subpar1=c10")
      in find_used_partitions(key_tree = "par1=c2")
      in prune_partitions(...)
    Here we don't have constraints for all partitioning fields. Since we've
    never set the ppar->part_iter to contain used set of partitions, we use
    its default "all partitions" value.  We get  subpartition id for 
    "subpar1=c3 AND subpar2=c5", and mark that subpartition as used in every
    partition.

    The inverse is also possible: we may get constraints on partitioning
    fields, but not constraints on subpartitioning fields. In that case,
    calls to find_used_partitions() with depth below (**) will return -1,
    and we will mark entire partition as used.

  TODO
    Replace recursion on SEL_ARG::left and SEL_ARG::right with a loop

  RETURN
    1   OK, one or more [sub]partitions are marked as used.
    0   The passed condition doesn't match any partitions
   -1   Couldn't infer any partition pruning "intervals" from the passed 
        SEL_ARG* tree (which means that all partitions should be marked as
        used) Marking partitions as used is the responsibility of the caller.
*/

static 
int find_used_partitions(PART_PRUNE_PARAM *ppar, SEL_ARG *key_tree)
{
  int res, left_res=0, right_res=0;
  int key_tree_part= (int)key_tree->part;
  bool set_full_part_if_bad_ret= FALSE;
  bool ignore_part_fields= ppar->ignore_part_fields;
  bool did_set_ignore_part_fields= FALSE;
  RANGE_OPT_PARAM *range_par= &(ppar->range_param);

  if (check_stack_overrun(range_par->thd, 3*STACK_MIN_SIZE, NULL))
    return -1;

  if (key_tree->left != &null_element)
  {
    if (-1 == (left_res= find_used_partitions(ppar,key_tree->left)))
      return -1;
  }

  /* Push SEL_ARG's to stack to enable looking backwards as well */
  ppar->cur_part_fields+= ppar->is_part_keypart[key_tree_part];
  ppar->cur_subpart_fields+= ppar->is_subpart_keypart[key_tree_part];
  *(ppar->arg_stack_end++)= key_tree;

  if (ignore_part_fields)
  {
    /*
      We come here when a condition on the first partitioning
      fields led to evaluating the partitioning condition
      (due to finding a condition of the type a < const or
      b > const). Thus we must ignore the rest of the
      partitioning fields but we still want to analyse the
      subpartitioning fields.
    */
    if (key_tree->next_key_part)
      res= find_used_partitions(ppar, key_tree->next_key_part);
    else
      res= -1;
    goto pop_and_go_right;
  }

  if (key_tree->type == SEL_ARG::KEY_RANGE)
  {
    if (ppar->part_info->get_part_iter_for_interval && 
        key_tree->part <= ppar->last_part_partno)
    {
      /* Collect left and right bound, their lengths and flags */
      uchar *min_key= ppar->cur_min_key;
      uchar *max_key= ppar->cur_max_key;
      uchar *tmp_min_key= min_key;
      uchar *tmp_max_key= max_key;
      key_tree->store_min(ppar->key[key_tree->part].store_length,
                          &tmp_min_key, ppar->cur_min_flag);
      key_tree->store_max(ppar->key[key_tree->part].store_length,
                          &tmp_max_key, ppar->cur_max_flag);
      uint flag;
      if (key_tree->next_key_part &&
          key_tree->next_key_part->part == key_tree->part+1 &&
          key_tree->next_key_part->part <= ppar->last_part_partno &&
          key_tree->next_key_part->type == SEL_ARG::KEY_RANGE)
      {
        /*
          There are more key parts for partition pruning to handle
          This mainly happens when the condition is an equality
          condition.
        */
        if ((tmp_min_key - min_key) == (tmp_max_key - max_key) && 
            (memcmp(min_key, max_key, (uint)(tmp_max_key - max_key)) == 0) &&
            !key_tree->min_flag && !key_tree->max_flag)
        {
          /* Set 'parameters' */
          ppar->cur_min_key= tmp_min_key;
          ppar->cur_max_key= tmp_max_key;
          uint save_min_flag= ppar->cur_min_flag;
          uint save_max_flag= ppar->cur_max_flag;

          ppar->cur_min_flag|= key_tree->min_flag;
          ppar->cur_max_flag|= key_tree->max_flag;
          
          res= find_used_partitions(ppar, key_tree->next_key_part);
           
          /* Restore 'parameters' back */
          ppar->cur_min_key= min_key;
          ppar->cur_max_key= max_key;

          ppar->cur_min_flag= save_min_flag;
          ppar->cur_max_flag= save_max_flag;
          goto pop_and_go_right;
        }
        /* We have arrived at the last field in the partition pruning */
        uint tmp_min_flag= key_tree->min_flag,
             tmp_max_flag= key_tree->max_flag;
        if (!tmp_min_flag)
          key_tree->next_key_part->store_min_key(ppar->key,
                                                 &tmp_min_key,
                                                 &tmp_min_flag,
                                                 ppar->last_part_partno);
        if (!tmp_max_flag)
          key_tree->next_key_part->store_max_key(ppar->key,
                                                 &tmp_max_key,
                                                 &tmp_max_flag,
                                                 ppar->last_part_partno);
        flag= tmp_min_flag | tmp_max_flag;
      }
      else
        flag= key_tree->min_flag | key_tree->max_flag;
      
      if (tmp_min_key != range_par->min_key)
        flag&= ~NO_MIN_RANGE;
      else
        flag|= NO_MIN_RANGE;
      if (tmp_max_key != range_par->max_key)
        flag&= ~NO_MAX_RANGE;
      else
        flag|= NO_MAX_RANGE;

      /*
        We need to call the interval mapper if we have a condition which
        makes sense to prune on. In the example of COLUMNS on a and
        b it makes sense if we have a condition on a, or conditions on
        both a and b. If we only have conditions on b it might make sense
        but this is a harder case we will solve later. For the harder case
        this clause then turns into use of all partitions and thus we
        simply set res= -1 as if the mapper had returned that.
        TODO: What to do here is defined in WL#4065.
      */
      if (ppar->arg_stack[0]->part == 0)
      {
        uint32 i;
        uint32 store_length_array[MAX_KEY];
        uint32 num_keys= ppar->part_fields;

        for (i= 0; i < num_keys; i++)
          store_length_array[i]= ppar->key[i].store_length;
        res= ppar->part_info->
             get_part_iter_for_interval(ppar->part_info,
                                        FALSE,
                                        store_length_array,
                                        range_par->min_key,
                                        range_par->max_key,
                                        tmp_min_key - range_par->min_key,
                                        tmp_max_key - range_par->max_key,
                                        flag,
                                        &ppar->part_iter);
        if (!res)
          goto pop_and_go_right; /* res==0 --> no satisfying partitions */
      }
      else
        res= -1;

      if (res == -1)
      {
        /* get a full range iterator */
        init_all_partitions_iterator(ppar->part_info, &ppar->part_iter);
      }
      /* 
        Save our intent to mark full partition as used if we will not be able
        to obtain further limits on subpartitions
      */
      if (key_tree_part < ppar->last_part_partno)
      {
        /*
          We need to ignore the rest of the partitioning fields in all
          evaluations after this
        */
        did_set_ignore_part_fields= TRUE;
        ppar->ignore_part_fields= TRUE;
      }
      set_full_part_if_bad_ret= TRUE;
      goto process_next_key_part;
    }

    if (key_tree_part == ppar->last_subpart_partno && 
        (NULL != ppar->part_info->get_subpart_iter_for_interval))
    {
      PARTITION_ITERATOR subpart_iter;
      DBUG_EXECUTE("info", dbug_print_segment_range(key_tree,
                                                    range_par->key_parts););
      res= ppar->part_info->
           get_subpart_iter_for_interval(ppar->part_info,
                                         TRUE,
                                         NULL, /* Currently not used here */
                                         key_tree->min_value, 
                                         key_tree->max_value,
                                         0, 0, /* Those are ignored here */
                                         key_tree->min_flag |
                                           key_tree->max_flag,
                                         &subpart_iter);
      if (res == 0)
      {
        /*
           The only case where we can get "no satisfying subpartitions"
           returned from the above call is when an error has occurred.
        */
        DBUG_ASSERT(range_par->thd->is_error());
        return 0;
      }

      if (res == -1)
        goto pop_and_go_right; /* all subpartitions satisfy */

      uint32 subpart_id;
      bitmap_clear_all(&ppar->subparts_bitmap);
      while ((subpart_id= subpart_iter.get_next(&subpart_iter)) !=
             NOT_A_PARTITION_ID)
        bitmap_set_bit(&ppar->subparts_bitmap, subpart_id);

      /* Mark each partition as used in each subpartition.  */
      uint32 part_id;
      while ((part_id= ppar->part_iter.get_next(&ppar->part_iter)) !=
              NOT_A_PARTITION_ID)
      {
        for (uint i= 0; i < ppar->part_info->num_subparts; i++)
          if (bitmap_is_set(&ppar->subparts_bitmap, i))
            bitmap_set_bit(&ppar->part_info->read_partitions,
                           part_id * ppar->part_info->num_subparts + i);
      }
      goto pop_and_go_right;
    }

    if (key_tree->is_singlepoint())
    {
      if (key_tree_part == ppar->last_part_partno &&
          ppar->cur_part_fields == ppar->part_fields &&
          ppar->part_info->get_part_iter_for_interval == NULL)
      {
        /* 
          Ok, we've got "fieldN<=>constN"-type SEL_ARGs for all partitioning
          fields. Save all constN constants into table record buffer.
        */
        store_selargs_to_rec(ppar, ppar->arg_stack, ppar->part_fields);
        DBUG_EXECUTE("info", dbug_print_singlepoint_range(ppar->arg_stack,
                                                       ppar->part_fields););
        uint32 part_id;
        longlong func_value;
        /* Find in which partition the {const1, ...,constN} tuple goes */
        if (ppar->get_top_partition_id_func(ppar->part_info, &part_id,
                                            &func_value))
        {
          res= 0; /* No satisfying partitions */
          goto pop_and_go_right;
        }
        /* Rembember the limit we got - single partition #part_id */
        init_single_partition_iterator(part_id, &ppar->part_iter);
        
        /*
          If there are no subpartitions/we fail to get any limit for them, 
          then we'll mark full partition as used. 
        */
        set_full_part_if_bad_ret= TRUE;
        goto process_next_key_part;
      }

      if (key_tree_part == ppar->last_subpart_partno &&
          ppar->cur_subpart_fields == ppar->subpart_fields)
      {
        /* 
          Ok, we've got "fieldN<=>constN"-type SEL_ARGs for all subpartitioning
          fields. Save all constN constants into table record buffer.
        */
        store_selargs_to_rec(ppar, ppar->arg_stack_end - ppar->subpart_fields,
                             ppar->subpart_fields);
        DBUG_EXECUTE("info", dbug_print_singlepoint_range(ppar->arg_stack_end- 
                                                       ppar->subpart_fields,
                                                       ppar->subpart_fields););
        /* Find the subpartition (it's HASH/KEY so we always have one) */
        partition_info *part_info= ppar->part_info;
        uint32 part_id, subpart_id;
                 
        if (part_info->get_subpartition_id(part_info, &subpart_id))
          return 0;

        /* Mark this partition as used in each subpartition. */
        while ((part_id= ppar->part_iter.get_next(&ppar->part_iter)) !=
                NOT_A_PARTITION_ID)
        {
          bitmap_set_bit(&part_info->read_partitions,
                         part_id * part_info->num_subparts + subpart_id);
        }
        res= 1; /* Some partitions were marked as used */
        goto pop_and_go_right;
      }
    }
    else
    {
      /* 
        Can't handle condition on current key part. If we're that deep that 
        we're processing subpartititoning's key parts, this means we'll not be
        able to infer any suitable condition, so bail out.
      */
      if (key_tree_part >= ppar->last_part_partno)
      {
        res= -1;
        goto pop_and_go_right;
      }
      /*
        No meaning in continuing with rest of partitioning key parts.
        Will try to continue with subpartitioning key parts.
      */
      ppar->ignore_part_fields= true;
      did_set_ignore_part_fields= true;
      goto process_next_key_part;
    }
  }

process_next_key_part:
  if (key_tree->next_key_part)
    res= find_used_partitions(ppar, key_tree->next_key_part);
  else
    res= -1;

  if (did_set_ignore_part_fields)
  {
    /*
      We have returned from processing all key trees linked to our next
      key part. We are ready to be moving down (using right pointers) and
      this tree is a new evaluation requiring its own decision on whether
      to ignore partitioning fields.
    */
    ppar->ignore_part_fields= FALSE;
  }
  if (set_full_part_if_bad_ret)
  {
    if (res == -1)
    {
      /* Got "full range" for subpartitioning fields */
      uint32 part_id;
      bool found= FALSE;
      while ((part_id= ppar->part_iter.get_next(&ppar->part_iter)) !=
             NOT_A_PARTITION_ID)
      {
        ppar->mark_full_partition_used(ppar->part_info, part_id);
        found= TRUE;
      }
      res= MY_TEST(found);
    }
    /*
      Restore the "used partitions iterator" to the default setting that
      specifies iteration over all partitions.
    */
    init_all_partitions_iterator(ppar->part_info, &ppar->part_iter);
  }

pop_and_go_right:
  /* Pop this key part info off the "stack" */
  ppar->arg_stack_end--;
  ppar->cur_part_fields-=    ppar->is_part_keypart[key_tree_part];
  ppar->cur_subpart_fields-= ppar->is_subpart_keypart[key_tree_part];

  if (res == -1)
    return -1;
  if (key_tree->right != &null_element)
  {
    if (-1 == (right_res= find_used_partitions(ppar,key_tree->right)))
      return -1;
  }
  return (left_res || right_res || res);
}
 

static void mark_all_partitions_as_used(partition_info *part_info)
{
  bitmap_copy(&(part_info->read_partitions),
              &(part_info->lock_partitions));
}


/*
  Check if field types allow to construct partitioning index description
 
  SYNOPSIS
    fields_ok_for_partition_index()
      pfield  NULL-terminated array of pointers to fields.

  DESCRIPTION
    For an array of fields, check if we can use all of the fields to create
    partitioning index description.
    
    We can't process GEOMETRY fields - for these fields singlepoint intervals
    cant be generated, and non-singlepoint are "special" kinds of intervals
    to which our processing logic can't be applied.

    It is not known if we could process ENUM fields, so they are disabled to be
    on the safe side.

  RETURN 
    TRUE   Yes, fields can be used in partitioning index
    FALSE  Otherwise
*/

static bool fields_ok_for_partition_index(Field **pfield)
{
  if (!pfield)
    return FALSE;
  for (; (*pfield); pfield++)
  {
    enum_field_types ftype= (*pfield)->real_type();
    if (ftype == MYSQL_TYPE_ENUM || ftype == MYSQL_TYPE_GEOMETRY)
      return FALSE;
  }
  return TRUE;
}


/*
  Create partition index description and fill related info in the context
  struct

  SYNOPSIS
    create_partition_index_description()
      prune_par  INOUT Partition pruning context

  DESCRIPTION
    Create partition index description. Partition index description is:

      part_index(used_fields_list(part_expr), used_fields_list(subpart_expr))

    If partitioning/sub-partitioning uses BLOB or Geometry fields, then
    corresponding fields_list(...) is not included into index description
    and we don't perform partition pruning for partitions/subpartitions.

  RETURN
    TRUE   Out of memory or can't do partition pruning at all
    FALSE  OK
*/

static bool create_partition_index_description(PART_PRUNE_PARAM *ppar)
{
  RANGE_OPT_PARAM *range_par= &(ppar->range_param);
  partition_info *part_info= ppar->part_info;
  uint used_part_fields, used_subpart_fields;

  used_part_fields= fields_ok_for_partition_index(part_info->part_field_array) ?
                      part_info->num_part_fields : 0;
  used_subpart_fields= 
    fields_ok_for_partition_index(part_info->subpart_field_array)? 
      part_info->num_subpart_fields : 0;
  
  uint total_parts= used_part_fields + used_subpart_fields;

  ppar->ignore_part_fields= FALSE;
  ppar->part_fields=      used_part_fields;
  ppar->last_part_partno= (int)used_part_fields - 1;

  ppar->subpart_fields= used_subpart_fields;
  ppar->last_subpart_partno= 
    used_subpart_fields?(int)(used_part_fields + used_subpart_fields - 1): -1;

  if (part_info->is_sub_partitioned())
  {
    ppar->mark_full_partition_used=  mark_full_partition_used_with_parts;
    ppar->get_top_partition_id_func= part_info->get_part_partition_id;
  }
  else
  {
    ppar->mark_full_partition_used=  mark_full_partition_used_no_parts;
    ppar->get_top_partition_id_func= part_info->get_partition_id;
  }

  KEY_PART *key_part;
  MEM_ROOT *alloc= range_par->mem_root;
  if (!total_parts || 
      !(key_part= (KEY_PART*)alloc_root(alloc, sizeof(KEY_PART)*
                                               total_parts)) ||
      !(ppar->arg_stack= (SEL_ARG**)alloc_root(alloc, sizeof(SEL_ARG*)* 
                                                      total_parts)) ||
      !(ppar->is_part_keypart= (my_bool*)alloc_root(alloc, sizeof(my_bool)*
                                                           total_parts)) ||
      !(ppar->is_subpart_keypart= (my_bool*)alloc_root(alloc, sizeof(my_bool)*
                                                           total_parts)))
    return TRUE;
 
  if (ppar->subpart_fields)
  {
    my_bitmap_map *buf;
    uint32 bufsize= bitmap_buffer_size(ppar->part_info->num_subparts);
    if (!(buf= (my_bitmap_map*) alloc_root(alloc, bufsize)))
      return TRUE;
    bitmap_init(&ppar->subparts_bitmap, buf, ppar->part_info->num_subparts,
                FALSE);
  }
  range_par->key_parts= key_part;
  Field **field= (ppar->part_fields)? part_info->part_field_array :
                                           part_info->subpart_field_array;
  bool in_subpart_fields= FALSE;
  for (uint part= 0; part < total_parts; part++, key_part++)
  {
    key_part->key=          0;
    key_part->part=	    part;
    key_part->length= (uint16)(*field)->key_length();
    key_part->store_length= (uint16)get_partition_field_store_length(*field);

    DBUG_PRINT("info", ("part %u length %u store_length %u", part,
                         key_part->length, key_part->store_length));

    key_part->field=        (*field);
    key_part->image_type =  Field::itRAW;
    /* 
      We set keypart flag to 0 here as the only HA_PART_KEY_SEG is checked
      in the RangeAnalysisModule.
    */
    key_part->flag=         0;
    /* We don't set key_parts->null_bit as it will not be used */

    ppar->is_part_keypart[part]= !in_subpart_fields;
    ppar->is_subpart_keypart[part]= in_subpart_fields;

    /*
      Check if this was last field in this array, in this case we
      switch to subpartitioning fields. (This will only happens if
      there are subpartitioning fields to cater for).
    */
    if (!*(++field))
    {
      field= part_info->subpart_field_array;
      in_subpart_fields= TRUE;
    }
  }
  range_par->key_parts_end= key_part;

  DBUG_EXECUTE("info", print_partitioning_index(range_par->key_parts,
                                                range_par->key_parts_end););
  return FALSE;
}


#ifndef DBUG_OFF

static void print_partitioning_index(KEY_PART *parts, KEY_PART *parts_end)
{
  DBUG_ENTER("print_partitioning_index");
  DBUG_LOCK_FILE;
  fprintf(DBUG_FILE, "partitioning INDEX(");
  for (KEY_PART *p=parts; p != parts_end; p++)
  {
    fprintf(DBUG_FILE, "%s%s", p==parts?"":" ,", p->field->field_name);
  }
  fputs(");\n", DBUG_FILE);
  DBUG_UNLOCK_FILE;
  DBUG_VOID_RETURN;
}


/* Print a "c1 < keypartX < c2" - type interval into debug trace. */
static void dbug_print_segment_range(SEL_ARG *arg, KEY_PART *part)
{
  DBUG_ENTER("dbug_print_segment_range");
  DBUG_LOCK_FILE;
  if (!(arg->min_flag & NO_MIN_RANGE))
  {
    store_key_image_to_rec(part->field, arg->min_value, part->length);
    part->field->dbug_print();
    if (arg->min_flag & NEAR_MIN)
      fputs(" < ", DBUG_FILE);
    else
      fputs(" <= ", DBUG_FILE);
  }

  fprintf(DBUG_FILE, "%s", part->field->field_name);

  if (!(arg->max_flag & NO_MAX_RANGE))
  {
    if (arg->max_flag & NEAR_MAX)
      fputs(" < ", DBUG_FILE);
    else
      fputs(" <= ", DBUG_FILE);
    store_key_image_to_rec(part->field, arg->max_value, part->length);
    part->field->dbug_print();
  }
  fputs("\n", DBUG_FILE);
  DBUG_UNLOCK_FILE;
  DBUG_VOID_RETURN;
}


/*
  Print a singlepoint multi-keypart range interval to debug trace
 
  SYNOPSIS
    dbug_print_singlepoint_range()
      start  Array of SEL_ARG* ptrs representing conditions on key parts
      num    Number of elements in the array.

  DESCRIPTION
    This function prints a "keypartN=constN AND ... AND keypartK=constK"-type 
    interval to debug trace.
*/

static void dbug_print_singlepoint_range(SEL_ARG **start, uint num)
{
  DBUG_ENTER("dbug_print_singlepoint_range");
  DBUG_LOCK_FILE;
  SEL_ARG **end= start + num;

  for (SEL_ARG **arg= start; arg != end; arg++)
  {
    Field *field= (*arg)->field;
    fprintf(DBUG_FILE, "%s%s=", (arg==start)?"":", ", field->field_name);
    field->dbug_print();
  }
  fputs("\n", DBUG_FILE);
  DBUG_UNLOCK_FILE;
  DBUG_VOID_RETURN;
}
#endif

/****************************************************************************
 * Partition pruning code ends
 ****************************************************************************/


/*
  Get best plan for a SEL_IMERGE disjunctive expression.
  SYNOPSIS
    get_best_disjunct_quick()
      param     Parameter from check_quick_select function
      imerge    Expression to use
      cost_est  Don't create scans with cost > cost_est

  NOTES
    index_merge cost is calculated as follows:
    index_merge_cost =
      cost(index_reads) +         (see #1)
      cost(rowid_to_row_scan) +   (see #2)
      cost(unique_use)            (see #3)

    1. cost(index_reads) =SUM_i(cost(index_read_i))
       For non-CPK scans,
         cost(index_read_i) = {cost of ordinary 'index only' scan}
       For CPK scan,
         cost(index_read_i) = {cost of non-'index only' scan}

    2. cost(rowid_to_row_scan)
      If table PK is clustered then
        cost(rowid_to_row_scan) =
          {cost of ordinary clustered PK scan with n_ranges=n_rows}

      Otherwise, we use the following model to calculate costs:
      We need to retrieve n_rows rows from file that occupies n_blocks blocks.
      We assume that offsets of rows we need are independent variates with
      uniform distribution in [0..max_file_offset] range.

      We'll denote block as "busy" if it contains row(s) we need to retrieve
      and "empty" if doesn't contain rows we need.

      Probability that a block is empty is (1 - 1/n_blocks)^n_rows (this
      applies to any block in file). Let x_i be a variate taking value 1 if
      block #i is empty and 0 otherwise.

      Then E(x_i) = (1 - 1/n_blocks)^n_rows;

      E(n_empty_blocks) = E(sum(x_i)) = sum(E(x_i)) =
        = n_blocks * ((1 - 1/n_blocks)^n_rows) =
       ~= n_blocks * exp(-n_rows/n_blocks).

      E(n_busy_blocks) = n_blocks*(1 - (1 - 1/n_blocks)^n_rows) =
       ~= n_blocks * (1 - exp(-n_rows/n_blocks)).

      Average size of "hole" between neighbor non-empty blocks is
           E(hole_size) = n_blocks/E(n_busy_blocks).

      The total cost of reading all needed blocks in one "sweep" is:

        E(n_busy_blocks) * disk_seek_cost(n_blocks/E(n_busy_blocks))

      This cost estimate is calculated in get_sweep_read_cost().

    3. Cost of Unique use is calculated in Unique::get_use_cost function.

  ROR-union cost is calculated in the same way index_merge, but instead of
  Unique a priority queue is used.

  RETURN
    Created read plan
    NULL - Out of memory or no read scan could be built.
*/

static
TABLE_READ_PLAN *get_best_disjunct_quick(PARAM *param, SEL_IMERGE *imerge,
                                         const Cost_estimate *cost_est)
{
  SEL_TREE **ptree;
  TRP_INDEX_MERGE *imerge_trp= NULL;
  uint n_child_scans= imerge->trees_next - imerge->trees;
  TRP_RANGE **range_scans;
  TRP_RANGE **cur_child;
  TRP_RANGE **cpk_scan= NULL;
  bool imerge_too_expensive= FALSE;
  Cost_estimate imerge_cost;
  ha_rows cpk_scan_records= 0;
  ha_rows non_cpk_scan_records= 0;
  bool pk_is_clustered= param->table->file->primary_key_is_clustered();
  bool all_scans_ror_able= TRUE;
  bool all_scans_rors= TRUE;
  size_t unique_calc_buff_size;
  TABLE_READ_PLAN **roru_read_plans;
  TABLE_READ_PLAN **cur_roru_plan;
  ha_rows roru_total_records;
  double roru_intersect_part= 1.0;
  const Cost_model_table *const cost_model= param->table->cost_model();
  Cost_estimate read_cost= *cost_est;

  DBUG_ENTER("get_best_disjunct_quick");
  DBUG_PRINT("info", ("Full table scan cost: %g", cost_est->total_cost()));

  DBUG_ASSERT(param->table->file->stats.records);

  Opt_trace_context * const trace= &param->thd->opt_trace;
  Opt_trace_object trace_best_disjunct(trace);
  if (!(range_scans= (TRP_RANGE**)alloc_root(param->mem_root,
                                             sizeof(TRP_RANGE*)*
                                             n_child_scans)))
    DBUG_RETURN(NULL);
  // Note: to_merge.end() is called to close this object after this for-loop.
  Opt_trace_array to_merge(trace, "indexes_to_merge");
  /*
    Collect best 'range' scan for each of disjuncts, and, while doing so,
    analyze possibility of ROR scans. Also calculate some values needed by
    other parts of the code.
  */
  for (ptree= imerge->trees, cur_child= range_scans;
       ptree != imerge->trees_next;
       ptree++, cur_child++)
  {
    DBUG_EXECUTE("info", print_sel_tree(param, *ptree, &(*ptree)->keys_map,
                                        "tree in SEL_IMERGE"););
    Opt_trace_object trace_idx(trace);
    if (!(*cur_child=
          get_key_scans_params(param, *ptree, true, false, &read_cost)))
    {
      /*
        One of index scans in this index_merge is more expensive than entire
        table read for another available option. The entire index_merge (and
        any possible ROR-union) will be more expensive then, too. We continue
        here only to update SQL_SELECT members.
      */
      imerge_too_expensive= true;
    }
    if (imerge_too_expensive)
    {
      trace_idx.add("chosen", false).add_alnum("cause", "cost");
      continue;
    }

    const uint keynr_in_table= param->real_keynr[(*cur_child)->key_idx];
    imerge_cost+= (*cur_child)->cost_est;
    all_scans_ror_able &= ((*ptree)->n_ror_scans > 0);
    all_scans_rors &= (*cur_child)->is_ror;
    if (pk_is_clustered &&
        keynr_in_table == param->table->s->primary_key)
    {
      cpk_scan= cur_child;
      cpk_scan_records= (*cur_child)->records;
    }
    else
      non_cpk_scan_records += (*cur_child)->records;

    trace_idx.
      add_utf8("index_to_merge", param->table->key_info[keynr_in_table].name).
      add("cumulated_cost", imerge_cost);
  }

  // Note: to_merge trace object is closed here
  to_merge.end();


  trace_best_disjunct.add("cost_of_reading_ranges", imerge_cost);
  if (imerge_too_expensive || (imerge_cost > read_cost) ||
      ((non_cpk_scan_records+cpk_scan_records >= param->table->file->stats.records) &&
      !read_cost.is_max_cost()))
  {
    /*
      Bail out if it is obvious that both index_merge and ROR-union will be
      more expensive
    */
    DBUG_PRINT("info", ("Sum of index_merge scans is more expensive than "
                        "full table scan, bailing out"));
    trace_best_disjunct.add("chosen", false).add_alnum("cause", "cost");
    DBUG_RETURN(NULL);
  }

  /* 
    If all scans happen to be ROR, proceed to generate a ROR-union plan (it's 
    guaranteed to be cheaper than non-ROR union), unless ROR-unions are
    disabled in @@optimizer_switch
  */
  if (all_scans_rors && 
      param->index_merge_union_allowed)
  {
    roru_read_plans= (TABLE_READ_PLAN**)range_scans;
    trace_best_disjunct.add("use_roworder_union", true).
      add_alnum("cause", "always_cheaper_than_not_roworder_retrieval");
    goto skip_to_ror_scan;
  }

  if (cpk_scan)
  {
    /*
      Add one rowid/key comparison for each row retrieved on non-CPK
      scan. (it is done in QUICK_RANGE_SELECT::row_in_ranges)
    */
    const double rid_comp_cost=
      cost_model->key_compare_cost(static_cast<double>(non_cpk_scan_records));
    imerge_cost.add_cpu(rid_comp_cost);
    trace_best_disjunct.add("cost_of_mapping_rowid_in_non_clustered_pk_scan",
                            rid_comp_cost);
  }

  /* Calculate cost(rowid_to_row_scan) */
  {
    Cost_estimate sweep_cost;
    JOIN *join= param->thd->lex->select_lex->join;
    const bool is_interrupted= join && join->tables != 1;
    get_sweep_read_cost(param->table, non_cpk_scan_records, is_interrupted,
                        &sweep_cost);
    imerge_cost+= sweep_cost;
    trace_best_disjunct.add("cost_sort_rowid_and_read_disk",
                            sweep_cost);
  }
  DBUG_PRINT("info",("index_merge cost with rowid-to-row scan: %g",
                     imerge_cost.total_cost()));
  if (imerge_cost > read_cost || 
      !param->index_merge_sort_union_allowed)
  {
    trace_best_disjunct.add("use_roworder_index_merge", true).
      add_alnum("cause", "cost");
    goto build_ror_index_merge;
  }

  /* Add Unique operations cost */
  unique_calc_buff_size=
    Unique::get_cost_calc_buff_size((ulong)non_cpk_scan_records,
                                    param->table->file->ref_length,
                                    param->thd->variables.sortbuff_size);
  if (param->imerge_cost_buff.size() < unique_calc_buff_size)
  {
    typedef Unique::Imerge_cost_buf_type::value_type element_type;
    void *rawmem=
      alloc_root(param->mem_root, unique_calc_buff_size * sizeof(element_type));
    if (!rawmem)
      DBUG_RETURN(NULL);
    param->imerge_cost_buff=
      Unique::Imerge_cost_buf_type(static_cast<element_type*>(rawmem),
                                   unique_calc_buff_size);
  }

  {
    const double dup_removal_cost=
      Unique::get_use_cost(param->imerge_cost_buff,
                           (uint)non_cpk_scan_records,
                           param->table->file->ref_length,
                           param->thd->variables.sortbuff_size,
                           cost_model);

    trace_best_disjunct.add("cost_duplicate_removal", dup_removal_cost);
    imerge_cost.add_cpu(dup_removal_cost);
 
    trace_best_disjunct.add("total_cost", imerge_cost);
    DBUG_PRINT("info",("index_merge total cost: %g (wanted: less then %g)",
              imerge_cost.total_cost(), read_cost.total_cost()));
  }
  if (imerge_cost < read_cost)
  {
    if ((imerge_trp= new (param->mem_root)TRP_INDEX_MERGE))
    {
      imerge_trp->cost_est= imerge_cost;
      imerge_trp->records= non_cpk_scan_records + cpk_scan_records;
      imerge_trp->records= min(imerge_trp->records,
                               param->table->file->stats.records);
      imerge_trp->range_scans= range_scans;
      imerge_trp->range_scans_end= range_scans + n_child_scans;
      read_cost= imerge_cost;
    }
  }

build_ror_index_merge:
  if (!all_scans_ror_able ||
      param->thd->lex->sql_command == SQLCOM_DELETE ||
      !param->index_merge_union_allowed)
    DBUG_RETURN(imerge_trp);

  /* Ok, it is possible to build a ROR-union, try it. */
  if (!(roru_read_plans=
          (TABLE_READ_PLAN**)alloc_root(param->mem_root,
                                        sizeof(TABLE_READ_PLAN*)*
                                        n_child_scans)))
    DBUG_RETURN(imerge_trp);
skip_to_ror_scan:
  Cost_estimate roru_index_cost;
  roru_total_records= 0;
  cur_roru_plan= roru_read_plans;

  /*
    Note: trace_analyze_ror.end() is called to close this object after
    this for-loop.
  */
  Opt_trace_array trace_analyze_ror(trace, "analyzing_roworder_scans");
  /* Find 'best' ROR scan for each of trees in disjunction */
  for (ptree= imerge->trees, cur_child= range_scans;
       ptree != imerge->trees_next;
       ptree++, cur_child++, cur_roru_plan++)
  {
    Opt_trace_object trp_info(trace);
    if (unlikely(trace->is_started()))
      (*cur_child)->trace_basic_info(param, &trp_info);

    /*
      Assume the best ROR scan is the one that has cheapest
      full-row-retrieval scan cost.
      Also accumulate index_only scan costs as we'll need them to
      calculate overall index_intersection cost.
    */
    Cost_estimate scan_cost;
    if ((*cur_child)->is_ror)
    {
      /* Ok, we have index_only cost, now get full rows scan cost */
      scan_cost=
        param->table->file->read_cost(param->real_keynr[(*cur_child)->key_idx],
          1, static_cast<double>((*cur_child)->records));
      scan_cost.add_cpu(
            cost_model->row_evaluate_cost(rows2double((*cur_child)->records)));
    }
    else
      scan_cost= read_cost;

    TABLE_READ_PLAN *prev_plan= *cur_child;
    if (!(*cur_roru_plan= get_best_ror_intersect(param, *ptree, &scan_cost)))
    {
      if (prev_plan->is_ror)
        *cur_roru_plan= prev_plan;
      else
        DBUG_RETURN(imerge_trp);
      roru_index_cost += (*cur_roru_plan)->cost_est;
    }
    else
    {
      roru_index_cost +=
        ((TRP_ROR_INTERSECT*)(*cur_roru_plan))->index_scan_cost;
    }
    roru_total_records += (*cur_roru_plan)->records;
    roru_intersect_part *= (*cur_roru_plan)->records /
      param->table->file->stats.records;
  }
  // Note: trace_analyze_ror trace object is closed here
  trace_analyze_ror.end();

  /*
    rows to retrieve=
      SUM(rows_in_scan_i) - table_rows * PROD(rows_in_scan_i / table_rows).
    This is valid because index_merge construction guarantees that conditions
    in disjunction do not share key parts.
  */
  roru_total_records -= (ha_rows)(roru_intersect_part*
                                  param->table->file->stats.records);
  /* ok, got a ROR read plan for each of the disjuncts
    Calculate cost:
    cost(index_union_scan(scan_1, ... scan_n)) =
      SUM_i(cost_of_index_only_scan(scan_i)) +
      queue_use_cost(rowid_len, n) +
      cost_of_row_retrieval
    See get_merge_buffers_cost function for queue_use_cost formula derivation.
  */
  Cost_estimate roru_total_cost;
  {
    JOIN *join= param->thd->lex->select_lex->join;
    const bool is_interrupted= join && join->tables != 1;
    get_sweep_read_cost(param->table, roru_total_records, is_interrupted,
                        &roru_total_cost);
    roru_total_cost += roru_index_cost;
    roru_total_cost.add_cpu(
      cost_model->key_compare_cost(rows2double(roru_total_records) *
                                   log((double)n_child_scans) / M_LN2));
  }

  trace_best_disjunct.add("index_roworder_union_cost",
                          roru_total_cost).
    add("members", n_child_scans);
  TRP_ROR_UNION* roru;
  if (roru_total_cost < read_cost)
  {
    if ((roru= new (param->mem_root) TRP_ROR_UNION))
    {
      trace_best_disjunct.add("chosen", true);
      roru->first_ror= roru_read_plans;
      roru->last_ror= roru_read_plans + n_child_scans;
      roru->cost_est= roru_total_cost;
      roru->records= roru_total_records;
      DBUG_RETURN(roru);
    }
  }
  trace_best_disjunct.add("chosen", false);

  DBUG_RETURN(imerge_trp);
}


/*
  Create ROR_SCAN_INFO* structure with a single ROR scan on index idx using
  sel_arg set of intervals.

  SYNOPSIS
    make_ror_scan()
      param    Parameter from test_quick_select function
      idx      Index of key in param->keys
      sel_arg  Set of intervals for a given key

  RETURN
    NULL - out of memory
    ROR scan structure containing a scan for {idx, sel_arg}
*/

static
ROR_SCAN_INFO *make_ror_scan(const PARAM *param, int idx, SEL_ARG *sel_arg)
{
  ROR_SCAN_INFO *ror_scan;
  my_bitmap_map *bitmap_buf1;
  my_bitmap_map *bitmap_buf2;
  uint keynr;
  DBUG_ENTER("make_ror_scan");

  if (!(ror_scan= (ROR_SCAN_INFO*)alloc_root(param->mem_root,
                                             sizeof(ROR_SCAN_INFO))))
    DBUG_RETURN(NULL);

  ror_scan->idx= idx;
  ror_scan->keynr= keynr= param->real_keynr[idx];
  ror_scan->sel_arg= sel_arg;
  ror_scan->records= param->table->quick_rows[keynr];

  if (!(bitmap_buf1= (my_bitmap_map*) alloc_root(param->mem_root,
                                                 param->fields_bitmap_size)))
    DBUG_RETURN(NULL);
  if (!(bitmap_buf2= (my_bitmap_map*) alloc_root(param->mem_root,
                                                 param->fields_bitmap_size)))
    DBUG_RETURN(NULL);

  if (bitmap_init(&ror_scan->covered_fields, bitmap_buf1,
                  param->table->s->fields, FALSE))
    DBUG_RETURN(NULL);
  if (bitmap_init(&ror_scan->covered_fields_remaining, bitmap_buf2,
                  param->table->s->fields, FALSE))
    DBUG_RETURN(NULL);

  bitmap_clear_all(&ror_scan->covered_fields);

  KEY_PART_INFO *key_part= param->table->key_info[keynr].key_part;
  KEY_PART_INFO *key_part_end=
    key_part + param->table->key_info[keynr].user_defined_key_parts;
  for (;key_part != key_part_end; ++key_part)
  {
    if (bitmap_is_set(&param->needed_fields, key_part->fieldnr-1))
      bitmap_set_bit(&ror_scan->covered_fields, key_part->fieldnr-1);
  }
  bitmap_copy(&ror_scan->covered_fields_remaining, &ror_scan->covered_fields);

  double rows= rows2double(param->table->quick_rows[ror_scan->keynr]);
  ror_scan->index_read_cost=
    param->table->file->index_scan_cost(ror_scan->keynr, 1, rows);
  DBUG_RETURN(ror_scan);
}


/**
  Compare two ROR_SCAN_INFO* by
    1. #fields in this index that are not already covered
       by other indexes earlier in the intersect ordering: descending
    2. E(#records): ascending

  @param scan1   first ror scan to compare
  @param scan2   second ror scan to compare

  @return true if scan1 > scan2, false otherwise
*/
static bool is_better_intersect_match(const ROR_SCAN_INFO *scan1,
                                      const ROR_SCAN_INFO *scan2)
{
  if (scan1 == scan2)
    return false;

  if (scan1->num_covered_fields_remaining >
      scan2->num_covered_fields_remaining)
    return false;

  if (scan1->num_covered_fields_remaining <
      scan2->num_covered_fields_remaining)
    return true;

  return (scan1->records > scan2->records);
}

/**
  Sort indexes in an order that is likely to be a good index merge
  intersection order. After running this function, [start, ..., end-1]
  is ordered according to this strategy:

    1) Minimize the number of indexes that must be used in the
       intersection. I.e., the index covering most fields not already
       covered by other indexes earlier in the sort order is picked first.
    2) When multiple indexes cover equally many uncovered fields, the
       index with lowest E(#rows) is chosen.

  Note that all permutations of index ordering are not tested, so this
  function may not find the optimal order.

  @param[in,out] start     Pointer to the start of indexes that may
                           be used in index merge intersection
  @param         end       Pointer past the last index that may be used.
  @param         param     Parameter from test_quick_select function.
*/
static void find_intersect_order(ROR_SCAN_INFO **start,
                                 ROR_SCAN_INFO **end,
                                 const PARAM *param)
{
  // nothing to sort if there are only zero or one ROR scans
  if ((start == end) || (start + 1 == end))
    return;

  /*
    Bitmap of fields we would like the ROR scans to cover. Will be
    modified by the loop below so that when we're looking for a ROR
    scan in position 'x' in the ordering, all fields covered by ROR
    scans 0,...,x-1 have been removed.
  */
  MY_BITMAP fields_to_cover;
  my_bitmap_map *map;
  if (!(map= (my_bitmap_map*) alloc_root(param->mem_root,
                                         param->fields_bitmap_size)))
    return;
  bitmap_init(&fields_to_cover, map, param->needed_fields.n_bits, FALSE);
  bitmap_copy(&fields_to_cover, &param->needed_fields);

  // Sort ROR scans in [start,...,end-1]
  for (ROR_SCAN_INFO **place= start; place < (end - 1); place++)
  {
    /* Placeholder for the best ROR scan found for position 'place' so far */
    ROR_SCAN_INFO **best= place;
    ROR_SCAN_INFO **current= place + 1;

    {
      /*
        Calculate how many fields in 'fields_to_cover' not already
        covered by [start,...,place-1] the 'best' index covers. The
        result is used in is_better_intersect_match() and is valid
        when finding the best ROR scan for position 'place' only.
      */
      bitmap_intersect(&(*best)->covered_fields_remaining, &fields_to_cover);
      (*best)->num_covered_fields_remaining=
        bitmap_bits_set(&(*best)->covered_fields_remaining);
    }
    for (; current < end; current++)
    {
      {
        /*
          Calculate how many fields in 'fields_to_cover' not already
          covered by [start,...,place-1] the 'current' index covers.
          The result is used in is_better_intersect_match() and is
          valid when finding the best ROR scan for position 'place' only.
        */
        bitmap_intersect(&(*current)->covered_fields_remaining,
                         &fields_to_cover);
        (*current)->num_covered_fields_remaining=
          bitmap_bits_set(&(*current)->covered_fields_remaining);

        /*
          No need to compare with 'best' if 'current' does not
          contribute with uncovered fields.
        */
        if ((*current)->num_covered_fields_remaining == 0)
          continue;
      }

      if (is_better_intersect_match(*best, *current))
        best= current;
    }

    /*
      'best' is now the ROR scan that will be sorted in position
      'place'. When searching for the best ROR scans later in the sort
      sequence we do not need coverage of the fields covered by 'best'
     */
    bitmap_subtract(&fields_to_cover, &(*best)->covered_fields);
    if (best != place)
      swap_variables(ROR_SCAN_INFO*, *best, *place);

    if (bitmap_is_clear_all(&fields_to_cover))
      return;                                   // No more fields to cover
  }
}

/* Auxiliary structure for incremental ROR-intersection creation */
typedef struct
{
  const PARAM *param;
  MY_BITMAP covered_fields; /* union of fields covered by all scans */
  /*
    Fraction of table records that satisfies conditions of all scans.
    This is the number of full records that will be retrieved if a
    non-index_only index intersection will be employed.
  */
  double out_rows;
  /* TRUE if covered_fields is a superset of needed_fields */
  bool is_covering;

  ha_rows index_records; /* sum(#records to look in indexes) */
  Cost_estimate index_scan_cost; /* SUM(cost of 'index-only' scans) */
  Cost_estimate total_cost;
} ROR_INTERSECT_INFO;


/*
  Allocate a ROR_INTERSECT_INFO and initialize it to contain zero scans.

  SYNOPSIS
    ror_intersect_init()
      param         Parameter from test_quick_select

  RETURN
    allocated structure
    NULL on error
*/

static
ROR_INTERSECT_INFO* ror_intersect_init(const PARAM *param)
{
  ROR_INTERSECT_INFO *info;
  my_bitmap_map* buf;
  if (!(info= (ROR_INTERSECT_INFO*)alloc_root(param->mem_root,
                                              sizeof(ROR_INTERSECT_INFO))))
    return NULL;
  info->param= param;
  if (!(buf= (my_bitmap_map*) alloc_root(param->mem_root,
                                         param->fields_bitmap_size)))
    return NULL;
  if (bitmap_init(&info->covered_fields, buf, param->table->s->fields,
                  FALSE))
    return NULL;
  info->is_covering= FALSE;
  info->index_scan_cost.reset();
  info->total_cost.reset();
  info->index_records= 0;
  info->out_rows= (double) param->table->file->stats.records;
  bitmap_clear_all(&info->covered_fields);
  return info;
}

void ror_intersect_cpy(ROR_INTERSECT_INFO *dst, const ROR_INTERSECT_INFO *src)
{
  dst->param= src->param;
  memcpy(dst->covered_fields.bitmap, src->covered_fields.bitmap, 
         no_bytes_in_map(&src->covered_fields));
  dst->out_rows= src->out_rows;
  dst->is_covering= src->is_covering;
  dst->index_records= src->index_records;
  dst->index_scan_cost= src->index_scan_cost;
  dst->total_cost= src->total_cost;
}


/*
  Get selectivity of adding a ROR scan to the ROR-intersection.

  SYNOPSIS
    ror_scan_selectivity()
      info  ROR-interection, an intersection of ROR index scans 
      scan  ROR scan that may or may not improve the selectivity
            of 'info'
      
  NOTES
    Suppose we have conditions on several keys
    cond=k_11=c_11 AND k_12=c_12 AND ...  // key_parts of first key in 'info'
         k_21=c_21 AND k_22=c_22 AND ...  // key_parts of second key in 'info'
          ...
         k_n1=c_n1 AND k_n3=c_n3 AND ...  (1) //key_parts of 'scan'

    where k_ij may be the same as any k_pq (i.e. keys may have common parts).

    Note that for ROR retrieval, only equality conditions are usable so there
    are no open ranges (e.g., k_ij > c_ij) in 'scan' or 'info'

    A full row is retrieved if entire condition holds.

    The recursive procedure for finding P(cond) is as follows:

    First step:
    Pick 1st part of 1st key and break conjunction (1) into two parts:
      cond= (k_11=c_11 AND R)

    Here R may still contain condition(s) equivalent to k_11=c_11.
    Nevertheless, the following holds:

      P(k_11=c_11 AND R) = P(k_11=c_11) * P(R | k_11=c_11).

    Mark k_11 as fixed field (and satisfied condition) F, save P(F),
    save R to be cond and proceed to recursion step.

    Recursion step:
    We have a set of fixed fields/satisfied conditions) F, probability P(F),
    and remaining conjunction R
    Pick next key part on current key and its condition "k_ij=c_ij".
    We will add "k_ij=c_ij" into F and update P(F).
    Lets denote k_ij as t,  R = t AND R1, where R1 may still contain t. Then

     P((t AND R1)|F) = P(t|F) * P(R1|t|F) = P(t|F) * P(R1|(t AND F)) (2)

    (where '|' mean conditional probability, not "or")

    Consider the first multiplier in (2). One of the following holds:
    a) F contains condition on field used in t (i.e. t AND F = F).
      Then P(t|F) = 1

    b) F doesn't contain condition on field used in t. Then F and t are
     considered independent.

     P(t|F) = P(t|(fields_before_t_in_key AND other_fields)) =
          = P(t|fields_before_t_in_key).

     P(t|fields_before_t_in_key) = #records(fields_before_t_in_key) /
                                   #records(fields_before_t_in_key, t)

    The second multiplier is calculated by applying this step recursively.

  IMPLEMENTATION
    This function calculates the result of application of the "recursion step"
    described above for all fixed key members of a single key, accumulating set
    of covered fields, selectivity, etc.

    The calculation is conducted as follows:
    Lets denote #records(keypart1, ... keypartK) as n_k. We need to calculate

     n_{k1}      n_{k2}
    --------- * ---------  * .... (3)
     n_{k1-1}    n_{k2-1}

    where k1,k2,... are key parts which fields were not yet marked as fixed
    ( this is result of application of option b) of the recursion step for
      parts of a single key).
    Since it is reasonable to expect that most of the fields are not marked
    as fixed, we calculate (3) as

                                  n_{i1}      n_{i2}
    (3) = n_{max_key_part}  / (   --------- * ---------  * ....  )
                                  n_{i1-1}    n_{i2-1}

    where i1,i2, .. are key parts that were already marked as fixed.

    In order to minimize number of expensive records_in_range calls we
    group and reduce adjacent fractions. Note that on the optimizer's
    request, index statistics may be used instead of records_in_range
    @see RANGE_OPT_PARAM::use_index_statistics.

  RETURN
    Selectivity of given ROR scan, a number between 0 and 1. 1 means that
    adding 'scan' to the intersection does not improve the selectivity.
*/

static double ror_scan_selectivity(const ROR_INTERSECT_INFO *info, 
                                   const ROR_SCAN_INFO *scan)
{
  double selectivity_mult= 1.0;
  const TABLE * const table= info->param->table;
  const KEY_PART_INFO * const key_part= table->key_info[scan->keynr].key_part;
  /**
    key values tuple, used to store both min_range.key and
    max_range.key. This function is only called for equality ranges;
    open ranges (e.g. "min_value < X < max_value") cannot be used for
    rowid ordered retrieval, so in this function we know that
    min_range.key == max_range.key
  */
  uchar key_val[MAX_KEY_LENGTH+MAX_FIELD_WIDTH];
  uchar *key_ptr= key_val;
  SEL_ARG *sel_arg, *tuple_arg= NULL;
  key_part_map keypart_map= 0;
  bool cur_covered;
  bool prev_covered= MY_TEST(bitmap_is_set(&info->covered_fields,
                                           key_part->fieldnr-1));
  key_range min_range;
  key_range max_range;
  min_range.key= key_val;
  min_range.flag= HA_READ_KEY_EXACT;
  max_range.key= key_val;
  max_range.flag= HA_READ_AFTER_KEY;
  ha_rows prev_records= table->file->stats.records;
  DBUG_ENTER("ror_scan_selectivity");

  for (sel_arg= scan->sel_arg; sel_arg;
       sel_arg= sel_arg->next_key_part)
  {
    DBUG_PRINT("info",("sel_arg step"));
    cur_covered= MY_TEST(bitmap_is_set(&info->covered_fields,
                                       key_part[sel_arg->part].fieldnr-1));
    if (cur_covered != prev_covered)
    {
      /* create (part1val, ..., part{n-1}val) tuple. */
      bool is_null_range= false;
      ha_rows records;
      if (!tuple_arg)
      {
        tuple_arg= scan->sel_arg;
        /* Here we use the length of the first key part */
        tuple_arg->store_min(key_part[0].store_length, &key_ptr, 0);
        is_null_range|= tuple_arg->is_null_interval();
        keypart_map= 1;
      }
      while (tuple_arg->next_key_part != sel_arg)
      {
        tuple_arg= tuple_arg->next_key_part;
        tuple_arg->store_min(key_part[tuple_arg->part].store_length,
                             &key_ptr, 0);
        is_null_range|= tuple_arg->is_null_interval();
        keypart_map= (keypart_map << 1) | 1;
      }
      min_range.length= max_range.length= (size_t) (key_ptr - key_val);
      min_range.keypart_map= max_range.keypart_map= keypart_map;

      /* 
        Get the number of rows in this range. This is done by calling
        records_in_range() unless all these are true:
          1) The user has requested that index statistics should be used
             for equality ranges to avoid the incurred overhead of 
             index dives in records_in_range()
          2) The range is not on the form "x IS NULL". The reason is
             that the number of rows with this value are likely to be
             very different than the values in the index statistics
          3) Index statistics is available.
        @see key_val
      */
      if (!info->param->use_index_statistics ||        // (1)
          is_null_range ||                             // (2)
          !table->key_info[scan->keynr].
           has_records_per_key(tuple_arg->part))       // (3)
      {
        DBUG_EXECUTE_IF("crash_records_in_range", DBUG_SUICIDE(););
        DBUG_ASSERT(min_range.length > 0);
        records=
          table->file->records_in_range(scan->keynr, &min_range, &max_range);
      }
      else
      {
        // Use index statistics
        records= static_cast<ha_rows>(
          table->key_info[scan->keynr].records_per_key(tuple_arg->part));
      }

      if (cur_covered)
      {
        /* uncovered -> covered */
        double tmp= rows2double(records)/rows2double(prev_records);
        DBUG_PRINT("info", ("Selectivity multiplier: %g", tmp));
        selectivity_mult *= tmp;
        prev_records= HA_POS_ERROR;
      }
      else
      {
        /* covered -> uncovered */
        prev_records= records;
      }
    }
    prev_covered= cur_covered;
  }
  if (!prev_covered)
  {
    double tmp= rows2double(table->quick_rows[scan->keynr]) /
                rows2double(prev_records);
    DBUG_PRINT("info", ("Selectivity multiplier: %g", tmp));
    selectivity_mult *= tmp;
  }
  // Todo: This assert fires in PB sysqa RQG tests.
  // DBUG_ASSERT(selectivity_mult <= 1.0);
  DBUG_PRINT("info", ("Returning multiplier: %g", selectivity_mult));
  DBUG_RETURN(selectivity_mult);
}


/*
  Check if adding a ROR scan to a ROR-intersection reduces its cost of
  ROR-intersection and if yes, update parameters of ROR-intersection,
  including its cost.

  SYNOPSIS
    ror_intersect_add()
      param        Parameter from test_quick_select
      info         ROR-intersection structure to add the scan to.
      ror_scan     ROR scan info to add.
      is_cpk_scan  If TRUE, add the scan as CPK scan (this can be inferred
                   from other parameters and is passed separately only to
                   avoid duplicating the inference code)
      trace_costs  Optimizer trace object cost details are added to

  NOTES
    Adding a ROR scan to ROR-intersect "makes sense" iff the cost of ROR-
    intersection decreases. The cost of ROR-intersection is calculated as
    follows:

    cost= SUM_i(key_scan_cost_i) + cost_of_full_rows_retrieval

    When we add a scan the first increases and the second decreases.

    cost_of_full_rows_retrieval=
      (union of indexes used covers all needed fields) ?
        cost_of_sweep_read(E(rows_to_retrieve), rows_in_table) :
        0

    E(rows_to_retrieve) = #rows_in_table * ror_scan_selectivity(null, scan1) *
                           ror_scan_selectivity({scan1}, scan2) * ... *
                           ror_scan_selectivity({scan1,...}, scanN). 
  RETURN
    TRUE   ROR scan added to ROR-intersection, cost updated.
    FALSE  It doesn't make sense to add this ROR scan to this ROR-intersection.
*/

static bool ror_intersect_add(ROR_INTERSECT_INFO *info,
                              ROR_SCAN_INFO* ror_scan, bool is_cpk_scan,
                              Opt_trace_object *trace_costs)
{
  double selectivity_mult= 1.0;

  DBUG_ENTER("ror_intersect_add");
  DBUG_PRINT("info", ("Current out_rows= %g", info->out_rows));
  DBUG_PRINT("info", ("Adding scan on %s",
                      info->param->table->key_info[ror_scan->keynr].name));
  DBUG_PRINT("info", ("is_cpk_scan: %d",is_cpk_scan));

  selectivity_mult = ror_scan_selectivity(info, ror_scan);
  if (selectivity_mult == 1.0)
  {
    /* Don't add this scan if it doesn't improve selectivity. */
    DBUG_PRINT("info", ("The scan doesn't improve selectivity."));
    DBUG_RETURN(FALSE);
  }
  
  info->out_rows *= selectivity_mult;
  
  if (is_cpk_scan)
  {
    /*
      CPK scan is used to filter out rows. We apply filtering for each
      record of every scan. For each record we assume that one key
      compare is done:
    */
    const Cost_model_table *const cost_model= info->param->table->cost_model();
    const double idx_cost= 
      cost_model->key_compare_cost(rows2double(info->index_records));
    info->index_scan_cost.add_cpu(idx_cost);
    trace_costs->add("index_scan_cost", idx_cost);
  }
  else
  {
    info->index_records += info->param->table->quick_rows[ror_scan->keynr];
    info->index_scan_cost+= ror_scan->index_read_cost;
    trace_costs->add("index_scan_cost", ror_scan->index_read_cost);
    bitmap_union(&info->covered_fields, &ror_scan->covered_fields);
    if (!info->is_covering && bitmap_is_subset(&info->param->needed_fields,
                                               &info->covered_fields))
    {
      DBUG_PRINT("info", ("ROR-intersect is covering now"));
      info->is_covering= TRUE;
    }
  }

  info->total_cost= info->index_scan_cost;
  trace_costs->add("cumulated_index_scan_cost", 
                   info->index_scan_cost);

  if (!info->is_covering)
  {
    Cost_estimate sweep_cost;
    JOIN *join= info->param->thd->lex->select_lex->join;
    const bool is_interrupted= join && join->tables != 1;

    get_sweep_read_cost(info->param->table, double2rows(info->out_rows),
                        is_interrupted, &sweep_cost);
    info->total_cost+= sweep_cost;
    trace_costs->add("disk_sweep_cost", sweep_cost);
  }
  else
    trace_costs->add("disk_sweep_cost", 0);

  DBUG_PRINT("info", ("New out_rows: %g", info->out_rows));
  DBUG_PRINT("info", ("New cost: %g, %scovering",
                      info->total_cost.total_cost(),
                      info->is_covering?"" : "non-"));
  DBUG_RETURN(TRUE);
}


/*
  Get best ROR-intersection plan using non-covering ROR-intersection search
  algorithm. The returned plan may be covering.

  SYNOPSIS
    get_best_ror_intersect()
      param            Parameter from test_quick_select function.
      tree             Transformed restriction condition to be used to look
                       for ROR scans.
      cost_est         Do not return read plans with cost > cost_est.
      are_all_covering [out] set to TRUE if union of all scans covers all
                       fields needed by the query (and it is possible to build
                       a covering ROR-intersection)

  NOTES
    get_key_scans_params must be called before this function can be called.
    
    When this function is called by ROR-union construction algorithm it
    assumes it is building an uncovered ROR-intersection (and thus # of full
    records to be retrieved is wrong here). This is a hack.

  IMPLEMENTATION
    The approximate best non-covering plan search algorithm is as follows:

    find_min_ror_intersection_scan()
    {
      R= select all ROR scans;
      order R by (E(#records_matched) * key_record_length).

      S= first(R); -- set of scans that will be used for ROR-intersection
      R= R-first(S);
      min_cost= cost(S);
      min_scan= make_scan(S);
      while (R is not empty)
      {
        firstR= R - first(R);
        if (!selectivity(S + firstR < selectivity(S)))
          continue;
          
        S= S + first(R);
        if (cost(S) < min_cost)
        {
          min_cost= cost(S);
          min_scan= make_scan(S);
        }
      }
      return min_scan;
    }

    See ror_intersect_add function for ROR intersection costs.

    Special handling for Clustered PK scans
    Clustered PK contains all table fields, so using it as a regular scan in
    index intersection doesn't make sense: a range scan on CPK will be less
    expensive in this case.
    Clustered PK scan has special handling in ROR-intersection: it is not used
    to retrieve rows, instead its condition is used to filter row references
    we get from scans on other keys.

  RETURN
    ROR-intersection table read plan
    NULL if out of memory or no suitable plan found.
*/

static
TRP_ROR_INTERSECT *get_best_ror_intersect(const PARAM *param, SEL_TREE *tree,
                                          const Cost_estimate *cost_est)
{
  uint idx;
  Cost_estimate min_cost;
  Opt_trace_context * const trace= &param->thd->opt_trace;
  DBUG_ENTER("get_best_ror_intersect");

  Opt_trace_object trace_ror(trace, "analyzing_roworder_intersect");

  min_cost.set_max_cost();

  if ((tree->n_ror_scans < 2) || !param->table->file->stats.records ||
      !param->index_merge_intersect_allowed)
  {
    trace_ror.add("usable", false);
    if (tree->n_ror_scans < 2)
      trace_ror.add_alnum("cause", "too_few_roworder_scans");
    else
      trace_ror.add("need_tracing", true);
    DBUG_RETURN(NULL);
  }

  if (param->order_direction == ORDER::ORDER_DESC)
    DBUG_RETURN(NULL);

  /*
    Step1: Collect ROR-able SEL_ARGs and create ROR_SCAN_INFO for each of 
    them. Also find and save clustered PK scan if there is one.
  */
  ROR_SCAN_INFO **cur_ror_scan;
  ROR_SCAN_INFO *cpk_scan= NULL;
  uint cpk_no;
  bool cpk_scan_used= FALSE;

  if (!(tree->ror_scans= (ROR_SCAN_INFO**)alloc_root(param->mem_root,
                                                     sizeof(ROR_SCAN_INFO*)*
                                                     param->keys)))
    DBUG_RETURN(NULL);
  cpk_no= ((param->table->file->primary_key_is_clustered()) ?
           param->table->s->primary_key : MAX_KEY);

  for (idx= 0, cur_ror_scan= tree->ror_scans; idx < param->keys; idx++)
  {
    ROR_SCAN_INFO *scan;
    if (!tree->ror_scans_map.is_set(idx))
      continue;
    if (!(scan= make_ror_scan(param, idx, tree->keys[idx])))
      DBUG_RETURN(NULL);
    if (param->real_keynr[idx] == cpk_no)
    {
      cpk_scan= scan;
      tree->n_ror_scans--;
    }
    else
      *(cur_ror_scan++)= scan;
  }

  tree->ror_scans_end= cur_ror_scan;
  DBUG_EXECUTE("info",print_ror_scans_arr(param->table, "original",
                                          tree->ror_scans,
                                          tree->ror_scans_end););
  /*
    Ok, [ror_scans, ror_scans_end) is array of ptrs to initialized
    ROR_SCAN_INFO's.
    Step 2: Get best ROR-intersection using an approximate algorithm.
  */
  find_intersect_order(tree->ror_scans, tree->ror_scans_end, param);

  DBUG_EXECUTE("info",print_ror_scans_arr(param->table, "ordered",
                                          tree->ror_scans,
                                          tree->ror_scans_end););

  ROR_SCAN_INFO **intersect_scans; /* ROR scans used in index intersection */
  ROR_SCAN_INFO **intersect_scans_end;
  if (!(intersect_scans= (ROR_SCAN_INFO**)alloc_root(param->mem_root,
                                                     sizeof(ROR_SCAN_INFO*)*
                                                     tree->n_ror_scans)))
    DBUG_RETURN(NULL);
  intersect_scans_end= intersect_scans;

  /* Create and incrementally update ROR intersection. */
  ROR_INTERSECT_INFO *intersect, *intersect_best;
  if (!(intersect= ror_intersect_init(param)) || 
      !(intersect_best= ror_intersect_init(param)))
    DBUG_RETURN(NULL);

  /* [intersect_scans,intersect_scans_best) will hold the best intersection */
  ROR_SCAN_INFO **intersect_scans_best;
  cur_ror_scan= tree->ror_scans;
  intersect_scans_best= intersect_scans;
  /*
    Note: trace_isect_idx.end() is called to close this object after
    this while-loop.
  */
  Opt_trace_array trace_isect_idx(trace, "intersecting_indexes");
  while (cur_ror_scan != tree->ror_scans_end && !intersect->is_covering)
  {
    Opt_trace_object trace_idx(trace);
    trace_idx.add_utf8("index",
                       param->table->key_info[(*cur_ror_scan)->keynr].name);
    /* S= S + first(R);  R= R - first(R); */
    if (!ror_intersect_add(intersect, *cur_ror_scan, FALSE, &trace_idx))
    {
      trace_idx.add("cumulated_total_cost",
                    intersect->total_cost).
        add("usable", false).
        add_alnum("cause", "does_not_reduce_cost_of_intersect");
      cur_ror_scan++;
      continue;
    }
    
    trace_idx.add("cumulated_total_cost",
                  intersect->total_cost).
      add("usable", true).
      add("matching_rows_now", intersect->out_rows).
      add("isect_covering_with_this_index", intersect->is_covering);

    *(intersect_scans_end++)= *(cur_ror_scan++);

    if (intersect->total_cost < min_cost)
    {
      /* Local minimum found, save it */
      ror_intersect_cpy(intersect_best, intersect);
      intersect_scans_best= intersect_scans_end;
      min_cost= intersect->total_cost;
      trace_idx.add("chosen", true);
    }
    else
    {
      trace_idx.add("chosen", false).
        add_alnum("cause", "does_not_reduce_cost");
    }
  }
  // Note: trace_isect_idx trace object is closed here
  trace_isect_idx.end();

  if (intersect_scans_best == intersect_scans)
  {
    trace_ror.add("chosen", false).
      add_alnum("cause", "does_not_increase_selectivity");
    DBUG_PRINT("info", ("None of scans increase selectivity"));
    DBUG_RETURN(NULL);
  }
    
  DBUG_EXECUTE("info",print_ror_scans_arr(param->table,
                                          "best ROR-intersection",
                                          intersect_scans,
                                          intersect_scans_best););

  uint best_num= intersect_scans_best - intersect_scans;
  ror_intersect_cpy(intersect, intersect_best);

  /*
    Ok, found the best ROR-intersection of non-CPK key scans.
    Check if we should add a CPK scan. If the obtained ROR-intersection is 
    covering, it doesn't make sense to add CPK scan.
  */
  { // Scope for trace object
    Opt_trace_object trace_cpk(trace, "clustered_pk");
    if (cpk_scan && !intersect->is_covering)
    {
      if (ror_intersect_add(intersect, cpk_scan, TRUE, &trace_cpk) &&
          (intersect->total_cost < min_cost))
      {
        trace_cpk.add("clustered_pk_scan_added_to_intersect", true).
          add("cumulated_cost", intersect->total_cost);
        cpk_scan_used= TRUE;
        intersect_best= intersect; //just set pointer here
      }
      else
        trace_cpk.add("clustered_pk_added_to_intersect", false).
          add_alnum("cause", "cost");
    }
    else
    {
      trace_cpk.add("clustered_pk_added_to_intersect", false).
        add_alnum("cause", cpk_scan ?
                  "roworder_is_covering" : "no_clustered_pk_index");
    }
  }
  /* Ok, return ROR-intersect plan if we have found one */
  TRP_ROR_INTERSECT *trp= NULL;
  if (min_cost < *cost_est && (cpk_scan_used || best_num > 1))
  {
    if (!(trp= new (param->mem_root) TRP_ROR_INTERSECT))
      DBUG_RETURN(trp);
    if (!(trp->first_scan=
           (ROR_SCAN_INFO**)alloc_root(param->mem_root,
                                       sizeof(ROR_SCAN_INFO*)*best_num)))
      DBUG_RETURN(NULL);
    memcpy(trp->first_scan, intersect_scans, best_num*sizeof(ROR_SCAN_INFO*));
    trp->last_scan=  trp->first_scan + best_num;
    trp->is_covering= intersect_best->is_covering;
    trp->cost_est= intersect_best->total_cost;
    /* Prevent divisons by zero */
    ha_rows best_rows = double2rows(intersect_best->out_rows);
    if (!best_rows)
      best_rows= 1;
    set_if_smaller(param->table->quick_condition_rows, best_rows);
    trp->records= best_rows;
    trp->index_scan_cost= intersect_best->index_scan_cost;
    trp->cpk_scan= cpk_scan_used? cpk_scan: NULL;

    trace_ror.add("rows", trp->records).
      add("cost", trp->cost_est).
      add("covering", trp->is_covering).
      add("chosen", true);

    DBUG_PRINT("info", ("Returning non-covering ROR-intersect plan:"
                        "cost %g, records %lu",
                        trp->cost_est.total_cost(), (ulong) trp->records));
  }
  else
  {
    trace_ror.add("chosen", false).
      add_alnum("cause", (*cost_est > min_cost) ? "too_few_indexes_to_merge" :
                "cost");
  }
  DBUG_RETURN(trp);
}

/*
  Get best "range" table read plan for given SEL_TREE, also update some info

  SYNOPSIS
    get_key_scans_params()
      param                    Parameters from test_quick_select
      tree                     Make range select for this SEL_TREE
      index_read_must_be_used  TRUE <=> assume 'index only' option will be set
                               (except for clustered PK indexes)
      update_tbl_stats         TRUE <=> update table->quick_* with information
                               about range scans we've evaluated.
      cost_est                 Maximum cost. i.e. don't create read plans with 
                               cost > cost_est.

  DESCRIPTION
    Find the best "range" table read plan for given SEL_TREE. 
    The side effects are 
     - tree->ror_scans is updated to indicate which scans are ROR scans.
     - if update_tbl_stats=TRUE then table->quick_* is updated with info
       about every possible range scan.

  RETURN
    Best range read plan
    NULL if no plan found or error occurred
*/

static TRP_RANGE *get_key_scans_params(PARAM *param, SEL_TREE *tree,
                                       bool index_read_must_be_used, 
                                       bool update_tbl_stats,
                                       const Cost_estimate *cost_est)
{
  uint idx, best_idx= 0;
  SEL_ARG *key, *key_to_read= NULL;
  ha_rows best_records= 0;              /* protected by key_to_read */
  uint    best_mrr_flags= 0, best_buf_size= 0;
  TRP_RANGE* read_plan= NULL;
  Cost_estimate read_cost= *cost_est;
  DBUG_ENTER("get_key_scans_params");
  Opt_trace_context * const trace= &param->thd->opt_trace;
  /*
    Note that there may be trees that have type SEL_TREE::KEY but contain no
    key reads at all, e.g. tree for expression "key1 is not null" where key1
    is defined as "not null".
  */
  DBUG_EXECUTE("info", print_sel_tree(param, tree, &tree->keys_map,
                                      "tree scans"););
  Opt_trace_array ota(trace, "range_scan_alternatives");

  tree->ror_scans_map.clear_all();
  tree->n_ror_scans= 0;
  for (idx= 0; idx < param->keys; idx++)
  {
    key= tree->keys[idx];
    if (key)
    {
      ha_rows found_records;
      Cost_estimate cost;
      uint mrr_flags, buf_size;
      uint keynr= param->real_keynr[idx];
      if (key->type == SEL_ARG::MAYBE_KEY ||
          key->maybe_flag)
        param->needed_reg->set_bit(keynr);

      bool read_index_only= index_read_must_be_used ? TRUE :
                            (bool) param->table->covering_keys.is_set(keynr);

      Opt_trace_object trace_idx(trace);
      trace_idx.add_utf8("index", param->table->key_info[keynr].name);

      found_records= check_quick_select(param, idx, read_index_only, key,
                                        update_tbl_stats, &mrr_flags,
                                        &buf_size, &cost);

#ifdef OPTIMIZER_TRACE
      // check_quick_select() says don't use range if it returns HA_POS_ERROR
      if (found_records != HA_POS_ERROR &&
          param->thd->opt_trace.is_started())
      {
        Opt_trace_array trace_range(&param->thd->opt_trace, "ranges");

        const KEY &cur_key= param->table->key_info[keynr];
        const KEY_PART_INFO *key_part= cur_key.key_part;

        String range_info;
        range_info.set_charset(system_charset_info);
        append_range_all_keyparts(&trace_range, NULL, &range_info,
                                  key, key_part, false);
        trace_range.end(); // NOTE: ends the tracing scope

        trace_idx.add("index_dives_for_eq_ranges", !param->use_index_statistics).
          add("rowid_ordered", param->is_ror_scan).
          add("using_mrr", !(mrr_flags & HA_MRR_USE_DEFAULT_IMPL)).
          add("index_only", read_index_only).
          add("rows", found_records).
          add("cost", cost);
      }
#endif

      if ((found_records != HA_POS_ERROR) && param->is_ror_scan)
      {
        tree->n_ror_scans++;
        tree->ror_scans_map.set_bit(idx);
      }

      if (found_records != HA_POS_ERROR &&
          read_cost > cost)
      {
        trace_idx.add("chosen", true);
        read_cost= cost;
        best_records= found_records;
        key_to_read=  key;
        best_idx= idx;
        best_mrr_flags= mrr_flags;
        best_buf_size=  buf_size;
      }
      else
      {
        trace_idx.add("chosen", false);
        if (found_records == HA_POS_ERROR)
          if (key->type == SEL_ARG::MAYBE_KEY)
            trace_idx.add_alnum("cause", "depends_on_unread_values");
          else
            trace_idx.add_alnum("cause", "unknown");
        else 
          trace_idx.add_alnum("cause", "cost");
      }

    }
  }

  DBUG_EXECUTE("info", print_sel_tree(param, tree, &tree->ror_scans_map,
                                      "ROR scans"););
  if (key_to_read)
  {
    if ((read_plan= new (param->mem_root) TRP_RANGE(key_to_read, best_idx,
                                                    best_mrr_flags)))
    {
      read_plan->records= best_records;
      read_plan->is_ror= tree->ror_scans_map.is_set(best_idx);
      read_plan->cost_est= read_cost;
      read_plan->mrr_buf_size= best_buf_size;
      DBUG_PRINT("info",
                ("Returning range plan for key %s, cost %g, records %lu",
                 param->table->key_info[param->real_keynr[best_idx]].name,
                 read_plan->cost_est.total_cost(), (ulong) read_plan->records));
    }
  }
  else
    DBUG_PRINT("info", ("No 'range' table read plan found"));

  DBUG_RETURN(read_plan);
}


QUICK_SELECT_I *TRP_INDEX_MERGE::make_quick(PARAM *param,
                                            bool retrieve_full_rows,
                                            MEM_ROOT *parent_alloc)
{
  QUICK_INDEX_MERGE_SELECT *quick_imerge;
  QUICK_RANGE_SELECT *quick;
  /* index_merge always retrieves full rows, ignore retrieve_full_rows */
  if (!(quick_imerge= new QUICK_INDEX_MERGE_SELECT(param->thd, param->table)))
    return NULL;

  quick_imerge->records= records;
  quick_imerge->cost_est= cost_est;

  for (TRP_RANGE **range_scan= range_scans; range_scan != range_scans_end;
       range_scan++)
  {
    if (!(quick= (QUICK_RANGE_SELECT*)
          ((*range_scan)->make_quick(param, FALSE, &quick_imerge->alloc)))||
        quick_imerge->push_quick_back(quick))
    {
      delete quick;
      delete quick_imerge;
      return NULL;
    }
  }
  return quick_imerge;
}

QUICK_SELECT_I *TRP_ROR_INTERSECT::make_quick(PARAM *param,
                                              bool retrieve_full_rows,
                                              MEM_ROOT *parent_alloc)
{
  QUICK_ROR_INTERSECT_SELECT *quick_intrsect;
  QUICK_RANGE_SELECT *quick;
  DBUG_ENTER("TRP_ROR_INTERSECT::make_quick");
  MEM_ROOT *alloc;

  if ((quick_intrsect=
         new QUICK_ROR_INTERSECT_SELECT(param->thd, param->table,
                                        (retrieve_full_rows? (!is_covering) :
                                         FALSE),
                                        parent_alloc)))
  {
    DBUG_EXECUTE("info", print_ror_scans_arr(param->table,
                                             "creating ROR-intersect",
                                             first_scan, last_scan););
    alloc= parent_alloc? parent_alloc: &quick_intrsect->alloc;
    for (st_ror_scan_info **current= first_scan;
         current != last_scan;
         current++)
    {
      if (!(quick= get_quick_select(param, (*current)->idx,
                                    (*current)->sel_arg,
                                    HA_MRR_SORTED,
                                    0, alloc)) ||
          quick_intrsect->push_quick_back(quick))
      {
        delete quick_intrsect;
        DBUG_RETURN(NULL);
      }
    }
    if (cpk_scan)
    {
      if (!(quick= get_quick_select(param, cpk_scan->idx,
                                    cpk_scan->sel_arg,
                                    HA_MRR_SORTED,
                                    0, alloc)))
      {
        delete quick_intrsect;
        DBUG_RETURN(NULL);
      }
      quick->file= NULL; 
      quick_intrsect->cpk_quick= quick;
    }
    quick_intrsect->records= records;
    quick_intrsect->cost_est= cost_est;
  }
  DBUG_RETURN(quick_intrsect);
}


QUICK_SELECT_I *TRP_ROR_UNION::make_quick(PARAM *param,
                                          bool retrieve_full_rows,
                                          MEM_ROOT *parent_alloc)
{
  QUICK_ROR_UNION_SELECT *quick_roru;
  TABLE_READ_PLAN **scan;
  QUICK_SELECT_I *quick;
  DBUG_ENTER("TRP_ROR_UNION::make_quick");
  /*
    It is impossible to construct a ROR-union that will not retrieve full
    rows, ignore retrieve_full_rows parameter.
  */
  if ((quick_roru= new QUICK_ROR_UNION_SELECT(param->thd, param->table)))
  {
    for (scan= first_ror; scan != last_ror; scan++)
    {
      if (!(quick= (*scan)->make_quick(param, FALSE, &quick_roru->alloc)) ||
          quick_roru->push_quick_back(quick))
        DBUG_RETURN(NULL);
    }
    quick_roru->records= records;
    quick_roru->cost_est= cost_est;
  }
  DBUG_RETURN(quick_roru);
}


/**
   If EXPLAIN or if the --safe-updates option is enabled, add a warning that
   the index cannot be used for range access due to either type conversion or
   different collations on the field used for comparison

   @param param              PARAM from test_quick_select
   @param key_num            Key number
   @param field              Field in the predicate
*/
static void warn_index_not_applicable(const RANGE_OPT_PARAM *param,
                                      const uint key_num, const Field *field)
{
  THD *thd= param->thd;
  if (param->using_real_indexes &&
      (param->thd->lex->describe ||
       thd->variables.option_bits & OPTION_SAFE_UPDATES))
    push_warning_printf(
            param->thd,
            Sql_condition::SL_WARNING,
            ER_WARN_INDEX_NOT_APPLICABLE,
            ER(ER_WARN_INDEX_NOT_APPLICABLE),
            "range",
            field->table->key_info[param->real_keynr[key_num]].name,
            field->field_name);
}


/*
  Build a SEL_TREE for <> or NOT BETWEEN predicate
 
  SYNOPSIS
    get_ne_mm_tree()
      param       PARAM from test_quick_select
      cond_func   item for the predicate
      field       field in the predicate
      lt_value    constant that field should be smaller
      gt_value    constant that field should be greaterr
      cmp_type    compare type for the field

  RETURN 
    #  Pointer to tree built tree
    0  on error
*/
static SEL_TREE *get_ne_mm_tree(RANGE_OPT_PARAM *param, Item_func *cond_func, 
                                Field *field,
                                Item *lt_value, Item *gt_value,
                                Item_result cmp_type)
{
  SEL_TREE *tree= NULL;

  if (param->has_errors())
    return NULL;

  tree= get_mm_parts(param, cond_func, field, Item_func::LT_FUNC,
                     lt_value, cmp_type);
  if (tree)
  {
    tree= tree_or(param, tree, get_mm_parts(param, cond_func, field,
					    Item_func::GT_FUNC,
					    gt_value, cmp_type));
  }
  return tree;
}


/**
  Factory function to build a SEL_TREE from an <in predicate>

  @param param      Information on 'just about everything'.
  @param predicand  The <in predicate's> predicand, i.e. the left-hand
                    side of the <in predicate> expression.
  @param op         The 'in' operator itself.
  @param value      The right-hand side of the <in predicate> expression.
  @param cmp_type   What types we should pretend that the arguments are.
  @param is_negated If true, the operator is NOT IN, otherwise IN.
*/
static SEL_TREE *get_func_mm_tree_from_in_predicate(RANGE_OPT_PARAM *param,
                                                    Item *predicand,
                                                    Item_func_in *op,
                                                    Item *value,
                                                    Item_result cmp_type,
                                                    bool is_negated)
{
  if (param->has_errors())
    return NULL;

  if (is_negated)
  {
    // We don't support row constructors (multiple columns on lhs) here.
    if (predicand->type() != Item::FIELD_ITEM)
      return NULL;

    Field *field= static_cast<Item_field*>(predicand)->field;

    if (op->array && op->array->result_type() != ROW_RESULT)
    {
      /*
        We get here for conditions on the form "t.key NOT IN (c1, c2, ...)",
        where c{i} are constants. Our goal is to produce a SEL_TREE that
        represents intervals:

        ($MIN<t.key<c1) OR (c1<t.key<c2) OR (c2<t.key<c3) OR ...    (*)

        where $MIN is either "-inf" or NULL.

        The most straightforward way to produce it is to convert NOT
        IN into "(t.key != c1) AND (t.key != c2) AND ... " and let the
        range analyzer build a SEL_TREE from that. The problem is that
        the range analyzer will use O(N^2) memory (which is probably a
        bug), and people who do use big NOT IN lists (e.g. see
        BUG#15872, BUG#21282), will run out of memory.

        Another problem with big lists like (*) is that a big list is
        unlikely to produce a good "range" access, while considering
        that range access will require expensive CPU calculations (and
        for MyISAM even index accesses). In short, big NOT IN lists
        are rarely worth analyzing.

        Considering the above, we'll handle NOT IN as follows:

        - if the number of entries in the NOT IN list is less than
          NOT_IN_IGNORE_THRESHOLD, construct the SEL_TREE (*)
          manually.

        - Otherwise, don't produce a SEL_TREE.
      */

      const uint NOT_IN_IGNORE_THRESHOLD= 1000;
      // If we have t.key NOT IN (null, null, ...) or the list is too long
      if (op->array->used_count == 0 ||
          op->array->used_count > NOT_IN_IGNORE_THRESHOLD)
        return NULL;

      MEM_ROOT *tmp_root= param->mem_root;
      param->thd->mem_root= param->old_root;
      /*
        Create one Item_type constant object. We'll need it as
        get_mm_parts only accepts constant values wrapped in Item_Type
        objects.
        We create the Item on param->mem_root which points to
        per-statement mem_root (while thd->mem_root is currently pointing
        to mem_root local to range optimizer).
      */
      Item *value_item= op->array->create_item();
      param->thd->mem_root= tmp_root;

      if (!value_item)
        return NULL;

      /* Get a SEL_TREE for "(-inf|NULL) < X < c_0" interval.  */
      uint i= 0;
      SEL_TREE *tree= NULL;
      do
      {
        op->array->value_to_item(i, value_item);
        tree= get_mm_parts(param, op, field, Item_func::LT_FUNC, value_item,
                           cmp_type);
        if (!tree)
          break;
        i++;
      }
      while (i < op->array->used_count && tree->type == SEL_TREE::IMPOSSIBLE);

      if (!tree || tree->type == SEL_TREE::IMPOSSIBLE)
        /* We get here in cases like "t.unsigned NOT IN (-1,-2,-3) */
        return NULL;
      SEL_TREE *tree2= NULL;
      for (; i < op->array->used_count; i++)
      {
        if (op->array->compare_elems(i, i - 1))
        {
          /* Get a SEL_TREE for "-inf < X < c_i" interval */
          op->array->value_to_item(i, value_item);
          tree2= get_mm_parts(param, op, field, Item_func::LT_FUNC,
                              value_item, cmp_type);
          if (!tree2)
          {
            tree= NULL;
            break;
          }

          /* Change all intervals to be "c_{i-1} < X < c_i" */
          for (uint idx= 0; idx < param->keys; idx++)
          {
            SEL_ARG *new_interval, *last_val;
            if (((new_interval= tree2->keys[idx])) &&
                (tree->keys[idx]) &&
                ((last_val= tree->keys[idx]->last())))
            {
              new_interval->min_value= last_val->max_value;
              new_interval->min_flag= NEAR_MIN;

              /*
                If the interval is over a partial keypart, the
                interval must be "c_{i-1} <= X < c_i" instead of
                "c_{i-1} < X < c_i". Reason:

                Consider a table with a column "my_col VARCHAR(3)",
                and an index with definition
                "INDEX my_idx my_col(1)". If the table contains rows
                with my_col values "f" and "foo", the index will not
                distinguish the two rows.

                Note that tree_or() below will effectively merge
                this range with the range created for c_{i-1} and
                we'll eventually end up with only one range:
                "NULL < X".

                Partitioning indexes are never partial.
              */
              if (param->using_real_indexes)
              {
                const KEY key=
                  param->table->key_info[param->real_keynr[idx]];
                const KEY_PART_INFO *kpi= key.key_part + new_interval->part;

                if (kpi->key_part_flag & HA_PART_KEY_SEG)
                  new_interval->min_flag= 0;
              }
            }
          }
          /*
            The following doesn't try to allocate memory so no need to
            check for NULL.
          */
          tree= tree_or(param, tree, tree2);
        }
      }

      if (tree && tree->type != SEL_TREE::IMPOSSIBLE)
      {
        /*
          Get the SEL_TREE for the last "c_last < X < +inf" interval
          (value_item cotains c_last already)
        */
        tree2= get_mm_parts(param, op, field, Item_func::GT_FUNC,
                            value_item, cmp_type);
        tree= tree_or(param, tree, tree2);
      }
      return tree;
    }
    else
    {
      SEL_TREE *tree= get_ne_mm_tree(param, op, field, op->arguments()[1],
                                     op->arguments()[1], cmp_type);
      if (tree)
      {
        Item **arg, **end;
        for (arg= op->arguments() + 2, end= arg + op->argument_count() - 2;
             arg < end ; arg++)
        {
          tree= tree_and(param, tree,
                         get_ne_mm_tree(param, op, field, *arg, *arg,
                                        cmp_type));
        }
      }
      return tree;
    }
    return NULL;
  }

  // The expression is IN, not negated.
  if (predicand->type() == Item::FIELD_ITEM)
  {
    // The expression is (<column>) IN (...)
    Field *field= static_cast<Item_field*>(predicand)->field;
    SEL_TREE *tree= get_mm_parts(param, op, field, Item_func::EQ_FUNC,
                                 op->arguments()[1], cmp_type);
    if (tree)
    {
      Item **arg, **end;
      for (arg= op->arguments() + 2, end= arg + op->argument_count() - 2;
           arg < end ; arg++)
      {
        tree= tree_or(param, tree, get_mm_parts(param, op, field,
                                                Item_func::EQ_FUNC,
                                                *arg, cmp_type));
      }
    }
    return tree;
  }
  if (predicand->type() == Item::ROW_ITEM)
  {
    /*
      The expression is (<column>,...) IN (...)

      We iterate over the rows on the rhs of the in predicate,
      building an OR tree of ANDs, a.k.a. a DNF expression out of this. E.g:

      (col1, col2) IN ((const1, const2), (const3, const4))
      becomes
      (col1 = const1 AND col2 = const2) OR (col1 = const3 AND col2 = const4)
    */
    SEL_TREE *or_tree= &null_sel_tree;
    Item_row *row_predicand= static_cast<Item_row*>(predicand);

    // Iterate over the rows on the rhs of the in predicate, building an OR.
    for (uint i= 1; i < op->argument_count(); ++i)
    {
      /*
        We only support row value expressions. Some optimizations rewrite
        the Item tree, and we don't handle that.
      */        
      Item *in_list_item= op->arguments()[i];
      if (in_list_item->type() != Item::ROW_ITEM)
        return NULL;
      Item_row *row= static_cast<Item_row*>(in_list_item);

      // Iterate over the columns, building an AND tree.
      SEL_TREE *and_tree= NULL;
      for (uint j= 0; j < row_predicand->cols(); ++j)
      {
        Item *item= row_predicand->element_index(j);

        // We only support columns in the row on the lhs of the in predicate.
        if (item->type() != Item::FIELD_ITEM)
          return NULL;
        Field *field= static_cast<Item_field*>(item)->field;

        Item *value= row->element_index(j);

        SEL_TREE *and_expr=
          get_mm_parts(param, op, field, Item_func::EQ_FUNC, value, cmp_type);

        and_tree= tree_and(param, and_tree, and_expr);
        /*
          Short-circuit evaluation: If and_expr is NULL then no key part in
          this disjunct can be used as a search key. Or in other words the
          condition is always true. Hence the whole disjunction is always true.
        */
        if (and_tree == NULL)
          return NULL;
      }
      or_tree= tree_or(param, and_tree, or_tree);
    }
    return or_tree;
  }
  return NULL;
}


/**
  Build a SEL_TREE for a simple predicate.

  @param param     PARAM from test_quick_select
  @param predicand field in the predicate
  @param cond_func item for the predicate
  @param value     constant in the predicate
  @param cmp_type  compare type for the field
  @param inv       TRUE <> NOT cond_func is considered
                  (makes sense only when cond_func is BETWEEN or IN)

  @return Pointer to the built tree.

  @todo Remove the appaling hack that 'value' can be a 1 cast to an Item*.
*/

static SEL_TREE *get_func_mm_tree(RANGE_OPT_PARAM *param,
                                  Item *predicand,
                                  Item_func *cond_func,
                                  Item *value,
                                  Item_result cmp_type,
                                  bool inv)
{
  SEL_TREE *tree= 0;
  DBUG_ENTER("get_func_mm_tree");

  if (param->has_errors())
    DBUG_RETURN(0);

  switch (cond_func->functype()) {

  case Item_func::XOR_FUNC:
    DBUG_RETURN(NULL); // Always true (don't use range access on XOR).
    break;             // See WL#5800

  case Item_func::NE_FUNC:
    if (predicand->type() == Item::FIELD_ITEM)
    {
      Field *field= static_cast<Item_field*>(predicand)->field;
      tree= get_ne_mm_tree(param, cond_func, field, value, value, cmp_type);
    }
    break;

  case Item_func::BETWEEN:
    if (predicand->type() == Item::FIELD_ITEM)
    {
      Field *field= static_cast<Item_field*>(predicand)->field;

      if (!value)
      {
        if (inv)
        {
          tree= get_ne_mm_tree(param, cond_func, field,
                               cond_func->arguments()[1],
                               cond_func->arguments()[2], cmp_type);
        }
        else
        {
          tree= get_mm_parts(param, cond_func, field, Item_func::GE_FUNC,
                             cond_func->arguments()[1],cmp_type);
          if (tree)
          {
            tree= tree_and(param, tree, get_mm_parts(param, cond_func, field,
                                                     Item_func::LE_FUNC,
                                                     cond_func->arguments()[2],
                                                     cmp_type));
          }
        }
      }
      else
        tree= get_mm_parts(param, cond_func, field,
                           (inv ?
                            (value == reinterpret_cast<Item*>(1) ?
                             Item_func::GT_FUNC :
                             Item_func::LT_FUNC):
                            (value == reinterpret_cast<Item*>(1) ?
                             Item_func::LE_FUNC :
                             Item_func::GE_FUNC)),
                           cond_func->arguments()[0], cmp_type);
    }
    break;
  case Item_func::IN_FUNC:
  {
    Item_func_in *in_pred= static_cast<Item_func_in*>(cond_func);
    tree= get_func_mm_tree_from_in_predicate(param, predicand, in_pred, value,
                                             cmp_type, inv);
  }
  break;
  default:
    if (predicand->type() == Item::FIELD_ITEM)
    {
      Field *field= static_cast<Item_field*>(predicand)->field;

      /*
         Here the function for the following predicates are processed:
         <, <=, =, >=, >, LIKE, IS NULL, IS NOT NULL and GIS functions.
         If the predicate is of the form (value op field) it is handled
         as the equivalent predicate (field rev_op value), e.g.
         2 <= a is handled as a >= 2.
      */
      Item_func::Functype func_type=
        (value != cond_func->arguments()[0]) ? cond_func->functype() :
        ((Item_bool_func2*) cond_func)->rev_functype();
      tree= get_mm_parts(param, cond_func, field, func_type, value, cmp_type);
    }
  }

  DBUG_RETURN(tree);
}


/*
  Build conjunction of all SEL_TREEs for a simple predicate applying equalities
 
  SYNOPSIS
    get_full_func_mm_tree()
      param       PARAM from test_quick_select
      predicand   column or row constructor in the predicate's left-hand side.
      op          Item for the predicate operator
      value       constant in the predicate (or a field already read from
                  a table in the case of dynamic range access)
                  For BETWEEN it contains the number of the field argument.
      inv         If true, the predicate is negated, e.g. NOT IN.
                  (makes sense only when cond_func is BETWEEN or IN)

  DESCRIPTION
    For a simple SARGable predicate of the form (f op c), where f is a field and
    c is a constant, the function builds a conjunction of all SEL_TREES that can
    be obtained by the substitution of f for all different fields equal to f.

  NOTES  
    If the WHERE condition contains a predicate (fi op c),
    then not only SELL_TREE for this predicate is built, but
    the trees for the results of substitution of fi for
    each fj belonging to the same multiple equality as fi
    are built as well.
    E.g. for WHERE t1.a=t2.a AND t2.a > 10 
    a SEL_TREE for t2.a > 10 will be built for quick select from t2
    and   
    a SEL_TREE for t1.a > 10 will be built for quick select from t1.

    A BETWEEN predicate of the form (fi [NOT] BETWEEN c1 AND c2) is treated
    in a similar way: we build a conjuction of trees for the results
    of all substitutions of fi for equal fj.
    Yet a predicate of the form (c BETWEEN f1i AND f2i) is processed
    differently. It is considered as a conjuction of two SARGable
    predicates (f1i <= c) and (f2i <=c) and the function get_full_func_mm_tree
    is called for each of them separately producing trees for 
       AND j (f1j <=c ) and AND j (f2j <= c) 
    After this these two trees are united in one conjunctive tree.
    It's easy to see that the same tree is obtained for
       AND j,k (f1j <=c AND f2k<=c)
    which is equivalent to 
       AND j,k (c BETWEEN f1j AND f2k).
    The validity of the processing of the predicate (c NOT BETWEEN f1i AND f2i)
    which equivalent to (f1i > c OR f2i < c) is not so obvious. Here the
    function get_full_func_mm_tree is called for (f1i > c) and (f2i < c)
    producing trees for AND j (f1j > c) and AND j (f2j < c). Then this two
    trees are united in one OR-tree. The expression 
      (AND j (f1j > c) OR AND j (f2j < c)
    is equivalent to the expression
      AND j,k (f1j > c OR f2k < c) 
    which is just a translation of 
      AND j,k (c NOT BETWEEN f1j AND f2k)

    In the cases when one of the items f1, f2 is a constant c1 we do not create
    a tree for it at all. It works for BETWEEN predicates but does not
    work for NOT BETWEEN predicates as we have to evaluate the expression
    with it. If it is TRUE then the other tree can be completely ignored.
    We do not do it now and no trees are built in these cases for
    NOT BETWEEN predicates.

    As to IN predicates only ones of the form (f IN (c1,...,cn)),
    where f1 is a field and c1,...,cn are constant, are considered as
    SARGable. We never try to narrow the index scan using predicates of
    the form (c IN (c1,...,f,...,cn)). 
      
  RETURN 
    Pointer to the tree representing the built conjunction of SEL_TREEs
*/

static SEL_TREE *get_full_func_mm_tree(RANGE_OPT_PARAM *param,
                                       Item *predicand,
                                       Item_func *op,
                                       Item *value,
                                       bool inv)
{
  SEL_TREE *tree= 0;
  SEL_TREE *ftree= 0;
  const table_map param_comp=
    ~(param->prev_tables | param->read_tables | param->current_table);
  DBUG_ENTER("get_full_func_mm_tree");

  if (param->has_errors())
    DBUG_RETURN(NULL);

  /*
    Here we compute a set of tables that we consider as constants
    suppliers during execution of the SEL_TREE that we produce below.
  */
  table_map ref_tables= 0;
  for (uint i= 0; i < op->arg_count; i++)
  {
    Item *arg= op->arguments()[i]->real_item();
    if (arg != predicand)
      ref_tables|= arg->used_tables();
  }
  if (predicand->type() == Item::FIELD_ITEM)
  {
    Item_field *item_field= static_cast<Item_field*>(predicand);
    Field *field= item_field->field;
    Item_result cmp_type= field->cmp_type();

    if (!((ref_tables | item_field->table_ref->map()) & param_comp))
      ftree= get_func_mm_tree(param, predicand, op, value, cmp_type, inv);
    Item_equal *item_equal= item_field->item_equal;
    if (item_equal != NULL)
    {
      Item_equal_iterator it(*item_equal);
      Item_field *item;
      while ((item= it++))
      {
        Field *f= item->field;
        if (!field->eq(f) &&
            !((ref_tables | item->table_ref->map()) & param_comp))
        {
          tree= get_func_mm_tree(param, item, op, value, cmp_type, inv);
          ftree= !ftree ? tree : tree_and(param, ftree, tree);
        }
      }
    }
  }
  else if (predicand->type() == Item::ROW_ITEM)
  {
    ftree= get_func_mm_tree(param, predicand, op, value, ROW_RESULT, inv);
    DBUG_RETURN(ftree);
  }
  DBUG_RETURN(ftree);
}

/**
  The Range Analysis Module, which finds range access alternatives
  applicable to single or multi-index (UNION) access. The function
  does not calculate or care about the cost of the different
  alternatives.

  get_mm_tree() employs a relaxed boolean algebra where the solution
  may be bigger than what the rules of boolean algebra accept. In
  other words, get_mm_tree() may return range access plans that will
  read more rows than the input conditions dictate. In it's simplest
  form, consider a condition on two fields indexed by two different
  indexes:

     "WHERE fld1 > 'x' AND fld2 > 'y'"

  In this case, there are two single-index range access alternatives.
  No matter which access path is chosen, rows that are not in the
  result set may be read.

  In the case above, get_mm_tree() will create range access
  alternatives for both indexes, so boolean algebra is still correct.
  In other cases, however, the conditions are too complex to be used
  without relaxing the rules. This typically happens when ORing a
  conjunction to a multi-index disjunctions (@see e.g.
  imerge_list_or_tree()). When this happens, the range optimizer may
  choose to ignore conjunctions (any condition connected with AND). The
  effect of this is that the result includes a "bigger" solution than
  neccessary. This is OK since all conditions will be used as filters
  after row retrieval.

  @see SEL_TREE::keys and SEL_TREE::merges for details of how single
  and multi-index range access alternatives are stored.
*/
static SEL_TREE *get_mm_tree(RANGE_OPT_PARAM *param,Item *cond)
{
  SEL_TREE *tree=0;
  SEL_TREE *ftree= 0;
  Item_field *field_item= 0;
  bool inv= FALSE;
  Item *value= 0;
  DBUG_ENTER("get_mm_tree");

  if (param->has_errors())
    DBUG_RETURN(NULL);

  if (cond->type() == Item::COND_ITEM)
  {
    List_iterator<Item> li(*((Item_cond*) cond)->argument_list());

    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      tree= NULL;
      Item *item;
      while ((item=li++))
      {
        SEL_TREE *new_tree= get_mm_tree(param,item);
        if (param->has_errors())
          DBUG_RETURN(NULL);
        tree= tree_and(param,tree,new_tree);
        dbug_print_tree("after_and", tree, param);
        if (tree && tree->type == SEL_TREE::IMPOSSIBLE)
          break;
      }
    }
    else
    {                                           // Item OR
      tree= get_mm_tree(param,li++);
      if (param->has_errors())
        DBUG_RETURN(NULL);
      if (tree)
      {
        Item *item;
        while ((item=li++))
        {
          SEL_TREE *new_tree=get_mm_tree(param,item);
          if (new_tree == NULL || param->has_errors())
            DBUG_RETURN(NULL);
          tree= tree_or(param,tree,new_tree);
          dbug_print_tree("after_or", tree, param);
          if (tree == NULL || tree->type == SEL_TREE::ALWAYS)
            break;
        }
      }
    }
    dbug_print_tree("tree_returned", tree, param);
    DBUG_RETURN(tree);
  }
  /* 
    Here when simple cond 
    There are limits on what kinds of const items we can evaluate.
    At this stage a subquery in 'cond' might not be fully transformed yet
    (example: semijoin) thus cannot be evaluated.
  */
  if (cond->const_item() && !cond->is_expensive() && !cond->has_subquery())
  {
    /*
      During the cond->val_int() evaluation we can come across a subselect 
      item which may allocate memory on the thd->mem_root and assumes 
      all the memory allocated has the same life span as the subselect 
      item itself. So we have to restore the thread's mem_root here.
    */
    MEM_ROOT *tmp_root= param->mem_root;
    param->thd->mem_root= param->old_root;
    const SEL_TREE::Type type=
      cond->val_int() ? SEL_TREE::ALWAYS : SEL_TREE::IMPOSSIBLE;
    tree= new (tmp_root) SEL_TREE(type, tmp_root, param->keys);

    param->thd->mem_root= tmp_root;
    if (param->has_errors())
      DBUG_RETURN(NULL);
    dbug_print_tree("tree_returned", tree, param);
    DBUG_RETURN(tree);
  }

  table_map ref_tables= 0;
  table_map param_comp= ~(param->prev_tables | param->read_tables |
		          param->current_table);
  if (cond->type() != Item::FUNC_ITEM)
  {						// Should be a field
    ref_tables= cond->used_tables();
    if ((ref_tables & param->current_table) ||
	(ref_tables & ~(param->prev_tables | param->read_tables)))
      DBUG_RETURN(0);
    DBUG_RETURN(new (param->mem_root)
                SEL_TREE(SEL_TREE::MAYBE, param->mem_root, param->keys));
  }

  Item_func *cond_func= (Item_func*) cond;
  if (cond_func->functype() == Item_func::BETWEEN ||
      cond_func->functype() == Item_func::IN_FUNC)
    inv= ((Item_func_opt_neg *) cond_func)->negated;
  else
  {
    /*
      During the cond_func->select_optimize() evaluation we can come across a
      subselect item which may allocate memory on the thd->mem_root and assumes
      all the memory allocated has the same life span as the subselect item
      itself. So we have to restore the thread's mem_root here.
    */
    MEM_ROOT *tmp_root= param->mem_root;
    param->thd->mem_root= param->old_root;
    Item_func::optimize_type opt_type= cond_func->select_optimize();
    param->thd->mem_root= tmp_root;
    if (opt_type == Item_func::OPTIMIZE_NONE)
      DBUG_RETURN(NULL);
  }

  param->cond= cond;

  /*
    Notice that all fields that are outer references are const during
    the execution and should not be considered for range analysis like
    fields coming from the local query block are.
  */
  switch (cond_func->functype())
  {
  case Item_func::BETWEEN:
  {
    Item *const arg_left= cond_func->arguments()[0];

    if (!(arg_left->used_tables() & OUTER_REF_TABLE_BIT) &&
        arg_left->real_item()->type() == Item::FIELD_ITEM)
    {
      field_item= (Item_field*) arg_left->real_item();
      ftree= get_full_func_mm_tree(param, field_item, cond_func, NULL, inv);
    }

    /*
      Concerning the code below see the NOTES section in
      the comments for the function get_full_func_mm_tree()
    */
    for (uint i= 1 ; i < cond_func->arg_count ; i++)
    {
      Item *const arg= cond_func->arguments()[i];

      if (!(arg->used_tables() & OUTER_REF_TABLE_BIT) &&
          arg->real_item()->type() == Item::FIELD_ITEM)
      {
        field_item= (Item_field*) arg->real_item();
        SEL_TREE *tmp=
          get_full_func_mm_tree(param, field_item, cond_func,
                                reinterpret_cast<Item*>(i), inv);
        if (inv)
        {
          tree= !tree ? tmp : tree_or(param, tree, tmp);
          if (tree == NULL)
            break;
        }
        else 
          tree= tree_and(param, tree, tmp);
      }
      else if (inv)
      { 
        tree= 0;
        break;
      }
    }

    ftree = tree_and(param, ftree, tree);
    break;
  } // end case Item_func::BETWEEN

  case Item_func::IN_FUNC:
  {
    Item *const predicand= ((Item_func_in*) cond_func)->key_item()->real_item();
    if (predicand->type() != Item::FIELD_ITEM &&
        predicand->type() != Item::ROW_ITEM)
      DBUG_RETURN(NULL);
    ftree= get_full_func_mm_tree(param, predicand, cond_func, NULL, inv);
    break;
  } // end case Item_func::IN_FUNC

  case Item_func::MULT_EQUAL_FUNC:
  {
    Item_equal *item_equal= (Item_equal *) cond;    
    if (!(value= item_equal->get_const()))
      DBUG_RETURN(0);
    Item_equal_iterator it(*item_equal);
    ref_tables= value->used_tables();
    while ((field_item= it++))
    {
      Field *field= field_item->field;
      Item_result cmp_type= field->cmp_type();
      if (!((ref_tables | field_item->table_ref->map()) & param_comp))
      {
        tree= get_mm_parts(param, item_equal, field, Item_func::EQ_FUNC,
		           value,cmp_type);
        ftree= !ftree ? tree : tree_and(param, ftree, tree);
      }
    }
    
    dbug_print_tree("tree_returned", ftree, param);
    DBUG_RETURN(ftree);
  } // end case Item_func::MULT_EQUAL_FUNC

  default:
  {
    Item *const arg_left= cond_func->arguments()[0];

    DBUG_ASSERT (!ftree);
    if (!(arg_left->used_tables() & OUTER_REF_TABLE_BIT) &&
        arg_left->real_item()->type() == Item::FIELD_ITEM)
    {
      field_item= (Item_field*) arg_left->real_item();
      value= cond_func->arg_count > 1 ? cond_func->arguments()[1] : NULL;
      ftree= get_full_func_mm_tree(param, field_item, cond_func, value, inv);
    }
    /*
      Even if get_full_func_mm_tree() was executed above and did not
      return a range predicate it may still be possible to create one
      by reversing the order of the operands. Note that this only
      applies to predicates where both operands are fields. Example: A
      query of the form

         WHERE t1.a OP t2.b

      In this case, arguments()[0] == t1.a and arguments()[1] == t2.b.
      When creating range predicates for t2, get_full_func_mm_tree()
      above will return NULL because 'field' belongs to t1 and only
      predicates that applies to t2 are of interest. In this case a
      call to get_full_func_mm_tree() with reversed operands (see
      below) may succeed.
     */
    Item *arg_right;
    if (!ftree && cond_func->have_rev_func() &&
        (arg_right= cond_func->arguments()[1]) &&
        !(arg_right->used_tables() & OUTER_REF_TABLE_BIT) &&
        arg_right->real_item()->type() == Item::FIELD_ITEM)
    {
      field_item= (Item_field*) arg_right->real_item();
      value= arg_left;
      ftree= get_full_func_mm_tree(param, field_item, cond_func, value, inv);
    }
  }  // end case default
  }  // end switch

  dbug_print_tree("tree_returned", ftree, param);
  DBUG_RETURN(ftree);
}

/**
  Test whether a comparison operator is a spatial comparison
  operator, i.e. Item_func::SP_*.

  Used to check if range access using operator 'op_type' is applicable
  for a non-spatial index.

  @param   op_type  The comparison operator.
  @return  true if 'op_type' is a spatial comparison operator, false otherwise.

*/
bool is_spatial_operator(Item_func::Functype op_type)
{
  switch (op_type)
  {
  case Item_func::SP_EQUALS_FUNC:
  case Item_func::SP_DISJOINT_FUNC:
  case Item_func::SP_INTERSECTS_FUNC:
  case Item_func::SP_TOUCHES_FUNC:
  case Item_func::SP_CROSSES_FUNC:
  case Item_func::SP_WITHIN_FUNC:
  case Item_func::SP_CONTAINS_FUNC:
  case Item_func::SP_COVEREDBY_FUNC:
  case Item_func::SP_COVERS_FUNC:
  case Item_func::SP_OVERLAPS_FUNC:
  case Item_func::SP_STARTPOINT:
  case Item_func::SP_ENDPOINT:
  case Item_func::SP_EXTERIORRING:
  case Item_func::SP_POINTN:
  case Item_func::SP_GEOMETRYN:
  case Item_func::SP_INTERIORRINGN:
    return true;
  default:
    return false;
  }
}

/**
  Test if 'value' is comparable to 'field' when setting up range
  access for predicate "field OP value". 'field' is a field in the
  table being optimized for while 'value' is whatever 'field' is
  compared to.

  @param cond_func   the predicate item that compares 'field' with 'value'
  @param field       field in the predicate
  @param itype       itMBR if indexed field is spatial, itRAW otherwise
  @param comp_type   comparator for the predicate
  @param value       whatever 'field' is compared to

  @return true if 'field' and 'value' are comparable, false otherwise
*/

static bool comparable_in_index(Item *cond_func,
                                const Field *field,
                                const Field::imagetype itype,
                                Item_func::Functype comp_type,
                                const Item *value)
{
  /*
    Usually an index cannot be used if the column collation differs
    from the operation collation. However, a case insensitive index
    may be used for some binary searches:

       WHERE latin1_swedish_ci_column = 'a' COLLATE lati1_bin;
       WHERE latin1_swedish_ci_colimn = BINARY 'a '
  */
  if ((field->result_type() == STRING_RESULT &&
       field->match_collation_to_optimize_range() &&
       value->result_type() == STRING_RESULT &&
       itype == Field::itRAW &&
       field->charset() != cond_func->compare_collation() &&
       !(cond_func->compare_collation()->state & MY_CS_BINSORT &&
         (comp_type == Item_func::EQUAL_FUNC ||
          comp_type == Item_func::EQ_FUNC))))
    return false;

  /*
    Temporal values: Cannot use range access if:
       'indexed_varchar_column = temporal_value'
    because there are many ways to represent the same date as a
    string. A few examples: "01-01-2001", "1-1-2001", "2001-01-01",
    "2001#01#01". The same problem applies to time. Thus, we cannot
    create a useful range predicate for temporal values into VARCHAR
    column indexes. @see add_key_field()
  */
  if (!field->is_temporal() && value->is_temporal())
    return false;

  /*
    Temporal values: Cannot use range access if 
       'indexed_time = temporal_value_with_date_part'
    because: 
      - without index, a TIME column with value '48:00:00' is 
        equal to a DATETIME column with value 
        'CURDATE() + 2 days' 
      - with range access into the TIME column, CURDATE() + 2 
        days becomes "00:00:00" (Field_timef::store_internal() 
        simply extracts the time part from the datetime) which 
        is a lookup key which does not match "48:00:00". On the other
        hand, we can do ref access for IndexedDatetimeComparedToTime
        because Field_temporal_with_date::store_time() will convert
        48:00:00 to CURDATE() + 2 days which is the correct lookup
        key.
  */
  if (field_time_cmp_date(field, value))
    return false;

  /*
    We can't always use indexes when comparing a string index to a
    number. cmp_type() is checked to allow comparison of dates and
    numbers.
  */
  if (field->result_type() == STRING_RESULT &&
      value->result_type() != STRING_RESULT &&
      field->cmp_type() != value->result_type())
    return false;

  /*
    We can't use indexes when comparing to a JSON value. For example,
    the string '{}' should compare equal to the JSON string "{}". If
    we use a string index to compare the two strings, we will be
    comparing '{}' and '"{}"', which don't compare equal.
  */
  if (value->result_type() == STRING_RESULT &&
      value->field_type() == MYSQL_TYPE_JSON)
    return false;

  return true;
}

static SEL_TREE *
get_mm_parts(RANGE_OPT_PARAM *param, Item_func *cond_func, Field *field,
	     Item_func::Functype type,
	     Item *value, Item_result cmp_type)
{
  DBUG_ENTER("get_mm_parts");

  if (param->has_errors())
    DBUG_RETURN(0);

  if (field->table != param->table)
    DBUG_RETURN(0);

  KEY_PART *key_part = param->key_parts;
  KEY_PART *end = param->key_parts_end;
  SEL_TREE *tree=0;
  if (value &&
      value->used_tables() & ~(param->prev_tables | param->read_tables))
    DBUG_RETURN(0);
  for (; key_part != end ; key_part++)
  {
    if (field->eq(key_part->field))
    {
      /*
        Cannot do range access for spatial operators when a
        non-spatial index is used.
      */
      if (key_part->image_type != Field::itMBR &&
          is_spatial_operator(cond_func->functype()))
        continue;

      SEL_ARG *sel_arg=0;
      if (!tree && !(tree=new (param->mem_root)
                     SEL_TREE(param->mem_root, param->keys)))
        DBUG_RETURN(0); // OOM
      if (!value || !(value->used_tables() & ~param->read_tables))
      {
        sel_arg=get_mm_leaf(param,cond_func,
                            key_part->field,key_part,type,value);
        if (!sel_arg)
          continue;
        if (sel_arg->type == SEL_ARG::IMPOSSIBLE)
        {
          tree->type=SEL_TREE::IMPOSSIBLE;
          DBUG_RETURN(tree);
        }
      }
      else
      {
        /*
          The index may not be used by dynamic range access unless
          'field' and 'value' are comparable.
        */
        if (!comparable_in_index(cond_func, key_part->field,
                                 key_part->image_type,
                                 type, value))
        {
          warn_index_not_applicable(param, key_part->key, field);
          DBUG_RETURN(NULL);
        }

        if (!(sel_arg= new (param->mem_root) SEL_ARG(SEL_ARG::MAYBE_KEY)))
          DBUG_RETURN(NULL);  //OOM
      }
      sel_arg->part=(uchar) key_part->part;
      tree->keys[key_part->key]=sel_add(tree->keys[key_part->key],sel_arg);
      tree->keys_map.set_bit(key_part->key);
    }
  }

  if (tree && tree->merges.is_empty() && tree->keys_map.is_clear_all())
    tree= NULL;
  DBUG_RETURN(tree);
}

/**
  Saves 'value' in 'field' and handles potential type conversion
  problems.

  @param tree [out]                 The SEL_ARG leaf under construction. If 
                                    an always false predicate is found it is 
                                    modified to point to a SEL_ARG with
                                    type == SEL_ARG::IMPOSSIBLE 
  @param value                      The Item that contains a value that shall
                                    be stored in 'field'.
  @param comp_op                    Comparison operator: >, >=, <=> etc.
  @param field                      The field that 'value' is stored into.
  @param impossible_cond_cause[out] Set to a descriptive string if an
                                    impossible condition is found.
  @param memroot                    Memroot for creation of new SEL_ARG.

  @retval false  if saving went fine and it makes sense to continue
                 optimizing for this predicate.
  @retval true   if always true/false predicate was found, in which
                 case 'tree' has been modified to reflect this: NULL
                 pointer if always true, SEL_ARG with type IMPOSSIBLE
                 if always false.
*/
static bool save_value_and_handle_conversion(SEL_ARG **tree,
                                             Item *value,
                                             const Item_func::Functype comp_op,
                                             Field *field,
                                             const char **impossible_cond_cause,
                                             MEM_ROOT *memroot)
{
  // A SEL_ARG should not have been created for this predicate yet.
  DBUG_ASSERT(*tree == NULL);

  if (!value->can_be_evaluated_now())
  {
    /*
      We cannot evaluate the value yet (i.e. required tables are not yet
      locked.)
      This is the case of prune_partitions() called during
      SELECT_LEX::prepare().
    */
    return true;
  }

  // For comparison purposes allow invalid dates like 2000-01-32
  const sql_mode_t orig_sql_mode= field->table->in_use->variables.sql_mode;
  field->table->in_use->variables.sql_mode|= MODE_INVALID_DATES;

  /*
    We want to change "field > value" to "field OP V"
    where:
    * V is what is in "field" after we stored "value" in it via
    save_in_field_no_warning() (such store operation may have done
    rounding...)
    * OP is > or >=, depending on what's correct.
    For example, if c is an INT column,
    "c > 2.9" is changed to "c OP 3"
    where OP is ">=" (">" would not be correct, as 3 > 2.9, a comparison
    done with stored_field_cmp_to_item()). And
    "c > 3.1" is changed to "c OP 3" where OP is ">" (3 < 3.1...).
  */

  // Note that value may be a stored function call, executed here.
  const type_conversion_status err= value->save_in_field_no_warnings(field, true);
  field->table->in_use->variables.sql_mode= orig_sql_mode;

  switch (err) {
  case TYPE_OK:
  case TYPE_NOTE_TRUNCATED:
  case TYPE_WARN_TRUNCATED:
    return false;
  case TYPE_WARN_INVALID_STRING:
    /*
      An invalid string does not produce any rows when used with
      equality operator.
    */
    if (comp_op == Item_func::EQUAL_FUNC || comp_op == Item_func::EQ_FUNC)
    {
      *impossible_cond_cause= "invalid_characters_in_string";
      goto impossible_cond;
    }
    /*
      For other operations on invalid strings, we assume that the range
      predicate is always true and let evaluate_join_record() decide
      the outcome.
    */
    return true;
  case TYPE_ERR_BAD_VALUE:
    /*
      In the case of incompatible values, MySQL's SQL dialect has some
      strange interpretations. For example,

          "int_col > 'foo'" is interpreted as "int_col > 0"

      instead of always false. Because of this, we assume that the
      range predicate is always true instead of always false and let
      evaluate_join_record() decide the outcome.
    */
    return true;
  case TYPE_ERR_NULL_CONSTRAINT_VIOLATION:
    // Checking NULL value on a field that cannot contain NULL.
    *impossible_cond_cause= "null_field_in_non_null_column";
    goto impossible_cond;
  case TYPE_WARN_OUT_OF_RANGE:
    /*
      value to store was either higher than field::max_value or lower
      than field::min_value. The field's max/min value has been stored
      instead.
     */
    if (comp_op == Item_func::EQUAL_FUNC || comp_op == Item_func::EQ_FUNC)
    {
      /*
        Independent of data type, "out_of_range_value =/<=> field" is
        always false.
      */
      *impossible_cond_cause= "value_out_of_range";
      goto impossible_cond;
    }

    // If the field is numeric, we can interpret the out of range value.
    if ((field->type() != FIELD_TYPE_BIT) &&
        (field->result_type() == REAL_RESULT ||
         field->result_type() == INT_RESULT ||
         field->result_type() == DECIMAL_RESULT))
    {
      /*
        value to store was higher than field::max_value if
           a) field has a value greater than 0, or
           b) if field is unsigned and has a negative value (which, when
              cast to unsigned, means some value higher than LLONG_MAX).
      */
      if ((field->val_int() > 0) ||                              // a)
          (static_cast<Field_num*>(field)->unsigned_flag &&
           field->val_int() < 0))                                // b)
      {
        if (comp_op == Item_func::LT_FUNC || comp_op == Item_func::LE_FUNC)
        {
          /*
            '<' or '<=' compared to a value higher than the field
            can store is always true.
          */
          return true;
        }
        if (comp_op == Item_func::GT_FUNC || comp_op == Item_func::GE_FUNC)
        {
          /*
            '>' or '>=' compared to a value higher than the field can
            store is always false.
          */
          *impossible_cond_cause= "value_out_of_range";
          goto impossible_cond;
        }
      }
      else // value is lower than field::min_value
      {
        if (comp_op == Item_func::GT_FUNC || comp_op == Item_func::GE_FUNC)
        {
          /*
            '>' or '>=' compared to a value lower than the field
            can store is always true.
          */
          return true;
        }
        if (comp_op == Item_func::LT_FUNC || comp_op == Item_func::LE_FUNC)
        {
          /*
            '<' or '=' compared to a value lower than the field can
            store is always false.
          */
          *impossible_cond_cause= "value_out_of_range";
          goto impossible_cond;
        }
      }
    }
    /*
      Value is out of range on a datatype where it can't be decided if
      it was underflow or overflow. It is therefore not possible to
      determine whether or not the condition is impossible or always
      true and we have to assume always true.
    */
    return true;
  case TYPE_NOTE_TIME_TRUNCATED:
    if (field->type() == FIELD_TYPE_DATE &&
        (comp_op == Item_func::GT_FUNC || comp_op == Item_func::GE_FUNC ||
         comp_op == Item_func::LT_FUNC || comp_op == Item_func::LE_FUNC))
    {
      /*
        We were saving DATETIME into a DATE column, the conversion went ok
        but a non-zero time part was cut off.

        In MySQL's SQL dialect, DATE and DATETIME are compared as datetime
        values. Index over a DATE column uses DATE comparison. Changing
        from one comparison to the other is possible:

        datetime(date_col)< '2007-12-10 12:34:55' -> date_col<='2007-12-10'
        datetime(date_col)<='2007-12-10 12:34:55' -> date_col<='2007-12-10'

        datetime(date_col)> '2007-12-10 12:34:55' -> date_col>='2007-12-10'
        datetime(date_col)>='2007-12-10 12:34:55' -> date_col>='2007-12-10'

        but we'll need to convert '>' to '>=' and '<' to '<='. This will
        be done together with other types at the end of get_mm_leaf()
        (grep for stored_field_cmp_to_item)
      */
      return false;
    }
    if (comp_op == Item_func::EQ_FUNC || comp_op == Item_func::EQUAL_FUNC)
    {
      // Equality comparison is always false when time info has been truncated.
      goto impossible_cond;
    }
    return true;
  case TYPE_ERR_OOM:
    return true;
    /*
      No default here to avoid adding new conversion status codes that are
      unhandled in this function.
    */
  }

  DBUG_ASSERT(FALSE); // Should never get here.

impossible_cond:
  *tree= new (memroot) SEL_ARG(field, 0, 0);
  (*tree)->type= SEL_ARG::IMPOSSIBLE;
  return true;
}


static SEL_ARG *
get_mm_leaf(RANGE_OPT_PARAM *param, Item *conf_func, Field *field,
            KEY_PART *key_part, Item_func::Functype type,Item *value)
{
  uint maybe_null=(uint) field->real_maybe_null();
  bool optimize_range;
  SEL_ARG *tree= 0;
  MEM_ROOT *alloc= param->mem_root;
  uchar *str;
  const char *impossible_cond_cause= NULL;
  DBUG_ENTER("get_mm_leaf");

  if (param->has_errors())
    goto end;

  /*
    We need to restore the runtime mem_root of the thread in this
    function because it evaluates the value of its argument, while
    the argument can be any, e.g. a subselect. The subselect
    items, in turn, assume that all the memory allocated during
    the evaluation has the same life span as the item itself.
    TODO: opt_range.cc should not reset thd->mem_root at all.
  */
  param->thd->mem_root= param->old_root;
  if (!value)					// IS NULL or IS NOT NULL
  {
    if (field->table->pos_in_table_list->outer_join)
      /*
        Range scan cannot be used to scan the inner table of an outer
        join if the predicate is IS NULL.
      */
      goto end;
    if (!maybe_null)                            // NOT NULL column
    {
      if (type == Item_func::ISNULL_FUNC)
        tree= &null_element;
      goto end;
    }
    uchar *null_string=
      static_cast<uchar*>(alloc_root(alloc, key_part->store_length + 1));
    if (!null_string)
      goto end;                                 // out of memory

    TRASH(null_string, key_part->store_length + 1);
    memcpy(null_string, is_null_string, sizeof(is_null_string));

    if (!(tree= new (alloc) SEL_ARG(field, null_string, null_string)))
      goto end;                                 // out of memory
    if (type == Item_func::ISNOTNULL_FUNC)
    {
      tree->min_flag=NEAR_MIN;		    /* IS NOT NULL ->  X > NULL */
      tree->max_flag=NO_MAX_RANGE;
    }
    goto end;
  }

  /*
    The range access method cannot be used unless 'field' and 'value'
    are comparable in the index. Examples of non-comparable
    field/values: different collation, DATETIME vs TIME etc.
  */
  if (!comparable_in_index(conf_func, field, key_part->image_type,
                           type, value))
  {
    warn_index_not_applicable(param, key_part->key, field);
    goto end;
  }

  if (key_part->image_type == Field::itMBR)
  {
    // @todo: use is_spatial_operator() instead?
    switch (type) {
    case Item_func::SP_EQUALS_FUNC:
    case Item_func::SP_DISJOINT_FUNC:
    case Item_func::SP_INTERSECTS_FUNC:
    case Item_func::SP_TOUCHES_FUNC:
    case Item_func::SP_CROSSES_FUNC:
    case Item_func::SP_WITHIN_FUNC:
    case Item_func::SP_CONTAINS_FUNC:
    case Item_func::SP_OVERLAPS_FUNC:
      break;
    default:
      /* 
        We cannot involve spatial indexes for queries that
        don't use MBREQUALS(), MBRDISJOINT(), etc. functions.
      */
      goto end;
    }
  }

  if (param->using_real_indexes)
    optimize_range= field->optimize_range(param->real_keynr[key_part->key],
                                          key_part->part);
  else
    optimize_range= TRUE;

  if (type == Item_func::LIKE_FUNC)
  {
    bool like_error;
    char buff1[MAX_FIELD_WIDTH];
    uchar *min_str,*max_str;
    String tmp(buff1,sizeof(buff1),value->collation.collation),*res;
    size_t length, offset, min_length, max_length;
    size_t field_length= field->pack_length()+maybe_null;

    if (!optimize_range)
      goto end;
    if (!(res= value->val_str(&tmp)))
    {
      tree= &null_element;
      goto end;
    }

    /*
      TODO:
      Check if this was a function. This should have be optimized away
      in the sql_select.cc
    */
    if (res != &tmp)
    {
      tmp.copy(*res);				// Get own copy
      res= &tmp;
    }
    if (field->cmp_type() != STRING_RESULT)
      goto end;                                 // Can only optimize strings

    offset=maybe_null;
    length=key_part->store_length;

    if (length != key_part->length  + maybe_null)
    {
      /* key packed with length prefix */
      offset+= HA_KEY_BLOB_LENGTH;
      field_length= length - HA_KEY_BLOB_LENGTH;
    }
    else
    {
      if (unlikely(length < field_length))
      {
	/*
	  This can only happen in a table created with UNIREG where one key
	  overlaps many fields
	*/
	length= field_length;
      }
      else
	field_length= length;
    }
    length+=offset;
    if (!(min_str= (uchar*) alloc_root(alloc, length*2)))
      goto end;

    max_str=min_str+length;
    if (maybe_null)
      max_str[0]= min_str[0]=0;

    Item_func_like *like_func= static_cast<Item_func_like*>(param->cond);

    // We can only optimize with LIKE if the escape string is known.
    if (!like_func->escape_is_evaluated())
      goto end;

    field_length-= maybe_null;
    like_error= my_like_range(field->charset(),
			      res->ptr(), res->length(),
			      like_func->escape,
			      wild_one, wild_many,
			      field_length,
			      (char*) min_str+offset, (char*) max_str+offset,
			      &min_length, &max_length);
    if (like_error)				// Can't optimize with LIKE
      goto end;

    if (offset != maybe_null)			// BLOB or VARCHAR
    {
      int2store(min_str+maybe_null, static_cast<uint16>(min_length));
      int2store(max_str+maybe_null, static_cast<uint16>(max_length));
    }
    tree= new (alloc) SEL_ARG(field, min_str, max_str);
    goto end;
  }

  if (!optimize_range &&
      type != Item_func::EQ_FUNC &&
      type != Item_func::EQUAL_FUNC)
    goto end;                                   // Can't optimize this

  /*
    Geometry operations may mix geometry types, e.g., we may be
    checking ST_Contains(<polygon field>, <point>). In such cases,
    field->geom_type will be a different type than the value we're
    trying to store in it, and the conversion will fail. Therefore,
    set the most general geometry type while saving, and revert to the
    original geometry type afterwards.
  */
  {
    const Field::geometry_type save_geom_type=
      (field->type() == MYSQL_TYPE_GEOMETRY) ?
      field->get_geometry_type() :
      Field::GEOM_GEOMETRY;
    if (field->type() == MYSQL_TYPE_GEOMETRY)
    {
      down_cast<Field_geom*>(field)->geom_type= Field::GEOM_GEOMETRY;
    }

    bool always_true_or_false=
      save_value_and_handle_conversion(&tree, value, type, field,
                                       &impossible_cond_cause, alloc);

    if (field->type() == MYSQL_TYPE_GEOMETRY &&
        save_geom_type != Field::GEOM_GEOMETRY)
    {
      down_cast<Field_geom*>(field)->geom_type= save_geom_type;
    }

    if (always_true_or_false)
      goto end;
  }

  /*
    Any sargable predicate except "<=>" involving NULL as a constant is always
    FALSE
  */
  if (type != Item_func::EQUAL_FUNC && field->is_real_null())
  {
    impossible_cond_cause= "comparison_with_null_always_false";
    tree= &null_element;
    goto end;
  }
  
  str= (uchar*) alloc_root(alloc, key_part->store_length+1);
  if (!str)
    goto end;
  if (maybe_null)
    *str= (uchar) field->is_real_null();        // Set to 1 if null
  field->get_key_image(str+maybe_null, key_part->length,
                       key_part->image_type);
  if (!(tree= new (alloc) SEL_ARG(field, str, str)))
    goto end;                                   // out of memory

  /*
    Check if we are comparing an UNSIGNED integer with a negative constant.
    In this case we know that:
    (a) (unsigned_int [< | <=] negative_constant) == FALSE
    (b) (unsigned_int [> | >=] negative_constant) == TRUE
    In case (a) the condition is false for all values, and in case (b) it
    is true for all values, so we can avoid unnecessary retrieval and condition
    testing, and we also get correct comparison of unsinged integers with
    negative integers (which otherwise fails because at query execution time
    negative integers are cast to unsigned if compared with unsigned).
   */
  if (field->result_type() == INT_RESULT &&
      value->result_type() == INT_RESULT &&
      ((field->type() == FIELD_TYPE_BIT || 
       ((Field_num *) field)->unsigned_flag) && 
       !((Item_int*) value)->unsigned_flag))
  {
    longlong item_val= value->val_int();
    if (item_val < 0)
    {
      if (type == Item_func::LT_FUNC || type == Item_func::LE_FUNC)
      {
        impossible_cond_cause= "unsigned_int_cannot_be_negative";
        tree->type= SEL_ARG::IMPOSSIBLE;
        goto end;
      }
      if (type == Item_func::GT_FUNC || type == Item_func::GE_FUNC)
      {
        tree= 0;
        goto end;
      }
    }
  }

  switch (type) {
  case Item_func::LT_FUNC:
    /* Don't use open ranges for partial key_segments */
    if ((!(key_part->flag & HA_PART_KEY_SEG)) &&
        stored_field_cmp_to_item(param->thd, field, value) == 0)
      tree->max_flag=NEAR_MAX;
    /* fall through */
  case Item_func::LE_FUNC:
    if (!maybe_null)
      tree->min_flag=NO_MIN_RANGE;		/* From start */
    else
    {						// > NULL
      if (!(tree->min_value=
            static_cast<uchar*>(alloc_root(alloc, key_part->store_length+1))))
        goto end;
      TRASH(tree->min_value, key_part->store_length + 1);
      memcpy(tree->min_value, is_null_string, sizeof(is_null_string));
      tree->min_flag=NEAR_MIN;
    }
    break;
  case Item_func::GT_FUNC:
    /* Don't use open ranges for partial key_segments */
    if ((!(key_part->flag & HA_PART_KEY_SEG)) &&
        (stored_field_cmp_to_item(param->thd, field, value) <= 0))
      tree->min_flag=NEAR_MIN;
    tree->max_flag= NO_MAX_RANGE;
    break;
  case Item_func::GE_FUNC:
    /* Don't use open ranges for partial key_segments */
    if ((!(key_part->flag & HA_PART_KEY_SEG)) &&
        (stored_field_cmp_to_item(param->thd, field, value) < 0))
      tree->min_flag= NEAR_MIN;
    tree->max_flag=NO_MAX_RANGE;
    break;
  case Item_func::SP_EQUALS_FUNC:
    tree->set_gis_index_read_function(HA_READ_MBR_EQUAL);
    break;
  case Item_func::SP_DISJOINT_FUNC:
    tree->set_gis_index_read_function(HA_READ_MBR_DISJOINT);
    break;
  case Item_func::SP_INTERSECTS_FUNC:
    tree->set_gis_index_read_function(HA_READ_MBR_INTERSECT);
    break;
  case Item_func::SP_TOUCHES_FUNC:
    tree->set_gis_index_read_function(HA_READ_MBR_INTERSECT);
    break;

  case Item_func::SP_CROSSES_FUNC:
    tree->set_gis_index_read_function(HA_READ_MBR_INTERSECT);
    break;
  case Item_func::SP_WITHIN_FUNC:
    /*
      Adjust the rkey_func_flag as it's assumed and observed that both
      MyISAM and Innodb implement this function in reverse order.
    */
    tree->set_gis_index_read_function(HA_READ_MBR_CONTAIN);
    break;

  case Item_func::SP_CONTAINS_FUNC:
    /*
      Adjust the rkey_func_flag as it's assumed and observed that both
      MyISAM and Innodb implement this function in reverse order.
    */
    tree->set_gis_index_read_function(HA_READ_MBR_WITHIN);
    break;
  case Item_func::SP_OVERLAPS_FUNC:
    tree->set_gis_index_read_function(HA_READ_MBR_INTERSECT);
    break;

  default:
    break;
  }

end:
  if (impossible_cond_cause != NULL)
  {
    Opt_trace_object wrapper (&param->thd->opt_trace);
    Opt_trace_object (&param->thd->opt_trace, "impossible_condition",
                      Opt_trace_context::RANGE_OPTIMIZER).
      add_alnum("cause", impossible_cond_cause);
  }
  param->thd->mem_root= alloc;
  DBUG_RETURN(tree);
}


/*
  Add a new key test to a key when scanning through all keys
  This will never be called for same key parts.
*/

static SEL_ARG *
sel_add(SEL_ARG *key1,SEL_ARG *key2)
{
  SEL_ARG *root,**key_link;

  if (!key1)
    return key2;
  if (!key2)
    return key1;

  key_link= &root;
  while (key1 && key2)
  {
    if (key1->part < key2->part)
    {
      *key_link= key1;
      key_link= &key1->next_key_part;
      key1=key1->next_key_part;
    }
    else
    {
      *key_link= key2;
      key_link= &key2->next_key_part;
      key2=key2->next_key_part;
    }
  }
  *key_link=key1 ? key1 : key2;
  return root;
}

#define CLONE_KEY1_MAYBE 1
#define CLONE_KEY2_MAYBE 2
#define swap_clone_flag(A) ((A & 1) << 1) | ((A & 2) >> 1)


static SEL_TREE *
tree_and(RANGE_OPT_PARAM *param,SEL_TREE *tree1,SEL_TREE *tree2)
{
  DBUG_ENTER("tree_and");

  if (param->has_errors())
    DBUG_RETURN(0);

  if (!tree1)
    DBUG_RETURN(tree2);
  if (!tree2)
    DBUG_RETURN(tree1);
  if (tree1->type == SEL_TREE::IMPOSSIBLE || tree2->type == SEL_TREE::ALWAYS)
    DBUG_RETURN(tree1);
  if (tree2->type == SEL_TREE::IMPOSSIBLE || tree1->type == SEL_TREE::ALWAYS)
    DBUG_RETURN(tree2);
  if (tree1->type == SEL_TREE::MAYBE)
  {
    if (tree2->type == SEL_TREE::KEY)
      tree2->type=SEL_TREE::KEY_SMALLER;
    DBUG_RETURN(tree2);
  }
  if (tree2->type == SEL_TREE::MAYBE)
  {
    tree1->type=SEL_TREE::KEY_SMALLER;
    DBUG_RETURN(tree1);
  }

  dbug_print_tree("tree1", tree1, param);
  dbug_print_tree("tree2", tree2, param);

  key_map  result_keys;
  
  /* Join the trees key per key */
  SEL_ARG **key1,**key2;
  for (uint idx=0; idx< param->keys; idx++)
  {
    key1= &tree1->keys[idx];
    key2= &tree2->keys[idx];

    uint flag=0;
    if (*key1 || *key2)
    {
      if (*key1 && !(*key1)->simple_key())
	flag|=CLONE_KEY1_MAYBE;
      if (*key2 && !(*key2)->simple_key())
	flag|=CLONE_KEY2_MAYBE;
      *key1= key_and(param, *key1, *key2, flag);
      if (*key1)
      {
        if ((*key1)->type == SEL_ARG::IMPOSSIBLE)
        {
          tree1->type= SEL_TREE::IMPOSSIBLE;
          DBUG_RETURN(tree1);
        }
        result_keys.set_bit(idx);
#ifndef DBUG_OFF
        /*
          Do not test use_count if there is a large range tree created.
          It takes too much time to traverse the tree.
        */
        if (param->mem_root->allocated_size < 2097152)
          (*key1)->test_use_count(*key1);
#endif
      }

    }
  }
  tree1->keys_map= result_keys;

  /* ok, both trees are index_merge trees */
  imerge_list_and_list(&tree1->merges, &tree2->merges);
  DBUG_RETURN(tree1);
}


/*
  Check if two SEL_TREES can be combined into one (i.e. a single key range
  read can be constructed for "cond_of_tree1 OR cond_of_tree2" ) without
  using index_merge.
*/

bool sel_trees_can_be_ored(SEL_TREE *tree1, SEL_TREE *tree2, 
                           RANGE_OPT_PARAM* param)
{
  key_map common_keys= tree1->keys_map;
  DBUG_ENTER("sel_trees_can_be_ored");
  common_keys.intersect(tree2->keys_map);

  dbug_print_tree("tree1", tree1, param);
  dbug_print_tree("tree2", tree2, param);

  if (common_keys.is_clear_all())
    DBUG_RETURN(FALSE);

  /* trees have a common key, check if they refer to same key part */
  SEL_ARG *key1,*key2;
  for (uint key_no=0; key_no < param->keys; key_no++)
  {
    if (common_keys.is_set(key_no))
    {
      key1= tree1->keys[key_no];
      key2= tree2->keys[key_no];
      /* GIS_OPTIMIZER_FIXME: temp solution. key1 could be all nulls */
      if (key1 && key2 && key1->part == key2->part)
        DBUG_RETURN(TRUE);
    }
  }
  DBUG_RETURN(FALSE);
}


/*
  Remove the trees that are not suitable for record retrieval.
  SYNOPSIS
    param  Range analysis parameter
    tree   Tree to be processed, tree->type is KEY or KEY_SMALLER
 
  DESCRIPTION
    This function walks through tree->keys[] and removes the SEL_ARG* trees
    that are not "maybe" trees (*) and cannot be used to construct quick range
    selects.
    (*) - have type MAYBE or MAYBE_KEY. Perhaps we should remove trees of
          these types here as well.

    A SEL_ARG* tree cannot be used to construct quick select if it has
    tree->part != 0. (e.g. it could represent "keypart2 < const").

    WHY THIS FUNCTION IS NEEDED
    
    Normally we allow construction of SEL_TREE objects that have SEL_ARG
    trees that do not allow quick range select construction. For example for
    " keypart1=1 AND keypart2=2 " the execution will proceed as follows:
    tree1= SEL_TREE { SEL_ARG{keypart1=1} }
    tree2= SEL_TREE { SEL_ARG{keypart2=2} } -- can't make quick range select
                                               from this
    call tree_and(tree1, tree2) -- this joins SEL_ARGs into a usable SEL_ARG
                                   tree.
    
    There is an exception though: when we construct index_merge SEL_TREE,
    any SEL_ARG* tree that cannot be used to construct quick range select can
    be removed, because current range analysis code doesn't provide any way
    that tree could be later combined with another tree.
    Consider an example: we should not construct
    st1 = SEL_TREE { 
      merges = SEL_IMERGE { 
                            SEL_TREE(t.key1part1 = 1), 
                            SEL_TREE(t.key2part2 = 2)   -- (*)
                          } 
                   };
    because 
     - (*) cannot be used to construct quick range select, 
     - There is no execution path that would cause (*) to be converted to 
       a tree that could be used.

    The latter is easy to verify: first, notice that the only way to convert
    (*) into a usable tree is to call tree_and(something, (*)).

    Second look at what tree_and/tree_or function would do when passed a
    SEL_TREE that has the structure like st1 tree has, and conlcude that 
    tree_and(something, (*)) will not be called.

  RETURN
    0  Ok, some suitable trees left
    1  No tree->keys[] left.
*/

static bool remove_nonrange_trees(RANGE_OPT_PARAM *param, SEL_TREE *tree)
{
  bool res= FALSE;
  for (uint i=0; i < param->keys; i++)
  {
    if (tree->keys[i])
    {
      if (tree->keys[i]->part)
      {
        tree->keys[i]= NULL;
        tree->keys_map.clear_bit(i);
      }
      else
        res= TRUE;
    }
  }
  return !res;
}


static SEL_TREE *
tree_or(RANGE_OPT_PARAM *param,SEL_TREE *tree1,SEL_TREE *tree2)
{
  DBUG_ENTER("tree_or");

  if (param->has_errors())
    DBUG_RETURN(0);

  if (!tree1 || !tree2)
    DBUG_RETURN(0);
  if (tree1->type == SEL_TREE::IMPOSSIBLE || tree2->type == SEL_TREE::ALWAYS)
    DBUG_RETURN(tree2);
  if (tree2->type == SEL_TREE::IMPOSSIBLE || tree1->type == SEL_TREE::ALWAYS)
    DBUG_RETURN(tree1);
  if (tree1->type == SEL_TREE::MAYBE)
    DBUG_RETURN(tree1);				// Can't use this
  if (tree2->type == SEL_TREE::MAYBE)
    DBUG_RETURN(tree2);

  /*
    It is possible that a tree contains both 
    a) simple range predicates (in tree->keys[]) and
    b) index merge range predicates (in tree->merges)

    If a tree has both, they represent equally *valid* range
    predicate alternatives; both will return all relevant rows from
    the table but one may return more unnecessary rows than the
    other (additional rows will be filtered later). However, doing
    an OR operation on trees with both types of predicates is too
    complex at the time. We therefore remove the index merge
    predicates (if we have both types) before OR'ing the trees.

    TODO: enable tree_or() for trees with both simple and index
    merge range predicates.
  */
  if (!tree1->merges.is_empty())
  {
    for (uint i= 0; i < param->keys; i++)
      if (tree1->keys[i] != NULL && tree1->keys[i] != &null_element)
      {
        tree1->merges.empty();
        break;
      }
  }
  if (!tree2->merges.is_empty())
  {
    for (uint i= 0; i< param->keys; i++)
      if (tree2->keys[i] != NULL && tree2->keys[i] != &null_element)
      {
        tree2->merges.empty();
        break;
      }
  }

  SEL_TREE *result= 0;
  key_map  result_keys;
  if (sel_trees_can_be_ored(tree1, tree2, param))
  {
    /* Join the trees key per key */
    SEL_ARG **key1,**key2;
    for (uint idx=0; idx < param->keys; idx++)
    {
      key1= &tree1->keys[idx];
      key2= &tree2->keys[idx];
      *key1= key_or(param, *key1, *key2);
      if (*key1)
      {
        result=tree1;				// Added to tree1
        result_keys.set_bit(idx);
#ifndef DBUG_OFF
        /*
          Do not test use count if there is a large range tree created.
          It takes too much time to traverse the tree.
        */
        if (param->mem_root->allocated_size < 2097152)
          (*key1)->test_use_count(*key1);
#endif
      }
    }
    if (result)
      result->keys_map= result_keys;
  }
  else
  {
    /* ok, two trees have KEY type but cannot be used without index merge */
    if (tree1->merges.is_empty() && tree2->merges.is_empty())
    {
      if (param->remove_jump_scans)
      {
        bool no_trees= remove_nonrange_trees(param, tree1);
        no_trees= no_trees || remove_nonrange_trees(param, tree2);
        if (no_trees)
          DBUG_RETURN(new (param->mem_root)
                      SEL_TREE(SEL_TREE::ALWAYS, param->mem_root, param->keys));
      }
      SEL_IMERGE *merge;
      /* both trees are "range" trees, produce new index merge structure */
      if (!(result= new (param->mem_root)
            SEL_TREE(param->mem_root, param->keys)) ||
          !(merge= new (param->mem_root) SEL_IMERGE()) ||
          (result->merges.push_back(merge)) ||
          (merge->or_sel_tree(param, tree1)) ||
          (merge->or_sel_tree(param, tree2)))
        result= NULL;
      else
        result->type= tree1->type;
    }
    else if (!tree1->merges.is_empty() && !tree2->merges.is_empty())
    {
      if (imerge_list_or_list(param, &tree1->merges, &tree2->merges))
        result= new (param->mem_root)
          SEL_TREE(SEL_TREE::ALWAYS, param->mem_root, param->keys);
      else
        result= tree1;
    }
    else
    {
      /* one tree is index merge tree and another is range tree */
      if (tree1->merges.is_empty())
        swap_variables(SEL_TREE*, tree1, tree2);
      
      if (param->remove_jump_scans && remove_nonrange_trees(param, tree2))
         DBUG_RETURN(new (param->mem_root)
                     SEL_TREE(SEL_TREE::ALWAYS, param->mem_root, param->keys));
      /* add tree2 to tree1->merges, checking if it collapses to ALWAYS */
      if (imerge_list_or_tree(param, &tree1->merges, tree2))
        result= new (param->mem_root)
          SEL_TREE(SEL_TREE::ALWAYS, param->mem_root, param->keys);
      else
        result= tree1;
    }
  }
  DBUG_RETURN(result);
}


/* And key trees where key1->part < key2 -> part */

static SEL_ARG *
and_all_keys(RANGE_OPT_PARAM *param, SEL_ARG *key1, SEL_ARG *key2, 
             uint clone_flag)
{
  SEL_ARG *next;
  ulong use_count=key1->use_count;

  if (key1->elements != 1)
  {
    key2->use_count+=key1->elements-1; //psergey: why we don't count that key1 has n-k-p?
    key2->increment_use_count((int) key1->elements-1);
  }
  if (key1->type == SEL_ARG::MAYBE_KEY)
  {
    // See todo for left/right pointers
    DBUG_ASSERT(!key1->left);
    DBUG_ASSERT(!key1->right);
    key1->next= key1->prev= 0;
  }
  for (next=key1->first(); next ; next=next->next)
  {
    if (next->next_key_part)
    {
      SEL_ARG *tmp= key_and(param, next->next_key_part, key2, clone_flag);
      if (tmp && tmp->type == SEL_ARG::IMPOSSIBLE)
      {
	key1=key1->tree_delete(next);
	continue;
      }
      next->next_key_part=tmp;
      if (use_count)
	next->increment_use_count(use_count);
    }
    else
      next->next_key_part=key2;
  }
  if (!key1)
    return &null_element;			// Impossible ranges
  key1->use_count++;
  return key1;
}


/*
  Produce a SEL_ARG graph that represents "key1 AND key2"

  SYNOPSIS
    key_and()
      param   Range analysis context (needed to track if we have allocated
              too many SEL_ARGs)
      key1    First argument, root of its RB-tree
      key2    Second argument, root of its RB-tree

  RETURN
    RB-tree root of the resulting SEL_ARG graph.
    NULL if the result of AND operation is an empty interval {0}.
*/

static SEL_ARG *
key_and(RANGE_OPT_PARAM *param, SEL_ARG *key1, SEL_ARG *key2, uint clone_flag)
{
  if (param->has_errors())
    return 0;

  if (key1 == NULL || key1->type == SEL_ARG::ALWAYS)
    return key2;
  if (key2 == NULL || key2->type == SEL_ARG::ALWAYS)
    return key1;
  if (key1->part != key2->part)
  {
    if (key1->part > key2->part)
    {
      swap_variables(SEL_ARG *, key1, key2);
      clone_flag=swap_clone_flag(clone_flag);
    }
    // key1->part < key2->part
    key1->use_count--;
    if (key1->use_count > 0)
      if (!(key1= key1->clone_tree(param)))
	return 0;				// OOM
    return and_all_keys(param, key1, key2, clone_flag);
  }

  if (((clone_flag & CLONE_KEY2_MAYBE) &&
       !(clone_flag & CLONE_KEY1_MAYBE) &&
       key2->type != SEL_ARG::MAYBE_KEY) ||
      key1->type == SEL_ARG::MAYBE_KEY)
  {						// Put simple key in key2
    swap_variables(SEL_ARG *, key1, key2);
    clone_flag=swap_clone_flag(clone_flag);
  }

  /* If one of the key is MAYBE_KEY then the found region may be smaller */
  if (key2->type == SEL_ARG::MAYBE_KEY)
  {
    if (key1->use_count > 1)
    {
      key1->use_count--;
      if (!(key1=key1->clone_tree(param)))
	return 0;				// OOM
      key1->use_count++;
    }
    if (key1->type == SEL_ARG::MAYBE_KEY)
    {						// Both are maybe key
      key1->next_key_part=key_and(param, key1->next_key_part, 
                                  key2->next_key_part, clone_flag);
      if (key1->next_key_part &&
	  key1->next_key_part->type == SEL_ARG::IMPOSSIBLE)
	return key1;
    }
    else
    {
      key1->maybe_smaller();
      if (key2->next_key_part)
      {
	key1->use_count--;			// Incremented in and_all_keys
	return and_all_keys(param, key1, key2, clone_flag);
      }
      key2->use_count--;			// Key2 doesn't have a tree
    }
    return key1;
  }

  if ((key1->min_flag | key2->min_flag) & GEOM_FLAG)
  {
    /*
      Cannot optimize geometry ranges. The next best thing is to keep
      one of them.
    */
    key2->free_tree();
    return key1;
  }

  key1->use_count--;
  key2->use_count--;
  SEL_ARG *e1=key1->first(), *e2=key2->first(), *new_tree=0;

  while (e1 && e2)
  {
    int cmp=e1->cmp_min_to_min(e2);
    if (cmp < 0)
    {
      if (get_range(&e1,&e2,key1))
	continue;
    }
    else if (get_range(&e2,&e1,key2))
      continue;
    SEL_ARG *next=key_and(param, e1->next_key_part, e2->next_key_part,
                          clone_flag);
    e1->increment_use_count(1);
    e2->increment_use_count(1);
    if (!next || next->type != SEL_ARG::IMPOSSIBLE)
    {
      SEL_ARG *new_arg= e1->clone_and(e2, param->mem_root);
      if (!new_arg)
	return &null_element;			// End of memory
      new_arg->next_key_part=next;
      if (!new_tree)
      {
	new_tree=new_arg;
      }
      else
	new_tree=new_tree->insert(new_arg);
    }
    if (e1->cmp_max_to_max(e2) < 0)
      e1=e1->next;				// e1 can't overlapp next e2
    else
      e2=e2->next;
  }
  key1->free_tree();
  key2->free_tree();
  if (!new_tree)
    return &null_element;			// Impossible range
  return new_tree;
}


static bool
get_range(SEL_ARG **e1,SEL_ARG **e2,SEL_ARG *root1)
{
  (*e1)=root1->find_range(*e2);			// first e1->min < e2->min
  if ((*e1)->cmp_max_to_min(*e2) < 0)
  {
    if (!((*e1)=(*e1)->next))
      return 1;
    if ((*e1)->cmp_min_to_max(*e2) > 0)
    {
      (*e2)=(*e2)->next;
      return 1;
    }
  }
  return 0;
}


/**
   Combine two range expression under a common OR. On a logical level, the
   transformation is key_or( expr1, expr2 ) => expr1 OR expr2.

   Both expressions are assumed to be in the SEL_ARG format. In a logic sense,
   theformat is reminiscent of DNF, since an expression such as the following

   ( 1 < kp1 < 10 AND p1 ) OR ( 10 <= kp2 < 20 AND p2 )

   where there is a key consisting of keyparts ( kp1, kp2, ..., kpn ) and p1
   and p2 are valid SEL_ARG expressions over keyparts kp2 ... kpn, is a valid
   SEL_ARG condition. The disjuncts appear ordered by the minimum endpoint of
   the first range and ranges must not overlap. It follows that they are also
   ordered by maximum endpoints. Thus

   ( 1 < kp1 <= 2 AND ( kp2 = 2 OR kp2 = 3 ) ) OR kp1 = 3

   Is a a valid SER_ARG expression for a key of at least 2 keyparts.
   
   For simplicity, we will assume that expr2 is a single range predicate,
   i.e. on the form ( a < x < b AND ... ). It is easy to generalize to a
   disjunction of several predicates by subsequently call key_or for each
   disjunct.

   The algorithm iterates over each disjunct of expr1, and for each disjunct
   where the first keypart's range overlaps with the first keypart's range in
   expr2:
   
   If the predicates are equal for the rest of the keyparts, or if there are
   no more, the range in expr2 has its endpoints copied in, and the SEL_ARG
   node in expr2 is deallocated. If more ranges became connected in expr1, the
   surplus is also dealocated. If they differ, two ranges are created.
   
   - The range leading up to the overlap. Empty if endpoints are equal.

   - The overlapping sub-range. May be the entire range if they are equal.

   Finally, there may be one more range if expr2's first keypart's range has a
   greater maximum endpoint than the last range in expr1.

   For the overlapping sub-range, we recursively call key_or. Thus in order to
   compute key_or of

     (1) ( 1 < kp1 < 10 AND 1 < kp2 < 10 ) 

     (2) ( 2 < kp1 < 20 AND 4 < kp2 < 20 )

   We create the ranges 1 < kp <= 2, 2 < kp1 < 10, 10 <= kp1 < 20. For the
   first one, we simply hook on the condition for the second keypart from (1)
   : 1 < kp2 < 10. For the second range 2 < kp1 < 10, key_or( 1 < kp2 < 10, 4
   < kp2 < 20 ) is called, yielding 1 < kp2 < 20. For the last range, we reuse
   the range 4 < kp2 < 20 from (2) for the second keypart. The result is thus
   
   ( 1  <  kp1 <= 2 AND 1 < kp2 < 10 ) OR
   ( 2  <  kp1 < 10 AND 1 < kp2 < 20 ) OR
   ( 10 <= kp1 < 20 AND 4 < kp2 < 20 )

   @param param    PARAM from test_quick_select
   @param key1     Root of RB-tree of SEL_ARGs to be ORed with key2
   @param key2     Root of RB-tree of SEL_ARGs to be ORed with key1
*/
static SEL_ARG *
key_or(RANGE_OPT_PARAM *param, SEL_ARG *key1, SEL_ARG *key2)
{

  if (param->has_errors())
    return 0;

  if (key1 == NULL || key1->type == SEL_ARG::ALWAYS)
  {
    if (key2)
    {
      key2->use_count--;
      key2->free_tree();
    }
    return key1;
  }
  if (key2 == NULL || key2->type == SEL_ARG::ALWAYS)
    // Case is symmetric to the one above, just flip parameters.
    return key_or(param, key2, key1);

  key1->use_count--;
  key2->use_count--;

  if (key1->part != key2->part || 
      (key1->min_flag | key2->min_flag) & GEOM_FLAG)
  {
    key1->free_tree();
    key2->free_tree();
    return 0;                                   // Can't optimize this
  }

  // If one of the key is MAYBE_KEY then the found region may be bigger
  if (key1->type == SEL_ARG::MAYBE_KEY)
  {
    key2->free_tree();
    key1->use_count++;
    return key1;
  }
  if (key2->type == SEL_ARG::MAYBE_KEY)
  {
    key1->free_tree();
    key2->use_count++;
    return key2;
  }

  if (key1->use_count > 0)
  {
    if (key2->use_count == 0 || key1->elements > key2->elements)
    {
      swap_variables(SEL_ARG *,key1,key2);
    }
    if (key1->use_count > 0 && (key1= key1->clone_tree(param)) == NULL)
      return 0;                                 // OOM
  }

  // Add tree at key2 to tree at key1
  const bool key2_shared= (key2->use_count != 0);
  key1->maybe_flag|= key2->maybe_flag;

  /*
    Notation for illustrations used in the rest of this function: 

      Range: [--------]
             ^        ^
             start    stop

      Two overlapping ranges:
        [-----]               [----]            [--]
            [---]     or    [---]       or   [-------]

      Ambiguity: *** 
        The range starts or stops somewhere in the "***" range.
        Example: a starts before b and may end before/the same place/after b
        a: [----***]
        b:   [---]

      Adjacent ranges:
        Ranges that meet but do not overlap. Example: a = "x < 3", b = "x >= 3"
        a: ----]
        b:      [----
   */

  SEL_ARG *cur_key2= key2->first();
  while (cur_key2)
  {
    /*
      key1 consists of one or more ranges. cur_key1 is the
      range currently being handled.

      initialize cur_key1 to the latest range in key1 that starts the
      same place or before the range in cur_key2 starts

      cur_key2:            [------]
      key1:      [---] [-----] [----]
                       ^
                       cur_key1
    */
    SEL_ARG *cur_key1= key1->find_range(cur_key2);

    /*
      Used to describe how two key values are positioned compared to
      each other. Consider key_value_a.<cmp_func>(key_value_b):

        -2: key_value_a is smaller than key_value_b, and they are adjacent
        -1: key_value_a is smaller than key_value_b (not adjacent)
         0: the key values are equal
         1: key_value_a is bigger than key_value_b (not adjacent)
         2: key_value_a is bigger than key_value_b, and they are adjacent

      Example: "cmp= cur_key1->cmp_max_to_min(cur_key2)"

      cur_key2:          [--------           (10 <= x ...  )
      cur_key1:    -----]                    (  ... x <  10) => cmp==-2
      cur_key1:    ----]                     (  ... x <   9) => cmp==-1
      cur_key1:    ------]                   (  ... x <= 10) => cmp== 0
      cur_key1:    --------]                 (  ... x <= 12) => cmp== 1
      (cmp == 2 does not make sense for cmp_max_to_min())
     */
    int cmp= 0;

    if (!cur_key1)
    {
      /*
        The range in cur_key2 starts before the first range in key1. Use
        the first range in key1 as cur_key1.

        cur_key2: [--------]
        key1:            [****--] [----]   [-------]
                         ^
                         cur_key1
      */
      cur_key1= key1->first();
      cmp= -1;
    }
    else if ((cmp= cur_key1->cmp_max_to_min(cur_key2)) < 0)
    {
      /*
        This is the case:
        cur_key2:           [-------]
        cur_key1:   [----**]
       */
      SEL_ARG *next_key1= cur_key1->next;
      if (cmp == -2 && 
          eq_tree(cur_key1->next_key_part, cur_key2->next_key_part))
      {
        /*
          Adjacent (cmp==-2) and equal next_key_parts => ranges can be merged

          This is the case:
          cur_key2:           [-------]
          cur_key1:     [----]

          Result:
          cur_key2:     [-------------]     => inserted into key1 below
          cur_key1:                         => deleted
        */
        SEL_ARG *next_key2= cur_key2->next;
        if (key2_shared)
        {
          if (!(cur_key2= new (param->mem_root) SEL_ARG(*cur_key2)))
            return 0;           // out of memory
          cur_key2->increment_use_count(key1->use_count+1);
          cur_key2->next= next_key2;                 // New copy of cur_key2
        }

        if (cur_key2->copy_min(cur_key1))
        {
          // cur_key2 is full range: [-inf <= cur_key2 <= +inf]
          key1->free_tree();
          key2->free_tree();
          key1->type= SEL_ARG::ALWAYS;
          key2->type= SEL_ARG::ALWAYS;
          if (key1->maybe_flag)
            return new (param->mem_root) SEL_ARG(SEL_ARG::MAYBE_KEY);
          return 0;
        }

        if (!(key1= key1->tree_delete(cur_key1)))
        {
          /*
            cur_key1 was the last range in key1; move the cur_key2
            range that was merged above to key1
          */
          key1= cur_key2;
          key1->make_root();
          cur_key2= next_key2;
          break;
        }
      }
      // Move to next range in key1. Now cur_key1.min > cur_key2.min
      if (!(cur_key1= next_key1)) 
        break;         // No more ranges in key1. Copy rest of key2
    }

    if (cmp < 0)
    {
      /*
        This is the case:
        cur_key2:   [--***]
        cur_key1:       [----]
      */
      int cur_key1_cmp;
      if ((cur_key1_cmp= cur_key1->cmp_min_to_max(cur_key2)) > 0)
      {
        /*
          This is the case:
          cur_key2:  [------**]
          cur_key1:            [----]
        */
        if (cur_key1_cmp == 2 && 
            eq_tree(cur_key1->next_key_part, cur_key2->next_key_part))
        {
          /*
            Adjacent ranges with equal next_key_part. Merge like this:

            This is the case:
            cur_key2:    [------]
            cur_key1:            [-----]

            Result:
            cur_key2:    [------]
            cur_key1:    [-------------]

            Then move on to next key2 range.
          */
          cur_key1->copy_min_to_min(cur_key2);
          key1->merge_flags(cur_key2); //should be cur_key1->merge...() ?
          if (cur_key1->min_flag & NO_MIN_RANGE &&
              cur_key1->max_flag & NO_MAX_RANGE)
          {
            if (key1->maybe_flag)
              return new (param->mem_root) SEL_ARG(SEL_ARG::MAYBE_KEY);
            return 0;
          }
          cur_key2->increment_use_count(-1);        // Free not used tree
          cur_key2=cur_key2->next;
          continue;
        }
        else
        {
          /*
            cur_key2 not adjacent to cur_key1 or has different next_key_part.
            Insert into key1 and move to next range in key2
            
            This is the case:
            cur_key2:   [------**]
            cur_key1:             [----]

            Result:
            key1:       [------**][----]
                        ^         ^
                        insert    cur_key1
          */
          SEL_ARG *next_key2= cur_key2->next;
          if (key2_shared)
          {
            SEL_ARG *cpy= new (param->mem_root) SEL_ARG(*cur_key2);   // Must make copy
            if (!cpy)
              return 0;                         // OOM
            key1= key1->insert(cpy);
            cur_key2->increment_use_count(key1->use_count+1);
          }
          else
            key1= key1->insert(cur_key2); // Will destroy key2_root
          cur_key2= next_key2;
          continue;
        }
      }
    }

    /*
      The ranges in cur_key1 and cur_key2 are overlapping:

      cur_key2:       [----------] 
      cur_key1:    [*****-----*****]

      Corollary: cur_key1.min <= cur_key2.max
    */
    if (eq_tree(cur_key1->next_key_part, cur_key2->next_key_part))
    {
      // Merge overlapping ranges with equal next_key_part
      if (cur_key1->is_same(cur_key2))
      {
        /*
          cur_key1 covers exactly the same range as cur_key2
          Use the relevant range in key1.
        */
        cur_key1->merge_flags(cur_key2);        // Copy maybe flags
        cur_key2->increment_next_key_part_use_count(-1);  // Free not used tree
      }
      else
      {
        SEL_ARG *last= cur_key1;
        SEL_ARG *first= cur_key1;

        /*
          Find the last range in key1 that overlaps cur_key2 and
          where all ranges first...last have the same next_key_part as
          cur_key2.

          cur_key2:  [****----------------------*******]
          key1:         [--]  [----] [---]  [-----] [xxxx]
                        ^                   ^       ^
                        first               last    different next_key_part

          Since cur_key2 covers them, the ranges between first and last
          are merged into one range by deleting first...last-1 from
          the key1 tree. In the figure, this applies to first and the
          two consecutive ranges. The range of last is then extended:
            * last.min: Set to min(cur_key2.min, first.min)
            * last.max: If there is a last->next that overlaps cur_key2 
                        (i.e., last->next has a different next_key_part):
                                        Set adjacent to last->next.min
                        Otherwise:      Set to max(cur_key2.max, last.max)

          Result:
          cur_key2:  [****----------------------*******]
                        [--]  [----] [---]                 => deleted from key1
          key1:      [**------------------------***][xxxx]
                     ^                              ^
                     cur_key1=last                  different next_key_part
        */
        while (last->next && last->next->cmp_min_to_max(cur_key2) <= 0 &&
               eq_tree(last->next->next_key_part, cur_key2->next_key_part))
        {
          /*
            last->next is covered by cur_key2 and has same next_key_part.
            last can be deleted
          */
          SEL_ARG *save=last;
          last=last->next;
          key1= key1->tree_delete(save);
        }
        // Redirect cur_key1 to last which will cover the entire range
        cur_key1= last;

        /*
          Extend last to cover the entire range of
          [min(first.min_value,cur_key2.min_value)...last.max_value].
          If this forms a full range (the range covers all possible
          values) we return no SEL_ARG RB-tree.
        */
        bool full_range= last->copy_min(first);
        if (!full_range)
          full_range= last->copy_min(cur_key2);

        if (!full_range)
        {
          if (last->next && cur_key2->cmp_max_to_min(last->next) >= 0)
          {
            /*
              This is the case:
              cur_key2:   [-------------]
              key1:     [***------]  [xxxx]
                        ^            ^
                        last         different next_key_part

              Extend range of last up to last->next:
              cur_key2:   [-------------]
              key1:     [***--------][xxxx]
            */
            last->copy_min_to_max(last->next);
          }
          else
            /*
              This is the case:
              cur_key2:   [--------*****]
              key1:     [***---------]    [xxxx]
                        ^                 ^
                        last              different next_key_part

              Extend range of last up to max(last.max, cur_key2.max):
              cur_key2:   [--------*****]
              key1:     [***----------**] [xxxx]
             */
            full_range= last->copy_max(cur_key2);
        }
        if (full_range)
        {                                       // Full range
          key1->free_tree();
          key1->type= SEL_ARG::ALWAYS;
          key2->type= SEL_ARG::ALWAYS;
          for (; cur_key2 ; cur_key2= cur_key2->next)
            cur_key2->increment_use_count(-1);  // Free not used tree
          if (key1->maybe_flag)
            return new (param->mem_root) SEL_ARG(SEL_ARG::MAYBE_KEY);
          return 0;
        }
      }
    }

    if (cmp >= 0 && cur_key1->cmp_min_to_min(cur_key2) < 0)
    {
      /*
        This is the case ("cmp>=0" means that cur_key1.max >= cur_key2.min):
        cur_key2:                [-------]
        cur_key1:         [----------*******]
      */

      if (!cur_key1->next_key_part)
      {
        /*
          cur_key1->next_key_part is empty: cut the range that
          is covered by cur_key1 from cur_key2.
          Reason: (cur_key2->next_key_part OR
          cur_key1->next_key_part) will be empty and therefore
          equal to cur_key1->next_key_part. Thus, this part of
          the cur_key2 range is completely covered by cur_key1.
        */
        if (cur_key1->cmp_max_to_max(cur_key2) >= 0)
        {
          /*
            cur_key1 covers the entire range in cur_key2.
            cur_key2:            [-------]
            cur_key1:     [-----------------]

            Move on to next range in key2
          */
          /*
            cur_key2 will no longer be used. Reduce reference count
            of SEL_ARGs in its next_key_part.
          */
          cur_key2->increment_next_key_part_use_count(-1);
          cur_key2= cur_key2->next;
          continue;
        }
        else
        {
          /*
            This is the case:
            cur_key2:            [-------]
            cur_key1:     [---------]

            Result:
            cur_key2:                [---]
            cur_key1:     [---------]
          */
          cur_key2->copy_max_to_min(cur_key1);
          continue;
        }
      }

      /*
        The ranges are overlapping but have not been merged because
        next_key_part of cur_key1 and cur_key2 differ. 
        cur_key2:               [----]
        cur_key1:     [------------*****]

        Split cur_key1 in two where cur_key2 starts:
        cur_key2:               [----]
        key1:         [--------][--*****]
                      ^         ^
                      insert    cur_key1
      */
      SEL_ARG *new_arg= cur_key1->clone_first(cur_key2, param->mem_root);
      if (!new_arg)
        return 0;                               // OOM
      if ((new_arg->next_key_part= cur_key1->next_key_part))
        new_arg->increment_use_count(key1->use_count+1);
      cur_key1->copy_min_to_min(cur_key2);
      key1= key1->insert(new_arg);
    } // cur_key1.min >= cur_key2.min due to this if()

    /*
      Now cur_key2.min <= cur_key1.min <= cur_key2.max:
      cur_key2:    [---------]
      cur_key1:    [****---*****]
     */
    SEL_ARG key2_cpy(*cur_key2); // Get copy we can modify
    for (;;)
    {
      if (cur_key1->cmp_min_to_min(&key2_cpy) > 0)
      {
        /*
          This is the case:
          key2_cpy:    [------------]
          key1:                 [-*****]
                                ^
                                cur_key1
                             
          Result:
          key2_cpy:             [---]
          key1:        [-------][-*****]
                       ^        ^
                       insert   cur_key1
         */
        SEL_ARG *new_arg=key2_cpy.clone_first(cur_key1, param->mem_root);
        if (!new_arg)
          return 0; // OOM
        if ((new_arg->next_key_part=key2_cpy.next_key_part))
          new_arg->increment_use_count(key1->use_count+1);
        key1= key1->insert(new_arg);
        key2_cpy.copy_min_to_min(cur_key1);
      } 
      // Now key2_cpy.min == cur_key1.min

      if ((cmp= cur_key1->cmp_max_to_max(&key2_cpy)) <= 0)
      {
        /*
          cur_key1.max <= key2_cpy.max:
          key2_cpy:       a)  [-------]    or b)     [----]
          cur_key1:           [----]                 [----]

          Steps:

           1) Update next_key_part of cur_key1: OR it with
              key2_cpy->next_key_part.
           2) If case a: Insert range [cur_key1.max, key2_cpy.max] 
              into key1 using next_key_part of key2_cpy

           Result:
           key1:          a)  [----][-]    or b)     [----]
         */
        cur_key1->maybe_flag|= key2_cpy.maybe_flag;
        key2_cpy.increment_use_count(key1->use_count+1);
        cur_key1->next_key_part= 
          key_or(param, cur_key1->next_key_part, key2_cpy.next_key_part);

        if (!cmp)
          break;                     // case b: done with this key2 range

        // Make key2_cpy the range [cur_key1.max, key2_cpy.max]
        key2_cpy.copy_max_to_min(cur_key1);
        if (!(cur_key1= cur_key1->next))
        {
          /*
            No more ranges in key1. Insert key2_cpy and go to "end"
            label to insert remaining ranges in key2 if any.
          */
          SEL_ARG *new_key1_range= new (param->mem_root) SEL_ARG(key2_cpy);
          if (!new_key1_range)
            return 0; // OOM
          key1= key1->insert(new_key1_range);
          cur_key2= cur_key2->next;
          goto end;
        }
        if (cur_key1->cmp_min_to_max(&key2_cpy) > 0)
        {
          /*
            The next range in key1 does not overlap with key2_cpy.
            Insert this range into key1 and move on to the next range
            in key2.
          */
          SEL_ARG *new_key1_range= new (param->mem_root) SEL_ARG(key2_cpy);
          if (!new_key1_range)
            return 0;                           // OOM
          key1= key1->insert(new_key1_range);
          break;
        }
        /*
          key2_cpy overlaps with the next range in key1 and the case
          is now "cur_key2.min <= cur_key1.min <= cur_key2.max". Go back
          to for(;;) to handle this situation.
        */
        continue;
      }
      else
      {
        /*
          This is the case:
          key2_cpy:        [-------]
          cur_key1:        [------------]

          Result:
          key1:            [-------][---]
                           ^        ^
                           new_arg  cur_key1
          Steps:

           0) If cur_key1->next_key_part is empty: do nothing.
              Reason: (key2_cpy->next_key_part OR
              cur_key1->next_key_part) will be empty and
              therefore equal to cur_key1->next_key_part. Thus,
              the range in key2_cpy is completely covered by
              cur_key1
           1) Make new_arg with range [cur_key1.min, key2_cpy.max]. 
              new_arg->next_key_part is OR between next_key_part of 
              cur_key1 and key2_cpy
           2) Make cur_key1 the range [key2_cpy.max, cur_key1.max]
           3) Insert new_arg into key1
        */
        if (!cur_key1->next_key_part) // Step 0
        {
          key2_cpy.increment_use_count(-1);     // Free not used tree
          break;
        }
        SEL_ARG *new_arg= cur_key1->clone_last(&key2_cpy, param->mem_root);
        if (!new_arg)
          return 0; // OOM
        cur_key1->copy_max_to_min(&key2_cpy);
        cur_key1->increment_use_count(key1->use_count+1);
        /* Increment key count as it may be used for next loop */
        key2_cpy.increment_use_count(1);
        new_arg->next_key_part= key_or(param, cur_key1->next_key_part,
                                       key2_cpy.next_key_part);
        key1= key1->insert(new_arg);
        break;
      }
    }
    // Move on to next range in key2
    cur_key2= cur_key2->next;                            
  }

end:
  /*
    Add key2 ranges that are non-overlapping with and higher than the
    highest range in key1.
  */
  while (cur_key2)
  {
    SEL_ARG *next= cur_key2->next;
    if (key2_shared)
    {
      SEL_ARG *key2_cpy=new (param->mem_root) SEL_ARG(*cur_key2);  // Must make copy
      if (!key2_cpy)
        return 0;
      cur_key2->increment_use_count(key1->use_count+1);
      key1= key1->insert(key2_cpy);
    }
    else
      key1= key1->insert(cur_key2);   // Will destroy key2_root
    cur_key2= next;
  }
  key1->use_count++;

  return key1;
}


/* Compare if two trees are equal */

static bool eq_tree(SEL_ARG* a,SEL_ARG *b)
{
  if (a == b)
    return 1;
  if (!a || !b || !a->is_same(b))
    return 0;
  if (a->left != &null_element && b->left != &null_element)
  {
    if (!eq_tree(a->left,b->left))
      return 0;
  }
  else if (a->left != &null_element || b->left != &null_element)
    return 0;
  if (a->right != &null_element && b->right != &null_element)
  {
    if (!eq_tree(a->right,b->right))
      return 0;
  }
  else if (a->right != &null_element || b->right != &null_element)
    return 0;
  if (a->next_key_part != b->next_key_part)
  {						// Sub range
    if (!a->next_key_part != !b->next_key_part ||
	!eq_tree(a->next_key_part, b->next_key_part))
      return 0;
  }
  return 1;
}


SEL_ARG *
SEL_ARG::insert(SEL_ARG *key)
{
  SEL_ARG *element, **par= NULL, *last_element= NULL;

  for (element= this; element != &null_element ; )
  {
    last_element=element;
    if (key->cmp_min_to_min(element) > 0)
    {
      par= &element->right; element= element->right;
    }
    else
    {
      par = &element->left; element= element->left;
    }
  }
  *par=key;
  key->parent=last_element;
	/* Link in list */
  if (par == &last_element->left)
  {
    key->next=last_element;
    if ((key->prev=last_element->prev))
      key->prev->next=key;
    last_element->prev=key;
  }
  else
  {
    if ((key->next=last_element->next))
      key->next->prev=key;
    key->prev=last_element;
    last_element->next=key;
  }
  key->left=key->right= &null_element;
  SEL_ARG *root=rb_insert(key);			// rebalance tree
  root->use_count=this->use_count;		// copy root info
  root->elements= this->elements+1;
  root->maybe_flag=this->maybe_flag;
  return root;
}


/*
** Find best key with min <= given key
** Because the call context this should never return 0 to get_range
*/

SEL_ARG *
SEL_ARG::find_range(SEL_ARG *key)
{
  SEL_ARG *element=this,*found=0;

  for (;;)
  {
    if (element == &null_element)
      return found;
    int cmp=element->cmp_min_to_min(key);
    if (cmp == 0)
      return element;
    if (cmp < 0)
    {
      found=element;
      element=element->right;
    }
    else
      element=element->left;
  }
}


/*
  Remove a element from the tree

  SYNOPSIS
    tree_delete()
    key		Key that is to be deleted from tree (this)

  NOTE
    This also frees all sub trees that is used by the element

  RETURN
    root of new tree (with key deleted)
*/

SEL_ARG *
SEL_ARG::tree_delete(SEL_ARG *key)
{
  enum leaf_color remove_color;
  SEL_ARG *root,*nod,**par,*fix_par;
  DBUG_ENTER("tree_delete");

  root=this;
  this->parent= 0;

  /* Unlink from list */
  if (key->prev)
    key->prev->next=key->next;
  if (key->next)
    key->next->prev=key->prev;
  key->increment_next_key_part_use_count(-1);
  if (!key->parent)
    par= &root;
  else
    par=key->parent_ptr();

  if (key->left == &null_element)
  {
    *par=nod=key->right;
    fix_par=key->parent;
    if (nod != &null_element)
      nod->parent=fix_par;
    remove_color= key->color;
  }
  else if (key->right == &null_element)
  {
    *par= nod=key->left;
    nod->parent=fix_par=key->parent;
    remove_color= key->color;
  }
  else
  {
    SEL_ARG *tmp=key->next;			// next bigger key (exist!)
    nod= *tmp->parent_ptr()= tmp->right;	// unlink tmp from tree
    fix_par=tmp->parent;
    if (nod != &null_element)
      nod->parent=fix_par;
    remove_color= tmp->color;

    tmp->parent=key->parent;			// Move node in place of key
    (tmp->left=key->left)->parent=tmp;
    if ((tmp->right=key->right) != &null_element)
      tmp->right->parent=tmp;
    tmp->color=key->color;
    *par=tmp;
    if (fix_par == key)				// key->right == key->next
      fix_par=tmp;				// new parent of nod
  }

  if (root == &null_element)
    DBUG_RETURN(0);				// Maybe root later
  if (remove_color == BLACK)
    root=rb_delete_fixup(root,nod,fix_par);
#ifndef DBUG_OFF
  test_rb_tree(root,root->parent);
#endif
  root->use_count=this->use_count;		// Fix root counters
  root->elements=this->elements-1;
  root->maybe_flag=this->maybe_flag;
  DBUG_RETURN(root);
}


	/* Functions to fix up the tree after insert and delete */

static void left_rotate(SEL_ARG **root,SEL_ARG *leaf)
{
  SEL_ARG *y=leaf->right;
  leaf->right=y->left;
  if (y->left != &null_element)
    y->left->parent=leaf;
  if (!(y->parent=leaf->parent))
    *root=y;
  else
    *leaf->parent_ptr()=y;
  y->left=leaf;
  leaf->parent=y;
}

static void right_rotate(SEL_ARG **root,SEL_ARG *leaf)
{
  SEL_ARG *y=leaf->left;
  leaf->left=y->right;
  if (y->right != &null_element)
    y->right->parent=leaf;
  if (!(y->parent=leaf->parent))
    *root=y;
  else
    *leaf->parent_ptr()=y;
  y->right=leaf;
  leaf->parent=y;
}


SEL_ARG *
SEL_ARG::rb_insert(SEL_ARG *leaf)
{
  SEL_ARG *y,*par,*par2,*root;
  root= this; root->parent= 0;

  leaf->color=RED;
  while (leaf != root && (par= leaf->parent)->color == RED)
  {					// This can't be root or 1 level under
    if (par == (par2= leaf->parent->parent)->left)
    {
      y= par2->right;
      if (y->color == RED)
      {
	par->color=BLACK;
	y->color=BLACK;
	leaf=par2;
	leaf->color=RED;		/* And the loop continues */
      }
      else
      {
	if (leaf == par->right)
	{
	  left_rotate(&root,leaf->parent);
	  par=leaf;			/* leaf is now parent to old leaf */
	}
	par->color=BLACK;
	par2->color=RED;
	right_rotate(&root,par2);
	break;
      }
    }
    else
    {
      y= par2->left;
      if (y->color == RED)
      {
	par->color=BLACK;
	y->color=BLACK;
	leaf=par2;
	leaf->color=RED;		/* And the loop continues */
      }
      else
      {
	if (leaf == par->left)
	{
	  right_rotate(&root,par);
	  par=leaf;
	}
	par->color=BLACK;
	par2->color=RED;
	left_rotate(&root,par2);
	break;
      }
    }
  }
  root->color=BLACK;
#ifndef DBUG_OFF
  test_rb_tree(root,root->parent);
#endif
  return root;
}


SEL_ARG *rb_delete_fixup(SEL_ARG *root,SEL_ARG *key,SEL_ARG *par)
{
  SEL_ARG *x,*w;
  root->parent=0;

  x= key;
  while (x != root && x->color == SEL_ARG::BLACK)
  {
    if (x == par->left)
    {
      w=par->right;
      if (w->color == SEL_ARG::RED)
      {
	w->color=SEL_ARG::BLACK;
	par->color=SEL_ARG::RED;
	left_rotate(&root,par);
	w=par->right;
      }
      if (w->left->color == SEL_ARG::BLACK && w->right->color == SEL_ARG::BLACK)
      {
	w->color=SEL_ARG::RED;
	x=par;
      }
      else
      {
	if (w->right->color == SEL_ARG::BLACK)
	{
	  w->left->color=SEL_ARG::BLACK;
	  w->color=SEL_ARG::RED;
	  right_rotate(&root,w);
	  w=par->right;
	}
	w->color=par->color;
	par->color=SEL_ARG::BLACK;
	w->right->color=SEL_ARG::BLACK;
	left_rotate(&root,par);
	x=root;
	break;
      }
    }
    else
    {
      w=par->left;
      if (w->color == SEL_ARG::RED)
      {
	w->color=SEL_ARG::BLACK;
	par->color=SEL_ARG::RED;
	right_rotate(&root,par);
	w=par->left;
      }
      if (w->right->color == SEL_ARG::BLACK && w->left->color == SEL_ARG::BLACK)
      {
	w->color=SEL_ARG::RED;
	x=par;
      }
      else
      {
	if (w->left->color == SEL_ARG::BLACK)
	{
	  w->right->color=SEL_ARG::BLACK;
	  w->color=SEL_ARG::RED;
	  left_rotate(&root,w);
	  w=par->left;
	}
	w->color=par->color;
	par->color=SEL_ARG::BLACK;
	w->left->color=SEL_ARG::BLACK;
	right_rotate(&root,par);
	x=root;
	break;
      }
    }
    par=x->parent;
  }
  x->color=SEL_ARG::BLACK;
  return root;
}


#ifndef DBUG_OFF
	/* Test that the properties for a red-black tree hold */

int test_rb_tree(SEL_ARG *element,SEL_ARG *parent)
{
  int count_l,count_r;

  if (element == &null_element)
    return 0;					// Found end of tree
  if (element->parent != parent)
  {
    sql_print_error("Wrong tree: Parent doesn't point at parent");
    return -1;
  }
  if (element->color == SEL_ARG::RED &&
      (element->left->color == SEL_ARG::RED ||
       element->right->color == SEL_ARG::RED))
  {
    sql_print_error("Wrong tree: Found two red in a row");
    return -1;
  }
  if (element->left == element->right && element->left != &null_element)
  {						// Dummy test
    sql_print_error("Wrong tree: Found right == left");
    return -1;
  }
  count_l=test_rb_tree(element->left,element);
  count_r=test_rb_tree(element->right,element);
  if (count_l >= 0 && count_r >= 0)
  {
    if (count_l == count_r)
      return count_l+(element->color == SEL_ARG::BLACK);
    sql_print_error("Wrong tree: Incorrect black-count: %d - %d",
	    count_l,count_r);
  }
  return -1;					// Error, no more warnings
}
#endif


/**
  Count how many times SEL_ARG graph "root" refers to its part "key" via
  transitive closure.
  
  @param root  An RB-Root node in a SEL_ARG graph.
  @param key   Another RB-Root node in that SEL_ARG graph.

  The passed "root" node may refer to "key" node via root->next_key_part,
  root->next->n

  This function counts how many times the node "key" is referred (via
  SEL_ARG::next_key_part) by 
  - intervals of RB-tree pointed by "root", 
  - intervals of RB-trees that are pointed by SEL_ARG::next_key_part from 
  intervals of RB-tree pointed by "root",
  - and so on.
    
  Here is an example (horizontal links represent next_key_part pointers, 
  vertical links - next/prev prev pointers):  
    
         +----+               $
         |root|-----------------+
         +----+               $ |
           |                  $ |
           |                  $ |
         +----+       +---+   $ |     +---+    Here the return value
         |    |- ... -|   |---$-+--+->|key|    will be 4.
         +----+       +---+   $ |  |  +---+
           |                  $ |  |
          ...                 $ |  |
           |                  $ |  |
         +----+   +---+       $ |  |
         |    |---|   |---------+  |
         +----+   +---+       $    |
           |        |         $    |
          ...     +---+       $    |
                  |   |------------+
                  +---+       $
  @return 
  Number of links to "key" from nodes reachable from "root".
*/

static ulong count_key_part_usage(SEL_ARG *root, SEL_ARG *key)
{
  ulong count= 0;
  for (root=root->first(); root ; root=root->next)
  {
    if (root->next_key_part)
    {
      if (root->next_key_part == key)
	count++;
      if (root->next_key_part->part < key->part)
	count+=count_key_part_usage(root->next_key_part,key);
    }
  }
  return count;
}


/*
  Check if SEL_ARG::use_count value is correct

  SYNOPSIS
    SEL_ARG::test_use_count()
      root  The root node of the SEL_ARG graph (an RB-tree root node that
            has the least value of sel_arg->part in the entire graph, and
            thus is the "origin" of the graph)

  DESCRIPTION
    Check if SEL_ARG::use_count value is correct. See the definition of
    use_count for what is "correct".

  RETURN
    true    an incorrect SEL_ARG::use_count is found,
    false   otherwise
*/

bool SEL_ARG::test_use_count(SEL_ARG *root)
{
  uint e_count=0;
  if (this == root && use_count != 1)
  {
    sql_print_information("Use_count: Wrong count %lu for root",use_count);
    // DBUG_ASSERT(false); // Todo - enable and clean up mess
    return true;
  }
  if (this->type != SEL_ARG::KEY_RANGE)
    return false;
  for (SEL_ARG *pos=first(); pos ; pos=pos->next)
  {
    e_count++;
    if (pos->next_key_part)
    {
      ulong count=count_key_part_usage(root,pos->next_key_part);
      if (count > pos->next_key_part->use_count)
      {
        sql_print_information("Use_count: Wrong count for key at 0x%lx, %lu "
                              "should be %lu", (long unsigned int)pos,
                              pos->next_key_part->use_count, count);
        // DBUG_ASSERT(false); // Todo - enable and clean up mess
	return true;
      }
      pos->next_key_part->test_use_count(root);
    }
  }
  if (e_count != elements)
  {
    sql_print_warning("Wrong use count: %u (should be %u) for tree at 0x%lx",
                      e_count, elements, (long unsigned int) this);
    // DBUG_ASSERT(false); // Todo - enable and clean up mess
    return true;
  }
  return false;
}

/****************************************************************************
  MRR Range Sequence Interface implementation that walks a SEL_ARG* tree.
 ****************************************************************************/

/* MRR range sequence, SEL_ARG* implementation: stack entry */
typedef struct st_range_seq_entry 
{
  /* 
    Pointers in min and max keys. They point to right-after-end of key
    images. The 0-th entry has these pointing to key tuple start.
  */
  uchar *min_key, *max_key;
  
  /* 
    Flags, for {keypart0, keypart1, ... this_keypart} subtuple.
    min_key_flag may have NULL_RANGE set.
  */
  uint min_key_flag, max_key_flag;
  enum ha_rkey_function rkey_func_flag;
  /* Number of key parts */
  uint min_key_parts, max_key_parts;
  /**
    Pointer into the R-B tree for this keypart. It points to the
    currently active range for the keypart, so calling next on it will
    get to the next range. sel_arg_range_seq_next() uses this to avoid
    reparsing the R-B range trees each time a new range is fetched.
  */
  SEL_ARG *key_tree;
} RANGE_SEQ_ENTRY;


/*
  MRR range sequence, SEL_ARG* implementation: SEL_ARG graph traversal context
*/
class Sel_arg_range_sequence
{
private:
  
  /**
    Stack of ranges for the curr_kp first keyparts. Used by
    sel_arg_range_seq_next() so that if the next range is equal to the
    previous one for the first x keyparts, stack[x-1] can be
    accumulated with the new range in keyparts > x to quickly form
    the next range to return.

    Notation used below: "x:y" means a range where
    "column_in_keypart_0=x" and "column_in_keypart_1=y". For
    simplicity, only equality (no BETWEEN, < etc) is considered in the
    example but the same principle applies to other range predicate
    operators too.

    Consider a query with these range predicates: 
      (kp0=1 and kp1=2 and kp2=3) or
      (kp0=1 and kp1=2 and kp2=4) or
      (kp0=1 and kp1=3 and kp2=5) or
      (kp0=1 and kp1=3 and kp2=6)

    1) sel_arg_range_seq_next() is called the first time
       - traverse the R-B tree (see SEL_ARG) to find the first range
       - returns range "1:2:3"
       - values in stack after this: stack[1, 1:2, 1:2:3]
    2) sel_arg_range_seq_next() is called second time
       - keypart 2 has another range, so the next range in 
         keypart 2 is appended to stack[1] and saved 
         in stack[2]
       - returns range "1:2:4"
       - values in stack after this: stack[1, 1:2, 1:2:4]
    3) sel_arg_range_seq_next() is called the third time
       - no more ranges in keypart 2, but keypart 1 has 
         another range, so the next range in keypart 1 is
         appended to stack[0] and saved in stack[1]. The first
         range in keypart 2 is then appended to stack[1] and
         saved in stack[2]
       - returns range "1:3:5"
       - values in stack after this: stack[1, 1:3, 1:3:5]
    4) sel_arg_range_seq_next() is called the fourth time
       - keypart 2 has another range, see 2)
       - returns range "1:3:6"
       - values in stack after this: stack[1, 1:3, 1:3:6]
   */
  RANGE_SEQ_ENTRY stack[MAX_REF_PARTS];
  /*
    Index of last used element in the above array. A value of -1 means
    that the stack is empty.
  */
  int curr_kp;

public:
  uint keyno;      /* index of used tree in SEL_TREE structure */
  uint real_keyno; /* Number of the index in tables */
  
  PARAM * const param;
  SEL_ARG *start; /* Root node of the traversed SEL_ARG* graph */

  Sel_arg_range_sequence(PARAM *param_arg) : param(param_arg) { reset(); }

  void reset()
  {
    stack[0].key_tree= NULL;
    stack[0].min_key= (uchar*)param->min_key;
    stack[0].min_key_flag= 0;
    stack[0].min_key_parts= 0;
    stack[0].rkey_func_flag= HA_READ_INVALID;

    stack[0].max_key= (uchar*)param->max_key;
    stack[0].max_key_flag= 0;
    stack[0].max_key_parts= 0;
    curr_kp= -1;
  }  

  bool stack_empty() const { return (curr_kp == -1); }

  void stack_push_range(SEL_ARG *key_tree);

  void stack_pop_range()
  {
    DBUG_ASSERT(!stack_empty());
    if (curr_kp == 0)
      reset();
    else 
      curr_kp--;
  }

  int stack_size() const { return curr_kp + 1; }

  RANGE_SEQ_ENTRY *stack_top()
  {
    return stack_empty() ? NULL : &stack[curr_kp];
  }
};


/*
  Range sequence interface, SEL_ARG* implementation: Initialize the traversal

  SYNOPSIS
    init()
      init_params  SEL_ARG tree traversal context
      n_ranges     [ignored] The number of ranges obtained 
      flags        [ignored] HA_MRR_SINGLE_POINT, HA_MRR_FIXED_KEY

  RETURN
    Value of init_param
*/

range_seq_t sel_arg_range_seq_init(void *init_param, uint n_ranges, uint flags)
{
  Sel_arg_range_sequence *seq= 
    static_cast<Sel_arg_range_sequence*>(init_param);
  seq->reset();
  return init_param;
}


void Sel_arg_range_sequence::stack_push_range(SEL_ARG *key_tree)
{

  DBUG_ASSERT((uint)curr_kp+1 < MAX_REF_PARTS);

  RANGE_SEQ_ENTRY *push_position= &stack[curr_kp + 1];
  RANGE_SEQ_ENTRY *last_added_kp= stack_top();
  if (stack_empty())
  {
    /*
       If we get here this is either 
         a) the first time a range sequence is constructed for this
            range access method (in which case stack[0] has not been
            modified since the constructor was called), or
         b) there are multiple ranges for the first keypart in the
            condition (and we have called stack_pop_range() to empty
            the stack). 
       In both cases, reset() has been called and all fields in
       push_position have been reset. All we need to do is to copy the
       min/max key flags from the predicate we're about to add to
       stack[0].
    */
    push_position->min_key_flag= key_tree->min_flag;
    push_position->max_key_flag= key_tree->max_flag;
    push_position->rkey_func_flag= key_tree->rkey_func_flag;
  }
  else
  {
    push_position->min_key= last_added_kp->min_key;
    push_position->max_key= last_added_kp->max_key;
    push_position->min_key_parts= last_added_kp->min_key_parts;
    push_position->max_key_parts= last_added_kp->max_key_parts;
    push_position->min_key_flag= last_added_kp->min_key_flag |
                                 key_tree->min_flag;
    push_position->max_key_flag= last_added_kp->max_key_flag |
                                 key_tree->max_flag;
    push_position->rkey_func_flag= key_tree->rkey_func_flag;
  }

  push_position->key_tree= key_tree;
  uint16 stor_length= param->key[keyno][key_tree->part].store_length;
  /* psergey-merge-done:
  key_tree->store(arg->param->key[arg->keyno][key_tree->part].store_length,
                  &cur->min_key, prev->min_key_flag,
                  &cur->max_key, prev->max_key_flag);
  */
  push_position->min_key_parts+=
    key_tree->store_min(stor_length, &push_position->min_key,
                        last_added_kp ? last_added_kp->min_key_flag : 0);
  push_position->max_key_parts+=
    key_tree->store_max(stor_length, &push_position->max_key,
                        last_added_kp ? last_added_kp->max_key_flag : 0);

  if (key_tree->is_null_interval())
    push_position->min_key_flag |= NULL_RANGE;
  curr_kp++;
}


/*
  Range sequence interface, SEL_ARG* implementation: get the next interval
  in the R-B tree
  
  SYNOPSIS
    sel_arg_range_seq_next()
      rseq        Value returned from sel_arg_range_seq_init
      range  OUT  Store information about the range here

  DESCRIPTION
    This is "get_next" function for Range sequence interface implementation
    for SEL_ARG* tree.

  IMPLEMENTATION
    The traversal also updates those param members:
      - is_ror_scan
      - range_count
      - max_key_part

  RETURN
    0  Ok
    1  No more ranges in the sequence

  NOTE: append_range_all_keyparts(), which is used to e.g. print
  ranges to Optimizer Trace in a human readable format, mimics the
  behavior of this function.
*/

//psergey-merge-todo: support check_quick_keys:max_keypart
uint sel_arg_range_seq_next(range_seq_t rseq, KEY_MULTI_RANGE *range)
{
  SEL_ARG *key_tree;
  Sel_arg_range_sequence *seq= static_cast<Sel_arg_range_sequence*>(rseq);

  if (seq->stack_empty())
  {
    /*
      This is the first time sel_arg_range_seq_next is called.
      seq->start points to the root of the R-B tree for the first
      keypart
    */
    key_tree= seq->start;

    /*
      Move to the first range for the first keypart. Save this range
      in seq->stack[0] and carry on to ranges in the next keypart if
      any
    */
    key_tree= key_tree->first();
    seq->stack_push_range(key_tree);
  }
  else
  {
    /*
      This is not the first time sel_arg_range_seq_next is called, so
      seq->stack is populated with the range the last call to this
      function found. seq->stack[current_keypart].key_tree points to a
      leaf in the R-B tree of the last keypart that was part of the
      former range. This is the starting point for finding the next
      range. @see Sel_arg_range_sequence::stack
    */
    // See if there are more ranges in this or any of the previous keyparts
    while (true)
    {
      key_tree= seq->stack_top()->key_tree;
      seq->stack_pop_range();
      if (key_tree->next)
      {
        /* This keypart has more ranges */
        DBUG_ASSERT(key_tree->next != &null_element);
        key_tree= key_tree->next;

        /*
          save the next range for this keypart and carry on to ranges in
          the next keypart if any
        */
        seq->stack_push_range(key_tree);
        seq->param->is_ror_scan= FALSE;
        break;
      }

      if (seq->stack_empty())
      {
        // There are no more ranges for the first keypart: we're done
        return 1;
      }
      /* 
         There are no more ranges for the current keypart. Step back
         to the previous keypart and see if there are more ranges
         there.
      */
    }
  }

  DBUG_ASSERT(!seq->stack_empty());

  /*
    Add range info for the next keypart if
      1) there is a range predicate for a later keypart
      2) the range predicate is for the next keypart in the index: a
         range predicate on keypartX+1 can only be used if there is a
         range predicate on keypartX.
      3) the range predicate on the next keypart is usable
  */
  while (key_tree->next_key_part &&                              // 1)
         key_tree->next_key_part != &null_element &&             // 1)
         key_tree->next_key_part->part == key_tree->part + 1 &&  // 2)
         key_tree->next_key_part->type == SEL_ARG::KEY_RANGE)    // 3)
  {
    {
      DBUG_PRINT("info", ("while(): key_tree->part %d",key_tree->part));
      RANGE_SEQ_ENTRY *cur= seq->stack_top();
      const size_t min_key_total_length= cur->min_key - seq->param->min_key;
      const size_t max_key_total_length= cur->max_key - seq->param->max_key;

      /*
        Check if more ranges can be added. This is the case if all
        predicates for keyparts handled so far are equality
        predicates. If either of the following apply, there are
        non-equality predicates in stack[]:

        1) min_key_total_length != max_key_total_length (because
           equality ranges are stored as "min_key = max_key = <value>")
        2) memcmp(<min_key_values>,<max_key_values>) != 0 (same argument as 1)
        3) A min or max flag has been set: Because flags denote ranges
           ('<', '<=' etc), any value but 0 indicates a non-equality
           predicate.
       */

      uchar* min_key_start;
      uchar* max_key_start;
      size_t cur_key_length;

      if (seq->stack_size() == 1)
      {
        min_key_start= seq->param->min_key;
        max_key_start= seq->param->max_key;
        cur_key_length= min_key_total_length;
      }
      else
      {
        const RANGE_SEQ_ENTRY prev= cur[-1];
        min_key_start= prev.min_key;
        max_key_start= prev.max_key;
        cur_key_length= cur->min_key - prev.min_key;
      }

      if ((min_key_total_length != max_key_total_length) ||         // 1)
          (memcmp(min_key_start, max_key_start, cur_key_length)) || // 2)
          (key_tree->min_flag || key_tree->max_flag))               // 3)
      {
        DBUG_PRINT("info", ("while(): inside if()"));
        /*
          The range predicate up to and including the one in key_tree
          is usable by range access but does not allow subranges made
          up from predicates in later keyparts. This may e.g. be
          because the predicate operator is "<". Since there are range
          predicates on more keyparts, we use those to more closely
          specify the start and stop locations for the range. Example:

                "SELECT * FROM t1 WHERE a >= 2 AND b >= 3":

                t1 content:
                -----------
                1 1
                2 1     <- 1)
                2 2
                2 3     <- 2)
                2 4
                3 1
                3 2
                3 3

          The predicate cannot be translated into something like
             "(a=2 and b>=3) or (a=3 and b>=3) or ..."
          I.e., it cannot be divided into subranges, but by storing
          min/max key below we can at least start the scan from 2)
          instead of 1)
        */
        SEL_ARG *store_key_part= key_tree->next_key_part;
        seq->param->is_ror_scan= FALSE;
        if (!key_tree->min_flag)
          cur->min_key_parts += 
            store_key_part->store_min_key(seq->param->key[seq->keyno],
                                          &cur->min_key,
                                          &cur->min_key_flag,
                                          MAX_KEY);
        if (!key_tree->max_flag)
          cur->max_key_parts += 
            store_key_part->store_max_key(seq->param->key[seq->keyno],
                                          &cur->max_key,
                                          &cur->max_key_flag,
                                          MAX_KEY);
        break;
      }
    }
  
    /*
      There are usable range predicates for the next keypart and the
      range predicate for the current keypart allows us to make use of
      them. Move to the first range predicate for the next keypart.
      Push this range predicate to seq->stack and move on to the next
      keypart (if any). @see Sel_arg_range_sequence::stack
    */
    key_tree= key_tree->next_key_part->first();
    seq->stack_push_range(key_tree);
  }

  DBUG_ASSERT(!seq->stack_empty() && (seq->stack_top() != NULL));

  // We now have a full range predicate in seq->stack_top()
  RANGE_SEQ_ENTRY *cur= seq->stack_top();
  PARAM *param= seq->param;
  size_t min_key_length= cur->min_key - param->min_key;

  if (cur->min_key_flag & GEOM_FLAG)
  {
    range->range_flag= cur->min_key_flag;

    /* Here minimum contains also function code bits, and maximum is +inf */
    range->start_key.key=    param->min_key;
    range->start_key.length= min_key_length;
    range->start_key.keypart_map= make_prev_keypart_map(cur->min_key_parts);
    range->start_key.flag= cur->rkey_func_flag;
    /*
      Spatial operators are only allowed on spatial indexes, and no
      spatial index can at the moment return rows in ROWID order
    */
    DBUG_ASSERT(!param->is_ror_scan);
  }
  else
  {
    const KEY *cur_key_info= &param->table->key_info[seq->real_keyno];
    range->range_flag= cur->min_key_flag | cur->max_key_flag;

    range->start_key.key=    param->min_key;
    range->start_key.length= cur->min_key - param->min_key;
    range->start_key.keypart_map= make_prev_keypart_map(cur->min_key_parts);
    range->start_key.flag= (cur->min_key_flag & NEAR_MIN ? HA_READ_AFTER_KEY : 
                                                           HA_READ_KEY_EXACT);

    range->end_key.key=    param->max_key;
    range->end_key.length= cur->max_key - param->max_key;
    range->end_key.keypart_map= make_prev_keypart_map(cur->max_key_parts);
    range->end_key.flag= (cur->max_key_flag & NEAR_MAX ? HA_READ_BEFORE_KEY : 
                                                         HA_READ_AFTER_KEY);

    /* 
      This is an equality range (keypart_0=X and ... and keypart_n=Z) if 
        1) There are no flags indicating open range (e.g., 
           "keypart_x > y") or GIS.
        2) The lower bound and the upper bound of the range has the
           same value (min_key == max_key).
     */
    const uint is_open_range= (NO_MIN_RANGE | NO_MAX_RANGE | 
                               NEAR_MIN | NEAR_MAX | GEOM_FLAG);
    const bool is_eq_range_pred=
      !(cur->min_key_flag & is_open_range) &&                           // 1)
      !(cur->max_key_flag & is_open_range) &&                           // 1)
      range->start_key.length == range->end_key.length &&               // 2)
      !memcmp(param->min_key, param->max_key, range->start_key.length);

    if (is_eq_range_pred)
    {
      range->range_flag= EQ_RANGE;
      /*
        Use statistics instead of index dives for estimates of rows in
        this range if the user requested it
      */
      if (param->use_index_statistics)
        range->range_flag|= USE_INDEX_STATISTICS;

      /* 
        An equality range is a unique range (0 or 1 rows in the range)
        if the index is unique (1) and all keyparts are used (2).
        Note that keys which are extended with PK parts have no
        HA_NOSAME flag. So we can use user_defined_key_parts.
      */
      if (cur_key_info->flags & HA_NOSAME &&                              // 1)
          (uint)key_tree->part+1 == cur_key_info->user_defined_key_parts) // 2)
        range->range_flag|= UNIQUE_RANGE | (cur->min_key_flag & NULL_RANGE);
    }

    if (param->is_ror_scan)
    {
      const uint key_part_number= key_tree->part + 1;
      /*
        If we get here, the condition on the key was converted to form
        "(keyXpart1 = c1) AND ... AND (keyXpart{key_tree->part - 1} = cN) AND
          somecond(keyXpart{key_tree->part})"
        Check if
          somecond is "keyXpart{key_tree->part} = const" and
          uncovered "tail" of KeyX parts is either empty or is identical to
          first members of clustered primary key.

        If last key part is PK part added to the key as an extension
        and is_key_scan_ror() result is TRUE then it's possible to
        use ROR scan.
      */
      if ((!is_eq_range_pred &&
           key_part_number <= cur_key_info->user_defined_key_parts) ||
          !is_key_scan_ror(param, seq->real_keyno, key_part_number))
        param->is_ror_scan= FALSE;
    }
  }

  seq->param->range_count++;
  seq->param->max_key_part=max<uint>(seq->param->max_key_part,key_tree->part);

  return 0;
}


/*
  Calculate estimate of number records that will be retrieved by a range
  scan on given index using given SEL_ARG intervals tree.

  SYNOPSIS
    check_quick_select()
      param             Parameter from test_quick_select
      idx               Number of index to use in PARAM::key SEL_TREE::key
      index_only        TRUE  - assume only index tuples will be accessed
                        FALSE - assume full table rows will be read
      tree              Transformed selection condition, tree->key[idx] holds
                        the intervals for the given index.
      update_tbl_stats  TRUE <=> update table->quick_* with information
                        about range scan we've evaluated.
      mrr_flags   INOUT MRR access flags
      cost        OUT   Scan cost

  NOTES
    param->is_ror_scan is set to reflect if the key scan is a ROR (see
    is_key_scan_ror function for more info)
    param->table->quick_*, param->range_count (and maybe others) are
    updated with data of given key scan, see quick_range_seq_next for details.

  RETURN
    Estimate # of records to be retrieved.
    HA_POS_ERROR if estimate calculation failed due to table handler problems.
*/

static
ha_rows check_quick_select(PARAM *param, uint idx, bool index_only,
                           SEL_ARG *tree, bool update_tbl_stats, 
                           uint *mrr_flags, uint *bufsize, Cost_estimate *cost)
{
  Sel_arg_range_sequence seq(param);
  RANGE_SEQ_IF seq_if = {sel_arg_range_seq_init, sel_arg_range_seq_next, 0, 0};
  handler *file= param->table->file;
  ha_rows rows;
  uint keynr= param->real_keynr[idx];
  DBUG_ENTER("check_quick_select");
  
  /* Handle cases when we don't have a valid non-empty list of range */
  if (!tree)
    DBUG_RETURN(HA_POS_ERROR);
  if (tree->type == SEL_ARG::IMPOSSIBLE)
    DBUG_RETURN(0L);
  if (tree->type != SEL_ARG::KEY_RANGE || tree->part != 0)
    DBUG_RETURN(HA_POS_ERROR);				// Don't use tree

  seq.keyno= idx;
  seq.real_keyno= keynr;
  seq.start= tree;

  param->range_count=0;
  param->max_key_part=0;

  /* 
    If there are more equality ranges than specified by the
    eq_range_index_dive_limit variable we switches from using index
    dives to use statistics.
  */ 
  uint range_count= 0;
  param->use_index_statistics= 
    eq_ranges_exceeds_limit(tree, &range_count, 
                            param->thd->variables.eq_range_index_dive_limit);

  param->is_ror_scan= TRUE;
  if (file->index_flags(keynr, 0, TRUE) & HA_KEY_SCAN_NOT_ROR)
    param->is_ror_scan= FALSE;
  
  *mrr_flags= param->force_default_mrr? HA_MRR_USE_DEFAULT_IMPL: 0;
  *mrr_flags|= HA_MRR_NO_ASSOCIATION;
  /*
    Pass HA_MRR_SORTED to see if MRR implementation can handle sorting.
  */
  if (param->order_direction != ORDER::ORDER_NOT_RELEVANT)
    *mrr_flags|= HA_MRR_SORTED;

  bool pk_is_clustered= file->primary_key_is_clustered();
  if (index_only && 
      (file->index_flags(keynr, param->max_key_part, 1) & HA_KEYREAD_ONLY) &&
      !(pk_is_clustered && keynr == param->table->s->primary_key))
     *mrr_flags |= HA_MRR_INDEX_ONLY;
  
  if (current_thd->lex->sql_command != SQLCOM_SELECT)
    *mrr_flags|= HA_MRR_SORTED; // Assumed to give faster ins/upd/del

  *bufsize= param->thd->variables.read_rnd_buff_size;
  // Sets is_ror_scan to false for some queries, e.g. multi-ranges
  rows= file->multi_range_read_info_const(keynr, &seq_if, (void*)&seq, 0,
                                          bufsize, mrr_flags, cost);
  if (rows != HA_POS_ERROR)
  {
    param->table->quick_rows[keynr]=rows;
    if (update_tbl_stats)
    {
      param->table->quick_keys.set_bit(keynr);
      param->table->quick_key_parts[keynr]=param->max_key_part+1;
      param->table->quick_n_ranges[keynr]= param->range_count;
      param->table->quick_condition_rows=
        min(param->table->quick_condition_rows, rows);
    }
    param->table->possible_quick_keys.set_bit(keynr);
  }
  /* Figure out if the key scan is ROR (returns rows in ROWID order) or not */
  enum ha_key_alg key_alg= param->table->key_info[seq.real_keyno].algorithm;
  if ((key_alg != HA_KEY_ALG_BTREE) && (key_alg!= HA_KEY_ALG_UNDEF))
  {
    /* 
      All scans are non-ROR scans for those index types.
      TODO: Don't have this logic here, make table engines return 
      appropriate flags instead.
    */
    param->is_ror_scan= FALSE;
  }
  else
  {
    /* Clustered PK scan is always a ROR scan (TODO: same as above) */
    if (param->table->s->primary_key == keynr && pk_is_clustered)
      param->is_ror_scan= TRUE;
  }
  if (param->table->file->index_flags(keynr, 0, TRUE) & HA_KEY_SCAN_NOT_ROR)
    param->is_ror_scan= FALSE;

  /*
    QUICK_ROR_INTERSECT_SELECT and QUICK_ROR_UNION_SELECT do read_set
    manipulations in reset(), which breaks virtual generated column's
    computation logic, which is used when reading index values.
    So, disable index merge intersection/union for any index on such column.
    @todo lift this implementation restriction
  */
  if (param->table->index_contains_some_virtual_gcol(keynr))
    param->is_ror_scan= false;

  DBUG_PRINT("exit", ("Records: %lu", (ulong) rows));
  DBUG_RETURN(rows);
}


/*
  Check if key scan on given index with equality conditions on first n key
  parts is a ROR scan.

  SYNOPSIS
    is_key_scan_ror()
      param  Parameter from test_quick_select
      keynr  Number of key in the table. The key must not be a clustered
             primary key.
      nparts Number of first key parts for which equality conditions
             are present.

  NOTES
    ROR (Rowid Ordered Retrieval) key scan is a key scan that produces
    ordered sequence of rowids (ha_xxx::cmp_ref is the comparison function)

    This function is needed to handle a practically-important special case:
    an index scan is a ROR scan if it is done using a condition in form

        "key1_1=c_1 AND ... AND key1_n=c_n"

    where the index is defined on (key1_1, ..., key1_N [,a_1, ..., a_n])

    and the table has a clustered Primary Key defined as 

      PRIMARY KEY(a_1, ..., a_n, b1, ..., b_k) 
    
    i.e. the first key parts of it are identical to uncovered parts ot the 
    key being scanned. This function assumes that the index flags do not
    include HA_KEY_SCAN_NOT_ROR flag (that is checked elsewhere).

    Check (1) is made in quick_range_seq_next()

  RETURN
    TRUE   The scan is ROR-scan
    FALSE  Otherwise
*/

static bool is_key_scan_ror(PARAM *param, uint keynr, uint nparts)
{
  KEY *table_key= param->table->key_info + keynr;

  /*
    Range predicates on hidden key parts do not change the fact
    that a scan is rowid ordered, so we only care about user
    defined keyparts
  */
  const uint user_defined_nparts=
    std::min<uint>(nparts, table_key->user_defined_key_parts);

  KEY_PART_INFO *key_part= table_key->key_part + user_defined_nparts;
  KEY_PART_INFO *key_part_end= (table_key->key_part +
                                table_key->user_defined_key_parts);
  uint pk_number;
  
  for (KEY_PART_INFO *kp= table_key->key_part; kp < key_part; kp++)
  {
    uint16 fieldnr= param->table->key_info[keynr].
                    key_part[kp - table_key->key_part].fieldnr - 1;
    if (param->table->field[fieldnr]->key_length() != kp->length)
      return FALSE;
  }

  if (key_part == key_part_end)
    return TRUE;

  key_part= table_key->key_part + user_defined_nparts;
  pk_number= param->table->s->primary_key;
  if (!param->table->file->primary_key_is_clustered() || pk_number == MAX_KEY)
    return FALSE;

  KEY_PART_INFO *pk_part= param->table->key_info[pk_number].key_part;
  KEY_PART_INFO *pk_part_end=
    pk_part + param->table->key_info[pk_number].user_defined_key_parts;
  for (;(key_part!=key_part_end) && (pk_part != pk_part_end);
       ++key_part, ++pk_part)
  {
    if ((key_part->field != pk_part->field) ||
        (key_part->length != pk_part->length))
      return FALSE;
  }
  return (key_part == key_part_end);
}


/*
  Create a QUICK_RANGE_SELECT from given key and SEL_ARG tree for that key.

  SYNOPSIS
    get_quick_select()
      param
      idx            Index of used key in param->key.
      key_tree       SEL_ARG tree for the used key
      mrr_flags      MRR parameter for quick select
      mrr_buf_size   MRR parameter for quick select
      parent_alloc   If not NULL, use it to allocate memory for
                     quick select data. Otherwise use quick->alloc.
  NOTES
    The caller must call QUICK_SELECT::init for returned quick select.

    CAUTION! This function may change thd->mem_root to a MEM_ROOT which will be
    deallocated when the returned quick select is deleted.

  RETURN
    NULL on error
    otherwise created quick select
*/

QUICK_RANGE_SELECT *
get_quick_select(PARAM *param,uint idx,SEL_ARG *key_tree, uint mrr_flags,
                 uint mrr_buf_size, MEM_ROOT *parent_alloc)
{
  QUICK_RANGE_SELECT *quick;
  bool create_err= FALSE;
  DBUG_ENTER("get_quick_select");

  if (param->table->key_info[param->real_keynr[idx]].flags & HA_SPATIAL)
    quick=new QUICK_RANGE_SELECT_GEOM(param->thd, param->table,
                                      param->real_keynr[idx],
                                      MY_TEST(parent_alloc),
                                      parent_alloc, &create_err);
  else
    quick=new QUICK_RANGE_SELECT(param->thd, param->table,
                                 param->real_keynr[idx],
                                 MY_TEST(parent_alloc), NULL, &create_err);

  if (quick)
  {
    if (create_err ||
	get_quick_keys(param,quick,param->key[idx],key_tree,param->min_key,0,
		       param->max_key,0))
    {
      delete quick;
      quick=0;
    }
    else
    {
      quick->mrr_flags= mrr_flags;
      quick->mrr_buf_size= mrr_buf_size;
      quick->key_parts=(KEY_PART*)
        memdup_root(parent_alloc? parent_alloc : &quick->alloc,
                    (char*) param->key[idx],
                    sizeof(KEY_PART) *
                    actual_key_parts(&param->
                                     table->key_info[param->real_keynr[idx]]));
    }
  }
  DBUG_RETURN(quick);
}


/*
** Fix this to get all possible sub_ranges
*/
bool
get_quick_keys(PARAM *param,QUICK_RANGE_SELECT *quick,KEY_PART *key,
	       SEL_ARG *key_tree, uchar *min_key,uint min_key_flag,
	       uchar *max_key, uint max_key_flag)
{
  QUICK_RANGE *range;
  uint flag;
  int min_part= key_tree->part-1, // # of keypart values in min_key buffer
      max_part= key_tree->part-1; // # of keypart values in max_key buffer

  if (key_tree->left != &null_element)
  {
    if (get_quick_keys(param,quick,key,key_tree->left,
		       min_key,min_key_flag, max_key, max_key_flag))
      return 1;
  }
  uchar *tmp_min_key=min_key,*tmp_max_key=max_key;
  min_part+= key_tree->store_min(key[key_tree->part].store_length,
                                 &tmp_min_key,min_key_flag);
  max_part+= key_tree->store_max(key[key_tree->part].store_length,
                                 &tmp_max_key,max_key_flag);

  if (key_tree->next_key_part &&
      key_tree->next_key_part->type == SEL_ARG::KEY_RANGE &&
      key_tree->next_key_part->part == key_tree->part+1)
  {						  // const key as prefix
    if ((tmp_min_key - min_key) == (tmp_max_key - max_key) &&
         memcmp(min_key, max_key, (uint)(tmp_max_key - max_key))==0 &&
	 key_tree->min_flag==0 && key_tree->max_flag==0)
    {
      if (get_quick_keys(param,quick,key,key_tree->next_key_part,
			 tmp_min_key, min_key_flag | key_tree->min_flag,
			 tmp_max_key, max_key_flag | key_tree->max_flag))
	return 1;
      goto end;					// Ugly, but efficient
    }
    {
      uint tmp_min_flag=key_tree->min_flag,tmp_max_flag=key_tree->max_flag;
      if (!tmp_min_flag)
        min_part+= key_tree->next_key_part->store_min_key(key,
                                                          &tmp_min_key,
                                                          &tmp_min_flag,
                                                          MAX_KEY);
      if (!tmp_max_flag)
        max_part+= key_tree->next_key_part->store_max_key(key,
                                                          &tmp_max_key,
                                                          &tmp_max_flag,
                                                          MAX_KEY);
      flag=tmp_min_flag | tmp_max_flag;
    }
  }
  else
  {
    flag = (key_tree->min_flag & GEOM_FLAG) ?
      key_tree->min_flag : key_tree->min_flag | key_tree->max_flag;
  }

  /*
    Ensure that some part of min_key and max_key are used.  If not,
    regard this as no lower/upper range
  */
  if ((flag & GEOM_FLAG) == 0)
  {
    if (tmp_min_key != param->min_key)
      flag&= ~NO_MIN_RANGE;
    else
      flag|= NO_MIN_RANGE;
    if (tmp_max_key != param->max_key)
      flag&= ~NO_MAX_RANGE;
    else
      flag|= NO_MAX_RANGE;
  }
  if (flag == 0)
  {
    uint length= (uint) (tmp_min_key - param->min_key);
    if (length == (uint) (tmp_max_key - param->max_key) &&
	!memcmp(param->min_key,param->max_key,length))
    {
      const KEY *table_key=quick->head->key_info+quick->index;
      flag=EQ_RANGE;
      /*
        Note that keys which are extended with PK parts have no
        HA_NOSAME flag. So we can use user_defined_key_parts.
      */
      if ((table_key->flags & HA_NOSAME) &&
          key_tree->part == table_key->user_defined_key_parts - 1)
      {
        if ((table_key->flags & HA_NULL_PART_KEY) &&
            null_part_in_key(key,
                             param->min_key,
                             (uint) (tmp_min_key - param->min_key)))
          flag|= NULL_RANGE;
        else
          flag|= UNIQUE_RANGE;
      }
    }
  }

  /* Get range for retrieving rows in QUICK_SELECT::get_next */
  if (!(range= new QUICK_RANGE(param->min_key,
			       (uint) (tmp_min_key - param->min_key),
                               min_part >=0 ? make_keypart_map(min_part) : 0,
			       param->max_key,
			       (uint) (tmp_max_key - param->max_key),
                               max_part >=0 ? make_keypart_map(max_part) : 0,
			       flag, key_tree->rkey_func_flag)))
    return 1;			// out of memory

  set_if_bigger(quick->max_used_key_length, range->min_length);
  set_if_bigger(quick->max_used_key_length, range->max_length);
  set_if_bigger(quick->used_key_parts, (uint) key_tree->part+1);
  if (quick->ranges.push_back(range))
    return 1;

 end:
  if (key_tree->right != &null_element)
    return get_quick_keys(param,quick,key,key_tree->right,
			  min_key,min_key_flag,
			  max_key,max_key_flag);
  return 0;
}

/*
  Return 1 if there is only one range and this uses the whole unique key
*/

bool QUICK_RANGE_SELECT::unique_key_range()
{
  if (ranges.size() == 1)
  {
    QUICK_RANGE *tmp= ranges[0];
    if ((tmp->flag & (EQ_RANGE | NULL_RANGE)) == EQ_RANGE)
    {
      KEY *key=head->key_info+index;
      return (key->flags & HA_NOSAME) && key->key_length == tmp->min_length;
    }
  }
  return 0;
}



/*
  Return TRUE if any part of the key is NULL

  SYNOPSIS
    null_part_in_key()    
      key_part  Array of key parts (index description)
      key       Key values tuple
      length    Length of key values tuple in bytes.

  RETURN
    TRUE   The tuple has at least one "keypartX is NULL"
    FALSE  Otherwise
*/

static bool null_part_in_key(KEY_PART *key_part, const uchar *key, uint length)
{
  for (const uchar *end=key+length ;
       key < end;
       key+= key_part++->store_length)
  {
    if (key_part->null_bit && *key)
      return 1;
  }
  return 0;
}


bool QUICK_SELECT_I::is_keys_used(const MY_BITMAP *fields)
{
  return is_key_used(head, index, fields);
}

bool QUICK_INDEX_MERGE_SELECT::is_keys_used(const MY_BITMAP *fields)
{
  QUICK_RANGE_SELECT *quick;
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  while ((quick= it++))
  {
    if (is_key_used(head, quick->index, fields))
      return 1;
  }
  return 0;
}

bool QUICK_ROR_INTERSECT_SELECT::is_keys_used(const MY_BITMAP *fields)
{
  QUICK_RANGE_SELECT *quick;
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  while ((quick= it++))
  {
    if (is_key_used(head, quick->index, fields))
      return 1;
  }
  return 0;
}

bool QUICK_ROR_UNION_SELECT::is_keys_used(const MY_BITMAP *fields)
{
  QUICK_SELECT_I *quick;
  List_iterator_fast<QUICK_SELECT_I> it(quick_selects);
  while ((quick= it++))
  {
    if (quick->is_keys_used(fields))
      return 1;
  }
  return 0;
}


FT_SELECT *get_ft_select(THD *thd, TABLE *table, uint key)
{
  bool create_err= FALSE;
  FT_SELECT *fts= new FT_SELECT(thd, table, key, &create_err);
  if (create_err)
  {
    delete fts;
    return NULL;
  }
  else
    return fts;
}


/*
  Check if any columns in the key value specified
  by 'key_info' has a NULL-value.
*/

static bool
key_has_nulls(const KEY* key_info, const uchar *key, uint key_len)
{
  KEY_PART_INFO *curr_part, *end_part;
  const uchar* end_ptr= key + key_len;
  curr_part= key_info->key_part;
  end_part= curr_part + key_info->user_defined_key_parts;

  for (; curr_part != end_part && key < end_ptr; curr_part++)
  {
    if (curr_part->null_bit && *key)
      return TRUE;

    key += curr_part->store_length;
  }
  return FALSE;
}

/*
  Create quick select from ref/ref_or_null scan.

  SYNOPSIS
    get_quick_select_for_ref()
      thd      Thread handle
      table    Table to access
      ref      ref[_or_null] scan parameters
      records  Estimate of number of records (needed only to construct
               quick select)
  NOTES
    This allocates things in a new memory root, as this may be called many
    times during a query.

  RETURN
    Quick select that retrieves the same rows as passed ref scan
    NULL on error.
*/

QUICK_RANGE_SELECT *get_quick_select_for_ref(THD *thd, TABLE *table,
                                             TABLE_REF *ref, ha_rows records)
{
  MEM_ROOT *old_root, *alloc;
  QUICK_RANGE_SELECT *quick;
  KEY *key_info = &table->key_info[ref->key];
  KEY_PART *key_part;
  QUICK_RANGE *range;
  uint part;
  bool create_err= FALSE;
  Cost_estimate cost;

  old_root= thd->mem_root;
  /* The following call may change thd->mem_root */
  quick= new QUICK_RANGE_SELECT(thd, table, ref->key, 0, 0, &create_err);
  /* save mem_root set by QUICK_RANGE_SELECT constructor */
  alloc= thd->mem_root;
  /*
    return back default mem_root (thd->mem_root) changed by
    QUICK_RANGE_SELECT constructor
  */
  thd->mem_root= old_root;

  if (!quick || create_err)
    return 0;			/* no ranges found */
  if (quick->init())
    goto err;
  quick->records= records;

  if (!(range= new (alloc) QUICK_RANGE()))
    goto err;                                   // out of memory

  range->min_key= range->max_key= ref->key_buff;
  range->min_length= range->max_length= ref->key_length;
  range->min_keypart_map= range->max_keypart_map=
    make_prev_keypart_map(ref->key_parts);
  range->flag= (ref->key_length == key_info->key_length ? EQ_RANGE : 0);

  if (!(quick->key_parts=key_part=(KEY_PART *)
	alloc_root(&quick->alloc,sizeof(KEY_PART)*ref->key_parts)))
    goto err;

  for (part=0 ; part < ref->key_parts ;part++,key_part++)
  {
    key_part->part=part;
    key_part->field=        key_info->key_part[part].field;
    key_part->length=       key_info->key_part[part].length;
    key_part->store_length= key_info->key_part[part].store_length;
    key_part->null_bit=     key_info->key_part[part].null_bit;
    key_part->flag=         (uint8) key_info->key_part[part].key_part_flag;
  }
  if (quick->ranges.push_back(range))
    goto err;

  /*
     Add a NULL range if REF_OR_NULL optimization is used.
     For example:
       if we have "WHERE A=2 OR A IS NULL" we created the (A=2) range above
       and have ref->null_ref_key set. Will create a new NULL range here.
  */
  if (ref->null_ref_key)
  {
    QUICK_RANGE *null_range;

    *ref->null_ref_key= 1;		// Set null byte then create a range
    if (!(null_range= new (alloc)
          QUICK_RANGE(ref->key_buff, ref->key_length,
                      make_prev_keypart_map(ref->key_parts),
                      ref->key_buff, ref->key_length,
                      make_prev_keypart_map(ref->key_parts), EQ_RANGE,
                      HA_READ_INVALID)))
      goto err;
    *ref->null_ref_key= 0;		// Clear null byte
    if (quick->ranges.push_back(null_range))
      goto err;
  }

  /* Call multi_range_read_info() to get the MRR flags and buffer size */
  quick->mrr_flags= HA_MRR_NO_ASSOCIATION | 
                    (table->key_read ? HA_MRR_INDEX_ONLY : 0);
  if (thd->lex->sql_command != SQLCOM_SELECT)
    quick->mrr_flags|= HA_MRR_SORTED; // Assumed to give faster ins/upd/del
  if (!ref->null_ref_key && !key_has_nulls(key_info, range->min_key,
                                           ref->key_length))
    quick->mrr_flags |= HA_MRR_NO_NULL_ENDPOINTS;

  quick->mrr_buf_size= thd->variables.read_rnd_buff_size;
  if (table->file->multi_range_read_info(quick->index, 1,
                                         static_cast<uint>(records),
                                         &quick->mrr_buf_size,
                                         &quick->mrr_flags, &cost))
    goto err;

  return quick;
err:
  delete quick;
  return 0;
}


/*
  Perform key scans for all used indexes (except CPK), get rowids and merge 
  them into an ordered non-recurrent sequence of rowids.
  
  The merge/duplicate removal is performed using Unique class. We put all
  rowids into Unique, get the sorted sequence and destroy the Unique.
  
  If table has a clustered primary key that covers all rows (TRUE for bdb
  and innodb currently) and one of the index_merge scans is a scan on PK,
  then rows that will be retrieved by PK scan are not put into Unique and 
  primary key scan is not performed here, it is performed later separately.

  RETURN
    0     OK
    other error
*/

int QUICK_INDEX_MERGE_SELECT::read_keys_and_merge()
{
  List_iterator_fast<QUICK_RANGE_SELECT> cur_quick_it(quick_selects);
  QUICK_RANGE_SELECT* cur_quick;
  int result;
  handler *file= head->file;
  DBUG_ENTER("QUICK_INDEX_MERGE_SELECT::read_keys_and_merge");

  /* We're going to just read rowids. */
  head->set_keyread(TRUE);
  head->prepare_for_position();

  cur_quick_it.rewind();
  cur_quick= cur_quick_it++;
  DBUG_ASSERT(cur_quick != 0);
  
  DBUG_EXECUTE_IF("simulate_bug13919180",
                  {
                    my_error(ER_UNKNOWN_ERROR, MYF(0));
                    DBUG_RETURN(1);
                  });
  /*
    We reuse the same instance of handler so we need to call both init and 
    reset here.
  */
  if (cur_quick->init() || cur_quick->reset())
    DBUG_RETURN(1);

  if (unique == NULL)
  {
    DBUG_EXECUTE_IF("index_merge_may_not_create_a_Unique", DBUG_ABORT(); );
    DBUG_EXECUTE_IF("only_one_Unique_may_be_created", 
                    DBUG_SET("+d,index_merge_may_not_create_a_Unique"); );

    unique= new Unique(refpos_order_cmp, (void *)file,
                       file->ref_length,
                       thd->variables.sortbuff_size);
  }
  else
  {
    unique->reset();
    filesort_free_buffers(head, false);
  }

  DBUG_ASSERT(file->ref_length == unique->get_size());
  DBUG_ASSERT(thd->variables.sortbuff_size == unique->get_max_in_memory_size());

  if (!unique)
    DBUG_RETURN(1);
  for (;;)
  {
    while ((result= cur_quick->get_next()) == HA_ERR_END_OF_FILE)
    {
      cur_quick->range_end();
      cur_quick= cur_quick_it++;
      if (!cur_quick)
        break;

      if (cur_quick->file->inited) 
        cur_quick->file->ha_index_end();
      if (cur_quick->init() || cur_quick->reset())
        DBUG_RETURN(1);
    }

    if (result)
    {
      if (result != HA_ERR_END_OF_FILE)
      {
        cur_quick->range_end();
        DBUG_RETURN(result);
      }
      break;
    }

    if (thd->killed)
      DBUG_RETURN(1);

    /* skip row if it will be retrieved by clustered PK scan */
    if (pk_quick_select && pk_quick_select->row_in_ranges())
      continue;

    cur_quick->file->position(cur_quick->record);
    result= unique->unique_add((char*)cur_quick->file->ref);
    if (result)
      DBUG_RETURN(1);
  }

  /*
    Ok all rowids are in the Unique now. The next call will initialize
    head->sort structure so it can be used to iterate through the rowids
    sequence.
  */
  result= unique->get(head);
  doing_pk_scan= FALSE;
  /* index_merge currently doesn't support "using index" at all */
  head->set_keyread(FALSE);
  if (init_read_record(&read_record, thd, head, NULL, 1, 1, TRUE))
    DBUG_RETURN(1);
  DBUG_RETURN(result);
}


/*
  Get next row for index_merge.
  NOTES
    The rows are read from
      1. rowids stored in Unique.
      2. QUICK_RANGE_SELECT with clustered primary key (if any).
    The sets of rows retrieved in 1) and 2) are guaranteed to be disjoint.
*/

int QUICK_INDEX_MERGE_SELECT::get_next()
{
  int result;
  DBUG_ENTER("QUICK_INDEX_MERGE_SELECT::get_next");

  if (doing_pk_scan)
    DBUG_RETURN(pk_quick_select->get_next());

  if ((result= read_record.read_record(&read_record)) == -1)
  {
    result= HA_ERR_END_OF_FILE;
    end_read_record(&read_record);
    free_io_cache(head);
    /* All rows from Unique have been retrieved, do a clustered PK scan */
    if (pk_quick_select)
    {
      doing_pk_scan= TRUE;
      if ((result= pk_quick_select->init()) ||
          (result= pk_quick_select->reset()))
        DBUG_RETURN(result);
      DBUG_RETURN(pk_quick_select->get_next());
    }
  }

  DBUG_RETURN(result);
}


/*
  Retrieve next record.
  SYNOPSIS
     QUICK_ROR_INTERSECT_SELECT::get_next()

  NOTES
    Invariant on enter/exit: all intersected selects have retrieved all index
    records with rowid <= some_rowid_val and no intersected select has
    retrieved any index records with rowid > some_rowid_val.
    We start fresh and loop until we have retrieved the same rowid in each of
    the key scans or we got an error.

    If a Clustered PK scan is present, it is used only to check if row
    satisfies its condition (and never used for row retrieval).

    Locking: to ensure that exclusive locks are only set on records that
    are included in the final result we must release the lock
    on all rows we read but do not include in the final result. This
    must be done on each index that reads the record and the lock
    must be released using the same handler (the same quick object) as
    used when reading the record.

  RETURN
   0     - Ok
   other - Error code if any error occurred.
*/

int QUICK_ROR_INTERSECT_SELECT::get_next()
{
  List_iterator_fast<QUICK_RANGE_SELECT> quick_it(quick_selects);
  QUICK_RANGE_SELECT* quick;

  /* quick that reads the given rowid first. This is needed in order
  to be able to unlock the row using the same handler object that locked
  it */
  QUICK_RANGE_SELECT* quick_with_last_rowid;

  int error, cmp;
  uint last_rowid_count=0;
  DBUG_ENTER("QUICK_ROR_INTERSECT_SELECT::get_next");

  do
  {
    /* Get a rowid for first quick and save it as a 'candidate' */
    quick= quick_it++;
    error= quick->get_next();
    if (cpk_quick)
    {
      while (!error && !cpk_quick->row_in_ranges())
      {
        quick->file->unlock_row(); /* row not in range; unlock */
        error= quick->get_next();
      }
    }
    if (error)
      DBUG_RETURN(error);

    quick->file->position(quick->record);
    memcpy(last_rowid, quick->file->ref, head->file->ref_length);
    last_rowid_count= 1;
    quick_with_last_rowid= quick;

    while (last_rowid_count < quick_selects.elements)
    {
      if (!(quick= quick_it++))
      {
        quick_it.rewind();
        quick= quick_it++;
      }

      do
      {
        DBUG_EXECUTE_IF("innodb_quick_report_deadlock",
                        DBUG_SET("+d,innodb_report_deadlock"););
        if ((error= quick->get_next()))
        {
          /* On certain errors like deadlock, trx might be rolled back.*/
          if (!current_thd->transaction_rollback_request)
            quick_with_last_rowid->file->unlock_row();
          DBUG_RETURN(error);
        }
        quick->file->position(quick->record);
        cmp= head->file->cmp_ref(quick->file->ref, last_rowid);
        if (cmp < 0)
        {
          /* This row is being skipped.  Release lock on it. */
          quick->file->unlock_row();
        }
      } while (cmp < 0);

      /* Ok, current select 'caught up' and returned ref >= cur_ref */
      if (cmp > 0)
      {
        /* Found a row with ref > cur_ref. Make it a new 'candidate' */
        if (cpk_quick)
        {
          while (!cpk_quick->row_in_ranges())
          {
            quick->file->unlock_row(); /* row not in range; unlock */
            if ((error= quick->get_next()))
            {
              /* On certain errors like deadlock, trx might be rolled back.*/
              if (!current_thd->transaction_rollback_request)
                quick_with_last_rowid->file->unlock_row();
              DBUG_RETURN(error);
            }
          }
          quick->file->position(quick->record);
        }
        memcpy(last_rowid, quick->file->ref, head->file->ref_length);
        quick_with_last_rowid->file->unlock_row();
        last_rowid_count= 1;
        quick_with_last_rowid= quick;
      }
      else
      {
        /* current 'candidate' row confirmed by this select */
        last_rowid_count++;
      }
    }

    /* We get here if we got the same row ref in all scans. */
    if (need_to_fetch_row)
      error= head->file->ha_rnd_pos(head->record[0], last_rowid);
  } while (error == HA_ERR_RECORD_DELETED);
  DBUG_RETURN(error);
}


/*
  Retrieve next record.
  SYNOPSIS
    QUICK_ROR_UNION_SELECT::get_next()

  NOTES
    Enter/exit invariant:
    For each quick select in the queue a {key,rowid} tuple has been
    retrieved but the corresponding row hasn't been passed to output.

  RETURN
   0     - Ok
   other - Error code if any error occurred.
*/

int QUICK_ROR_UNION_SELECT::get_next()
{
  int error, dup_row;
  QUICK_SELECT_I *quick;
  uchar *tmp;
  DBUG_ENTER("QUICK_ROR_UNION_SELECT::get_next");

  do
  {
    do
    {
      if (queue.empty())
        DBUG_RETURN(HA_ERR_END_OF_FILE);
      /* Ok, we have a queue with >= 1 scans */

      quick= queue.top();
      memcpy(cur_rowid, quick->last_rowid, rowid_length);

      /* put into queue rowid from the same stream as top element */
      if ((error= quick->get_next()))
      {
        if (error != HA_ERR_END_OF_FILE)
          DBUG_RETURN(error);
        queue.pop();
      }
      else
      {
        quick->save_last_pos();
        queue.update_top();
      }

      if (!have_prev_rowid)
      {
        /* No rows have been returned yet */
        dup_row= FALSE;
        have_prev_rowid= TRUE;
      }
      else
        dup_row= !head->file->cmp_ref(cur_rowid, prev_rowid);
    } while (dup_row);

    tmp= cur_rowid;
    cur_rowid= prev_rowid;
    prev_rowid= tmp;

    error= head->file->ha_rnd_pos(quick->record, prev_rowid);
  } while (error == HA_ERR_RECORD_DELETED);
  DBUG_RETURN(error);
}


int QUICK_RANGE_SELECT::reset()
{
  uint  buf_size;
  uchar *mrange_buff;
  int   error;
  HANDLER_BUFFER empty_buf;
  DBUG_ENTER("QUICK_RANGE_SELECT::reset");
  last_range= NULL;
  cur_range= ranges.begin();

  /* set keyread to TRUE if index is covering */
  if(!head->no_keyread && head->covering_keys.is_set(index))
    head->set_keyread(true);
  else
    head->set_keyread(false);

  if (!file->inited)
  {
    /*
      read_set is set to the correct value for ror_merge_scan here as a
      subquery execution during optimization might result in innodb not
      initializing the read set in index_read() leading to wrong
      results while merging.
    */
    MY_BITMAP * const save_read_set= head->read_set;
    MY_BITMAP * const save_write_set= head->write_set;
    const bool sorted= (mrr_flags & HA_MRR_SORTED);
    DBUG_EXECUTE_IF("bug14365043_2",
                    DBUG_SET("+d,ha_index_init_fail"););

    /* Pass index specifc read set for ror_merged_scan */
    if (in_ror_merged_scan)
    {
      /*
        We don't need to signal the bitmap change as the bitmap is always the
        same for this head->file
      */
      head->column_bitmaps_set_no_signal(&column_bitmap, &column_bitmap);
    }
    if ((error= file->ha_index_init(index, sorted)))
    {
      file->print_error(error, MYF(0));
      DBUG_RETURN(error);
    }
    if (in_ror_merged_scan)
    {
      /* Restore bitmaps set on entry */
      head->column_bitmaps_set_no_signal(save_read_set, save_write_set);
    }
  }

  /* Allocate buffer if we need one but haven't allocated it yet */
  if (mrr_buf_size && !mrr_buf_desc)
  {
    buf_size= mrr_buf_size;
    while (buf_size && !my_multi_malloc(key_memory_QUICK_RANGE_SELECT_mrr_buf_desc,
                                        MYF(MY_WME),
                                        &mrr_buf_desc, sizeof(*mrr_buf_desc),
                                        &mrange_buff, buf_size,
                                        NullS))
    {
      /* Try to shrink the buffers until both are 0. */
      buf_size/= 2;
    }
    if (!mrr_buf_desc)
      DBUG_RETURN(HA_ERR_OUT_OF_MEM);

    /* Initialize the handler buffer. */
    mrr_buf_desc->buffer= mrange_buff;
    mrr_buf_desc->buffer_end= mrange_buff + buf_size;
    mrr_buf_desc->end_of_used_area= mrange_buff;
  }

  if (!mrr_buf_desc)
    empty_buf.buffer= empty_buf.buffer_end= empty_buf.end_of_used_area= NULL;
 
  RANGE_SEQ_IF seq_funcs= {quick_range_seq_init, quick_range_seq_next, 0, 0};
  error= file->multi_range_read_init(&seq_funcs, this, ranges.size(),
                                     mrr_flags, mrr_buf_desc? mrr_buf_desc:
                                                              &empty_buf);
  DBUG_RETURN(error);
}


/*
  Range sequence interface implementation for array<QUICK_RANGE>: initialize
  
  SYNOPSIS
    quick_range_seq_init()
      init_param  Caller-opaque paramenter: QUICK_RANGE_SELECT* pointer
      n_ranges    Number of ranges in the sequence (ignored)
      flags       MRR flags (currently not used) 

  RETURN
    Opaque value to be passed to quick_range_seq_next
*/

range_seq_t quick_range_seq_init(void *init_param, uint n_ranges, uint flags)
{
  QUICK_RANGE_SELECT *quick= static_cast<QUICK_RANGE_SELECT*>(init_param);
  quick->qr_traversal_ctx.first= quick->ranges.begin();
  quick->qr_traversal_ctx.cur= quick->ranges.begin();
  quick->qr_traversal_ctx.last= quick->ranges.end();
  return &quick->qr_traversal_ctx;
}


/*
  Range sequence interface implementation for array<QUICK_RANGE>: get next
  
  SYNOPSIS
    quick_range_seq_next()
      rseq        Value returned from quick_range_seq_init
      range  OUT  Store information about the range here

  RETURN
    0  Ok
    1  No more ranges in the sequence
*/

uint quick_range_seq_next(range_seq_t rseq, KEY_MULTI_RANGE *range)
{
  QUICK_RANGE_SEQ_CTX *ctx= (QUICK_RANGE_SEQ_CTX*)rseq;

  if (ctx->cur == ctx->last)
    return 1; /* no more ranges */

  QUICK_RANGE *cur= *(ctx->cur);
  key_range *start_key= &range->start_key;
  key_range *end_key=   &range->end_key;

  start_key->key=    cur->min_key;
  start_key->length= cur->min_length;
  start_key->keypart_map= cur->min_keypart_map;
  start_key->flag=   ((cur->flag & NEAR_MIN) ? HA_READ_AFTER_KEY :
                      (cur->flag & EQ_RANGE) ?
                      HA_READ_KEY_EXACT : HA_READ_KEY_OR_NEXT);
  end_key->key=      cur->max_key;
  end_key->length=   cur->max_length;
  end_key->keypart_map= cur->max_keypart_map;
  /*
    We use HA_READ_AFTER_KEY here because if we are reading on a key
    prefix. We want to find all keys with this prefix.
  */
  end_key->flag=     (cur->flag & NEAR_MAX ? HA_READ_BEFORE_KEY :
                      HA_READ_AFTER_KEY);
  range->range_flag= cur->flag;
  ctx->cur++;
  return 0;
}


/*
  MRR range sequence interface: array<QUICK_RANGE> impl: utility func for NDB

  SYNOPSIS
    mrr_persistent_flag_storage()
      seq  Range sequence being traversed
      idx  Number of range

  DESCRIPTION
    MRR/NDB implementation needs to store some bits for each range. This
    function returns a reference to the "range_flag" associated with the
    range number idx.

    This function should be removed when we get a proper MRR/NDB 
    implementation.

  RETURN
    Reference to range_flag associated with range number #idx
*/

uint16 &mrr_persistent_flag_storage(range_seq_t seq, uint idx)
{
  QUICK_RANGE_SEQ_CTX *ctx= (QUICK_RANGE_SEQ_CTX*)seq;
  return ctx->first[idx]->flag;
}


/*
  MRR range sequence interface: array<QUICK_RANGE> impl: utility func for NDB

  SYNOPSIS
    mrr_get_ptr_by_idx()
      seq  Range sequence bening traversed
      idx  Number of the range

  DESCRIPTION
    An extension of MRR range sequence interface needed by NDB: return the
    data associated with the given range.

    A proper MRR interface implementer is supposed to store and return
    range-associated data. NDB stores number of the range instead. So this
    is a helper function that translates range number to range associated
    data.

    This function does nothing, as currrently there is only one user of the
    MRR interface - the quick range select code, and this user doesn't need
    to use range-associated data.

  RETURN
    Reference to range-associated data
*/

char* &mrr_get_ptr_by_idx(range_seq_t seq, uint idx)
{
  static char *dummy;
  return dummy;
}


/*
  Get next possible record using quick-struct.

  SYNOPSIS
    QUICK_RANGE_SELECT::get_next()

  NOTES
    Record is read into table->record[0]

  RETURN
    0			Found row
    HA_ERR_END_OF_FILE	No (more) rows in range
    #			Error code
*/

int QUICK_RANGE_SELECT::get_next()
{
  char *dummy;
  MY_BITMAP * const save_read_set= head->read_set;
  MY_BITMAP * const save_write_set= head->write_set;
  DBUG_ENTER("QUICK_RANGE_SELECT::get_next");

  if (in_ror_merged_scan)
  {
    /*
      We don't need to signal the bitmap change as the bitmap is always the
      same for this head->file
    */
    head->column_bitmaps_set_no_signal(&column_bitmap, &column_bitmap);
  }

  int result= file->multi_range_read_next(&dummy);

  if (in_ror_merged_scan)
  {
    /* Restore bitmaps set on entry */
    head->column_bitmaps_set_no_signal(save_read_set, save_write_set);
  }
  DBUG_RETURN(result);
}


/*
  Get the next record with a different prefix.

  @param prefix_length   length of cur_prefix
  @param group_key_parts The number of key parts in the group prefix
  @param cur_prefix      prefix of a key to be searched for

  Each subsequent call to the method retrieves the first record that has a
  prefix with length prefix_length and which is different from cur_prefix,
  such that the record with the new prefix is within the ranges described by
  this->ranges. The record found is stored into the buffer pointed by
  this->record. The method is useful for GROUP-BY queries with range
  conditions to discover the prefix of the next group that satisfies the range
  conditions.

  @todo

    This method is a modified copy of QUICK_RANGE_SELECT::get_next(), so both
    methods should be unified into a more general one to reduce code
    duplication.

  @retval 0                  on success
  @retval HA_ERR_END_OF_FILE if returned all keys
  @retval other              if some error occurred
*/

int QUICK_RANGE_SELECT::get_next_prefix(uint prefix_length,
                                        uint group_key_parts,
                                        uchar *cur_prefix)
{
  DBUG_ENTER("QUICK_RANGE_SELECT::get_next_prefix");
  const key_part_map keypart_map= make_prev_keypart_map(group_key_parts);

  for (;;)
  {
    int result;
    if (last_range)
    {
      /* Read the next record in the same range with prefix after cur_prefix. */
      DBUG_ASSERT(cur_prefix != NULL);
      result= file->ha_index_read_map(record, cur_prefix, keypart_map,
                                      HA_READ_AFTER_KEY);
      if (result || last_range->max_keypart_map == 0)
        DBUG_RETURN(result);

      key_range previous_endpoint;
      last_range->make_max_endpoint(&previous_endpoint, prefix_length, keypart_map);
      if (file->compare_key(&previous_endpoint) <= 0)
        DBUG_RETURN(0);
    }

    const size_t count= ranges.size() - (cur_range - ranges.begin());
    if (count == 0)
    {
      /* Ranges have already been used up before. None is left for read. */
      last_range= 0;
      DBUG_RETURN(HA_ERR_END_OF_FILE);
    }
    last_range= *(cur_range++);

    key_range start_key, end_key;
    last_range->make_min_endpoint(&start_key, prefix_length, keypart_map);
    last_range->make_max_endpoint(&end_key, prefix_length, keypart_map);

    const bool sorted= (mrr_flags & HA_MRR_SORTED);
    result= file->read_range_first(last_range->min_keypart_map ? &start_key : 0,
				   last_range->max_keypart_map ? &end_key : 0,
                                   MY_TEST(last_range->flag & EQ_RANGE),
				   sorted);
    if (last_range->flag == (UNIQUE_RANGE | EQ_RANGE))
      last_range= 0;			// Stop searching

    if (result != HA_ERR_END_OF_FILE)
      DBUG_RETURN(result);
    last_range= 0;			// No matching rows; go to next range
  }
}


/* Get next for geometrical indexes */

int QUICK_RANGE_SELECT_GEOM::get_next()
{
  DBUG_ENTER("QUICK_RANGE_SELECT_GEOM::get_next");

  for (;;)
  {
    int result;
    if (last_range)
    {
      // Already read through key
      result= file->ha_index_next_same(record, last_range->min_key,
                                       last_range->min_length);
      if (result != HA_ERR_END_OF_FILE)
	DBUG_RETURN(result);
    }

    const size_t count= ranges.size() - (cur_range-ranges.begin());
    if (count == 0)
    {
      /* Ranges have already been used up before. None is left for read. */
      last_range= 0;
      DBUG_RETURN(HA_ERR_END_OF_FILE);
    }
    last_range= *(cur_range++);

    result= file->ha_index_read_map(record, last_range->min_key,
                                    last_range->min_keypart_map,
                                    last_range->rkey_func_flag);
    if (result != HA_ERR_KEY_NOT_FOUND && result != HA_ERR_END_OF_FILE)
      DBUG_RETURN(result);
    last_range= 0;				// Not found, to next range
  }
}


/*
  Check if current row will be retrieved by this QUICK_RANGE_SELECT

  NOTES
    It is assumed that currently a scan is being done on another index
    which reads all necessary parts of the index that is scanned by this
    quick select.
    The implementation does a binary search on sorted array of disjoint
    ranges, without taking size of range into account.

    This function is used to filter out clustered PK scan rows in
    index_merge quick select.

  RETURN
    TRUE  if current row will be retrieved by this quick select
    FALSE if not
*/

bool QUICK_RANGE_SELECT::row_in_ranges()
{
  QUICK_RANGE *res;
  size_t min= 0;
  size_t max= ranges.size() - 1;
  size_t mid= (max + min)/2;

  while (min != max)
  {
    if (cmp_next(ranges[mid]))
    {
      /* current row value > mid->max */
      min= mid + 1;
    }
    else
      max= mid;
    mid= (min + max) / 2;
  }
  res= ranges[mid];
  return (!cmp_next(res) && !cmp_prev(res));
}

/*
  This is a hack: we inherit from QUICK_RANGE_SELECT so that we can use the
  get_next() interface, but we have to hold a pointer to the original
  QUICK_RANGE_SELECT because its data are used all over the place. What
  should be done is to factor out the data that is needed into a base
  class (QUICK_SELECT), and then have two subclasses (_ASC and _DESC)
  which handle the ranges and implement the get_next() function.  But
  for now, this seems to work right at least.
 */

QUICK_SELECT_DESC::QUICK_SELECT_DESC(QUICK_RANGE_SELECT *q,
                                     uint used_key_parts_arg,
                                     bool *error)
 :QUICK_RANGE_SELECT(*q), rev_it(rev_ranges),
  used_key_parts (used_key_parts_arg)
{
  QUICK_RANGE *r;
  /* 
    Use default MRR implementation for reverse scans. No table engine
    currently can do an MRR scan with output in reverse index order.
  */
  mrr_buf_desc= NULL;
  mrr_flags |= HA_MRR_USE_DEFAULT_IMPL;
  mrr_flags |= HA_MRR_SORTED; // 'sorted' as internals use index_last/_prev
  mrr_buf_size= 0;


  Quick_ranges::const_iterator pr= ranges.begin();
  Quick_ranges::const_iterator end_range= ranges.end();
  for (; pr != end_range; pr++)
    rev_ranges.push_front(*pr);

  /* Remove EQ_RANGE flag for keys that are not using the full key */
  for (r = rev_it++; r; r = rev_it++)
  {
    if ((r->flag & EQ_RANGE) &&
	head->key_info[index].key_length != r->max_length)
      r->flag&= ~EQ_RANGE;
  }
  rev_it.rewind();
  q->dont_free=1;				// Don't free shared mem
}


int QUICK_SELECT_DESC::get_next()
{
  DBUG_ENTER("QUICK_SELECT_DESC::get_next");

  /* The max key is handled as follows:
   *   - if there is NO_MAX_RANGE, start at the end and move backwards
   *   - if it is an EQ_RANGE (which means that max key covers the entire
   *     key) and the query does not use any hidden key fields that are
   *     not considered when the range optimzier sets EQ_RANGE (e.g. the 
   *     primary key added by InnoDB), then go directly to the key and 
   *     read through it (sorting backwards is same as sorting forwards).
   *   - if it is NEAR_MAX, go to the key or next, step back once, and
   *     move backwards
   *   - otherwise (not NEAR_MAX == include the key), go after the key,
   *     step back once, and move backwards
   */

  for (;;)
  {
    int result;
    if (last_range)
    {						// Already read through key
      result = ((last_range->flag & EQ_RANGE && 
                 used_key_parts <=
                 head->key_info[index].user_defined_key_parts) ?
                file->ha_index_next_same(record, last_range->min_key,
                                         last_range->min_length) :
                file->ha_index_prev(record));
      if (!result)
      {
	if (cmp_prev(*rev_it.ref()) == 0)
	  DBUG_RETURN(0);
      }
      else if (result != HA_ERR_END_OF_FILE)
	DBUG_RETURN(result);
    }

    if (!(last_range= rev_it++))
      DBUG_RETURN(HA_ERR_END_OF_FILE);		// All ranges used

    // Case where we can avoid descending scan, see comment above
    const bool eqrange_all_keyparts= (last_range->flag & EQ_RANGE) && 
      (used_key_parts <= head->key_info[index].user_defined_key_parts);

    /*
      If we have pushed an index condition (ICP) and this quick select
      will use ha_index_prev() to read data, we need to let the
      handler know where to end the scan in order to avoid that the
      ICP implemention continues to read past the range boundary.
    */
    if (file->pushed_idx_cond)
    {
      if (!eqrange_all_keyparts)
      {
        key_range min_range;
        last_range->make_min_endpoint(&min_range);
        if(min_range.length > 0)
          file->set_end_range(&min_range, handler::RANGE_SCAN_DESC);
        else
          file->set_end_range(NULL, handler::RANGE_SCAN_DESC);
      }
      else
      {
        /*
          Will use ha_index_next_same() for reading records. In case we have
          set the end range for an earlier range, this need to be cleared.
        */
        file->set_end_range(NULL, handler::RANGE_SCAN_ASC);
      }
    }

    if (last_range->flag & NO_MAX_RANGE)        // Read last record
    {
      int local_error;
      if ((local_error= file->ha_index_last(record)))
      {
        /*
          HA_ERR_END_OF_FILE is returned both when the table is empty and when
          there are no qualifying records in the range (when using ICP).
          Interpret this return value as "no qualifying rows in the range" to
          avoid loss of records. If the error code truly meant "empty table"
          the next iteration of the loop will exit.
        */
        if (local_error != HA_ERR_END_OF_FILE)
          DBUG_RETURN(local_error);
        last_range= NULL;                       // Go to next range
        continue;
      }

      if (cmp_prev(last_range) == 0)
	DBUG_RETURN(0);
      last_range= 0;                            // No match; go to next range
      continue;
    }

    if (eqrange_all_keyparts)

    {
      result= file->ha_index_read_map(record, last_range->max_key,
                                      last_range->max_keypart_map,
                                      HA_READ_KEY_EXACT);
    }
    else
    {
      DBUG_ASSERT(last_range->flag & NEAR_MAX ||
                  (last_range->flag & EQ_RANGE && 
                   used_key_parts >
                   head->key_info[index].user_defined_key_parts) ||
                  range_reads_after_key(last_range));
      result= file->ha_index_read_map(record, last_range->max_key,
                                      last_range->max_keypart_map,
                                      ((last_range->flag & NEAR_MAX) ?
                                       HA_READ_BEFORE_KEY :
                                       HA_READ_PREFIX_LAST_OR_PREV));
    }
    if (result)
    {
      if (result != HA_ERR_KEY_NOT_FOUND && result != HA_ERR_END_OF_FILE)
	DBUG_RETURN(result);
      last_range= 0;                            // Not found, to next range
      continue;
    }
    if (cmp_prev(last_range) == 0)
    {
      if (last_range->flag == (UNIQUE_RANGE | EQ_RANGE))
	last_range= 0;				// Stop searching
      DBUG_RETURN(0);				// Found key is in range
    }
    last_range= 0;                              // To next range
  }
}


/**
  Create a compatible quick select with the result ordered in an opposite way

  @param used_key_parts_arg  Number of used key parts

  @retval NULL in case of errors (OOM etc)
  @retval pointer to a newly created QUICK_SELECT_DESC if success
*/

QUICK_SELECT_I *QUICK_RANGE_SELECT::make_reverse(uint used_key_parts_arg)
{
  bool error= FALSE;
  QUICK_SELECT_DESC *new_quick= new QUICK_SELECT_DESC(this, used_key_parts_arg,
                                                      &error);
  if (new_quick == NULL || error)
  {
    delete new_quick;
    return NULL;
  }
  return new_quick;
}


/*
  Compare if found key is over max-value
  Returns 0 if key <= range->max_key
  TODO: Figure out why can't this function be as simple as cmp_prev().
  At least it could use key_cmp() from key.cc, it's almost identical.
*/

int QUICK_RANGE_SELECT::cmp_next(QUICK_RANGE *range_arg)
{
  if (range_arg->flag & NO_MAX_RANGE)
    return 0;                                   /* key can't be to large */

  KEY_PART *key_part=key_parts;
  uint store_length;

  for (uchar *key=range_arg->max_key, *end=key+range_arg->max_length;
       key < end;
       key+= store_length, key_part++)
  {
    int cmp;
    store_length= key_part->store_length;
    if (key_part->null_bit)
    {
      if (*key)
      {
        if (!key_part->field->is_null())
          return 1;
        continue;
      }
      else if (key_part->field->is_null())
        return 0;
      key++;					// Skip null byte
      store_length--;
    }
    if ((cmp=key_part->field->key_cmp(key, key_part->length)) < 0)
      return 0;
    if (cmp > 0)
      return 1;
  }
  return (range_arg->flag & NEAR_MAX) ? 1 : 0;          // Exact match
}


/*
  Returns 0 if found key is inside range (found key >= range->min_key).
*/

int QUICK_RANGE_SELECT::cmp_prev(QUICK_RANGE *range_arg)
{
  int cmp;
  if (range_arg->flag & NO_MIN_RANGE)
    return 0;					/* key can't be to small */

  cmp= key_cmp(key_part_info, range_arg->min_key,
               range_arg->min_length);
  if (cmp > 0 || (cmp == 0 && !(range_arg->flag & NEAR_MIN)))
    return 0;
  return 1;                                     // outside of range
}


/*
 * TRUE if this range will require using HA_READ_AFTER_KEY
   See comment in get_next() about this
 */

bool QUICK_SELECT_DESC::range_reads_after_key(QUICK_RANGE *range_arg)
{
  return ((range_arg->flag & (NO_MAX_RANGE | NEAR_MAX)) ||
	  !(range_arg->flag & EQ_RANGE) ||
	  head->key_info[index].key_length != range_arg->max_length) ? 1 : 0;
}


void QUICK_RANGE_SELECT::add_info_string(String *str)
{
  KEY *key_info= head->key_info + index;
  str->append(key_info->name);
}

void QUICK_INDEX_MERGE_SELECT::add_info_string(String *str)
{
  QUICK_RANGE_SELECT *quick;
  bool first= TRUE;
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  str->append(STRING_WITH_LEN("sort_union("));
  while ((quick= it++))
  {
    if (!first)
      str->append(',');
    else
      first= FALSE;
    quick->add_info_string(str);
  }
  if (pk_quick_select)
  {
    str->append(',');
    pk_quick_select->add_info_string(str);
  }
  str->append(')');
}

void QUICK_ROR_INTERSECT_SELECT::add_info_string(String *str)
{
  bool first= TRUE;
  QUICK_RANGE_SELECT *quick;
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  str->append(STRING_WITH_LEN("intersect("));
  while ((quick= it++))
  {
    KEY *key_info= head->key_info + quick->index;
    if (!first)
      str->append(',');
    else
      first= FALSE;
    str->append(key_info->name);
  }
  if (cpk_quick)
  {
    KEY *key_info= head->key_info + cpk_quick->index;
    str->append(',');
    str->append(key_info->name);
  }
  str->append(')');
}

void QUICK_ROR_UNION_SELECT::add_info_string(String *str)
{
  bool first= TRUE;
  QUICK_SELECT_I *quick;
  List_iterator_fast<QUICK_SELECT_I> it(quick_selects);
  str->append(STRING_WITH_LEN("union("));
  while ((quick= it++))
  {
    if (!first)
      str->append(',');
    else
      first= FALSE;
    quick->add_info_string(str);
  }
  str->append(')');
}

void QUICK_GROUP_MIN_MAX_SELECT::add_info_string(String *str)
{
  str->append(STRING_WITH_LEN("index_for_group_by("));
  str->append(index_info->name);
  str->append(')');
}

void QUICK_RANGE_SELECT::add_keys_and_lengths(String *key_names,
                                              String *used_lengths)
{
  char buf[64];
  size_t length;
  KEY *key_info= head->key_info + index;
  key_names->append(key_info->name);
  length= longlong2str(max_used_key_length, buf, 10) - buf;
  used_lengths->append(buf, length);
}

void QUICK_INDEX_MERGE_SELECT::add_keys_and_lengths(String *key_names,
                                                    String *used_lengths)
{
  char buf[64];
  size_t length;
  bool first= TRUE;
  QUICK_RANGE_SELECT *quick;

  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  while ((quick= it++))
  {
    if (first)
      first= FALSE;
    else
    {
      key_names->append(',');
      used_lengths->append(',');
    }

    KEY *key_info= head->key_info + quick->index;
    key_names->append(key_info->name);
    length= longlong2str(quick->max_used_key_length, buf, 10) - buf;
    used_lengths->append(buf, length);
  }
  if (pk_quick_select)
  {
    KEY *key_info= head->key_info + pk_quick_select->index;
    key_names->append(',');
    key_names->append(key_info->name);
    length= longlong2str(pk_quick_select->max_used_key_length, buf, 10) - buf;
    used_lengths->append(',');
    used_lengths->append(buf, length);
  }
}

void QUICK_ROR_INTERSECT_SELECT::add_keys_and_lengths(String *key_names,
                                                      String *used_lengths)
{
  char buf[64];
  size_t length;
  bool first= TRUE;
  QUICK_RANGE_SELECT *quick;
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  while ((quick= it++))
  {
    KEY *key_info= head->key_info + quick->index;
    if (first)
      first= FALSE;
    else
    {
      key_names->append(',');
      used_lengths->append(',');
    }
    key_names->append(key_info->name);
    length= longlong2str(quick->max_used_key_length, buf, 10) - buf;
    used_lengths->append(buf, length);
  }

  if (cpk_quick)
  {
    KEY *key_info= head->key_info + cpk_quick->index;
    key_names->append(',');
    key_names->append(key_info->name);
    length= longlong2str(cpk_quick->max_used_key_length, buf, 10) - buf;
    used_lengths->append(',');
    used_lengths->append(buf, length);
  }
}

void QUICK_ROR_UNION_SELECT::add_keys_and_lengths(String *key_names,
                                                  String *used_lengths)
{
  bool first= TRUE;
  QUICK_SELECT_I *quick;
  List_iterator_fast<QUICK_SELECT_I> it(quick_selects);
  while ((quick= it++))
  {
    if (first)
      first= FALSE;
    else
    {
      used_lengths->append(',');
      key_names->append(',');
    }
    quick->add_keys_and_lengths(key_names, used_lengths);
  }
}


/*******************************************************************************
* Implementation of QUICK_GROUP_MIN_MAX_SELECT
*******************************************************************************/

static inline uint get_field_keypart(KEY *index, Field *field);
static inline SEL_ARG * get_index_range_tree(uint index, SEL_TREE* range_tree,
                                             PARAM *param);
static bool get_sel_arg_for_keypart(Field *field, SEL_ARG *index_range_tree,
                                    SEL_ARG **cur_range);
static bool get_constant_key_infix(KEY *index_info, SEL_ARG *index_range_tree,
                       KEY_PART_INFO *first_non_group_part,
                       KEY_PART_INFO *min_max_arg_part,
                       KEY_PART_INFO *last_part, THD *thd,
                       uchar *key_infix, uint *key_infix_len,
                       KEY_PART_INFO **first_non_infix_part);
static bool
check_group_min_max_predicates(Item *cond, Item_field *min_max_arg_item,
                               Field::imagetype image_type);

static bool
min_max_inspect_cond_for_fields(Item *cond, Item_field *min_max_arg_item,
                                bool *min_max_arg_present,
                                bool *non_min_max_arg_present);

static void
cost_group_min_max(TABLE* table, uint key, uint used_key_parts,
                   uint group_key_parts, SEL_TREE *range_tree,
                   SEL_ARG *index_tree, ha_rows quick_prefix_records,
                   bool have_min, bool have_max,
                   Cost_estimate *cost_est, ha_rows *records);


/**
  Test if this access method is applicable to a GROUP query with MIN/MAX
  functions, and if so, construct a new TRP object.

  DESCRIPTION
    Test whether a query can be computed via a QUICK_GROUP_MIN_MAX_SELECT.
    Queries computable via a QUICK_GROUP_MIN_MAX_SELECT must satisfy the
    following conditions:
    A) Table T has at least one compound index I of the form:
       I = <A_1, ...,A_k, [B_1,..., B_m], C, [D_1,...,D_n]>
    B) Query conditions:
    B0. Q is over a single table T.
    B1. The attributes referenced by Q are a subset of the attributes of I.
    B2. All attributes QA in Q can be divided into 3 overlapping groups:
        - SA = {S_1, ..., S_l, [C]} - from the SELECT clause, where C is
          referenced by any number of MIN and/or MAX functions if present.
        - WA = {W_1, ..., W_p} - from the WHERE clause
        - GA = <G_1, ..., G_k> - from the GROUP BY clause (if any)
             = SA              - if Q is a DISTINCT query (based on the
                                 equivalence of DISTINCT and GROUP queries.
        - NGA = QA - (GA union C) = {NG_1, ..., NG_m} - the ones not in
          GROUP BY and not referenced by MIN/MAX functions.
        with the following properties specified below.
    B3. If Q has a GROUP BY WITH ROLLUP clause the access method is not 
        applicable.

    SA1. There is at most one attribute in SA referenced by any number of
         MIN and/or MAX functions which, which if present, is denoted as C.
    SA2. The position of the C attribute in the index is after the last A_k.
    SA3. The attribute C can be referenced in the WHERE clause only in
         predicates of the forms:
         - (C {< | <= | > | >= | =} const)
         - (const {< | <= | > | >= | =} C)
         - (C between const_i and const_j)
         - C IS NULL
         - C IS NOT NULL
         - C != const
    SA4. If Q has a GROUP BY clause, there are no other aggregate functions
         except MIN and MAX. For queries with DISTINCT, aggregate functions
         are allowed.
    SA5. The select list in DISTINCT queries should not contain expressions.
    SA6. Clustered index can not be used by GROUP_MIN_MAX quick select
         for AGG_FUNC(DISTINCT ...) optimization because cursor position is
         never stored after a unique key lookup in the clustered index and
         furhter index_next/prev calls can not be used. So loose index scan
         optimization can not be used in this case.
    SA7. If Q has both AGG_FUNC(DISTINCT ...) and MIN/MAX() functions then this
         access method is not used.
         For above queries MIN/MAX() aggregation has to be done at
         nested_loops_join (end_send_group). But with current design MIN/MAX()
         is always set as part of loose index scan. Because of this mismatch
         MIN() and MAX() values will be set incorrectly. For such queries to
         work we need a new interface for loose index scan. This new interface
         should only fetch records with min and max values and let
         end_send_group to do aggregation. Until then do not use
         loose_index_scan.
    GA1. If Q has a GROUP BY clause, then GA is a prefix of I. That is, if
         G_i = A_j => i = j.
    GA2. If Q has a DISTINCT clause, then there is a permutation of SA that
         forms a prefix of I. This permutation is used as the GROUP clause
         when the DISTINCT query is converted to a GROUP query.
    GA3. The attributes in GA may participate in arbitrary predicates, divided
         into two groups:
         - RNG(G_1,...,G_q ; where q <= k) is a range condition over the
           attributes of a prefix of GA
         - PA(G_i1,...G_iq) is an arbitrary predicate over an arbitrary subset
           of GA. Since P is applied to only GROUP attributes it filters some
           groups, and thus can be applied after the grouping.
    GA4. There are no expressions among G_i, just direct column references.
    NGA1.If in the index I there is a gap between the last GROUP attribute G_k,
         and the MIN/MAX attribute C, then NGA must consist of exactly the
         index attributes that constitute the gap. As a result there is a
         permutation of NGA, BA=<B_1,...,B_m>, that coincides with the gap
         in the index.
    NGA2.If BA <> {}, then the WHERE clause must contain a conjunction EQ of
         equality conditions for all NG_i of the form (NG_i = const) or
         (const = NG_i), such that each NG_i is referenced in exactly one
         conjunct. Informally, the predicates provide constants to fill the
         gap in the index.
    NGA3.If BA <> {}, there can only be one range. TODO: This is a code
         limitation and is not strictly needed. See BUG#15947433
    WA1. There are no other attributes in the WHERE clause except the ones
         referenced in predicates RNG, PA, PC, EQ defined above. Therefore
         WA is subset of (GA union NGA union C) for GA,NGA,C that pass the
         above tests. By transitivity then it also follows that each WA_i
         participates in the index I (if this was already tested for GA, NGA
         and C).
    WA2. If there is a predicate on C, then it must be in conjunction
         to all predicates on all earlier keyparts in I.

    C) Overall query form:
       SELECT EXPR([A_1,...,A_k], [B_1,...,B_m], [MIN(C)], [MAX(C)])
         FROM T
        WHERE [RNG(A_1,...,A_p ; where p <= k)]
         [AND EQ(B_1,...,B_m)]
         [AND PC(C)]
         [AND PA(A_i1,...,A_iq)]
       GROUP BY A_1,...,A_k
       [HAVING PH(A_1, ..., B_1,..., C)]
    where EXPR(...) is an arbitrary expression over some or all SELECT fields,
    or:
       SELECT DISTINCT A_i1,...,A_ik
         FROM T
        WHERE [RNG(A_1,...,A_p ; where p <= k)]
         [AND PA(A_i1,...,A_iq)];

  NOTES
    If the current query satisfies the conditions above, and if
    (mem_root! = NULL), then the function constructs and returns a new TRP
    object, that is later used to construct a new QUICK_GROUP_MIN_MAX_SELECT.
    If (mem_root == NULL), then the function only tests whether the current
    query satisfies the conditions above, and, if so, sets
    is_applicable = TRUE.

    Queries with DISTINCT for which index access can be used are transformed
    into equivalent group-by queries of the form:

    SELECT A_1,...,A_k FROM T
     WHERE [RNG(A_1,...,A_p ; where p <= k)]
      [AND PA(A_i1,...,A_iq)]
    GROUP BY A_1,...,A_k;

    The group-by list is a permutation of the select attributes, according
    to their order in the index.

  TODO
  - What happens if the query groups by the MIN/MAX field, and there is no
    other field as in: "select min(a) from t1 group by a" ?
  - We assume that the general correctness of the GROUP-BY query was checked
    before this point. Is this correct, or do we have to check it completely?
  - Lift the limitation in condition (B3), that is, make this access method 
    applicable to ROLLUP queries.

 @param  param     Parameter from test_quick_select
 @param  sel_tree  Range tree generated by get_mm_tree
 @param  cost_est  Best cost so far (=table/index scan time)
 @return table read plan
   @retval NULL  Loose index scan not applicable or mem_root == NULL
   @retval !NULL Loose index scan table read plan
*/

static TRP_GROUP_MIN_MAX *
get_best_group_min_max(PARAM *param, SEL_TREE *tree, const Cost_estimate *cost_est)
{
  THD *thd= param->thd;
  JOIN *join= thd->lex->current_select()->join;
  TABLE *table= param->table;
  bool have_min= FALSE;              /* TRUE if there is a MIN function. */
  bool have_max= FALSE;              /* TRUE if there is a MAX function. */
  Item_field *min_max_arg_item= NULL; // The argument of all MIN/MAX functions
  KEY_PART_INFO *min_max_arg_part= NULL; /* The corresponding keypart. */
  uint group_prefix_len= 0; /* Length (in bytes) of the key prefix. */
  KEY *index_info= NULL;    /* The index chosen for data access. */
  uint index= 0;            /* The id of the chosen index. */
  uint group_key_parts= 0;  // Number of index key parts in the group prefix.
  uint used_key_parts= 0;   /* Number of index key parts used for access. */
  uchar key_infix[MAX_KEY_LENGTH]; /* Constants from equality predicates.*/
  uint key_infix_len= 0;          /* Length of key_infix. */
  TRP_GROUP_MIN_MAX *read_plan= NULL; /* The eventually constructed TRP. */
  uint key_part_nr;
  ORDER *tmp_group;
  Item *item;
  Item_field *item_field;
  bool is_agg_distinct;
  List<Item_field> agg_distinct_flds;
  /* Cost-related variables for the best index so far. */
  Cost_estimate best_read_cost;
  ha_rows best_records= 0;
  SEL_ARG *best_index_tree= NULL;
  ha_rows best_quick_prefix_records= 0;
  uint best_param_idx= 0;
  List_iterator<Item> select_items_it;
  Opt_trace_context * const trace= &param->thd->opt_trace;

  DBUG_ENTER("get_best_group_min_max");

  Opt_trace_object trace_group(trace, "group_index_range",
                               Opt_trace_context::RANGE_OPTIMIZER);
  const char* cause= NULL;
  best_read_cost.set_max_cost();

  /* Perform few 'cheap' tests whether this access method is applicable. */
  if (!join)
    cause= "no_join";
  else if (join->primary_tables != 1)  /* Query must reference one table. */
    cause= "not_single_table";
  else if (join->select_lex->olap == ROLLUP_TYPE) /* Check (B3) for ROLLUP */
    cause= "rollup";
  else if (table->s->keys == 0)        /* There are no indexes to use. */
    cause= "no_index";
  else if (param->order_direction == ORDER::ORDER_DESC)
    cause= "cannot_do_reverse_ordering";
  if (cause != NULL)
  {
    trace_group.add("chosen", false).add_alnum("cause", cause);
    DBUG_RETURN(NULL);
  }

  /* Check (SA1,SA4) and store the only MIN/MAX argument - the C attribute.*/
  is_agg_distinct = is_indexed_agg_distinct(join, &agg_distinct_flds);

  if ((!join->group_list) && /* Neither GROUP BY nor a DISTINCT query. */
      (!join->select_distinct) &&
      !is_agg_distinct)
  {
    trace_group.add("chosen", false).
      add_alnum("cause", "not_group_by_or_distinct");
    DBUG_RETURN(NULL);
  }
  /* Analyze the query in more detail. */

  if (join->sum_funcs[0])
  {
    Item_sum *min_max_item;
    Item_sum **func_ptr= join->sum_funcs;
    while ((min_max_item= *(func_ptr++)))
    {
      if (min_max_item->sum_func() == Item_sum::MIN_FUNC)
        have_min= TRUE;
      else if (min_max_item->sum_func() == Item_sum::MAX_FUNC)
        have_max= TRUE;
      else if (is_agg_distinct &&
               (min_max_item->sum_func() == Item_sum::COUNT_DISTINCT_FUNC ||
                min_max_item->sum_func() == Item_sum::SUM_DISTINCT_FUNC ||
                min_max_item->sum_func() == Item_sum::AVG_DISTINCT_FUNC))
        continue;
      else
      {
        trace_group.add("chosen", false).
          add_alnum("cause", "not_applicable_aggregate_function");
        DBUG_RETURN(NULL);
      }

      /* The argument of MIN/MAX. */
      Item *expr= min_max_item->get_arg(0)->real_item();
      if (expr->type() == Item::FIELD_ITEM) /* Is it an attribute? */
      {
        if (! min_max_arg_item)
          min_max_arg_item= (Item_field*) expr;
        else if (! min_max_arg_item->eq(expr, 1))
          DBUG_RETURN(NULL);
      }
      else
        DBUG_RETURN(NULL);
    }
  }

  /**
    Test (Part of WA2): Skip loose index scan on disjunctive WHERE clause which
    results in null tree or merge tree.
  */
  if (tree && !tree->merges.is_empty())
  {
    /**
      The tree structure contains multiple disjoint trees. This happens when
      the WHERE clause can't be represented in a single range tree due to the
      disjunctive nature of it but there exists indexes to perform index
      merge scan.
    */
    trace_group.add("chosen", false).
      add_alnum("cause", "disjuntive_predicate_present");
    DBUG_RETURN(NULL);
  }
  else if (!tree && join->where_cond && min_max_arg_item)
  {
    /**
      Skip loose index scan if min_max attribute is present along with
      at least one other attribute in the WHERE cluse when the tree is null.
      There is no range tree if WHERE condition can't be represented in a
      single range tree and index merge is not possible.
    */
    bool min_max_arg_present= false;
    bool non_min_max_arg_present= false;
    if (min_max_inspect_cond_for_fields(join->where_cond,
                                        min_max_arg_item,
                                        &min_max_arg_present,
                                        &non_min_max_arg_present))
    {
      trace_group.add("chosen", false).
        add_alnum("cause", "minmax_keypart_in_disjunctive_query");
      DBUG_RETURN(NULL);
    }
  }

  /* Check (SA7). */
  if (is_agg_distinct && (have_max || have_min))
  {
    trace_group.add("chosen", false).
      add_alnum("cause", "have_both_agg_distinct_and_min_max");
    DBUG_RETURN(NULL);
  }

  select_items_it= List_iterator<Item>(join->fields_list);
  /* Check (SA5). */
  if (join->select_distinct)
  {
    trace_group.add("distinct_query", true);
    while ((item= select_items_it++))
    {
      if (item->real_item()->type() != Item::FIELD_ITEM)
        DBUG_RETURN(NULL);
    }
  }

  /* Check (GA4) - that there are no expressions among the group attributes. */
  for (tmp_group= join->group_list; tmp_group; tmp_group= tmp_group->next)
  {
    if ((*tmp_group->item)->real_item()->type() != Item::FIELD_ITEM)
    {
      trace_group.add("chosen", false).
        add_alnum("cause", "group_field_is_expression");
      DBUG_RETURN(NULL);
    }
  }

  /*
    Check that table has at least one compound index such that the conditions
    (GA1,GA2) are all TRUE. If there is more than one such index, select the
    first one. Here we set the variables: group_prefix_len and index_info.
  */

  const uint pk= param->table->s->primary_key;
  SEL_ARG *cur_index_tree= NULL;
  ha_rows cur_quick_prefix_records= 0;
  Opt_trace_array trace_indexes(trace, "potential_group_range_indexes");
  // We go through allowed indexes
  for (uint cur_param_idx= 0; cur_param_idx < param->keys ; ++cur_param_idx)
  {
    const uint cur_index= param->real_keynr[cur_param_idx];
    KEY *const cur_index_info= &table->key_info[cur_index];
    Opt_trace_object trace_idx(trace);
    trace_idx.add_utf8("index", cur_index_info->name);
    KEY_PART_INFO *cur_part;
    KEY_PART_INFO *end_part; /* Last part for loops. */
    /* Last index part. */
    KEY_PART_INFO *last_part;
    KEY_PART_INFO *first_non_group_part;
    KEY_PART_INFO *first_non_infix_part;
    uint key_infix_parts;
    uint cur_group_key_parts= 0;
    uint cur_group_prefix_len= 0;
    Cost_estimate cur_read_cost;
    ha_rows cur_records;
    key_map used_key_parts_map;
    uint max_key_part= 0;
    uint cur_key_infix_len= 0;
    uchar cur_key_infix[MAX_KEY_LENGTH];
    uint cur_used_key_parts;

    /* Check (B1) - if current index is covering. */
    if (!table->covering_keys.is_set(cur_index))
    {
      cause= "not_covering";
      goto next_index;
    }

    /*
      If the current storage manager is such that it appends the primary key to
      each index, then the above condition is insufficient to check if the
      index is covering. In such cases it may happen that some fields are
      covered by the PK index, but not by the current index. Since we can't
      use the concatenation of both indexes for index lookup, such an index
      does not qualify as covering in our case. If this is the case, below
      we check that all query fields are indeed covered by 'cur_index'.
    */
    if (pk < MAX_KEY && cur_index != pk &&
        (table->file->ha_table_flags() & HA_PRIMARY_KEY_IN_READ_INDEX))
    {
      /* For each table field */
      for (uint i= 0; i < table->s->fields; i++)
      {
        Field *cur_field= table->field[i];
        /*
          If the field is used in the current query ensure that it's
          part of 'cur_index'
        */
        if (bitmap_is_set(table->read_set, cur_field->field_index) &&
            !cur_field->is_part_of_actual_key(thd, cur_index, cur_index_info))
        {
          cause= "not_covering";
          goto next_index;                  // Field was not part of key
        }
      }
    }
    trace_idx.add("covering", true);

    /*
      Check (GA1) for GROUP BY queries.
    */
    if (join->group_list)
    {
      cur_part= cur_index_info->key_part;
      end_part= cur_part + actual_key_parts(cur_index_info);
      /* Iterate in parallel over the GROUP list and the index parts. */
      for (tmp_group= join->group_list; tmp_group && (cur_part != end_part);
           tmp_group= tmp_group->next, cur_part++)
      {
        /*
          TODO:
          tmp_group::item is an array of Item, is it OK to consider only the
          first Item? If so, then why? What is the array for?
        */
        /* Above we already checked that all group items are fields. */
        DBUG_ASSERT((*tmp_group->item)->real_item()->type() == Item::FIELD_ITEM);
        Item_field *group_field= (Item_field *) (*tmp_group->item)->real_item();
        if (group_field->field->eq(cur_part->field))
        {
          cur_group_prefix_len+= cur_part->store_length;
          ++cur_group_key_parts;
          max_key_part= cur_part - cur_index_info->key_part + 1;
          used_key_parts_map.set_bit(max_key_part);
        }
        else
        {
          cause= "group_attribute_not_prefix_in_index";
          goto next_index;
        }
      }
    }

    /*
      Check (GA2) if this is a DISTINCT query.
      If GA2, then Store a new ORDER object in group_fields_array at the
      position of the key part of item_field->field. Thus we get the ORDER
      objects for each field ordered as the corresponding key parts.
      Later group_fields_array of ORDER objects is used to convert the query
      to a GROUP query.
    */
    if ((!join->group_list && join->select_distinct) ||
        is_agg_distinct)
    {
      if (!is_agg_distinct)
      {
        select_items_it.rewind();
      }

      List_iterator<Item_field> agg_distinct_flds_it (agg_distinct_flds);
      while (NULL !=
             (item= (is_agg_distinct ?
                     (Item *) agg_distinct_flds_it++ : select_items_it++)))
      {
        /* (SA5) already checked above. */
        item_field= (Item_field*) item->real_item(); 
        DBUG_ASSERT(item->real_item()->type() == Item::FIELD_ITEM);

        /* not doing loose index scan for derived tables */
        if (!item_field->field)
        {
          cause= "derived_table";
          goto next_index;
        }

        /* Find the order of the key part in the index. */
        key_part_nr= get_field_keypart(cur_index_info, item_field->field);
        /*
          Check if this attribute was already present in the select list.
          If it was present, then its corresponding key part was alredy used.
        */
        if (used_key_parts_map.is_set(key_part_nr))
          continue;
        if (key_part_nr < 1 ||
            (!is_agg_distinct && key_part_nr > join->fields_list.elements))
        {
          cause= "select_attribute_not_prefix_in_index";
          goto next_index;
        }
        cur_part= cur_index_info->key_part + key_part_nr - 1;
        cur_group_prefix_len+= cur_part->store_length;
        used_key_parts_map.set_bit(key_part_nr);
        ++cur_group_key_parts;
        max_key_part= max(max_key_part,key_part_nr);
      }
      /*
        Check that used key parts forms a prefix of the index.
        To check this we compare bits in all_parts and cur_parts.
        all_parts have all bits set from 0 to (max_key_part-1).
        cur_parts have bits set for only used keyparts.
      */
      ulonglong all_parts, cur_parts;
      all_parts= (1ULL << max_key_part) - 1;
      cur_parts= used_key_parts_map.to_ulonglong() >> 1;
      if (all_parts != cur_parts)
        goto next_index;
    }

    /* Check (SA2). */
    if (min_max_arg_item)
    {
      key_part_nr= get_field_keypart(cur_index_info, min_max_arg_item->field);
      if (key_part_nr <= cur_group_key_parts)
      {
        cause= "aggregate_column_not_suffix_in_idx";
        goto next_index;
      }
      min_max_arg_part= cur_index_info->key_part + key_part_nr - 1;
    }

    /* Check (SA6) if clustered key is used. */
    if (is_agg_distinct && cur_index == table->s->primary_key &&
        table->file->primary_key_is_clustered())
    {
      cause= "primary_key_is_clustered";
      goto next_index;
    }

    /*
      Check (NGA1, NGA2) and extract a sequence of constants to be used as part
      of all search keys.
    */

    /*
      If there is MIN/MAX, each keypart between the last group part and the
      MIN/MAX part must participate in one equality with constants, and all
      keyparts after the MIN/MAX part must not be referenced in the query.

      If there is no MIN/MAX, the keyparts after the last group part can be
      referenced only in equalities with constants, and the referenced keyparts
      must form a sequence without any gaps that starts immediately after the
      last group keypart.
    */
    last_part= cur_index_info->key_part + actual_key_parts(cur_index_info);
    first_non_group_part= 
      (cur_group_key_parts < actual_key_parts(cur_index_info)) ?
      cur_index_info->key_part + cur_group_key_parts :
      NULL;
    first_non_infix_part= min_max_arg_part ?
      (min_max_arg_part < last_part) ?
      min_max_arg_part :
      NULL :
      NULL;
    if (first_non_group_part &&
        (!min_max_arg_part || (min_max_arg_part - first_non_group_part > 0)))
    {
      if (tree)
      {
        SEL_ARG *index_range_tree= get_index_range_tree(cur_index, tree, param);
        if (!get_constant_key_infix(cur_index_info, index_range_tree,
                                    first_non_group_part, min_max_arg_part,
                                    last_part, thd, cur_key_infix, 
                                    &cur_key_infix_len,
                                    &first_non_infix_part))
        {
          cause= "nonconst_equality_gap_attribute";
          goto next_index;
        }
      }
      else if (min_max_arg_part &&
               (min_max_arg_part - first_non_group_part > 0))
      {
        /*
          There is a gap but no range tree, thus no predicates at all for the
          non-group keyparts.
        */
        cause= "no_nongroup_keypart_predicate";
        goto next_index;
      }
      else if (first_non_group_part && join->where_cond)
      {
        /*
          If there is no MIN/MAX function in the query, but some index
          key part is referenced in the WHERE clause, then this index
          cannot be used because the WHERE condition over the keypart's
          field cannot be 'pushed' to the index (because there is no
          range 'tree'), and the WHERE clause must be evaluated before
          GROUP BY/DISTINCT.
        */
        /*
          Store the first and last keyparts that need to be analyzed
          into one array that can be passed as parameter.
        */
        KEY_PART_INFO *key_part_range[2];
        key_part_range[0]= first_non_group_part;
        key_part_range[1]= last_part;

        /* Check if cur_part is referenced in the WHERE clause. */
        if (join->where_cond->walk(&Item::find_item_in_field_list_processor,
                                   Item::WALK_SUBQUERY_POSTFIX,
                                   (uchar*) key_part_range))
        {
          cause= "keypart_reference_from_where_clause";
          goto next_index;
        }
      }
    }

    /*
      Test (WA1) partially - that no other keypart after the last infix part is
      referenced in the query.
    */
    if (first_non_infix_part)
    {
      cur_part= first_non_infix_part +
        (min_max_arg_part && (min_max_arg_part < last_part));
      for (; cur_part != last_part; cur_part++)
      {
        if (bitmap_is_set(table->read_set, cur_part->field->field_index))
        {
          cause= "keypart_after_infix_in_query";
          goto next_index;
        }
      }
    }

    /**
      Test Part of WA2:If there are conditions on a column C participating in
      MIN/MAX, those conditions must be conjunctions to all earlier
      keyparts. Otherwise, Loose Index Scan cannot be used.
    */
    if (tree && min_max_arg_item)
    {
      SEL_ARG *index_range_tree= get_index_range_tree(cur_index, tree, param);
      SEL_ARG *cur_range= NULL;
      if (get_sel_arg_for_keypart(min_max_arg_part->field,
                                  index_range_tree, &cur_range) ||
          (cur_range && cur_range->type != SEL_ARG::KEY_RANGE))
      {
        cause= "minmax_keypart_in_disjunctive_query";
        goto next_index;
      }
    }

    /* If we got to this point, cur_index_info passes the test. */
    key_infix_parts= cur_key_infix_len ? (uint) 
      (first_non_infix_part - first_non_group_part) : 0;
    cur_used_key_parts= cur_group_key_parts + key_infix_parts;

    /* Compute the cost of using this index. */
    if (tree)
    {
      /* Find the SEL_ARG sub-tree that corresponds to the chosen index. */
      cur_index_tree= get_index_range_tree(cur_index, tree, param);
      /* Check if this range tree can be used for prefix retrieval. */
      Cost_estimate dummy_cost;
      uint mrr_flags= HA_MRR_SORTED;
      uint mrr_bufsize=0;
      cur_quick_prefix_records= check_quick_select(param, cur_param_idx, 
                                                   FALSE /*don't care*/, 
                                                   cur_index_tree, TRUE,
                                                   &mrr_flags, &mrr_bufsize,
                                                   &dummy_cost);
#ifdef OPTIMIZER_TRACE
      if (unlikely(cur_index_tree && trace->is_started()))
      {
        trace_idx.add("index_dives_for_eq_ranges", !param->use_index_statistics);
        Opt_trace_array trace_range(trace, "ranges");

        const KEY_PART_INFO *key_part= cur_index_info->key_part;

        String range_info;
        range_info.set_charset(system_charset_info);
        append_range_all_keyparts(&trace_range, NULL, &range_info,
                                  cur_index_tree, key_part, false);
      }
#endif
    }
    cost_group_min_max(table, cur_index, cur_used_key_parts,
                       cur_group_key_parts, tree, cur_index_tree,
                       cur_quick_prefix_records, have_min, have_max,
                       &cur_read_cost, &cur_records);
    /*
      If cur_read_cost is lower than best_read_cost use cur_index.
      Do not compare doubles directly because they may have different
      representations (64 vs. 80 bits).
    */
    trace_idx.add("rows", cur_records).add("cost", cur_read_cost);
    {
      Cost_estimate min_diff_cost= cur_read_cost;
      min_diff_cost.multiply(DBL_EPSILON);
      if (cur_read_cost < (best_read_cost - min_diff_cost))
      {
        index_info= cur_index_info;
        index= cur_index;
        best_read_cost= cur_read_cost;
        best_records= cur_records;
        best_index_tree= cur_index_tree;
        best_quick_prefix_records= cur_quick_prefix_records;
        best_param_idx= cur_param_idx;
        group_key_parts= cur_group_key_parts;
        group_prefix_len= cur_group_prefix_len;
        key_infix_len= cur_key_infix_len;
        if (key_infix_len)
          memcpy (key_infix, cur_key_infix, sizeof (key_infix));
        used_key_parts= cur_used_key_parts;
      }
    }

  next_index:
    if (cause)
    {
      trace_idx.add("usable", false).add_alnum("cause", cause);
      cause= NULL;
    }
  }
  trace_indexes.end();

  if (!index_info) /* No usable index found. */
    DBUG_RETURN(NULL);

  /* Check (SA3) for the where clause. */
  if (join->where_cond && min_max_arg_item &&
      !check_group_min_max_predicates(join->where_cond, min_max_arg_item,
                                      (index_info->flags & HA_SPATIAL) ?
                                      Field::itMBR : Field::itRAW))
  {
    trace_group.add("usable", false).
      add_alnum("cause", "unsupported_predicate_on_agg_attribute");
    DBUG_RETURN(NULL);
  }

  /* The query passes all tests, so construct a new TRP object. */
  read_plan= new (param->mem_root)
                 TRP_GROUP_MIN_MAX(have_min, have_max, is_agg_distinct,
                                   min_max_arg_part,
                                   group_prefix_len, used_key_parts,
                                   group_key_parts, index_info, index,
                                   key_infix_len,
                                   (key_infix_len > 0) ? key_infix : NULL,
                                   tree, best_index_tree, best_param_idx,
                                   best_quick_prefix_records);
  if (read_plan)
  {
    if (tree && read_plan->quick_prefix_records == 0)
      DBUG_RETURN(NULL);

    read_plan->cost_est= best_read_cost;
    read_plan->records=   best_records;
    if (*cost_est < best_read_cost &&
        is_agg_distinct)
    {
      trace_group.add("index_scan", true);
      read_plan->cost_est.reset();
      read_plan->use_index_scan();
    }

    DBUG_PRINT("info",
               ("Returning group min/max plan: cost: %g, records: %lu",
                read_plan->cost_est.total_cost(), (ulong) read_plan->records));
  }

  DBUG_RETURN(read_plan);
}


/*
  Check that the MIN/MAX attribute participates only in range predicates
  with constants.

  SYNOPSIS
    check_group_min_max_predicates()
    cond              tree (or subtree) describing all or part of the WHERE
                      clause being analyzed
    min_max_arg_item  the field referenced by the MIN/MAX function(s)
    min_max_arg_part  the keypart of the MIN/MAX argument if any

  DESCRIPTION
    The function walks recursively over the cond tree representing a WHERE
    clause, and checks condition (SA3) - if a field is referenced by a MIN/MAX
    aggregate function, it is referenced only by one of the following
    predicates: {=, !=, <, <=, >, >=, between, is null, is not null}.

  RETURN
    TRUE  if cond passes the test
    FALSE o/w
*/

static bool
check_group_min_max_predicates(Item *cond, Item_field *min_max_arg_item,
                               Field::imagetype image_type)
{
  DBUG_ENTER("check_group_min_max_predicates");
  DBUG_ASSERT(cond && min_max_arg_item);

  cond= cond->real_item();
  Item::Type cond_type= cond->type();
  if (cond_type == Item::COND_ITEM) /* 'AND' or 'OR' */
  {
    DBUG_PRINT("info", ("Analyzing: %s", ((Item_func*) cond)->func_name()));
    List_iterator_fast<Item> li(*((Item_cond*) cond)->argument_list());
    Item *and_or_arg;
    while ((and_or_arg= li++))
    {
      if (!check_group_min_max_predicates(and_or_arg, min_max_arg_item,
                                         image_type))
        DBUG_RETURN(FALSE);
    }
    DBUG_RETURN(TRUE);
  }

  /*
    TODO:
    This is a very crude fix to handle sub-selects in the WHERE clause
    (Item_subselect objects). With the test below we rule out from the
    optimization all queries with subselects in the WHERE clause. What has to
    be done, is that here we should analyze whether the subselect references
    the MIN/MAX argument field, and disallow the optimization only if this is
    so.
    Need to handle subselect in min_max_inspect_cond_for_fields() once this
    is fixed.
  */
  if (cond_type == Item::SUBSELECT_ITEM)
    DBUG_RETURN(FALSE);

  /*
    Condition of the form 'field' is equivalent to 'field <> 0' and thus
    satisfies the SA3 condition.
  */
  if (cond_type == Item::FIELD_ITEM)
  {
    DBUG_PRINT("info", ("Analyzing: %s", cond->full_name()));
    DBUG_RETURN(TRUE);
  }

  /*
    At this point, we have weeded out most conditions other than
    function items. However, there are cases like the following:

      select 1 in (select max(c) from t1 where max(1) group by a)

    Here the condition "where max(1)" is an Item_sum_max, not an
    Item_func. In this particular case, the where clause should
    be equivalent to "where max(1) <> 0". A where clause
    phrased that way does not satisfy the SA3 condition of
    get_best_group_min_max(). The "where max(1) = true" clause
    causes this method to reject the access method
    (i.e., to return FALSE).

    It's been suggested that it may be possible to use the access method
    for a sub-family of cases when we're aggregating constants or
    outer references. For the moment, we bale out and we reject
    the access method for the query.

    It's hard to prove that there are no other cases where the
    condition is not an Item_func. So, for the moment, don't apply
    the optimization if the condition is not a function item.
  */
  if (cond_type == Item::SUM_FUNC_ITEM)
  {
    DBUG_RETURN(FALSE);
  }

  /*
   If this is a debug server, then we want to know about
   additional oddball cases which might benefit from this
   optimization.
  */
  DBUG_ASSERT(cond_type == Item::FUNC_ITEM);
  if (cond_type != Item::FUNC_ITEM)
  {
    DBUG_RETURN(FALSE);
  }

  /* Test if cond references only group-by or non-group fields. */
  Item_func *pred= (Item_func*) cond;
  Item *cur_arg;
  DBUG_PRINT("info", ("Analyzing: %s", pred->func_name()));
  for (uint arg_idx= 0; arg_idx < pred->argument_count (); arg_idx++)
  {
    Item **arguments= pred->arguments();
    cur_arg= arguments[arg_idx]->real_item();
    DBUG_PRINT("info", ("cur_arg: %s", cur_arg->full_name()));
    if (cur_arg->type() == Item::FIELD_ITEM)
    {
      if (min_max_arg_item->eq(cur_arg, 1)) 
      {
       /*
         If pred references the MIN/MAX argument, check whether pred is a range
         condition that compares the MIN/MAX argument with a constant.
       */
        Item_func::Functype pred_type= pred->functype();
        if (pred_type != Item_func::EQUAL_FUNC     &&
            pred_type != Item_func::LT_FUNC        &&
            pred_type != Item_func::LE_FUNC        &&
            pred_type != Item_func::GT_FUNC        &&
            pred_type != Item_func::GE_FUNC        &&
            pred_type != Item_func::BETWEEN        &&
            pred_type != Item_func::ISNULL_FUNC    &&
            pred_type != Item_func::ISNOTNULL_FUNC &&
            pred_type != Item_func::EQ_FUNC        &&
            pred_type != Item_func::NE_FUNC)
          DBUG_RETURN(FALSE);

        /* Check that pred compares min_max_arg_item with a constant. */
        Item *args[3];
        memset(args, 0, 3 * sizeof(Item*));
        bool inv;
        /* Test if this is a comparison of a field and a constant. */
        if (!simple_pred(pred, args, &inv))
          DBUG_RETURN(FALSE);

        /* Check for compatible string comparisons - similar to get_mm_leaf. */
        if (args[0] && args[1] && !args[2] && // this is a binary function
            min_max_arg_item->result_type() == STRING_RESULT &&
            /*
              Don't use an index when comparing strings of different collations.
            */
            ((args[1]->result_type() == STRING_RESULT &&
              image_type == Field::itRAW &&
              min_max_arg_item->field->charset() != pred->compare_collation())
             ||
             /*
               We can't always use indexes when comparing a string index to a
               number.
             */
             (args[1]->result_type() != STRING_RESULT &&
              min_max_arg_item->field->cmp_type() != args[1]->result_type())))
          DBUG_RETURN(FALSE);
      }
    }
    else if (cur_arg->type() == Item::FUNC_ITEM)
    {
      if (!check_group_min_max_predicates(cur_arg, min_max_arg_item,
                                         image_type))
        DBUG_RETURN(FALSE);
    }
    else if (cur_arg->const_item())
    {
      /*
        For predicates of the form "const OP expr" we also have to check 'expr'
        to make a decision.
      */
      continue;
    }
    else
      DBUG_RETURN(FALSE);
  }

  DBUG_RETURN(TRUE);
}

/**
  Utility function used by min_max_inspect_cond_for_fields() for comparing
  FILED item with given MIN/MAX item and setting appropriate out paramater.

@param         item_field         Item field for comparison.
@param         min_max_arg_item   The field referenced by the MIN/MAX
                                  function(s).
@param [out]   min_max_arg_present    This out parameter is set to true if
                                      MIN/MAX argument is present in cond.
@param [out]   non_min_max_arg_present This out parameter is set to true if
                                       any field item other than MIN/MAX
                                       argument is present in cond.
*/
static inline void util_min_max_inspect_item(Item *item_field,
                                             Item_field *min_max_arg_item,
                                             bool *min_max_arg_present,
                                             bool *non_min_max_arg_present)
{
  if (item_field->type() == Item::FIELD_ITEM)
  {
    if(min_max_arg_item->eq(item_field, 1))
      *min_max_arg_present= true;
    else
      *non_min_max_arg_present= true;
  }
}

/**
  This function detects the presents of MIN/MAX field along with at least
  one non MIN/MAX field participation in the given condition. Subqueries
  inspection is skipped as of now.

  @param         cond   tree (or subtree) describing all or part of the WHERE
                        clause being analyzed.
  @param         min_max_arg_item   The field referenced by the MIN/MAX
                                    function(s).
  @param [out]   min_max_arg_present    This out parameter is set to true if
                                        MIN/MAX argument is present in cond.
  @param [out]   non_min_max_arg_present This out parameter is set to true if
                                         any field item other than MIN/MAX
                                         argument is present in cond.

  @return  TRUE if both MIN/MAX field and non MIN/MAX field is present in cond.
           FALSE o/w.

  @todo: When the hack present in check_group_min_max_predicate() is removed,
         subqueries needs to be inspected.
*/

static bool
min_max_inspect_cond_for_fields(Item *cond, Item_field *min_max_arg_item,
                                bool *min_max_arg_present,
                                bool *non_min_max_arg_present)
{
  DBUG_ENTER("inspect_cond_for_fields");
  DBUG_ASSERT(cond && min_max_arg_item);

  cond= cond->real_item();
  Item::Type cond_type= cond->type();

  switch (cond_type)  {
    case Item::COND_ITEM:  {
      DBUG_PRINT("info", ("Analyzing: %s", ((Item_func*) cond)->func_name()));
      List_iterator_fast<Item> li(*((Item_cond*) cond)->argument_list());
      Item *and_or_arg;
      while ((and_or_arg= li++))
      {
        min_max_inspect_cond_for_fields(and_or_arg, min_max_arg_item,
                                            min_max_arg_present,
                                            non_min_max_arg_present);
        if (*min_max_arg_present && *non_min_max_arg_present)
          DBUG_RETURN(true);
      }

      DBUG_RETURN(false);
    }
    case Item::FUNC_ITEM:  {
      /* Test if cond references both group-by and non-group fields. */
      Item_func *pred= (Item_func*) cond;
      Item *cur_arg;
      DBUG_PRINT("info", ("Analyzing: %s", pred->func_name()));
      for (uint arg_idx= 0; arg_idx < pred->argument_count(); arg_idx++)
      {
        Item **arguments= pred->arguments();
        cur_arg= arguments[arg_idx]->real_item();
        DBUG_PRINT("info", ("cur_arg: %s", cur_arg->full_name()));

        if (cur_arg->type() == Item::FUNC_ITEM)
        {
          min_max_inspect_cond_for_fields(cur_arg, min_max_arg_item,
                                              min_max_arg_present,
                                              non_min_max_arg_present);
        }
        else
        {
          util_min_max_inspect_item(cur_arg,
                                    min_max_arg_item,
                                    min_max_arg_present,
                                    non_min_max_arg_present);
        }

        if (*min_max_arg_present && *non_min_max_arg_present)
          DBUG_RETURN(true);
      }

      if (pred->functype() == Item_func::MULT_EQUAL_FUNC)
      {
        /*
          Analyze participating fields in a multiequal condition.
        */
        Item_equal_iterator it(*(Item_equal*)cond);

        Item *item_field;
        while ((item_field= it++))
        {
          util_min_max_inspect_item(item_field,
                                    min_max_arg_item,
                                    min_max_arg_present,
                                    non_min_max_arg_present);

          if (*min_max_arg_present && *non_min_max_arg_present)
            DBUG_RETURN(true);
        }
      }

      break;
    }
    case Item::FIELD_ITEM:  {
      util_min_max_inspect_item(cond,
                                min_max_arg_item,
                                min_max_arg_present,
                                non_min_max_arg_present);
      DBUG_PRINT("info", ("Analyzing: %s", cond->full_name()));
      DBUG_RETURN(false);
    }
    default:
      break;
  }

  DBUG_RETURN(false);
}

/*
  Get the SEL_ARG tree 'tree' for the keypart covering 'field', if
  any. 'tree' must be a unique conjunction to ALL predicates in earlier
  keyparts of 'keypart_tree'.

  E.g., if 'keypart_tree' is for a composite index (kp1,kp2) and kp2
  covers 'field', all these conditions satisfies the requirement:

   1. "(kp1=2 OR kp1=3) AND kp2=10"    => returns "kp2=10"
   2. "(kp1=2 AND kp2=10) OR (kp1=3 AND kp2=10)"  => returns "kp2=10"
   3. "(kp1=2 AND (kp2=10 OR kp2=11)) OR (kp1=3 AND (kp2=10 OR kp2=11))"
                                       => returns "kp2=10  OR kp2=11"

   whereas these do not
   1. "(kp1=2 AND kp2=10) OR kp1=3"
   2. "(kp1=2 AND kp2=10) OR (kp1=3 AND kp2=11)"
   3. "(kp1=2 AND kp2=10) OR (kp1=3 AND (kp2=10 OR kp2=11))"

   This function effectively tests requirement WA2. In combination with
   a test that the returned tree has no more than one range it is also
   a test of NGA3.

  @param[in]   field          The field we want the SEL_ARG tree for
  @param[in]   keypart_tree   Root node of the SEL_ARG* tree for the index
  @param[out]  cur_range      The SEL_ARG tree, if any, for the keypart
                              covering field 'keypart_field'
  @retval true   'keypart_tree' contained a predicate for 'field' that
                  is not conjunction to all predicates on earlier keyparts
  @retval false  otherwise
*/

static bool
get_sel_arg_for_keypart(Field *field,
                        SEL_ARG *keypart_tree,
                        SEL_ARG **cur_range)
{
  if (keypart_tree == NULL)
    return false;
  if (keypart_tree->type != SEL_ARG::KEY_RANGE)
  {
    /*
      A range predicate not usable by Loose Index Scan is found.
      Predicates for keypart 'keypart_tree->part' and later keyparts
      cannot be used.
    */
    *cur_range= keypart_tree;
    return false;
  }
  if (keypart_tree->field->eq(field))
  {
    *cur_range= keypart_tree;
    return false;
  }

  SEL_ARG *tree_first_range= NULL;
  SEL_ARG *first_kp=  keypart_tree->first();

  for (SEL_ARG *cur_kp= first_kp; cur_kp; cur_kp= cur_kp->next)
  {
    SEL_ARG *curr_tree= NULL;
    if (cur_kp->next_key_part)
    {
      if (get_sel_arg_for_keypart(field,
                                  cur_kp->next_key_part,
                                  &curr_tree))
        return true;
    }
    /**
      Check if the SEL_ARG tree for 'field' is identical for all ranges in
      'keypart_tree
     */
    if (cur_kp == first_kp)
      tree_first_range= curr_tree;
    else if (!all_same(tree_first_range, curr_tree))
      return true;
  }
  *cur_range= tree_first_range;
  return false;
}

/*
  Extract a sequence of constants from a conjunction of equality predicates.

  SYNOPSIS
    get_constant_key_infix()
    index_info             [in]  Descriptor of the chosen index.
    index_range_tree       [in]  Range tree for the chosen index
    first_non_group_part   [in]  First index part after group attribute parts
    min_max_arg_part       [in]  The keypart of the MIN/MAX argument if any
    last_part              [in]  Last keypart of the index
    thd                    [in]  Current thread
    key_infix              [out] Infix of constants to be used for index lookup
    key_infix_len          [out] Lenghth of the infix
    first_non_infix_part   [out] The first keypart after the infix (if any)

  DESCRIPTION
    Test conditions (NGA1, NGA2) from get_best_group_min_max(). Namely,
    for each keypart field NGF_i not in GROUP-BY, check that there is a
    constant equality predicate among conds with the form (NGF_i = const_ci) or
    (const_ci = NGF_i).
    Thus all the NGF_i attributes must fill the 'gap' between the last group-by
    attribute and the MIN/MAX attribute in the index (if present).  Also ensure
    that there is only a single range on NGF_i (NGA3). If these
    conditions hold, copy each constant from its corresponding predicate into
    key_infix, in the order its NG_i attribute appears in the index, and update
    key_infix_len with the total length of the key parts in key_infix.

  RETURN
    TRUE  if the index passes the test
    FALSE o/w
*/
static bool
get_constant_key_infix(KEY *index_info, SEL_ARG *index_range_tree,
                       KEY_PART_INFO *first_non_group_part,
                       KEY_PART_INFO *min_max_arg_part,
                       KEY_PART_INFO *last_part, THD *thd,
                       uchar *key_infix, uint *key_infix_len,
                       KEY_PART_INFO **first_non_infix_part)
{
  SEL_ARG       *cur_range;
  KEY_PART_INFO *cur_part;
  /* End part for the first loop below. */
  KEY_PART_INFO *end_part= min_max_arg_part ? min_max_arg_part : last_part;

  *key_infix_len= 0;
  uchar *key_ptr= key_infix;
  for (cur_part= first_non_group_part; cur_part != end_part; cur_part++)
  {
    cur_range= NULL;
    /*
      Check NGA3:
      1. get_sel_arg_for_keypart gets the range tree for the 'field' and also
         checks for a unique conjunction of this tree with all the predicates
         on the earlier keyparts in the index.
      2. Check for multiple ranges on the found keypart tree.

      We assume that index_range_tree points to the leftmost keypart in
      the index.
    */
    if (get_sel_arg_for_keypart(cur_part->field, index_range_tree,
                                &cur_range))
      return false;

    if (cur_range && cur_range->elements > 1)
      return false;

    if (!cur_range || cur_range->type != SEL_ARG::KEY_RANGE)
    {
      if (min_max_arg_part)
        return false; /* The current keypart has no range predicates at all. */
      else
      {
        *first_non_infix_part= cur_part;
        return true;
      }
    }

    if ((cur_range->min_flag & NO_MIN_RANGE) ||
        (cur_range->max_flag & NO_MAX_RANGE) ||
        (cur_range->min_flag & NEAR_MIN) || (cur_range->max_flag & NEAR_MAX))
      return false;

    uint field_length= cur_part->store_length;
    if (cur_range->maybe_null &&
         cur_range->min_value[0] && cur_range->max_value[0])
    {
      /*
        cur_range specifies 'IS NULL'. In this case the argument points
        to a "null value" (a copy of is_null_string) that we do not
        memcmp(), or memcpy to a field.
      */
      DBUG_ASSERT (field_length > 0);
      *key_ptr= 1;
      key_ptr+= field_length;
      *key_infix_len+= field_length;
    }
    else if (memcmp(cur_range->min_value, cur_range->max_value, field_length) == 0)
    { /* cur_range specifies an equality condition. */
      memcpy(key_ptr, cur_range->min_value, field_length);
      key_ptr+= field_length;
      *key_infix_len+= field_length;
    }
    else
      return false;
  }

  if (!min_max_arg_part && (cur_part == last_part))
    *first_non_infix_part= last_part;

  return TRUE;
}


/*
  Find the key part referenced by a field.

  SYNOPSIS
    get_field_keypart()
    index  descriptor of an index
    field  field that possibly references some key part in index

  NOTES
    The return value can be used to get a KEY_PART_INFO pointer by
    part= index->key_part + get_field_keypart(...) - 1;

  RETURN
    Positive number which is the consecutive number of the key part, or
    0 if field does not reference any index field.
*/

static inline uint
get_field_keypart(KEY *index, Field *field)
{
  KEY_PART_INFO *part, *end;

  for (part= index->key_part, end= part + actual_key_parts(index) ;
       part < end; part++)
  {
    if (field->eq(part->field))
      return part - index->key_part + 1;
  }
  return 0;
}


/*
  Find the SEL_ARG sub-tree that corresponds to the chosen index.

  SYNOPSIS
    get_index_range_tree()
    index     [in]  The ID of the index being looked for
    range_tree[in]  Tree of ranges being searched
    param     [in]  PARAM from test_quick_select

  DESCRIPTION

    A SEL_TREE contains range trees for all usable indexes. This procedure
    finds the SEL_ARG sub-tree for 'index'. The members of a SEL_TREE are
    ordered in the same way as the members of PARAM::key, thus we first find
    the corresponding index in the array PARAM::key. This index is returned
    through the variable param_idx, to be used later as argument of
    check_quick_select().

  RETURN
    Pointer to the SEL_ARG subtree that corresponds to index.
*/

SEL_ARG * get_index_range_tree(uint index, SEL_TREE* range_tree, PARAM *param)
{
  uint idx= 0; /* Index nr in param->key_parts */
  while (idx < param->keys)
  {
    if (index == param->real_keynr[idx])
      break;
    idx++;
  }
  return(range_tree->keys[idx]);
}


/*
  Compute the cost of a quick_group_min_max_select for a particular index.

  SYNOPSIS
    cost_group_min_max()
    table                [in] The table being accessed
    key                  [in] The index used to access the table
    used_key_parts       [in] Number of key parts used to access the index
    group_key_parts      [in] Number of index key parts in the group prefix
    range_tree           [in] Tree of ranges for all indexes
    index_tree           [in] The range tree for the current index
    quick_prefix_records [in] Number of records retrieved by the internally
			      used quick range select if any
    have_min             [in] True if there is a MIN function
    have_max             [in] True if there is a MAX function
    cost_est            [out] The cost to retrieve rows via this quick select
    records             [out] The number of rows retrieved

  DESCRIPTION
    This method computes the access cost of a TRP_GROUP_MIN_MAX instance and
    the number of rows returned.

  NOTES
    The cost computation distinguishes several cases:
    1) No equality predicates over non-group attributes (thus no key_infix).
       If groups are bigger than blocks on the average, then we assume that it
       is very unlikely that block ends are aligned with group ends, thus even
       if we look for both MIN and MAX keys, all pairs of neighbor MIN/MAX
       keys, except for the first MIN and the last MAX keys, will be in the
       same block.  If groups are smaller than blocks, then we are going to
       read all blocks.
    2) There are equality predicates over non-group attributes.
       In this case the group prefix is extended by additional constants, and
       as a result the min/max values are inside sub-groups of the original
       groups. The number of blocks that will be read depends on whether the
       ends of these sub-groups will be contained in the same or in different
       blocks. We compute the probability for the two ends of a subgroup to be
       in two different blocks as the ratio of:
       - the number of positions of the left-end of a subgroup inside a group,
         such that the right end of the subgroup is past the end of the buffer
         containing the left-end, and
       - the total number of possible positions for the left-end of the
         subgroup, which is the number of keys in the containing group.
       We assume it is very unlikely that two ends of subsequent subgroups are
       in the same block.
    3) The are range predicates over the group attributes.
       Then some groups may be filtered by the range predicates. We use the
       selectivity of the range predicates to decide how many groups will be
       filtered.

  TODO
     - Take into account the optional range predicates over the MIN/MAX
       argument.
     - Check if we have a PK index and we use all cols - then each key is a
       group, and it will be better to use an index scan.

  RETURN
    None
*/

void cost_group_min_max(TABLE* table, uint key, uint used_key_parts,
                        uint group_key_parts, SEL_TREE *range_tree,
                        SEL_ARG *index_tree, ha_rows quick_prefix_records,
                        bool have_min, bool have_max,
                        Cost_estimate *cost_est, ha_rows *records)
{
  ha_rows table_records;
  uint num_groups;
  uint num_blocks;
  uint keys_per_block;
  rec_per_key_t keys_per_group;
  double p_overlap; /* Probability that a sub-group overlaps two blocks. */
  double quick_prefix_selectivity;
  double io_blocks;       // Number of blocks to read from table
  DBUG_ENTER("cost_group_min_max");
  DBUG_ASSERT(cost_est->is_zero());

  const KEY *const index_info= &table->key_info[key];
  table_records= table->file->stats.records;
  keys_per_block= (table->file->stats.block_size / 2 /
                   (index_info->key_length + table->file->ref_length)
                        + 1);
  num_blocks= (uint)(table_records / keys_per_block) + 1;

  /* Compute the number of keys in a group. */
  if (index_info->has_records_per_key(group_key_parts - 1))
    // Use index statistics
    keys_per_group= index_info->records_per_key(group_key_parts - 1);
  else
    /* If there is no statistics try to guess */
    keys_per_group= guess_rec_per_key(table, index_info, group_key_parts);

  num_groups= (uint)(table_records / keys_per_group) + 1;

  /* Apply the selectivity of the quick select for group prefixes. */
  if (range_tree && (quick_prefix_records != HA_POS_ERROR))
  {
    quick_prefix_selectivity= (double) quick_prefix_records /
                              (double) table_records;
    num_groups= (uint) rint(num_groups * quick_prefix_selectivity);
    set_if_bigger(num_groups, 1);
  }

  if (used_key_parts > group_key_parts)
  {
    // Average number of keys in sub-groups formed by a key infix
    rec_per_key_t keys_per_subgroup;
    if (index_info->has_records_per_key(used_key_parts - 1))
      // Use index statistics
      keys_per_subgroup= index_info->records_per_key(used_key_parts - 1);
    else
    {
      // If no index statistics then we use a guessed records per key value.
      keys_per_subgroup= guess_rec_per_key(table, index_info, used_key_parts);
      set_if_smaller(keys_per_subgroup, keys_per_group);
    }

    /*
      Compute the probability that two ends of a subgroup are inside
      different blocks.
    */
    if (keys_per_subgroup >= keys_per_block) /* If a subgroup is bigger than */
      p_overlap= 1.0;       /* a block, it will overlap at least two blocks. */
    else
    {
      double blocks_per_group= (double) num_blocks / (double) num_groups;
      p_overlap= (blocks_per_group * (keys_per_subgroup - 1)) / keys_per_group;
      p_overlap= min(p_overlap, 1.0);
    }
    io_blocks= min<double>(num_groups * (1 + p_overlap), num_blocks);
  }
  else
    io_blocks= (keys_per_group > keys_per_block) ?
               (have_min && have_max) ? (double) (num_groups + 1) :
                                        (double) num_groups :
               (double) num_blocks;

  /*
    Estimate IO cost.
  */
  const Cost_model_table *const cost_model= table->cost_model();
  cost_est->add_io(cost_model->page_read_cost_index(key, io_blocks));

  /*
    CPU cost must be comparable to that of an index scan as computed
    in test_quick_select(). When the groups are small,
    e.g. for a unique index, using index scan will be cheaper since it
    reads the next record without having to re-position to it on every
    group. To make the CPU cost reflect this, we estimate the CPU cost
    as the sum of:
    1. Cost for evaluating the condition (similarly as for index scan).
    2. Cost for navigating the index structure (assuming a b-tree).
       Note: We only add the cost for one comparision per block. For a
             b-tree the number of comparisons will be larger.
       TODO: This cost should be provided by the storage engine.
  */
  const double tree_height= table_records == 0 ?
                            1.0 :
                            ceil(log(double(table_records)) /
                                 log(double(keys_per_block)));
  const double tree_traversal_cost= cost_model->key_compare_cost(tree_height);

  const double cpu_cost= num_groups * (tree_traversal_cost +
                                       cost_model->row_evaluate_cost(1.0));
  cost_est->add_cpu(cpu_cost);
  *records= num_groups;

  DBUG_PRINT("info",
             ("table rows: %lu  keys/block: %u  keys/group: %.1f  result rows: %lu  blocks: %u",
              (ulong)table_records, keys_per_block, keys_per_group, 
              (ulong) *records, num_blocks));
  DBUG_VOID_RETURN;
}


/*
  Construct a new quick select object for queries with group by with min/max.

  SYNOPSIS
    TRP_GROUP_MIN_MAX::make_quick()
    param              Parameter from test_quick_select
    retrieve_full_rows ignored
    parent_alloc       Memory pool to use, if any.

  NOTES
    Make_quick ignores the retrieve_full_rows parameter because
    QUICK_GROUP_MIN_MAX_SELECT always performs 'index only' scans.
    The other parameter are ignored as well because all necessary
    data to create the QUICK object is computed at this TRP creation
    time.

  RETURN
    New QUICK_GROUP_MIN_MAX_SELECT object if successfully created,
    NULL otherwise.
*/

QUICK_SELECT_I *
TRP_GROUP_MIN_MAX::make_quick(PARAM *param, bool retrieve_full_rows,
                              MEM_ROOT *parent_alloc)
{
  QUICK_GROUP_MIN_MAX_SELECT *quick;
  DBUG_ENTER("TRP_GROUP_MIN_MAX::make_quick");

  quick= new QUICK_GROUP_MIN_MAX_SELECT(param->table,
                                        param->thd->lex->current_select()->join,
                                        have_min, have_max, 
                                        have_agg_distinct, min_max_arg_part,
                                        group_prefix_len, group_key_parts,
                                        used_key_parts, index_info, index,
                                        &cost_est, records, key_infix_len,
                                        key_infix, parent_alloc, is_index_scan);
  if (!quick)
    DBUG_RETURN(NULL);

  if (quick->init())
  {
    delete quick;
    DBUG_RETURN(NULL);
  }

  if (range_tree)
  {
    DBUG_ASSERT(quick_prefix_records > 0);
    if (quick_prefix_records == HA_POS_ERROR)
      quick->quick_prefix_select= NULL; /* Can't construct a quick select. */
    else
    {
      /* Make a QUICK_RANGE_SELECT to be used for group prefix retrieval. */
      quick->quick_prefix_select= get_quick_select(param, param_idx,
                                                   index_tree,
                                                   HA_MRR_SORTED,
                                                   0,
                                                   &quick->alloc);
      if (!quick->quick_prefix_select)
      {
        delete quick;
        DBUG_RETURN(NULL);
      }
    }
    /*
      Extract the SEL_ARG subtree that contains only ranges for the MIN/MAX
      attribute, and create an array of QUICK_RANGES to be used by the
      new quick select.
    */
    if (min_max_arg_part)
    {
      SEL_ARG *min_max_range= index_tree;
      while (min_max_range) /* Find the tree for the MIN/MAX key part. */
      {
        if (min_max_range->field->eq(min_max_arg_part->field))
          break;
        min_max_range= min_max_range->next_key_part;
      }
      /* Scroll to the leftmost interval for the MIN/MAX argument. */
      while (min_max_range && min_max_range->prev)
        min_max_range= min_max_range->prev;
      /* Create an array of QUICK_RANGEs for the MIN/MAX argument. */
      while (min_max_range)
      {
        if (quick->add_range(min_max_range))
        {
          delete quick;
          quick= NULL;
          DBUG_RETURN(NULL);
        }
        min_max_range= min_max_range->next;
      }
    }
  }
  else
    quick->quick_prefix_select= NULL;

  quick->update_key_stat();
  quick->adjust_prefix_ranges();

  DBUG_RETURN(quick);
}


/*
  Construct new quick select for group queries with min/max.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::QUICK_GROUP_MIN_MAX_SELECT()
    table             The table being accessed
    join              Descriptor of the current query
    have_min          TRUE if the query selects a MIN function
    have_max          TRUE if the query selects a MAX function
    min_max_arg_part  The only argument field of all MIN/MAX functions
    group_prefix_len  Length of all key parts in the group prefix
    prefix_key_parts  All key parts in the group prefix
    index_info        The index chosen for data access
    use_index         The id of index_info
    read_cost         Cost of this access method
    records           Number of records returned
    key_infix_len     Length of the key infix appended to the group prefix
    key_infix         Infix of constants from equality predicates
    parent_alloc      Memory pool for this and quick_prefix_select data
    is_index_scan     get the next different key not by jumping on it via
                      index read, but by scanning until the end of the 
                      rows with equal key value.

  RETURN
    None
*/

QUICK_GROUP_MIN_MAX_SELECT::
QUICK_GROUP_MIN_MAX_SELECT(TABLE *table, JOIN *join_arg, bool have_min_arg,
                           bool have_max_arg, bool have_agg_distinct_arg,
                           KEY_PART_INFO *min_max_arg_part_arg,
                           uint group_prefix_len_arg, uint group_key_parts_arg,
                           uint used_key_parts_arg, KEY *index_info_arg,
                           uint use_index, const Cost_estimate *read_cost_arg,
                           ha_rows records_arg, uint key_infix_len_arg,
                           uchar *key_infix_arg, MEM_ROOT *parent_alloc,
                           bool is_index_scan_arg)
  :join(join_arg), index_info(index_info_arg),
   group_prefix_len(group_prefix_len_arg),
   group_key_parts(group_key_parts_arg), have_min(have_min_arg),
   have_max(have_max_arg), have_agg_distinct(have_agg_distinct_arg),
   seen_first_key(FALSE), min_max_arg_part(min_max_arg_part_arg),
   key_infix(key_infix_arg), key_infix_len(key_infix_len_arg),
   min_max_ranges(PSI_INSTRUMENT_ME),
   min_functions_it(NULL), max_functions_it(NULL), 
   is_index_scan(is_index_scan_arg)
{
  head=       table;
  index=      use_index;
  record=     head->record[0];
  tmp_record= head->record[1];
  cost_est= *read_cost_arg;
  records= records_arg;
  used_key_parts= used_key_parts_arg;
  real_key_parts= used_key_parts_arg;
  real_prefix_len= group_prefix_len + key_infix_len;
  group_prefix= NULL;
  min_max_arg_len= min_max_arg_part ? min_max_arg_part->store_length : 0;

  /*
    We can't have parent_alloc set as the init function can't handle this case
    yet.
  */
  DBUG_ASSERT(!parent_alloc);
  if (!parent_alloc)
  {
    init_sql_alloc(key_memory_quick_group_min_max_select_root,
                   &alloc, join->thd->variables.range_alloc_block_size, 0);
    join->thd->mem_root= &alloc;
  }
  else
    memset(&alloc, 0, sizeof(MEM_ROOT));  // ensure that it's not used
}


/*
  Do post-constructor initialization.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::init()
  
  DESCRIPTION
    The method performs initialization that cannot be done in the constructor
    such as memory allocations that may fail. It allocates memory for the
    group prefix and inifix buffers, and for the lists of MIN/MAX item to be
    updated during execution.

  RETURN
    0      OK
    other  Error code
*/

int QUICK_GROUP_MIN_MAX_SELECT::init()
{
  if (group_prefix) /* Already initialized. */
    return 0;

  if (!(last_prefix= (uchar*) alloc_root(&alloc, group_prefix_len)))
      return 1;
  /*
    We may use group_prefix to store keys with all select fields, so allocate
    enough space for it.
  */
  if (!(group_prefix= (uchar*) alloc_root(&alloc,
                                         real_prefix_len + min_max_arg_len)))
    return 1;

  if (key_infix_len > 0)
  {
    /*
      The memory location pointed to by key_infix will be deleted soon, so
      allocate a new buffer and copy the key_infix into it.
    */
    uchar *tmp_key_infix= (uchar*) alloc_root(&alloc, key_infix_len);
    if (!tmp_key_infix)
      return 1;
    memcpy(tmp_key_infix, this->key_infix, key_infix_len);
    this->key_infix= tmp_key_infix;
  }

  if (min_max_arg_part)
  {
    if (have_min)
    {
      if (!(min_functions= new List<Item_sum>))
        return 1;
    }
    else
      min_functions= NULL;
    if (have_max)
    {
      if (!(max_functions= new List<Item_sum>))
        return 1;
    }
    else
      max_functions= NULL;

    Item_sum *min_max_item;
    Item_sum **func_ptr= join->sum_funcs;
    while ((min_max_item= *(func_ptr++)))
    {
      if (have_min && (min_max_item->sum_func() == Item_sum::MIN_FUNC))
        min_functions->push_back(min_max_item);
      else if (have_max && (min_max_item->sum_func() == Item_sum::MAX_FUNC))
        max_functions->push_back(min_max_item);
    }

    if (have_min)
    {
      if (!(min_functions_it= new List_iterator<Item_sum>(*min_functions)))
        return 1;
    }

    if (have_max)
    {
      if (!(max_functions_it= new List_iterator<Item_sum>(*max_functions)))
        return 1;
    }
  }

  return 0;
}


QUICK_GROUP_MIN_MAX_SELECT::~QUICK_GROUP_MIN_MAX_SELECT()
{
  DBUG_ENTER("QUICK_GROUP_MIN_MAX_SELECT::~QUICK_GROUP_MIN_MAX_SELECT");
  if (head->file->inited)
    /*
      We may have used this object for index access during
      create_sort_index() and then switched to rnd access for the rest
      of execution. Since we don't do cleanup until now, we must call
      ha_*_end() for whatever is the current access method.
    */
    head->file->ha_index_or_rnd_end();

  free_root(&alloc,MYF(0));
  delete min_functions_it;
  delete max_functions_it;
  delete quick_prefix_select;
  DBUG_VOID_RETURN; 
}


/*
  Eventually create and add a new quick range object.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::add_range()
    sel_range  Range object from which a 

  NOTES
    Construct a new QUICK_RANGE object from a SEL_ARG object, and
    add it to the array min_max_ranges. If sel_arg is an infinite
    range, e.g. (x < 5 or x > 4), then skip it and do not construct
    a quick range.

  RETURN
    FALSE on success
    TRUE  otherwise
*/

bool QUICK_GROUP_MIN_MAX_SELECT::add_range(SEL_ARG *sel_range)
{
  QUICK_RANGE *range;
  uint range_flag= sel_range->min_flag | sel_range->max_flag;

  /* Skip (-inf,+inf) ranges, e.g. (x < 5 or x > 4). */
  if ((range_flag & NO_MIN_RANGE) && (range_flag & NO_MAX_RANGE))
    return FALSE;

  if (!(sel_range->min_flag & NO_MIN_RANGE) &&
      !(sel_range->max_flag & NO_MAX_RANGE))
  {
    if (sel_range->maybe_null &&
        sel_range->min_value[0] && sel_range->max_value[0])
      range_flag|= NULL_RANGE; /* IS NULL condition */
    /*
      Do not perform comparison if one of the argiment is NULL value.
    */
    else if (!sel_range->min_value[0] &&
             !sel_range->max_value[0] &&
             memcmp(sel_range->min_value, sel_range->max_value,
                    min_max_arg_len) == 0)
      range_flag|= EQ_RANGE;  /* equality condition */
  }
  range= new QUICK_RANGE(sel_range->min_value, min_max_arg_len,
                         make_keypart_map(sel_range->part),
                         sel_range->max_value, min_max_arg_len,
                         make_keypart_map(sel_range->part),
                         range_flag, HA_READ_INVALID);
  if (!range)
    return TRUE;
  if (min_max_ranges.push_back(range))
    return TRUE;
  return FALSE;
}


/*
  Opens the ranges if there are more conditions in quick_prefix_select than
  the ones used for jumping through the prefixes.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::adjust_prefix_ranges()

  NOTES
    quick_prefix_select is made over the conditions on the whole key.
    It defines a number of ranges of length x. 
    However when jumping through the prefixes we use only the the first 
    few most significant keyparts in the range key. However if there
    are more keyparts to follow the ones we are using we must make the 
    condition on the key inclusive (because x < "ab" means 
    x[0] < 'a' OR (x[0] == 'a' AND x[1] < 'b').
    To achive the above we must turn off the NEAR_MIN/NEAR_MAX
*/
void QUICK_GROUP_MIN_MAX_SELECT::adjust_prefix_ranges ()
{
  if (quick_prefix_select &&
      group_prefix_len < quick_prefix_select->max_used_key_length)
  {
    for (size_t ix= 0; ix < quick_prefix_select->ranges.size(); ++ix)
    {
      QUICK_RANGE *range= quick_prefix_select->ranges[ix];
      range->flag&= ~(NEAR_MIN | NEAR_MAX);
    }
  }
}


/*
  Determine the total number and length of the keys that will be used for
  index lookup.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::update_key_stat()

  DESCRIPTION
    The total length of the keys used for index lookup depends on whether
    there are any predicates referencing the min/max argument, and/or if
    the min/max argument field can be NULL.
    This function does an optimistic analysis whether the search key might
    be extended by a constant for the min/max keypart. It is 'optimistic'
    because during actual execution it may happen that a particular range
    is skipped, and then a shorter key will be used. However this is data
    dependent and can't be easily estimated here.

  RETURN
    None
*/

void QUICK_GROUP_MIN_MAX_SELECT::update_key_stat()
{
  max_used_key_length= real_prefix_len;
  if (min_max_ranges.size() > 0)
  {
    if (have_min)
    { /* Check if the right-most range has a lower boundary. */
      QUICK_RANGE *rightmost_range= min_max_ranges[min_max_ranges.size() - 1];
      if (!(rightmost_range->flag & NO_MIN_RANGE))
      {
        max_used_key_length+= min_max_arg_len;
        used_key_parts++;
        return;
      }
    }
    if (have_max)
    { /* Check if the left-most range has an upper boundary. */
      QUICK_RANGE *leftmost_range= min_max_ranges[0];
      if (!(leftmost_range->flag & NO_MAX_RANGE))
      {
        max_used_key_length+= min_max_arg_len;
        used_key_parts++;
        return;
      }
    }
  }
  else if (have_min && min_max_arg_part &&
           min_max_arg_part->field->real_maybe_null())
  {
    /*
      If a MIN/MAX argument value is NULL, we can quickly determine
      that we're in the beginning of the next group, because NULLs
      are always < any other value. This allows us to quickly
      determine the end of the current group and jump to the next
      group (see next_min()) and thus effectively increases the
      usable key length.
    */
    max_used_key_length+= min_max_arg_len;
    used_key_parts++;
  }
}


/*
  Initialize a quick group min/max select for key retrieval.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::reset()

  DESCRIPTION
    Initialize the index chosen for access and find and store the prefix
    of the last group. The method is expensive since it performs disk access.

  RETURN
    0      OK
    other  Error code
*/

int QUICK_GROUP_MIN_MAX_SELECT::reset(void)
{
  int result;
  DBUG_ENTER("QUICK_GROUP_MIN_MAX_SELECT::reset");

  seen_first_key= false;
  head->set_keyread(TRUE); /* We need only the key attributes */
  /*
    Request ordered index access as usage of ::index_last(), 
    ::index_first() within QUICK_GROUP_MIN_MAX_SELECT depends on it.
  */
  if ((result= head->file->ha_index_init(index, true)))
  {
    head->file->print_error(result, MYF(0));
    DBUG_RETURN(result);
  }
  if (quick_prefix_select && quick_prefix_select->reset())
    DBUG_RETURN(1);

  result= head->file->ha_index_last(record);
  if (result != 0)
  {
    if (result == HA_ERR_END_OF_FILE)
      DBUG_RETURN(0);
    else
      DBUG_RETURN(result);
  }

  /* Save the prefix of the last group. */
  key_copy(last_prefix, record, index_info, group_prefix_len);

  DBUG_RETURN(0);
}



/* 
  Get the next key containing the MIN and/or MAX key for the next group.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::get_next()

  DESCRIPTION
    The method finds the next subsequent group of records that satisfies the
    query conditions and finds the keys that contain the MIN/MAX values for
    the key part referenced by the MIN/MAX function(s). Once a group and its
    MIN/MAX values are found, store these values in the Item_sum objects for
    the MIN/MAX functions. The rest of the values in the result row are stored
    in the Item_field::result_field of each select field. If the query does
    not contain MIN and/or MAX functions, then the function only finds the
    group prefix, which is a query answer itself.

  NOTES
    If both MIN and MAX are computed, then we use the fact that if there is
    no MIN key, there can't be a MAX key as well, so we can skip looking
    for a MAX key in this case.

  RETURN
    0                  on success
    HA_ERR_END_OF_FILE if returned all keys
    other              if some error occurred
*/

int QUICK_GROUP_MIN_MAX_SELECT::get_next()
{
  int min_res= 0;
  int max_res= 0;
  int result;
  int is_last_prefix= 0;

  DBUG_ENTER("QUICK_GROUP_MIN_MAX_SELECT::get_next");

  /*
    Loop until a group is found that satisfies all query conditions or the last
    group is reached.
  */
  do
  {
    result= next_prefix();
    /*
      Check if this is the last group prefix. Notice that at this point
      this->record contains the current prefix in record format.
    */
    if (!result)
    {
      is_last_prefix= key_cmp(index_info->key_part, last_prefix,
                              group_prefix_len);
      DBUG_ASSERT(is_last_prefix <= 0);
    }
    else 
    {
      if (result == HA_ERR_KEY_NOT_FOUND)
        continue;
      break;
    }

    if (have_min)
    {
      min_res= next_min();
      if (min_res == 0)
        update_min_result();
    }
    /* If there is no MIN in the group, there is no MAX either. */
    if ((have_max && !have_min) ||
        (have_max && have_min && (min_res == 0)))
    {
      max_res= next_max();
      if (max_res == 0)
        update_max_result();
      /* If a MIN was found, a MAX must have been found as well. */
      DBUG_ASSERT((have_max && !have_min) ||
                  (have_max && have_min && (max_res == 0)));
    }
    /*
      If this is just a GROUP BY or DISTINCT without MIN or MAX and there
      are equality predicates for the key parts after the group, find the
      first sub-group with the extended prefix.
    */
    if (!have_min && !have_max && key_infix_len > 0)
      result= head->file->ha_index_read_map(record, group_prefix,
                                            make_prev_keypart_map(real_key_parts),
                                            HA_READ_KEY_EXACT);

    result= have_min ? min_res : have_max ? max_res : result;
  } while ((result == HA_ERR_KEY_NOT_FOUND || result == HA_ERR_END_OF_FILE) &&
           is_last_prefix != 0);

  if (result == HA_ERR_KEY_NOT_FOUND)
    result= HA_ERR_END_OF_FILE;

  DBUG_RETURN(result);
}


/*
  Retrieve the minimal key in the next group.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::next_min()

  DESCRIPTION
    Find the minimal key within this group such that the key satisfies the query
    conditions and NULL semantics. The found key is loaded into this->record.

  IMPLEMENTATION
    Depending on the values of min_max_ranges.elements, key_infix_len, and
    whether there is a  NULL in the MIN field, this function may directly
    return without any data access. In this case we use the key loaded into
    this->record by the call to this->next_prefix() just before this call.

  RETURN
    0                    on success
    HA_ERR_KEY_NOT_FOUND if no MIN key was found that fulfills all conditions.
    HA_ERR_END_OF_FILE   - "" -
    other                if some error occurred
*/

int QUICK_GROUP_MIN_MAX_SELECT::next_min()
{
  int result= 0;
  DBUG_ENTER("QUICK_GROUP_MIN_MAX_SELECT::next_min");

  /* Find the MIN key using the eventually extended group prefix. */
  if (min_max_ranges.size() > 0)
  {
    if ((result= next_min_in_range()))
      DBUG_RETURN(result);
  }
  else
  {
    /* Apply the constant equality conditions to the non-group select fields */
    if (key_infix_len > 0)
    {
      if ((result= head->file->ha_index_read_map(record, group_prefix,
                                                 make_prev_keypart_map(real_key_parts),
                                                 HA_READ_KEY_EXACT)))
        DBUG_RETURN(result);
    }

    /*
      If the min/max argument field is NULL, skip subsequent rows in the same
      group with NULL in it. Notice that:
      - if the first row in a group doesn't have a NULL in the field, no row
      in the same group has (because NULL < any other value),
      - min_max_arg_part->field->ptr points to some place in 'record'.
    */
    if (min_max_arg_part && min_max_arg_part->field->is_null())
    {
      uchar key_buf[MAX_KEY_LENGTH];

      /* Find the first subsequent record without NULL in the MIN/MAX field. */
      key_copy(key_buf, record, index_info, max_used_key_length);
      result= head->file->ha_index_read_map(record, key_buf,
                                            make_keypart_map(real_key_parts),
                                            HA_READ_AFTER_KEY);
      /*
        Check if the new record belongs to the current group by comparing its
        prefix with the group's prefix. If it is from the next group, then the
        whole group has NULLs in the MIN/MAX field, so use the first record in
        the group as a result.
        TODO:
        It is possible to reuse this new record as the result candidate for the
        next call to next_min(), and to save one lookup in the next call. For
        this add a new member 'this->next_group_prefix'.
      */
      if (!result)
      {
        if (key_cmp(index_info->key_part, group_prefix, real_prefix_len))
          key_restore(record, key_buf, index_info, 0);
      }
      else if (result == HA_ERR_KEY_NOT_FOUND || result == HA_ERR_END_OF_FILE)
        result= 0; /* There is a result in any case. */
    }
  }

  /*
    If the MIN attribute is non-nullable, this->record already contains the
    MIN key in the group, so just return.
  */
  DBUG_RETURN(result);
}


/* 
  Retrieve the maximal key in the next group.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::next_max()

  DESCRIPTION
    Lookup the maximal key of the group, and store it into this->record.

  RETURN
    0                    on success
    HA_ERR_KEY_NOT_FOUND if no MAX key was found that fulfills all conditions.
    HA_ERR_END_OF_FILE	 - "" -
    other                if some error occurred
*/

int QUICK_GROUP_MIN_MAX_SELECT::next_max()
{
  int result;

  DBUG_ENTER("QUICK_GROUP_MIN_MAX_SELECT::next_max");

  /* Get the last key in the (possibly extended) group. */
  if (min_max_ranges.size() > 0)
    result= next_max_in_range();
  else
    result= head->file->ha_index_read_map(record, group_prefix,
                                          make_prev_keypart_map(real_key_parts),
                                          HA_READ_PREFIX_LAST);
  DBUG_RETURN(result);
}


/** 
  Find the next different key value by skiping all the rows with the same key 
  value.

  Implements a specialized loose index access method for queries 
  containing aggregate functions with distinct of the form:
    SELECT [SUM|COUNT|AVG](DISTINCT a,...) FROM t
  This method comes to replace the index scan + Unique class 
  (distinct selection) for loose index scan that visits all the rows of a 
  covering index instead of jumping in the begining of each group.
  TODO: Placeholder function. To be replaced by a handler API call

  @param is_index_scan     hint to use index scan instead of random index read 
                           to find the next different value.
  @param file              table handler
  @param key_part          group key to compare
  @param record            row data
  @param group_prefix      current key prefix data
  @param group_prefix_len  length of the current key prefix data
  @param group_key_parts   number of the current key prefix columns
  @return status
    @retval  0  success
    @retval !0  failure
*/

static int index_next_different (bool is_index_scan, handler *file, 
                                KEY_PART_INFO *key_part, uchar * record, 
                                const uchar * group_prefix,
                                uint group_prefix_len, 
                                uint group_key_parts)
{
  if (is_index_scan)
  {
    int result= 0;

    while (!key_cmp (key_part, group_prefix, group_prefix_len))
    {
      result= file->ha_index_next(record);
      if (result)
        return(result);
    }
    return result;
  }
  else
    return file->ha_index_read_map(record, group_prefix,
                                   make_prev_keypart_map(group_key_parts),
                                   HA_READ_AFTER_KEY);
}


/*
  Determine the prefix of the next group.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::next_prefix()

  DESCRIPTION
    Determine the prefix of the next group that satisfies the query conditions.
    If there is a range condition referencing the group attributes, use a
    QUICK_RANGE_SELECT object to retrieve the *first* key that satisfies the
    condition. If there is a key infix of constants, append this infix
    immediately after the group attributes. The possibly extended prefix is
    stored in this->group_prefix. The first key of the found group is stored in
    this->record, on which relies this->next_min().

  RETURN
    0                    on success
    HA_ERR_KEY_NOT_FOUND if there is no key with the formed prefix
    HA_ERR_END_OF_FILE   if there are no more keys
    other                if some error occurred
*/
int QUICK_GROUP_MIN_MAX_SELECT::next_prefix()
{
  int result;
  DBUG_ENTER("QUICK_GROUP_MIN_MAX_SELECT::next_prefix");

  if (quick_prefix_select)
  {
    uchar *cur_prefix= seen_first_key ? group_prefix : NULL;
    if ((result= quick_prefix_select->get_next_prefix(group_prefix_len,
                                                      group_key_parts, 
                                                      cur_prefix)))
      DBUG_RETURN(result);
    seen_first_key= TRUE;
  }
  else
  {
    if (!seen_first_key)
    {
      result= head->file->ha_index_first(record);
      if (result)
        DBUG_RETURN(result);
      seen_first_key= TRUE;
    }
    else
    {
      /* Load the first key in this group into record. */
      result= index_next_different (is_index_scan, head->file,
                                    index_info->key_part,
                                    record, group_prefix, group_prefix_len, 
                                    group_key_parts);
      if (result)
        DBUG_RETURN(result);
    }
  }

  /* Save the prefix of this group for subsequent calls. */
  key_copy(group_prefix, record, index_info, group_prefix_len);
  /* Append key_infix to group_prefix. */
  if (key_infix_len > 0)
    memcpy(group_prefix + group_prefix_len,
           key_infix, key_infix_len);

  DBUG_RETURN(0);
}


/*
  Find the minimal key in a group that satisfies some range conditions for the
  min/max argument field.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::next_min_in_range()

  DESCRIPTION
    Given the sequence of ranges min_max_ranges, find the minimal key that is
    in the left-most possible range. If there is no such key, then the current
    group does not have a MIN key that satisfies the WHERE clause. If a key is
    found, its value is stored in this->record.

  RETURN
    0                    on success
    HA_ERR_KEY_NOT_FOUND if there is no key with the given prefix in any of
                         the ranges
    HA_ERR_END_OF_FILE   - "" -
    other                if some error
*/

int QUICK_GROUP_MIN_MAX_SELECT::next_min_in_range()
{
  ha_rkey_function find_flag;
  key_part_map keypart_map;
  bool found_null= FALSE;
  int result= HA_ERR_KEY_NOT_FOUND;

  DBUG_ASSERT(min_max_ranges.size() > 0);

  /* Search from the left-most range to the right. */
  for (Quick_ranges::const_iterator it= min_max_ranges.begin();
       it != min_max_ranges.end(); ++it)
  {
    QUICK_RANGE *cur_range= *it;
    /*
      If the current value for the min/max argument is bigger than the right
      boundary of cur_range, there is no need to check this range.
    */
    if (it != min_max_ranges.begin() && !(cur_range->flag & NO_MAX_RANGE) &&
        (key_cmp(min_max_arg_part, (const uchar*) cur_range->max_key,
                 min_max_arg_len) == 1))
      continue;

    if (cur_range->flag & NO_MIN_RANGE)
    {
      keypart_map= make_prev_keypart_map(real_key_parts);
      find_flag= HA_READ_KEY_EXACT;
    }
    else
    {
      /* Extend the search key with the lower boundary for this range. */
      memcpy(group_prefix + real_prefix_len, cur_range->min_key,
             cur_range->min_length);
      keypart_map= make_keypart_map(real_key_parts);
      find_flag= (cur_range->flag & (EQ_RANGE | NULL_RANGE)) ?
                 HA_READ_KEY_EXACT : (cur_range->flag & NEAR_MIN) ?
                 HA_READ_AFTER_KEY : HA_READ_KEY_OR_NEXT;
    }

    result= head->file->ha_index_read_map(record, group_prefix, keypart_map,
                                          find_flag);
    if (result)
    {
      if ((result == HA_ERR_KEY_NOT_FOUND || result == HA_ERR_END_OF_FILE) &&
          (cur_range->flag & (EQ_RANGE | NULL_RANGE)))
        continue; /* Check the next range. */

      /*
        In all other cases (HA_ERR_*, HA_READ_KEY_EXACT with NO_MIN_RANGE,
        HA_READ_AFTER_KEY, HA_READ_KEY_OR_NEXT) if the lookup failed for this
        range, it can't succeed for any other subsequent range.
      */
      break;
    }

    /* A key was found. */
    if (cur_range->flag & EQ_RANGE)
      break; /* No need to perform the checks below for equal keys. */

    if (cur_range->flag & NULL_RANGE)
    {
      /*
        Remember this key, and continue looking for a non-NULL key that
        satisfies some other condition.
      */
      memcpy(tmp_record, record, head->s->rec_buff_length);
      found_null= TRUE;
      continue;
    }

    /* Check if record belongs to the current group. */
    if (key_cmp(index_info->key_part, group_prefix, real_prefix_len))
    {
      result= HA_ERR_KEY_NOT_FOUND;
      continue;
    }

    /* If there is an upper limit, check if the found key is in the range. */
    if ( !(cur_range->flag & NO_MAX_RANGE) )
    {
      /* Compose the MAX key for the range. */
      uchar *max_key= (uchar*) my_alloca(real_prefix_len + min_max_arg_len);
      memcpy(max_key, group_prefix, real_prefix_len);
      memcpy(max_key + real_prefix_len, cur_range->max_key,
             cur_range->max_length);
      /* Compare the found key with max_key. */
      int cmp_res= key_cmp(index_info->key_part, max_key,
                           real_prefix_len + min_max_arg_len);
      /*
        The key is outside of the range if: 
        the interval is open and the key is equal to the maximum boundry
        or
        the key is greater than the maximum
      */
      if (((cur_range->flag & NEAR_MAX) && cmp_res == 0) ||
          cmp_res > 0)
      {
        result= HA_ERR_KEY_NOT_FOUND;
        continue;
      }
    }
    /* If we got to this point, the current key qualifies as MIN. */
    DBUG_ASSERT(result == 0);
    break;
  }
  /*
    If there was a key with NULL in the MIN/MAX field, and there was no other
    key without NULL from the same group that satisfies some other condition,
    then use the key with the NULL.
  */
  if (found_null && result)
  {
    memcpy(record, tmp_record, head->s->rec_buff_length);
    result= 0;
  }
  return result;
}


/*
  Find the maximal key in a group that satisfies some range conditions for the
  min/max argument field.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::next_max_in_range()

  DESCRIPTION
    Given the sequence of ranges min_max_ranges, find the maximal key that is
    in the right-most possible range. If there is no such key, then the current
    group does not have a MAX key that satisfies the WHERE clause. If a key is
    found, its value is stored in this->record.

  RETURN
    0                    on success
    HA_ERR_KEY_NOT_FOUND if there is no key with the given prefix in any of
                         the ranges
    HA_ERR_END_OF_FILE   - "" -
    other                if some error
*/

int QUICK_GROUP_MIN_MAX_SELECT::next_max_in_range()
{
  ha_rkey_function find_flag;
  key_part_map keypart_map;
  int result;

  DBUG_ASSERT(min_max_ranges.size() > 0);

  /* Search from the right-most range to the left. */
  for (Quick_ranges::const_iterator it= min_max_ranges.end();
       it != min_max_ranges.begin(); --it)
  {
    QUICK_RANGE *cur_range = *(it - 1);
    /*
      If the current value for the min/max argument is smaller than the left
      boundary of cur_range, there is no need to check this range.
    */
    if (it != min_max_ranges.end() &&
        !(cur_range->flag & NO_MIN_RANGE) &&
        (key_cmp(min_max_arg_part, (const uchar*) cur_range->min_key,
                 min_max_arg_len) == -1))
      continue;

    if (cur_range->flag & NO_MAX_RANGE)
    {
      keypart_map= make_prev_keypart_map(real_key_parts);
      find_flag= HA_READ_PREFIX_LAST;
    }
    else
    {
      /* Extend the search key with the upper boundary for this range. */
      memcpy(group_prefix + real_prefix_len, cur_range->max_key,
             cur_range->max_length);
      keypart_map= make_keypart_map(real_key_parts);
      find_flag= (cur_range->flag & EQ_RANGE) ?
                 HA_READ_KEY_EXACT : (cur_range->flag & NEAR_MAX) ?
                 HA_READ_BEFORE_KEY : HA_READ_PREFIX_LAST_OR_PREV;
    }

    result= head->file->ha_index_read_map(record, group_prefix, keypart_map,
                                          find_flag);

    if (result)
    {
      if ((result == HA_ERR_KEY_NOT_FOUND || result == HA_ERR_END_OF_FILE) &&
          (cur_range->flag & EQ_RANGE))
        continue; /* Check the next range. */

      /*
        In no key was found with this upper bound, there certainly are no keys
        in the ranges to the left.
      */
      return result;
    }
    /* A key was found. */
    if (cur_range->flag & EQ_RANGE)
      return 0; /* No need to perform the checks below for equal keys. */

    /* Check if record belongs to the current group. */
    if (key_cmp(index_info->key_part, group_prefix, real_prefix_len))
      continue;                                 // Row not found

    /* If there is a lower limit, check if the found key is in the range. */
    if ( !(cur_range->flag & NO_MIN_RANGE) )
    {
      /* Compose the MIN key for the range. */
      uchar *min_key= (uchar*) my_alloca(real_prefix_len + min_max_arg_len);
      memcpy(min_key, group_prefix, real_prefix_len);
      memcpy(min_key + real_prefix_len, cur_range->min_key,
             cur_range->min_length);
      /* Compare the found key with min_key. */
      int cmp_res= key_cmp(index_info->key_part, min_key,
                           real_prefix_len + min_max_arg_len);
      /*
        The key is outside of the range if: 
        the interval is open and the key is equal to the minimum boundry
        or
        the key is less than the minimum
      */
      if (((cur_range->flag & NEAR_MIN) && cmp_res == 0) ||
          cmp_res < 0)
        continue;
    }
    /* If we got to this point, the current key qualifies as MAX. */
    return result;
  }
  return HA_ERR_KEY_NOT_FOUND;
}


/*
  Update all MIN function results with the newly found value.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::update_min_result()

  DESCRIPTION
    The method iterates through all MIN functions and updates the result value
    of each function by calling Item_sum::reset(), which in turn picks the new
    result value from this->head->record[0], previously updated by
    next_min(). The updated value is stored in a member variable of each of the
    Item_sum objects, depending on the value type.

  IMPLEMENTATION
    The update must be done separately for MIN and MAX, immediately after
    next_min() was called and before next_max() is called, because both MIN and
    MAX take their result value from the same buffer this->head->record[0]
    (i.e.  this->record).

  RETURN
    None
*/

void QUICK_GROUP_MIN_MAX_SELECT::update_min_result()
{
  Item_sum *min_func;

  min_functions_it->rewind();
  while ((min_func= (*min_functions_it)++))
    min_func->reset_and_add();
}


/*
  Update all MAX function results with the newly found value.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::update_max_result()

  DESCRIPTION
    The method iterates through all MAX functions and updates the result value
    of each function by calling Item_sum::reset(), which in turn picks the new
    result value from this->head->record[0], previously updated by
    next_max(). The updated value is stored in a member variable of each of the
    Item_sum objects, depending on the value type.

  IMPLEMENTATION
    The update must be done separately for MIN and MAX, immediately after
    next_max() was called, because both MIN and MAX take their result value
    from the same buffer this->head->record[0] (i.e.  this->record).

  RETURN
    None
*/

void QUICK_GROUP_MIN_MAX_SELECT::update_max_result()
{
  Item_sum *max_func;

  max_functions_it->rewind();
  while ((max_func= (*max_functions_it)++))
    max_func->reset_and_add();
}


/*
  Append comma-separated list of keys this quick select uses to key_names;
  append comma-separated list of corresponding used lengths to used_lengths.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::add_keys_and_lengths()
    key_names    [out] Names of used indexes
    used_lengths [out] Corresponding lengths of the index names

  DESCRIPTION
    This method is used by select_describe to extract the names of the
    indexes used by a quick select.

*/

void QUICK_GROUP_MIN_MAX_SELECT::add_keys_and_lengths(String *key_names,
                                                      String *used_lengths)
{
  char buf[64];
  size_t length;
  key_names->append(index_info->name);
  length= longlong2str(max_used_key_length, buf, 10) - buf;
  used_lengths->append(buf, length);
}



/**
  Traverse the R-B range tree for this and later keyparts to see if
  there are at least as many equality ranges as defined by the limit.

  @param keypart_root   The root of a R-B tree of ranges for a given keypart.
  @param count[in,out]  The number of equality ranges found so far
  @param limit          The number of ranges 

  @retval true if limit > 0 and 'limit' or more equality ranges have been 
          found in the range R-B trees
  @retval false otherwise         

*/
static bool eq_ranges_exceeds_limit(SEL_ARG *keypart_root, uint* count, uint limit)
{
  // "Statistics instead of index dives" feature is turned off
  if (limit == 0)
    return false;
  
  /*
    Optimization: if there is at least one equality range, index
    statistics will be used when limit is 1. It's safe to return true
    even without checking that there is an equality range because if
    there are none, index statistics will not be used anyway.
  */
  if (limit == 1)
    return true;

  for(SEL_ARG *keypart_range= keypart_root->first(); 
      keypart_range; keypart_range= keypart_range->next)
  {
    /*
      This is an equality range predicate and should be counted if:
      1) the range for this keypart does not have a min/max flag 
         (which indicates <, <= etc), and
      2) the lower and upper range boundaries have the same value
         (it's not a "x BETWEEN a AND b")
      
      Note, however, that if this is an "x IS NULL" condition we don't
      count it because the number of NULL-values is likely to be off
      the index statistics we plan to use.
    */
    if (!keypart_range->min_flag && !keypart_range->max_flag && // 1)
        !keypart_range->cmp_max_to_min(keypart_range) &&        // 2)
        !keypart_range->is_null_interval())                     // "x IS NULL"
    {
      /* 
         Count predicates in the next keypart, but only if that keypart
         is the next in the index. 
      */
      if (keypart_range->next_key_part && 
          keypart_range->next_key_part->part == keypart_range->part + 1)
        eq_ranges_exceeds_limit(keypart_range->next_key_part, count, limit);
      else
        // We've found a path of equlity predicates down to a keypart leaf
        (*count)++; 

      if (*count >= limit)
        return true;
    }
  }
  return false;
}

#ifndef DBUG_OFF

static void print_sel_tree(PARAM *param, SEL_TREE *tree, key_map *tree_map,
                           const char *msg)
{
  char buff[1024];
  DBUG_ENTER("print_sel_tree");

  String tmp(buff,sizeof(buff),&my_charset_bin);
  tmp.length(0);
  for (uint idx= 0; idx < param->keys; idx++)
  {
    if (tree_map->is_set(idx))
    {
      uint keynr= param->real_keynr[idx];
      if (tmp.length())
        tmp.append(',');
      tmp.append(param->table->key_info[keynr].name);
    }
  }
  if (!tmp.length())
    tmp.append(STRING_WITH_LEN("(empty)"));

  DBUG_PRINT("info", ("SEL_TREE: %p (%s)  scans: %s", tree, msg, tmp.ptr()));
  DBUG_VOID_RETURN;
}


static void print_ror_scans_arr(TABLE *table, const char *msg,
                                struct st_ror_scan_info **start,
                                struct st_ror_scan_info **end)
{
  DBUG_ENTER("print_ror_scans_arr");

  char buff[1024];
  String tmp(buff,sizeof(buff),&my_charset_bin);
  tmp.length(0);
  for (;start != end; start++)
  {
    if (tmp.length())
      tmp.append(',');
    tmp.append(table->key_info[(*start)->keynr].name);
  }
  if (!tmp.length())
    tmp.append(STRING_WITH_LEN("(empty)"));
  DBUG_PRINT("info", ("ROR key scans (%s): %s", msg, tmp.ptr()));
  fprintf(DBUG_FILE,"ROR key scans (%s): %s", msg, tmp.ptr());

  DBUG_VOID_RETURN;
}


#endif /* !DBUG_OFF */

/**
  Print a key to a string

  @param[out] out          String the key is appended to
  @param[in]  key_part     Index components description
  @param[in]  key          Key tuple
  @param[in]  used_length  Key tuple length
*/
static void
print_key_value(String *out, const KEY_PART_INFO *key_part, const uchar *key)
{
  Field *field= key_part->field;

  if (field->flags & BLOB_FLAG)
  {
    // Byte 0 of a nullable key is the null-byte. If set, key is NULL.
    if (field->real_maybe_null() && *key)
      out->append(STRING_WITH_LEN("NULL"));
    else
      (field->type() == MYSQL_TYPE_GEOMETRY) ?
        out->append(STRING_WITH_LEN("unprintable_geometry_value")) :
        out->append(STRING_WITH_LEN("unprintable_blob_value"));    
    return;
  }

  uint store_length= key_part->store_length;

  if (field->real_maybe_null())
  {
    /*
      Byte 0 of key is the null-byte. If set, key is NULL.
      Otherwise, print the key value starting immediately after the
      null-byte
    */
    if (*key)
    {
      out->append(STRING_WITH_LEN("NULL"));
      return;
    }
    key++;                                    // Skip null byte
    store_length--;
  }

  /*
    Binary data cannot be converted to UTF8 which is what the
    optimizer trace expects. If the column is binary, the hex
    representation is printed to the trace instead.
   */
  if (field->flags & BINARY_FLAG)
  {
    out->append("0x");
    for (uint i= 0; i < store_length; i++)
    {
      out->append(_dig_vec_lower[*(key+i) >> 4]);
      out->append(_dig_vec_lower[*(key+i) & 0x0F]);
    }
    return;
  }    

  char buff[128];
  String tmp(buff, sizeof(buff), system_charset_info);
  tmp.length(0);

  TABLE *table= field->table;
  my_bitmap_map *old_sets[2];

  dbug_tmp_use_all_columns(table, old_sets, table->read_set,
                           table->write_set);

  field->set_key_image(key, key_part->length);
  if (field->type() == MYSQL_TYPE_BIT)
    (void) field->val_int_as_str(&tmp, 1); // may change tmp's charset
  else
    field->val_str(&tmp); // may change tmp's charset
  out->append(tmp.ptr(), tmp.length(), tmp.charset());

  dbug_tmp_restore_column_maps(table->read_set, table->write_set, old_sets);
}

/**
  Append range info for a key part to a string

  @param[in,out] out          String the range info is appended to
  @param[in]     key_part     Indexed column used in a range select
  @param[in]     min_key      Key tuple describing lower bound of range
  @param[in]     max_key      Key tuple describing upper bound of range
  @param[in]     flag         Key range flags defining what min_key
                              and max_key represent @see my_base.h
 */
void append_range(String *out,
                  const KEY_PART_INFO *key_part,
                  const uchar *min_key, const uchar *max_key,
                  const uint flag)
{
  if (out->length() > 0)
    out->append(STRING_WITH_LEN(" AND "));

  if (flag & GEOM_FLAG)
  {
    /*
      The flags of GEOM ranges do not work the same way as for other
      range types, so printing "col < some_geom" doesn't make sense.
      Just print the column name, not operator.
    */
    out->append(key_part->field->field_name);
    out->append(STRING_WITH_LEN(" "));
    print_key_value(out, key_part, min_key);
    return;
  }

  if (!(flag & NO_MIN_RANGE))
  {
    print_key_value(out, key_part, min_key);
    if (flag & NEAR_MIN)
      out->append(STRING_WITH_LEN(" < "));
    else
      out->append(STRING_WITH_LEN(" <= "));
  }

  out->append(key_part->field->field_name);

  if (!(flag & NO_MAX_RANGE))
  {
    if (flag & NEAR_MAX)
      out->append(STRING_WITH_LEN(" < "));
    else
      out->append(STRING_WITH_LEN(" <= "));
    print_key_value(out, key_part, max_key);
  }
}

/**
  Traverse an R-B tree of range conditions and append all ranges for
  this keypart and consecutive keyparts to range_trace (if non-NULL)
  or to range_string (if range_trace is NULL). See description of R-B
  trees/SEL_ARG for details on how ranges are linked.

  @param[in,out] range_trace   Optimizer trace array ranges are appended to
  @param[in,out] range_string  The string where range predicates are
                               appended when the last keypart has
                               been reached.
  @param         range_so_far  String containing ranges for keyparts prior
                               to this keypart.
  @param         keypart_root  The root of the R-B tree containing intervals
                               for this keypart.
  @param         key_parts     Index components description, used when adding
                               information to the optimizer trace
  @param         print_full    Whether or not ranges on unusable keyparts
                               should be printed. Useful for debugging.

  @note This function mimics the behavior of sel_arg_range_seq_next()
*/
static void append_range_all_keyparts(Opt_trace_array *range_trace,
                                      String *range_string,
                                      String *range_so_far,
                                      SEL_ARG *keypart_root,
                                      const KEY_PART_INFO *key_parts,
                                      const bool print_full)
{
  DBUG_ASSERT(keypart_root && keypart_root != &null_element);

  const bool append_to_trace= (range_trace != NULL);

  // Either add info to range_string or to range_trace
  DBUG_ASSERT(append_to_trace ? !range_string : (range_string != NULL));

  // Navigate to first interval in red-black tree
  const KEY_PART_INFO *cur_key_part= key_parts + keypart_root->part;
  const SEL_ARG *keypart_range= keypart_root->first();

  const size_t save_range_so_far_length= range_so_far->length();

  while (keypart_range)
  {
    /*
      Skip the rest of condition printing to avoid OOM if appending to
      range_string and the string becomes too long. Printing very long
      range conditions normally doesn't make sense either.
     */
    if (!append_to_trace && range_string->length() > 500)
    {
      range_string->append(STRING_WITH_LEN("..."));
      break;
    }

    // Append the current range predicate to the range String
    append_range(range_so_far, cur_key_part,
                 keypart_range->min_value, keypart_range->max_value,
                 keypart_range->min_flag | keypart_range->max_flag);

    /*
      Print range predicates for consecutive keyparts if
      1) There are predicates for later keyparts, and
      2) We explicitly requested to print even the ranges that will
         not be usable by range access, or
      3) There are no "holes" in the used keyparts (keypartX can only
         be used if there is a range predicate on keypartX-1), and
      4) The current range is an equality range
     */
    if (keypart_range->next_key_part &&                                    // 1
        (print_full ||                                                     // 2
         (keypart_range->next_key_part->part == keypart_range->part + 1 && // 3
          keypart_range->is_singlepoint())))                               // 4
    {
      append_range_all_keyparts(range_trace, range_string, range_so_far,
                                keypart_range->next_key_part, key_parts,
                                print_full);
    }
    else
    {
      /*
        This is the last keypart with a usable range predicate. Print
        full range info to the optimizer trace or to the string
      */
      if (append_to_trace)
        range_trace->add_utf8(range_so_far->ptr(),
                              range_so_far->length());
      else
      {
        if (range_string->length() == 0)
          range_string->append(STRING_WITH_LEN("("));
        else
          range_string->append(STRING_WITH_LEN(" OR ("));

        range_string->append(range_so_far->ptr(), range_so_far->length());
        range_string->append(STRING_WITH_LEN(")"));
      }
    }
    keypart_range= keypart_range->next;
    /*
      Now moving to next range for this keypart, so "reset"
      range_so_far to include only range description of earlier
      keyparts
    */
    range_so_far->length(save_range_so_far_length);
  }
}

/**
  Print the ranges in a SEL_TREE to debug log.

  @param tree_name   Descriptive name of the tree
  @param tree        The SEL_TREE that will be printed to debug log
  @param param       PARAM from test_quick_select
*/
static inline void dbug_print_tree(const char *tree_name,
                                   SEL_TREE *tree,
                                   const RANGE_OPT_PARAM *param)
{
#ifndef DBUG_OFF
  print_tree(NULL, tree_name, tree, param, true);
#endif
}


static inline void print_tree(String *out,
                              const char *tree_name,
                              SEL_TREE *tree,
                              const RANGE_OPT_PARAM *param,
                              const bool print_full)
{
  if (!param->using_real_indexes)
  {
    if (out)
    {
      out->append(tree_name);
      out->append(" uses a partitioned index and cannot be printed");
    }
    else
      DBUG_PRINT("info",
                 ("sel_tree: "
                  "%s uses a partitioned index and cannot be printed",
                  tree_name));
    return;
  }

  if (!tree)
  {
    if (out)
    {
      out->append(tree_name);
      out->append(" is NULL");
    }
    else
      DBUG_PRINT("info", ("sel_tree: %s is NULL", tree_name));
    return;
  }

  if (tree->type == SEL_TREE::IMPOSSIBLE)
  {
    if (out)
    {
      out->append(tree_name);
      out->append(" is IMPOSSIBLE");
    }
    else
      DBUG_PRINT("info", ("sel_tree: %s is IMPOSSIBLE", tree_name));
    return;
  }

  if (tree->type == SEL_TREE::ALWAYS)
  {
    if (out)
    {
      out->append(tree_name);
      out->append(" is ALWAYS");
    }
    else
      DBUG_PRINT("info", ("sel_tree: %s is ALWAYS", tree_name));
    return;
  }

  if (tree->type == SEL_TREE::MAYBE)
  {
    if (out)
    {
      out->append(tree_name);
      out->append(" is MAYBE");
    }
    else
      DBUG_PRINT("info", ("sel_tree: %s is MAYBE", tree_name));
    return;
  }

  if (!tree->merges.is_empty())
  {
    if (out)
    {
      out->append(tree_name);
      out->append(" contains the following merges");
    }
    else
      DBUG_PRINT("info",
                 ("sel_tree: "
                  "%s contains the following merges", tree_name));

    List_iterator<SEL_IMERGE> it(tree->merges);
    int i= 1;
    for (SEL_IMERGE *el= it++; el; el= it++, i++)
    {
      if (out)
      {
        out->append("\n--- alternative ");
        char istr[22];
        out->append(llstr(i, istr));
        out->append(" ---\n");
      }
      else
        DBUG_PRINT("info", ("sel_tree: --- alternative %d ---",i));
      for (SEL_TREE** current= el->trees;
           current != el->trees_next;
           current++)
        print_tree(out, "  merge_tree", *current, param, print_full);
    }
  }

  for (uint i= 0; i< param->keys; i++)
  {
    if (tree->keys[i] == NULL || tree->keys[i] == &null_element)
      continue;

    uint real_key_nr= param->real_keynr[i];

    const KEY &cur_key= param->table->key_info[real_key_nr];
    const KEY_PART_INFO *key_part= cur_key.key_part;

    /*
      String holding the final range description from
      append_range_all_keyparts()
    */
    char buff1[512];
    String range_result(buff1, sizeof(buff1), system_charset_info);
    range_result.length(0);

    /*
      Range description up to a certain keypart - used internally in
      append_range_all_keyparts()
    */
    char buff2[128];
    String range_so_far(buff2, sizeof(buff2), system_charset_info);
    range_so_far.length(0);

    append_range_all_keyparts(NULL, &range_result, &range_so_far,
                              tree->keys[i], key_part, print_full);

    if (out)
    {
      char istr[22];

      out->append(tree_name);
      out->append(" keys[");
      out->append(llstr(i, istr));
      out->append("]: ");
      out->append(range_result.ptr());
      out->append("\n");
    }
    else
      DBUG_PRINT("info",
                 ("sel_tree: %p, type=%d, %s->keys[%u(%u)]: %s",
                  tree->keys[i], tree->keys[i]->type, tree_name, i, 
                  real_key_nr, range_result.ptr()));
  }
}


/*****************************************************************************
** Print a quick range for debugging
** TODO:
** This should be changed to use a String to store each row instead
** of locking the DEBUG stream !
*****************************************************************************/

#ifndef DBUG_OFF

static void
print_multiple_key_values(KEY_PART *key_part, const uchar *key,
                          uint used_length)
{
  char buff[1024];
  const uchar *key_end= key+used_length;
  String tmp(buff,sizeof(buff),&my_charset_bin);
  uint store_length;
  TABLE *table= key_part->field->table;
  my_bitmap_map *old_sets[2];

  dbug_tmp_use_all_columns(table, old_sets, table->read_set, table->write_set);

  for (; key < key_end; key+=store_length, key_part++)
  {
    Field *field=      key_part->field;
    store_length= key_part->store_length;

    if (field->real_maybe_null())
    {
      if (*key)
      {
        if (fwrite("NULL",sizeof(char),4,DBUG_FILE) != 4) {
          goto restore_col_map;
        }
        continue;
      }
      key++;                                    // Skip null byte
      store_length--;
    }
    field->set_key_image(key, key_part->length);
    if (field->type() == MYSQL_TYPE_BIT)
      (void) field->val_int_as_str(&tmp, 1);
    else
      field->val_str(&tmp);
    if (fwrite(tmp.ptr(),sizeof(char),tmp.length(),DBUG_FILE) != tmp.length()) {
      goto restore_col_map;
    }
    if (key+store_length < key_end)
      fputc('/',DBUG_FILE);
  }
restore_col_map:
  dbug_tmp_restore_column_maps(table->read_set, table->write_set, old_sets);
}

static void print_quick(QUICK_SELECT_I *quick, const key_map *needed_reg)
{
  char buf[MAX_KEY/8+1];
  TABLE *table;
  my_bitmap_map *old_sets[2];
  DBUG_ENTER("print_quick");
  if (!quick)
    DBUG_VOID_RETURN;
  DBUG_LOCK_FILE;

  table= quick->head;
  dbug_tmp_use_all_columns(table, old_sets, table->read_set, table->write_set);
  quick->dbug_dump(0, TRUE);
  dbug_tmp_restore_column_maps(table->read_set, table->write_set, old_sets);

  fprintf(DBUG_FILE,"other_keys: 0x%s:\n", needed_reg->print(buf));

  DBUG_UNLOCK_FILE;
  DBUG_VOID_RETURN;
}

void QUICK_RANGE_SELECT::dbug_dump(int indent, bool verbose)
{
  /* purecov: begin inspected */
  fprintf(DBUG_FILE, "%*squick range select, key %s, length: %d\n",
          indent, "", head->key_info[index].name, max_used_key_length);

  if (verbose)
  {
    for (size_t ix= 0; ix < ranges.size(); ++ix)
    {
      fprintf(DBUG_FILE, "%*s", indent + 2, "");
      QUICK_RANGE *range= ranges[ix];
      if (!(range->flag & NO_MIN_RANGE))
      {
        print_multiple_key_values(key_parts, range->min_key,
                                  range->min_length);
        if (range->flag & NEAR_MIN)
          fputs(" < ",DBUG_FILE);
        else
          fputs(" <= ",DBUG_FILE);
      }
      fputs("X",DBUG_FILE);

      if (!(range->flag & NO_MAX_RANGE))
      {
        if (range->flag & NEAR_MAX)
          fputs(" < ",DBUG_FILE);
        else
          fputs(" <= ",DBUG_FILE);
        print_multiple_key_values(key_parts, range->max_key,
                                  range->max_length);
      }
      fputs("\n",DBUG_FILE);
    }
  }
  /* purecov: end */
}

void QUICK_INDEX_MERGE_SELECT::dbug_dump(int indent, bool verbose)
{
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  QUICK_RANGE_SELECT *quick;
  fprintf(DBUG_FILE, "%*squick index_merge select\n", indent, "");
  fprintf(DBUG_FILE, "%*smerged scans {\n", indent, "");
  while ((quick= it++))
    quick->dbug_dump(indent+2, verbose);
  if (pk_quick_select)
  {
    fprintf(DBUG_FILE, "%*sclustered PK quick:\n", indent, "");
    pk_quick_select->dbug_dump(indent+2, verbose);
  }
  fprintf(DBUG_FILE, "%*s}\n", indent, "");
}

void QUICK_ROR_INTERSECT_SELECT::dbug_dump(int indent, bool verbose)
{
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  QUICK_RANGE_SELECT *quick;
  fprintf(DBUG_FILE, "%*squick ROR-intersect select, %scovering\n",
          indent, "", need_to_fetch_row? "":"non-");
  fprintf(DBUG_FILE, "%*smerged scans {\n", indent, "");
  while ((quick= it++))
    quick->dbug_dump(indent+2, verbose);
  if (cpk_quick)
  {
    fprintf(DBUG_FILE, "%*sclustered PK quick:\n", indent, "");
    cpk_quick->dbug_dump(indent+2, verbose);
  }
  fprintf(DBUG_FILE, "%*s}\n", indent, "");
}

void QUICK_ROR_UNION_SELECT::dbug_dump(int indent, bool verbose)
{
  List_iterator_fast<QUICK_SELECT_I> it(quick_selects);
  QUICK_SELECT_I *quick;
  fprintf(DBUG_FILE, "%*squick ROR-union select\n", indent, "");
  fprintf(DBUG_FILE, "%*smerged scans {\n", indent, "");
  while ((quick= it++))
    quick->dbug_dump(indent+2, verbose);
  fprintf(DBUG_FILE, "%*s}\n", indent, "");
}

/*
  Print quick select information to DBUG_FILE.

  SYNOPSIS
    QUICK_GROUP_MIN_MAX_SELECT::dbug_dump()
    indent  Indentation offset
    verbose If TRUE show more detailed output.

  DESCRIPTION
    Print the contents of this quick select to DBUG_FILE. The method also
    calls dbug_dump() for the used quick select if any.

  IMPLEMENTATION
    Caller is responsible for locking DBUG_FILE before this call and unlocking
    it afterwards.

  RETURN
    None
*/

void QUICK_GROUP_MIN_MAX_SELECT::dbug_dump(int indent, bool verbose)
{
  fprintf(DBUG_FILE,
          "%*squick_group_min_max_select: index %s (%d), length: %d\n",
          indent, "", index_info->name, index, max_used_key_length);
  if (key_infix_len > 0)
  {
    fprintf(DBUG_FILE, "%*susing key_infix with length %d:\n",
            indent, "", key_infix_len);
  }
  if (quick_prefix_select)
  {
    fprintf(DBUG_FILE, "%*susing quick_range_select:\n", indent, "");
    quick_prefix_select->dbug_dump(indent + 2, verbose);
  }
  if (min_max_ranges.size() > 0)
  {
    fprintf(DBUG_FILE, "%*susing %d quick_ranges for MIN/MAX:\n",
            indent, "", static_cast<int>(min_max_ranges.size()));
  }
}


#endif /* !DBUG_OFF */
#endif /* OPT_RANGE_CC_INCLUDED */
