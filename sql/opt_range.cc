/* Copyright (c) 2000, 2015, Oracle and/or its affiliates.
   Copyright (c) 2008, 2015, MariaDB

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

/*
  This file contains:

  RangeAnalysisModule  
    A module that accepts a condition, index (or partitioning) description, 
    and builds lists of intervals (in index/partitioning space), such that 
    all possible records that match the condition are contained within the 
    intervals.
    The entry point for the range analysis module is get_mm_tree() function.
    
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

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "sql_priv.h"
#include "key.h"        // is_key_used, key_copy, key_cmp, key_restore
#include "sql_parse.h"                          // check_stack_overrun
#include "sql_partition.h"    // get_part_id_func, PARTITION_ITERATOR,
                              // struct partition_info
#include "sql_base.h"         // free_io_cache
#include "records.h"          // init_read_record, end_read_record
#include <m_ctype.h>
#include "sql_select.h"
#include "filesort.h"         // filesort_free_buffers

#ifndef EXTRA_DEBUG
#define test_rb_tree(A,B) {}
#define test_use_count(A) {}
#endif

/*
  Convert double value to #rows. Currently this does floor(), and we
  might consider using round() instead.
*/
#define double2rows(x) ((ha_rows)(x))

static int sel_cmp(Field *f,uchar *a,uchar *b,uint8 a_flag,uint8 b_flag);

/*
  this should be long enough so that any memcmp with a string that
  starts from '\0' won't cross is_null_string boundaries, even
  if the memcmp is optimized to compare 4- 8- or 16- bytes at once
*/
static uchar is_null_string[20]= {1,0};

class RANGE_OPT_PARAM;
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

  An interval list is a sequence of ordered disjoint intervals over the same
  key part. SEL_ARG are linked via "next" and "prev" pointers. Additionally,
  all intervals in the list form an RB-tree, linked via left/right/parent 
  pointers. The RB-tree root SEL_ARG object will be further called "root of the
  interval list".
  
    In the example pic, there are 4 interval lists: 
    "kp<1 OR kp1=2 OR kp1=3", "kp2=5", "kp3=10 OR kp3=12", "kp3=11 OR kp3=13".
    The vertical lines represent SEL_ARG::next/prev pointers.
    
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
  /* 
    The ordinal number the least significant component encountered in
    the ranges of the SEL_ARG tree (the first component has number 1) 
  */
  uint16 max_part_no; 
  /* 
    Number of children of this element in the RB-tree, plus 1 for this
    element itself.
  */
  uint16 elements;
  /*
    Valid only for elements which are RB-tree roots: Number of times this
    RB-tree is referred to (it is referred by SEL_ARG::next_key_part or by
    SEL_TREE::keys[i] or by a temporary SEL_ARG* variable)
  */
  ulong use_count;

  Field *field;
  uchar *min_value,*max_value;			// Pointer to range

  /*
    eq_tree() requires that left == right == 0 if the type is MAYBE_KEY.
   */
  SEL_ARG *left,*right;   /* R-B tree children */
  SEL_ARG *next,*prev;    /* Links for bi-directional interval list */
  SEL_ARG *parent;        /* R-B tree parent */
  SEL_ARG *next_key_part; 
  enum leaf_color { BLACK,RED } color;
  enum Type { IMPOSSIBLE, MAYBE, MAYBE_KEY, KEY_RANGE } type;

  enum { MAX_SEL_ARGS = 16000 };

  SEL_ARG() {}
  SEL_ARG(SEL_ARG &);
  SEL_ARG(Field *,const uchar *, const uchar *);
  SEL_ARG(Field *field, uint8 part, uchar *min_value, uchar *max_value,
	  uint8 min_flag, uint8 max_flag, uint8 maybe_flag);
  SEL_ARG(enum Type type_arg)
    :min_flag(0), max_part_no(0) /* first key part means 1. 0 mean 'no parts'*/, 
     elements(1),use_count(1),left(0),right(0),
     next_key_part(0), color(BLACK), type(type_arg)
  {}
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
  SEL_ARG *clone_and(SEL_ARG* arg)
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
    return new SEL_ARG(field, part, new_min, new_max, flag_min, flag_max,
		       test(maybe_flag && arg->maybe_flag));
  }
  SEL_ARG *clone_first(SEL_ARG *arg)
  {						// min <= X < arg->min
    return new SEL_ARG(field,part, min_value, arg->min_value,
		       min_flag, arg->min_flag & NEAR_MIN ? 0 : NEAR_MAX,
		       maybe_flag | arg->maybe_flag);
  }
  SEL_ARG *clone_last(SEL_ARG *arg)
  {						// min <= X <= key_max
    return new SEL_ARG(field, part, min_value, arg->max_value,
		       min_flag, arg->max_flag, maybe_flag | arg->maybe_flag);
  }
  SEL_ARG *clone(RANGE_OPT_PARAM *param, SEL_ARG *new_parent, SEL_ARG **next);

  bool copy_min(SEL_ARG* arg)
  {						// Get overlapping range
    if (cmp_min_to_min(arg) > 0)
    {
      min_value=arg->min_value; min_flag=arg->min_flag;
      if ((max_flag & (NO_MAX_RANGE | NO_MIN_RANGE)) ==
	  (NO_MAX_RANGE | NO_MIN_RANGE))
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
      if ((max_flag & (NO_MAX_RANGE | NO_MIN_RANGE)) ==
	  (NO_MAX_RANGE | NO_MIN_RANGE))
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
	bzero(*min_key+1,length-1);
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
	bzero(*max_key+1,length-1);
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
#ifdef EXTRA_DEBUG
  friend int test_rb_tree(SEL_ARG *element,SEL_ARG *parent);
  void test_use_count(SEL_ARG *root);
#endif
  SEL_ARG *first();
  const SEL_ARG *first() const;
  SEL_ARG *last();
  void make_root();
  inline bool simple_key()
  {
    return !next_key_part && elements == 1;
  }
  void increment_use_count(long count)
  {
    if (next_key_part)
    {
      next_key_part->use_count+=count;
      count*= (next_key_part->use_count-count);
      for (SEL_ARG *pos=next_key_part->first(); pos ; pos=pos->next)
	if (pos->next_key_part)
	  pos->increment_use_count(count);
    }
  }
  void incr_refs()
  {
    increment_use_count(1);
    use_count++;
  }
  void incr_refs_all()
  {
    for (SEL_ARG *pos=first(); pos ; pos=pos->next)
    {
      pos->increment_use_count(1);
    }
    use_count++;
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

  bool is_singlepoint()
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

#define CLONE_KEY1_MAYBE 1
#define CLONE_KEY2_MAYBE 2
#define swap_clone_flag(A) ((A & 1) << 1) | ((A & 2) >> 1)


/*
  While objects of the class SEL_ARG represent ranges for indexes or
  index infixes (including ranges for index prefixes and index suffixes),
  objects of the class SEL_TREE represent AND/OR formulas of such ranges.
  Currently an AND/OR formula represented by a SEL_TREE object can have
  at most three levels: 

    <SEL_TREE formula> ::= 
      [ <SEL_RANGE_TREE formula> AND ]
      [ <SEL_IMERGE formula> [ AND <SEL_IMERGE formula> ...] ]

    <SEL_RANGE_TREE formula> ::=
      <SEL_ARG formula> [ AND  <SEL_ARG_formula> ... ]

    <SEL_IMERGE formula> ::=  
      <SEL_RANGE_TREE formula> [ OR <SEL_RANGE_TREE formula> ]

  As we can see from the above definitions:
   - SEL_RANGE_TREE formula is a conjunction of SEL_ARG formulas
   - SEL_IMERGE formula is a disjunction of SEL_RANGE_TREE formulas
   - SEL_TREE formula is a conjunction of a SEL_RANGE_TREE formula
     and SEL_IMERGE formulas. 
  It's required above that a SEL_TREE formula has at least one conjunct.

  Usually we will consider normalized SEL_RANGE_TREE formulas where we use
  TRUE as conjunct members for those indexes whose SEL_ARG trees are empty.
  
  We will call an SEL_TREE object simply 'tree'. 
  The part of a tree that represents SEL_RANGE_TREE formula is called
  'range part' of the tree while the remaining part is called 'imerge part'. 
  If a tree contains only a range part then we call such a tree 'range tree'.
  Components of a range tree that represent SEL_ARG formulas are called ranges.
  If a tree does not contain any range part we call such a tree 'imerge tree'.
  Components of the imerge part of a tree that represent SEL_IMERGE formula
  are called imerges.

  Usually we'll designate:
    SEL_TREE formulas         by T_1,...,T_k
    SEL_ARG formulas          by R_1,...,R_k
    SEL_RANGE_TREE formulas   by RT_1,...,RT_k
    SEL_IMERGE formulas       by M_1,...,M_k
  Accordingly we'll use:
    t_1,...,t_k - to designate trees representing T_1,...,T_k
    r_1,...,r_k - to designate ranges representing R_1,...,R_k 
    rt_1,...,r_tk - to designate range trees representing RT_1,...,RT_k
    m_1,...,m_k - to designate imerges representing M_1,...,M_k

  SEL_TREE objects are usually built from WHERE conditions or
  ON expressions.
  A SEL_TREE object always represents an inference of the condition it is
  built from. Therefore, if a row satisfies a SEL_TREE formula it also
  satisfies the condition it is built from.

  The following transformations of tree t representing SEL_TREE formula T 
  yield a new tree t1 thar represents an inference of T: T=>T1.  
    (1) remove any of SEL_ARG tree from the range part of t
    (2) remove any imerge from the tree t 
    (3) remove any of SEL_ARG tree from any range tree contained
        in any imerge of tree   
 
  Since the basic blocks of any SEL_TREE objects are ranges, SEL_TREE
  objects in many cases can be effectively used to filter out a big part
  of table rows that do not satisfy WHERE/IN conditions utilizing
  only single or multiple range index scans.

  A single range index scan is constructed for a range tree that contains
  only one SEL_ARG object for an index or an index prefix.
  An index intersection scan can be constructed for a range tree
  that contains several SEL_ARG objects. Currently index intersection
  scans are constructed only for single-point ranges.
  An index merge scan is constructed for a imerge tree that contains only
  one imerge. If range trees of this imerge contain only single-point merges
  than a union of index intersections can be built.

  Usually the tree built by the range optimizer for a query table contains
  more than one range in the range part, and additionally may contain some
  imerges in the imerge part. The range optimizer evaluates all of them one
  by one and chooses the range or the imerge that provides the cheapest
  single or multiple range index scan of the table.  According to rules 
  (1)-(3) this scan always filter out only those rows that do not satisfy
  the query conditions. 

  For any condition the SEL_TREE object for it is built in a bottom up
  manner starting from the range trees for the predicates. The tree_and
  function builds a tree for any conjunction of formulas from the trees
  for its conjuncts. The tree_or function builds a tree for any disjunction
  of formulas from the trees for its disjuncts.    
*/ 
  
class SEL_TREE :public Sql_alloc
{
public:
  /*
    Starting an effort to document this field:
    (for some i, keys[i]->type == SEL_ARG::IMPOSSIBLE) => 
       (type == SEL_TREE::IMPOSSIBLE)
  */
  enum Type { IMPOSSIBLE, ALWAYS, MAYBE, KEY, KEY_SMALLER } type;
  SEL_TREE(enum Type type_arg) :type(type_arg) {}
  SEL_TREE() :type(KEY)
  {
    keys_map.clear_all();
    bzero((char*) keys,sizeof(keys));
  }
  SEL_TREE(SEL_TREE *arg, bool without_merges, RANGE_OPT_PARAM *param);
  /*
    Note: there may exist SEL_TREE objects with sel_tree->type=KEY and
    keys[i]=0 for all i. (SergeyP: it is not clear whether there is any
    merit in range analyzer functions (e.g. get_mm_parts) returning a
    pointer to such SEL_TREE instead of NULL)
  */
  SEL_ARG *keys[MAX_KEY];
  key_map keys_map;        /* bitmask of non-NULL elements in keys */

  /*
    Possible ways to read rows using index_merge. The list is non-empty only
    if type==KEY. Currently can be non empty only if keys_map.is_clear_all().
  */
  List<SEL_IMERGE> merges;

  /* The members below are filled/used only after get_mm_tree is done */
  key_map ror_scans_map;   /* bitmask of ROR scan-able elements in keys */
  uint    n_ror_scans;     /* number of set bits in ror_scans_map */

  struct st_index_scan_info **index_scans;     /* list of index scans */
  struct st_index_scan_info **index_scans_end; /* last index scan */

  struct st_ror_scan_info **ror_scans;     /* list of ROR key scans */
  struct st_ror_scan_info **ror_scans_end; /* last ROR scan */
  /* Note that #records for each key scan is stored in table->quick_rows */

  bool without_ranges() { return keys_map.is_clear_all(); }
  bool without_imerges() { return merges.is_empty(); }
};

class RANGE_OPT_PARAM
{
public:
  THD	*thd;   /* Current thread handle */
  TABLE *table; /* Table being analyzed */
  COND *cond;   /* Used inside get_mm_tree(). */
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
    Otherwise index description describes fake indexes.
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

  /* Number of SEL_ARG objects allocated by SEL_ARG::clone_tree operations */
  uint alloced_sel_args; 

  bool force_default_mrr;
  KEY_PART *key[MAX_KEY]; /* First key parts of keys used in the query */

  bool statement_should_be_aborted() const
  {
    return
      thd->is_fatal_error ||
      thd->is_error() ||
      alloced_sel_args > SEL_ARG::MAX_SEL_ARGS;
  }
};

class PARAM : public RANGE_OPT_PARAM
{
public:
  ha_rows quick_rows[MAX_KEY];
  longlong baseflag;
  uint max_key_part, range_count;

  bool quick;				// Don't calulate possible keys

  uint fields_bitmap_size;
  MY_BITMAP needed_fields;    /* bitmask of fields needed by the query */
  MY_BITMAP tmp_covered_fields;

  key_map *needed_reg;        /* ptr to SQL_SELECT::needed_reg */

  uint *imerge_cost_buff;     /* buffer for index_merge cost estimates */
  uint imerge_cost_buff_size; /* size of the buffer */

  /* TRUE if last checked tree->key can be used for ROR-scan */
  bool is_ror_scan;
  /* Number of ranges in the last checked tree->key */
  uint n_ranges;
  uint8 first_null_comp; /* first null component if any, 0 - otherwise */
};


class TABLE_READ_PLAN;
  class TRP_RANGE;
  class TRP_ROR_INTERSECT;
  class TRP_ROR_UNION;
  class TRP_INDEX_INTERSECT;
  class TRP_INDEX_MERGE;
  class TRP_GROUP_MIN_MAX;

struct st_index_scan_info;
struct st_ror_scan_info;

static SEL_TREE * get_mm_parts(RANGE_OPT_PARAM *param,COND *cond_func,Field *field,
			       Item_func::Functype type,Item *value,
			       Item_result cmp_type);
static SEL_ARG *get_mm_leaf(RANGE_OPT_PARAM *param,COND *cond_func,Field *field,
			    KEY_PART *key_part,
			    Item_func::Functype type,Item *value);
static SEL_TREE *get_mm_tree(RANGE_OPT_PARAM *param,COND *cond);

static bool is_key_scan_ror(PARAM *param, uint keynr, uint8 nparts);
static ha_rows check_quick_select(PARAM *param, uint idx, bool index_only,
                                  SEL_ARG *tree, bool update_tbl_stats, 
                                  uint *mrr_flags, uint *bufsize,
                                  COST_VECT *cost);

QUICK_RANGE_SELECT *get_quick_select(PARAM *param,uint index,
                                     SEL_ARG *key_tree, uint mrr_flags, 
                                     uint mrr_buf_size, MEM_ROOT *alloc);
static TRP_RANGE *get_key_scans_params(PARAM *param, SEL_TREE *tree,
                                       bool index_read_must_be_used,
                                       bool update_tbl_stats,
                                       double read_time);
static
TRP_INDEX_INTERSECT *get_best_index_intersect(PARAM *param, SEL_TREE *tree,
                                              double read_time);
static
TRP_ROR_INTERSECT *get_best_ror_intersect(const PARAM *param, SEL_TREE *tree,
                                          double read_time,
                                          bool *are_all_covering);
static
TRP_ROR_INTERSECT *get_best_covering_ror_intersect(PARAM *param,
                                                   SEL_TREE *tree,
                                                   double read_time);
static
TABLE_READ_PLAN *get_best_disjunct_quick(PARAM *param, SEL_IMERGE *imerge,
                                         double read_time);
static
TABLE_READ_PLAN *merge_same_index_scans(PARAM *param, SEL_IMERGE *imerge,
                                        TRP_INDEX_MERGE *imerge_trp,
                                        double read_time);
static
TRP_GROUP_MIN_MAX *get_best_group_min_max(PARAM *param, SEL_TREE *tree,
                                          double read_time);

#ifndef DBUG_OFF
static void print_sel_tree(PARAM *param, SEL_TREE *tree, key_map *tree_map,
                           const char *msg);
static void print_ror_scans_arr(TABLE *table, const char *msg,
                                struct st_ror_scan_info **start,
                                struct st_ror_scan_info **end);
static void print_quick(QUICK_SELECT_I *quick, const key_map *needed_reg);
#endif

static SEL_TREE *tree_and(RANGE_OPT_PARAM *param,
                          SEL_TREE *tree1, SEL_TREE *tree2);
static SEL_TREE *tree_or(RANGE_OPT_PARAM *param,
                         SEL_TREE *tree1,SEL_TREE *tree2);
static SEL_ARG *sel_add(SEL_ARG *key1,SEL_ARG *key2);
static SEL_ARG *key_or(RANGE_OPT_PARAM *param,
                       SEL_ARG *key1, SEL_ARG *key2);
static SEL_ARG *key_and(RANGE_OPT_PARAM *param,
                        SEL_ARG *key1, SEL_ARG *key2,
                        uint clone_flag);
static bool get_range(SEL_ARG **e1,SEL_ARG **e2,SEL_ARG *root1);
bool get_quick_keys(PARAM *param,QUICK_RANGE_SELECT *quick,KEY_PART *key,
                    SEL_ARG *key_tree, uchar *min_key,uint min_key_flag,
                    uchar *max_key,uint max_key_flag);
static bool eq_tree(SEL_ARG* a,SEL_ARG *b);

static SEL_ARG null_element(SEL_ARG::IMPOSSIBLE);
static bool null_part_in_key(KEY_PART *key_part, const uchar *key,
                             uint length);
static bool is_key_scan_ror(PARAM *param, uint keynr, uint8 nparts);

#include "opt_range_mrr.cc"

static bool sel_trees_have_common_keys(SEL_TREE *tree1, SEL_TREE *tree2, 
                                       key_map *common_keys);
static void eliminate_single_tree_imerges(RANGE_OPT_PARAM *param,
                                          SEL_TREE *tree);

static bool sel_trees_can_be_ored(RANGE_OPT_PARAM* param,
                                  SEL_TREE *tree1, SEL_TREE *tree2, 
                                  key_map *common_keys);
static bool sel_trees_must_be_ored(RANGE_OPT_PARAM* param,
                                   SEL_TREE *tree1, SEL_TREE *tree2,
                                   key_map common_keys);
static int and_range_trees(RANGE_OPT_PARAM *param,
                           SEL_TREE *tree1, SEL_TREE *tree2,
                           SEL_TREE *result);
static bool remove_nonrange_trees(RANGE_OPT_PARAM *param, SEL_TREE *tree);


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
  SEL_IMERGE (SEL_IMERGE *arg, uint cnt, RANGE_OPT_PARAM *param);
  int or_sel_tree(RANGE_OPT_PARAM *param, SEL_TREE *tree);
  bool have_common_keys(RANGE_OPT_PARAM *param, SEL_TREE *tree);
  int and_sel_tree(RANGE_OPT_PARAM *param, SEL_TREE *tree, 
                   SEL_IMERGE *new_imerge);
  int or_sel_tree_with_checks(RANGE_OPT_PARAM *param,
                              uint n_init_trees, 
                              SEL_TREE *new_tree,
                              bool is_first_check_pass,
                              bool *is_last_check_pass);
  int or_sel_imerge_with_checks(RANGE_OPT_PARAM *param,
                                uint n_init_trees,
                                SEL_IMERGE* imerge,
                                bool is_first_check_pass,
                                bool *is_last_check_pass);
};


/*
  Add a range tree to the range trees of this imerge 

  SYNOPSIS
    or_sel_tree()
      param                  Context info for the operation         
      tree                   SEL_TREE to add to this imerge 

  DESCRIPTION 
    The function just adds the range tree 'tree' to the range trees
    of this imerge.

  RETURN
     0   if the operation is success
    -1   if the function runs out memory
*/

int SEL_IMERGE::or_sel_tree(RANGE_OPT_PARAM *param, SEL_TREE *tree)
{
  if (trees_next == trees_end)
  {
    const int realloc_ratio= 2;		/* Double size for next round */
    uint old_elements= (trees_end - trees);
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
  Check if any of the range trees of this imerge intersects with a given tree 

  SYNOPSIS
    have_common_keys()
      param    Context info for the function
      tree     SEL_TREE intersection with the imerge range trees is checked for 

  DESCRIPTION
    The function checks whether there is any range tree rt_i in this imerge
    such that there are some indexes for which ranges are defined in both
    rt_i and the range part of the SEL_TREE tree.  
    To check this the function calls the function sel_trees_have_common_keys.

  RETURN 
    TRUE    if there are such range trees in this imerge
    FALSE   otherwise
*/

bool SEL_IMERGE::have_common_keys(RANGE_OPT_PARAM *param, SEL_TREE *tree)
{
  for (SEL_TREE** or_tree= trees, **bound= trees_next;
       or_tree != bound; or_tree++)
  {
    key_map common_keys;
    if (sel_trees_have_common_keys(*or_tree, tree, &common_keys))
      return TRUE;
  }
  return FALSE;
}


/* 
  Perform AND operation for this imerge and the range part of a tree

  SYNOPSIS
    and_sel_tree()
      param           Context info for the operation
      tree            SEL_TREE for the second operand of the operation
      new_imerge  OUT imerge for the result of the operation

  DESCRIPTION
    This function performs AND operation for this imerge m and the
    range part of the SEL_TREE tree rt. In other words the function
    pushes rt into this imerge. The resulting imerge is returned in
    the parameter new_imerge.
    If this imerge m represent the formula
      RT_1 OR ... OR RT_k
    then the resulting imerge of the function represents the formula
      (RT_1 AND RT) OR ... OR (RT_k AND RT)
    The function calls the function and_range_trees to construct the
    range tree representing (RT_i AND RT).
    
  NOTE
    The function may return an empty imerge without any range trees.
    This happens when each call of and_range_trees returns an 
    impossible range tree (SEL_TREE::IMPOSSIBLE).
    Example: (key1 < 2 AND key2 > 10) AND (key1 > 4 OR key2 < 6).
         
  RETURN
     0  if the operation is a success
    -1  otherwise: there is not enough memory to perform the operation
*/

int SEL_IMERGE::and_sel_tree(RANGE_OPT_PARAM *param, SEL_TREE *tree,
                             SEL_IMERGE *new_imerge)
{
  for (SEL_TREE** or_tree= trees; or_tree != trees_next; or_tree++) 
  {
    SEL_TREE *res_or_tree= 0;
    SEL_TREE *and_tree= 0;
    if (!(res_or_tree= new SEL_TREE()) ||
        !(and_tree= new SEL_TREE(tree, TRUE, param)))
      return (-1);
    if (!and_range_trees(param, *or_tree, and_tree, res_or_tree))
    {
      if (new_imerge->or_sel_tree(param, res_or_tree))
        return (-1);
    }        
  }
  return 0;
}      


/*
  Perform OR operation on this imerge and the range part of a tree

  SYNOPSIS
    or_sel_tree_with_checks()
      param                  Context info for the operation 
      n_trees                Number of trees in this imerge to check for oring        
      tree                   SEL_TREE whose range part is to be ored 
      is_first_check_pass    <=> the first call of the function for this imerge  
      is_last_check_pass OUT <=> no more calls of the function for this imerge

  DESCRIPTION
    The function performs OR operation on this imerge m and the range part
    of the SEL_TREE tree rt. It always replaces this imerge with the result
    of the operation.
 
    The operation can be performed in two different modes: with
    is_first_check_pass==TRUE and is_first_check_pass==FALSE, transforming
    this imerge differently.

    Given this imerge represents the formula
      RT_1 OR ... OR RT_k:

    1. In the first mode, when is_first_check_pass==TRUE :
      1.1. If rt must be ored(see the function sel_trees_must_be_ored) with
           some rt_j (there may be only one such range tree in the imerge)
           then the function produces an imerge representing the formula
             RT_1 OR ... OR (RT_j OR RT) OR ... OR RT_k,
           where the tree for (RT_j OR RT) is built by oring the pairs
           of SEL_ARG trees for the corresponding indexes
      1.2. Otherwise the function produces the imerge representing the formula:
           RT_1 OR ... OR RT_k OR RT.

    2. In the second mode, when is_first_check_pass==FALSE :
      2.1. For each rt_j in the imerge that can be ored (see the function
           sel_trees_can_be_ored) with rt the function replaces rt_j for a
           range tree such that for each index for which ranges are defined
           in both in rt_j and rt  the tree contains the  result of oring of
           these ranges.
      2.2. In other cases the function does not produce any imerge.

    When is_first_check==TRUE the function returns FALSE in the parameter
    is_last_check_pass if there is no rt_j such that rt_j can be ored with rt,
    but, at the same time, it's not true that rt_j must be ored with rt.
    When is_first_check==FALSE the function always returns FALSE in the
    parameter is_last_check_pass.    
          
  RETURN
    1  The result of oring of rt_j and rt that must be ored returns the
       the range tree with type==SEL_TREE::ALWAYS
       (in this case the imerge m should be discarded)
   -1  The function runs out of memory
    0  in all other cases 
*/

int SEL_IMERGE::or_sel_tree_with_checks(RANGE_OPT_PARAM *param,
                                        uint n_trees,
                                        SEL_TREE *tree,
                                        bool is_first_check_pass,
                                        bool *is_last_check_pass)
{
  bool was_ored= FALSE;
  *is_last_check_pass= is_first_check_pass;
  SEL_TREE** or_tree = trees;
  for (uint i= 0; i < n_trees; i++, or_tree++)
  {
    SEL_TREE *result= 0;
    key_map result_keys;
    key_map ored_keys;
    if (sel_trees_can_be_ored(param, *or_tree, tree, &ored_keys))
    {
      bool must_be_ored= sel_trees_must_be_ored(param, *or_tree, tree,
                                                ored_keys);
      if (must_be_ored || !is_first_check_pass)
      {
        result_keys.clear_all();
        result= *or_tree;
        for (uint key_no= 0; key_no < param->keys; key_no++)
        {
          if (!ored_keys.is_set(key_no))
	  {
            result->keys[key_no]= 0;
	    continue;
          }
          SEL_ARG *key1= (*or_tree)->keys[key_no];
          SEL_ARG *key2= tree->keys[key_no];
          key2->incr_refs();
          if ((result->keys[key_no]= key_or(param, key1, key2)))
          {
            
            result_keys.set_bit(key_no);
#ifdef EXTRA_DEBUG
            if (param->alloced_sel_args < SEL_ARG::MAX_SEL_ARGS)
	    {
              key1= result->keys[key_no]; 
              (key1)->test_use_count(key1);
            }
#endif
          }       
        }
      }
      else if(is_first_check_pass) 
        *is_last_check_pass= FALSE;
    } 

    if (result)
    {
      result->keys_map= result_keys;
      if (result_keys.is_clear_all())
        result->type= SEL_TREE::ALWAYS;
      if ((result->type == SEL_TREE::MAYBE) ||
          (result->type == SEL_TREE::ALWAYS))
        return 1;
      /* SEL_TREE::IMPOSSIBLE is impossible here */
      *or_tree= result;
      was_ored= TRUE;
    }
  }
  if (was_ored)
    return 0;

  if (is_first_check_pass && !*is_last_check_pass &&
      !(tree= new SEL_TREE(tree, FALSE, param)))
    return (-1);
  return or_sel_tree(param, tree);
}


/*
  Perform OR operation on this imerge and and another imerge

  SYNOPSIS
    or_sel_imerge_with_checks()
      param                  Context info for the operation 
      n_trees           Number of trees in this imerge to check for oring        
      imerge                 The second operand of the operation 
      is_first_check_pass    <=> the first call of the function for this imerge  
      is_last_check_pass OUT <=> no more calls of the function for this imerge

  DESCRIPTION
    For each range tree rt from 'imerge' the function calls the method
    SEL_IMERGE::or_sel_tree_with_checks that performs OR operation on this
    SEL_IMERGE object m and the tree rt. The mode of the operation is
    specified by the parameter is_first_check_pass. Each call of
    SEL_IMERGE::or_sel_tree_with_checks transforms this SEL_IMERGE object m.
    The function returns FALSE in the prameter is_last_check_pass if
    at least one of the calls of SEL_IMERGE::or_sel_tree_with_checks
    returns FALSE as the value of its last parameter. 
    
  RETURN
    1  One of the calls of SEL_IMERGE::or_sel_tree_with_checks returns 1.
       (in this case the imerge m should be discarded)
   -1  The function runs out of memory
    0  in all other cases 
*/

int SEL_IMERGE::or_sel_imerge_with_checks(RANGE_OPT_PARAM *param,
                                          uint n_trees,
                                          SEL_IMERGE* imerge,
                                          bool is_first_check_pass,
                                          bool *is_last_check_pass)
{
  *is_last_check_pass= TRUE;
  SEL_TREE** tree= imerge->trees;
  SEL_TREE** tree_end= imerge->trees_next;
  for ( ; tree < tree_end; tree++)
  {
    uint rc;
    bool is_last= TRUE; 
    rc= or_sel_tree_with_checks(param, n_trees, *tree, 
                               is_first_check_pass, &is_last);
    if (!is_last)
      *is_last_check_pass= FALSE;
    if (rc)
      return rc;
  }
  return 0;
}


/*
  Copy constructor for SEL_TREE objects

  SYNOPSIS
    SEL_TREE
      arg            The source tree for the constructor
      without_merges <=> only the range part of the tree arg is copied
      param          Context info for the operation

  DESCRIPTION
    The constructor creates a full copy of the SEL_TREE arg if
    the prameter without_merges==FALSE. Otherwise a tree is created
    that contains the copy only of the range part of the tree arg. 
*/ 

SEL_TREE::SEL_TREE(SEL_TREE *arg, bool without_merges,
                   RANGE_OPT_PARAM *param): Sql_alloc()
{
  keys_map= arg->keys_map;
  type= arg->type;
  for (uint idx= 0; idx < param->keys; idx++)
  {
    if ((keys[idx]= arg->keys[idx]))
      keys[idx]->incr_refs_all();
  }

  if (without_merges)
    return;

  List_iterator<SEL_IMERGE> it(arg->merges);
  for (SEL_IMERGE *el= it++; el; el= it++)
  {
    SEL_IMERGE *merge= new SEL_IMERGE(el, 0, param);
    if (!merge || merge->trees == merge->trees_next)
    {
      merges.empty();
      return;
    }
    merges.push_back (merge);
  }
}


/*
  Copy constructor for SEL_IMERGE objects

  SYNOPSIS
    SEL_IMERGE
      arg         The source imerge for the constructor
      cnt         How many trees from arg are to be copied
      param       Context info for the operation

  DESCRIPTION
    The cnt==0 then the constructor creates a full copy of the 
    imerge arg. Otherwise only the first cnt trees of the imerge
    are copied.
*/ 

SEL_IMERGE::SEL_IMERGE(SEL_IMERGE *arg, uint cnt,
                       RANGE_OPT_PARAM *param) : Sql_alloc()
{
  uint elements= (arg->trees_end - arg->trees);
  if (elements > PREALLOCED_TREES)
  {
    uint size= elements * sizeof (SEL_TREE **);
    if (!(trees= (SEL_TREE **)alloc_root(param->mem_root, size)))
      goto mem_err;
  }
  else
    trees= &trees_prealloced[0];

  trees_next= trees + (cnt ? cnt : arg->trees_next-arg->trees);
  trees_end= trees + elements;

  for (SEL_TREE **tree = trees, **arg_tree= arg->trees; tree < trees_next; 
       tree++, arg_tree++)
  {
    if (!(*tree= new SEL_TREE(*arg_tree, TRUE, param)))
      goto mem_err;
  }

  return;

mem_err:
  trees= &trees_prealloced[0];
  trees_next= trees;
  trees_end= trees;
}


/*
  Perform AND operation on two imerge lists

  SYNOPSIS
    imerge_list_and_list()
      param             Context info for the operation         
      im1               The first imerge list for the operation
      im2               The second imerge list for the operation

  DESCRIPTION
    The function just appends the imerge list im2 to the imerge list im1  
    
  RETURN VALUE
    none
*/

inline void imerge_list_and_list(List<SEL_IMERGE> *im1, List<SEL_IMERGE> *im2)
{
  im1->concat(im2);
}


/*
  Perform OR operation on two imerge lists

  SYNOPSIS
    imerge_list_or_list()
      param             Context info for the operation         
      im1               The first imerge list for the operation
      im2               The second imerge list for the operation
     
  DESCRIPTION
    Assuming that the first imerge list represents the formula
      F1= M1_1 AND ... AND M1_k1 
    while the second imerge list represents the formula 
      F2= M2_1 AND ... AND M2_k2,
    where M1_i= RT1_i_1 OR ... OR RT1_i_l1i (i in [1..k1])
    and M2_i = RT2_i_1 OR ... OR RT2_i_l2i (i in [1..k2]),
    the function builds a list of imerges for some formula that can be 
    inferred from the formula (F1 OR F2).

    More exactly the function builds imerges for the formula (M1_1 OR M2_1).
    Note that
      (F1 OR F2) = (M1_1 AND ... AND M1_k1) OR (M2_1 AND ... AND M2_k2) =
      AND (M1_i OR M2_j) (i in [1..k1], j in [1..k2]) =>
      M1_1 OR M2_1.
    So (M1_1 OR M2_1) is indeed an inference formula for (F1 OR F2).

    To build imerges for the formula (M1_1 OR M2_1) the function invokes,
    possibly twice, the method SEL_IMERGE::or_sel_imerge_with_checks
    for the imerge m1_1.
    At its first invocation the method SEL_IMERGE::or_sel_imerge_with_checks
    performs OR operation on the imerge m1_1 and the range tree rt2_1_1 by
    calling SEL_IMERGE::or_sel_tree_with_checks with is_first_pass_check==TRUE.
    The resulting imerge of the operation is ored with the next range tree of
    the imerge m2_1. This oring continues until the last range tree from
    m2_1 has been ored. 
    At its second invocation the method SEL_IMERGE::or_sel_imerge_with_checks
    performs the same sequence of OR operations, but now calling
    SEL_IMERGE::or_sel_tree_with_checks with is_first_pass_check==FALSE.

    The imerges that the operation produces replace those in the list im1   
       
  RETURN
    0     if the operation is a success 
   -1     if the function has run out of memory 
*/

int imerge_list_or_list(RANGE_OPT_PARAM *param,
                        List<SEL_IMERGE> *im1,
                        List<SEL_IMERGE> *im2)
{

  uint rc;
  bool is_last_check_pass= FALSE;

  SEL_IMERGE *imerge= im1->head();
  uint elems= imerge->trees_next-imerge->trees;
  im1->empty();
  im1->push_back(imerge);

  rc= imerge->or_sel_imerge_with_checks(param, elems, im2->head(),
                                        TRUE, &is_last_check_pass);
  if (rc)
  {
    if (rc == 1)
    {
      im1->empty();
      rc= 0;
    }
    return rc;
  }

  if (!is_last_check_pass)
  {
    SEL_IMERGE* new_imerge= new SEL_IMERGE(imerge, elems, param);
    if (new_imerge)
    {
      is_last_check_pass= TRUE;
      rc= new_imerge->or_sel_imerge_with_checks(param, elems, im2->head(),
                                                 FALSE, &is_last_check_pass);
      if (!rc)
        im1->push_back(new_imerge); 
    }
  }
  return rc;  
}


/*
  Perform OR operation for each imerge from a list and the range part of a tree

  SYNOPSIS
    imerge_list_or_tree()
      param       Context info for the operation
      merges      The list of imerges to be ored with the range part of tree          
      tree        SEL_TREE whose range part is to be ored with the imerges

  DESCRIPTION
    For each imerge mi from the list 'merges' the function performes OR
    operation with mi and the range part of 'tree' rt, producing one or
    two imerges.

    Given the merge mi represent the formula RTi_1 OR ... OR RTi_k, 
    the function forms the merges by the following rules:
 
    1. If rt cannot be ored with any of the trees rti the function just
       produces an imerge that represents the formula
         RTi_1 OR ... RTi_k OR RT.
    2. If there exist a tree rtj that must be ored with rt the function
       produces an imerge the represents the formula
         RTi_1 OR ... OR (RTi_j OR RT) OR ... OR RTi_k,
       where the range tree for (RTi_j OR RT) is constructed by oring the
       SEL_ARG trees that must be ored.
    3. For each rti_j that can be ored with rt the function produces
       the new tree rti_j' and substitutes rti_j for this new range tree.

    In any case the function removes mi from the list and then adds all
    produced imerges.

    To build imerges by rules 1-3 the function calls the method
    SEL_IMERGE::or_sel_tree_with_checks, possibly twice. With the first
    call it passes TRUE for the third parameter of the function.
    At this first call imerges by rules 1-2 are built. If the call
    returns FALSE as the return value of its fourth parameter then the
    function are called for the second time. At this call the imerge
    of rule 3 is produced.

    If a call of SEL_IMERGE::or_sel_tree_with_checks returns 1 then
    then it means that the produced tree contains an always true
    range tree and the whole imerge can be discarded.
    
  RETURN
    1     if no imerges are produced
    0     otherwise
*/

static
int imerge_list_or_tree(RANGE_OPT_PARAM *param,
                        List<SEL_IMERGE> *merges,
                        SEL_TREE *tree)
{

  SEL_IMERGE *imerge;
  List<SEL_IMERGE> additional_merges;
  List_iterator<SEL_IMERGE> it(*merges);
  
  while ((imerge= it++))
  {
    bool is_last_check_pass;
    int rc= 0;
    int rc1= 0;
    SEL_TREE *or_tree= new SEL_TREE (tree, FALSE, param);
    if (or_tree)
    {
      uint elems= imerge->trees_next-imerge->trees;
      rc= imerge->or_sel_tree_with_checks(param, elems, or_tree,
                                          TRUE, &is_last_check_pass);
      if (!is_last_check_pass)
      {
        SEL_IMERGE *new_imerge= new SEL_IMERGE(imerge, elems, param);
        if (new_imerge)
	{ 
          rc1= new_imerge->or_sel_tree_with_checks(param, elems, or_tree,
                                                   FALSE, &is_last_check_pass);
          if (!rc1)
            additional_merges.push_back(new_imerge);
        }
      }
    }
    if (rc || rc1 || !or_tree)
      it.remove();
  }

  merges->concat(&additional_merges);  
  return merges->is_empty();
}


/*
  Perform pushdown operation of the range part of a tree into given imerges 

  SYNOPSIS
    imerge_list_and_tree()
      param           Context info for the operation
      merges   IN/OUT List of imerges to push the range part of 'tree' into
      tree            SEL_TREE whose range part is to be pushed into imerges
      replace         if the pushdow operation for a imerge is a success
                      then the original imerge is replaced for the result
                      of the pushdown 

  DESCRIPTION
    For each imerge from the list merges the function pushes the range part
    rt of 'tree' into the imerge. 
    More exactly if the imerge mi from the list represents the formula
      RTi_1 OR ... OR RTi_k 
    the function bulds a new imerge that represents the formula
      (RTi_1 AND RT) OR ... OR (RTi_k AND RT)
    and adds this imerge to the list merges.
    To perform this pushdown operation the function calls the method
    SEL_IMERGE::and_sel_tree. 
    For any imerge mi the new imerge is not created if for each pair of
    trees rti_j and rt the intersection of the indexes with defined ranges
    is empty.
    If the result of the pushdown operation for the imerge mi returns an
    imerge with no trees then then not only nothing is added to the list 
    merges but mi itself is removed from the list. 

  TODO
    Optimize the code in order to not create new SEL_IMERGE and new SER_TREE
    objects when 'replace' is TRUE. (Currently this function is called always
    with this parameter equal to TRUE.)
    
  RETURN
    1    if no imerges are left in the list merges             
    0    otherwise
*/

static
int imerge_list_and_tree(RANGE_OPT_PARAM *param,
                         List<SEL_IMERGE> *merges,
                         SEL_TREE *tree, 
                         bool replace)
{
  SEL_IMERGE *imerge;
  SEL_IMERGE *new_imerge= NULL;
  List<SEL_IMERGE> new_merges;
  List_iterator<SEL_IMERGE> it(*merges);
  
  while ((imerge= it++))
  {
    if (!new_imerge)
       new_imerge= new SEL_IMERGE();
    if (imerge->have_common_keys(param, tree) && 
        new_imerge && !imerge->and_sel_tree(param, tree, new_imerge))
    {
      if (new_imerge->trees == new_imerge->trees_next)
        it.remove();
      else
      { 
        if (replace)
          it.replace(new_imerge);
        else        
          new_merges.push_back(new_imerge);
        new_imerge= NULL;
      }
    }
  }
  imerge_list_and_list(&new_merges, merges);
  *merges= new_merges;
  return merges->is_empty();
}


/***************************************************************************
** Basic functions for SQL_SELECT and QUICK_RANGE_SELECT
***************************************************************************/

	/* make a select from mysql info
	   Error is set as following:
	   0 = ok
	   1 = Got some error (out of memory?)
	   */

SQL_SELECT *make_select(TABLE *head, table_map const_tables,
			table_map read_tables, COND *conds,
                        bool allow_null_cond,
                        int *error)
{
  SQL_SELECT *select;
  DBUG_ENTER("make_select");

  *error=0;

  if (!conds && !allow_null_cond)
    DBUG_RETURN(0);
  if (!(select= new SQL_SELECT))
  {
    *error= 1;			// out of memory
    DBUG_RETURN(0);		/* purecov: inspected */
  }
  select->read_tables=read_tables;
  select->const_tables=const_tables;
  select->head=head;
  select->cond= conds;

  if (head->sort.io_cache)
  {
    select->file= *head->sort.io_cache;
    select->records=(ha_rows) (select->file.end_of_file/
			       head->file->ref_length);
    my_free(head->sort.io_cache);
    head->sort.io_cache=0;
  }
  DBUG_RETURN(select);
}


SQL_SELECT::SQL_SELECT() :quick(0),cond(0),pre_idx_push_select_cond(NULL),free_cond(0)
{
  quick_keys.clear_all(); needed_reg.clear_all();
  my_b_clear(&file);
}


void SQL_SELECT::cleanup()
{
  delete quick;
  quick= 0;
  if (free_cond)
  {
    free_cond=0;
    delete cond;
    cond= 0;
  }
  close_cached_file(&file);
}


SQL_SELECT::~SQL_SELECT()
{
  cleanup();
}

#undef index					// Fix for Unixware 7

QUICK_SELECT_I::QUICK_SELECT_I()
  :max_used_key_length(0),
   used_key_parts(0)
{}

QUICK_RANGE_SELECT::QUICK_RANGE_SELECT(THD *thd, TABLE *table, uint key_nr,
                                       bool no_alloc, MEM_ROOT *parent_alloc,
                                       bool *create_error)
  :doing_key_read(0),free_file(0),cur_range(NULL),last_range(0),dont_free(0)
{
  my_bitmap_map *bitmap;
  DBUG_ENTER("QUICK_RANGE_SELECT::QUICK_RANGE_SELECT");

  in_ror_merged_scan= 0;
  index= key_nr;
  head=  table;
  key_part_info= head->key_info[index].key_part;
  my_init_dynamic_array(&ranges, sizeof(QUICK_RANGE*), 16, 16);

  /* 'thd' is not accessible in QUICK_RANGE_SELECT::reset(). */
  mrr_buf_size= thd->variables.mrr_buff_size;
  mrr_buf_desc= NULL;

  if (!no_alloc && !parent_alloc)
  {
    // Allocates everything through the internal memroot
    init_sql_alloc(&alloc, thd->variables.range_alloc_block_size, 0);
    thd->mem_root= &alloc;
  }
  else
    bzero((char*) &alloc,sizeof(alloc));
  file= head->file;
  record= head->record[0];

  /* Allocate a bitmap for used columns (Q: why not on MEM_ROOT?) */
  if (!(bitmap= (my_bitmap_map*) my_malloc(head->s->column_bitmap_size,
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
  if (!(mrr_flags & HA_MRR_SORTED))
  {
    /*
      Native implementation can't produce sorted output. We'll have to
      switch to default
    */
    mrr_flags |= HA_MRR_USE_DEFAULT_IMPL; 
  }
  mrr_flags |= HA_MRR_SORTED;
}


int QUICK_RANGE_SELECT::init()
{
  DBUG_ENTER("QUICK_RANGE_SELECT::init");

  if (file->inited != handler::NONE)
    file->ha_index_or_rnd_end();
  DBUG_RETURN(FALSE);
}


void QUICK_RANGE_SELECT::range_end()
{
  if (file->inited != handler::NONE)
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
      if (doing_key_read)
        file->extra(HA_EXTRA_NO_KEYREAD);
      if (free_file)
      {
        DBUG_PRINT("info", ("Freeing separate handler 0x%lx (free: %d)", (long) file,
                            free_file));
        file->ha_external_lock(current_thd, F_UNLCK);
        file->ha_close();
        delete file;
      }
    }
    delete_dynamic(&ranges); /* ranges are allocated in alloc */
    free_root(&alloc,MYF(0));
    my_free(column_bitmap.bitmap);
  }
  my_free(mrr_buf_desc);
  DBUG_VOID_RETURN;
}

/*
  QUICK_INDEX_SORT_SELECT works as follows:
  - Do index scans, accumulate rowids in the Unique object 
    (Unique will also sort and de-duplicate rowids)
  - Use rowids from unique to run a disk-ordered sweep
*/

QUICK_INDEX_SORT_SELECT::QUICK_INDEX_SORT_SELECT(THD *thd_param,
                                                 TABLE *table)
  :unique(NULL), pk_quick_select(NULL), thd(thd_param)
{
  DBUG_ENTER("QUICK_INDEX_SORT_SELECT::QUICK_INDEX_SORT_SELECT");
  index= MAX_KEY;
  head= table;
  bzero(&read_record, sizeof(read_record));
  init_sql_alloc(&alloc, thd->variables.range_alloc_block_size, 0);
  DBUG_VOID_RETURN;
}

int QUICK_INDEX_SORT_SELECT::init()
{
  DBUG_ENTER("QUICK_INDEX_SORT_SELECT::init");
  DBUG_RETURN(0);
}

int QUICK_INDEX_SORT_SELECT::reset()
{
  DBUG_ENTER("QUICK_INDEX_SORT_SELECT::reset");
  const int retval= read_keys_and_merge();
  DBUG_RETURN(retval);
}

bool
QUICK_INDEX_SORT_SELECT::push_quick_back(QUICK_RANGE_SELECT *quick_sel_range)
{
  DBUG_ENTER("QUICK_INDEX_SORT_SELECT::push_quick_back");
  if (head->file->primary_key_is_clustered() &&
      quick_sel_range->index == head->s->primary_key)
  {
   /*
     A quick_select over a clustered primary key is handled specifically
     Here we assume:
     - PK columns are included in any other merged index
     - Scan on the PK is disk-ordered.
       (not meeting #2 will only cause performance degradation)

       We could treat clustered PK as any other index, but that would
       be inefficient. There is no point in doing scan on
       CPK, remembering the rowid, then making rnd_pos() call with
       that rowid.
    */
    pk_quick_select= quick_sel_range;
    DBUG_RETURN(0);
  }
  DBUG_RETURN(quick_selects.push_back(quick_sel_range));
}

QUICK_INDEX_SORT_SELECT::~QUICK_INDEX_SORT_SELECT()
{
  List_iterator_fast<QUICK_RANGE_SELECT> quick_it(quick_selects);
  QUICK_RANGE_SELECT* quick;
  DBUG_ENTER("QUICK_INDEX_SORT_SELECT::~QUICK_INDEX_SORT_SELECT");
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
    init_sql_alloc(&alloc, thd->variables.range_alloc_block_size, 0);
  else
    bzero(&alloc, sizeof(MEM_ROOT));
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

int QUICK_RANGE_SELECT::init_ror_merged_scan(bool reuse_handler, MEM_ROOT *alloc)
{
  handler *save_file= file, *org_file;
  my_bool org_key_read;
  THD *thd= head->in_use;
  MY_BITMAP * const save_read_set= head->read_set;
  MY_BITMAP * const save_write_set= head->write_set;
  DBUG_ENTER("QUICK_RANGE_SELECT::init_ror_merged_scan");

  in_ror_merged_scan= 1;
  if (reuse_handler)
  {
    DBUG_PRINT("info", ("Reusing handler 0x%lx", (long) file));
    if (init())
    {
      DBUG_RETURN(1);
    }
    head->column_bitmaps_set(&column_bitmap, &column_bitmap);
    goto end;
  }

  /* Create a separate handler object for this quick select */
  if (free_file)
  {
    /* already have own 'handler' object. */
    DBUG_RETURN(0);
  }

  if (!(file= head->file->clone(head->s->normalized_path.str, alloc)))
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

  if (init())
  {
    file->ha_external_lock(thd, F_UNLCK);
    file->ha_close();
    goto failure;
  }
  free_file= TRUE;
  last_rowid= file->ref;

end:
  DBUG_ASSERT(head->read_set == &column_bitmap);
  /*
    We are only going to read key fields and call position() on 'file'
    The following sets head->tmp_set to only use this key and then updates
    head->read_set and head->write_set to use this bitmap.
    The now bitmap is stored in 'column_bitmap' which is used in ::get_next()
  */
  org_file= head->file;
  org_key_read= head->key_read;
  head->file= file;
  head->key_read= 0;
  head->mark_columns_used_by_index_no_reset(index, head->read_set);

  if (!head->no_keyread)
  {
    doing_key_read= 1;
    head->enable_keyread();
  }

  head->prepare_for_position();

  head->file= org_file;
  head->key_read= org_key_read;

  /* Restore head->read_set (and write_set) to what they had before the call */
  head->column_bitmaps_set(save_read_set, save_write_set);
 
  if (reset())
  {
    if (!reuse_handler)
    {
      file->ha_external_lock(thd, F_UNLCK);
      file->ha_close();
      goto failure;
    }
    else
      DBUG_RETURN(1);
  }
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
int QUICK_ROR_INTERSECT_SELECT::init_ror_merged_scan(bool reuse_handler, 
                                                     MEM_ROOT *alloc)
{
  List_iterator_fast<QUICK_SELECT_WITH_RECORD> quick_it(quick_selects);
  QUICK_SELECT_WITH_RECORD *cur;
  QUICK_RANGE_SELECT *quick;
  DBUG_ENTER("QUICK_ROR_INTERSECT_SELECT::init_ror_merged_scan");

  /* Initialize all merged "children" quick selects */
  DBUG_ASSERT(!need_to_fetch_row || reuse_handler);
  if (!need_to_fetch_row && reuse_handler)
  {
    cur= quick_it++;
    quick= cur->quick;
    /*
      There is no use of this->file. Use it for the first of merged range
      selects.
    */
    int error= quick->init_ror_merged_scan(TRUE, alloc);
    if (error)
      DBUG_RETURN(error);
    quick->file->extra(HA_EXTRA_KEYREAD_PRESERVE_FIELDS);
  }
  while ((cur= quick_it++))
  {
    quick= cur->quick;
#ifndef DBUG_OFF
    const MY_BITMAP * const save_read_set= quick->head->read_set;
    const MY_BITMAP * const save_write_set= quick->head->write_set;
#endif
    if (quick->init_ror_merged_scan(FALSE, alloc))
      DBUG_RETURN(1);
    quick->file->extra(HA_EXTRA_KEYREAD_PRESERVE_FIELDS);

    // Sets are shared by all members of "quick_selects" so must not change
#ifndef DBUG_OFF
    DBUG_ASSERT(quick->head->read_set == save_read_set);
    DBUG_ASSERT(quick->head->write_set == save_write_set);
#endif
    /* All merged scans share the same record buffer in intersection. */
    quick->record= head->record[0];
  }

  if (need_to_fetch_row && head->file->ha_rnd_init_with_error(1))
  {
    DBUG_PRINT("error", ("ROR index_merge rnd_init call failed"));
    DBUG_RETURN(1);
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
  if (!scans_inited && init_ror_merged_scan(TRUE, &alloc))
    DBUG_RETURN(1);
  scans_inited= TRUE;
  List_iterator_fast<QUICK_SELECT_WITH_RECORD> it(quick_selects);
  QUICK_SELECT_WITH_RECORD *qr;
  while ((qr= it++))
    qr->quick->reset();
  DBUG_RETURN(0);
}


/*
  Add a merged quick select to this ROR-intersection quick select.

  SYNOPSIS
    QUICK_ROR_INTERSECT_SELECT::push_quick_back()
      alloc Mem root to create auxiliary structures on
      quick Quick select to be added. The quick select must return
            rows in rowid order.
  NOTES
    This call can only be made before init() is called.

  RETURN
    FALSE OK
    TRUE  Out of memory.
*/

bool
QUICK_ROR_INTERSECT_SELECT::push_quick_back(MEM_ROOT *alloc, QUICK_RANGE_SELECT *quick)
{
  QUICK_SELECT_WITH_RECORD *qr;
  if (!(qr= new QUICK_SELECT_WITH_RECORD) || 
      !(qr->key_tuple= (uchar*)alloc_root(alloc, quick->max_used_key_length)))
    return TRUE;
  qr->quick= quick;
  return quick_selects.push_back(qr);
}


QUICK_ROR_INTERSECT_SELECT::~QUICK_ROR_INTERSECT_SELECT()
{
  DBUG_ENTER("QUICK_ROR_INTERSECT_SELECT::~QUICK_ROR_INTERSECT_SELECT");
  quick_selects.delete_elements();
  delete cpk_quick;
  free_root(&alloc,MYF(0));
  if (need_to_fetch_row && head->file->inited != handler::NONE)
    head->file->ha_rnd_end();
  DBUG_VOID_RETURN;
}


QUICK_ROR_UNION_SELECT::QUICK_ROR_UNION_SELECT(THD *thd_param,
                                               TABLE *table)
  : thd(thd_param), scans_inited(FALSE)
{
  index= MAX_KEY;
  head= table;
  rowid_length= table->file->ref_length;
  record= head->record[0];
  init_sql_alloc(&alloc, thd->variables.range_alloc_block_size, 0);
  thd_param->mem_root= &alloc;
}


/*
  Comparison function to be used QUICK_ROR_UNION_SELECT::queue priority
  queue.

  SYNPOSIS
    QUICK_ROR_UNION_SELECT_queue_cmp()
      arg   Pointer to QUICK_ROR_UNION_SELECT
      val1  First merged select
      val2  Second merged select
*/

C_MODE_START

static int QUICK_ROR_UNION_SELECT_queue_cmp(void *arg, uchar *val1, uchar *val2)
{
  QUICK_ROR_UNION_SELECT *self= (QUICK_ROR_UNION_SELECT*)arg;
  return self->head->file->cmp_ref(((QUICK_SELECT_I*)val1)->last_rowid,
                                   ((QUICK_SELECT_I*)val2)->last_rowid);
}

C_MODE_END


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
  if (init_queue(&queue, quick_selects.elements, 0,
                 FALSE , QUICK_ROR_UNION_SELECT_queue_cmp,
                 (void*) this, 0, 0))
  {
    bzero(&queue, sizeof(QUEUE));
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
      if (quick->init_ror_merged_scan(FALSE, &alloc))
        DBUG_RETURN(1);
    }
    scans_inited= TRUE;
  }
  queue_remove_all(&queue);
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
    queue_insert(&queue, (uchar*)quick);
  }

  if ((error= head->file->ha_rnd_init(1)))
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
  delete_queue(&queue);
  quick_selects.delete_elements();
  if (head->file->inited != handler::NONE)
    head->file->ha_rnd_end();
  free_root(&alloc,MYF(0));
  DBUG_VOID_RETURN;
}


QUICK_RANGE::QUICK_RANGE()
  :min_key(0),max_key(0),min_length(0),max_length(0),
   flag(NO_MIN_RANGE | NO_MAX_RANGE),
  min_keypart_map(0), max_keypart_map(0)
{}

SEL_ARG::SEL_ARG(SEL_ARG &arg) :Sql_alloc()
{
  type=arg.type;
  min_flag=arg.min_flag;
  max_flag=arg.max_flag;
  maybe_flag=arg.maybe_flag;
  maybe_null=arg.maybe_null;
  part=arg.part;
  field=arg.field;
  min_value=arg.min_value;
  max_value=arg.max_value;
  next_key_part=arg.next_key_part;
  max_part_no= arg.max_part_no;
  use_count=1; elements=1;
}


inline void SEL_ARG::make_root()
{
  left=right= &null_element;
  color=BLACK;
  next=prev=0;
  use_count=0; elements=1;
}

SEL_ARG::SEL_ARG(Field *f,const uchar *min_value_arg,
                 const uchar *max_value_arg)
  :min_flag(0), max_flag(0), maybe_flag(0), maybe_null(f->real_maybe_null()),
   elements(1), use_count(1), field(f), min_value((uchar*) min_value_arg),
   max_value((uchar*) max_value_arg), next(0),prev(0),
   next_key_part(0), color(BLACK), type(KEY_RANGE)
{
  left=right= &null_element;
  max_part_no= 1;
}

SEL_ARG::SEL_ARG(Field *field_,uint8 part_,
                 uchar *min_value_, uchar *max_value_,
		 uint8 min_flag_,uint8 max_flag_,uint8 maybe_flag_)
  :min_flag(min_flag_),max_flag(max_flag_),maybe_flag(maybe_flag_),
   part(part_),maybe_null(field_->real_maybe_null()), elements(1),use_count(1),
   field(field_), min_value(min_value_), max_value(max_value_),
   next(0),prev(0),next_key_part(0),color(BLACK),type(KEY_RANGE)
{
  max_part_no= part+1;
  left=right= &null_element;
}

SEL_ARG *SEL_ARG::clone(RANGE_OPT_PARAM *param, SEL_ARG *new_parent, 
                        SEL_ARG **next_arg)
{
  SEL_ARG *tmp;

  /* Bail out if we have already generated too many SEL_ARGs */
  if (++param->alloced_sel_args > MAX_SEL_ARGS)
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
  tmp->max_part_no= max_part_no;
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
  if (!(root= clone(param, (SEL_ARG *) 0, &next_arg)))
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
  double read_cost;
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
  { return (void*) alloc_root(mem_root, (uint) size); }
  static void operator delete(void *ptr,size_t size) { TRASH(ptr, size); }
  static void operator delete(void *ptr, MEM_ROOT *mem_root) { /* Never called */ }
  virtual ~TABLE_READ_PLAN() {}               /* Remove gcc warning */

};

class TRP_ROR_INTERSECT;
class TRP_ROR_UNION;
class TRP_INDEX_MERGE;


/*
  Plan for a QUICK_RANGE_SELECT scan.
  TRP_RANGE::make_quick ignores retrieve_full_rows parameter because
  QUICK_RANGE_SELECT doesn't distinguish between 'index only' scans and full
  record retrieval scans.
*/

class TRP_RANGE : public TABLE_READ_PLAN
{
public:
  SEL_ARG *key; /* set of intervals to be used in "range" method retrieval */
  uint     key_idx; /* key number in PARAM::key */
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
    if ((quick= get_quick_select(param, key_idx, key,  mrr_flags, 
                                 mrr_buf_size, parent_alloc)))
    {
      quick->records= records;
      quick->read_time= read_cost;
    }
    DBUG_RETURN(quick);
  }
};


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
  double index_scan_costs; /* SUM(cost(index_scan)) */
};


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
};


/*
  Plan for QUICK_INDEX_INTERSECT_SELECT scan.
  QUICK_INDEX_INTERSECT_SELECT always retrieves full rows, so retrieve_full_rows
  is ignored by make_quick.
*/

class TRP_INDEX_INTERSECT : public TABLE_READ_PLAN
{
public:
  TRP_INDEX_INTERSECT() {}                        /* Remove gcc warning */
  virtual ~TRP_INDEX_INTERSECT() {}               /* Remove gcc warning */
  QUICK_SELECT_I *make_quick(PARAM *param, bool retrieve_full_rows,
                             MEM_ROOT *parent_alloc);
  TRP_RANGE **range_scans; /* array of ptrs to plans of intersected scans */
  TRP_RANGE **range_scans_end; /* end of the array */
  /* keys whose scans are to be filtered by cpk conditions */
  key_map filtered_scans;  
};


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
};


/*
  Plan for a QUICK_GROUP_MIN_MAX_SELECT scan. 
*/

class TRP_GROUP_MIN_MAX : public TABLE_READ_PLAN
{
private:
  bool have_min, have_max, have_agg_distinct;
  KEY_PART_INFO *min_max_arg_part;
  uint group_prefix_len;
  uint used_key_parts;
  uint group_key_parts;
  KEY *index_info;
  uint index;
  uint key_infix_len;
  uchar key_infix[MAX_KEY_LENGTH];
  SEL_TREE *range_tree; /* Represents all range predicates in the query. */
  SEL_ARG  *index_tree; /* The SEL_ARG sub-tree corresponding to index_info. */
  uint param_idx; /* Index of used key in param->key. */
  bool is_index_scan; /* Use index_next() instead of random read */ 
public:
  /* Number of records selected by the ranges in index_tree. */
  ha_rows quick_prefix_records;
public:
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


typedef struct st_index_scan_info
{
  uint      idx;      /* # of used key in param->keys */
  uint      keynr;    /* # of used key in table */
  uint      range_count;
  ha_rows   records;  /* estimate of # records this scan will return */

  /* Set of intervals over key fields that will be used for row retrieval. */
  SEL_ARG   *sel_arg;

  KEY *key_info;
  uint used_key_parts;

  /* Estimate of # records filtered out by intersection with cpk */
  ha_rows   filtered_out;
  /* Bitmap of fields used in index intersection */ 
  MY_BITMAP used_fields;

  /* Fields used in the query and covered by ROR scan. */
  MY_BITMAP covered_fields;
  uint      used_fields_covered; /* # of set bits in covered_fields */
  int       key_rec_length; /* length of key record (including rowid) */

  /*
    Cost of reading all index records with values in sel_arg intervals set
    (assuming there is no need to access full table records)
  */
  double    index_read_cost;
  uint      first_uncovered_field; /* first unused bit in covered_fields */
  uint      key_components; /* # of parts in the key */
} INDEX_SCAN_INFO;

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
    KEY_PART_INFO *key_part_end= key_part +
                                 param->table->key_info[pk].key_parts;
    for (;key_part != key_part_end; ++key_part)
      bitmap_clear_bit(&param->needed_fields, key_part->fieldnr-1);
  }
  return 0;
}


/*
  Test if a key can be used in different ranges

  SYNOPSIS
    SQL_SELECT::test_quick_select()
      thd               Current thread
      keys_to_use       Keys to use for range retrieval
      prev_tables       Tables assumed to be already read when the scan is
                        performed (but not read at the moment of this call)
      limit             Query limit
      force_quick_range Prefer to use range (instead of full table scan) even
                        if it is more expensive.

  NOTES
    Updates the following in the select parameter:
      needed_reg - Bits for keys with may be used if all prev regs are read
      quick      - Parameter to use when reading records.

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
*/

int SQL_SELECT::test_quick_select(THD *thd, key_map keys_to_use,
				  table_map prev_tables,
				  ha_rows limit, bool force_quick_range, 
                                  bool ordered_output)
{
  uint idx;
  double scan_time;
  DBUG_ENTER("SQL_SELECT::test_quick_select");
  DBUG_PRINT("enter",("keys_to_use: %lu  prev_tables: %lu  const_tables: %lu",
		      (ulong) keys_to_use.to_ulonglong(), (ulong) prev_tables,
		      (ulong) const_tables));
  DBUG_PRINT("info", ("records: %lu", (ulong) head->file->stats.records));
  delete quick;
  quick=0;
  needed_reg.clear_all();
  quick_keys.clear_all();
  DBUG_ASSERT(!head->is_filled_at_execution());
  if (keys_to_use.is_clear_all() || head->is_filled_at_execution())
    DBUG_RETURN(0);
  records= head->file->stats.records;
  if (!records)
    records++;					/* purecov: inspected */
  scan_time= (double) records / TIME_FOR_COMPARE + 1;
  read_time= (double) head->file->scan_time() + scan_time + 1.1;
  if (head->force_index)
    scan_time= read_time= DBL_MAX;
  if (limit < records)
    read_time= (double) records + scan_time + 1; // Force to use index
  else if (read_time <= 2.0 && !force_quick_range)
    DBUG_RETURN(0);				/* No need for quick select */

  DBUG_PRINT("info",("Time to scan table: %g", read_time));

  keys_to_use.intersect(head->keys_in_use_for_query);
  if (!keys_to_use.is_clear_all())
  {
    uchar buff[STACK_BUFF_ALLOC];
    MEM_ROOT alloc;
    SEL_TREE *tree= NULL;
    KEY_PART *key_parts;
    KEY *key_info;
    PARAM param;

    if (check_stack_overrun(thd, 2*STACK_MIN_SIZE + sizeof(PARAM), buff))
      DBUG_RETURN(0);                           // Fatal error flag is set

    /* set up parameter that is passed to all functions */
    param.thd= thd;
    param.baseflag= head->file->ha_table_flags();
    param.prev_tables=prev_tables | const_tables;
    param.read_tables=read_tables;
    param.current_table= head->map;
    param.table=head;
    param.keys=0;
    param.mem_root= &alloc;
    param.old_root= thd->mem_root;
    param.needed_reg= &needed_reg;
    param.imerge_cost_buff_size= 0;
    param.using_real_indexes= TRUE;
    param.remove_jump_scans= TRUE;
    param.force_default_mrr= ordered_output;

    thd->no_errors=1;				// Don't warn about NULL
    init_sql_alloc(&alloc, thd->variables.range_alloc_block_size, 0);
    if (!(param.key_parts=
           (KEY_PART*) alloc_root(&alloc,
                                  sizeof(KEY_PART) *
	                          head->s->actual_n_key_parts(thd))) ||
        fill_used_fields_bitmap(&param))
    {
      thd->no_errors=0;
      free_root(&alloc,MYF(0));			// Return memory & allocator
      DBUG_RETURN(0);				// Can't use range
    }
    key_parts= param.key_parts;
    thd->mem_root= &alloc;

    /*
      Make an array with description of all key parts of all table keys.
      This is used in get_mm_parts function.
    */
    key_info= head->key_info;
    for (idx=0 ; idx < head->s->keys ; idx++, key_info++)
    {
      KEY_PART_INFO *key_part_info;
      uint n_key_parts= head->actual_n_key_parts(key_info);

      if (!keys_to_use.is_set(idx))
	continue;
      if (key_info->flags & HA_FULLTEXT)
	continue;    // ToDo: ft-keys in non-ft ranges, if possible   SerG

      param.key[param.keys]=key_parts;
      key_part_info= key_info->key_part;
      for (uint part= 0 ; part < n_key_parts ; 
           part++, key_parts++, key_part_info++)
     {
	key_parts->key=		 param.keys;
	key_parts->part=	 part;
	key_parts->length=       key_part_info->length;
	key_parts->store_length= key_part_info->store_length;
	key_parts->field=	 key_part_info->field;
	key_parts->null_bit=	 key_part_info->null_bit;
        key_parts->image_type =
          (key_info->flags & HA_SPATIAL) ? Field::itMBR : Field::itRAW;
        /* Only HA_PART_KEY_SEG is used */
        key_parts->flag=         (uint8) key_part_info->key_part_flag;
      }
      param.real_keynr[param.keys++]=idx;
    }
    param.key_parts_end=key_parts;
    param.alloced_sel_args= 0;

    /* Calculate cost of full index read for the shortest covering index */
    if (!head->covering_keys.is_clear_all())
    {
      int key_for_use= find_shortest_key(head, &head->covering_keys);
      double key_read_time= head->file->keyread_time(key_for_use, 1, records) +
                            (double) records / TIME_FOR_COMPARE;
      DBUG_PRINT("info",  ("'all'+'using index' scan will be using key %d, "
                           "read time %g", key_for_use, key_read_time));
      if (key_read_time < read_time)
        read_time= key_read_time;
    }

    TABLE_READ_PLAN *best_trp= NULL;
    TRP_GROUP_MIN_MAX *group_trp;
    double best_read_time= read_time;

    if (cond)
    {
      if ((tree= get_mm_tree(&param,cond)))
      {
        if (tree->type == SEL_TREE::IMPOSSIBLE)
        {
          records=0L;                      /* Return -1 from this function. */
          read_time= (double) HA_POS_ERROR;
          goto free_mem;
        }
        /*
          If the tree can't be used for range scans, proceed anyway, as we
          can construct a group-min-max quick select
        */
        if (tree->type != SEL_TREE::KEY && tree->type != SEL_TREE::KEY_SMALLER)
          tree= NULL;
      }
    }

    /*
      Try to construct a QUICK_GROUP_MIN_MAX_SELECT.
      Notice that it can be constructed no matter if there is a range tree.
    */
    group_trp= get_best_group_min_max(&param, tree, best_read_time);
    if (group_trp)
    {
      param.table->quick_condition_rows= min(group_trp->records,
                                             head->file->stats.records);
      if (group_trp->read_cost < best_read_time)
      {
        best_trp= group_trp;
        best_read_time= best_trp->read_cost;
      }
    }

    if (tree)
    {
      /*
        It is possible to use a range-based quick select (but it might be
        slower than 'all' table scan).
      */
      TRP_RANGE         *range_trp;
      TRP_ROR_INTERSECT *rori_trp;
      TRP_INDEX_INTERSECT *intersect_trp;
      bool can_build_covering= FALSE;
      
      remove_nonrange_trees(&param, tree);

      /* Get best 'range' plan and prepare data for making other plans */
      if ((range_trp= get_key_scans_params(&param, tree, FALSE, TRUE,
                                           best_read_time)))
      {
        best_trp= range_trp;
        best_read_time= best_trp->read_cost;
      }

      /*
        Simultaneous key scans and row deletes on several handler
        objects are not allowed so don't use ROR-intersection for
        table deletes.
      */
      if ((thd->lex->sql_command != SQLCOM_DELETE) && 
           optimizer_flag(thd, OPTIMIZER_SWITCH_INDEX_MERGE))
      {
        /*
          Get best non-covering ROR-intersection plan and prepare data for
          building covering ROR-intersection.
        */
        if ((rori_trp= get_best_ror_intersect(&param, tree, best_read_time,
                                              &can_build_covering)))
        {
          best_trp= rori_trp;
          best_read_time= best_trp->read_cost;
          /*
            Try constructing covering ROR-intersect only if it looks possible
            and worth doing.
          */
          if (!rori_trp->is_covering && can_build_covering &&
              (rori_trp= get_best_covering_ror_intersect(&param, tree,
                                                         best_read_time)))
            best_trp= rori_trp;
        }
      }
      /*
        Do not look for an index intersection  plan if there is a covering
        index. The scan by this covering index will be always cheaper than
        any index intersection.
      */
      if (param.table->covering_keys.is_clear_all() &&
          optimizer_flag(thd, OPTIMIZER_SWITCH_INDEX_MERGE) &&
          optimizer_flag(thd, OPTIMIZER_SWITCH_INDEX_MERGE_SORT_INTERSECT))
      {
        if ((intersect_trp= get_best_index_intersect(&param, tree,
                                                    best_read_time)))
        {
          best_trp= intersect_trp;
          best_read_time= best_trp->read_cost; 
          set_if_smaller(param.table->quick_condition_rows, 
                         intersect_trp->records);
        }
      }

      if (optimizer_flag(thd, OPTIMIZER_SWITCH_INDEX_MERGE))
      {
        /* Try creating index_merge/ROR-union scan. */
        SEL_IMERGE *imerge;
        TABLE_READ_PLAN *best_conj_trp= NULL, *new_conj_trp;
        LINT_INIT(new_conj_trp); /* no empty index_merge lists possible */
        DBUG_PRINT("info",("No range reads possible,"
                           " trying to construct index_merge"));
        List_iterator_fast<SEL_IMERGE> it(tree->merges);
        while ((imerge= it++))
        {
          new_conj_trp= get_best_disjunct_quick(&param, imerge, best_read_time);
          if (new_conj_trp)
            set_if_smaller(param.table->quick_condition_rows, 
                           new_conj_trp->records);
          if (new_conj_trp &&
              (!best_conj_trp || 
               new_conj_trp->read_cost < best_conj_trp->read_cost))
          {
            best_conj_trp= new_conj_trp;
            best_read_time= best_conj_trp->read_cost;
          }
        }
        if (best_conj_trp)
          best_trp= best_conj_trp;
      }
    }

    thd->mem_root= param.old_root;

    /* If we got a read plan, create a quick select from it. */
    if (best_trp)
    {
      records= best_trp->records;
      if (!(quick= best_trp->make_quick(&param, TRUE)) || quick->init())
      {
        delete quick;
        quick= NULL;
      }
    }

  free_mem:
    free_root(&alloc,MYF(0));			// Return memory & allocator
    thd->mem_root= param.old_root;
    thd->no_errors=0;
  }

  DBUG_EXECUTE("info", print_quick(quick, &needed_reg););

  /*
    Assume that if the user is using 'limit' we will only need to scan
    limit rows if we are using a key
  */
  DBUG_RETURN(records ? test(quick) : -1);
}

/****************************************************************************
 * Partition pruning module
 ****************************************************************************/
#ifdef WITH_PARTITION_STORAGE_ENGINE

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
static void dbug_print_field(Field *field);
static void dbug_print_segment_range(SEL_ARG *arg, KEY_PART *part);
static void dbug_print_singlepoint_range(SEL_ARG **start, uint num);
#endif


/*
  Perform partition pruning for a given table and condition.

  SYNOPSIS
    prune_partitions()
      thd           Thread handle
      table         Table to perform partition pruning for
      pprune_cond   Condition to use for partition pruning
  
  DESCRIPTION
    This function assumes that all partitions are marked as unused when it
    is invoked. The function analyzes the condition, finds partitions that
    need to be used to retrieve the records that match the condition, and 
    marks them as used by setting appropriate bit in part_info->used_partitions
    In the worst case all partitions are marked as used.

  NOTE
    This function returns promptly if called for non-partitioned table.

  RETURN
    TRUE   We've inferred that no partitions need to be used (i.e. no table
           records will satisfy pprune_cond)
    FALSE  Otherwise
*/

bool prune_partitions(THD *thd, TABLE *table, Item *pprune_cond)
{
  bool retval= FALSE;
  partition_info *part_info = table->part_info;
  DBUG_ENTER("prune_partitions");

  if (!part_info)
    DBUG_RETURN(FALSE); /* not a partitioned table */
  
  if (!pprune_cond)
  {
    mark_all_partitions_as_used(part_info);
    DBUG_RETURN(FALSE);
  }
  
  PART_PRUNE_PARAM prune_param;
  MEM_ROOT alloc;
  RANGE_OPT_PARAM  *range_par= &prune_param.range_param;
  my_bitmap_map *old_sets[2];

  prune_param.part_info= part_info;
  init_sql_alloc(&alloc, thd->variables.range_alloc_block_size, 0);
  range_par->mem_root= &alloc;
  range_par->old_root= thd->mem_root;

  if (create_partition_index_description(&prune_param))
  {
    mark_all_partitions_as_used(part_info);
    free_root(&alloc,MYF(0));		// Return memory & allocator
    DBUG_RETURN(FALSE);
  }
  
  dbug_tmp_use_all_columns(table, old_sets, 
                           table->read_set, table->write_set);
  range_par->thd= thd;
  range_par->table= table;
  /* range_par->cond doesn't need initialization */
  range_par->prev_tables= range_par->read_tables= 0;
  range_par->current_table= table->map;

  range_par->keys= 1; // one index
  range_par->using_real_indexes= FALSE;
  range_par->remove_jump_scans= FALSE;
  range_par->real_keynr[0]= 0;
  range_par->alloced_sel_args= 0;

  thd->no_errors=1;				// Don't warn about NULL
  thd->mem_root=&alloc;

  bitmap_clear_all(&part_info->used_partitions);

  prune_param.key= prune_param.range_param.key_parts;
  SEL_TREE *tree;
  int res;

  tree= get_mm_tree(range_par, pprune_cond);
  if (!tree)
    goto all_used;

  if (tree->type == SEL_TREE::IMPOSSIBLE)
  {
    retval= TRUE;
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
    res == 0 => no used partitions => retval=TRUE
    res == 1 => some used partitions => retval=FALSE
    res == -1 - we jump over this line to all_used:
  */
  retval= test(!res);
  goto end;

all_used:
  retval= FALSE; // some partitions are used
  mark_all_partitions_as_used(prune_param.part_info);
end:
  dbug_tmp_restore_column_maps(table->read_set, table->write_set, old_sets);
  thd->no_errors=0;
  thd->mem_root= range_par->old_root;
  free_root(&alloc,MYF(0));			// Return memory & allocator
  DBUG_RETURN(retval);
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
  /* Do the same as print_key() does */ 
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
  bitmap_set_bit(&part_info->used_partitions, part_id);
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
    bitmap_set_bit(&part_info->used_partitions, start);
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
  uint n_bits= ppar->part_info->used_partitions.n_bits;
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
      bitmap_intersect(&all_merges, &ppar->part_info->used_partitions);

    if (bitmap_is_clear_all(&all_merges))
      return 0;

    bitmap_clear_all(&ppar->part_info->used_partitions);
  }
  memcpy(ppar->part_info->used_partitions.bitmap, all_merges.bitmap,
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
            bitmap_set_bit(&ppar->part_info->used_partitions,
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
          bitmap_set_bit(&part_info->used_partitions,
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
      res= test(found);
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
  bitmap_set_all(&part_info->used_partitions);
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

/* Print field value into debug trace, in NULL-aware way. */
static void dbug_print_field(Field *field)
{
  if (field->is_real_null())
    fprintf(DBUG_FILE, "NULL");
  else
  {
    char buf[256];
    String str(buf, sizeof(buf), &my_charset_bin);
    str.length(0);
    String *pstr;
    pstr= field->val_str(&str);
    fprintf(DBUG_FILE, "'%s'", pstr->c_ptr_safe());
  }
}


/* Print a "c1 < keypartX < c2" - type interval into debug trace. */
static void dbug_print_segment_range(SEL_ARG *arg, KEY_PART *part)
{
  DBUG_ENTER("dbug_print_segment_range");
  DBUG_LOCK_FILE;
  if (!(arg->min_flag & NO_MIN_RANGE))
  {
    store_key_image_to_rec(part->field, arg->min_value, part->length);
    dbug_print_field(part->field);
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
    dbug_print_field(part->field);
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
    dbug_print_field(field);
  }
  fputs("\n", DBUG_FILE);
  DBUG_UNLOCK_FILE;
  DBUG_VOID_RETURN;
}
#endif

/****************************************************************************
 * Partition pruning code ends
 ****************************************************************************/
#endif


/*
  Get cost of 'sweep' full records retrieval.
  SYNOPSIS
    get_sweep_read_cost()
      param            Parameter from test_quick_select
      records          # of records to be retrieved
  RETURN
    cost of sweep
*/

double get_sweep_read_cost(const PARAM *param, ha_rows records)
{
  double result;
  DBUG_ENTER("get_sweep_read_cost");
  if (param->table->file->primary_key_is_clustered())
  {
    /*
      We are using the primary key to find the rows.
      Calculate the cost for this.
    */
    result= param->table->file->read_time(param->table->s->primary_key,
                                          (uint)records, records);
  }
  else
  {
    /*
      Rows will be retreived with rnd_pos(). Caluclate the expected
      cost for this.
    */
    double n_blocks=
      ceil(ulonglong2double(param->table->file->stats.data_file_length) /
           IO_SIZE);
    double busy_blocks=
      n_blocks * (1.0 - pow(1.0 - 1.0/n_blocks, rows2double(records)));
    if (busy_blocks < 1.0)
      busy_blocks= 1.0;
    DBUG_PRINT("info",("sweep: nblocks: %g, busy_blocks: %g", n_blocks,
                       busy_blocks));
    /*
      Disabled: Bail out if # of blocks to read is bigger than # of blocks in
      table data file.
    if (max_cost != DBL_MAX  && (busy_blocks+index_reads_cost) >= n_blocks)
      return 1;
    */
    JOIN *join= param->thd->lex->select_lex.join;
    if (!join || join->table_count == 1)
    {
      /* No join, assume reading is done in one 'sweep' */
      result= busy_blocks*(DISK_SEEK_BASE_COST +
                          DISK_SEEK_PROP_COST*n_blocks/busy_blocks);
    }
    else
    {
      /*
        Possibly this is a join with source table being non-last table, so
        assume that disk seeks are random here.
      */
      result= busy_blocks;
    }
  }
  DBUG_PRINT("return",("cost: %g", result));
  DBUG_RETURN(result);
}


/*
  Get best plan for a SEL_IMERGE disjunctive expression.
  SYNOPSIS
    get_best_disjunct_quick()
      param     Parameter from check_quick_select function
      imerge    Expression to use
      read_time Don't create scans with cost > read_time

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

      E(n_busy_blocks)*
       (DISK_SEEK_BASE_COST + DISK_SEEK_PROP_COST*n_blocks/E(n_busy_blocks)).

    3. Cost of Unique use is calculated in Unique::get_use_cost function.

  ROR-union cost is calculated in the same way index_merge, but instead of
  Unique a priority queue is used.

  RETURN
    Created read plan
    NULL - Out of memory or no read scan could be built.
*/

static
TABLE_READ_PLAN *get_best_disjunct_quick(PARAM *param, SEL_IMERGE *imerge,
                                         double read_time)
{
  SEL_TREE **ptree;
  TRP_INDEX_MERGE *imerge_trp= NULL;
  TRP_RANGE **range_scans;
  TRP_RANGE **cur_child;
  TRP_RANGE **cpk_scan= NULL;
  bool imerge_too_expensive= FALSE;
  double imerge_cost= 0.0;
  ha_rows cpk_scan_records= 0;
  ha_rows non_cpk_scan_records= 0;
  bool pk_is_clustered= param->table->file->primary_key_is_clustered();
  bool all_scans_ror_able= TRUE;
  bool all_scans_rors= TRUE;
  uint unique_calc_buff_size;
  TABLE_READ_PLAN **roru_read_plans;
  TABLE_READ_PLAN **cur_roru_plan;
  double roru_index_costs;
  ha_rows roru_total_records;
  double roru_intersect_part= 1.0;
  DBUG_ENTER("get_best_disjunct_quick");
  DBUG_PRINT("info", ("Full table scan cost: %g", read_time));

  /*
    In every tree of imerge remove SEL_ARG trees that do not make ranges.
    If after this removal some SEL_ARG tree becomes empty discard imerge.  
  */
  for (ptree= imerge->trees; ptree != imerge->trees_next; ptree++)
  {
    if (remove_nonrange_trees(param, *ptree))
    {
      imerge->trees_next= imerge->trees;
      break;
    }
  }

  uint n_child_scans= imerge->trees_next - imerge->trees;
  
  if (!n_child_scans)
    DBUG_RETURN(NULL);

  if (!(range_scans= (TRP_RANGE**)alloc_root(param->mem_root,
                                             sizeof(TRP_RANGE*)*
                                             n_child_scans)))
    DBUG_RETURN(NULL);
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
    if (!(*cur_child= get_key_scans_params(param, *ptree, TRUE, FALSE, read_time)))
    {
      /*
        One of index scans in this index_merge is more expensive than entire
        table read for another available option. The entire index_merge (and
        any possible ROR-union) will be more expensive then, too. We continue
        here only to update SQL_SELECT members.
      */
      imerge_too_expensive= TRUE;
    }
    if (imerge_too_expensive)
      continue;

    imerge_cost += (*cur_child)->read_cost;
    all_scans_ror_able &= ((*ptree)->n_ror_scans > 0);
    all_scans_rors &= (*cur_child)->is_ror;
    if (pk_is_clustered &&
        param->real_keynr[(*cur_child)->key_idx] ==
        param->table->s->primary_key)
    {
      cpk_scan= cur_child;
      cpk_scan_records= (*cur_child)->records;
    }
    else
      non_cpk_scan_records += (*cur_child)->records;
  }

  DBUG_PRINT("info", ("index_merge scans cost %g", imerge_cost));
  if (imerge_too_expensive || (imerge_cost > read_time) ||
      ((non_cpk_scan_records+cpk_scan_records >=
        param->table->file->stats.records) &&
       read_time != DBL_MAX))
  {
    /*
      Bail out if it is obvious that both index_merge and ROR-union will be
      more expensive
    */
    DBUG_PRINT("info", ("Sum of index_merge scans is more expensive than "
                        "full table scan, bailing out"));
    DBUG_RETURN(NULL);
  }

  /* 
    If all scans happen to be ROR, proceed to generate a ROR-union plan (it's 
    guaranteed to be cheaper than non-ROR union), unless ROR-unions are
    disabled in @@optimizer_switch
  */
  if (all_scans_rors && 
      optimizer_flag(param->thd, OPTIMIZER_SWITCH_INDEX_MERGE_UNION))
  {
    roru_read_plans= (TABLE_READ_PLAN**)range_scans;
    goto skip_to_ror_scan;
  }

  if (cpk_scan)
  {
    /*
      Add one ROWID comparison for each row retrieved on non-CPK scan.  (it
      is done in QUICK_RANGE_SELECT::row_in_ranges)
     */
    imerge_cost += non_cpk_scan_records / TIME_FOR_COMPARE_ROWID;
  }

  /* Calculate cost(rowid_to_row_scan) */
  imerge_cost += get_sweep_read_cost(param, non_cpk_scan_records);
  DBUG_PRINT("info",("index_merge cost with rowid-to-row scan: %g",
                     imerge_cost));
  if (imerge_cost > read_time || 
      !optimizer_flag(param->thd, OPTIMIZER_SWITCH_INDEX_MERGE_SORT_UNION))
  {
    goto build_ror_index_merge;
  }

  /* Add Unique operations cost */
  unique_calc_buff_size=
    Unique::get_cost_calc_buff_size((ulong)non_cpk_scan_records,
                                    param->table->file->ref_length,
                                    param->thd->variables.sortbuff_size);
  if (param->imerge_cost_buff_size < unique_calc_buff_size)
  {
    if (!(param->imerge_cost_buff= (uint*)alloc_root(param->mem_root,
                                                     unique_calc_buff_size)))
      DBUG_RETURN(NULL);
    param->imerge_cost_buff_size= unique_calc_buff_size;
  }

  imerge_cost +=
    Unique::get_use_cost(param->imerge_cost_buff, (uint)non_cpk_scan_records,
                         param->table->file->ref_length,
                         param->thd->variables.sortbuff_size,
                         TIME_FOR_COMPARE_ROWID,
                         FALSE, NULL);
  DBUG_PRINT("info",("index_merge total cost: %g (wanted: less then %g)",
                     imerge_cost, read_time));
  if (imerge_cost < read_time)
  {
    if ((imerge_trp= new (param->mem_root)TRP_INDEX_MERGE))
    {
      imerge_trp->read_cost= imerge_cost;
      imerge_trp->records= non_cpk_scan_records + cpk_scan_records;
      imerge_trp->records= min(imerge_trp->records,
                               param->table->file->stats.records);
      imerge_trp->range_scans= range_scans;
      imerge_trp->range_scans_end= range_scans + n_child_scans;
      read_time= imerge_cost;
    }
    if (imerge_trp)
    {
      TABLE_READ_PLAN *trp= merge_same_index_scans(param, imerge, imerge_trp,
                                                   read_time);
      if (trp != imerge_trp)
        DBUG_RETURN(trp);
    }
  }

build_ror_index_merge:
  if (!all_scans_ror_able || 
      param->thd->lex->sql_command == SQLCOM_DELETE ||
      !optimizer_flag(param->thd, OPTIMIZER_SWITCH_INDEX_MERGE_UNION))
    DBUG_RETURN(imerge_trp);

  /* Ok, it is possible to build a ROR-union, try it. */
  bool dummy;
  if (!(roru_read_plans=
          (TABLE_READ_PLAN**)alloc_root(param->mem_root,
                                        sizeof(TABLE_READ_PLAN*)*
                                        n_child_scans)))
    DBUG_RETURN(imerge_trp);

skip_to_ror_scan:
  roru_index_costs= 0.0;
  roru_total_records= 0;
  cur_roru_plan= roru_read_plans;

  /* Find 'best' ROR scan for each of trees in disjunction */
  for (ptree= imerge->trees, cur_child= range_scans;
       ptree != imerge->trees_next;
       ptree++, cur_child++, cur_roru_plan++)
  {
    /*
      Assume the best ROR scan is the one that has cheapest full-row-retrieval
      scan cost.
      Also accumulate index_only scan costs as we'll need them to calculate
      overall index_intersection cost.
    */
    double cost;
    if ((*cur_child)->is_ror)
    {
      /* Ok, we have index_only cost, now get full rows scan cost */
      cost= param->table->file->
              read_time(param->real_keynr[(*cur_child)->key_idx], 1,
                        (*cur_child)->records) +
              rows2double((*cur_child)->records) / TIME_FOR_COMPARE;
    }
    else
      cost= read_time;

    TABLE_READ_PLAN *prev_plan= *cur_child;
    if (!(*cur_roru_plan= get_best_ror_intersect(param, *ptree, cost,
                                                 &dummy)))
    {
      if (prev_plan->is_ror)
        *cur_roru_plan= prev_plan;
      else
        DBUG_RETURN(imerge_trp);
      roru_index_costs += (*cur_roru_plan)->read_cost;
    }
    else
      roru_index_costs +=
        ((TRP_ROR_INTERSECT*)(*cur_roru_plan))->index_scan_costs;
    roru_total_records += (*cur_roru_plan)->records;
    roru_intersect_part *= (*cur_roru_plan)->records /
                           param->table->file->stats.records;
  }

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

  double roru_total_cost;
  roru_total_cost= roru_index_costs +
                   rows2double(roru_total_records)*log((double)n_child_scans) /
                   (TIME_FOR_COMPARE_ROWID * M_LN2) +
                   get_sweep_read_cost(param, roru_total_records);

  DBUG_PRINT("info", ("ROR-union: cost %g, %d members", roru_total_cost,
                      n_child_scans));
  TRP_ROR_UNION* roru;
  if (roru_total_cost < read_time)
  {
    if ((roru= new (param->mem_root) TRP_ROR_UNION))
    {
      roru->first_ror= roru_read_plans;
      roru->last_ror= roru_read_plans + n_child_scans;
      roru->read_cost= roru_total_cost;
      roru->records= roru_total_records;
      DBUG_RETURN(roru);
    }
  }
    DBUG_RETURN(imerge_trp);
}


/*
  Merge index scans for the same indexes in an index merge plan

  SYNOPSIS
    merge_same_index_scans()
      param           Context info for the operation
      imerge   IN/OUT SEL_IMERGE from which imerge_trp has been extracted          
      imerge_trp      The index merge plan where index scans for the same
                      indexes are to be merges
      read_time       The upper bound for the cost of the plan to be evaluated

  DESRIPTION
    For the given index merge plan imerge_trp extracted from the SEL_MERGE
    imerge the function looks for range scans with the same indexes and merges
    them into SEL_ARG trees. Then for each such SEL_ARG tree r_i the function
    creates a range tree rt_i that contains only r_i. All rt_i are joined
    into one index merge that replaces the original index merge imerge.
    The function calls get_best_disjunct_quick for the new index merge to
    get a new index merge plan that contains index scans only for different
    indexes.
    If there are no index scans for the same index in the original index
    merge plan the function does not change the original imerge and returns
    imerge_trp as its result.

  RETURN
    The original or or improved index merge plan                        
*/

static
TABLE_READ_PLAN *merge_same_index_scans(PARAM *param, SEL_IMERGE *imerge,
                                        TRP_INDEX_MERGE *imerge_trp,
                                        double read_time)
{
  uint16 first_scan_tree_idx[MAX_KEY];
  SEL_TREE **tree;
  TRP_RANGE **cur_child;
  uint removed_cnt= 0;

  DBUG_ENTER("merge_same_index_scans");

  bzero(first_scan_tree_idx, sizeof(first_scan_tree_idx[0])*param->keys);

  for (tree= imerge->trees, cur_child= imerge_trp->range_scans;
       tree != imerge->trees_next;
       tree++, cur_child++)
  {
    DBUG_ASSERT(tree);
    uint key_idx= (*cur_child)->key_idx;
    uint16 *tree_idx_ptr= &first_scan_tree_idx[key_idx];
    if (!*tree_idx_ptr)
      *tree_idx_ptr= (uint16) (tree-imerge->trees+1);
    else
    {
      SEL_TREE **changed_tree= imerge->trees+(*tree_idx_ptr-1);
      SEL_ARG *key= (*changed_tree)->keys[key_idx];
      bzero((*changed_tree)->keys,
            sizeof((*changed_tree)->keys[0])*param->keys);
      (*changed_tree)->keys_map.clear_all();
      if (key) 
        key->incr_refs(); 
      if ((*tree)->keys[key_idx]) 
        (*tree)->keys[key_idx]->incr_refs(); 
      if (((*changed_tree)->keys[key_idx]=
             key_or(param, key, (*tree)->keys[key_idx])))
        (*changed_tree)->keys_map.set_bit(key_idx);
      *tree= NULL;
      removed_cnt++;
    }
  }
  if (!removed_cnt)
    DBUG_RETURN(imerge_trp);

  TABLE_READ_PLAN *trp= NULL;
  SEL_TREE **new_trees_next= imerge->trees;
  for (tree= new_trees_next; tree != imerge->trees_next; tree++)
  {
    if (!*tree)
      continue;
    if (tree > new_trees_next)
      *new_trees_next= *tree;
    new_trees_next++;
  }
  imerge->trees_next= new_trees_next;

  DBUG_ASSERT(imerge->trees_next>imerge->trees);

  if (imerge->trees_next-imerge->trees > 1)
    trp= get_best_disjunct_quick(param, imerge, read_time);
  else
  {
    /*
      This alternative theoretically can be reached when the cost
      of the index merge for such a formula as
        (key1 BETWEEN c1_1 AND c1_2) AND key2 > c2 OR
        (key1 BETWEEN c1_3 AND c1_4) AND key3 > c3
      is estimated as being cheaper than the cost of index scan for
      the formula
        (key1 BETWEEN c1_1 AND c1_2) OR (key1 BETWEEN c1_3 AND c1_4)
      
      In the current code this may happen for two reasons:
      1. for a single index range scan data records are accessed in
         a random order
      2. the functions that estimate the cost of a range scan and an
         index merge retrievals are not well calibrated
    */
    trp= get_key_scans_params(param, *imerge->trees, FALSE, TRUE,
                              read_time);
  }

  DBUG_RETURN(trp); 
}


/*
  This structure contains the info common for all steps of a partial
  index intersection plan. Morever it contains also the info common
  for index intersect plans. This info is filled in by the function
  prepare_search_best just before searching for the best index
  intersection plan.
*/  

typedef struct st_common_index_intersect_info
{
  PARAM *param;           /* context info for range optimizations            */
  uint key_size;          /* size of a ROWID element stored in Unique object */
  uint compare_factor;         /* 1/compare - cost to compare two ROWIDs     */
  ulonglong max_memory_size;   /* maximum space allowed for Unique objects   */   
  ha_rows table_cardinality;   /* estimate of the number of records in table */
  double cutoff_cost;        /* discard index intersects with greater costs  */ 
  INDEX_SCAN_INFO *cpk_scan;  /* clustered primary key used in intersection  */

  bool in_memory;  /* unique object for intersection is completely in memory */

  INDEX_SCAN_INFO **search_scans;    /* scans possibly included in intersect */ 
  uint n_search_scans;               /* number of elements in search_scans   */

  bool best_uses_cpk;   /* current best intersect uses clustered primary key */
  double best_cost;       /* cost of the current best index intersection     */
  /* estimate of the number of records in the current best intersection      */
  ha_rows best_records;
  uint best_length;    /* number of indexes in the current best intersection */
  INDEX_SCAN_INFO **best_intersect;  /* the current best index intersection  */
  /* scans from the best intersect to be filtrered by cpk conditions         */
  key_map filtered_scans; 

  uint *buff_elems;        /* buffer to calculate cost of index intersection */
  
} COMMON_INDEX_INTERSECT_INFO;


/*
  This structure contains the info specific for one step of an index
  intersection plan. The structure is filled in by the function 
   check_index_intersect_extension.
*/

typedef struct st_partial_index_intersect_info
{
  COMMON_INDEX_INTERSECT_INFO *common_info;    /* shared by index intersects */
  uint length;         /* number of index scans in the partial intersection  */
  ha_rows records;     /* estimate of the number of records in intersection  */
  double cost;         /* cost of the partial index intersection             */

  /* estimate of total number of records of all scans of the partial index
     intersect sent to the Unique object used for the intersection  */
  ha_rows records_sent_to_unique;

  /* total cost of the scans of indexes from the partial index intersection  */
  double index_read_cost; 

  bool use_cpk_filter;      /* cpk filter is to be used for this       scan  */  
  bool in_memory;            /* uses unique object in memory                 */
  double in_memory_cost;     /* cost of using unique object in memory        */

  key_map filtered_scans;    /* scans to be filtered by cpk conditions       */
         
  MY_BITMAP *intersect_fields;     /* bitmap of fields used in intersection  */
} PARTIAL_INDEX_INTERSECT_INFO;


/* Check whether two indexes have the same first n components */

static
bool same_index_prefix(KEY *key1, KEY *key2, uint used_parts)
{
  KEY_PART_INFO *part1= key1->key_part;
  KEY_PART_INFO *part2= key2->key_part;
  for(uint i= 0; i < used_parts; i++, part1++, part2++)
  {
    if (part1->fieldnr != part2->fieldnr)
      return FALSE;
  }
  return TRUE;
}


/* Create a bitmap for all fields of a table */

static
bool create_fields_bitmap(PARAM *param, MY_BITMAP *fields_bitmap)
{
  my_bitmap_map *bitmap_buf;

  if (!(bitmap_buf= (my_bitmap_map *) alloc_root(param->mem_root,
                                                 param->fields_bitmap_size)))
    return TRUE;
  if (bitmap_init(fields_bitmap, bitmap_buf, param->table->s->fields, FALSE))
    return TRUE;
  
  return FALSE;
}

/* Compare two indexes scans for sort before search for the best intersection */

static
int cmp_intersect_index_scan(INDEX_SCAN_INFO **a, INDEX_SCAN_INFO **b)
{
  return (*a)->records < (*b)->records ?
          -1 : (*a)->records == (*b)->records ? 0 : 1;
}


static inline
void set_field_bitmap_for_index_prefix(MY_BITMAP *field_bitmap,
                                       KEY_PART_INFO *key_part,
                                       uint used_key_parts)
{
  bitmap_clear_all(field_bitmap);
  for (KEY_PART_INFO *key_part_end= key_part+used_key_parts;
       key_part < key_part_end; key_part++)
  {
    bitmap_set_bit(field_bitmap, key_part->fieldnr-1);
  }
}


/*
  Round up table cardinality read from statistics provided by engine.
  This function should go away when mysql test will allow to handle
  more or less easily in the test suites deviations of InnoDB 
  statistical data.
*/
 
static inline
ha_rows get_table_cardinality_for_index_intersect(TABLE *table)
{
  if (table->file->ha_table_flags() & HA_STATS_RECORDS_IS_EXACT)
    return table->file->stats.records;
  else
  {
    ha_rows d;
    double q;
    for (q= (double)table->file->stats.records, d= 1 ; q >= 10; q/= 10, d*= 10 ) ;
    return (ha_rows) (floor(q+0.5) * d);
  } 
}

  
static
ha_rows records_in_index_intersect_extension(PARTIAL_INDEX_INTERSECT_INFO *curr,
                                             INDEX_SCAN_INFO *ext_index_scan);

/*
  Prepare to search for the best index intersection

  SYNOPSIS
    prepare_search_best_index_intersect()
      param         common info about index ranges
      tree          tree of ranges for indexes than can be intersected
      common    OUT info needed for search to be filled by the function 
      init      OUT info for an initial pseudo step of the intersection plans
      cutoff_cost   cut off cost of the interesting index intersection 

  DESCRIPTION
    The function initializes all fields of the structure 'common' to be used
    when searching for the best intersection plan. It also allocates
    memory to store the most cheap index intersection.

  NOTES
    When selecting candidates for index intersection we always take only
    one representative out of any set of indexes that share the same range
    conditions. These indexes always have the same prefixes and the
    components of this prefixes are exactly those used in these range
    conditions.
    Range conditions over clustered primary key (cpk) is always used only
    as the condition that filters out some rowids retrieved by the scans
    for secondary indexes. The cpk index will be handled in special way by
    the function that search for the best index intersection. 

  RETURN
    FALSE  in the case of success
    TRUE   otherwise
*/

static
bool prepare_search_best_index_intersect(PARAM *param, 
                                         SEL_TREE *tree,
                                         COMMON_INDEX_INTERSECT_INFO *common,
                                         PARTIAL_INDEX_INTERSECT_INFO *init,
                                         double cutoff_cost)
{
  uint i;
  uint n_search_scans;
  double cost;
  INDEX_SCAN_INFO **index_scan;
  INDEX_SCAN_INFO **scan_ptr;
  INDEX_SCAN_INFO *cpk_scan= NULL;
  TABLE *table= param->table;
  uint n_index_scans= tree->index_scans_end - tree->index_scans;

  if (!n_index_scans)
    return 1;

  bzero(init, sizeof(*init));
  init->common_info= common;
  init->cost= cutoff_cost;

  common->param= param;
  common->key_size= table->file->ref_length;
  common->compare_factor= TIME_FOR_COMPARE_ROWID;
  common->max_memory_size= param->thd->variables.sortbuff_size;
  common->cutoff_cost= cutoff_cost;
  common->cpk_scan= NULL;
  common->table_cardinality= 
    get_table_cardinality_for_index_intersect(table);

  if (n_index_scans <= 1)
    return TRUE;

  if (table->file->primary_key_is_clustered())
  {
    INDEX_SCAN_INFO **index_scan_end;
    index_scan= tree->index_scans;
    index_scan_end= index_scan+n_index_scans;
    for ( ; index_scan < index_scan_end; index_scan++)
    {  
      if ((*index_scan)->keynr == table->s->primary_key)
      {
        common->cpk_scan= cpk_scan= *index_scan;
        break;
      }
    }
  }

  i= n_index_scans - test(cpk_scan != NULL) + 1;

  if (!(common->search_scans =
	(INDEX_SCAN_INFO **) alloc_root (param->mem_root,
                                         sizeof(INDEX_SCAN_INFO *) * i)))
    return TRUE;
  bzero(common->search_scans, sizeof(INDEX_SCAN_INFO *) * i);

  INDEX_SCAN_INFO **selected_index_scans= common->search_scans;
    
  for (i=0, index_scan= tree->index_scans; i < n_index_scans; i++, index_scan++)
  {
    uint used_key_parts= (*index_scan)->used_key_parts;
    KEY *key_info= (*index_scan)->key_info;

    if (*index_scan == cpk_scan)
      continue;
    if (cpk_scan && cpk_scan->used_key_parts >= used_key_parts &&
        same_index_prefix(cpk_scan->key_info, key_info, used_key_parts))
      continue;

    cost= table->file->keyread_time((*index_scan)->keynr,
                                    (*index_scan)->range_count,
                                    (*index_scan)->records);
    if (cost >= cutoff_cost)
      continue;
   
    for (scan_ptr= selected_index_scans; *scan_ptr ; scan_ptr++)
    {
      /*
        When we have range conditions for two different indexes with the same
        beginning it does not make sense to consider both of them for index 
        intersection if the range conditions are covered by common initial
        components of the indexes. Actually in this case the indexes are
        guaranteed to have the same range conditions.
      */
      if ((*scan_ptr)->used_key_parts == used_key_parts &&
          same_index_prefix((*scan_ptr)->key_info, key_info, used_key_parts))
        break;
    }
    if (!*scan_ptr || cost < (*scan_ptr)->index_read_cost)
    {
      *scan_ptr= *index_scan;
      (*scan_ptr)->index_read_cost= cost;
    }
  } 

  ha_rows records_in_scans= 0;

  for (scan_ptr=selected_index_scans, i= 0; *scan_ptr; scan_ptr++, i++)
  {
    if (create_fields_bitmap(param, &(*scan_ptr)->used_fields))
      return TRUE;
    records_in_scans+= (*scan_ptr)->records;
  }
  n_search_scans= i;

  if (cpk_scan && create_fields_bitmap(param, &cpk_scan->used_fields))
    return TRUE;
  
  if (!(common->n_search_scans= n_search_scans))
    return TRUE;
    
  common->best_uses_cpk= FALSE;
  common->best_cost= cutoff_cost + COST_EPS;
  common->best_length= 0;

  if (!(common->best_intersect=
	(INDEX_SCAN_INFO **) alloc_root (param->mem_root,
                                         sizeof(INDEX_SCAN_INFO *) *
                                         (i + test(cpk_scan != NULL)))))
    return TRUE;

  size_t calc_cost_buff_size=
         Unique::get_cost_calc_buff_size((size_t)records_in_scans,
                                         common->key_size,
				         common->max_memory_size);
  if (!(common->buff_elems= (uint *) alloc_root(param->mem_root,
                                                calc_cost_buff_size)))
    return TRUE;

  my_qsort(selected_index_scans, n_search_scans, sizeof(INDEX_SCAN_INFO *),
           (qsort_cmp) cmp_intersect_index_scan);

  if (cpk_scan)
  {
    PARTIAL_INDEX_INTERSECT_INFO curr;
    set_field_bitmap_for_index_prefix(&cpk_scan->used_fields,
                                      cpk_scan->key_info->key_part,
                                      cpk_scan->used_key_parts);
    curr.common_info= common;
    curr.intersect_fields= &cpk_scan->used_fields;
    curr.records= cpk_scan->records;
    curr.length= 1;
    for (scan_ptr=selected_index_scans; *scan_ptr; scan_ptr++)
    {
      ha_rows scan_records= (*scan_ptr)->records;
      ha_rows records= records_in_index_intersect_extension(&curr, *scan_ptr);
      (*scan_ptr)->filtered_out= records >= scan_records ?
                                   0 : scan_records-records; 
    }
  } 
  else
  {
    for (scan_ptr=selected_index_scans; *scan_ptr; scan_ptr++)
      (*scan_ptr)->filtered_out= 0;
  }

  return FALSE;
}


/*
  On Estimation of the Number of Records in an Index Intersection 
  ===============================================================

  Consider query Q over table t. Let C be the WHERE condition of  this query,
  and, idx1(a1_1,...,a1_k1) and idx2(a2_1,...,a2_k2) be some indexes defined
  on table t.
  Let rt1 and rt2 be the range trees extracted by the range optimizer from C
  for idx1 and idx2 respectively.
  Let #t be the estimate of the number of records in table t provided for the
  optimizer. 
  Let #r1 and #r2 be the estimates of the number of records in the range trees
  rt1 and rt2, respectively, obtained by the range optimizer.

  We need to get an estimate for the number of records in the index 
  intersection of rt1 and rt2. In other words, we need to estimate the
  cardinality of the set of records that are in both trees. Let's designate
  this number by #r.

  If we do not make any assumptions then we can only state that
     #r<=min(#r1,#r2).
  With this estimate we can't say that the index intersection scan will be 
  cheaper than the cheapest index scan.

  Let Rt1 and Rt2 be AND/OR conditions representing rt and rt2 respectively.
  The probability that a record belongs to rt1 is sel(Rt1)=#r1/#t.
  The probability that a record belongs to rt2 is sel(Rt2)=#r2/#t.

  If we assume that the values in columns of idx1 and idx2 are independent
  then #r/#t=sel(Rt1&Rt2)=sel(Rt1)*sel(Rt2)=(#r1/#t)*(#r2/#t).
  So in this case we have: #r=#r1*#r2/#t.

  The above assumption of independence of the columns in idx1 and idx2 means
  that:
  - all columns are different
  - values from one column do not correlate with values from any other column.

  We can't help with the case when column correlate with each other.
  Yet, if they are assumed to be uncorrelated the value of #r theoretically can
  be evaluated . Unfortunately this evaluation, in general, is rather complex.

  Let's consider two indexes idx1:(dept, manager),  idx2:(dept, building)
  over table 'employee' and two range conditions over these indexes:
    Rt1: dept=10 AND manager LIKE 'S%'
    Rt2: dept=10 AND building LIKE 'L%'.
  We can state that:
    sel(Rt1&Rt2)=sel(dept=10)*sel(manager LIKE 'S%')*sel(building LIKE 'L%')
    =sel(Rt1)*sel(Rt2)/sel(dept=10).
  sel(Rt1/2_0:dept=10) can be estimated if we know the cardinality #r1_0 of
  the range for sub-index idx1_0 (dept) of the index idx1 or the cardinality
  #rt2_0 of the same range for sub-index idx2_0(dept) of the index idx2.
  The current code does not make an estimate either for #rt1_0, or for #rt2_0,
  but it can be adjusted to provide those numbers.
  Alternatively, min(rec_per_key) for (dept) could be used to get an upper 
  bound for the value of sel(Rt1&Rt2). Yet this statistics is not provided
  now.  
 
  Let's consider two other indexes idx1:(dept, last_name), 
  idx2:(first_name, last_name) and two range conditions over these indexes:
    Rt1: dept=5 AND last_name='Sm%'
    Rt2: first_name='Robert' AND last_name='Sm%'.

  sel(Rt1&Rt2)=sel(dept=5)*sel(last_name='Sm5')*sel(first_name='Robert')
  =sel(Rt2)*sel(dept=5)
  Here max(rec_per_key) for (dept) could be used to get an upper bound for
  the value of sel(Rt1&Rt2).
  
  When the intersected indexes have different major columns, but some
  minor column are common the picture may be more complicated.

  Let's consider the following range conditions for the same indexes as in
  the previous example:
    Rt1: (Rt11: dept=5 AND last_name='So%') 
         OR 
         (Rt12: dept=7 AND last_name='Saw%')
    Rt2: (Rt21: first_name='Robert' AND last_name='Saw%')
         OR
         (Rt22: first_name='Bob' AND last_name='So%')
  Here we have:
  sel(Rt1&Rt2)= sel(Rt11)*sel(Rt21)+sel(Rt22)*sel(dept=5) +
                sel(Rt21)*sel(dept=7)+sel(Rt12)*sel(Rt22)
  Now consider the range condition:
    Rt1_0: (dept=5 OR dept=7)
  For this condition we can state that:
  sel(Rt1_0&Rt2)=(sel(dept=5)+sel(dept=7))*(sel(Rt21)+sel(Rt22))=
  sel(dept=5)*sel(Rt21)+sel(dept=7)*sel(Rt21)+
  sel(dept=5)*sel(Rt22)+sel(dept=7)*sel(Rt22)=
  sel(dept=5)*sel(Rt21)+sel(Rt21)*sel(dept=7)+
  sel(Rt22)*sel(dept=5)+sel(dept=7)*sel(Rt22) >
  sel(Rt11)*sel(Rt21)+sel(Rt22)*sel(dept=5)+
  sel(Rt21)*sel(dept=7)+sel(Rt12)*sel(Rt22) >
  sel(Rt1 & Rt2) 

 We've just demonstrated for an example what is intuitively almost obvious
 in general. We can  remove the ending parts fromrange trees getting less
 selective range conditions for sub-indexes.
 So if not a most major component with the number k of an index idx is
 encountered in the index with which we intersect we can use the sub-index
 idx_k-1 that includes the components of idx up to the i-th component and
 the range tree for idx_k-1 to make an upper bound estimate for the number
  of records in the index intersection.
 The range tree for idx_k-1 we use here is the subtree of the original range
  tree for idx that contains only parts from the first k-1 components.

  As it was mentioned above the range optimizer currently does not provide
  an estimate for the number of records in the ranges for sub-indexes.
  However, some reasonable upper bound estimate can be obtained.

  Let's consider the following range tree:
    Rt: (first_name='Robert' AND last_name='Saw%')
        OR
        (first_name='Bob' AND last_name='So%')
  Let #r be the number of records in Rt. Let f_1 be the fan-out of column
  last_name:
    f_1 = rec_per_key[first_name]/rec_per_key[last_name].
  The the number of records in the range tree:
    Rt_0:  (first_name='Robert' OR first_name='Bob')
  for the sub-index (first_name) is not greater than max(#r*f_1, #t).
  Strictly speaking, we can state only that it's not greater than 
  max(#r*max_f_1, #t), where
    max_f_1= max_rec_per_key[first_name]/min_rec_per_key[last_name].
  Yet, if #r/#t is big enough (and this is the case of an index intersection,
  because using this index range with a single index scan is cheaper than
  the cost of the intersection when #r/#t is small) then almost safely we
  can use here f_1 instead of max_f_1.

  The above considerations can be used in future development. Now, they are
  used partly in the function that provides a rough upper bound estimate for
  the number of records in an index intersection that follow below.
*/

/*
  Estimate the number of records selected by an extension a partial intersection

  SYNOPSIS
    records_in_index_intersect_extension()
     curr            partial intersection plan to be extended
     ext_index_scan  the evaluated extension of this partial plan

  DESCRIPTION
    The function provides an estimate for the number of records in the
    intersection of the partial index intersection curr with the index
    ext_index_scan. If all intersected indexes does not have common columns
    then  the function returns an exact estimate (assuming there are no
    correlations between values in the columns). If the intersected indexes
    have common  columns the function returns an upper bound for the number
    of records in the intersection provided that the intersection of curr
    with ext_index_scan can is expected to have less records than the expected
    number of records in the partial intersection curr. In this case the
    function also assigns the bitmap of the columns in the extended 
    intersection to ext_index_scan->used_fields.
    If the function cannot expect that the number of records in the extended
    intersection is less that the expected number of records #r in curr then
    the function returns a number bigger than #r.

  NOTES
   See the comment before the desription of the function that explains the
   reasoning used  by this function.
    
  RETURN
    The expected number of rows in the extended index intersection
*/

static
ha_rows records_in_index_intersect_extension(PARTIAL_INDEX_INTERSECT_INFO *curr,
                                             INDEX_SCAN_INFO *ext_index_scan)
{
  KEY *key_info= ext_index_scan->key_info;
  KEY_PART_INFO* key_part= key_info->key_part;
  uint used_key_parts= ext_index_scan->used_key_parts;
  MY_BITMAP *used_fields= &ext_index_scan->used_fields;
  
  if (!curr->length)
  {
    /* 
      If this the first index in the intersection just mark the
      fields in the used_fields bitmap and return the expected
      number of records in the range scan for the index provided
      by the range optimizer.
    */ 
    set_field_bitmap_for_index_prefix(used_fields, key_part, used_key_parts);
    return ext_index_scan->records;
  }

  uint i;
  bool better_selectivity= FALSE;
  ha_rows records= curr->records;
  MY_BITMAP *curr_intersect_fields= curr->intersect_fields; 
  for (i= 0; i < used_key_parts; i++, key_part++)
  {
    if (bitmap_is_set(curr_intersect_fields, key_part->fieldnr-1))
      break;
  }
  if (i)
  {
    ha_rows table_cardinality= curr->common_info->table_cardinality;
    ha_rows ext_records= ext_index_scan->records;
    if (i < used_key_parts)
    {
      ulong *rec_per_key= key_info->rec_per_key+i-1;
      ulong f1= rec_per_key[0] ? rec_per_key[0] : 1;
      ulong f2= rec_per_key[1] ? rec_per_key[1] : 1;
      ext_records= (ha_rows) ((double) ext_records / f2 * f1);
    }
    if (ext_records < table_cardinality)
    {
      better_selectivity= TRUE;
      records= (ha_rows) ((double) records / table_cardinality *
			  ext_records);
      bitmap_copy(used_fields, curr_intersect_fields);
      key_part= key_info->key_part;
      for (uint j= 0; j < used_key_parts; j++, key_part++)
        bitmap_set_bit(used_fields, key_part->fieldnr-1);
    }
  }
  return !better_selectivity ? records+1 :
                               !records ? 1 : records;
}


/* 
  Estimate the cost a binary search within disjoint cpk range intervals

  Number of comparisons to check whether a cpk value satisfies
  the cpk range condition = log2(cpk_scan->range_count).
*/ 

static inline
double get_cpk_filter_cost(ha_rows filtered_records, 
                           INDEX_SCAN_INFO *cpk_scan,
                           double compare_factor)
{
  return log((double) (cpk_scan->range_count+1)) / (compare_factor * M_LN2) *
           filtered_records;
}


/*
  Check whether a patial index intersection plan can be extended 

  SYNOPSIS
    check_index_intersect_extension()
     curr            partial intersection plan to be extended
     ext_index_scan  a possible extension of this plan to be checked
     next       OUT  the structure to be filled for the extended plan 

  DESCRIPTION
    The function checks whether it makes sense to extend the index
    intersection plan adding the index ext_index_scan, and, if this
    the case, the function fills in the structure for the extended plan.

  RETURN
    TRUE      if it makes sense to extend the given plan 
    FALSE     otherwise
*/

static
bool check_index_intersect_extension(PARTIAL_INDEX_INTERSECT_INFO *curr,
                                     INDEX_SCAN_INFO *ext_index_scan,
                                     PARTIAL_INDEX_INTERSECT_INFO *next)
{
  ha_rows records;
  ha_rows records_sent_to_unique;
  double cost;
  ha_rows ext_index_scan_records= ext_index_scan->records;
  ha_rows records_filtered_out_by_cpk= ext_index_scan->filtered_out;
  COMMON_INDEX_INTERSECT_INFO *common_info= curr->common_info;
  double cutoff_cost= common_info->cutoff_cost;
  uint idx= curr->length;
  next->index_read_cost= curr->index_read_cost+ext_index_scan->index_read_cost;
  if (next->index_read_cost > cutoff_cost)
    return FALSE; 

  if ((next->in_memory= curr->in_memory))
    next->in_memory_cost= curr->in_memory_cost;

  next->intersect_fields= &ext_index_scan->used_fields;
  next->filtered_scans= curr->filtered_scans;

  records_sent_to_unique= curr->records_sent_to_unique;

  next->use_cpk_filter= FALSE;

  /* Calculate the cost of using a Unique object for index intersection */
  if (idx && next->in_memory)
  { 
    /* 
      All rowids received from the first scan are expected in one unique tree
    */
    ha_rows elems_in_tree= common_info->search_scans[0]->records-
                           common_info->search_scans[0]->filtered_out ;
    next->in_memory_cost+= Unique::get_search_cost(elems_in_tree,
                                                   common_info->compare_factor)* 
                             ext_index_scan_records;
    cost= next->in_memory_cost;
  }
  else
  {
    uint *buff_elems= common_info->buff_elems;
    uint key_size= common_info->key_size;
    uint compare_factor= common_info->compare_factor;         
    ulonglong max_memory_size= common_info->max_memory_size; 
    
    records_sent_to_unique+= ext_index_scan_records;
    cost= Unique::get_use_cost(buff_elems, (size_t) records_sent_to_unique, key_size,
                               max_memory_size, compare_factor, TRUE,
                               &next->in_memory);
    if (records_filtered_out_by_cpk)
    {
      /* Check whether using cpk filter for this scan is beneficial */

      double cost2;
      bool in_memory2;
      ha_rows records2= records_sent_to_unique-records_filtered_out_by_cpk;
      cost2=  Unique::get_use_cost(buff_elems, (size_t) records2, key_size,
                                   max_memory_size, compare_factor, TRUE,
                                   &in_memory2);
      cost2+= get_cpk_filter_cost(ext_index_scan_records, common_info->cpk_scan,
                                  compare_factor);
      if (cost > cost2 + COST_EPS)
      {
        cost= cost2;
        next->in_memory= in_memory2;
        next->use_cpk_filter= TRUE;
        records_sent_to_unique= records2;
      }

    }   
    if (next->in_memory)
      next->in_memory_cost= cost;
  }

  if (next->use_cpk_filter)
  {
    next->filtered_scans.set_bit(ext_index_scan->keynr);
    bitmap_union(&ext_index_scan->used_fields,
                 &common_info->cpk_scan->used_fields);
  }
  next->records_sent_to_unique= records_sent_to_unique;
       
  records= records_in_index_intersect_extension(curr, ext_index_scan);
  if (idx && records > curr->records)
    return FALSE;
  if (next->use_cpk_filter && curr->filtered_scans.is_clear_all())
    records-= records_filtered_out_by_cpk;
  next->records= records;

  cost+= next->index_read_cost;
  if (cost >= cutoff_cost)
    return FALSE;

  cost+= get_sweep_read_cost(common_info->param, records);

  next->cost= cost;
  next->length= curr->length+1;

  return TRUE;
}


/*
  Search for the cheapest extensions of range scans used to access a table    

  SYNOPSIS
    find_index_intersect_best_extension()
      curr        partial intersection to evaluate all possible extension for 

  DESCRIPTION
    The function tries to extend the partial plan curr in all possible ways
    to look for a cheapest index intersection whose cost less than the 
    cut off value set in curr->common_info.cutoff_cost. 
*/

static 
void find_index_intersect_best_extension(PARTIAL_INDEX_INTERSECT_INFO *curr)
{
  PARTIAL_INDEX_INTERSECT_INFO next;
  COMMON_INDEX_INTERSECT_INFO *common_info= curr->common_info;
  INDEX_SCAN_INFO **index_scans= common_info->search_scans;
  uint idx= curr->length;
  INDEX_SCAN_INFO **rem_first_index_scan_ptr= &index_scans[idx];
  double cost= curr->cost;

  if (cost + COST_EPS < common_info->best_cost)
  {
    common_info->best_cost= cost;
    common_info->best_length= curr->length;
    common_info->best_records= curr->records;
    common_info->filtered_scans= curr->filtered_scans;
    /* common_info->best_uses_cpk <=> at least one scan uses a cpk filter */
    common_info->best_uses_cpk= !curr->filtered_scans.is_clear_all();
    uint sz= sizeof(INDEX_SCAN_INFO *) * curr->length;
    memcpy(common_info->best_intersect, common_info->search_scans, sz);
    common_info->cutoff_cost= cost;
  }   

  if (!(*rem_first_index_scan_ptr))
    return;  

  next.common_info= common_info;
 
  INDEX_SCAN_INFO *rem_first_index_scan= *rem_first_index_scan_ptr;
  for (INDEX_SCAN_INFO **index_scan_ptr= rem_first_index_scan_ptr;
       *index_scan_ptr; index_scan_ptr++)
  {
    *rem_first_index_scan_ptr= *index_scan_ptr;
    *index_scan_ptr= rem_first_index_scan;
    if (check_index_intersect_extension(curr, *rem_first_index_scan_ptr, &next))
      find_index_intersect_best_extension(&next);
    *index_scan_ptr= *rem_first_index_scan_ptr;
    *rem_first_index_scan_ptr= rem_first_index_scan;
  }
}


/*
  Get the plan of the best intersection of range scans used to access a table    

  SYNOPSIS
    get_best_index_intersect()
      param         common info about index ranges
      tree          tree of ranges for indexes than can be intersected
      read_time     cut off value for the evaluated plans 

  DESCRIPTION
    The function looks for the cheapest index intersection of the range
    scans to access a table. The info about the ranges for all indexes
    is provided by the range optimizer and is passed through the
    parameters param and tree. Any plan whose cost is greater than read_time
    is rejected. 
    After the best index intersection is found the function constructs
    the structure that manages the execution by the chosen plan.

  RETURN
    Pointer to the generated execution structure if a success,
    0 - otherwise.
*/

static
TRP_INDEX_INTERSECT *get_best_index_intersect(PARAM *param, SEL_TREE *tree,
                                              double read_time)
{
  uint i;
  uint count;
  TRP_RANGE **cur_range;
  TRP_RANGE **range_scans;
  INDEX_SCAN_INFO *index_scan;
  COMMON_INDEX_INTERSECT_INFO common;
  PARTIAL_INDEX_INTERSECT_INFO init;
  TRP_INDEX_INTERSECT *intersect_trp= NULL;
  TABLE *table= param->table;
  
  
  DBUG_ENTER("get_best_index_intersect");

  if (prepare_search_best_index_intersect(param, tree, &common, &init,
                                          read_time))
    DBUG_RETURN(NULL);

  find_index_intersect_best_extension(&init);

  if (common.best_length <= 1 && !common.best_uses_cpk)
    DBUG_RETURN(NULL);

  if (common.best_uses_cpk)
  {
    memmove((char *) (common.best_intersect+1), (char *) common.best_intersect,
            sizeof(INDEX_SCAN_INFO *) * common.best_length);
    common.best_intersect[0]= common.cpk_scan;
    common.best_length++;
  }

  count= common.best_length;

  if (!(range_scans= (TRP_RANGE**)alloc_root(param->mem_root,
                                            sizeof(TRP_RANGE *)*
                                            count)))
    DBUG_RETURN(NULL);

  for (i= 0, cur_range= range_scans; i < count; i++)
  {
    index_scan= common.best_intersect[i];
    if ((*cur_range= new (param->mem_root) TRP_RANGE(index_scan->sel_arg,
                                                     index_scan->idx, 0)))
    {  
      TRP_RANGE *trp= *cur_range;  
      trp->read_cost= index_scan->index_read_cost;  
      trp->records= index_scan->records;        
      trp->is_ror= FALSE;
      trp->mrr_buf_size= 0;
      table->intersect_keys.set_bit(index_scan->keynr);
      cur_range++;
    }
  }
  
  count= tree->index_scans_end - tree->index_scans;
  for (i= 0; i < count; i++)
  {
    index_scan= tree->index_scans[i]; 
    if (!table->intersect_keys.is_set(index_scan->keynr))
    {
      for (uint j= 0; j < common.best_length; j++)
      {
	INDEX_SCAN_INFO *scan= common.best_intersect[j];
        if (same_index_prefix(index_scan->key_info, scan->key_info,
                              scan->used_key_parts))
	{
          table->intersect_keys.set_bit(index_scan->keynr);
          break;
        } 
      }
    }
  }
      
  if ((intersect_trp= new (param->mem_root)TRP_INDEX_INTERSECT))
  {
    intersect_trp->read_cost= common.best_cost;
    intersect_trp->records= common.best_records;
    intersect_trp->range_scans= range_scans;
    intersect_trp->range_scans_end= cur_range;
    intersect_trp->filtered_scans= common.filtered_scans;
  }
  DBUG_RETURN(intersect_trp);
}


typedef struct st_ror_scan_info : INDEX_SCAN_INFO
{ 
} ROR_SCAN_INFO;


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
  my_bitmap_map *bitmap_buf;
  uint keynr;
  DBUG_ENTER("make_ror_scan");

  if (!(ror_scan= (ROR_SCAN_INFO*)alloc_root(param->mem_root,
                                             sizeof(ROR_SCAN_INFO))))
    DBUG_RETURN(NULL);

  ror_scan->idx= idx;
  ror_scan->keynr= keynr= param->real_keynr[idx];
  ror_scan->key_rec_length= (param->table->key_info[keynr].key_length +
                             param->table->file->ref_length);
  ror_scan->sel_arg= sel_arg;
  ror_scan->records= param->quick_rows[keynr];

  if (!(bitmap_buf= (my_bitmap_map*) alloc_root(param->mem_root,
                                                param->fields_bitmap_size)))
    DBUG_RETURN(NULL);

  if (bitmap_init(&ror_scan->covered_fields, bitmap_buf,
                  param->table->s->fields, FALSE))
    DBUG_RETURN(NULL);
  bitmap_clear_all(&ror_scan->covered_fields);

  KEY_PART_INFO *key_part= param->table->key_info[keynr].key_part;
  KEY_PART_INFO *key_part_end= key_part +
                               param->table->key_info[keynr].key_parts;
  for (;key_part != key_part_end; ++key_part)
  {
    if (bitmap_is_set(&param->needed_fields, key_part->fieldnr-1))
      bitmap_set_bit(&ror_scan->covered_fields, key_part->fieldnr-1);
  }
  ror_scan->index_read_cost=
    param->table->file->keyread_time(ror_scan->keynr, 1, ror_scan->records);
  DBUG_RETURN(ror_scan);
}


/*
  Compare two ROR_SCAN_INFO** by  E(#records_matched) * key_record_length.
  SYNOPSIS
    cmp_ror_scan_info()
      a ptr to first compared value
      b ptr to second compared value

  RETURN
   -1 a < b
    0 a = b
    1 a > b
*/

static int cmp_ror_scan_info(ROR_SCAN_INFO** a, ROR_SCAN_INFO** b)
{
  double val1= rows2double((*a)->records) * (*a)->key_rec_length;
  double val2= rows2double((*b)->records) * (*b)->key_rec_length;
  return (val1 < val2)? -1: (val1 == val2)? 0 : 1;
}

/*
  Compare two ROR_SCAN_INFO** by
   (#covered fields in F desc,
    #components asc,
    number of first not covered component asc)

  SYNOPSIS
    cmp_ror_scan_info_covering()
      a ptr to first compared value
      b ptr to second compared value

  RETURN
   -1 a < b
    0 a = b
    1 a > b
*/

static int cmp_ror_scan_info_covering(ROR_SCAN_INFO** a, ROR_SCAN_INFO** b)
{
  if ((*a)->used_fields_covered > (*b)->used_fields_covered)
    return -1;
  if ((*a)->used_fields_covered < (*b)->used_fields_covered)
    return 1;
  if ((*a)->key_components < (*b)->key_components)
    return -1;
  if ((*a)->key_components > (*b)->key_components)
    return 1;
  if ((*a)->first_uncovered_field < (*b)->first_uncovered_field)
    return -1;
  if ((*a)->first_uncovered_field > (*b)->first_uncovered_field)
    return 1;
  return 0;
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
  double index_scan_costs; /* SUM(cost of 'index-only' scans) */
  double total_cost;
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
  info->index_scan_costs= 0.0;
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
  dst->index_scan_costs= src->index_scan_costs;
  dst->total_cost= src->total_cost;
}


/*
  Get selectivity of a ROR scan wrt ROR-intersection.

  SYNOPSIS
    ror_scan_selectivity()
      info  ROR-interection 
      scan  ROR scan
      
  NOTES
    Suppose we have a condition on several keys
    cond=k_11=c_11 AND k_12=c_12 AND ...  // parts of first key
         k_21=c_21 AND k_22=c_22 AND ...  // parts of second key
          ...
         k_n1=c_n1 AND k_n3=c_n3 AND ...  (1) //parts of the key used by *scan

    where k_ij may be the same as any k_pq (i.e. keys may have common parts).

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

    In order to minimize number of expensive records_in_range calls we group
    and reduce adjacent fractions.

  RETURN
    Selectivity of given ROR scan.
*/

static double ror_scan_selectivity(const ROR_INTERSECT_INFO *info, 
                                   const ROR_SCAN_INFO *scan)
{
  double selectivity_mult= 1.0;
  KEY_PART_INFO *key_part= info->param->table->key_info[scan->keynr].key_part;
  uchar key_val[MAX_KEY_LENGTH+MAX_FIELD_WIDTH]; /* key values tuple */
  uchar *key_ptr= key_val;
  SEL_ARG *sel_arg, *tuple_arg= NULL;
  key_part_map keypart_map= 0;
  bool cur_covered;
  bool prev_covered= test(bitmap_is_set(&info->covered_fields,
                                        key_part->fieldnr-1));
  key_range min_range;
  key_range max_range;
  min_range.key= key_val;
  min_range.flag= HA_READ_KEY_EXACT;
  max_range.key= key_val;
  max_range.flag= HA_READ_AFTER_KEY;
  ha_rows prev_records= info->param->table->file->stats.records;
  DBUG_ENTER("ror_scan_selectivity");

  for (sel_arg= scan->sel_arg; sel_arg;
       sel_arg= sel_arg->next_key_part)
  {
    DBUG_PRINT("info",("sel_arg step"));
    cur_covered= test(bitmap_is_set(&info->covered_fields,
                                    key_part[sel_arg->part].fieldnr-1));
    if (cur_covered != prev_covered)
    {
      /* create (part1val, ..., part{n-1}val) tuple. */
      ha_rows records;
      if (!tuple_arg)
      {
        tuple_arg= scan->sel_arg;
        /* Here we use the length of the first key part */
        tuple_arg->store_min(key_part->store_length, &key_ptr, 0);
        keypart_map= 1;
      }
      while (tuple_arg->next_key_part != sel_arg)
      {
        tuple_arg= tuple_arg->next_key_part;
        tuple_arg->store_min(key_part[tuple_arg->part].store_length,
                             &key_ptr, 0);
        keypart_map= (keypart_map << 1) | 1;
      }
      min_range.length= max_range.length= (size_t) (key_ptr - key_val);
      min_range.keypart_map= max_range.keypart_map= keypart_map;
      records= (info->param->table->file->
                records_in_range(scan->keynr, &min_range, &max_range));
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
    double tmp= rows2double(info->param->quick_rows[scan->keynr]) /
                rows2double(prev_records);
    DBUG_PRINT("info", ("Selectivity multiplier: %g", tmp));
    selectivity_mult *= tmp;
  }
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
                              ROR_SCAN_INFO* ror_scan, bool is_cpk_scan)
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
      CPK scan is used to filter out rows. We apply filtering for 
      each record of every scan. Assuming 1/TIME_FOR_COMPARE_ROWID
      per check this gives us:
    */
    info->index_scan_costs += rows2double(info->index_records) / 
                              TIME_FOR_COMPARE_ROWID;
  }
  else
  {
    info->index_records += info->param->quick_rows[ror_scan->keynr];
    info->index_scan_costs += ror_scan->index_read_cost;
    bitmap_union(&info->covered_fields, &ror_scan->covered_fields);
    if (!info->is_covering && bitmap_is_subset(&info->param->needed_fields,
                                               &info->covered_fields))
    {
      DBUG_PRINT("info", ("ROR-intersect is covering now"));
      info->is_covering= TRUE;
    }
  }

  info->total_cost= info->index_scan_costs;
  DBUG_PRINT("info", ("info->total_cost: %g", info->total_cost));
  if (!info->is_covering)
  {
    info->total_cost += 
      get_sweep_read_cost(info->param, double2rows(info->out_rows));
    DBUG_PRINT("info", ("info->total_cost= %g", info->total_cost));
  }
  DBUG_PRINT("info", ("New out_rows: %g", info->out_rows));
  DBUG_PRINT("info", ("New cost: %g, %scovering", info->total_cost,
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
      read_time        Do not return read plans with cost > read_time.
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
                                          double read_time,
                                          bool *are_all_covering)
{
  uint idx;
  double min_cost= DBL_MAX;
  DBUG_ENTER("get_best_ror_intersect");

  if ((tree->n_ror_scans < 2) || !param->table->file->stats.records ||
      !optimizer_flag(param->thd, OPTIMIZER_SWITCH_INDEX_MERGE_INTERSECT))
    DBUG_RETURN(NULL);

  /*
    Step1: Collect ROR-able SEL_ARGs and create ROR_SCAN_INFO for each of 
    them. Also find and save clustered PK scan if there is one.
  */
  ROR_SCAN_INFO **cur_ror_scan;
  ROR_SCAN_INFO *cpk_scan= NULL;
  uint cpk_no;

  if (!(tree->ror_scans= (ROR_SCAN_INFO**)alloc_root(param->mem_root,
                                                     sizeof(ROR_SCAN_INFO*)*
                                                     param->keys)))
    return NULL;
  cpk_no= ((param->table->file->primary_key_is_clustered()) ?
           param->table->s->primary_key : MAX_KEY);

  for (idx= 0, cur_ror_scan= tree->ror_scans; idx < param->keys; idx++)
  {
    ROR_SCAN_INFO *scan;
    uint key_no;
    if (!tree->ror_scans_map.is_set(idx))
      continue;
    key_no= param->real_keynr[idx];
    if (key_no != cpk_no &&
        param->table->file->index_flags(key_no,0,0) & HA_CLUSTERED_INDEX)
    {
      /* Ignore clustering keys */
      tree->n_ror_scans--;
      continue;
    }
    if (!(scan= make_ror_scan(param, idx, tree->keys[idx])))
      return NULL;
    if (key_no == cpk_no)
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
  my_qsort(tree->ror_scans, tree->n_ror_scans, sizeof(ROR_SCAN_INFO*),
           (qsort_cmp)cmp_ror_scan_info);
  DBUG_EXECUTE("info",print_ror_scans_arr(param->table, "ordered",
                                          tree->ror_scans,
                                          tree->ror_scans_end););

  ROR_SCAN_INFO **intersect_scans; /* ROR scans used in index intersection */
  ROR_SCAN_INFO **intersect_scans_end;
  if (!(intersect_scans= (ROR_SCAN_INFO**)alloc_root(param->mem_root,
                                                     sizeof(ROR_SCAN_INFO*)*
                                                     tree->n_ror_scans)))
    return NULL;
  intersect_scans_end= intersect_scans;

  /* Create and incrementally update ROR intersection. */
  ROR_INTERSECT_INFO *intersect, *intersect_best;
  if (!(intersect= ror_intersect_init(param)) || 
      !(intersect_best= ror_intersect_init(param)))
    return NULL;

  /* [intersect_scans,intersect_scans_best) will hold the best intersection */
  ROR_SCAN_INFO **intersect_scans_best;
  cur_ror_scan= tree->ror_scans;
  intersect_scans_best= intersect_scans;
  while (cur_ror_scan != tree->ror_scans_end && !intersect->is_covering)
  {
    /* S= S + first(R);  R= R - first(R); */
    if (!ror_intersect_add(intersect, *cur_ror_scan, FALSE))
    {
      cur_ror_scan++;
      continue;
    }
    
    *(intersect_scans_end++)= *(cur_ror_scan++);

    if (intersect->total_cost < min_cost)
    {
      /* Local minimum found, save it */
      ror_intersect_cpy(intersect_best, intersect);
      intersect_scans_best= intersect_scans_end;
      min_cost = intersect->total_cost;
    }
  }

  if (intersect_scans_best == intersect_scans)
  {
    DBUG_PRINT("info", ("None of scans increase selectivity"));
    DBUG_RETURN(NULL);
  }
    
  DBUG_EXECUTE("info",print_ror_scans_arr(param->table,
                                          "best ROR-intersection",
                                          intersect_scans,
                                          intersect_scans_best););

  *are_all_covering= intersect->is_covering;
  uint best_num= intersect_scans_best - intersect_scans;
  ror_intersect_cpy(intersect, intersect_best);

  /*
    Ok, found the best ROR-intersection of non-CPK key scans.
    Check if we should add a CPK scan. If the obtained ROR-intersection is 
    covering, it doesn't make sense to add CPK scan.
  */
  if (cpk_scan && !intersect->is_covering)
  {
    if (ror_intersect_add(intersect, cpk_scan, TRUE) && 
        (intersect->total_cost < min_cost))
      intersect_best= intersect; //just set pointer here
  }
  else
    cpk_scan= 0;                                // Don't use cpk_scan

  /* Ok, return ROR-intersect plan if we have found one */
  TRP_ROR_INTERSECT *trp= NULL;
  if (min_cost < read_time && (cpk_scan || best_num > 1))
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
    trp->read_cost= intersect_best->total_cost;
    /* Prevent divisons by zero */
    ha_rows best_rows = double2rows(intersect_best->out_rows);
    if (!best_rows)
      best_rows= 1;
    set_if_smaller(param->table->quick_condition_rows, best_rows);
    trp->records= best_rows;
    trp->index_scan_costs= intersect_best->index_scan_costs;
    trp->cpk_scan= cpk_scan;
    DBUG_PRINT("info", ("Returning non-covering ROR-intersect plan:"
                        "cost %g, records %lu",
                        trp->read_cost, (ulong) trp->records));
  }
  DBUG_RETURN(trp);
}


/*
  Get best covering ROR-intersection.
  SYNOPSIS
    get_best_ntersectcovering_ror_intersect()
      param     Parameter from test_quick_select function.
      tree      SEL_TREE with sets of intervals for different keys.
      read_time Don't return table read plans with cost > read_time.

  RETURN
    Best covering ROR-intersection plan
    NULL if no plan found.

  NOTES
    get_best_ror_intersect must be called for a tree before calling this
    function for it.
    This function invalidates tree->ror_scans member values.

  The following approximate algorithm is used:
    I=set of all covering indexes
    F=set of all fields to cover
    S={}

    do
    {
      Order I by (#covered fields in F desc,
                  #components asc,
                  number of first not covered component asc);
      F=F-covered by first(I);
      S=S+first(I);
      I=I-first(I);
    } while F is not empty.
*/

static
TRP_ROR_INTERSECT *get_best_covering_ror_intersect(PARAM *param,
                                                   SEL_TREE *tree,
                                                   double read_time)
{
  ROR_SCAN_INFO **ror_scan_mark;
  ROR_SCAN_INFO **ror_scans_end= tree->ror_scans_end;
  DBUG_ENTER("get_best_covering_ror_intersect");

  if (!optimizer_flag(param->thd, OPTIMIZER_SWITCH_INDEX_MERGE_INTERSECT))
    DBUG_RETURN(NULL);

  for (ROR_SCAN_INFO **scan= tree->ror_scans; scan != ror_scans_end; ++scan)
    (*scan)->key_components=
      param->table->key_info[(*scan)->keynr].key_parts;

  /*
    Run covering-ROR-search algorithm.
    Assume set I is [ror_scan .. ror_scans_end)
  */

  /*I=set of all covering indexes */
  ror_scan_mark= tree->ror_scans;

  MY_BITMAP *covered_fields= &param->tmp_covered_fields;
  if (!covered_fields->bitmap) 
    covered_fields->bitmap= (my_bitmap_map*)alloc_root(param->mem_root,
                                               param->fields_bitmap_size);
  if (!covered_fields->bitmap ||
      bitmap_init(covered_fields, covered_fields->bitmap,
                  param->table->s->fields, FALSE))
    DBUG_RETURN(0);
  bitmap_clear_all(covered_fields);

  double total_cost= 0.0f;
  ha_rows records=0;
  bool all_covered;

  DBUG_PRINT("info", ("Building covering ROR-intersection"));
  DBUG_EXECUTE("info", print_ror_scans_arr(param->table,
                                           "building covering ROR-I",
                                           ror_scan_mark, ror_scans_end););
  do
  {
    /*
      Update changed sorting info:
        #covered fields,
	number of first not covered component
      Calculate and save these values for each of remaining scans.
    */
    for (ROR_SCAN_INFO **scan= ror_scan_mark; scan != ror_scans_end; ++scan)
    {
      bitmap_subtract(&(*scan)->covered_fields, covered_fields);
      (*scan)->used_fields_covered=
        bitmap_bits_set(&(*scan)->covered_fields);
      (*scan)->first_uncovered_field=
        bitmap_get_first(&(*scan)->covered_fields);
    }

    my_qsort(ror_scan_mark, ror_scans_end-ror_scan_mark, sizeof(ROR_SCAN_INFO*),
             (qsort_cmp)cmp_ror_scan_info_covering);

    DBUG_EXECUTE("info", print_ror_scans_arr(param->table,
                                             "remaining scans",
                                             ror_scan_mark, ror_scans_end););

    /* I=I-first(I) */
    total_cost += (*ror_scan_mark)->index_read_cost;
    records += (*ror_scan_mark)->records;
    DBUG_PRINT("info", ("Adding scan on %s",
                        param->table->key_info[(*ror_scan_mark)->keynr].name));
    if (total_cost > read_time)
      DBUG_RETURN(NULL);
    /* F=F-covered by first(I) */
    bitmap_union(covered_fields, &(*ror_scan_mark)->covered_fields);
    all_covered= bitmap_is_subset(&param->needed_fields, covered_fields);
  } while ((++ror_scan_mark < ror_scans_end) && !all_covered);
  
  if (!all_covered || (ror_scan_mark - tree->ror_scans) == 1)
    DBUG_RETURN(NULL);

  /*
    Ok, [tree->ror_scans .. ror_scan) holds covering index_intersection with
    cost total_cost.
  */
  DBUG_PRINT("info", ("Covering ROR-intersect scans cost: %g", total_cost));
  DBUG_EXECUTE("info", print_ror_scans_arr(param->table,
                                           "creating covering ROR-intersect",
                                           tree->ror_scans, ror_scan_mark););

  /* Add priority queue use cost. */
  total_cost += rows2double(records)*
                log((double)(ror_scan_mark - tree->ror_scans)) /
                (TIME_FOR_COMPARE_ROWID * M_LN2);
  DBUG_PRINT("info", ("Covering ROR-intersect full cost: %g", total_cost));

  if (total_cost > read_time)
    DBUG_RETURN(NULL);

  TRP_ROR_INTERSECT *trp;
  if (!(trp= new (param->mem_root) TRP_ROR_INTERSECT))
    DBUG_RETURN(trp);
  uint best_num= (ror_scan_mark - tree->ror_scans);
  if (!(trp->first_scan= (ROR_SCAN_INFO**)alloc_root(param->mem_root,
                                                     sizeof(ROR_SCAN_INFO*)*
                                                     best_num)))
    DBUG_RETURN(NULL);
  memcpy(trp->first_scan, tree->ror_scans, best_num*sizeof(ROR_SCAN_INFO*));
  trp->last_scan=  trp->first_scan + best_num;
  trp->is_covering= TRUE;
  trp->read_cost= total_cost;
  trp->records= records;
  trp->cpk_scan= NULL;
  set_if_smaller(param->table->quick_condition_rows, records); 

  DBUG_PRINT("info",
             ("Returning covering ROR-intersect plan: cost %g, records %lu",
              trp->read_cost, (ulong) trp->records));
  DBUG_RETURN(trp);
}


/*
  Get best "range" table read plan for given SEL_TREE.
  Also update PARAM members and store ROR scans info in the SEL_TREE.
  SYNOPSIS
    get_key_scans_params
      param        parameters from test_quick_select
      tree         make range select for this SEL_TREE
      index_read_must_be_used if TRUE, assume 'index only' option will be set
                             (except for clustered PK indexes)
      read_time    don't create read plans with cost > read_time.
  RETURN
    Best range read plan
    NULL if no plan found or error occurred
*/

static TRP_RANGE *get_key_scans_params(PARAM *param, SEL_TREE *tree,
                                       bool index_read_must_be_used, 
                                       bool update_tbl_stats,
                                       double read_time)
{
  uint idx;
  SEL_ARG **key,**end, **key_to_read= NULL;
  ha_rows UNINIT_VAR(best_records);              /* protected by key_to_read */
  uint    UNINIT_VAR(best_mrr_flags),            /* protected by key_to_read */
          UNINIT_VAR(best_buf_size);             /* protected by key_to_read */
  TRP_RANGE* read_plan= NULL;
  DBUG_ENTER("get_key_scans_params");
  /*
    Note that there may be trees that have type SEL_TREE::KEY but contain no
    key reads at all, e.g. tree for expression "key1 is not null" where key1
    is defined as "not null".
  */
  DBUG_EXECUTE("info", print_sel_tree(param, tree, &tree->keys_map,
                                      "tree scans"););
  tree->ror_scans_map.clear_all();
  tree->n_ror_scans= 0;
  tree->index_scans= 0;
  if (!tree->keys_map.is_clear_all())
  {
    tree->index_scans=
      (INDEX_SCAN_INFO **) alloc_root(param->mem_root,
                                      sizeof(INDEX_SCAN_INFO *) * param->keys);
  }
  tree->index_scans_end= tree->index_scans;                                                  
  for (idx= 0,key=tree->keys, end=key+param->keys; key != end; key++,idx++)
  {
    if (*key)
    {
      ha_rows found_records;
      COST_VECT cost;
      double found_read_time;
      uint mrr_flags, buf_size;
      INDEX_SCAN_INFO *index_scan;
      uint keynr= param->real_keynr[idx];
      if ((*key)->type == SEL_ARG::MAYBE_KEY ||
          (*key)->maybe_flag)
        param->needed_reg->set_bit(keynr);

      bool read_index_only= index_read_must_be_used ? TRUE :
                            (bool) param->table->covering_keys.is_set(keynr);

      found_records= check_quick_select(param, idx, read_index_only, *key,
                                        update_tbl_stats, &mrr_flags,
                                        &buf_size, &cost);

      if (found_records != HA_POS_ERROR && tree->index_scans &&
          (index_scan= (INDEX_SCAN_INFO *)alloc_root(param->mem_root,
						     sizeof(INDEX_SCAN_INFO))))
      {
        index_scan->idx= idx;
        index_scan->keynr= keynr;
        index_scan->key_info= &param->table->key_info[keynr];
        index_scan->used_key_parts= param->max_key_part+1;
        index_scan->range_count= param->range_count;
        index_scan->records= found_records;
        index_scan->sel_arg= *key;
        *tree->index_scans_end++= index_scan;
      }        
      if ((found_records != HA_POS_ERROR) && param->is_ror_scan)
      {
        tree->n_ror_scans++;
        tree->ror_scans_map.set_bit(idx);
      }
      if (found_records != HA_POS_ERROR &&
          read_time > (found_read_time= cost.total_cost()))
      {
        read_time=    found_read_time;
        best_records= found_records;
        key_to_read=  key;
        best_mrr_flags= mrr_flags;
        best_buf_size=  buf_size;
      }
    }
  }

  DBUG_EXECUTE("info", print_sel_tree(param, tree, &tree->ror_scans_map,
                                      "ROR scans"););
  if (key_to_read)
  {
    idx= key_to_read - tree->keys;
    if ((read_plan= new (param->mem_root) TRP_RANGE(*key_to_read, idx,
                                                    best_mrr_flags)))
    {
      read_plan->records= best_records;
      read_plan->is_ror= tree->ror_scans_map.is_set(idx);
      read_plan->read_cost= read_time;
      read_plan->mrr_buf_size= best_buf_size;
      DBUG_PRINT("info",
                 ("Returning range plan for key %s, cost %g, records %lu",
                  param->table->key_info[param->real_keynr[idx]].name,
                  read_plan->read_cost, (ulong) read_plan->records));
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
  quick_imerge->read_time= read_cost;
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


QUICK_SELECT_I *TRP_INDEX_INTERSECT::make_quick(PARAM *param,
                                                bool retrieve_full_rows,
                                                MEM_ROOT *parent_alloc)
{
  QUICK_INDEX_INTERSECT_SELECT *quick_intersect;
  QUICK_RANGE_SELECT *quick;
  /* index_merge always retrieves full rows, ignore retrieve_full_rows */
  if (!(quick_intersect= new QUICK_INDEX_INTERSECT_SELECT(param->thd, param->table)))
    return NULL;

  quick_intersect->records= records;
  quick_intersect->read_time= read_cost;
  quick_intersect->filtered_scans= filtered_scans;
  for (TRP_RANGE **range_scan= range_scans; range_scan != range_scans_end;
       range_scan++)
  {
    if (!(quick= (QUICK_RANGE_SELECT*)
          ((*range_scan)->make_quick(param, FALSE, &quick_intersect->alloc)))||
        quick_intersect->push_quick_back(quick))
    {
      delete quick;
      delete quick_intersect;
      return NULL;
    }
  }
  return quick_intersect;
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
    for (; first_scan != last_scan;++first_scan)
    {
      if (!(quick= get_quick_select(param, (*first_scan)->idx,
                                    (*first_scan)->sel_arg,
                                    HA_MRR_USE_DEFAULT_IMPL | HA_MRR_SORTED,
                                    0, alloc)) ||
          quick_intrsect->push_quick_back(alloc, quick))
      {
        delete quick_intrsect;
        DBUG_RETURN(NULL);
      }
    }
    if (cpk_scan)
    {
      if (!(quick= get_quick_select(param, cpk_scan->idx,
                                    cpk_scan->sel_arg,
                                    HA_MRR_USE_DEFAULT_IMPL | HA_MRR_SORTED,
                                    0, alloc)))
      {
        delete quick_intrsect;
        DBUG_RETURN(NULL);
      }
      quick->file= NULL; 
      quick_intrsect->cpk_quick= quick;
    }
    quick_intrsect->records= records;
    quick_intrsect->read_time= read_cost;
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
    quick_roru->read_time= read_cost;
  }
  DBUG_RETURN(quick_roru);
}


/*
  Build a SEL_TREE for <> or NOT BETWEEN predicate
 
  SYNOPSIS
    get_ne_mm_tree()
      param       PARAM from SQL_SELECT::test_quick_select
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
  SEL_TREE *tree;
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
   

/*
  Build a SEL_TREE for a simple predicate
 
  SYNOPSIS
    get_func_mm_tree()
      param       PARAM from SQL_SELECT::test_quick_select
      cond_func   item for the predicate
      field       field in the predicate
      value       constant in the predicate
      cmp_type    compare type for the field
      inv         TRUE <> NOT cond_func is considered
                  (makes sense only when cond_func is BETWEEN or IN) 

  RETURN 
    Pointer to the tree built tree
*/

static SEL_TREE *get_func_mm_tree(RANGE_OPT_PARAM *param, Item_func *cond_func, 
                                  Field *field, Item *value,
                                  Item_result cmp_type, bool inv)
{
  SEL_TREE *tree= 0;
  DBUG_ENTER("get_func_mm_tree");

  switch (cond_func->functype()) {

  case Item_func::NE_FUNC:
    tree= get_ne_mm_tree(param, cond_func, field, value, value, cmp_type);
    break;

  case Item_func::BETWEEN:
  {
    if (!value)
    {
      if (inv)
      {
        tree= get_ne_mm_tree(param, cond_func, field, cond_func->arguments()[1],
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
                          (value == (Item*)1 ? Item_func::GT_FUNC :
                                               Item_func::LT_FUNC):
                          (value == (Item*)1 ? Item_func::LE_FUNC :
                                               Item_func::GE_FUNC)),
                         cond_func->arguments()[0], cmp_type);
    break;
  }
  case Item_func::IN_FUNC:
  {
    Item_func_in *func=(Item_func_in*) cond_func;

    /*
      Array for IN() is constructed when all values have the same result
      type. Tree won't be built for values with different result types,
      so we check it here to avoid unnecessary work.
    */
    if (!func->arg_types_compatible)
      break;     

    if (inv)
    {
      if (func->array && func->array->result_type() != ROW_RESULT)
      {
        /*
          We get here for conditions in form "t.key NOT IN (c1, c2, ...)",
          where c{i} are constants. Our goal is to produce a SEL_TREE that 
          represents intervals:
          
          ($MIN<t.key<c1) OR (c1<t.key<c2) OR (c2<t.key<c3) OR ...    (*)
          
          where $MIN is either "-inf" or NULL.
          
          The most straightforward way to produce it is to convert NOT IN
          into "(t.key != c1) AND (t.key != c2) AND ... " and let the range
          analyzer to build SEL_TREE from that. The problem is that the
          range analyzer will use O(N^2) memory (which is probably a bug),
          and people do use big NOT IN lists (e.g. see BUG#15872, BUG#21282),
          will run out of memory.

          Another problem with big lists like (*) is that a big list is
          unlikely to produce a good "range" access, while considering that
          range access will require expensive CPU calculations (and for 
          MyISAM even index accesses). In short, big NOT IN lists are rarely
          worth analyzing.

          Considering the above, we'll handle NOT IN as follows:
          * if the number of entries in the NOT IN list is less than
            NOT_IN_IGNORE_THRESHOLD, construct the SEL_TREE (*) manually.
          * Otherwise, don't produce a SEL_TREE.
        */
#define NOT_IN_IGNORE_THRESHOLD 1000
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
        Item *value_item= func->array->create_item();
        param->thd->mem_root= tmp_root;

        if (func->array->count > NOT_IN_IGNORE_THRESHOLD || !value_item)
          break;

        /* Get a SEL_TREE for "(-inf|NULL) < X < c_0" interval.  */
        uint i=0;
        do 
        {
          func->array->value_to_item(i, value_item);
          tree= get_mm_parts(param, cond_func, field, Item_func::LT_FUNC,
                             value_item, cmp_type);
          if (!tree)
            break;
          i++;
        } while (i < func->array->count && tree->type == SEL_TREE::IMPOSSIBLE);

        if (!tree || tree->type == SEL_TREE::IMPOSSIBLE)
        {
          /* We get here in cases like "t.unsigned NOT IN (-1,-2,-3) */
          tree= NULL;
          break;
        }
        SEL_TREE *tree2;
        for (; i < func->array->count; i++)
        {
          if (func->array->compare_elems(i, i-1))
          {
            /* Get a SEL_TREE for "-inf < X < c_i" interval */
            func->array->value_to_item(i, value_item);
            tree2= get_mm_parts(param, cond_func, field, Item_func::LT_FUNC,
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
          tree2= get_mm_parts(param, cond_func, field, Item_func::GT_FUNC,
                              value_item, cmp_type);
          tree= tree_or(param, tree, tree2);
        }
      }
      else
      {
        tree= get_ne_mm_tree(param, cond_func, field,
                             func->arguments()[1], func->arguments()[1],
                             cmp_type);
        if (tree)
        {
          Item **arg, **end;
          for (arg= func->arguments()+2, end= arg+func->argument_count()-2;
               arg < end ; arg++)
          {
            tree=  tree_and(param, tree, get_ne_mm_tree(param, cond_func, field, 
                                                        *arg, *arg, cmp_type));
          }
        }
      }
    }
    else
    {    
      tree= get_mm_parts(param, cond_func, field, Item_func::EQ_FUNC,
                         func->arguments()[1], cmp_type);
      if (tree)
      {
        Item **arg, **end;
        for (arg= func->arguments()+2, end= arg+func->argument_count()-2;
             arg < end ; arg++)
        {
          tree= tree_or(param, tree, get_mm_parts(param, cond_func, field, 
                                                  Item_func::EQ_FUNC,
                                                  *arg, cmp_type));
        }
      }
    }
    break;
  }
  default: 
  {
    /* 
       Here the function for the following predicates are processed:
       <, <=, =, >=, >, LIKE, IS NULL, IS NOT NULL.
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
      param       PARAM from SQL_SELECT::test_quick_select
      cond_func   item for the predicate
      field_item  field in the predicate
      value       constant in the predicate
                  (for BETWEEN it contains the number of the field argument,
                   for IN it's always 0) 
      inv         TRUE <> NOT cond_func is considered
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
                                       Item_func *cond_func,
                                       Item_field *field_item, Item *value, 
                                       bool inv)
{
  SEL_TREE *tree= 0;
  SEL_TREE *ftree= 0;
  table_map ref_tables= 0;
  table_map param_comp= ~(param->prev_tables | param->read_tables |
		          param->current_table);
  DBUG_ENTER("get_full_func_mm_tree");

#ifdef HAVE_SPATIAL
  if (field_item->field->type() == MYSQL_TYPE_GEOMETRY)
  {
    /* We have to be able to store all sorts of spatial features here */
    ((Field_geom*) field_item->field)->geom_type= Field::GEOM_GEOMETRY;
  }
#endif /*HAVE_SPATIAL*/

  for (uint i= 0; i < cond_func->arg_count; i++)
  {
    Item *arg= cond_func->arguments()[i]->real_item();
    if (arg != field_item)
      ref_tables|= arg->used_tables();
  }
  Field *field= field_item->field;
  Item_result cmp_type= field->cmp_type();
  if (!((ref_tables | field->table->map) & param_comp))
    ftree= get_func_mm_tree(param, cond_func, field, value, cmp_type, inv);
  Item_equal *item_equal= field_item->item_equal;
  if (item_equal)
  {
    Item_equal_fields_iterator it(*item_equal);
    while (it++)
    {
      Field *f= it.get_curr_field();
      if (field->eq(f))
        continue;
      if (!((ref_tables | f->table->map) & param_comp))
      {
        tree= get_func_mm_tree(param, cond_func, f, value, cmp_type, inv);
        ftree= !ftree ? tree : tree_and(param, ftree, tree);
      }
    }
  }
  DBUG_RETURN(ftree);
}

	/* make a select tree of all keys in condition */

static SEL_TREE *get_mm_tree(RANGE_OPT_PARAM *param,COND *cond)
{
  SEL_TREE *tree=0;
  SEL_TREE *ftree= 0;
  Item_field *field_item= 0;
  bool inv= FALSE;
  Item *value= 0;
  DBUG_ENTER("get_mm_tree");

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
        if (param->statement_should_be_aborted())
          DBUG_RETURN(NULL);
        tree= tree_and(param,tree,new_tree);
        if (tree && tree->type == SEL_TREE::IMPOSSIBLE)
          break;
      }
    }
    else
    {                                           // COND OR
      tree= get_mm_tree(param,li++);
      if (param->statement_should_be_aborted())
        DBUG_RETURN(NULL);
      if (tree)
      {
        Item *item;
        while ((item=li++))
        {
          SEL_TREE *new_tree=get_mm_tree(param,item);
          if (new_tree == NULL || param->statement_should_be_aborted())
            DBUG_RETURN(NULL);
          tree= tree_or(param,tree,new_tree);
          if (tree == NULL || tree->type == SEL_TREE::ALWAYS)
            break;
        }
      }
    }
    DBUG_RETURN(tree);
  }
  /* Here when simple cond */
  if (cond->const_item())
  {
    if (cond->is_expensive())
      DBUG_RETURN(0);
    /*
      During the cond->val_int() evaluation we can come across a subselect 
      item which may allocate memory on the thd->mem_root and assumes 
      all the memory allocated has the same life span as the subselect 
      item itself. So we have to restore the thread's mem_root here.
    */
    MEM_ROOT *tmp_root= param->mem_root;
    param->thd->mem_root= param->old_root;
    tree= cond->val_int() ? new(tmp_root) SEL_TREE(SEL_TREE::ALWAYS) :
                            new(tmp_root) SEL_TREE(SEL_TREE::IMPOSSIBLE);
    param->thd->mem_root= tmp_root;
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
    DBUG_RETURN(new SEL_TREE(SEL_TREE::MAYBE));
  }

  Item_func *cond_func= (Item_func*) cond;
  if (cond_func->functype() == Item_func::BETWEEN ||
      cond_func->functype() == Item_func::IN_FUNC)
    inv= ((Item_func_opt_neg *) cond_func)->negated;
  else if (cond_func->select_optimize() == Item_func::OPTIMIZE_NONE)
    DBUG_RETURN(0);			       

  param->cond= cond;

  switch (cond_func->functype()) {
  case Item_func::BETWEEN:
    if (cond_func->arguments()[0]->real_item()->type() == Item::FIELD_ITEM)
    {
      field_item= (Item_field*) (cond_func->arguments()[0]->real_item());
      ftree= get_full_func_mm_tree(param, cond_func, field_item, NULL, inv);
    }

    /*
      Concerning the code below see the NOTES section in
      the comments for the function get_full_func_mm_tree()
    */
    for (uint i= 1 ; i < cond_func->arg_count ; i++)
    {
      if (cond_func->arguments()[i]->real_item()->type() == Item::FIELD_ITEM)
      {
        field_item= (Item_field*) (cond_func->arguments()[i]->real_item());
        SEL_TREE *tmp= get_full_func_mm_tree(param, cond_func, 
                                    field_item, (Item*)(intptr)i, inv);
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
  case Item_func::IN_FUNC:
  {
    Item_func_in *func=(Item_func_in*) cond_func;
    if (func->key_item()->real_item()->type() != Item::FIELD_ITEM)
      DBUG_RETURN(0);
    field_item= (Item_field*) (func->key_item()->real_item());
    ftree= get_full_func_mm_tree(param, cond_func, field_item, NULL, inv);
    break;
  }
  case Item_func::MULT_EQUAL_FUNC:
  {
    Item_equal *item_equal= (Item_equal *) cond;    
    if (!(value= item_equal->get_const()) || value->is_expensive())
      DBUG_RETURN(0);
    Item_equal_fields_iterator it(*item_equal);
    ref_tables= value->used_tables();
    while (it++)
    {
      Field *field= it.get_curr_field();
      Item_result cmp_type= field->cmp_type();
      if (!((ref_tables | field->table->map) & param_comp))
      {
        tree= get_mm_parts(param, cond, field, Item_func::EQ_FUNC,
		           value,cmp_type);
        ftree= !ftree ? tree : tree_and(param, ftree, tree);
      }
    }
    
    DBUG_RETURN(ftree);
  }
  default:
    if (cond_func->arguments()[0]->real_item()->type() == Item::FIELD_ITEM)
    {
      field_item= (Item_field*) (cond_func->arguments()[0]->real_item());
      value= cond_func->arg_count > 1 ? cond_func->arguments()[1] : 0;
    }
    else if (cond_func->have_rev_func() &&
             cond_func->arguments()[1]->real_item()->type() ==
                                                            Item::FIELD_ITEM)
    {
      field_item= (Item_field*) (cond_func->arguments()[1]->real_item());
      value= cond_func->arguments()[0];
    }
    else
      DBUG_RETURN(0);
    if (value && value->is_expensive())
      DBUG_RETURN(0);

    ftree= get_full_func_mm_tree(param, cond_func, field_item, value, inv);
  }

  DBUG_RETURN(ftree);
}


static SEL_TREE *
get_mm_parts(RANGE_OPT_PARAM *param, COND *cond_func, Field *field,
	     Item_func::Functype type,
	     Item *value, Item_result cmp_type)
{
  DBUG_ENTER("get_mm_parts");
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
      SEL_ARG *sel_arg=0;
      if (!tree && !(tree=new SEL_TREE()))
	DBUG_RETURN(0);				// OOM
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
	// This key may be used later
	if (!(sel_arg= new SEL_ARG(SEL_ARG::MAYBE_KEY)))
	  DBUG_RETURN(0);			// OOM
      }
      sel_arg->part=(uchar) key_part->part;
      sel_arg->max_part_no= sel_arg->part+1;
      tree->keys[key_part->key]=sel_add(tree->keys[key_part->key],sel_arg);
      tree->keys_map.set_bit(key_part->key);
    }
  }

  if (tree && tree->merges.is_empty() && tree->keys_map.is_clear_all())
    tree= NULL;
  DBUG_RETURN(tree);
}


static SEL_ARG *
get_mm_leaf(RANGE_OPT_PARAM *param, COND *conf_func, Field *field,
            KEY_PART *key_part, Item_func::Functype type,Item *value)
{
  uint maybe_null=(uint) field->real_maybe_null();
  bool optimize_range;
  SEL_ARG *tree= 0;
  MEM_ROOT *alloc= param->mem_root;
  uchar *str;
  int err;
  DBUG_ENTER("get_mm_leaf");

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
    if (field->table->maybe_null)		// Can't use a key on this
      goto end;
    if (!maybe_null)				// Not null field
    {
      if (type == Item_func::ISNULL_FUNC)
        tree= &null_element;
      goto end;
    }
    if (!(tree= new (alloc) SEL_ARG(field,is_null_string,is_null_string)))
      goto end;                                 // out of memory
    if (type == Item_func::ISNOTNULL_FUNC)
    {
      tree->min_flag=NEAR_MIN;		    /* IS NOT NULL ->  X > NULL */
      tree->max_flag=NO_MAX_RANGE;
    }
    goto end;
  }

  /*
    1. Usually we can't use an index if the column collation
       differ from the operation collation.

    2. However, we can reuse a case insensitive index for
       the binary searches:

       WHERE latin1_swedish_ci_column = 'a' COLLATE lati1_bin;

       WHERE latin1_swedish_ci_colimn = BINARY 'a '

  */
  if (field->result_type() == STRING_RESULT &&
      ((Field_str*) field)->match_collation_to_optimize_range() &&
      value->result_type() == STRING_RESULT &&
      key_part->image_type == Field::itRAW &&
      ((Field_str*)field)->charset() != conf_func->compare_collation() &&
      !(conf_func->compare_collation()->state & MY_CS_BINSORT &&
        (type == Item_func::EQUAL_FUNC || type == Item_func::EQ_FUNC)))
    goto end;

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
    uint field_length= field->pack_length()+maybe_null;

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

    field_length-= maybe_null;
    like_error= my_like_range(field->charset(),
			      res->ptr(), res->length(),
			      ((Item_func_like*)(param->cond))->escape,
			      wild_one, wild_many,
			      field_length,
			      (char*) min_str+offset, (char*) max_str+offset,
			      &min_length, &max_length);
    if (like_error)				// Can't optimize with LIKE
      goto end;

    if (offset != maybe_null)			// BLOB or VARCHAR
    {
      int2store(min_str+maybe_null,min_length);
      int2store(max_str+maybe_null,max_length);
    }
    tree= new (alloc) SEL_ARG(field, min_str, max_str);
    goto end;
  }

  if (!optimize_range &&
      type != Item_func::EQ_FUNC &&
      type != Item_func::EQUAL_FUNC)
    goto end;                                   // Can't optimize this

  /*
    We can't always use indexes when comparing a string index to a number
    cmp_type() is checked to allow compare of dates to numbers
  */
  if (field->cmp_type() == STRING_RESULT && value->cmp_type() != STRING_RESULT)
    goto end;
  err= value->save_in_field_no_warnings(field, 1);
  if (err == 2 && field->cmp_type() == STRING_RESULT)
  {
    if (type == Item_func::EQ_FUNC)
    {
      tree= new (alloc) SEL_ARG(field, 0, 0);
      tree->type= SEL_ARG::IMPOSSIBLE;
    }
    else 
      tree= NULL; /*  Cannot infer anything */
    goto end;
  }
  if (err > 0)
  {
    if (field->cmp_type() != value->result_type())
    {
      if ((type == Item_func::EQ_FUNC || type == Item_func::EQUAL_FUNC) &&
          value->result_type() == item_cmp_type(field->result_type(),
                                                value->result_type()))
      {
        tree= new (alloc) SEL_ARG(field, 0, 0);
        tree->type= SEL_ARG::IMPOSSIBLE;
        goto end;
      }
      else
      {
        /*
          TODO: We should return trees of the type SEL_ARG::IMPOSSIBLE
          for the cases like int_field > 999999999999999999999999 as well.
        */
        tree= 0;
        if (err == 3 && field->type() == FIELD_TYPE_DATE &&
            (type == Item_func::GT_FUNC || type == Item_func::GE_FUNC ||
             type == Item_func::LT_FUNC || type == Item_func::LE_FUNC) )
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
            be done together with other types at the end of this function
            (grep for stored_field_cmp_to_item)
          */
        }
        else
          goto end;
      }
    }

    /*
      guaranteed at this point:  err > 0; field and const of same type
      If an integer got bounded (e.g. to within 0..255 / -128..127)
      for < or >, set flags as for <= or >= (no NEAR_MAX / NEAR_MIN)
    */
    else if (err == 1 && field->result_type() == INT_RESULT)
    {
      if (type == Item_func::LT_FUNC && (value->val_int() > 0))
        type = Item_func::LE_FUNC;
      else if (type == Item_func::GT_FUNC &&
               (field->type() != FIELD_TYPE_BIT) &&
               !((Field_num*)field)->unsigned_flag &&
               !((Item_int*)value)->unsigned_flag &&
               (value->val_int() < 0))
        type = Item_func::GE_FUNC;
    }
  }
  else if (err < 0)
  {
    /* This happens when we try to insert a NULL field in a not null column */
    tree= &null_element;                        // cmp with NULL is never TRUE
    goto end;
  }

  /*
    Any sargable predicate except "<=>" involving NULL as a constant is always
    FALSE
  */
  if (type != Item_func::EQUAL_FUNC && field->is_real_null())
  {
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
    if (stored_field_cmp_to_item(param->thd, field, value) == 0)
      tree->max_flag=NEAR_MAX;
    /* fall through */
  case Item_func::LE_FUNC:
    if (!maybe_null)
      tree->min_flag=NO_MIN_RANGE;		/* From start */
    else
    {						// > NULL
      tree->min_value=is_null_string;
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
    tree->min_flag=GEOM_FLAG | HA_READ_MBR_EQUAL;// NEAR_MIN;//512;
    tree->max_flag=NO_MAX_RANGE;
    break;
  case Item_func::SP_DISJOINT_FUNC:
    tree->min_flag=GEOM_FLAG | HA_READ_MBR_DISJOINT;// NEAR_MIN;//512;
    tree->max_flag=NO_MAX_RANGE;
    break;
  case Item_func::SP_INTERSECTS_FUNC:
    tree->min_flag=GEOM_FLAG | HA_READ_MBR_INTERSECT;// NEAR_MIN;//512;
    tree->max_flag=NO_MAX_RANGE;
    break;
  case Item_func::SP_TOUCHES_FUNC:
    tree->min_flag=GEOM_FLAG | HA_READ_MBR_INTERSECT;// NEAR_MIN;//512;
    tree->max_flag=NO_MAX_RANGE;
    break;

  case Item_func::SP_CROSSES_FUNC:
    tree->min_flag=GEOM_FLAG | HA_READ_MBR_INTERSECT;// NEAR_MIN;//512;
    tree->max_flag=NO_MAX_RANGE;
    break;
  case Item_func::SP_WITHIN_FUNC:
    tree->min_flag=GEOM_FLAG | HA_READ_MBR_WITHIN;// NEAR_MIN;//512;
    tree->max_flag=NO_MAX_RANGE;
    break;

  case Item_func::SP_CONTAINS_FUNC:
    tree->min_flag=GEOM_FLAG | HA_READ_MBR_CONTAIN;// NEAR_MIN;//512;
    tree->max_flag=NO_MAX_RANGE;
    break;
  case Item_func::SP_OVERLAPS_FUNC:
    tree->min_flag=GEOM_FLAG | HA_READ_MBR_INTERSECT;// NEAR_MIN;//512;
    tree->max_flag=NO_MAX_RANGE;
    break;

  default:
    break;
  }

end:
  param->thd->mem_root= alloc;
  DBUG_RETURN(tree);
}


/******************************************************************************
** Tree manipulation functions
** If tree is 0 it means that the condition can't be tested. It refers
** to a non existent table or to a field in current table with isn't a key.
** The different tree flags:
** IMPOSSIBLE:	 Condition is never TRUE
** ALWAYS:	 Condition is always TRUE
** MAYBE:	 Condition may exists when tables are read
** MAYBE_KEY:	 Condition refers to a key that may be used in join loop
** KEY_RANGE:	 Condition uses a key
******************************************************************************/

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


/* 
  Build a range tree for the conjunction of the range parts of two trees

  SYNOPSIS
    and_range_trees()
      param           Context info for the operation
      tree1           SEL_TREE for the first conjunct          
      tree2           SEL_TREE for the second conjunct
      result          SEL_TREE for the result

  DESCRIPTION
    This function takes range parts of two trees tree1 and tree2 and builds
    a range tree for the conjunction of the formulas that these two range parts
    represent.
    More exactly: 
    if the range part of tree1 represents the normalized formula 
      R1_1 AND ... AND R1_k,
    and the range part of tree2 represents the normalized formula
      R2_1 AND ... AND R2_k,
    then the range part of the result represents the formula:
     RT = R_1 AND ... AND R_k, where R_i=(R1_i AND R2_i) for each i from [1..k]

    The function assumes that tree1 is never equal to tree2. At the same
    time the tree result can be the same as tree1 (but never as tree2).
    If result==tree1 then rt replaces the range part of tree1 leaving
    imerges as they are.
    if result!=tree1 than it is assumed that the SEL_ARG trees in tree1 and
    tree2 should be preserved. Otherwise they can be destroyed.

  RETURN 
    1    if the type the result tree is  SEL_TREE::IMPOSSIBLE
    0    otherwise    
*/

static
int and_range_trees(RANGE_OPT_PARAM *param, SEL_TREE *tree1, SEL_TREE *tree2,
                    SEL_TREE *result)
{
  DBUG_ENTER("and_ranges");
  key_map  result_keys;
  result_keys.clear_all();
  key_map anded_keys= tree1->keys_map;
  anded_keys.merge(tree2->keys_map);
  int key_no;
  key_map::Iterator it(anded_keys);
  while ((key_no= it++) != key_map::Iterator::BITMAP_END)
  {
    uint flag=0;
    SEL_ARG *key1= tree1->keys[key_no];
    SEL_ARG *key2= tree2->keys[key_no];
    if (key1 && !key1->simple_key())
      flag|= CLONE_KEY1_MAYBE;
    if (key2 && !key2->simple_key())
      flag|=CLONE_KEY2_MAYBE;
    if (result != tree1)
    { 
      if (key1)
        key1->incr_refs();
      if (key2)
        key2->incr_refs();
    }
    SEL_ARG *key;
    if ((result->keys[key_no]= key =key_and(param, key1, key2, flag)))
    {
      if (key && key->type == SEL_ARG::IMPOSSIBLE)
      {
	result->type= SEL_TREE::IMPOSSIBLE;
        DBUG_RETURN(1);
      }
      result_keys.set_bit(key_no);
#ifdef EXTRA_DEBUG
      if (param->alloced_sel_args < SEL_ARG::MAX_SEL_ARGS) 
        key->test_use_count(key);
#endif
    }
  }
  result->keys_map= result_keys;
  DBUG_RETURN(0);
}
  

/*
  Build a SEL_TREE for a conjunction out of such trees for the conjuncts

  SYNOPSIS
    tree_and()
      param           Context info for the operation
      tree1           SEL_TREE for the first conjunct          
      tree2           SEL_TREE for the second conjunct

  DESCRIPTION
    This function builds a tree for the formula (A AND B) out of the trees
    tree1 and tree2 that has been built for the formulas A and B respectively.

    In a general case
      tree1 represents the formula RT1 AND MT1,
        where RT1 = R1_1 AND ... AND R1_k1, MT1=M1_1 AND ... AND M1_l1;
      tree2 represents the formula RT2 AND MT2 
        where RT2 = R2_1 AND ... AND R2_k2, MT2=M2_1 AND ... AND M2_l2.

    The result tree will represent the formula of the the following structure:
      RT AND RT1MT2 AND RT2MT1, such that
        rt is a tree obtained by range intersection of trees tree1 and tree2,
        RT1MT2 = RT1M2_1 AND ... AND RT1M2_l2,
        RT2MT1 = RT2M1_1 AND ... AND RT2M1_l1,
        where rt1m2_i (i=1,...,l2) is the result of the pushdown operation
        of range tree rt1 into imerge m2_i, while rt2m1_j (j=1,...,l1) is the
        result of the pushdown operation of range tree rt2 into imerge m1_j.

    RT1MT2/RT2MT is empty if MT2/MT1 is empty.
 
    The range intersection of two range trees is produced by the function
    and_range_trees. The pushdown of a range tree to a imerge is performed
    by the function imerge_list_and_tree. This function may produce imerges
    containing only one range tree. Such trees are intersected with rt and 
    the result of intersection is returned as the range part of the result
    tree, while the corresponding imerges are removed altogether from its
    imerge part. 
    
  NOTE
    The pushdown operation of range trees into imerges is needed to be able
    to construct valid imerges for the condition like this:
      key1_p1=c1 AND (key1_p2 BETWEEN c21 AND c22 OR key2 < c2)

  NOTE
    Currently we do not support intersection between indexes and index merges.
    When this will be supported the list of imerges for the result tree
    should include also imerges from M1 and M2. That's why an extra parameter
    is added to the function imerge_list_and_tree. If we call the function
    with the last parameter equal to FALSE then MT1 and MT2 will be preserved
    in the imerge list of the result tree. This can lead to the exponential
    growth of the imerge list though. 
    Currently the last parameter of imerge_list_and_tree calls is always
    TRUE.

  RETURN
    The result tree, if a success
    0 - otherwise.        
*/

static 
SEL_TREE *tree_and(RANGE_OPT_PARAM *param, SEL_TREE *tree1, SEL_TREE *tree2)
{
  DBUG_ENTER("tree_and");
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

  if (!tree1->merges.is_empty())
    imerge_list_and_tree(param, &tree1->merges, tree2, TRUE);
  if (!tree2->merges.is_empty())
    imerge_list_and_tree(param, &tree2->merges, tree1, TRUE);
  if (and_range_trees(param, tree1, tree2, tree1))
    DBUG_RETURN(tree1);
  imerge_list_and_list(&tree1->merges, &tree2->merges);
  eliminate_single_tree_imerges(param, tree1);
  DBUG_RETURN(tree1);
}


/*
  Eliminate single tree imerges in a SEL_TREE objects

  SYNOPSIS
    eliminate_single_tree_imerges()
      param      Context info for the function
      tree       SEL_TREE where single tree imerges are to be eliminated 

  DESCRIPTION
    For each imerge in 'tree' that contains only one disjunct tree, i.e.
    for any imerge of the form m=rt, the function performs and operation
    the range part of tree, replaces rt the with the result of anding and
    removes imerge m from the the merge part of 'tree'.

  RETURN VALUE
    none          
*/

static
void eliminate_single_tree_imerges(RANGE_OPT_PARAM *param, SEL_TREE *tree)
{
  SEL_IMERGE *imerge;
  List<SEL_IMERGE> merges= tree->merges;
  List_iterator<SEL_IMERGE> it(merges);
  tree->merges.empty();
  while ((imerge= it++))
  {
    if (imerge->trees+1 == imerge->trees_next)
    {
      tree= tree_and(param, tree, *imerge->trees);
      it.remove();
    }
  }
  tree->merges= merges;
} 


/*
  For two trees check that there are indexes with ranges in both of them  
 
  SYNOPSIS
    sel_trees_have_common_keys()
      tree1           SEL_TREE for the first tree
      tree2           SEL_TREE for the second tree
      common_keys OUT bitmap of all indexes with ranges in both trees

  DESCRIPTION
    For two trees tree1 and tree1 the function checks if there are indexes
    in their range parts such that SEL_ARG trees are defined for them in the
    range parts of both trees. The function returns the bitmap of such 
    indexes in the parameter common_keys.

  RETURN 
    TRUE    if there are such indexes (common_keys is nor empty)
    FALSE   otherwise
*/

static
bool sel_trees_have_common_keys(SEL_TREE *tree1, SEL_TREE *tree2, 
                                key_map *common_keys)
{
  *common_keys= tree1->keys_map;
  common_keys->intersect(tree2->keys_map);
  return !common_keys->is_clear_all();
}


/*
  Check whether range parts of two trees can be ored for some indexes

  SYNOPSIS
    sel_trees_can_be_ored()
      param              Context info for the function
      tree1              SEL_TREE for the first tree
      tree2              SEL_TREE for the second tree
      common_keys IN/OUT IN: bitmap of all indexes with SEL_ARG in both trees
                        OUT: bitmap of all indexes that can be ored

  DESCRIPTION
    For two trees tree1 and tree2 and the bitmap common_keys containing
    bits for indexes that have SEL_ARG trees in range parts of both trees
    the function checks if there are indexes for which SEL_ARG trees can
    be ored. Two SEL_ARG trees for the same index can be ored if the most
    major components of the index used in these trees coincide. If the 
    SEL_ARG trees for an index cannot be ored the function clears the bit
    for this index in the bitmap common_keys.

    The function does not verify that indexes marked in common_keys really
    have SEL_ARG trees in both tree1 and tree2. It assumes that this is true.

  NOTE
    The function sel_trees_can_be_ored is usually used in pair with the
    function sel_trees_have_common_keys.

  RETURN
    TRUE    if there are indexes for which SEL_ARG trees can be ored 
    FALSE   otherwise
*/

static
bool sel_trees_can_be_ored(RANGE_OPT_PARAM* param,
                           SEL_TREE *tree1, SEL_TREE *tree2, 
                           key_map *common_keys)
{
  DBUG_ENTER("sel_trees_can_be_ored");
  if (!sel_trees_have_common_keys(tree1, tree2, common_keys))
    DBUG_RETURN(FALSE);
  int key_no;
  key_map::Iterator it(*common_keys);
  while ((key_no= it++) != key_map::Iterator::BITMAP_END)
  {
    DBUG_ASSERT(tree1->keys[key_no] && tree2->keys[key_no]);
    /* Trees have a common key, check if they refer to the same key part */
    if (tree1->keys[key_no]->part != tree2->keys[key_no]->part)
      common_keys->clear_bit(key_no);
  }
  DBUG_RETURN(!common_keys->is_clear_all());
}

/*
  Check whether range parts of two trees must be ored for some indexes

  SYNOPSIS
    sel_trees_must_be_ored()
      param              Context info for the function
      tree1              SEL_TREE for the first tree
      tree2              SEL_TREE for the second tree
      ordable_keys       bitmap of SEL_ARG trees that can be ored

  DESCRIPTION
    For two trees tree1 and tree2 the function checks whether they must be
    ored. The function assumes that the bitmap ordable_keys contains bits for
    those corresponding pairs of SEL_ARG trees from tree1 and tree2 that can
    be ored.
    We believe that tree1 and tree2 must be ored if any pair of SEL_ARG trees
    r1 and r2, such that r1 is from tree1 and r2 is from tree2 and both
    of them are marked in ordable_keys, can be merged.
    
  NOTE
    The function sel_trees_must_be_ored as a rule is used in pair with the
    function sel_trees_can_be_ored.

  RETURN
    TRUE    if there are indexes for which SEL_ARG trees must be ored 
    FALSE   otherwise
*/

static
bool sel_trees_must_be_ored(RANGE_OPT_PARAM* param,
                            SEL_TREE *tree1, SEL_TREE *tree2,
                            key_map oredable_keys)
{
  key_map tmp;
  DBUG_ENTER("sel_trees_must_be_ored");

  tmp= tree1->keys_map;
  tmp.merge(tree2->keys_map);
  tmp.subtract(oredable_keys);
  if (!tmp.is_clear_all())
    DBUG_RETURN(FALSE);

  int idx1, idx2;
  key_map::Iterator it1(oredable_keys);
  while ((idx1= it1++) != key_map::Iterator::BITMAP_END)
  {
    KEY_PART *key1_init= param->key[idx1]+tree1->keys[idx1]->part;
    KEY_PART *key1_end= param->key[idx1]+tree1->keys[idx1]->max_part_no;
    key_map::Iterator it2(oredable_keys);
    while ((idx2= it2++) != key_map::Iterator::BITMAP_END)
    {
      if (idx2 <= idx1)
        continue;
      
      KEY_PART *key2_init= param->key[idx2]+tree2->keys[idx2]->part;
      KEY_PART *key2_end= param->key[idx2]+tree2->keys[idx2]->max_part_no;
      KEY_PART *part1, *part2;
      for (part1= key1_init, part2= key2_init;
           part1 < key1_end && part2 < key2_end;
           part1++, part2++)
      { 
        if (!part1->field->eq(part2->field))
          DBUG_RETURN(FALSE);
      }
    }
  }
      
  DBUG_RETURN(TRUE);
}  


/*
  Remove the trees that are not suitable for record retrieval

  SYNOPSIS
    remove_nonrange_trees()
      param  Context info for the function
      tree   Tree to be processed, tree->type is KEY or KEY_SMALLER
 
  DESCRIPTION
    This function walks through tree->keys[] and removes the SEL_ARG* trees
    that are not "maybe" trees (*) and cannot be used to construct quick range
    selects.
    (*) - have type MAYBE or MAYBE_KEY. Perhaps we should remove trees of
          these types here as well.

    A SEL_ARG* tree cannot be used to construct quick select if it has
    tree->part != 0. (e.g. it could represent "keypart2 < const").
    
    Normally we allow construction of SEL_TREE objects that have SEL_ARG
    trees that do not allow quick range select construction.
    For example:
    for " keypart1=1 AND keypart2=2 " the execution will proceed as follows:
    tree1= SEL_TREE { SEL_ARG{keypart1=1} }
    tree2= SEL_TREE { SEL_ARG{keypart2=2} } -- can't make quick range select
                                               from this
    call tree_and(tree1, tree2) -- this joins SEL_ARGs into a usable SEL_ARG
                                   tree.

    Another example:
    tree3= SEL_TREE { SEL_ARG{key1part1 = 1} }
    tree4= SEL_TREE { SEL_ARG{key2part2 = 2} }  -- can't make quick range select
                                               from this
    call tree_or(tree3, tree4) -- creates a SEL_MERGE ot of which no index
    merge can be constructed, but it is potentially useful, as anding it with
    tree5= SEL_TREE { SEL_ARG{key2part1 = 3} } creates an index merge that
    represents the formula
      key1part1=1 AND key2part1=3 OR key2part1=3 AND key2part2=2 
    for which an index merge can be built. 

    Any final SEL_TREE may contain SEL_ARG trees for which no quick select
    can be built. Such SEL_ARG trees should be removed from the range part
    before different range scans are evaluated. Such SEL_ARG trees also should
    be removed from all range trees of each index merge before different
    possible index merge plans are evaluated. If after this removal one
    of the range trees in the index merge becomes empty the whole index merge
    must be discarded.
       
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


/*
  Build a SEL_TREE for a disjunction out of such trees for the disjuncts

  SYNOPSIS
    tree_or()
      param           Context info for the operation
      tree1           SEL_TREE for the first disjunct          
      tree2           SEL_TREE for the second disjunct

  DESCRIPTION
    This function builds a tree for the formula (A OR B) out of the trees
    tree1 and tree2 that has been built for the formulas A and B respectively.

    In a general case
      tree1 represents the formula RT1 AND MT1,
        where RT1=R1_1 AND ... AND R1_k1, MT1=M1_1 AND ... AND M1_l1;
      tree2 represents the formula RT2 AND MT2 
        where RT2=R2_1 AND ... AND R2_k2, MT2=M2_1 and ... and M2_l2.

    The function constructs the result tree according the formula
      (RT1 OR RT2) AND (MT1 OR RT1) AND (MT2 OR RT2) AND (MT1 OR MT2)
    that is equivalent to the formula (RT1 AND MT1) OR (RT2 AND MT2).

    To limit the number of produced imerges the function considers
    a weaker formula than the original one:
      (RT1 AND M1_1) OR (RT2 AND M2_1) 
    that is equivalent to:
      (RT1 OR RT2)                  (1)
        AND 
      (M1_1 OR M2_1)                (2)
        AND
      (M1_1 OR RT2)                 (3)
        AND
      (M2_1 OR RT1)                 (4)

    For the first conjunct (1) the function builds a tree with a range part
    and, possibly, one imerge. For the other conjuncts (2-4)the function
    produces sets of imerges. All constructed imerges are included into the
    result tree.
    
    For the formula (1) the function produces the tree representing a formula  
    of the structure RT [AND M], such that:
     - the range tree rt contains the result of oring SEL_ARG trees from rt1
       and rt2
     - the imerge m consists of two range trees rt1 and rt2.
    The imerge m is added if it's not true that rt1 and rt2 must be ored
    If rt1 and rt2 can't be ored rt is empty and only m is produced for (1).

    To produce imerges for the formula (2) the function calls the function
    imerge_list_or_list passing it the merge parts of tree1 and tree2 as
    parameters.

    To produce imerges for the formula (3) the function calls the function
    imerge_list_or_tree passing it the imerge m1_1 and the range tree rt2 as
    parameters. Similarly, to produce imerges for the formula (4) the function
    calls the function imerge_list_or_tree passing it the imerge m2_1 and the
    range tree rt1.

    If rt1 is empty then the trees for (1) and (4) are empty.
    If rt2 is empty then the trees for (1) and (3) are empty.
    If mt1 is empty then the trees for (2) and (3) are empty.
    If mt2 is empty then the trees for (2) and (4) are empty.

  RETURN
    The result tree for the operation if a success
    0 - otherwise
*/

static SEL_TREE *
tree_or(RANGE_OPT_PARAM *param,SEL_TREE *tree1,SEL_TREE *tree2)
{
  DBUG_ENTER("tree_or");
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

  SEL_TREE *result= NULL;
  key_map result_keys;
  key_map ored_keys;
  SEL_TREE *rtree[2]= {NULL,NULL};
  SEL_IMERGE *imerge[2]= {NULL, NULL};
  bool no_ranges1= tree1->without_ranges();
  bool no_ranges2= tree2->without_ranges();
  bool no_merges1= tree1->without_imerges();
  bool no_merges2= tree2->without_imerges();
  if (!no_ranges1 && !no_merges2)
  {
    rtree[0]= new SEL_TREE(tree1, TRUE, param);
    imerge[1]= new SEL_IMERGE(tree2->merges.head(), 0, param);
  }
  if (!no_ranges2 && !no_merges1)
  {
    rtree[1]= new SEL_TREE(tree2, TRUE, param);
    imerge[0]= new SEL_IMERGE(tree1->merges.head(), 0, param);
  }
  bool no_imerge_from_ranges= FALSE;
  if (!(result= new SEL_TREE()))
    DBUG_RETURN(result);

  /* Build the range part of the tree for the formula (1) */ 
  if (sel_trees_can_be_ored(param, tree1, tree2, &ored_keys))
  {
    bool must_be_ored= sel_trees_must_be_ored(param, tree1, tree2, ored_keys);
    no_imerge_from_ranges= must_be_ored;
    key_map::Iterator it(ored_keys);
    int key_no;
    while ((key_no= it++) != key_map::Iterator::BITMAP_END)
    {
      SEL_ARG *key1= tree1->keys[key_no];
      SEL_ARG *key2= tree2->keys[key_no];
      if (!must_be_ored)
      {
        key1->incr_refs();
        key2->incr_refs();
      }
      if ((result->keys[key_no]= key_or(param, key1, key2)))
        result->keys_map.set_bit(key_no);
    }
    result->type= tree1->type;
  }
      
  if (no_imerge_from_ranges && no_merges1 && no_merges2)
  {
    if (result->keys_map.is_clear_all())
      result->type= SEL_TREE::ALWAYS;
    DBUG_RETURN(result);
  }

  SEL_IMERGE *imerge_from_ranges;
  if (!(imerge_from_ranges= new SEL_IMERGE()))
    result= NULL;
  else if (!no_ranges1 && !no_ranges2 && !no_imerge_from_ranges)
  {
    /* Build the imerge part of the tree for the formula (1) */
    SEL_TREE *rt1= tree1;
    SEL_TREE *rt2= tree2;
    if (no_merges1)
      rt1= new SEL_TREE(tree1, TRUE, param);
    if (no_merges2)
      rt2= new SEL_TREE(tree2, TRUE, param);
    if (!rt1 || !rt2 ||
        result->merges.push_back(imerge_from_ranges) ||
        imerge_from_ranges->or_sel_tree(param, rt1) ||
        imerge_from_ranges->or_sel_tree(param, rt2))
      result= NULL;
  }
  if (!result)
    DBUG_RETURN(result);

  result->type= tree1->type;

  if (!no_merges1 && !no_merges2 && 
      !imerge_list_or_list(param, &tree1->merges, &tree2->merges))
  {
    /* Build the imerges for the formula (2) */
    imerge_list_and_list(&result->merges, &tree1->merges);
  }

  /* Build the imerges for the formulas (3) and (4) */
  for (uint i=0; i < 2; i++)
  {
    List<SEL_IMERGE> merges;
    SEL_TREE *rt= rtree[i];
    SEL_IMERGE *im= imerge[1-i];
    
    if (rt && im && !merges.push_back(im) && 
        !imerge_list_or_tree(param, &merges, rt))
      imerge_list_and_list(&result->merges, &merges);
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
    key1->right= key1->left= &null_element;
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
      if (param->alloced_sel_args > SEL_ARG::MAX_SEL_ARGS)
        break;
    }
    else
      next->next_key_part=key2;
  }
  if (!key1)
    return &null_element;			// Impossible ranges
  key1->use_count++;
  key1->max_part_no= max(key2->max_part_no, key2->part+1);
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
  if (!key1)
    return key2;
  if (!key2)
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
    /* TODO: why not leave one of the trees? */
    key1->free_tree();
    key2->free_tree();
    return 0;					// Can't optimize this
  }

  key1->use_count--;
  key2->use_count--;
  SEL_ARG *e1=key1->first(), *e2=key2->first(), *new_tree=0;
  uint max_part_no= max(key1->max_part_no, key2->max_part_no);

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
    e1->incr_refs();
    e2->incr_refs();
    if (!next || next->type != SEL_ARG::IMPOSSIBLE)
    {
      SEL_ARG *new_arg= e1->clone_and(e2);
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
  new_tree->max_part_no= max_part_no;
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
*/
static SEL_ARG *
key_or(RANGE_OPT_PARAM *param, SEL_ARG *key1,SEL_ARG *key2)
{
  if (!key1)
  {
    if (key2)
    {
      key2->use_count--;
      key2->free_tree();
    }
    return 0;
  }
  if (!key2)
  {
    key1->use_count--;
    key1->free_tree();
    return 0;
  }
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
    if (key1->use_count > 0 && !(key1=key1->clone_tree(param)))
      return 0;                                 // OOM
  }

  // Add tree at key2 to tree at key1
  bool key2_shared=key2->use_count != 0;
  key1->maybe_flag|=key2->maybe_flag;

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
        Example: a starts before b and may end before/the same plase/after b
        a: [----***]
        b:   [---]

      Adjacent ranges:
        Ranges that meet but do not overlap. Example: a = "x < 3", b = "x >= 3"
        a: ----]
        b:      [----
   */

  uint max_part_no= max(key1->max_part_no, key2->max_part_no);

  for (key2=key2->first(); key2; )
  {
    /*
      key1 consists of one or more ranges. tmp is the range currently
      being handled.

      initialize tmp to the latest range in key1 that starts the same
      place or before the range in key2 starts

      key2:           [------]
      key1: [---] [-----] [----]
                  ^
                  tmp
    */
    SEL_ARG *tmp=key1->find_range(key2);

    /*
      Used to describe how two key values are positioned compared to
      each other. Consider key_value_a.<cmp_func>(key_value_b):

        -2: key_value_a is smaller than key_value_b, and they are adjacent
        -1: key_value_a is smaller than key_value_b (not adjacent)
         0: the key values are equal
         1: key_value_a is bigger than key_value_b (not adjacent)
        -2: key_value_a is bigger than key_value_b, and they are adjacent

      Example: "cmp= tmp->cmp_max_to_min(key2)"

      key2:         [--------            (10 <= x ...)
      tmp:    -----]                      (... x <  10) => cmp==-2
      tmp:    ----]                       (... x <=  9) => cmp==-1
      tmp:    ------]                     (... x  = 10) => cmp== 0
      tmp:    --------]                   (... x <= 12) => cmp== 1
      (cmp == 2 does not make sense for cmp_max_to_min())
     */
    int cmp= 0;

    if (!tmp)
    {
      /*
        The range in key2 starts before the first range in key1. Use
        the first range in key1 as tmp.

        key2:     [--------]
        key1:            [****--] [----]   [-------]
                         ^
                         tmp
      */
      tmp=key1->first();
      cmp= -1;
    }
    else if ((cmp= tmp->cmp_max_to_min(key2)) < 0)
    {
      /*
        This is the case:
        key2:          [-------]
        tmp:   [----**]
       */
      SEL_ARG *next=tmp->next;
      if (cmp == -2 && eq_tree(tmp->next_key_part,key2->next_key_part))
      {
        /*
          Adjacent (cmp==-2) and equal next_key_parts => ranges can be merged

          This is the case:
          key2:          [-------]
          tmp:     [----]

          Result:
          key2:    [-------------]     => inserted into key1 below
          tmp:                         => deleted
        */
        SEL_ARG *key2_next=key2->next;
        if (key2_shared)
        {
          if (!(key2=new SEL_ARG(*key2)))
            return 0;           // out of memory
          key2->increment_use_count(key1->use_count+1);
          key2->next=key2_next;                 // New copy of key2
        }

        key2->copy_min(tmp);
        if (!(key1=key1->tree_delete(tmp)))
        {                                       // Only one key in tree
          key1=key2;
          key1->make_root();
          key2=key2_next;
          break;
        }
      }
      if (!(tmp=next)) // Move to next range in key1. Now tmp.min > key2.min
        break;         // No more ranges in key1. Copy rest of key2
    }

    if (cmp < 0)
    {
      /*
        This is the case:
        key2:  [--***]
        tmp:       [----]
      */
      int tmp_cmp;
      if ((tmp_cmp=tmp->cmp_min_to_max(key2)) > 0)
      {
        /*
          This is the case:
          key2:  [------**]
          tmp:             [----]
        */
        if (tmp_cmp == 2 && eq_tree(tmp->next_key_part,key2->next_key_part))
        {
          /*
            Adjacent ranges with equal next_key_part. Merge like this:

            This is the case:
            key2:    [------]
            tmp:             [-----]

            Result:
            key2:    [------]
            tmp:     [-------------]

            Then move on to next key2 range.
          */
          tmp->copy_min_to_min(key2);
          key1->merge_flags(key2);
          if (tmp->min_flag & NO_MIN_RANGE &&
              tmp->max_flag & NO_MAX_RANGE)
          {
            if (key1->maybe_flag)
              return new SEL_ARG(SEL_ARG::MAYBE_KEY);
            return 0;
          }
          key2->increment_use_count(-1);        // Free not used tree
          key2=key2->next;
          continue;
        }
        else
        {
          /*
            key2 not adjacent to tmp or has different next_key_part.
            Insert into key1 and move to next range in key2
            
            This is the case:
            key2:  [------**]
            tmp:             [----]

            Result:
            key1_  [------**][----]
                   ^         ^
                   insert    tmp
          */
          SEL_ARG *next=key2->next;
          if (key2_shared)
          {
            SEL_ARG *cpy= new SEL_ARG(*key2);   // Must make copy
            if (!cpy)
              return 0;                         // OOM
            key1=key1->insert(cpy);
            key2->increment_use_count(key1->use_count+1);
          }
          else
            key1=key1->insert(key2);            // Will destroy key2_root
          key2=next;
          continue;
        }
      }
    }

    /*
      The ranges in tmp and key2 are overlapping:

      key2:          [----------] 
      tmp:        [*****-----*****]

      Corollary: tmp.min <= key2.max
    */
    if (eq_tree(tmp->next_key_part,key2->next_key_part))
    {
      // Merge overlapping ranges with equal next_key_part
      if (tmp->is_same(key2))
      {
        /*
          Found exact match of key2 inside key1.
          Use the relevant range in key1.
        */
        tmp->merge_flags(key2);                 // Copy maybe flags
        key2->increment_use_count(-1);          // Free not used tree
      }
      else
      {
        SEL_ARG *last= tmp;
        SEL_ARG *first= tmp;

        /*
          Find the last range in key1 that overlaps key2 and
          where all ranges first...last have the same next_key_part as
          key2.

          key2:  [****----------------------*******]
          key1:     [--]  [----] [---]  [-----] [xxxx]
                    ^                   ^       ^
                    first               last    different next_key_part

          Since key2 covers them, the ranges between first and last
          are merged into one range by deleting first...last-1 from
          the key1 tree. In the figure, this applies to first and the
          two consecutive ranges. The range of last is then extended:
            * last.min: Set to min(key2.min, first.min)
            * last.max: If there is a last->next that overlaps key2 (i.e.,
                        last->next has a different next_key_part):
                                        Set adjacent to last->next.min
                        Otherwise:      Set to max(key2.max, last.max)

          Result:
          key2:  [****----------------------*******]
                    [--]  [----] [---]                   => deleted from key1
          key1:  [**------------------------***][xxxx]
                 ^                              ^
                 tmp=last                       different next_key_part
        */
        while (last->next && last->next->cmp_min_to_max(key2) <= 0 &&
               eq_tree(last->next->next_key_part,key2->next_key_part))
        {
          /*
            last->next is covered by key2 and has same next_key_part.
            last can be deleted
          */
          SEL_ARG *save=last;
          last=last->next;
          key1=key1->tree_delete(save);
        }
        // Redirect tmp to last which will cover the entire range
        tmp= last;

        /*
          We need the minimum endpoint of first so we can compare it
          with the minimum endpoint of the enclosing key2 range.
        */
        last->copy_min(first);
        bool full_range= last->copy_min(key2);
        if (!full_range)
        {
          if (last->next && key2->cmp_max_to_min(last->next) >= 0)
          {
            /*
              This is the case:
              key2:    [-------------]
              key1:  [***------]  [xxxx]
                     ^            ^
                     last         different next_key_part

              Extend range of last up to last->next:
              key2:    [-------------]
              key1:  [***--------][xxxx]
            */
            last->copy_min_to_max(last->next);
          }
          else
            /*
              This is the case:
              key2:    [--------*****]
              key1:  [***---------]    [xxxx]
                     ^                 ^
                     last              different next_key_part

              Extend range of last up to max(last.max, key2.max):
              key2:    [--------*****]
              key1:  [***----------**] [xxxx]
             */
            full_range= last->copy_max(key2);
        }
        if (full_range)
        {                                       // Full range
          key1->free_tree();
          for (; key2 ; key2=key2->next)
            key2->increment_use_count(-1);      // Free not used tree
          if (key1->maybe_flag)
            return new SEL_ARG(SEL_ARG::MAYBE_KEY);
          return 0;
        }
      }
    }

    if (cmp >= 0 && tmp->cmp_min_to_min(key2) < 0)
    {
      /*
        This is the case ("cmp>=0" means that tmp.max >= key2.min):
        key2:              [----]
        tmp:     [------------*****]
      */

      if (!tmp->next_key_part)
      {
        if (key2->use_count)
	{
	  SEL_ARG *key2_cpy= new SEL_ARG(*key2);
          if (key2_cpy)
            return 0;
          key2= key2_cpy;
	}
        /*
          tmp->next_key_part is empty: cut the range that is covered
          by tmp from key2. 
          Reason: (key2->next_key_part OR tmp->next_key_part) will be
          empty and therefore equal to tmp->next_key_part. Thus, this
          part of the key2 range is completely covered by tmp.
        */
        if (tmp->cmp_max_to_max(key2) >= 0)
        {
          /*
            tmp covers the entire range in key2. 
            key2:              [----]
            tmp:     [-----------------]

            Move on to next range in key2
          */
          key2->increment_use_count(-1); // Free not used tree
          key2=key2->next;
          continue;
        }
        else
        {
          /*
            This is the case:
            key2:           [-------]
            tmp:     [---------]

            Result:
            key2:               [---]
            tmp:     [---------]
          */
          key2->copy_max_to_min(tmp);
          continue;
        }
      }

      /*
        The ranges are overlapping but have not been merged because
        next_key_part of tmp and key2 differ. 
        key2:              [----]
        tmp:     [------------*****]

        Split tmp in two where key2 starts:
        key2:              [----]
        key1:    [--------][--*****]
                 ^         ^
                 insert    tmp
      */
      SEL_ARG *new_arg=tmp->clone_first(key2);
      if (!new_arg)
        return 0;                               // OOM
      if ((new_arg->next_key_part= tmp->next_key_part))
        new_arg->increment_use_count(key1->use_count+1);
      tmp->copy_min_to_min(key2);
      key1=key1->insert(new_arg);
    } // tmp.min >= key2.min due to this if()

    /*
      Now key2.min <= tmp.min <= key2.max:
      key2:   [---------]
      tmp:    [****---*****]
     */
    SEL_ARG key2_cpy(*key2); // Get copy we can modify
    for (;;)
    {
      if (tmp->cmp_min_to_min(&key2_cpy) > 0)
      {
        /*
          This is the case:
          key2_cpy:    [------------]
          key1:                 [-*****]
                                ^
                                tmp
                             
          Result:
          key2_cpy:             [---]
          key1:        [-------][-*****]
                       ^        ^
                       insert   tmp
         */
        SEL_ARG *new_arg=key2_cpy.clone_first(tmp);
        if (!new_arg)
          return 0; // OOM
        if ((new_arg->next_key_part=key2_cpy.next_key_part))
          new_arg->increment_use_count(key1->use_count+1);
        key1=key1->insert(new_arg);
        key2_cpy.copy_min_to_min(tmp);
      } 
      // Now key2_cpy.min == tmp.min

      if ((cmp= tmp->cmp_max_to_max(&key2_cpy)) <= 0)
      {
        /*
          tmp.max <= key2_cpy.max:
          key2_cpy:   a)  [-------]    or b)     [----]
          tmp:            [----]                 [----]

          Steps:
           1) Update next_key_part of tmp: OR it with key2_cpy->next_key_part.
           2) If case a: Insert range [tmp.max, key2_cpy.max] into key1 using
                         next_key_part of key2_cpy

           Result:
           key1:      a)  [----][-]    or b)     [----]
         */
        tmp->maybe_flag|= key2_cpy.maybe_flag;
        key2_cpy.increment_use_count(key1->use_count+1);
        tmp->next_key_part= key_or(param, tmp->next_key_part,
                                   key2_cpy.next_key_part);

        if (!cmp)
          break;                     // case b: done with this key2 range

        // Make key2_cpy the range [tmp.max, key2_cpy.max]
        key2_cpy.copy_max_to_min(tmp);
        if (!(tmp=tmp->next))
        {
          /*
            No more ranges in key1. Insert key2_cpy and go to "end"
            label to insert remaining ranges in key2 if any.
          */
          SEL_ARG *tmp2= new SEL_ARG(key2_cpy);
          if (!tmp2)
            return 0; // OOM
          key1=key1->insert(tmp2);
          key2=key2->next;
          goto end;
        }
        if (tmp->cmp_min_to_max(&key2_cpy) > 0)
        {
          /*
            The next range in key1 does not overlap with key2_cpy.
            Insert this range into key1 and move on to the next range
            in key2.
          */
          SEL_ARG *tmp2= new SEL_ARG(key2_cpy);
          if (!tmp2)
            return 0;                           // OOM
          key1=key1->insert(tmp2);
          break;
        }
        /*
          key2_cpy overlaps with the next range in key1 and the case
          is now "key2.min <= tmp.min <= key2.max". Go back to for(;;)
          to handle this situation.
        */
        continue;
      }
      else
      {
        /*
          This is the case:
          key2_cpy:   [-------]
          tmp:        [------------]

          Result:
          key1:       [-------][---]
                      ^        ^
                      new_arg  tmp
          Steps:
           0) If tmp->next_key_part is empty: do nothing. Reason:
              (key2_cpy->next_key_part OR tmp->next_key_part) will be
              empty and therefore equal to tmp->next_key_part. Thus,
              the range in key2_cpy is completely covered by tmp
           1) Make new_arg with range [tmp.min, key2_cpy.max].
              new_arg->next_key_part is OR between next_key_part
              of tmp and key2_cpy
           2) Make tmp the range [key2.max, tmp.max]
           3) Insert new_arg into key1
        */
        if (!tmp->next_key_part) // Step 0
        {
          key2_cpy.increment_use_count(-1);     // Free not used tree
          break;
        }
        SEL_ARG *new_arg=tmp->clone_last(&key2_cpy);
        if (!new_arg)
          return 0; // OOM
        tmp->copy_max_to_min(&key2_cpy);
        tmp->increment_use_count(key1->use_count+1);
        /* Increment key count as it may be used for next loop */
        key2_cpy.increment_use_count(1);
        new_arg->next_key_part= key_or(param, tmp->next_key_part,
                                       key2_cpy.next_key_part);
        key1=key1->insert(new_arg);
        break;
      }
    }
    // Move on to next range in key2
    key2=key2->next;                            
  }

end:
  /*
    Add key2 ranges that are non-overlapping with and higher than the
    highest range in key1.
  */
  while (key2)
  {
    SEL_ARG *next=key2->next;
    if (key2_shared)
    {
      SEL_ARG *tmp=new SEL_ARG(*key2);          // Must make copy
      if (!tmp)
        return 0;
      key2->increment_use_count(key1->use_count+1);
      key1=key1->insert(tmp);
    }
    else
      key1=key1->insert(key2);                  // Will destroy key2_root
    key2=next;
  }
  key1->use_count++;

  key1->max_part_no= max_part_no;
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
  SEL_ARG *element,**UNINIT_VAR(par),*UNINIT_VAR(last_element);

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
  key->increment_use_count(-1);
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
  test_rb_tree(root,root->parent);

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
  test_rb_tree(root,root->parent);
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


	/* Test that the properties for a red-black tree hold */

#ifdef EXTRA_DEBUG
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
*/

void SEL_ARG::test_use_count(SEL_ARG *root)
{
  uint e_count=0;

  if (this->type != SEL_ARG::KEY_RANGE)
    return;
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
	return;
      }
      pos->next_key_part->test_use_count(root);
    }
  }
  if (e_count != elements)
    sql_print_warning("Wrong use count: %u (should be %u) for tree at 0x%lx",
                      e_count, elements, (long unsigned int) this);
}
#endif

/*
  Calculate cost and E(#rows) for a given index and intervals tree 

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
                           uint *mrr_flags, uint *bufsize, COST_VECT *cost)
{
  SEL_ARG_RANGE_SEQ seq;
  RANGE_SEQ_IF seq_if = {NULL, sel_arg_range_seq_init, sel_arg_range_seq_next, 0, 0};
  handler *file= param->table->file;
  ha_rows rows= HA_POS_ERROR;
  uint keynr= param->real_keynr[idx];
  DBUG_ENTER("check_quick_select");
  
  /* Handle cases when we don't have a valid non-empty list of range */
  if (!tree)
    DBUG_RETURN(HA_POS_ERROR);
  if (tree->type == SEL_ARG::IMPOSSIBLE)
    DBUG_RETURN(0L);
  if (tree->type != SEL_ARG::KEY_RANGE || tree->part != 0)
    DBUG_RETURN(HA_POS_ERROR);

  seq.keyno= idx;
  seq.real_keyno= keynr;
  seq.param= param;
  seq.start= tree;

  param->range_count=0;
  param->max_key_part=0;

  param->is_ror_scan= TRUE;
  if (file->index_flags(keynr, 0, TRUE) & HA_KEY_SCAN_NOT_ROR)
    param->is_ror_scan= FALSE;
  
  *mrr_flags= param->force_default_mrr? HA_MRR_USE_DEFAULT_IMPL: 0;
  /*
    Pass HA_MRR_SORTED to see if MRR implementation can handle sorting.
  */
  *mrr_flags|= HA_MRR_NO_ASSOCIATION | HA_MRR_SORTED;

  bool pk_is_clustered= file->primary_key_is_clustered();
  if (index_only && 
      (file->index_flags(keynr, param->max_key_part, 1) & HA_KEYREAD_ONLY) &&
      !(file->index_flags(keynr, param->max_key_part, 1) & HA_CLUSTERED_INDEX))
     *mrr_flags |= HA_MRR_INDEX_ONLY;
  
  if (param->thd->lex->sql_command != SQLCOM_SELECT)
    *mrr_flags |= HA_MRR_USE_DEFAULT_IMPL;

  *bufsize= param->thd->variables.mrr_buff_size;
  /*
    Skip materialized derived table/view result table from MRR check as
    they aren't contain any data yet.
  */
  if (param->table->pos_in_table_list->is_non_derived())
    rows= file->multi_range_read_info_const(keynr, &seq_if, (void*)&seq, 0,
                                            bufsize, mrr_flags, cost);
  if (rows != HA_POS_ERROR)
  {
    param->quick_rows[keynr]= rows;
    if (update_tbl_stats)
    {
      param->table->quick_keys.set_bit(keynr);
      param->table->quick_key_parts[keynr]= param->max_key_part+1;
      param->table->quick_n_ranges[keynr]= param->range_count;
      param->table->quick_condition_rows=
        min(param->table->quick_condition_rows, rows);
      param->table->quick_rows[keynr]= rows;
    }
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
  else if (param->table->s->primary_key == keynr && pk_is_clustered)
  {
    /* Clustered PK scan is always a ROR scan (TODO: same as above) */
    param->is_ror_scan= TRUE;
  }
  else if (param->range_count > 1)
  {
    /* 
      Scaning multiple key values in the index: the records are ROR
      for each value, but not between values. E.g, "SELECT ... x IN
      (1,3)" returns ROR order for all records with x=1, then ROR
      order for records with x=3
    */
    param->is_ror_scan= FALSE;
  }

  DBUG_PRINT("exit", ("Records: %lu", (ulong) rows));
  DBUG_RETURN(rows); //psergey-merge:todo: maintain first_null_comp.
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

static bool is_key_scan_ror(PARAM *param, uint keynr, uint8 nparts)
{
  KEY *table_key= param->table->key_info + keynr;
  KEY_PART_INFO *key_part= table_key->key_part + nparts;
  KEY_PART_INFO *key_part_end= (table_key->key_part +
                                table_key->key_parts);
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

  key_part= table_key->key_part + nparts;
  pk_number= param->table->s->primary_key;
  if (!param->table->file->primary_key_is_clustered() || pk_number == MAX_KEY)
    return FALSE;

  KEY_PART_INFO *pk_part= param->table->key_info[pk_number].key_part;
  KEY_PART_INFO *pk_part_end= pk_part +
                              param->table->key_info[pk_number].key_parts;
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
                                      test(parent_alloc),
                                      parent_alloc, &create_err);
  else
    quick=new QUICK_RANGE_SELECT(param->thd, param->table,
                                 param->real_keynr[idx],
                                 test(parent_alloc), NULL, &create_err);

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
      KEY *keyinfo= param->table->key_info+param->real_keynr[idx];
      quick->mrr_flags= mrr_flags;
      quick->mrr_buf_size= mrr_buf_size;
      quick->key_parts=(KEY_PART*)
        memdup_root(parent_alloc? parent_alloc : &quick->alloc,
                    (char*) param->key[idx],
                    sizeof(KEY_PART)*
                    param->table->actual_n_key_parts(keyinfo));
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
      KEY *table_key=quick->head->key_info+quick->index;
      flag=EQ_RANGE;
      if ((table_key->flags & HA_NOSAME) &&
          min_part == key_tree->part &&
          key_tree->part == table_key->key_parts-1)
      {
        DBUG_ASSERT(min_part == max_part);
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
			       flag)))
    return 1;			// out of memory

  set_if_bigger(quick->max_used_key_length, range->min_length);
  set_if_bigger(quick->max_used_key_length, range->max_length);
  set_if_bigger(quick->used_key_parts, (uint) key_tree->part+1);
  if (insert_dynamic(&quick->ranges, (uchar*) &range))
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
  if (ranges.elements == 1)
  {
    QUICK_RANGE *tmp= *((QUICK_RANGE**)ranges.buffer);
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

bool QUICK_INDEX_SORT_SELECT::is_keys_used(const MY_BITMAP *fields)
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
  QUICK_SELECT_WITH_RECORD *qr;
  List_iterator_fast<QUICK_SELECT_WITH_RECORD> it(quick_selects);
  while ((qr= it++))
  {
    if (is_key_used(head, qr->quick->index, fields))
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
  COST_VECT cost;
  uint max_used_key_len;

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

  if ((cp_buffer_from_ref(thd, table, ref) && thd->is_fatal_error) ||
      !(range= new(alloc) QUICK_RANGE()))
    goto err;                                   // out of memory

  range->min_key= range->max_key= ref->key_buff;
  range->min_length= range->max_length= ref->key_length;
  range->min_keypart_map= range->max_keypart_map=
    make_prev_keypart_map(ref->key_parts);
  range->flag= EQ_RANGE;

  if (!(quick->key_parts=key_part=(KEY_PART *)
	alloc_root(&quick->alloc,sizeof(KEY_PART)*ref->key_parts)))
    goto err;
  
  max_used_key_len=0;
  for (part=0 ; part < ref->key_parts ;part++,key_part++)
  {
    key_part->part=part;
    key_part->field=        key_info->key_part[part].field;
    key_part->length=       key_info->key_part[part].length;
    key_part->store_length= key_info->key_part[part].store_length;
    key_part->null_bit=     key_info->key_part[part].null_bit;
    key_part->flag=         (uint8) key_info->key_part[part].key_part_flag;

    max_used_key_len +=key_info->key_part[part].store_length; 
  }

  quick->max_used_key_length= max_used_key_len;

  if (insert_dynamic(&quick->ranges,(uchar*)&range))
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
                      make_prev_keypart_map(ref->key_parts), EQ_RANGE)))
      goto err;
    *ref->null_ref_key= 0;		// Clear null byte
    if (insert_dynamic(&quick->ranges,(uchar*)&null_range))
      goto err;
  }

  /* Call multi_range_read_info() to get the MRR flags and buffer size */
  quick->mrr_flags= HA_MRR_NO_ASSOCIATION | 
                    (table->key_read ? HA_MRR_INDEX_ONLY : 0);
  if (thd->lex->sql_command != SQLCOM_SELECT)
    quick->mrr_flags |= HA_MRR_USE_DEFAULT_IMPL;

  quick->mrr_buf_size= thd->variables.mrr_buff_size;
  if (table->file->multi_range_read_info(quick->index, 1, (uint)records,
                                         ~0, 
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

int read_keys_and_merge_scans(THD *thd,
                              TABLE *head,
                              List<QUICK_RANGE_SELECT> quick_selects,
                              QUICK_RANGE_SELECT *pk_quick_select,
                              READ_RECORD *read_record,
                              bool intersection,
                              key_map *filtered_scans,
                              Unique **unique_ptr)
{
  List_iterator_fast<QUICK_RANGE_SELECT> cur_quick_it(quick_selects);
  QUICK_RANGE_SELECT* cur_quick;
  int result;
  Unique *unique= *unique_ptr;
  handler *file= head->file;
  bool with_cpk_filter= pk_quick_select != NULL;
  bool enabled_keyread= 0;
  DBUG_ENTER("read_keys_and_merge");

  /* We're going to just read rowids. */
  if (!head->key_read)
  {
    enabled_keyread= 1;
    head->enable_keyread();
  }
  head->prepare_for_position();

  cur_quick_it.rewind();
  cur_quick= cur_quick_it++;
  bool first_quick= TRUE;
  DBUG_ASSERT(cur_quick != 0);
  
  /*
    We reuse the same instance of handler so we need to call both init and 
    reset here.
  */
  if (cur_quick->init() || cur_quick->reset())
    goto err;

  if (unique == NULL)
  {
    DBUG_EXECUTE_IF("index_merge_may_not_create_a_Unique", DBUG_ABORT(); );
    DBUG_EXECUTE_IF("only_one_Unique_may_be_created", 
                    DBUG_SET("+d,index_merge_may_not_create_a_Unique"); );

    unique= new Unique(refpos_order_cmp, (void *)file,
                       file->ref_length,
                       thd->variables.sortbuff_size,
		       intersection ? quick_selects.elements : 0);                     
    if (!unique)
      goto err;
    *unique_ptr= unique;
  }
  else
  {
    unique->reset();
    filesort_free_buffers(head, false);
  }

  DBUG_ASSERT(file->ref_length == unique->get_size());
  DBUG_ASSERT(thd->variables.sortbuff_size == unique->get_max_in_memory_size());

  for (;;)
  {
    while ((result= cur_quick->get_next()) == HA_ERR_END_OF_FILE)
    {
      if (intersection)
        with_cpk_filter= filtered_scans->is_set(cur_quick->index);
      if (first_quick)
      {
        first_quick= FALSE;
        if (intersection && unique->is_in_memory())
          unique->close_for_expansion();
      }
      cur_quick->range_end();
      cur_quick= cur_quick_it++;
      if (!cur_quick)
        break;

      if (cur_quick->file->inited != handler::NONE) 
        cur_quick->file->ha_index_end();
      if (cur_quick->init() || cur_quick->reset())
        goto err;
    }

    if (result)
    {
      if (result != HA_ERR_END_OF_FILE)
      {
        cur_quick->range_end();
        goto err;
      }
      break;
    }

    if (thd->killed)
      goto err;

    if (with_cpk_filter &&
        pk_quick_select->row_in_ranges() != intersection )
      continue;

    cur_quick->file->position(cur_quick->record);
    if (unique->unique_add((char*)cur_quick->file->ref))
      goto err;
  }

  /*
    Ok all rowids are in the Unique now. The next call will initialize
    head->sort structure so it can be used to iterate through the rowids
    sequence.
  */
  result= unique->get(head);
  /*
    index merge currently doesn't support "using index" at all
  */
  if (enabled_keyread)
    head->disable_keyread();
  if (init_read_record(read_record, thd, head, (SQL_SELECT*) 0, 1 , 1, TRUE))
    result= 1;
 DBUG_RETURN(result);

err:
  if (enabled_keyread)
    head->disable_keyread();
  DBUG_RETURN(1);
}


int QUICK_INDEX_MERGE_SELECT::read_keys_and_merge()

{
  int result;
  DBUG_ENTER("QUICK_INDEX_MERGE_SELECT::read_keys_and_merge");
  result= read_keys_and_merge_scans(thd, head, quick_selects, pk_quick_select,
                                    &read_record, FALSE, NULL, &unique);
  doing_pk_scan= FALSE;
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

int QUICK_INDEX_INTERSECT_SELECT::read_keys_and_merge()

{
  int result;
  DBUG_ENTER("QUICK_INDEX_INTERSECT_SELECT::read_keys_and_merge");
  result= read_keys_and_merge_scans(thd, head, quick_selects, pk_quick_select,
                                    &read_record, TRUE, &filtered_scans,
                                    &unique);
  DBUG_RETURN(result);
}

int QUICK_INDEX_INTERSECT_SELECT::get_next()
{
  int result;
  DBUG_ENTER("QUICK_INDEX_INTERSECT_SELECT::get_next");

  if ((result= read_record.read_record(&read_record)) == -1)
  {
    result= HA_ERR_END_OF_FILE;
    end_read_record(&read_record);
    free_io_cache(head);
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
  List_iterator_fast<QUICK_SELECT_WITH_RECORD> quick_it(quick_selects);
  QUICK_SELECT_WITH_RECORD *qr;
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
    qr= quick_it++;
    quick= qr->quick;
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

    /* Save the read key tuple */
    key_copy(qr->key_tuple, record, head->key_info + quick->index,
             quick->max_used_key_length);

    quick->file->position(quick->record);
    memcpy(last_rowid, quick->file->ref, head->file->ref_length);
    last_rowid_count= 1;
    quick_with_last_rowid= quick;

    while (last_rowid_count < quick_selects.elements)
    {
      if (!(qr= quick_it++))
      {
        quick_it.rewind();
        qr= quick_it++;
      }
      quick= qr->quick;

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

      key_copy(qr->key_tuple, record, head->key_info + quick->index,
               quick->max_used_key_length);

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

        //save the fields here
        key_copy(qr->key_tuple, record, head->key_info + quick->index,
                 quick->max_used_key_length);
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

  if (!need_to_fetch_row)
  {
    /* Restore the columns we've read/saved with other quick selects */
    quick_it.rewind();
    while ((qr= quick_it++))
    {
      if (qr->quick != quick)
      {
        key_restore(record, qr->key_tuple, head->key_info + qr->quick->index,
                    qr->quick->max_used_key_length);
      }
    }
  }

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
      if (!queue.elements)
        DBUG_RETURN(HA_ERR_END_OF_FILE);
      /* Ok, we have a queue with >= 1 scans */

      quick= (QUICK_SELECT_I*)queue_top(&queue);
      memcpy(cur_rowid, quick->last_rowid, rowid_length);

      /* put into queue rowid from the same stream as top element */
      if ((error= quick->get_next()))
      {
        if (error != HA_ERR_END_OF_FILE)
          DBUG_RETURN(error);
        queue_remove_top(&queue);
      }
      else
      {
        quick->save_last_pos();
        queue_replace_top(&queue);
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
  MY_BITMAP * const save_read_set= head->read_set;
  MY_BITMAP * const save_write_set= head->write_set;
  DBUG_ENTER("QUICK_RANGE_SELECT::reset");
  last_range= NULL;
  cur_range= (QUICK_RANGE**) ranges.buffer;
  RANGE_SEQ_IF seq_funcs= {NULL, quick_range_seq_init, quick_range_seq_next, 0, 0};

  if (in_ror_merged_scan)
    head->column_bitmaps_set_no_signal(&column_bitmap, &column_bitmap);
  
  if (file->inited == handler::RND)
  {
    /* Handler could be left in this state by MRR */
    if ((error= file->ha_rnd_end()))
      DBUG_RETURN(error);
  }

  if (file->inited == handler::NONE)
  {
    DBUG_EXECUTE_IF("bug14365043_2",
                    DBUG_SET("+d,ha_index_init_fail"););
    if ((error= file->ha_index_init(index,1)))
    {
        file->print_error(error, MYF(0));
        goto err;
    }
  }

  /* Allocate buffer if we need one but haven't allocated it yet */
  if (mrr_buf_size && !mrr_buf_desc)
  {
    buf_size= mrr_buf_size;
    while (buf_size && !my_multi_malloc(MYF(MY_WME),
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
#ifdef HAVE_valgrind
    /*
      We need this until ndb will use the buffer efficiently
      (Now ndb stores  complete row in here, instead of only the used fields
      which gives us valgrind warnings in compare_record[])
    */
    bzero((char*) mrange_buff, buf_size);
#endif
  }

  if (!mrr_buf_desc)
    empty_buf.buffer= empty_buf.buffer_end= empty_buf.end_of_used_area= NULL;
 
  error= file->multi_range_read_init(&seq_funcs, (void*)this, ranges.elements,
                                     mrr_flags, mrr_buf_desc? mrr_buf_desc: 
                                                              &empty_buf);
err:
  /* Restore bitmaps set on entry */
  if (in_ror_merged_scan)
    head->column_bitmaps_set_no_signal(save_read_set, save_write_set);

  DBUG_RETURN(error);
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
  range_id_t dummy;
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

    uint count= ranges.elements - (cur_range - (QUICK_RANGE**) ranges.buffer);
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

    result= file->read_range_first(last_range->min_keypart_map ? &start_key : 0,
				   last_range->max_keypart_map ? &end_key : 0,
                                   test(last_range->flag & EQ_RANGE),
				   TRUE);
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

    uint count= ranges.elements - (cur_range - (QUICK_RANGE**) ranges.buffer);
    if (count == 0)
    {
      /* Ranges have already been used up before. None is left for read. */
      last_range= 0;
      DBUG_RETURN(HA_ERR_END_OF_FILE);
    }
    last_range= *(cur_range++);

    result= file->ha_index_read_map(record, last_range->min_key,
                                    last_range->min_keypart_map,
                                    (ha_rkey_function)(last_range->flag ^
                                                       GEOM_FLAG));
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
  uint min= 0;
  uint max= ranges.elements - 1;
  uint mid= (max + min)/2;

  while (min != max)
  {
    if (cmp_next(*(QUICK_RANGE**)dynamic_array_ptr(&ranges, mid)))
    {
      /* current row value > mid->max */
      min= mid + 1;
    }
    else
      max= mid;
    mid= (min + max) / 2;
  }
  res= *(QUICK_RANGE**)dynamic_array_ptr(&ranges, mid);
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
                                     uint used_key_parts_arg)
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
  mrr_buf_size= 0;

  QUICK_RANGE **pr= (QUICK_RANGE**)ranges.buffer;
  QUICK_RANGE **end_range= pr + ranges.elements;
  for (; pr!=end_range; pr++)
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
   *   - if it is an EQ_RANGE, which means that max key covers the entire
   *     key, go directly to the key and read through it (sorting backwards is
   *     same as sorting forwards)
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
                 used_key_parts <= head->key_info[index].key_parts) ? 
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

    key_range       start_key;
    start_key.key=    (const uchar*) last_range->min_key;
    start_key.length= last_range->min_length;
    start_key.flag=   ((last_range->flag & NEAR_MIN) ? HA_READ_AFTER_KEY :
                       (last_range->flag & EQ_RANGE) ?
                       HA_READ_KEY_EXACT : HA_READ_KEY_OR_NEXT);
    start_key.keypart_map= last_range->min_keypart_map;
    key_range       end_key;
    end_key.key=      (const uchar*) last_range->max_key;
    end_key.length=   last_range->max_length;
    end_key.flag=     (last_range->flag & NEAR_MAX ? HA_READ_BEFORE_KEY :
                       HA_READ_AFTER_KEY);
    end_key.keypart_map= last_range->max_keypart_map;
    result= file->prepare_range_scan((last_range->flag & NO_MIN_RANGE) ? NULL : &start_key,
                                     (last_range->flag & NO_MAX_RANGE) ? NULL : &end_key);
    if (result)
    {
      DBUG_RETURN(result);
    }

    if (last_range->flag & NO_MAX_RANGE)        // Read last record
    {
      int local_error;
      if ((local_error= file->ha_index_last(record)))
	DBUG_RETURN(local_error);		// Empty table
      if (cmp_prev(last_range) == 0)
	DBUG_RETURN(0);
      last_range= 0;                            // No match; go to next range
      continue;
    }

    if (last_range->flag & EQ_RANGE &&
        used_key_parts <= head->key_info[index].key_parts)

    {
      result= file->ha_index_read_map(record, last_range->max_key,
                                      last_range->max_keypart_map,
                                      HA_READ_KEY_EXACT);
    }
    else
    {
      DBUG_ASSERT(last_range->flag & NEAR_MAX ||
                  (last_range->flag & EQ_RANGE && 
                   used_key_parts > head->key_info[index].key_parts) ||
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
  QUICK_SELECT_DESC *new_quick= new QUICK_SELECT_DESC(this, used_key_parts_arg);
  if (new_quick == NULL)
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


void QUICK_SELECT_I::add_key_name(String *str, bool *first)
{
  KEY *key_info= head->key_info + index;

  if (*first)
    *first= FALSE;
  else
    str->append(',');
  str->append(key_info->name);
}
 

void QUICK_RANGE_SELECT::add_info_string(String *str)
{
  bool first= TRUE;
  
  add_key_name(str, &first);
}

void QUICK_INDEX_MERGE_SELECT::add_info_string(String *str)
{
  QUICK_RANGE_SELECT *quick;
  bool first= TRUE;
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);

  str->append(STRING_WITH_LEN("sort_union("));
  while ((quick= it++))
  {
    quick->add_key_name(str, &first);
  }
  if (pk_quick_select)
    pk_quick_select->add_key_name(str, &first);
  str->append(')');
}

void QUICK_INDEX_INTERSECT_SELECT::add_info_string(String *str)
{
  QUICK_RANGE_SELECT *quick;
  bool first= TRUE;
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);

  str->append(STRING_WITH_LEN("sort_intersect("));
  if (pk_quick_select)
    pk_quick_select->add_key_name(str, &first);
  while ((quick= it++))
  {
    quick->add_key_name(str, &first);
  }
  str->append(')');
}

void QUICK_ROR_INTERSECT_SELECT::add_info_string(String *str)
{
  bool first= TRUE;
  QUICK_SELECT_WITH_RECORD *qr;
  List_iterator_fast<QUICK_SELECT_WITH_RECORD> it(quick_selects);

  str->append(STRING_WITH_LEN("intersect("));
  while ((qr= it++))
  {
    qr->quick->add_key_name(str, &first);
  }
  if (cpk_quick)
    cpk_quick->add_key_name(str, &first);
  str->append(')');
}


void QUICK_ROR_UNION_SELECT::add_info_string(String *str)
{
  QUICK_SELECT_I *quick;
  bool first= TRUE;
  List_iterator_fast<QUICK_SELECT_I> it(quick_selects);

  str->append(STRING_WITH_LEN("union("));
  while ((quick= it++))
  {
    if (first)
      first= FALSE;
    else
      str->append(',');
    quick->add_info_string(str);
  }
  str->append(')');
}


void QUICK_SELECT_I::add_key_and_length(String *key_names,
                                        String *used_lengths,
                                        bool *first)
{
  char buf[64];
  uint length;
  KEY *key_info= head->key_info + index;

  if (*first)
    *first= FALSE;
  else
  {
    key_names->append(',');
    used_lengths->append(',');
  }
  key_names->append(key_info->name);
  length= longlong10_to_str(max_used_key_length, buf, 10) - buf;
  used_lengths->append(buf, length);
}


void QUICK_RANGE_SELECT::add_keys_and_lengths(String *key_names,
                                              String *used_lengths)
{
  bool first= TRUE;

  add_key_and_length(key_names, used_lengths, &first);
}

void QUICK_INDEX_MERGE_SELECT::add_keys_and_lengths(String *key_names,
                                                    String *used_lengths)
{
  QUICK_RANGE_SELECT *quick;
  bool first= TRUE;

  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);

  while ((quick= it++))
  {
    quick->add_key_and_length(key_names, used_lengths, &first);
  }

  if (pk_quick_select)
    pk_quick_select->add_key_and_length(key_names, used_lengths, &first);
}


void QUICK_INDEX_INTERSECT_SELECT::add_keys_and_lengths(String *key_names,
                                                        String *used_lengths)
{
  QUICK_RANGE_SELECT *quick;
  bool first= TRUE;

  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);

  if (pk_quick_select)
    pk_quick_select->add_key_and_length(key_names, used_lengths, &first);

  while ((quick= it++))
  {
    quick->add_key_and_length(key_names, used_lengths, &first);
  }
}

void QUICK_ROR_INTERSECT_SELECT::add_keys_and_lengths(String *key_names,
                                                      String *used_lengths)
{
  QUICK_SELECT_WITH_RECORD *qr;
  bool first= TRUE;

  List_iterator_fast<QUICK_SELECT_WITH_RECORD> it(quick_selects);

  while ((qr= it++))
  {
    qr->quick->add_key_and_length(key_names, used_lengths, &first);
  }
  if (cpk_quick)
    cpk_quick->add_key_and_length(key_names, used_lengths, &first);
}

void QUICK_ROR_UNION_SELECT::add_keys_and_lengths(String *key_names,
                                                  String *used_lengths)
{
  QUICK_SELECT_I *quick;
  bool first= TRUE;

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


void QUICK_RANGE_SELECT::add_used_key_part_to_set(MY_BITMAP *col_set)
{
  uint key_len;
  KEY_PART *part= key_parts;
  for (key_len=0; key_len < max_used_key_length; 
       key_len += (part++)->store_length)
  {
    bitmap_set_bit(col_set, part->field->field_index);
  }
}


void QUICK_GROUP_MIN_MAX_SELECT::add_used_key_part_to_set(MY_BITMAP *col_set)
{
  uint key_len;
  KEY_PART_INFO *part= index_info->key_part;
  for (key_len=0; key_len < max_used_key_length; 
       key_len += (part++)->store_length)
  {
    bitmap_set_bit(col_set, part->field->field_index);
  }
}


void QUICK_ROR_INTERSECT_SELECT::add_used_key_part_to_set(MY_BITMAP *col_set)
{
  List_iterator_fast<QUICK_SELECT_WITH_RECORD> it(quick_selects);
  QUICK_SELECT_WITH_RECORD *quick;
  while ((quick= it++))
  {
    quick->quick->add_used_key_part_to_set(col_set);
  }
}


void QUICK_INDEX_SORT_SELECT::add_used_key_part_to_set(MY_BITMAP *col_set)
{
  QUICK_RANGE_SELECT *quick;
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  while ((quick= it++))
  {
    quick->add_used_key_part_to_set(col_set);
  }
  if (pk_quick_select)
    pk_quick_select->add_used_key_part_to_set(col_set);
}


void QUICK_ROR_UNION_SELECT::add_used_key_part_to_set(MY_BITMAP *col_set)
{
  QUICK_SELECT_I *quick;
  List_iterator_fast<QUICK_SELECT_I> it(quick_selects);

  while ((quick= it++))
  {
    quick->add_used_key_part_to_set(col_set);
  }
}


/*******************************************************************************
* Implementation of QUICK_GROUP_MIN_MAX_SELECT
*******************************************************************************/

static inline uint get_field_keypart(KEY *index, Field *field);
static inline SEL_ARG * get_index_range_tree(uint index, SEL_TREE* range_tree,
                                             PARAM *param, uint *param_idx);
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
                               Field::imagetype image_type,
                               bool *has_min_max_fld, bool *has_other_fld);

static void
cost_group_min_max(TABLE* table, KEY *index_info, uint used_key_parts,
                   uint group_key_parts, SEL_TREE *range_tree,
                   SEL_ARG *index_tree, ha_rows quick_prefix_records,
                   bool have_min, bool have_max,
                   double *read_cost, ha_rows *records);


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
 @param  read_time Best read time so far (=table/index scan time)
 @return table read plan
   @retval NULL  Loose index scan not applicable or mem_root == NULL
   @retval !NULL Loose index scan table read plan
*/

static TRP_GROUP_MIN_MAX *
get_best_group_min_max(PARAM *param, SEL_TREE *tree, double read_time)
{
  THD *thd= param->thd;
  JOIN *join= thd->lex->current_select->join;
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

  DBUG_ENTER("get_best_group_min_max");

  /* Perform few 'cheap' tests whether this access method is applicable. */
  if (!join)
    DBUG_RETURN(NULL);        /* This is not a select statement. */
  if ((join->table_count != 1) ||  /* The query must reference one table. */
      (join->select_lex->olap == ROLLUP_TYPE)) /* Check (B3) for ROLLUP */
    DBUG_RETURN(NULL);
  if (table->s->keys == 0)        /* There are no indexes to use. */
    DBUG_RETURN(NULL);
  if (join->conds && join->conds->used_tables() & OUTER_REF_TABLE_BIT)
    DBUG_RETURN(NULL); /* Cannot execute with correlated conditions. */

  /* Check (SA1,SA4) and store the only MIN/MAX argument - the C attribute.*/
  if (join->make_sum_func_list(join->all_fields, join->fields_list, 1))
    DBUG_RETURN(NULL);

  List_iterator<Item> select_items_it(join->fields_list);
  is_agg_distinct = is_indexed_agg_distinct(join, &agg_distinct_flds);

  if ((!join->group_list) && /* Neither GROUP BY nor a DISTINCT query. */
      (!join->select_distinct) &&
      !is_agg_distinct)
    DBUG_RETURN(NULL);
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
        DBUG_RETURN(NULL);

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

  /* Check (SA7). */
  if (is_agg_distinct && (have_max || have_min))
  {
    DBUG_RETURN(NULL);
  }

  /* Check (SA5). */
  if (join->select_distinct)
  {
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
      DBUG_RETURN(NULL);
  }

  /*
    Check that table has at least one compound index such that the conditions
    (GA1,GA2) are all TRUE. If there is more than one such index, select the
    first one. Here we set the variables: group_prefix_len and index_info.
  */
  KEY *cur_index_info= table->key_info;
  KEY *cur_index_info_end= cur_index_info + table->s->keys;
  /* Cost-related variables for the best index so far. */
  double best_read_cost= DBL_MAX;
  ha_rows best_records= 0;
  SEL_ARG *best_index_tree= NULL;
  ha_rows best_quick_prefix_records= 0;
  uint best_param_idx= 0;

  const uint pk= param->table->s->primary_key;
  uint max_key_part;  
  SEL_ARG *cur_index_tree= NULL;
  ha_rows cur_quick_prefix_records= 0;
  uint cur_param_idx=MAX_KEY;

  for (uint cur_index= 0 ; cur_index_info != cur_index_info_end ;
       cur_index_info++, cur_index++)
  {
    KEY_PART_INFO *cur_part;
    KEY_PART_INFO *end_part; /* Last part for loops. */
    /* Last index part. */
    KEY_PART_INFO *last_part;
    KEY_PART_INFO *first_non_group_part;
    KEY_PART_INFO *first_non_infix_part;
    uint key_parts;
    uint key_infix_parts;
    uint cur_group_key_parts= 0;
    uint cur_group_prefix_len= 0;
    double cur_read_cost;
    ha_rows cur_records;
    key_map used_key_parts_map;
    uint cur_key_infix_len= 0;
    uchar cur_key_infix[MAX_KEY_LENGTH];
    uint cur_used_key_parts;
    
    /* Check (B1) - if current index is covering. */
    if (!table->covering_keys.is_set(cur_index))
      goto next_index;

    /*
      Unless extended keys can be used for cur_index:
      If the current storage manager is such that it appends the primary key to
      each index, then the above condition is insufficient to check if the
      index is covering. In such cases it may happen that some fields are
      covered by the PK index, but not by the current index. Since we can't
      use the concatenation of both indexes for index lookup, such an index
      does not qualify as covering in our case. If this is the case, below
      we check that all query fields are indeed covered by 'cur_index'.
    */
    if (cur_index_info->key_parts == table->actual_n_key_parts(cur_index_info)
        && pk < MAX_KEY && cur_index != pk &&
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
            !cur_field->part_of_key_not_clustered.is_set(cur_index))
          goto next_index;                  // Field was not part of key
      }
    }

    max_key_part= 0;
    used_key_parts_map.clear_all();

    /*
      Check (GA1) for GROUP BY queries.
    */
    if (join->group_list)
    {
      cur_part= cur_index_info->key_part;
      end_part= cur_part + table->actual_n_key_parts(cur_index_info);
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
          goto next_index;
      }
      /*
        This function is called on the precondition that the index is covering.
        Therefore if the GROUP BY list contains more elements than the index,
        these are duplicates. The GROUP BY list cannot be a prefix of the index.
      */
      if (cur_part == end_part && tmp_group)
        goto next_index;
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
      while (NULL != (item = (is_agg_distinct ?
             (Item *) agg_distinct_flds_it++ : select_items_it++)))
      {
        /* (SA5) already checked above. */
        item_field= (Item_field*) item->real_item(); 
        DBUG_ASSERT(item->real_item()->type() == Item::FIELD_ITEM);

        /* not doing loose index scan for derived tables */
        if (!item_field->field)
          goto next_index;

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
          goto next_index;
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
        goto next_index;
      min_max_arg_part= cur_index_info->key_part + key_part_nr - 1;
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
    key_parts= table->actual_n_key_parts(cur_index_info);
    last_part= cur_index_info->key_part + key_parts;
    first_non_group_part= (cur_group_key_parts < key_parts) ?
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
        uint dummy;
        SEL_ARG *index_range_tree= get_index_range_tree(cur_index, tree, param,
                                                        &dummy);
        if (!get_constant_key_infix(cur_index_info, index_range_tree,
                                    first_non_group_part, min_max_arg_part,
                                    last_part, thd, cur_key_infix, 
                                    &cur_key_infix_len,
                                    &first_non_infix_part))
          goto next_index;
      }
      else if (min_max_arg_part &&
               (min_max_arg_part - first_non_group_part > 0))
      {
        /*
          There is a gap but no range tree, thus no predicates at all for the
          non-group keyparts.
        */
        goto next_index;
      }
      else if (first_non_group_part && join->conds)
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
        if (join->conds->walk(&Item::find_item_in_field_list_processor, 0,
                              (uchar*) key_part_range))
          goto next_index;
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
          goto next_index;
      }
    }

    /**
      Test WA2:If there are conditions on a column C participating in
      MIN/MAX, those conditions must be conjunctions to all earlier
      keyparts. Otherwise, Loose Index Scan cannot be used.
    */
    if (tree && min_max_arg_item)
    {
      uint dummy;
      SEL_ARG *index_range_tree= get_index_range_tree(cur_index, tree, param,
                                                      &dummy);
      SEL_ARG *cur_range= NULL;
      if (get_sel_arg_for_keypart(min_max_arg_part->field,
                                  index_range_tree, &cur_range) ||
          (cur_range && cur_range->type != SEL_ARG::KEY_RANGE))
      {
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
      cur_index_tree= get_index_range_tree(cur_index, tree, param,
                                           &cur_param_idx);
      /* Check if this range tree can be used for prefix retrieval. */
      COST_VECT dummy_cost;
      uint mrr_flags= HA_MRR_USE_DEFAULT_IMPL;
      uint mrr_bufsize=0;
      cur_quick_prefix_records= check_quick_select(param, cur_param_idx,
                                                   FALSE /*don't care*/,
                                                   cur_index_tree, TRUE,
                                                   &mrr_flags, &mrr_bufsize,
                                                   &dummy_cost);
    }
    cost_group_min_max(table, cur_index_info, cur_used_key_parts,
                       cur_group_key_parts, tree, cur_index_tree,
                       cur_quick_prefix_records, have_min, have_max,
                       &cur_read_cost, &cur_records);
    /*
      If cur_read_cost is lower than best_read_cost use cur_index.
      Do not compare doubles directly because they may have different
      representations (64 vs. 80 bits).
    */
    if (cur_read_cost < best_read_cost - (DBL_EPSILON * cur_read_cost))
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

  next_index:;
  }
  if (!index_info) /* No usable index found. */
    DBUG_RETURN(NULL);

  /* Check (SA3) for the where clause. */
  bool has_min_max_fld= false, has_other_fld= false;
  if (join->conds && min_max_arg_item &&
      !check_group_min_max_predicates(join->conds, min_max_arg_item,
                                      (index_info->flags & HA_SPATIAL) ?
                                      Field::itMBR : Field::itRAW,
                                      &has_min_max_fld, &has_other_fld))
    DBUG_RETURN(NULL);

  /*
    Check (SA6) if clustered key is used
  */
  if (is_agg_distinct && index == table->s->primary_key &&
      table->file->primary_key_is_clustered())
    DBUG_RETURN(NULL);

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

    read_plan->read_cost= best_read_cost;
    read_plan->records=   best_records;
    if (read_time < best_read_cost && is_agg_distinct)
    {
      read_plan->read_cost= 0;
      read_plan->use_index_scan();
    }

    DBUG_PRINT("info",
               ("Returning group min/max plan: cost: %g, records: %lu",
                read_plan->read_cost, (ulong) read_plan->records));
  }

  DBUG_RETURN(read_plan);
}


/*
  Check that the MIN/MAX attribute participates only in range predicates
  with constants.

  SYNOPSIS
    check_group_min_max_predicates()
    cond            [in]  the expression tree being analyzed
    min_max_arg     [in]  the field referenced by the MIN/MAX function(s)
    image_type      [in]
    has_min_max_arg [out] true if the subtree being analyzed references
                          min_max_arg
    has_other_arg   [out] true if the subtree being analyzed references a
                          column other min_max_arg

  DESCRIPTION
    The function walks recursively over the cond tree representing a WHERE
    clause, and checks condition (SA3) - if a field is referenced by a MIN/MAX
    aggregate function, it is referenced only by one of the following
    predicates $FUNC$:
    {=, !=, <, <=, >, >=, between, is [not] null, multiple equal}.
    In addition the function checks that the WHERE condition is equivalent to
    "cond1 AND cond2" where :
    cond1 - does not use min_max_column at all.
    cond2 - is an AND/OR tree with leaves in form
    "$FUNC$(min_max_column[, const])".

  RETURN
    TRUE  if cond passes the test
    FALSE o/w
*/

static bool
check_group_min_max_predicates(Item *cond, Item_field *min_max_arg_item,
                               Field::imagetype image_type,
                               bool *has_min_max_arg, bool *has_other_arg)
{
  DBUG_ENTER("check_group_min_max_predicates");
  DBUG_ASSERT(cond && min_max_arg_item);

  cond= cond->real_item();
  Item::Type cond_type= cond->real_type();
  if (cond_type == Item::COND_ITEM) /* 'AND' or 'OR' */
  {
    DBUG_PRINT("info", ("Analyzing: %s", ((Item_func*) cond)->func_name()));
    List_iterator_fast<Item> li(*((Item_cond*) cond)->argument_list());
    Item *and_or_arg;
    Item_func::Functype func_type= ((Item_cond*) cond)->functype();
    bool has_min_max= false, has_other= false;
    while ((and_or_arg= li++))
    {
      /*
        The WHERE clause doesn't pass the condition if:
        (1) any subtree doesn't pass the condition or
        (2) the subtree passes the test, but it is an OR and it references both
            the min/max argument and other columns.
      */
      if (!check_group_min_max_predicates(and_or_arg, min_max_arg_item,     //1
                                          image_type,
                                          &has_min_max, &has_other) ||
          (func_type == Item_func::COND_OR_FUNC && has_min_max && has_other))//2
        DBUG_RETURN(FALSE);
    }
    *has_min_max_arg= has_min_max || *has_min_max_arg;
    *has_other_arg= has_other || *has_other_arg;
    DBUG_RETURN(TRUE);
  }

  /*
    Disallow loose index scan if the MIN/MAX argument field is referenced by
    a subquery in the WHERE clause.
  */

  if (unlikely(cond_type == Item::SUBSELECT_ITEM))
  {
    Item_subselect *subs_cond= (Item_subselect*) cond;
    if (subs_cond->is_correlated)
    {
      DBUG_ASSERT(subs_cond->upper_refs.elements > 0);
      List_iterator_fast<Item_subselect::Ref_to_outside>
        li(subs_cond->upper_refs);
      Item_subselect::Ref_to_outside *dep;
      while ((dep= li++))
      {
        if (dep->item->eq(min_max_arg_item, FALSE))
          DBUG_RETURN(FALSE);
      }
    }
    DBUG_RETURN(TRUE);
  }
  /*
    Subquery with IS [NOT] NULL
    TODO: Look into the cache_item and optimize it like we do for
    subselect's above
   */
  if (unlikely(cond_type == Item::CACHE_ITEM))
    DBUG_RETURN(cond->const_item());
  
  /*
    Condition of the form 'field' is equivalent to 'field <> 0' and thus
    satisfies the SA3 condition.
  */
  if (cond_type == Item::FIELD_ITEM)
  {
    DBUG_PRINT("info", ("Analyzing: %s", cond->full_name()));
    if (min_max_arg_item->eq((Item_field*)cond, 1))
      *has_min_max_arg= true;
    else
      *has_other_arg= true;
    DBUG_RETURN(TRUE);
  }

  /* We presume that at this point there are no other Items than functions. */
  DBUG_ASSERT(cond_type == Item::FUNC_ITEM);
  if (unlikely(cond_type != Item::FUNC_ITEM))   /* Safety */
    DBUG_RETURN(FALSE);
  
  /* Test if cond references only group-by or non-group fields. */
  Item_func *pred= (Item_func*) cond;
  Item_func::Functype pred_type= pred->functype();
  DBUG_PRINT("info", ("Analyzing: %s", pred->func_name()));
  if (pred_type == Item_func::MULT_EQUAL_FUNC)
  {
    /*
      Check that each field in a multiple equality is either a constant or
      it is a reference to the min/max argument, or it doesn't contain the
      min/max argument at all.
    */
    Item_equal_fields_iterator eq_it(*((Item_equal*)pred));
    Item *eq_item;
    bool has_min_max= false, has_other= false;
    while ((eq_item= eq_it++))
    {
      if (min_max_arg_item->eq(eq_item->real_item(), 1))
        has_min_max= true;
      else
        has_other= true;
    }
    *has_min_max_arg= has_min_max || *has_min_max_arg;
    *has_other_arg= has_other || *has_other_arg;
    DBUG_RETURN(!(has_min_max && has_other));
  }

  Item **arguments= pred->arguments();
  Item *cur_arg;
  bool has_min_max= false, has_other= false;
  for (uint arg_idx= 0; arg_idx < pred->argument_count (); arg_idx++)
  {
    cur_arg= arguments[arg_idx]->real_item();
    DBUG_PRINT("info", ("cur_arg: %s", cur_arg->full_name()));
    if (cur_arg->type() == Item::FIELD_ITEM)
    {
      if (min_max_arg_item->eq(cur_arg, 1)) 
      {
        has_min_max= true;
        /*
          If pred references the MIN/MAX argument, check whether pred is a range
          condition that compares the MIN/MAX argument with a constant.
        */
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
        bzero(args, 3 * sizeof(Item*));
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
              ((Field_str*) min_max_arg_item->field)->charset() !=
              pred->compare_collation())
             ||
             /*
               We can't always use indexes when comparing a string index to a
               number.
             */
             (args[1]->result_type() != STRING_RESULT &&
              min_max_arg_item->field->cmp_type() != args[1]->result_type())))
          DBUG_RETURN(FALSE);
      }
      else
        has_other= true;
    }
    else if (cur_arg->type() == Item::FUNC_ITEM)
    {
      if (!check_group_min_max_predicates(cur_arg, min_max_arg_item, image_type,
                                          &has_min_max, &has_other))
        DBUG_RETURN(FALSE);
    }
    else if (cur_arg->const_item() && !cur_arg->is_expensive())
    {
      /*
        For predicates of the form "const OP expr" we also have to check 'expr'
        to make a decision.
      */
      continue;
    }
    else
      DBUG_RETURN(FALSE);
    if(has_min_max && has_other)
      DBUG_RETURN(FALSE);
  }
  *has_min_max_arg= has_min_max || *has_min_max_arg;
  *has_other_arg= has_other || *has_other_arg;

  DBUG_RETURN(TRUE);
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
    /*
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
    Test conditions (NGA1, NGA2, NGA3) from get_best_group_min_max(). Namely,
    for each keypart field NG_i not in GROUP-BY, check that there is exactly one
    constant equality predicate among conds with the form (NG_i = const_ci) or
    (const_ci = NG_i).. In addition, there can only be one range when there is
    such a gap.
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
        to a "null value" (is_null_string) that may not always be long
        enough for a direct memcpy to a field.
      */
      DBUG_ASSERT (field_length > 0);
      *key_ptr= 1;
      bzero(key_ptr+1,field_length-1);
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

  for (part= index->key_part,
         end= part + field->table->actual_n_key_parts(index);
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
    param     [in]  PARAM from SQL_SELECT::test_quick_select
    param_idx [out] Index in the array PARAM::key that corresponds to 'index'

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

SEL_ARG * get_index_range_tree(uint index, SEL_TREE* range_tree, PARAM *param,
                               uint *param_idx)
{
  uint idx= 0; /* Index nr in param->key_parts */
  while (idx < param->keys)
  {
    if (index == param->real_keynr[idx])
      break;
    idx++;
  }
  *param_idx= idx;
  return(range_tree->keys[idx]);
}


/*
  Compute the cost of a quick_group_min_max_select for a particular index.

  SYNOPSIS
    cost_group_min_max()
    table                [in] The table being accessed
    index_info           [in] The index used to access the table
    used_key_parts       [in] Number of key parts used to access the index
    group_key_parts      [in] Number of index key parts in the group prefix
    range_tree           [in] Tree of ranges for all indexes
    index_tree           [in] The range tree for the current index
    quick_prefix_records [in] Number of records retrieved by the internally
			      used quick range select if any
    have_min             [in] True if there is a MIN function
    have_max             [in] True if there is a MAX function
    read_cost           [out] The cost to retrieve rows via this quick select
    records             [out] The number of rows retrieved

  DESCRIPTION
    This method computes the access cost of a TRP_GROUP_MIN_MAX instance and
    the number of rows returned. It updates this->read_cost and this->records.

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

void cost_group_min_max(TABLE* table, KEY *index_info, uint used_key_parts,
                        uint group_key_parts, SEL_TREE *range_tree,
                        SEL_ARG *index_tree, ha_rows quick_prefix_records,
                        bool have_min, bool have_max,
                        double *read_cost, ha_rows *records)
{
  ha_rows table_records;
  uint num_groups;
  uint num_blocks;
  uint keys_per_block;
  uint keys_per_group;
  uint keys_per_subgroup; /* Average number of keys in sub-groups */
                          /* formed by a key infix. */
  double p_overlap; /* Probability that a sub-group overlaps two blocks. */
  double quick_prefix_selectivity;
  double io_cost;
  double cpu_cost= 0; /* TODO: CPU cost of index_read calls? */
  DBUG_ENTER("cost_group_min_max");

  table_records= table->file->stats.records;
  keys_per_block= (table->file->stats.block_size / 2 /
                   (index_info->key_length + table->file->ref_length)
                        + 1);
  num_blocks= (uint)(table_records / keys_per_block) + 1;

  /* Compute the number of keys in a group. */
  keys_per_group= index_info->rec_per_key[group_key_parts - 1];
  if (keys_per_group == 0) /* If there is no statistics try to guess */
    /* each group contains 10% of all records */
    keys_per_group= (uint)(table_records / 10) + 1;
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
  { /*
      Compute the probability that two ends of a subgroup are inside
      different blocks.
    */
    keys_per_subgroup= index_info->rec_per_key[used_key_parts - 1];
    if (keys_per_subgroup >= keys_per_block) /* If a subgroup is bigger than */
      p_overlap= 1.0;       /* a block, it will overlap at least two blocks. */
    else
    {
      double blocks_per_group= (double) num_blocks / (double) num_groups;
      p_overlap= (blocks_per_group * (keys_per_subgroup - 1)) / keys_per_group;
      p_overlap= min(p_overlap, 1.0);
    }
    io_cost= (double) min(num_groups * (1 + p_overlap), num_blocks);
  }
  else
    io_cost= (keys_per_group > keys_per_block) ?
             (have_min && have_max) ? (double) (num_groups + 1) :
                                      (double) num_groups :
             (double) num_blocks;

  /*
    TODO: If there is no WHERE clause and no other expressions, there should be
    no CPU cost. We leave it here to make this cost comparable to that of index
    scan as computed in SQL_SELECT::test_quick_select().
  */
  cpu_cost= (double) num_groups / TIME_FOR_COMPARE;

  *read_cost= io_cost + cpu_cost;
  *records= num_groups;

  DBUG_PRINT("info",
             ("table rows: %lu  keys/block: %u  keys/group: %u  result rows: %lu  blocks: %u",
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
                                        param->thd->lex->current_select->join,
                                        have_min, have_max, 
                                        have_agg_distinct, min_max_arg_part,
                                        group_prefix_len, group_key_parts,
                                        used_key_parts, index_info, index,
                                        read_cost, records, key_infix_len,
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
      /* Make a QUICK_RANGE_SELECT to be used for group prefix retrieval. */
      quick->quick_prefix_select= get_quick_select(param, param_idx,
                                                   index_tree,
                                                   HA_MRR_USE_DEFAULT_IMPL, 0,
                                                   &quick->alloc);

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
                           uint use_index, double read_cost_arg,
                           ha_rows records_arg, uint key_infix_len_arg,
                           uchar *key_infix_arg, MEM_ROOT *parent_alloc,
                           bool is_index_scan_arg)
  :file(table->file), join(join_arg), index_info(index_info_arg),
   group_prefix_len(group_prefix_len_arg),
   group_key_parts(group_key_parts_arg), have_min(have_min_arg),
   have_max(have_max_arg), have_agg_distinct(have_agg_distinct_arg),
   seen_first_key(FALSE), doing_key_read(FALSE), min_max_arg_part(min_max_arg_part_arg),
   key_infix(key_infix_arg), key_infix_len(key_infix_len_arg),
   min_functions_it(NULL), max_functions_it(NULL),
   is_index_scan(is_index_scan_arg)
{
  head=       table;
  index=      use_index;
  record=     head->record[0];
  tmp_record= head->record[1];
  read_time= read_cost_arg;
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
    init_sql_alloc(&alloc, join->thd->variables.range_alloc_block_size, 0);
    join->thd->mem_root= &alloc;
  }
  else
    bzero(&alloc, sizeof(MEM_ROOT));            // ensure that it's not used
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
    if (my_init_dynamic_array(&min_max_ranges, sizeof(QUICK_RANGE*), 16, 16))
      return 1;

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
  else
    min_max_ranges.elements= 0;

  return 0;
}


QUICK_GROUP_MIN_MAX_SELECT::~QUICK_GROUP_MIN_MAX_SELECT()
{
  DBUG_ENTER("QUICK_GROUP_MIN_MAX_SELECT::~QUICK_GROUP_MIN_MAX_SELECT");
  if (file->inited != handler::NONE) 
  {
    DBUG_ASSERT(file == head->file);
    if (doing_key_read)
      head->disable_keyread();
    /*
      There may be a code path when the same table was first accessed by index,
      then the index is closed, and the table is scanned (order by + loose scan).
    */
    file->ha_index_or_rnd_end();
  }
  if (min_max_arg_part)
    delete_dynamic(&min_max_ranges);
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
    else if (memcmp(sel_range->min_value, sel_range->max_value,
                    min_max_arg_len) == 0)
      range_flag|= EQ_RANGE;  /* equality condition */
  }
  range= new QUICK_RANGE(sel_range->min_value, min_max_arg_len,
                         make_keypart_map(sel_range->part),
                         sel_range->max_value, min_max_arg_len,
                         make_keypart_map(sel_range->part),
                         range_flag);
  if (!range)
    return TRUE;
  if (insert_dynamic(&min_max_ranges, (uchar*)&range))
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
    DYNAMIC_ARRAY *arr;
    uint inx;

    for (inx= 0, arr= &quick_prefix_select->ranges; inx < arr->elements; inx++)
    {
      QUICK_RANGE *range;

      get_dynamic(arr, (uchar*)&range, inx);
      range->flag &= ~(NEAR_MIN | NEAR_MAX);
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
  if (min_max_ranges.elements > 0)
  {
    QUICK_RANGE *cur_range;
    if (have_min)
    { /* Check if the right-most range has a lower boundary. */
      get_dynamic(&min_max_ranges, (uchar*)&cur_range,
                  min_max_ranges.elements - 1);
      if (!(cur_range->flag & NO_MIN_RANGE))
      {
        max_used_key_length+= min_max_arg_len;
        used_key_parts++;
        return;
      }
    }
    if (have_max)
    { /* Check if the left-most range has an upper boundary. */
      get_dynamic(&min_max_ranges, (uchar*)&cur_range, 0);
      if (!(cur_range->flag & NO_MAX_RANGE))
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

  seen_first_key= FALSE;
  if (!head->key_read)
  {
    doing_key_read= 1;
    head->enable_keyread(); /* We need only the key attributes */
  }
  if ((result= file->ha_index_init(index,1)))
  {
    head->file->print_error(result, MYF(0));
    DBUG_RETURN(result);
  }
  if (quick_prefix_select && quick_prefix_select->reset())
    DBUG_RETURN(1);
  result= file->ha_index_last(record);
  if (result == HA_ERR_END_OF_FILE)
    DBUG_RETURN(0);
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
#ifdef HPUX11
  /*
    volatile is required by a bug in the HP compiler due to which the
    last test of result fails.
  */
  volatile int result;
#else
  int result;
#endif
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
      result= file->ha_index_read_map(record, group_prefix,
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
  if (min_max_ranges.elements > 0)
  {
    if ((result= next_min_in_range()))
      DBUG_RETURN(result);
  }
  else
  {
    /* Apply the constant equality conditions to the non-group select fields */
    if (key_infix_len > 0)
    {
      if ((result=
           file->ha_index_read_map(record, group_prefix,
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
      uchar *tmp_key_buff= (uchar*)my_alloca(max_used_key_length);
      /* Find the first subsequent record without NULL in the MIN/MAX field. */
      key_copy(tmp_key_buff, record, index_info, max_used_key_length);
      result= file->ha_index_read_map(record, tmp_key_buff,
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
          key_restore(record, tmp_key_buff, index_info, 0);
      }
      else if (result == HA_ERR_KEY_NOT_FOUND || result == HA_ERR_END_OF_FILE)
        result= 0; /* There is a result in any case. */
      my_afree(tmp_key_buff);
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
  if (min_max_ranges.elements > 0)
    result= next_max_in_range();
  else
    result= file->ha_index_read_map(record, group_prefix,
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
      result= file->ha_index_first(record);
      if (result)
        DBUG_RETURN(result);
      seen_first_key= TRUE;
    }
    else
    {
      /* Load the first key in this group into record. */
      result= index_next_different (is_index_scan, file, index_info->key_part,
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
  QUICK_RANGE *cur_range;
  bool found_null= FALSE;
  int result= HA_ERR_KEY_NOT_FOUND;

  DBUG_ASSERT(min_max_ranges.elements > 0);

  for (uint range_idx= 0; range_idx < min_max_ranges.elements; range_idx++)
  { /* Search from the left-most range to the right. */
    get_dynamic(&min_max_ranges, (uchar*)&cur_range, range_idx);

    /*
      If the current value for the min/max argument is bigger than the right
      boundary of cur_range, there is no need to check this range.
    */
    if (range_idx != 0 && !(cur_range->flag & NO_MAX_RANGE) &&
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

    result= file->ha_index_read_map(record, group_prefix, keypart_map,
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
      my_afree(max_key);
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
  QUICK_RANGE *cur_range;
  int result;

  DBUG_ASSERT(min_max_ranges.elements > 0);

  for (uint range_idx= min_max_ranges.elements; range_idx > 0; range_idx--)
  { /* Search from the right-most range to the left. */
    get_dynamic(&min_max_ranges, (uchar*)&cur_range, range_idx - 1);

    /*
      If the current value for the min/max argument is smaller than the left
      boundary of cur_range, there is no need to check this range.
    */
    if (range_idx != min_max_ranges.elements &&
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

    result= file->ha_index_read_map(record, group_prefix, keypart_map,
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
      my_afree(min_key);
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
  bool first= TRUE;

  add_key_and_length(key_names, used_lengths, &first);
}


#ifndef DBUG_OFF

static void print_sel_tree(PARAM *param, SEL_TREE *tree, key_map *tree_map,
                           const char *msg)
{
  SEL_ARG **key,**end;
  int idx;
  char buff[1024];
  DBUG_ENTER("print_sel_tree");

  String tmp(buff,sizeof(buff),&my_charset_bin);
  tmp.length(0);
  for (idx= 0,key=tree->keys, end=key+param->keys ;
       key != end ;
       key++,idx++)
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

  DBUG_PRINT("info", ("SEL_TREE: 0x%lx (%s)  scans: %s", (long) tree, msg,
                      tmp.c_ptr_safe()));

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
  DBUG_PRINT("info", ("ROR key scans (%s): %s", msg, tmp.c_ptr()));
  DBUG_VOID_RETURN;
}


/*****************************************************************************
** Print a quick range for debugging
** TODO:
** This should be changed to use a String to store each row instead
** of locking the DEBUG stream !
*****************************************************************************/

static void
print_key(KEY_PART *key_part, const uchar *key, uint used_length)
{
  char buff[1024];
  const uchar *key_end= key+used_length;
  uint store_length;
  TABLE *table= key_part->field->table;
  my_bitmap_map *old_sets[2];

  dbug_tmp_use_all_columns(table, old_sets, table->read_set, table->write_set);

  for (; key < key_end; key+=store_length, key_part++)
  {
    String tmp(buff,sizeof(buff),&my_charset_bin);
    Field *field=      key_part->field;
    store_length= key_part->store_length;

    if (field->real_maybe_null())
    {
      if (*key)
      {
	fwrite("NULL",sizeof(char),4,DBUG_FILE);
	continue;
      }
      key++;					// Skip null byte
      store_length--;
    }
    field->set_key_image(key, key_part->length);
    if (field->type() == MYSQL_TYPE_BIT)
      (void) field->val_int_as_str(&tmp, 1);
    else
      field->val_str(&tmp);
    fwrite(tmp.ptr(),sizeof(char),tmp.length(),DBUG_FILE);
    if (key+store_length < key_end)
      fputc('/',DBUG_FILE);
  }
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
    QUICK_RANGE *range;
    QUICK_RANGE **pr= (QUICK_RANGE**)ranges.buffer;
    QUICK_RANGE **end_range= pr + ranges.elements;
    for (; pr != end_range; ++pr)
    {
      fprintf(DBUG_FILE, "%*s", indent + 2, "");
      range= *pr;
      if (!(range->flag & NO_MIN_RANGE))
      {
        print_key(key_parts, range->min_key, range->min_length);
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
        print_key(key_parts, range->max_key, range->max_length);
      }
      fputs("\n",DBUG_FILE);
    }
  }
  /* purecov: end */    
}

void QUICK_INDEX_SORT_SELECT::dbug_dump(int indent, bool verbose)
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
  List_iterator_fast<QUICK_SELECT_WITH_RECORD> it(quick_selects);
  QUICK_SELECT_WITH_RECORD *qr;
  fprintf(DBUG_FILE, "%*squick ROR-intersect select, %scovering\n",
          indent, "", need_to_fetch_row? "":"non-");
  fprintf(DBUG_FILE, "%*smerged scans {\n", indent, "");
  while ((qr= it++))
    qr->quick->dbug_dump(indent+2, verbose);
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
  if (min_max_ranges.elements > 0)
  {
    fprintf(DBUG_FILE, "%*susing %d quick_ranges for MIN/MAX:\n",
            indent, "", min_max_ranges.elements);
  }
}


#endif /* !DBUG_OFF */

/*****************************************************************************
** Instantiate templates 
*****************************************************************************/

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class List<QUICK_RANGE>;
template class List_iterator<QUICK_RANGE>;
#endif
