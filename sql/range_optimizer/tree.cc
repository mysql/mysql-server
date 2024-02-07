/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

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

#include "sql/range_optimizer/tree.h"

#include <algorithm>
#include <set>
#include <utility>

#include "m_string.h"
#include "memory_debugging.h"
#include "my_dbug.h"
#include "my_sqlcommand.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/strings/m_ctype.h"
#include "mysqld_error.h"
#include "sql/handler.h"
#include "sql/key.h"
#include "sql/key_spec.h"
#include "sql/range_optimizer/internal.h"
#include "sql/range_optimizer/range_opt_param.h"
#include "sql/range_optimizer/range_optimizer.h"
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql_string.h"

using std::max;
using std::min;

// Note: tree1 and tree2 are not usable by themselves after tree_and() or
// tree_or().
SEL_TREE *tree_and(RANGE_OPT_PARAM *param, SEL_TREE *tree1, SEL_TREE *tree2);
SEL_TREE *tree_or(RANGE_OPT_PARAM *param, bool remove_jump_scans,
                  SEL_TREE *tree1, SEL_TREE *tree2);
SEL_ROOT *key_or(RANGE_OPT_PARAM *param, SEL_ROOT *key1, SEL_ROOT *key2);
SEL_ROOT *key_and(RANGE_OPT_PARAM *param, SEL_ROOT *key1, SEL_ROOT *key2);

SEL_ARG *rb_delete_fixup(SEL_ARG *root, SEL_ARG *key, SEL_ARG *par);
#ifndef NDEBUG
int test_rb_tree(SEL_ARG *element, SEL_ARG *parent);
#endif

static bool eq_tree(const SEL_ROOT *a, const SEL_ROOT *b);
static bool eq_tree(const SEL_ARG *a, const SEL_ARG *b);
static bool get_range(SEL_ARG **e1, SEL_ARG **e2, const SEL_ROOT *root1);

using opt_range::null_element;

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

int SEL_ROOT::store_min_key(KEY_PART *key, uchar **range_key,
                            uint *range_key_flag, uint last_part,
                            bool start_key) {
  SEL_ARG *key_tree = root->first();
  uint res = key_tree->store_min_value(key[key_tree->part].store_length,
                                       range_key, *range_key_flag);
  // We've stored min_value, so append min_flag
  *range_key_flag |= key_tree->min_flag;
  if (key_tree->next_key_part &&
      key_tree->next_key_part->type == SEL_ROOT::Type::KEY_RANGE &&
      key_tree->part != last_part &&
      key_tree->next_key_part->root->part == key_tree->part + 1 &&
      !(*range_key_flag & (NO_MIN_RANGE | NEAR_MIN))) {
    const bool asc = key_tree->next_key_part->root->is_ascending;
    if ((start_key && asc) || (!start_key && !asc))
      res += key_tree->next_key_part->store_min_key(
          key, range_key, range_key_flag, last_part, start_key);
    else {
      uint tmp_flag = invert_min_flag(*range_key_flag);
      res += key_tree->next_key_part->store_max_key(key, range_key, &tmp_flag,
                                                    last_part, start_key);
      *range_key_flag = invert_max_flag(tmp_flag);
    }
  }
  return res;
}

/*
  Returns the number of keypart values appended to the key buffer.

  Note: Caller of this function should take care of sending the
  correct flags and correct key to be stored into.  In case of
  ascending indexes, store_max_key() gets called while storing the
  max_value into range end_key. In case of descending indexes,
  its max_value to range start_key.
*/

int SEL_ROOT::store_max_key(KEY_PART *key, uchar **range_key,
                            uint *range_key_flag, uint last_part,
                            bool start_key) {
  SEL_ARG *key_tree = root->last();
  uint res = key_tree->store_max_value(key[key_tree->part].store_length,
                                       range_key, *range_key_flag);
  // We've stored max value, so return max_flag
  (*range_key_flag) |= key_tree->max_flag;
  if (key_tree->next_key_part &&
      key_tree->next_key_part->type == SEL_ROOT::Type::KEY_RANGE &&
      key_tree->part != last_part &&
      key_tree->next_key_part->root->part == key_tree->part + 1 &&
      !(*range_key_flag & (NO_MAX_RANGE | NEAR_MAX))) {
    const bool asc = key_tree->next_key_part->root->is_ascending;
    if ((!start_key && asc) || (start_key && !asc))
      res += key_tree->next_key_part->store_max_key(
          key, range_key, range_key_flag, last_part, start_key);
    else {
      uint tmp_flag = invert_max_flag(*range_key_flag);
      res += key_tree->next_key_part->store_min_key(key, range_key, &tmp_flag,
                                                    last_part, start_key);
      *range_key_flag = invert_min_flag(tmp_flag);
    }
  }
  return res;
}

void SEL_ROOT::free_tree() {
  if (use_count == 0) {
    for (SEL_ARG *pos = root->first(); pos; pos = pos->next) {
      SEL_ROOT *root = pos->release_next_key_part();
      if (root) root->free_tree();
    }
  }
}

/**
  Helper function to compare two SEL_ROOTs.
*/
static bool all_same(const SEL_ROOT *sa1, const SEL_ROOT *sa2) {
  if (sa1 == nullptr && sa2 == nullptr) return true;
  if ((sa1 != nullptr && sa2 == nullptr) || (sa1 == nullptr && sa2 != nullptr))
    return false;
  if (sa1->type == SEL_ROOT::Type::KEY_RANGE &&
      sa2->type == SEL_ROOT::Type::KEY_RANGE) {
    const SEL_ARG *sa1_arg = sa1->root->first();
    const SEL_ARG *sa2_arg = sa2->root->first();
    for (; sa1_arg && sa2_arg && sa1_arg->is_same(sa2_arg);
         sa1_arg = sa1_arg->next, sa2_arg = sa2_arg->next)
      ;
    if (sa1_arg || sa2_arg) return false;
    return true;
  } else
    return sa1->type == sa2->type;
}

SEL_TREE::SEL_TREE(SEL_TREE *arg, RANGE_OPT_PARAM *param)
    : keys(param->temp_mem_root, param->keys), n_ror_scans(0) {
  keys_map = arg->keys_map;
  type = arg->type;
  for (uint idx = 0; idx < param->keys; idx++) {
    if (arg->keys[idx]) {
      set_key(idx, arg->keys[idx]->clone_tree(param));
      if (!keys[idx]) break;
    } else
      set_key(idx, nullptr);
  }

  List_iterator<SEL_IMERGE> it(arg->merges);
  for (SEL_IMERGE *el = it++; el; el = it++) {
    SEL_IMERGE *merge = new (param->temp_mem_root) SEL_IMERGE(el, param);
    if (!merge || merge->trees.empty() || param->has_errors()) {
      merges.clear();
      return;
    }
    merges.push_back(merge);
  }

  /*
    SEL_TREEs are only created by get_mm_tree() (and functions called
    by get_mm_tree()). Index intersection is checked after
    get_mm_tree() has constructed all ranges. In other words, there
    should not be any ROR scans to copy when this ctor is called.
  */
  assert(n_ror_scans == 0);
}

/*
  Perform AND operation on two index_merge lists and store result in *im1.
*/

inline void imerge_list_and_list(List<SEL_IMERGE> *im1, List<SEL_IMERGE> *im2) {
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

    If (a_1||b_1) produce a condition that is always true, NULL is returned
    and index_merge is discarded (while it is actually possible to try
    harder).

    As a consequence of this, choice of keys to do index_merge read may depend
    on the order of conditions in WHERE part of the query.

  RETURN
    0     OK, result is stored in *im1
    other Error, both passed lists are unusable
*/

static int imerge_list_or_list(RANGE_OPT_PARAM *param, bool remove_jump_scans,
                               List<SEL_IMERGE> *im1, List<SEL_IMERGE> *im2) {
  SEL_IMERGE *imerge = im1->head();
  im1->clear();
  im1->push_back(imerge);

  return imerge->or_sel_imerge_with_checks(param, remove_jump_scans,
                                           im2->head());
}

/*
  Perform OR operation on index_merge list and key tree.

  RETURN
    false     OK, result is stored in *im1.
    true      Error
*/

static bool imerge_list_or_tree(RANGE_OPT_PARAM *param, bool remove_jump_scans,
                                List<SEL_IMERGE> *im1, SEL_TREE *tree) {
  DBUG_TRACE;
  SEL_IMERGE *imerge;
  List_iterator<SEL_IMERGE> it(*im1);

  uint remaining_trees = im1->elements;
  while ((imerge = it++)) {
    SEL_TREE *or_tree;
    /*
      Need to make a copy of 'tree' for all but the last OR operation
      because or_sel_tree_with_checks() may change it.
    */
    if (--remaining_trees == 0)
      or_tree = tree;
    else {
      or_tree = new (param->temp_mem_root) SEL_TREE(tree, param);
      if (!or_tree || param->has_errors()) return true;
      if (or_tree->keys_map.is_clear_all() && or_tree->merges.is_empty())
        return false;
    }

    int result_or =
        imerge->or_sel_tree_with_checks(param, remove_jump_scans, or_tree);
    if (result_or == 1)
      it.remove();
    else if (result_or == -1)
      return true;
  }
  assert(remaining_trees == 0);
  return im1->is_empty();
}

SEL_ARG::SEL_ARG(SEL_ARG &arg)
    : min_flag(arg.min_flag),
      max_flag(arg.max_flag),
      maybe_flag(arg.maybe_flag),
      part(arg.part),
      rkey_func_flag(arg.rkey_func_flag),
      field(arg.field),
      min_value(arg.min_value),
      max_value(arg.max_value),
      left(null_element),
      right(null_element),
      next(nullptr),
      prev(nullptr),
      next_key_part(arg.next_key_part),
      is_ascending(arg.is_ascending) {
  if (next_key_part) ++next_key_part->use_count;
}

SEL_ARG::SEL_ARG(Field *f, const uchar *min_value_arg,
                 const uchar *max_value_arg, bool asc)
    : part(0),
      rkey_func_flag(HA_READ_INVALID),
      field(f),
      min_value(const_cast<uchar *>(min_value_arg)),
      max_value(const_cast<uchar *>(max_value_arg)),
      left(null_element),
      right(null_element),
      next(nullptr),
      prev(nullptr),
      parent(nullptr),
      color(BLACK),
      is_ascending(asc) {}

SEL_ARG::SEL_ARG(Field *field_, uint8 part_, uchar *min_value_,
                 uchar *max_value_, uint8 min_flag_, uint8 max_flag_,
                 bool maybe_flag_, bool asc, ha_rkey_function gis_flag)
    : min_flag(min_flag_),
      max_flag(max_flag_),
      maybe_flag(maybe_flag_),
      part(part_),
      rkey_func_flag(gis_flag),
      field(field_),
      min_value(min_value_),
      max_value(max_value_),
      left(null_element),
      right(null_element),
      next(nullptr),
      prev(nullptr),
      parent(nullptr),
      color(BLACK),
      is_ascending(asc) {}

SEL_ARG *SEL_ARG::clone(RANGE_OPT_PARAM *param, SEL_ARG *new_parent,
                        SEL_ARG **next_arg) {
  SEL_ARG *tmp;

  if (param->has_errors()) return nullptr;

  if (!(tmp = new (param->temp_mem_root)
            SEL_ARG(field, part, min_value, max_value, min_flag, max_flag,
                    maybe_flag, is_ascending,
                    min_flag & GEOM_FLAG ? rkey_func_flag : HA_READ_INVALID)))
    return nullptr;  // OOM
  tmp->parent = new_parent;
  tmp->set_next_key_part(next_key_part);
  if (left == null_element || left == nullptr) {
    tmp->left = left;
  } else {
    if (!(tmp->left = left->clone(param, tmp, next_arg)))
      return nullptr;  // OOM
  }

  tmp->prev = *next_arg;  // Link into next/prev chain
  (*next_arg)->next = tmp;
  (*next_arg) = tmp;

  if (right == null_element || right == nullptr) {
    tmp->right = right;
  } else {
    if (!(tmp->right = right->clone(param, tmp, next_arg)))
      return nullptr;  // OOM
  }
  tmp->color = color;
  return tmp;
}

/**
  This gives the first SEL_ARG in the interval list, and the minimal element
  in the red-black tree

  @return
  SEL_ARG   first SEL_ARG in the interval list
*/
SEL_ARG *SEL_ARG::first() {
  SEL_ARG *next_arg = this;
  if (!next_arg->left) return nullptr;  // MAYBE_KEY
  while (next_arg->left != null_element) next_arg = next_arg->left;
  return next_arg;
}

const SEL_ARG *SEL_ARG::first() const {
  return const_cast<SEL_ARG *>(this)->first();
}

SEL_ARG *SEL_ARG::last() {
  SEL_ARG *next_arg = this;
  if (!next_arg->right) return nullptr;  // MAYBE_KEY
  while (next_arg->right != null_element) next_arg = next_arg->right;
  return next_arg;
}

/*
  Check if a compare is ok, when one takes ranges in account
  Returns -2 or 2 if the ranges where 'joined' like  < 2 and >= 2
*/

int sel_cmp(Field *field, uchar *a, uchar *b, uint8 a_flag, uint8 b_flag) {
  int cmp;
  /* First check if there was a compare to a min or max element */
  if (a_flag & (NO_MIN_RANGE | NO_MAX_RANGE)) {
    if ((a_flag & (NO_MIN_RANGE | NO_MAX_RANGE)) ==
        (b_flag & (NO_MIN_RANGE | NO_MAX_RANGE)))
      return 0;
    return (a_flag & NO_MIN_RANGE) ? -1 : 1;
  }
  if (b_flag & (NO_MIN_RANGE | NO_MAX_RANGE))
    return (b_flag & NO_MIN_RANGE) ? 1 : -1;

  if (field->is_nullable())  // If null is part of key
  {
    if (*a != *b) {
      return *a ? -1 : 1;
    }
    if (*a) goto end;  // NULL where equal
    a++;
    b++;  // Skip NULL marker
  }
  cmp = field->key_cmp(a, b);
  if (cmp) return cmp < 0 ? -1 : 1;  // The values differed

  // Check if the compared equal arguments was defined with open/closed range
end:
  if (a_flag & (NEAR_MIN | NEAR_MAX)) {
    if ((a_flag & (NEAR_MIN | NEAR_MAX)) == (b_flag & (NEAR_MIN | NEAR_MAX)))
      return 0;
    if (!(b_flag & (NEAR_MIN | NEAR_MAX))) return (a_flag & NEAR_MIN) ? 2 : -2;
    return (a_flag & NEAR_MIN) ? 1 : -1;
  }
  if (b_flag & (NEAR_MIN | NEAR_MAX)) return (b_flag & NEAR_MIN) ? -2 : 2;
  return 0;  // The elements where equal
}

namespace {

size_t count_elements(const SEL_ARG *arg) {
  size_t elements = 1;
  assert(arg->left);
  assert(arg->right);
  if (arg->left && arg->left != null_element)
    elements += count_elements(arg->left);
  if (arg->right && arg->right != null_element)
    elements += count_elements(arg->right);
  return elements;
}

}  // Namespace

SEL_ROOT::SEL_ROOT(SEL_ARG *root_arg)
    : type(Type::KEY_RANGE),
      root(root_arg),
      elements(count_elements(root_arg)) {}

SEL_ROOT::SEL_ROOT(MEM_ROOT *mem_root, Type type_arg)
    : type(type_arg), root(new (mem_root) SEL_ARG()), elements(1) {
  assert(type_arg == Type::MAYBE_KEY || type_arg == Type::IMPOSSIBLE);
  root->make_root();
  if (type_arg == Type::MAYBE_KEY) {
    // See todo for left/right pointers
    root->left = root->right = nullptr;
  }
}

SEL_ROOT *SEL_ROOT::clone_tree(RANGE_OPT_PARAM *param) const {
  /*
    Only SEL_ROOTs of type KEY_RANGE has any elements that need to be cloned.
    For other types, just create a new SEL_ROOT object.
  */
  if (type != Type::KEY_RANGE)
    return new (param->temp_mem_root) SEL_ROOT(param->temp_mem_root, type);

  SEL_ARG tmp_link, *next_arg, *new_root;
  SEL_ROOT *new_tree;
  next_arg = &tmp_link;

  // Clone the underlying SEL_ARG tree, starting from the root node.
  if (!(new_root = root->clone(param, (SEL_ARG *)nullptr, &next_arg)) ||
      (param && param->has_errors()))
    return nullptr;

  // Make the SEL_ROOT itself.
  if (!(new_tree = new (param->temp_mem_root) SEL_ROOT(new_root)))
    return nullptr;
  new_tree->elements = elements;
  next_arg->next = nullptr;       // Fix last link
  tmp_link.next->prev = nullptr;  // Fix first link
  new_tree->use_count = 0;
  return new_tree;
}

SEL_TREE *tree_and(RANGE_OPT_PARAM *param, SEL_TREE *tree1, SEL_TREE *tree2) {
  DBUG_TRACE;

  if (param->has_errors()) return nullptr;

  if (tree1 == nullptr) {
    if (tree2 != nullptr) {
      tree2->inexact = true;
    }
    return tree2;
  }
  if (tree2 == nullptr) {
    if (tree1 != nullptr) {
      tree1->inexact = true;
    }
    return tree1;
  }
  if (tree1->type == SEL_TREE::IMPOSSIBLE) {
    return tree1;
  }
  if (tree2->type == SEL_TREE::IMPOSSIBLE) {
    return tree2;
  }
  if (tree2->type == SEL_TREE::ALWAYS) {
    tree1->inexact |= tree2->inexact;
    return tree1;
  }
  if (tree1->type == SEL_TREE::ALWAYS) {
    tree2->inexact |= tree1->inexact;
    return tree2;
  }

  dbug_print_tree("tree1", tree1, param);
  dbug_print_tree("tree2", tree2, param);

  Key_map result_keys;

  /* Join the trees key per key */
  for (uint idx = 0; idx < param->keys; idx++) {
    SEL_ROOT *key1 = tree1->release_key(idx);
    SEL_ROOT *key2 = tree2->release_key(idx);

    if (key1 != nullptr || key2 != nullptr) {
      if (key1 == nullptr || key2 == nullptr) {
        // If AND-ing two trees together, and one has an expression over a
        // different index from the other, we cannot guarantee that the entire
        // expression is exact if that index is chosen. (The only time this
        // really matters is when there's an AND within an OR; only the
        // hypergraph optimizer cares about the inexact flag, and it does its
        // own splitting of top-level ANDs.)
        tree1->inexact = true;
      }
      SEL_ROOT *new_key = key_and(param, key1, key2);
      tree1->set_key(idx, new_key);
      if (new_key) {
        if (new_key->type == SEL_ROOT::Type::IMPOSSIBLE) {
          tree1->type = SEL_TREE::IMPOSSIBLE;
          return tree1;
        }
        result_keys.set_bit(idx);
#ifndef NDEBUG
        /*
          Do not test use_count if there is a large range tree created.
          It takes too much time to traverse the tree.
        */
        if (param->temp_mem_root->allocated_size() < 2097152)
          new_key->test_use_count(new_key);
#endif
      }
    }
  }
  tree1->keys_map = result_keys;
  tree1->inexact |= tree2->inexact;

  /* ok, both trees are index_merge trees */
  imerge_list_and_list(&tree1->merges, &tree2->merges);
  // An index merge is a union/OR, so it cannot exactly represent an
  // intersection/AND.
  tree1->inexact |= !tree1->merges.is_empty();

  return tree1;
}

/*
  Check if two SEL_TREES can be combined into one (i.e. a single key range
  read can be constructed for "cond_of_tree1 OR cond_of_tree2" ) without
  using index_merge.
*/

bool sel_trees_can_be_ored(SEL_TREE *tree1, SEL_TREE *tree2,
                           RANGE_OPT_PARAM *param) {
  Key_map common_keys = tree1->keys_map;
  DBUG_TRACE;
  common_keys.intersect(tree2->keys_map);

  dbug_print_tree("tree1", tree1, param);
  dbug_print_tree("tree2", tree2, param);

  if (common_keys.is_clear_all()) return false;

  /* trees have a common key, check if they refer to same key part */
  for (uint key_no = 0; key_no < param->keys; key_no++) {
    if (common_keys.is_set(key_no)) {
      const SEL_ROOT *key1 = tree1->keys[key_no];
      const SEL_ROOT *key2 = tree2->keys[key_no];
      /* GIS_OPTIMIZER_FIXME: temp solution. key1 could be all nulls */
      if (key1 && key2 && key1->root->part == key2->root->part) return true;
    }
  }
  return false;
}

/*
  Remove the trees that are not suitable for record retrieval.
  SYNOPSIS
    param  Range analysis parameter
    tree   Tree to be processed, tree->type is KEY

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
    SEL_TREE that has the structure like st1 tree has, and conclude that
    tree_and(something, (*)) will not be called.

  RETURN
    0  Ok, some suitable trees left
    1  No tree->keys[] left.
*/

static bool remove_nonrange_trees(RANGE_OPT_PARAM *param, SEL_TREE *tree) {
  bool res = false;
  for (uint i = 0; i < param->keys; i++) {
    if (tree->keys[i]) {
      if (tree->keys[i]->root->part) {
        tree->keys[i] = nullptr;
        tree->keys_map.clear_bit(i);
      } else
        res = true;
    }
  }
  return !res;
}

SEL_TREE *tree_or(RANGE_OPT_PARAM *param, bool remove_jump_scans,
                  SEL_TREE *tree1, SEL_TREE *tree2) {
  DBUG_TRACE;

  if (param->has_errors()) return nullptr;

  if (!tree1 || !tree2) return nullptr;
  tree1->inexact = tree2->inexact = tree1->inexact | tree2->inexact;
  if (tree1->type == SEL_TREE::IMPOSSIBLE || tree2->type == SEL_TREE::ALWAYS)
    return tree2;
  if (tree2->type == SEL_TREE::IMPOSSIBLE || tree1->type == SEL_TREE::ALWAYS)
    return tree1;

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
  if (!tree1->merges.is_empty()) {
    for (uint i = 0; i < param->keys; i++)
      if (tree1->keys[i] != nullptr &&
          tree1->keys[i]->type == SEL_ROOT::Type::KEY_RANGE) {
        tree1->merges.clear();
        break;
      }
  }
  if (!tree2->merges.is_empty()) {
    for (uint i = 0; i < param->keys; i++)
      if (tree2->keys[i] != nullptr &&
          tree2->keys[i]->type == SEL_ROOT::Type::KEY_RANGE) {
        tree2->merges.clear();
        break;
      }
  }

  SEL_TREE *result = nullptr;
  Key_map result_keys;
  if (sel_trees_can_be_ored(tree1, tree2, param)) {
    /* Join the trees key per key */
    for (uint idx = 0; idx < param->keys; idx++) {
      SEL_ROOT *key1 = tree1->release_key(idx);
      SEL_ROOT *key2 = tree2->release_key(idx);
      SEL_ROOT *new_key = key_or(param, key1, key2);
      tree1->set_key(idx, new_key);
      if (new_key) {
        result = tree1;  // Added to tree1
        result_keys.set_bit(idx);
#ifndef NDEBUG
        /*
          Do not test use count if there is a large range tree created.
          It takes too much time to traverse the tree.
        */
        if (param->temp_mem_root->allocated_size() < 2097152)
          new_key->test_use_count(new_key);
#endif
      }
    }
    if (result) result->keys_map = result_keys;
  } else {
    /* ok, two trees have KEY type but cannot be used without index merge */
    if (tree1->merges.is_empty() && tree2->merges.is_empty()) {
      if (remove_jump_scans) {
        bool no_trees = remove_nonrange_trees(param, tree1);
        no_trees = no_trees || remove_nonrange_trees(param, tree2);
        if (no_trees)
          return new (param->temp_mem_root)
              SEL_TREE(SEL_TREE::ALWAYS, param->temp_mem_root, param->keys);
      }
      SEL_IMERGE *merge;
      /* both trees are "range" trees, produce new index merge structure */
      if (!(result = new (param->temp_mem_root)
                SEL_TREE(param->temp_mem_root, param->keys)) ||
          !(merge =
                new (param->temp_mem_root) SEL_IMERGE(param->temp_mem_root)) ||
          result->merges.push_back(merge) || merge->or_sel_tree(tree1) ||
          merge->or_sel_tree(tree2))
        result = nullptr;
      else
        result->type = tree1->type;
    } else if (!tree1->merges.is_empty() && !tree2->merges.is_empty()) {
      if (imerge_list_or_list(param, remove_jump_scans, &tree1->merges,
                              &tree2->merges))
        result = new (param->temp_mem_root)
            SEL_TREE(SEL_TREE::ALWAYS, param->temp_mem_root, param->keys);
      else
        result = tree1;
    } else {
      /* one tree is index merge tree and another is range tree */
      if (tree1->merges.is_empty()) std::swap(tree1, tree2);

      if (remove_jump_scans && remove_nonrange_trees(param, tree2))
        return new (param->temp_mem_root)
            SEL_TREE(SEL_TREE::ALWAYS, param->temp_mem_root, param->keys);
      /* add tree2 to tree1->merges, checking if it collapses to ALWAYS */
      if (imerge_list_or_tree(param, remove_jump_scans, &tree1->merges, tree2))
        result = new (param->temp_mem_root)
            SEL_TREE(SEL_TREE::ALWAYS, param->temp_mem_root, param->keys);
      else
        result = tree1;
    }
  }
  return result;
}

/**
  And key trees where key1->part < key2->part

  key2 will be connected to every key in key1, and thus
  have its use_count incremented many times. The returned node
  will not have its use_count increased; you are supposed to do
  that yourself when you connect it to a root.

  @param param Range analysis context (needed to track if we have allocated
               too many SEL_ARGs)
  @param key1 Root of first tree to AND together
  @param key2 Root of second tree to AND together
  @return Root of (key1 AND key2)
*/

static SEL_ROOT *and_all_keys(RANGE_OPT_PARAM *param, SEL_ROOT *key1,
                              SEL_ROOT *key2) {
  SEL_ARG *next;

  // We will be modifying key1, so clone it if we need to.
  if (key1->use_count > 0) {
    if (!(key1 = key1->clone_tree(param))) return nullptr;  // OOM
  }

  /*
    We will be using key2 several times, so temporarily increase
    its use_count artificially to keep key_and() below from modifying
    it in-place.

    Note that this makes test_use_count() fail since our use_count is
    now higher than the actual number of references, but that is only ever
    called from tree_and() and tree_or(), not from anything below this,
    and we undo it below.
  */
  ++key2->use_count;

  if (key1->type == SEL_ROOT::Type::MAYBE_KEY) {
    // See todo for left/right pointers
    assert(!key1->root->left);
    assert(!key1->root->right);
    key1->root->next = key1->root->prev = nullptr;
  }
  for (next = key1->root->first(); next; next = next->next) {
    if (next->next_key_part) {
      /*
        The more complicated case; there's already another AND clause,
        so we cannot connect key2 to key1 directly, but need to recurse.
      */
      SEL_ROOT *tmp = key_and(param, next->release_next_key_part(), key2);
      next->set_next_key_part(tmp);
      if (tmp && tmp->type == SEL_ROOT::Type::IMPOSSIBLE) {
        key1->tree_delete(next);
      }
    } else {
      // The trivial case.
      next->set_next_key_part(key2);
    }
  }

  // Undo the temporary use_count modification above.
  --key2->use_count;

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

  key_and() does not modify key1 nor key2 if they are in use by other roots
  (although typical use is that key1 has been disconnected from its root
  and thus can be modified in-place). Thus, it does not change their use_count
  in the typical case.

  The returned node will not have its use_count increased; you are supposed
  to do that yourself when you connect it to a root.

  RETURN
    RB-tree root of the resulting SEL_ARG graph.
    NULL if the result of AND operation is an empty interval {0}.
*/

SEL_ROOT *key_and(RANGE_OPT_PARAM *param, SEL_ROOT *key1, SEL_ROOT *key2) {
  if (param->has_errors()) return nullptr;

  if (key1 == nullptr || key1->is_always()) {
    if (key1) key1->free_tree();
    return key2;
  }
  if (key2 == nullptr || key2->is_always()) {
    if (key2) key2->free_tree();
    return key1;
  }

  if (key1->root->part != key2->root->part) {
    if (key1->root->part > key2->root->part) {
      std::swap(key1, key2);
    }
    assert(key1->root->part < key2->root->part);
    return and_all_keys(param, key1, key2);
  }

  if ((!key2->simple_key() && key1->simple_key() &&
       key2->type != SEL_ROOT::Type::MAYBE_KEY) ||
      key1->type == SEL_ROOT::Type::MAYBE_KEY) {  // Put simple key in key2
    std::swap(key1, key2);
  }

  /* If one of the key is MAYBE_KEY then the found region may be smaller */
  if (key2->type == SEL_ROOT::Type::MAYBE_KEY) {
    if (key1->use_count > 0) {
      // We are going to modify key1, so we need to clone it.
      if (!(key1 = key1->clone_tree(param))) return nullptr;  // OOM
    }
    if (key1->type == SEL_ROOT::Type::MAYBE_KEY) {  // Both are maybe key
      SEL_ROOT *new_part = key_and(param, key1->root->release_next_key_part(),
                                   key2->root->next_key_part);
      key1->root->set_next_key_part(new_part);
      return key1;
    } else {
      key1->root->maybe_smaller();
      if (key2->root->next_key_part) {
        return and_all_keys(param, key1, key2);
      } else {
        /*
          key2 is MAYBE_KEY and nothing more; simply discard it,
          since we've now moved that information into key1's maybe_flag.
        */
        key2->free_tree();
        return key1;
      }
    }
    // Unreachable.
    assert(false);
    return nullptr;
  }

  if ((key1->root->min_flag | key2->root->min_flag) & GEOM_FLAG) {
    /*
      Cannot optimize geometry ranges. The next best thing is to keep
      one of them.
    */
    key2->free_tree();
    return key1;
  }

  // Two non-overlapped key ranges for multi-valued index do not mean
  // an always false condition.
  // For example, "1 member of(f) AND 2 member of(f)" for f=[1, 2].
  if (key1->root->field->is_array() || key2->root->field->is_array()) {
    return and_all_keys(param, key1, key2);
  }

  SEL_ARG *e1 = key1->root->first(), *e2 = key2->root->first();
  SEL_ROOT *new_tree = nullptr;

  while (e1 && e2) {
    int cmp = e1->cmp_min_to_min(e2);
    if (cmp < 0) {
      if (get_range(&e1, &e2, key1)) continue;
    } else if (get_range(&e2, &e1, key2))
      continue;
    /*
      NOTE: We don't destroy e1->next_key_part nor e2->next_key_part
      (if used at all, the return value here goes into a brand new element;
      it does not overwrite either of them), so we keep their use_counts
      intact here.
    */
    SEL_ROOT *next = key_and(param, e1->next_key_part, e2->next_key_part);
    if (next && next->type == SEL_ROOT::Type::IMPOSSIBLE)
      next->free_tree();
    else {
      SEL_ARG *new_arg = e1->clone_and(e2, param->temp_mem_root);
      if (!new_arg) return nullptr;  // End of memory
      new_arg->set_next_key_part(next);
      if (!new_tree) {
        new_tree = new (param->temp_mem_root) SEL_ROOT(new_arg);
      } else
        new_tree->insert(new_arg);
    }
    if (e1->cmp_max_to_max(e2) < 0)
      e1 = e1->next;  // e1 can't overlap next e2
    else
      e2 = e2->next;
  }
  key1->free_tree();
  key2->free_tree();
  if (!new_tree)
    // Impossible range
    return new (param->temp_mem_root)
        SEL_ROOT(param->temp_mem_root, SEL_ROOT::Type::IMPOSSIBLE);
  return new_tree;
}

static bool get_range(SEL_ARG **e1, SEL_ARG **e2, const SEL_ROOT *root1) {
  (*e1) = root1->find_range(*e2);  // first e1->min < e2->min
  if ((*e1)->cmp_max_to_min(*e2) < 0) {
    if (!((*e1) = (*e1)->next)) return true;
    if ((*e1)->cmp_min_to_max(*e2) > 0) {
      (*e2) = (*e2)->next;
      return true;
    }
  }
  return false;
}

/**
   Combine two range expression under a common OR. On a logical level, the
   transformation is key_or( expr1, expr2 ) => expr1 OR expr2.

   Both expressions are assumed to be in the SEL_ARG format. In a logic sense,
   the format is reminiscent of DNF, since an expression such as the following

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

   key_or() does not modify key1 nor key2 if they are in use by other roots
   (although typical use is that key1 has been disconnected from its root
   and thus can be modified in-place). Thus, it does not change their
   use_count.

   The returned node will not have its use_count increased; you are supposed
   to do that yourself when you connect it to a root.

   @param param    RANGE_OPT_PARAM from test_quick_select
   @param key1     Root of RB-tree of SEL_ARGs to be ORed with key2
   @param key2     Root of RB-tree of SEL_ARGs to be ORed with key1
*/
SEL_ROOT *key_or(RANGE_OPT_PARAM *param, SEL_ROOT *key1, SEL_ROOT *key2) {
  if (param->has_errors()) return nullptr;

  if (key1 == nullptr || key1->is_always()) {
    if (key2) key2->free_tree();
    return key1;
  }
  if (key2 == nullptr || key2->is_always())
    // Case is symmetric to the one above, just flip parameters.
    return key_or(param, key2, key1);

  if (key1->root->part != key2->root->part ||
      (key1->root->min_flag | key2->root->min_flag) & GEOM_FLAG) {
    key1->free_tree();
    key2->free_tree();
    return nullptr;  // Can't optimize this
  }

  // If one of the key is MAYBE_KEY then the found region may be bigger
  if (key1->type == SEL_ROOT::Type::MAYBE_KEY) {
    key2->free_tree();
    return key1;
  }
  if (key2->type == SEL_ROOT::Type::MAYBE_KEY) {
    key1->free_tree();
    return key2;
  }

  // (cond) OR (IMPOSSIBLE) <=> (cond).
  if (key1->type == SEL_ROOT::Type::IMPOSSIBLE) {
    key1->free_tree();
    return key2;
  }
  if (key2->type == SEL_ROOT::Type::IMPOSSIBLE) {
    key2->free_tree();
    return key1;
  }

  /*
    We need to modify one of key1 or key2 (whichever we choose, we will
    call it key1 afterwards). If either is used only by us (use_count == 0),
    we can use that directly. If not, we need to clone one of them; we pick
    the one with the fewest elements since that is the cheapest.
  */
  if (key1->use_count > 0) {
    if (key2->use_count == 0 || key1->elements > key2->elements) {
      std::swap(key1, key2);
    }
    if (key1->use_count > 0 && (key1 = key1->clone_tree(param)) == nullptr)
      return nullptr;  // OOM
  }
  assert(key1->use_count == 0);

  /*
    Add tree at key2 to tree at key1. If key2 is used by nobody else,
    we can cannibalize its nodes and add them directly into key1.
    If not, we'll need to make copies of them.
  */
  const bool key2_shared = (key2->use_count != 0);
  key1->root->maybe_flag |= key2->root->maybe_flag;

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

  SEL_ARG *cur_key2 = key2->root->first();
  while (cur_key2) {
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
    SEL_ARG *cur_key1 = key1->find_range(cur_key2);

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
    int cmp = 0;

    if (!cur_key1) {
      /*
        The range in cur_key2 starts before the first range in key1. Use
        the first range in key1 as cur_key1.

        cur_key2: [--------]
        key1:            [****--] [----]   [-------]
                         ^
                         cur_key1
      */
      cur_key1 = key1->root->first();
      cmp = -1;
    } else if ((cmp = cur_key1->cmp_max_to_min(cur_key2)) < 0) {
      /*
        This is the case:
        cur_key2:           [-------]
        cur_key1:   [----**]
      */
      SEL_ARG *next_key1 = cur_key1->next;
      if (cmp == -2 &&
          eq_tree(cur_key1->next_key_part, cur_key2->next_key_part)) {
        /*
          Adjacent (cmp==-2) and equal next_key_parts => ranges can be merged

          This is the case:
          cur_key2:           [-------]
          cur_key1:     [----]

          Result:
          cur_key2:     [-------------]     => inserted into key1 below
          cur_key1:                         => deleted
        */
        SEL_ARG *next_key2 = cur_key2->next;
        if (key2_shared) {
          if (!(cur_key2 = new (param->temp_mem_root) SEL_ARG(*cur_key2)))
            return nullptr;            // out of memory
          cur_key2->next = next_key2;  // New copy of cur_key2
        }

        if (cur_key2->copy_min(cur_key1)) {
          // cur_key2 is full range: [-inf <= cur_key2 <= +inf]
          key1->free_tree();
          key2->free_tree();
          if (key1->root->maybe_flag)
            return new (param->temp_mem_root)
                SEL_ROOT(param->temp_mem_root, SEL_ROOT::Type::MAYBE_KEY);
          return nullptr;
        }

        key1->tree_delete(cur_key1);
        if (key1->type == SEL_ROOT::Type::IMPOSSIBLE) {
          /*
            cur_key1 was the last range in key1; move the cur_key2
            range that was merged above to key1
          */
          key1->insert(cur_key2);
          cur_key2 = next_key2;
          break;
        }
      }
      // Move to next range in key1. Now cur_key1.min > cur_key2.min
      if (!(cur_key1 = next_key1))
        break;  // No more ranges in key1. Copy rest of key2
    }

    if (cmp < 0) {
      /*
        This is the case:
        cur_key2:   [--***]
        cur_key1:       [----]
      */
      int cur_key1_cmp;
      if ((cur_key1_cmp = cur_key1->cmp_min_to_max(cur_key2)) > 0) {
        /*
          This is the case:
          cur_key2:  [------**]
          cur_key1:            [----]
        */
        if (cur_key1_cmp == 2 &&
            eq_tree(cur_key1->next_key_part, cur_key2->next_key_part)) {
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
          key1->root->merge_flags(cur_key2);  // should be cur_key1->merge...()
                                              // ?
          if (cur_key1->min_flag & NO_MIN_RANGE &&
              cur_key1->max_flag & NO_MAX_RANGE) {
            key1->free_tree();
            key2->free_tree();
            if (key1->root->maybe_flag)
              return new (param->temp_mem_root)
                  SEL_ROOT(param->temp_mem_root, SEL_ROOT::Type::MAYBE_KEY);
            return nullptr;
          }
          cur_key2->release_next_key_part();  // Free not used tree
          cur_key2 = cur_key2->next;
          continue;
        } else {
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
          SEL_ARG *next_key2 = cur_key2->next;
          if (key2_shared) {
            SEL_ARG *cpy = new (param->temp_mem_root)
                SEL_ARG(*cur_key2);    // Must make copy
            if (!cpy) return nullptr;  // OOM
            key1->insert(cpy);
          } else
            key1->insert(cur_key2);
          cur_key2 = next_key2;
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
    if (eq_tree(cur_key1->next_key_part, cur_key2->next_key_part)) {
      // Merge overlapping ranges with equal next_key_part
      if (cur_key1->is_same(cur_key2)) {
        /*
          cur_key1 covers exactly the same range as cur_key2
          Use the relevant range in key1.
        */
        cur_key1->merge_flags(cur_key2);    // Copy maybe flags
        cur_key2->release_next_key_part();  // Free not used tree
        // Move to the next range in cur_key2
        cur_key2 = cur_key2->next;
        continue;
      } else {
        SEL_ARG *last = cur_key1;
        SEL_ARG *first = cur_key1;

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
               eq_tree(last->next->next_key_part, cur_key2->next_key_part)) {
          /*
            last->next is covered by cur_key2 and has same next_key_part.
            last can be deleted
          */
          SEL_ARG *save = last;
          last = last->next;
          key1->tree_delete(save);
        }
        // Redirect cur_key1 to last which will cover the entire range
        cur_key1 = last;

        /*
          Extend last to cover the entire range of
          [min(first.min_value,cur_key2.min_value)...last.max_value].
          If this forms a full range (the range covers all possible
          values) we return no SEL_ARG RB-tree.
        */
        bool full_range = last->copy_min(first);
        if (!full_range) full_range = last->copy_min(cur_key2);

        if (!full_range) {
          if (last->next && cur_key2->cmp_max_to_min(last->next) >= 0) {
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
          } else
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
            full_range = last->copy_max(cur_key2);
        }
        if (full_range) {  // Full range
          key1->free_tree();
          cur_key2->release_next_key_part();
          if (key1->root->maybe_flag)
            return new (param->temp_mem_root)
                SEL_ROOT(param->temp_mem_root, SEL_ROOT::Type::MAYBE_KEY);
          return nullptr;
        }
      }
    }

    if (cmp >= 0 && cur_key1->cmp_min_to_min(cur_key2) < 0) {
      /*
        This is the case ("cmp>=0" means that cur_key1.max >= cur_key2.min):
        cur_key2:                [-------]
        cur_key1:         [----------*******]
      */

      if (!cur_key1->next_key_part) {
        /*
          cur_key1->next_key_part is empty: cut the range that
          is covered by cur_key1 from cur_key2.
          Reason: (cur_key2->next_key_part OR
          cur_key1->next_key_part) will be empty and therefore
          equal to cur_key1->next_key_part. Thus, this part of
          the cur_key2 range is completely covered by cur_key1.
        */
        if (cur_key1->cmp_max_to_max(cur_key2) >= 0) {
          /*
            cur_key1 covers the entire range in cur_key2.
            cur_key2:            [-------]
            cur_key1:     [-----------------]

            Move on to next range in key2
          */
          cur_key2 = cur_key2->next;
          continue;
        } else {
          /*
            This is the case:
            cur_key2:            [-------]
            cur_key1:     [---------]

            Result:
            cur_key2:                [---]
            cur_key1:     [---------]
          */
          cur_key2->copy_max_to_min(cur_key1);
          // FIXME: what if key2_shared?
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
      SEL_ARG *new_arg = cur_key1->clone_first(cur_key2, param->temp_mem_root);
      if (!new_arg) return nullptr;  // OOM
      new_arg->set_next_key_part(cur_key1->next_key_part);
      cur_key1->copy_min_to_min(cur_key2);
      key1->insert(new_arg);
    }  // cur_key1.min >= cur_key2.min due to this if()

    /*
      Now cur_key2.min <= cur_key1.min <= cur_key2.max:
      cur_key2:    [---------]
      cur_key1:    [****---*****]
    */

    /*
      Get a copy we can modify. Note that this will keep an extra reference
      to its next_key_part (if any), but the destructor will clean that up
      when we exit from the function. key2_cpy is ephemeral and will not be
      inserted in any tree, although copies of it might be.
    */
    SEL_ARG key2_cpy(*cur_key2);

    for (;;) {
      if (cur_key1->cmp_min_to_min(&key2_cpy) > 0) {
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
        SEL_ARG *new_arg = key2_cpy.clone_first(cur_key1, param->temp_mem_root);
        if (!new_arg) return nullptr;  // OOM
        new_arg->set_next_key_part(key2_cpy.next_key_part);
        key1->insert(new_arg);
        key2_cpy.copy_min_to_min(cur_key1);
      }
      // Now key2_cpy.min == cur_key1.min

      if ((cmp = cur_key1->cmp_max_to_max(&key2_cpy)) <= 0) {
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
        cur_key1->maybe_flag |= key2_cpy.maybe_flag;
        cur_key1->set_next_key_part(key_or(
            param, cur_key1->release_next_key_part(), key2_cpy.next_key_part));

        if (!cmp) break;  // case b: done with this key2 range

        // Make key2_cpy the range [cur_key1.max, key2_cpy.max]
        key2_cpy.copy_max_to_min(cur_key1);
        if (!(cur_key1 = cur_key1->next)) {
          /*
            No more ranges in key1. Insert key2_cpy and go to "end"
            label to insert remaining ranges in key2 if any.
          */
          SEL_ARG *new_key1_range =
              new (param->temp_mem_root) SEL_ARG(key2_cpy);
          if (!new_key1_range) return nullptr;  // OOM
          key1->insert(new_key1_range);
          cur_key2 = cur_key2->next;
          goto end;
        }
        if (cur_key1->cmp_min_to_max(&key2_cpy) > 0) {
          /*
            The next range in key1 does not overlap with key2_cpy.
            Insert this range into key1 and move on to the next range
            in key2.
          */
          SEL_ARG *new_key1_range =
              new (param->temp_mem_root) SEL_ARG(key2_cpy);
          if (!new_key1_range) return nullptr;  // OOM
          key1->insert(new_key1_range);
          break;
        }
        /*
          key2_cpy overlaps with the next range in key1 and the case
          is now "cur_key2.min <= cur_key1.min <= cur_key2.max". Go back
          to for(;;) to handle this situation.
        */
        continue;
      } else {
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
        if (!cur_key1->next_key_part)  // Step 0
        {
          key2_cpy.release_next_key_part();  // Free not used tree
          break;
        }
        SEL_ARG *new_arg =
            cur_key1->clone_last(&key2_cpy, param->temp_mem_root);
        if (!new_arg) return nullptr;  // OOM
        cur_key1->copy_max_to_min(&key2_cpy);

        new_arg->set_next_key_part(
            key_or(param, cur_key1->next_key_part, key2_cpy.next_key_part));
        key1->insert(new_arg);
        break;
      }
    }
    // Move on to next range in key2
    cur_key2 = cur_key2->next;
  }

end:
  /*
    Add key2 ranges that are non-overlapping with and higher than the
    highest range in key1.
  */
  while (cur_key2) {
    SEL_ARG *next = cur_key2->next;
    if (key2_shared) {
      SEL_ARG *key2_cpy =
          new (param->temp_mem_root) SEL_ARG(*cur_key2);  // Must make copy
      if (!key2_cpy) return nullptr;
      key1->insert(key2_cpy);
    } else
      key1->insert(cur_key2);
    cur_key2 = next;
  }

  /*
    TODO: We should call key2->free_tree() here, since this might be the
    last reference to the tree (if !key2_shared). However, the tree might
    be in an invalid state since we may have inserted nodes into key1 without
    taking them out of key2, so we need to clean that up first. As a temporary
    measure, we TRASH() it to expose any bugs where people hold on to it
    where we thought they wouldn't.
  */
#ifndef NDEBUG
  if (!key2_shared) TRASH(key2, sizeof(*key2));
#endif
  return key1;
}

/**
  Compare if two trees are equal, recursively (not necessarily the same
  elements, but in terms of structure and values in each leaf).

  NOTE: The demand for the same structure means that some trees that are
  equivalent could be deemed inequal by this function, depending on insertion
  order.

  @param a First tree to compare.
  @param b Second tree to compare.

  @return true iff they are equivalent.
*/
static bool eq_tree(const SEL_ROOT *a, const SEL_ROOT *b) {
  if (a == b) return true;
  if (!a || !b) return false;
  if (a->type == SEL_ROOT::Type::KEY_RANGE &&
      b->type == SEL_ROOT::Type::KEY_RANGE)
    return eq_tree(a->root, b->root);
  else
    return a->type == b->type;
}

static bool eq_tree(const SEL_ARG *a, const SEL_ARG *b) {
  if (a == b) return true;
  if (!a || !b || !a->is_same(b)) return false;
  if (a->left != null_element && b->left != null_element) {
    if (!eq_tree(a->left, b->left)) return false;
  } else if (a->left != null_element || b->left != null_element)
    return false;
  if (a->right != null_element && b->right != null_element) {
    if (!eq_tree(a->right, b->right)) return false;
  } else if (a->right != null_element || b->right != null_element)
    return false;
  if (a->next_key_part != b->next_key_part) {  // Sub range
    if (!a->next_key_part != !b->next_key_part ||
        !eq_tree(a->next_key_part, b->next_key_part))
      return false;
  }
  return true;
}

void SEL_ROOT::insert(SEL_ARG *key) {
  SEL_ARG *element, **par = nullptr, *last_element = nullptr;

  if (type == Type::IMPOSSIBLE) {
    /*
      Used to be impossible, but now gets a new range; remove the dummy node
      that exists in that kind of tree, and set this one as the root
      (and sole element) instead.
    */
    root->release_next_key_part();
    uint8 maybe_flag = root->maybe_flag;
    root = key;
    root->maybe_flag = maybe_flag;
    root->make_root();
    type = Type::KEY_RANGE;
    return;
  }

  assert(type == Type::KEY_RANGE);
  assert(root->parent == nullptr);
  assert(root != null_element);
  for (element = root; element != null_element;) {
    last_element = element;
    if (key->cmp_min_to_min(element) > 0) {
      par = &element->right;
      element = element->right;
    } else {
      par = &element->left;
      element = element->left;
    }
  }
  *par = key;
  key->parent = last_element;
  /* Link in list */
  if (par == &last_element->left) {
    key->next = last_element;
    if ((key->prev = last_element->prev)) key->prev->next = key;
    last_element->prev = key;
  } else {
    if ((key->next = last_element->next)) key->next->prev = key;
    key->prev = last_element;
    last_element->next = key;
  }
  key->left = key->right = null_element;
  uint8 maybe_flag = root->maybe_flag;
  root = root->rb_insert(key);  // rebalance tree
  root->maybe_flag = maybe_flag;
  ++elements;
}

SEL_ARG *SEL_ROOT::find_range(const SEL_ARG *key) const {
  SEL_ARG *element = root, *found = nullptr;

  for (;;) {
    if (element == null_element) return found;
    int cmp = element->cmp_min_to_min(key);
    if (cmp == 0) return element;
    if (cmp < 0) {
      found = element;
      element = element->right;
    } else
      element = element->left;
  }
}

/*
  Remove a element from the tree

  SYNOPSIS
    tree_delete()
    key		Key that is to be deleted from tree (this)

  NOTE
    This also frees all sub trees that is used by the element

*/

void SEL_ROOT::tree_delete(SEL_ARG *key) {
  enum SEL_ARG::leaf_color remove_color;
  SEL_ARG *nod, **par, *fix_par;
  DBUG_TRACE;

  assert(this->type == Type::KEY_RANGE);
  assert(this->root->parent == nullptr);

  /*
    If deleting the last element, we are now of type IMPOSSIBLE.
    Keep the element around so that we have somewhere to store
    next_key_part etc. if needed in the future.
  */
  if (elements == 1) {
    assert(key == root);
    type = Type::IMPOSSIBLE;
    key->release_next_key_part();
    return;
  }

  /* Unlink from list */
  if (key->prev) key->prev->next = key->next;
  if (key->next) key->next->prev = key->prev;
  if (key->next_key_part) --key->next_key_part->use_count;
  if (!key->parent)
    par = &root;
  else
    par = key->parent_ptr();

  if (key->left == null_element) {
    *par = nod = key->right;
    fix_par = key->parent;
    if (nod != null_element) nod->parent = fix_par;
    remove_color = key->color;
  } else if (key->right == null_element) {
    *par = nod = key->left;
    nod->parent = fix_par = key->parent;
    remove_color = key->color;
  } else {
    SEL_ARG *tmp = key->next;               // next bigger key (exist!)
    nod = *tmp->parent_ptr() = tmp->right;  // unlink tmp from tree
    fix_par = tmp->parent;
    if (nod != null_element) nod->parent = fix_par;
    remove_color = tmp->color;

    tmp->parent = key->parent;  // Move node in place of key
    (tmp->left = key->left)->parent = tmp;
    if ((tmp->right = key->right) != null_element) tmp->right->parent = tmp;
    tmp->color = key->color;
    *par = tmp;
    if (fix_par == key)  // key->right == key->next
      fix_par = tmp;     // new parent of nod
  }

  --elements;
  if (root == null_element) return;  // Maybe root later
  if (remove_color == SEL_ARG::BLACK) {
    uint8 maybe_flag = root->maybe_flag;
    root = rb_delete_fixup(root, nod, fix_par);
    root->maybe_flag = maybe_flag;
  }
#ifndef NDEBUG
  test_rb_tree(root, root->parent);
#endif
}

/* Functions to fix up the tree after insert and delete */

static void left_rotate(SEL_ARG **root, SEL_ARG *leaf) {
  SEL_ARG *y = leaf->right;
  leaf->right = y->left;
  if (y->left != null_element) y->left->parent = leaf;
  if (!(y->parent = leaf->parent))
    *root = y;
  else
    *leaf->parent_ptr() = y;
  y->left = leaf;
  leaf->parent = y;
}

static void right_rotate(SEL_ARG **root, SEL_ARG *leaf) {
  SEL_ARG *y = leaf->left;
  leaf->left = y->right;
  if (y->right != null_element) y->right->parent = leaf;
  if (!(y->parent = leaf->parent))
    *root = y;
  else
    *leaf->parent_ptr() = y;
  y->right = leaf;
  leaf->parent = y;
}

SEL_ARG *SEL_ARG::rb_insert(SEL_ARG *leaf) {
  SEL_ARG *y, *par, *par2, *root;
  root = this;
  assert(!root->parent);
  assert(root->color == BLACK);

  leaf->color = RED;
  while (leaf != root && (par = leaf->parent)->color ==
                             RED) {  // This can't be root or 1 level under
    assert(leaf->parent->parent);
    if (par == (par2 = leaf->parent->parent)->left) {
      y = par2->right;
      if (y->color == RED) {
        par->color = BLACK;
        y->color = BLACK;
        leaf = par2;
        leaf->color = RED; /* And the loop continues */
      } else {
        if (leaf == par->right) {
          left_rotate(&root, leaf->parent);
          par = leaf; /* leaf is now parent to old leaf */
        }
        par->color = BLACK;
        par2->color = RED;
        right_rotate(&root, par2);
        break;
      }
    } else {
      y = par2->left;
      if (y->color == RED) {
        par->color = BLACK;
        y->color = BLACK;
        leaf = par2;
        leaf->color = RED; /* And the loop continues */
      } else {
        if (leaf == par->left) {
          right_rotate(&root, par);
          par = leaf;
        }
        par->color = BLACK;
        par2->color = RED;
        left_rotate(&root, par2);
        break;
      }
    }
  }
  root->color = BLACK;
#ifndef NDEBUG
  test_rb_tree(root, root->parent);
#endif
  return root;
}

SEL_ARG *rb_delete_fixup(SEL_ARG *root, SEL_ARG *key, SEL_ARG *par) {
  SEL_ARG *x, *w;
  root->parent = nullptr;

  x = key;
  while (x != root && x->color == SEL_ARG::BLACK) {
    if (x == par->left) {
      w = par->right;
      if (w->color == SEL_ARG::RED) {
        w->color = SEL_ARG::BLACK;
        par->color = SEL_ARG::RED;
        left_rotate(&root, par);
        w = par->right;
      }
      if (w->left->color == SEL_ARG::BLACK &&
          w->right->color == SEL_ARG::BLACK) {
        w->color = SEL_ARG::RED;
        x = par;
      } else {
        if (w->right->color == SEL_ARG::BLACK) {
          w->left->color = SEL_ARG::BLACK;
          w->color = SEL_ARG::RED;
          right_rotate(&root, w);
          w = par->right;
        }
        w->color = par->color;
        par->color = SEL_ARG::BLACK;
        w->right->color = SEL_ARG::BLACK;
        left_rotate(&root, par);
        x = root;
        break;
      }
    } else {
      w = par->left;
      if (w->color == SEL_ARG::RED) {
        w->color = SEL_ARG::BLACK;
        par->color = SEL_ARG::RED;
        right_rotate(&root, par);
        w = par->left;
      }
      if (w->right->color == SEL_ARG::BLACK &&
          w->left->color == SEL_ARG::BLACK) {
        w->color = SEL_ARG::RED;
        x = par;
      } else {
        if (w->left->color == SEL_ARG::BLACK) {
          w->right->color = SEL_ARG::BLACK;
          w->color = SEL_ARG::RED;
          left_rotate(&root, w);
          w = par->left;
        }
        w->color = par->color;
        par->color = SEL_ARG::BLACK;
        w->left->color = SEL_ARG::BLACK;
        right_rotate(&root, par);
        x = root;
        break;
      }
    }
    par = x->parent;
  }
  x->color = SEL_ARG::BLACK;
  return root;
}

#ifndef NDEBUG
/* Test that the properties for a red-black tree hold */

int test_rb_tree(SEL_ARG *element, SEL_ARG *parent) {
  int count_l, count_r;

  if (element == null_element) return 0;  // Found end of tree
  if (element->parent != parent) {
    LogErr(ERROR_LEVEL, ER_TREE_CORRUPT_PARENT_SHOULD_POINT_AT_PARENT);
    return -1;
  }
  if (!parent && element->color != SEL_ARG::BLACK) {
    LogErr(ERROR_LEVEL, ER_TREE_CORRUPT_ROOT_SHOULD_BE_BLACK);
    return -1;
  }
  if (element->color == SEL_ARG::RED &&
      (element->left->color == SEL_ARG::RED ||
       element->right->color == SEL_ARG::RED)) {
    LogErr(ERROR_LEVEL, ER_TREE_CORRUPT_2_CONSECUTIVE_REDS);
    return -1;
  }
  if (element->left == element->right &&
      element->left != null_element) {  // Dummy test
    LogErr(ERROR_LEVEL, ER_TREE_CORRUPT_RIGHT_IS_LEFT);
    return -1;
  }
  count_l = test_rb_tree(element->left, element);
  count_r = test_rb_tree(element->right, element);
  if (count_l >= 0 && count_r >= 0) {
    if (count_l == count_r) return count_l + (element->color == SEL_ARG::BLACK);
    LogErr(ERROR_LEVEL, ER_TREE_CORRUPT_INCORRECT_BLACK_COUNT, count_l,
           count_r);
  }
  return -1;  // Error, no more warnings
}
#endif

/**
  Count how many times SEL_ARG graph "root" refers to its part "key" via
  transitive closure.

  @param root  An RB-Root node in a SEL_ARG graph.
  @param key   Another RB-Root node in that SEL_ARG graph.
  @param seen  Which SEL_ARGs we have already seen in this traversal.
               Used for deduplication, so that we only count each
               SEL_ARG once.

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

static ulong count_key_part_usage(const SEL_ROOT *root, const SEL_ROOT *key,
                                  std::set<const SEL_ROOT *> *seen) {
  // Don't count paths from a given key twice.
  if (seen->count(root)) return 0;
  seen->insert(root);
  ulong count = 0;
  for (SEL_ARG *node = root->root->first(); node; node = node->next) {
    if (node->next_key_part) {
      if (node->next_key_part == key) count++;
      if (node->next_key_part->root->part < key->root->part)
        count += count_key_part_usage(node->next_key_part, key, seen);
    }
  }
  return count;
}

bool SEL_ROOT::test_use_count(const SEL_ROOT *origin) const {
  uint e_count = 0;
  if (this == origin && use_count != 1) {
    LogErr(INFORMATION_LEVEL, ER_WRONG_COUNT_FOR_ORIGIN, use_count, this);
    assert(false);
    return true;
  }
  if (type != SEL_ROOT::Type::KEY_RANGE) return false;
  for (SEL_ARG *pos = root->first(); pos; pos = pos->next) {
    e_count++;
    if (pos->next_key_part) {
      std::set<const SEL_ROOT *> seen;
      ulong count = count_key_part_usage(origin, pos->next_key_part, &seen);
      /*
        This cannot be a strict equality test, since there might be
        connections from the keys[] array that we don't see.
      */
      if (count > pos->next_key_part->use_count) {
        LogErr(INFORMATION_LEVEL, ER_WRONG_COUNT_FOR_KEY, pos->next_key_part,
               pos->next_key_part->use_count, count);
        assert(false);
        return true;
      }
      pos->next_key_part->test_use_count(origin);
    }
  }
  if (e_count != elements) {
    LogErr(WARNING_LEVEL, ER_WRONG_COUNT_OF_ELEMENTS, e_count, elements, this);
    assert(false);
    return true;
  }
  return false;
}

bool get_sel_root_for_keypart(uint key_part_num, SEL_ROOT *keypart_tree,
                              SEL_ROOT **cur_range) {
  if (keypart_tree == nullptr) return false;
  if (keypart_tree->type != SEL_ROOT::Type::KEY_RANGE) {
    /*
      A range predicate not usable by Loose Index Scan is found.
      Predicates for keypart 'keypart_tree->root->part' and later keyparts
      cannot be used.
    */
    *cur_range = keypart_tree;
    return false;
  }
  if (keypart_tree->root->part == key_part_num) {
    *cur_range = keypart_tree;
    return false;
  }

  SEL_ROOT *tree_first_range = nullptr;
  SEL_ARG *first_kp = keypart_tree->root->first();

  for (SEL_ARG *cur_kp = first_kp; cur_kp; cur_kp = cur_kp->next) {
    SEL_ROOT *curr_tree = nullptr;
    if (cur_kp->next_key_part) {
      if (get_sel_root_for_keypart(key_part_num, cur_kp->next_key_part,
                                   &curr_tree))
        return true;
    }
    /**
      Check if the SEL_ARG tree for 'field' is identical for all ranges in
      'keypart_tree'.
    */
    if (cur_kp == first_kp)
      tree_first_range = curr_tree;
    else if (!all_same(tree_first_range, curr_tree))
      return true;
  }
  *cur_range = tree_first_range;
  return false;
}

#ifndef NDEBUG
void print_sel_tree(RANGE_OPT_PARAM *param, SEL_TREE *tree, Key_map *tree_map,
                    const char *msg) {
  char buff[1024];
  DBUG_TRACE;

  String tmp(buff, sizeof(buff), &my_charset_bin);
  tmp.length(0);
  for (uint idx = 0; idx < param->keys; idx++) {
    if (tree_map->is_set(idx)) {
      uint keynr = param->real_keynr[idx];
      if (tmp.length()) tmp.append(',');
      tmp.append(param->table->key_info[keynr].name);
    }
  }
  if (!tmp.length()) tmp.append(STRING_WITH_LEN("(empty)"));

  DBUG_PRINT("info", ("SEL_TREE: %p (%s)  scans: %s", tree, msg, tmp.ptr()));
}
#endif
