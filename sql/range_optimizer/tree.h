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

#ifndef SQL_RANGE_OPTIMIZER_TREE_H_
#define SQL_RANGE_OPTIMIZER_TREE_H_

#include <assert.h>
#include <string.h>
#include <sys/types.h>

#include "my_alloc.h"
#include "my_base.h"
#include "my_inttypes.h"
#include "sql/field.h"
#include "sql/mem_root_array.h"
#include "sql/range_optimizer/internal.h"
#include "sql/range_optimizer/range_opt_param.h"
#include "sql/sql_bitmap.h"
#include "sql/sql_const.h"
#include "sql/sql_list.h"

class Cost_estimate;
class SEL_ARG;
class SEL_ROOT;
class SEL_TREE;
struct KEY_PART;
struct ROR_SCAN_INFO;

// Note: tree1 and tree2 are not usable by themselves after tree_and() or
// tree_or().
SEL_TREE *tree_and(RANGE_OPT_PARAM *param, SEL_TREE *tree1, SEL_TREE *tree2);
SEL_TREE *tree_or(RANGE_OPT_PARAM *param, bool remove_jump_scans,
                  SEL_TREE *tree1, SEL_TREE *tree2);
SEL_ROOT *key_or(RANGE_OPT_PARAM *param, SEL_ROOT *key1, SEL_ROOT *key2);
SEL_ROOT *key_and(RANGE_OPT_PARAM *param, SEL_ROOT *key1, SEL_ROOT *key2);

bool sel_trees_can_be_ored(SEL_TREE *tree1, SEL_TREE *tree2,
                           RANGE_OPT_PARAM *param);

/**
  A graph of (possible multiple) key ranges, represented as a red-black
  binary tree. There are three types (see the Type enum); if KEY_RANGE,
  we have zero or more SEL_ARGs, described in the documentation on SEL_ARG.

  As a special case, a nullptr SEL_ROOT means a range that is always true.
  This is true both for keys[] and next_key_part.
*/
class SEL_ROOT {
 public:
  /**
    Used to indicate if the range predicate for an index is always
    true/false, depends on values from other tables or can be
    evaluated as is.
  */
  enum class Type {
    /** The range predicate for this index is always false. */
    IMPOSSIBLE,
    /**
      There is a range predicate that refers to another table. The
      range access method cannot be used on this index unless that
      other table is earlier in the join sequence. The bit
      representing the index is set in JOIN_TAB::needed_reg to
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

  /**
    Constructs a tree of type KEY_RANGE, using the given root.
    (The root is allowed to have children.)
  */
  SEL_ROOT(SEL_ARG *root);

  /**
    Used to construct MAYBE_KEY and IMPOSSIBLE SEL_ARGs.
  */
  SEL_ROOT(MEM_ROOT *memroot, Type type_arg);

  /**
    Note that almost all SEL_ROOTs are created on the MEM_ROOT,
    so this destructor will only rarely be called.
  */
  ~SEL_ROOT() { assert(use_count == 0); }

  /**
    Returns true iff we have a single node that has no max nor min.
    Note that by convention, a nullptr SEL_ROOT means the same.
  */
  bool is_always() const;

  /**
    Returns a number of keypart values appended to the key buffer
    for min key and max key. This function is used by both Range
    Analysis and Partition pruning. For partition pruning we have
    to ensure that we don't store also subpartition fields. Thus
    we have to stop at the last partition part and not step into
    the subpartition fields. For Range Analysis we set last_part
    to MAX_KEY which we should never reach.
  */
  int store_min_key(KEY_PART *key, uchar **range_key, uint *range_key_flag,
                    uint last_part, bool start_key);

  /* returns a number of keypart values appended to the key buffer */
  int store_max_key(KEY_PART *key, uchar **range_key, uint *range_key_flag,
                    uint last_part, bool start_key);

  /**
    Signal to the tree that the caller will shortly be dropping it
    on the floor; if others are still using it, this is a no-op,
    but if the caller was the last one, it is now an orphan, and
    references from it should not count.
  */
  void free_tree();

  /**
    Insert the given node into the tree, and update the root.

    @param key The node to insert.
  */
  void insert(SEL_ARG *key);

  /**
    Delete the given node from the tree, and update the root.

    @param key The node to delete. Must exist in the tree.
  */
  void tree_delete(SEL_ARG *key);

  /**
    Find best key with min <= given key.
    Because of the call context, this should never return nullptr to get_range.

    @param key The key to search for.
  */
  SEL_ARG *find_range(const SEL_ARG *key) const;

  /**
    Create a new tree that's a duplicate of this one.

    @param param The parameters for the new tree. Used to find out which
      MEM_ROOT to allocate the new nodes on.

    @return The new tree, or nullptr in case of out of memory.
  */
  SEL_ROOT *clone_tree(RANGE_OPT_PARAM *param) const;

  /**
    Check if SEL_ROOT::use_count value is correct. See the definition
    of use_count for what is "correct".

    @param root The origin tree of the SEL_ARG graph (an RB-tree that
      has the least value of root->sel_root->root->part in the
      entire graph, and thus is the "origin" of the graph)

    @return true iff an incorrect SEL_ARG::use_count is found.
  */
  bool test_use_count(const SEL_ROOT *root) const;

  /** Returns true iff this is a single-element, single-field predicate. */
  inline bool simple_key() const;

  /**
    The root node of the tree. Note that this may change as the result
    of rotations during insertions or deletions, so pointers should be
    to the SEL_ROOT, not individual SEL_ARG nodes.

    This element can never be nullptr, but can be null_element
    if type == KEY_RANGE and the tree is empty (which then means the same as
    type == IMPOSSIBLE).

    If type == IMPOSSIBLE or type == MAYBE_KEY, there's a single root
    element which only serves to hold next_key_part (we don't really care
    about root->part in this case); the actual min/max values etc.
    do not matter and should not be accessed.
  */
  SEL_ARG *root;

  /**
    Number of references to this SEL_ARG tree. References may be from
    SEL_ARG::next_key_part of SEL_ARGs from earlier keyparts or
    SEL_TREE::keys[i].

    The SEL_ARG trees are re-used in a lazy-copy manner based on this
    reference counting.
  */
  ulong use_count{0};

  /**
    Number of nodes in the RB-tree, not including sentinels.
  */
  uint16 elements{0};
};

int sel_cmp(Field *f, uchar *a, uchar *b, uint8 a_flag, uint8 b_flag);

/**
  A helper function to invert min flags to max flags for DESC key parts.
  It changes NEAR_MIN, NO_MIN_RANGE to NEAR_MAX, NO_MAX_RANGE appropriately
*/

inline uint invert_min_flag(uint min_flag) {
  uint max_flag_out = min_flag & ~(NEAR_MIN | NO_MIN_RANGE);
  if (min_flag & NEAR_MIN) max_flag_out |= NEAR_MAX;
  if (min_flag & NO_MIN_RANGE) max_flag_out |= NO_MAX_RANGE;
  return max_flag_out;
}

/**
  A helper function to invert max flags to min flags for DESC key parts.
  It changes NEAR_MAX, NO_MAX_RANGE to NEAR_MIN, NO_MIN_RANGE appropriately
*/

inline uint invert_max_flag(uint max_flag) {
  uint min_flag_out = max_flag & ~(NEAR_MAX | NO_MAX_RANGE);
  if (max_flag & NEAR_MAX) min_flag_out |= NEAR_MIN;
  if (max_flag & NO_MAX_RANGE) min_flag_out |= NO_MIN_RANGE;
  return min_flag_out;
}

/*
  A construction block of the SEL_ARG-graph.

  One SEL_ARG object represents an "elementary interval" in form

      min_value <=?  table.keypartX  <=? max_value

  The interval is a non-empty interval of any kind: with[out] minimum/maximum
  bound, [half]open/closed, single-point interval, etc.

  1. SEL_ARG GRAPH STRUCTURE

  SEL_ARG objects are linked together in a graph, represented by the SEL_ROOT.
  The meaning of the graph is better demonstrated by an example:

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
      node3->left == ... node7->right == null_element

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
   - get_ranges_from_tree() - Walk the SEL_ARG, materialize the key intervals.

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

class SEL_ARG {
 public:
  uint8 min_flag{0}, max_flag{0};

  /**
    maybe_flag signals that this range is AND-ed with some unknown range
    (a MAYBE_KEY node). This means that the range could be smaller than
    what it would otherwise denote; e.g., a range such as

      (0 < x < 3) AND x=( SELECT ... )

    could in reality be e.g. (1 < x < 2), depending on what the subselect
    returns (and we don't know that when planning), but it could never be
    bigger.

    FIXME: It's unclear if this is really kept separately per SEL_ARG or is
    meaningful only at the root node, and thus should be moved to the
    SEL_ROOT. Most code seems to assume the latter, but a few select places,
    non-root nodes appear to be modified.
  */
  bool maybe_flag{false};

  /*
    Which key part. TODO: This is the same for all values in a SEL_ROOT,
    so we should move it there.
  */
  uint8 part{0};

  bool maybe_null() const { return field->is_nullable(); }

  /**
    The rtree index interval to scan, undefined unless
    SEL_ARG::min_flag == GEOM_FLAG.
  */
  enum ha_rkey_function rkey_func_flag;

  /*
    TODO: This is the same for all values in a SEL_ROOT, so we should
    move it there; however, be careful about cmp_* functions.
    Note that this should never be nullptr except in the special case
    where we have a dummy SEL_ARG to hold next_key_part only
    (see SEL_ROOT::root for more information).
  */
  Field *field{nullptr};
  uchar *min_value, *max_value;  // Pointer to range

  /*
    eq_tree(), first(), last() etc require that left == right == NULL
    if the type is MAYBE_KEY. Todo: fix this so SEL_ARGs without R-B
    children are handled consistently. See related WL#5894.
  */
  SEL_ARG *left, *right;    /* R-B tree children */
  SEL_ARG *next, *prev;     /* Links for bi-directional interval list */
  SEL_ARG *parent{nullptr}; /* R-B tree parent (nullptr for root) */
  /*
    R-B tree of intervals covering keyparts consecutive to this
    SEL_ARG. See documentation of SEL_ARG GRAPH semantics for details.
  */
  SEL_ROOT *next_key_part{nullptr};

  /**
    Convenience function for removing the next_key_part. The typical
    use for this function is to disconnect the next_key_part from the
    root, send it to key_and() or key_or(), and then connect the
    result of that function back to the SEL_ARG using set_next_key_part().

    @return The previous value of next_key_part.
  */
  SEL_ROOT *release_next_key_part() {
    SEL_ROOT *ret = next_key_part;
    if (next_key_part) {
      assert(next_key_part->use_count > 0);
      --next_key_part->use_count;
    }
    next_key_part = nullptr;
    return ret;
  }

  /**
    Convenience function for changing next_key_part, including
    updating the use_count. The argument is allowed to be nullptr.

    @param next_key_part_arg New value for next_key_part.
  */
  void set_next_key_part(SEL_ROOT *next_key_part_arg) {
    release_next_key_part();
    next_key_part = next_key_part_arg;
    if (next_key_part) ++next_key_part->use_count;
  }

  enum leaf_color { BLACK, RED } color;

  bool is_ascending;  ///< true - ASC order, false - DESC

  SEL_ARG() = default;
  SEL_ARG(SEL_ARG &);
  SEL_ARG(Field *, const uchar *, const uchar *, bool asc);
  SEL_ARG(Field *field, uint8 part, uchar *min_value, uchar *max_value,
          uint8 min_flag, uint8 max_flag, bool maybe_flag, bool asc,
          ha_rkey_function gis_flag);
  /**
    Note that almost all SEL_ARGs are created on the MEM_ROOT,
    so this destructor will only rarely be called.
  */
  ~SEL_ARG() { release_next_key_part(); }

  /**
    returns true if a range predicate is equal. Use all_same()
    to check for equality of all the predicates on this keypart.
  */
  inline bool is_same(const SEL_ARG *arg) const {
    if (part != arg->part) return false;
    return cmp_min_to_min(arg) == 0 && cmp_max_to_max(arg) == 0;
  }

  inline void merge_flags(SEL_ARG *arg) { maybe_flag |= arg->maybe_flag; }
  inline void maybe_smaller() { maybe_flag = true; }
  /* Return true iff it's a single-point null interval */
  inline bool is_null_interval() { return maybe_null() && max_value[0] == 1; }
  inline int cmp_min_to_min(const SEL_ARG *arg) const {
    return sel_cmp(field, min_value, arg->min_value, min_flag, arg->min_flag);
  }
  inline int cmp_min_to_max(const SEL_ARG *arg) const {
    return sel_cmp(field, min_value, arg->max_value, min_flag, arg->max_flag);
  }
  inline int cmp_max_to_max(const SEL_ARG *arg) const {
    return sel_cmp(field, max_value, arg->max_value, max_flag, arg->max_flag);
  }
  inline int cmp_max_to_min(const SEL_ARG *arg) const {
    return sel_cmp(field, max_value, arg->min_value, max_flag, arg->min_flag);
  }
  SEL_ARG *clone_and(SEL_ARG *arg,
                     MEM_ROOT *mem_root) {  // Get intersection of ranges.
    uchar *new_min, *new_max;
    uint8 flag_min, flag_max;
    if (cmp_min_to_min(arg) >= 0) {
      new_min = min_value;
      flag_min = min_flag;
    } else {
      new_min = arg->min_value;
      flag_min = arg->min_flag; /* purecov: deadcode */
    }
    if (cmp_max_to_max(arg) <= 0) {
      new_max = max_value;
      flag_max = max_flag;
    } else {
      new_max = arg->max_value;
      flag_max = arg->max_flag;
    }
    return new (mem_root)
        SEL_ARG(field, part, new_min, new_max, flag_min, flag_max,
                maybe_flag && arg->maybe_flag, is_ascending,
                min_flag & GEOM_FLAG ? rkey_func_flag : HA_READ_INVALID);
  }
  SEL_ARG *clone_first(SEL_ARG *arg,
                       MEM_ROOT *mem_root) {  // arg->min <= X < arg->min
    return new (mem_root) SEL_ARG(
        field, part, min_value, arg->min_value, min_flag,
        arg->min_flag & NEAR_MIN ? 0 : NEAR_MAX, maybe_flag || arg->maybe_flag,
        is_ascending, min_flag & GEOM_FLAG ? rkey_func_flag : HA_READ_INVALID);
  }
  SEL_ARG *clone_last(SEL_ARG *arg,
                      MEM_ROOT *mem_root) {  // arg->min <= X <= key_max
    return new (mem_root)
        SEL_ARG(field, part, min_value, arg->max_value, min_flag, arg->max_flag,
                maybe_flag || arg->maybe_flag, is_ascending,
                min_flag & GEOM_FLAG ? rkey_func_flag : HA_READ_INVALID);
  }
  SEL_ARG *clone(RANGE_OPT_PARAM *param, SEL_ARG *new_parent, SEL_ARG **next);

  bool copy_min(SEL_ARG *arg) {  // max(this->min, arg->min) <= x <= this->max
    if (cmp_min_to_min(arg) > 0) {
      min_value = arg->min_value;
      min_flag = arg->min_flag;
      if ((max_flag & NO_MAX_RANGE) && (min_flag & NO_MIN_RANGE))
        return true;  // Full range
    }
    maybe_flag |= arg->maybe_flag;
    return false;
  }
  bool copy_max(SEL_ARG *arg) {  // this->min <= x <= min(this->max, arg->max)
    if (cmp_max_to_max(arg) <= 0) {
      max_value = arg->max_value;
      max_flag = arg->max_flag;
      if ((max_flag & NO_MAX_RANGE) && (min_flag & NO_MIN_RANGE))
        return true;  // Full range
    }
    maybe_flag |= arg->maybe_flag;
    return false;
  }

  void copy_min_to_min(SEL_ARG *arg) {
    min_value = arg->min_value;
    min_flag = arg->min_flag;
  }
  void copy_min_to_max(SEL_ARG *arg) {
    max_value = arg->min_value;
    max_flag = arg->min_flag & NEAR_MIN ? 0 : NEAR_MAX;
  }
  void copy_max_to_min(SEL_ARG *arg) {
    min_value = arg->max_value;
    min_flag = arg->max_flag & NEAR_MAX ? 0 : NEAR_MIN;
  }

  /**
    Set spatial index range scan parameters. This object will be used to do
    spatial index range scan after this call.

    @param rkey_func The scan function to perform. It must be one of the
                     spatial index specific scan functions.
  */
  void set_gis_index_read_function(const enum ha_rkey_function rkey_func) {
    assert(rkey_func >= HA_READ_MBR_CONTAIN && rkey_func <= HA_READ_MBR_EQUAL);
    min_flag = GEOM_FLAG;
    rkey_func_flag = rkey_func;
    max_flag = NO_MAX_RANGE;
  }

  /* returns a number of keypart values (0 or 1) appended to the key buffer */
  int store_min_value(uint length, uchar **min_key, uint min_key_flag) {
    /* "(kp1 > c1) AND (kp2 OP c2) AND ..." -> (kp1 > c1) */
    if ((min_flag & GEOM_FLAG) ||
        (!(min_flag & NO_MIN_RANGE) &&
         !(min_key_flag & (NO_MIN_RANGE | NEAR_MIN)))) {
      if (maybe_null() && *min_value) {
        **min_key = 1;
        memset(*min_key + 1, 0, length - 1);
      } else
        memcpy(*min_key, min_value, length);
      (*min_key) += length;
      return 1;
    }
    return 0;
  }
  /* returns a number of keypart values (0 or 1) appended to the key buffer */
  int store_max_value(uint length, uchar **max_key, uint max_key_flag) {
    if (!(max_flag & NO_MAX_RANGE) &&
        !(max_key_flag & (NO_MAX_RANGE | NEAR_MAX))) {
      if (maybe_null() && *max_value) {
        **max_key = 1;
        memset(*max_key + 1, 0, length - 1);
      } else
        memcpy(*max_key, max_value, length);
      (*max_key) += length;
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

    Note: Caller of this function should take care of sending the
    correct flags and correct key to be stored into.  In case of
    ascending indexes, store_min_key() gets called to store the
    min_value to range start_key. In case of descending indexes, it's
    called for storing min_value to range end_key.
  */
  /**
    Helper function for storing min/max values of SEL_ARG taking into account
    key part's order.
  */
  void store_min_max_values(uint length, uchar **min_key, uint min_flag,
                            uchar **max_key, uint max_flag, int *min_part,
                            int *max_part) {
    if (is_ascending) {
      *min_part += store_min_value(length, min_key, min_flag);
      *max_part += store_max_value(length, max_key, max_flag);
    } else {
      *max_part += store_min_value(length, max_key, min_flag);
      *min_part += store_max_value(length, min_key, max_flag);
    }
  }

  /**
    Helper function for storing min/max keys of next SEL_ARG taking into
    account key part's order.

    @note On checking min/max flags: Flags are used to track whether there's
    a partial key in the key buffer. So for ASC key parts the flag
    corresponding to the key being added to should be checked, not
    corresponding to the value being added. I.e min_flag for min_key.
    For DESC key parts it's opposite - max_flag for min_key. It's flag
    of prev key part that should be checked.

  */
  void store_next_min_max_keys(KEY_PART *key, uchar **cur_min_key,
                               uint *cur_min_flag, uchar **cur_max_key,
                               uint *cur_max_flag, int *min_part,
                               int *max_part) {
    assert(next_key_part);
    const bool asc = next_key_part->root->is_ascending;
    if (!get_min_flag()) {
      if (asc)
        *min_part += next_key_part->store_min_key(key, cur_min_key,
                                                  cur_min_flag, MAX_KEY, true);
      else {
        uint tmp_flag = invert_min_flag(*cur_min_flag);
        *min_part += next_key_part->store_max_key(key, cur_min_key, &tmp_flag,
                                                  MAX_KEY, true);
        *cur_min_flag = invert_max_flag(tmp_flag);
      }
    }
    if (!get_max_flag()) {
      if (asc)
        *max_part += next_key_part->store_max_key(key, cur_max_key,
                                                  cur_max_flag, MAX_KEY, false);
      else {
        uint tmp_flag = invert_max_flag(*cur_max_flag);
        *max_part += next_key_part->store_min_key(key, cur_max_key, &tmp_flag,
                                                  MAX_KEY, false);
        *cur_max_flag = invert_min_flag(tmp_flag);
      }
    }
  }

  SEL_ARG *rb_insert(SEL_ARG *leaf);
  friend SEL_ARG *rb_delete_fixup(SEL_ARG *root, SEL_ARG *key, SEL_ARG *par);
#ifndef NDEBUG
  friend int test_rb_tree(SEL_ARG *element, SEL_ARG *parent);
#endif
  SEL_ARG *first();
  const SEL_ARG *first() const;
  SEL_ARG *last();
  void make_root() {
    left = right = opt_range::null_element;
    color = BLACK;
    parent = next = prev = nullptr;
  }

  inline SEL_ARG **parent_ptr() {
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
      true   This SEL_ARG object represents a singlepoint interval
      false  Otherwise
  */

  bool is_singlepoint() const {
    /*
      Check for NEAR_MIN ("strictly less") and NO_MIN_RANGE (-inf < field)
      flags, and the same for right edge.
    */
    if (min_flag || max_flag) return false;
    uchar *min_val = min_value;
    uchar *max_val = max_value;

    if (maybe_null()) {
      /* First byte is a NULL value indicator */
      if (*min_val != *max_val) return false;

      if (*min_val) return true; /* This "x IS NULL" */
      min_val++;
      max_val++;
    }
    return !field->key_cmp(min_val, max_val);
  }
  /**
    Return correct min_flag.

    For DESC key parts max flag should be used as min flag, but in order to
    be checked correctly, max flag should be flipped as code doesn't expect
    e.g NEAR_MAX in min flag.
  */
  uint get_min_flag() {
    return (is_ascending ? min_flag : invert_max_flag(max_flag));
  }
  /**
    Return correct max_flag.

    For DESC key parts min flag should be used as max flag, but in order to
    be checked correctly, min flag should be flipped as code doesn't expect
    e.g NEAR_MIN in max flag.
  */
  uint get_max_flag() {
    return (is_ascending ? max_flag : invert_min_flag(min_flag));
  }
};

inline bool SEL_ROOT::is_always() const {
  return type == Type::KEY_RANGE && elements == 1 && !root->maybe_flag &&
         (root->min_flag & NO_MIN_RANGE) && (root->max_flag & NO_MAX_RANGE);
}

inline bool SEL_ROOT::simple_key() const {
  return elements == 1 && !root->next_key_part;
}

class SEL_TREE {
 public:
  /**
    Starting an effort to document this field:

    IMPOSSIBLE: if keys[i]->type == SEL_ROOT::Type::IMPOSSIBLE for some i,
      then type == SEL_TREE::IMPOSSIBLE. Rationale: if the predicate for
      one of the indexes is always false, then the full predicate is also
      always false.

    ALWAYS: if either (keys[i]->is_always()) or (keys[i] == NULL) for all i,
      then type == SEL_TREE::ALWAYS. Rationale: the range access method
      will not be able to filter out any rows when there are no range
      predicates that can be used to filter on any index.

    KEY: There are range predicates that can be used on at least one
      index.
  */
  enum Type { IMPOSSIBLE, ALWAYS, KEY } type;

  /**
    Whether this SEL_TREE is an inexact (too broad) representation of the
    predicates it is based on; that is, if it does not necessarily subsume
    all of them. Note that a nullptr return from get_mm_tree() (which means
    “could not generate a tree from this predicate”) is by definition inexact.

    There are two main ways a SEL_TREE can become inexact:

      - The predicate references fields not contained in any indexes tracked
        by the SEL_TREE.
      - The predicate could be of a form that is not representable as a range.
        E.g., x > 30 is a range, x mod 2 = 1 is not (although it could
        in theory be converted to a large amount of disjunct ranges).

    If a SEL_TREE is inexact, the predicates must be rechecked after the
    range scan, using a filter. (Note that it is never too narrow, only ever
    exact or too broad.) The old join optimizer always does this, no matter
    what the inexact flag is set to.

    Note that additional checks are needed to subsume a predicate even if
    inexact == false. In particular, SEL_TREE contains information for all
    indexes over a table, but if a regular range scan is chosen, it can use
    only one index. So one must then go through all predicates to see if they
    refer to fields not contained in the given index. Furthermore, range scans
    on composite (multi-part) indexes can drop predicates on the later keyparts
    (making predicates on those keyparts inexact), since range scans only
    support inequalities on the last keypart in any given range. This check
    must be done in get_ranges_from_tree().
   */
  bool inexact = false;

  SEL_TREE(enum Type type_arg, MEM_ROOT *root, size_t num_keys)
      : type(type_arg), keys(root, num_keys), n_ror_scans(0) {}
  SEL_TREE(MEM_ROOT *root, size_t num_keys)
      : type(KEY), keys(root, num_keys), n_ror_scans(0) {}
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

    Note: If you want to set an element in keys[], use set_key()
    or release_key() to make sure the SEL_ARG's use_count is correctly
    updated.
  */
  Mem_root_array<SEL_ROOT *> keys;
  Key_map keys_map; /* bitmask of non-NULL elements in keys */

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
  Key_map ror_scans_map; /* bitmask of ROR scan-able elements in keys */
  uint n_ror_scans;      /* number of set bits in ror_scans_map */

  ROR_SCAN_INFO **ror_scans;     /* list of ROR key scans */
  ROR_SCAN_INFO **ror_scans_end; /* last ROR scan */
  /* Note that #records for each key scan is stored in table->quick_rows */

  /**
    Convenience function for removing an element in keys[]. The typical
    use for this function is to disconnect the next_key_part from the
    root, send it to key_and() or key_or(), and then connect the
    result of that function back to the SEL_ROOT using set_key().

    @param index Which index slot to release.
    @return The value in the slot (before removal).
  */
  SEL_ROOT *release_key(int index) {
    SEL_ROOT *ret = keys[index];
    if (keys[index]) {
      assert(keys[index]->use_count > 0);
      --keys[index]->use_count;
    }
    keys[index] = nullptr;
    return ret;
  }

  /**
    Convenience function for changing an element in keys[], including
    updating the use_count.

    @param index Which index slot to change.
    @param key The new contents of the index slot. Is allowed to be nullptr.
  */
  void set_key(int index, SEL_ROOT *key) {
    release_key(index);
    keys[index] = key;
    if (key) ++key->use_count;
  }
};

#ifndef NDEBUG
void print_sel_tree(RANGE_OPT_PARAM *param, SEL_TREE *tree, Key_map *tree_map,
                    const char *msg);
#endif

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

   This function effectively tests requirement WA2.

  @param[in]   key_part_num   Key part number we want the SEL_ARG tree for
  @param[in]   keypart_tree   The SEL_ARG* tree for the index
  @param[out]  cur_range      The SEL_ARG tree, if any, for the keypart

  @retval true   'keypart_tree' contained a predicate for key part that
                  is not conjunction to all predicates on earlier keyparts
  @retval false  otherwise
*/
bool get_sel_root_for_keypart(uint key_part_num, SEL_ROOT *keypart_tree,
                              SEL_ROOT **cur_range);

/*
  Find the SEL_ROOT tree that corresponds to the chosen index.

  SYNOPSIS
    get_index_range_tree()
    index     [in]  The ID of the index being looked for
    range_tree[in]  Tree of ranges being searched
    param     [in]  RANGE_OPT_PARAM from test_quick_select

  DESCRIPTION

    A SEL_TREE contains range trees for all usable indexes. This procedure
    finds the SEL_ROOT tree for 'index'. The members of a SEL_TREE are
    ordered in the same way as the members of RANGE_OPT_PARAM::key, thus we
    first find the corresponding index in the array RANGE_OPT_PARAM::key.
    This index is returned through the variable param_idx, to be used later
    as argument of check_quick_select().

  RETURN
    Pointer to the SEL_ROOT tree that corresponds to index.
*/

inline SEL_ROOT *get_index_range_tree(uint index, SEL_TREE *range_tree,
                                      RANGE_OPT_PARAM *param) {
  uint idx = 0; /* Index nr in param->key_parts */
  while (idx < param->keys) {
    if (index == param->real_keynr[idx]) break;
    idx++;
  }
  return (range_tree->keys[idx]);
}

/**
  Print the ranges in a SEL_TREE to debug log.

  @param tree_name   Descriptive name of the tree
  @param tree        The SEL_TREE that will be printed to debug log
  @param param       RANGE_OPT_PARAM from test_quick_select
*/
inline void dbug_print_tree([[maybe_unused]] const char *tree_name,
                            [[maybe_unused]] SEL_TREE *tree,
                            [[maybe_unused]] const RANGE_OPT_PARAM *param) {
#ifndef NDEBUG
  if (_db_enabled_()) print_tree(nullptr, tree_name, tree, param, true);
#endif
}

#endif  // SQL_RANGE_OPTIMIZER_TREE_H_
