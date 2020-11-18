/* Copyright (c) 2000, 2020, Oracle and/or its affiliates.

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

#ifndef OPT_RANGE_INTERNAL_INCLUDED
#define OPT_RANGE_INTERNAL_INCLUDED

#include "sql/opt_range.h"

#include "mysys_err.h"          // EE_CAPACITY_EXCEEDED
#include "sql/derror.h"         // ER_THD
#include "sql/error_handler.h"  // Internal_error_handler
#include "sql/sql_class.h"      // THD

class RANGE_OPT_PARAM;
struct ROR_SCAN_INFO;
class SEL_IMERGE;
class SEL_TREE;

SEL_TREE *get_mm_tree(RANGE_OPT_PARAM *param, Item *cond);
void print_tree(String *out, const char *tree_name, SEL_TREE *tree,
                const RANGE_OPT_PARAM *param, const bool print_full)
    MY_ATTRIBUTE((unused));

// Note: tree1 and tree2 are not usable by themselves after tree_and() or
// tree_or().
SEL_TREE *tree_and(RANGE_OPT_PARAM *param, SEL_TREE *tree1, SEL_TREE *tree2);
SEL_TREE *tree_or(RANGE_OPT_PARAM *param, SEL_TREE *tree1, SEL_TREE *tree2);
int sel_cmp(Field *f, uchar *a, uchar *b, uint8 a_flag, uint8 b_flag);
SEL_ROOT *key_or(RANGE_OPT_PARAM *param, SEL_ROOT *key1, SEL_ROOT *key2);
SEL_ROOT *key_and(RANGE_OPT_PARAM *param, SEL_ROOT *key1, SEL_ROOT *key2);

void append_range(String *out, const KEY_PART_INFO *key_parts,
                  const uchar *min_key, const uchar *max_key, const uint flag);
class Opt_trace_array;
void append_range_all_keyparts(Opt_trace_array *range_trace,
                               String *range_string, String *range_so_far,
                               SEL_ROOT *keypart,
                               const KEY_PART_INFO *key_parts,
                               const bool print_full);

/**
  Shared sentinel node for all trees. Initialized by range_optimizer_init(),
  destroyed by range_optimizer_free();
  Put it in a namespace, to avoid possible conflicts with the global namespace.
*/
namespace opt_range {
extern SEL_ARG *null_element;
}

/**
  Error handling class for range optimizer. We handle only out of memory
  error here. This is to give a hint to the user to
  raise range_optimizer_max_mem_size if required.
  Warning for the memory error is pushed only once. The consequent errors
  will be ignored.
*/
class Range_optimizer_error_handler : public Internal_error_handler {
 public:
  Range_optimizer_error_handler()
      : m_has_errors(false), m_is_mem_error(false) {}

  bool handle_condition(THD *thd, uint sql_errno, const char *,
                        Sql_condition::enum_severity_level *level,
                        const char *) override {
    if (*level == Sql_condition::SL_ERROR) {
      m_has_errors = true;
      /* Out of memory error is reported only once. Return as handled */
      if (m_is_mem_error && sql_errno == EE_CAPACITY_EXCEEDED) return true;
      if (sql_errno == EE_CAPACITY_EXCEEDED) {
        m_is_mem_error = true;
        /* Convert the error into a warning. */
        *level = Sql_condition::SL_WARNING;
        push_warning_printf(
            thd, Sql_condition::SL_WARNING, ER_CAPACITY_EXCEEDED,
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

/**
  A helper function to invert min flags to max flags for DESC key parts.
  It changes NEAR_MIN, NO_MIN_RANGE to NEAR_MAX, NO_MAX_RANGE appropriately
*/

static uint invert_min_flag(uint min_flag) {
  uint max_flag_out = min_flag & ~(NEAR_MIN | NO_MIN_RANGE);
  if (min_flag & NEAR_MIN) max_flag_out |= NEAR_MAX;
  if (min_flag & NO_MIN_RANGE) max_flag_out |= NO_MAX_RANGE;
  return max_flag_out;
}

/**
  A helper function to invert max flags to min flags for DESC key parts.
  It changes NEAR_MAX, NO_MAX_RANGE to NEAR_MIN, NO_MIN_RANGE appropriately
*/

static uint invert_max_flag(uint max_flag) {
  uint min_flag_out = max_flag & ~(NEAR_MAX | NO_MAX_RANGE);
  if (max_flag & NEAR_MAX) min_flag_out |= NEAR_MIN;
  if (max_flag & NO_MAX_RANGE) min_flag_out |= NO_MIN_RANGE;
  return min_flag_out;
}

class SEL_ARG;

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
  ~SEL_ROOT() { DBUG_ASSERT(use_count == 0); }

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
      DBUG_ASSERT(next_key_part->use_count > 0);
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

  SEL_ARG() {}
  SEL_ARG(SEL_ARG &);
  SEL_ARG(Field *, const uchar *, const uchar *, bool asc);
  SEL_ARG(Field *field, uint8 part, uchar *min_value, uchar *max_value,
          uint8 min_flag, uint8 max_flag, bool maybe_flag, bool asc);
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
                maybe_flag && arg->maybe_flag, is_ascending);
  }
  SEL_ARG *clone_first(SEL_ARG *arg,
                       MEM_ROOT *mem_root) {  // arg->min <= X < arg->min
    return new (mem_root)
        SEL_ARG(field, part, min_value, arg->min_value, min_flag,
                arg->min_flag & NEAR_MIN ? 0 : NEAR_MAX,
                maybe_flag || arg->maybe_flag, is_ascending);
  }
  SEL_ARG *clone_last(SEL_ARG *arg,
                      MEM_ROOT *mem_root) {  // arg->min <= X <= key_max
    return new (mem_root)
        SEL_ARG(field, part, min_value, arg->max_value, min_flag, arg->max_flag,
                maybe_flag || arg->maybe_flag, is_ascending);
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
    DBUG_ASSERT(rkey_func >= HA_READ_MBR_CONTAIN &&
                rkey_func <= HA_READ_MBR_EQUAL);
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
    DBUG_ASSERT(next_key_part);
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
#ifndef DBUG_OFF
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

    KEY_SMALLER: There are range predicates that can be used on at
      least one index. In addition, there are predicates that cannot
      be directly utilized by range access on key parts in the same
      index. These unused predicates makes it probable that the row
      estimate for range access on this index is too pessimistic.
  */
  enum Type { IMPOSSIBLE, ALWAYS, MAYBE, KEY, KEY_SMALLER } type;

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
      DBUG_ASSERT(keys[index]->use_count > 0);
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

class RANGE_OPT_PARAM {
 public:
  THD *thd;               /* Current thread handle */
  TABLE *table;           /* Table being analyzed */
  SELECT_LEX *select_lex; /* Query block the table is part of */
  Item *cond;             /* Used inside get_mm_tree(). */
  table_map prev_tables;
  table_map read_tables;
  table_map current_table; /* Bit of the table being analyzed */

  /* Array of parts of all keys for which range analysis is performed */
  KEY_PART *key_parts;
  KEY_PART *key_parts_end;
  MEM_ROOT
  *mem_root; /* Memory that will be freed when range analysis completes */
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
    using_real_indexes==true
  */
  uint real_keynr[MAX_KEY];

  /*
    Used to store 'current key tuples', in both range analysis and
    partitioning (list) analysis
  */
  uchar min_key[MAX_KEY_LENGTH + MAX_FIELD_WIDTH],
      max_key[MAX_KEY_LENGTH + MAX_FIELD_WIDTH];

  bool force_default_mrr;
  /**
    Whether index statistics or index dives should be used when
    estimating the number of rows in an equality range. If true, index
    statistics is used for these indexes.
  */
  bool use_index_statistics;

  /// Error handler for this param.

  Range_optimizer_error_handler error_handler;

  bool has_errors() const { return (error_handler.has_errors()); }

  virtual ~RANGE_OPT_PARAM() {}
};

/*
  SEL_IMERGE is a list of possible ways to do index merge, i.e. it is
  a condition in the following form:
   (t_1||t_2||...||t_N) && (next)

  where all t_i are SEL_TREEs, next is another SEL_IMERGE and no pair
  (t_i,t_j) contains SEL_ARGS for the same index.

  SEL_TREE contained in SEL_IMERGE always has merges=NULL.

  This class relies on memory manager to do the cleanup.
*/

class SEL_IMERGE {
  enum { PREALLOCED_TREES = 10 };

 public:
  SEL_TREE *trees_prealloced[PREALLOCED_TREES];
  SEL_TREE **trees;      /* trees used to do index_merge   */
  SEL_TREE **trees_next; /* last of these trees            */
  SEL_TREE **trees_end;  /* end of allocated space         */

  SEL_ARG ***best_keys; /* best keys to read in SEL_TREEs */

  SEL_IMERGE()
      : trees(&trees_prealloced[0]),
        trees_next(trees),
        trees_end(trees + PREALLOCED_TREES) {}
  SEL_IMERGE(SEL_IMERGE *arg, RANGE_OPT_PARAM *param);
  int or_sel_tree(RANGE_OPT_PARAM *param, SEL_TREE *tree);
  int or_sel_tree_with_checks(RANGE_OPT_PARAM *param, SEL_TREE *new_tree);
  int or_sel_imerge_with_checks(RANGE_OPT_PARAM *param, SEL_IMERGE *imerge);
};

#endif  // OPT_RANGE_INTERNAL_INCLUDED
