/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  TODO:
  Fix that MAYBE_KEY are stored in the tree so that we can detect use
  of full hash keys for queries like:

  select s.id, kws.keyword_id from sites as s,kws where s.id=kws.site_id and kws.keyword_id in (204,205);

*/

/*
  Classes in this file are used in the following way:
  1. For a selection condition a tree of SEL_IMERGE/SEL_TREE/SEL_ARG objects 
     is created. #of rows in table and index statistics are ignored at this 
     step.
  2. Created SEL_TREE and index stats data are used to construct a 
     TABLE_READ_PLAN-derived object (TRP_*). Several 'candidate' table read 
     plans may be created. 
  3. The least expensive table read plan is used to create a tree of 
     QUICK_SELECT_I-derived objects which are later used for row retrieval.
     QUICK_RANGEs are also created in this step.
*/

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include <m_ctype.h>
#include <nisam.h>
#include "sql_select.h"

#ifndef EXTRA_DEBUG
#define test_rb_tree(A,B) {}
#define test_use_count(A) {}
#endif


static int sel_cmp(Field *f,char *a,char *b,uint8 a_flag,uint8 b_flag);

static char is_null_string[2]= {1,0};

class SEL_ARG :public Sql_alloc
{
public:
  uint8 min_flag,max_flag,maybe_flag;
  uint8 part;					// Which key part
  uint8 maybe_null;
  uint16 elements;				// Elements in tree
  ulong use_count;				// use of this sub_tree
  Field *field;
  char *min_value,*max_value;			// Pointer to range

  SEL_ARG *left,*right,*next,*prev,*parent,*next_key_part;
  enum leaf_color { BLACK,RED } color;
  enum Type { IMPOSSIBLE, MAYBE, MAYBE_KEY, KEY_RANGE } type;

  SEL_ARG() {}
  SEL_ARG(SEL_ARG &);
  SEL_ARG(Field *,const char *,const char *);
  SEL_ARG(Field *field, uint8 part, char *min_value, char *max_value,
	  uint8 min_flag, uint8 max_flag, uint8 maybe_flag);
  SEL_ARG(enum Type type_arg)
    :elements(1),use_count(1),left(0),next_key_part(0),color(BLACK),
     type(type_arg)
  {}
  inline bool is_same(SEL_ARG *arg)
  {
    if (type != arg->type || part != arg->part)
      return 0;
    if (type != KEY_RANGE)
      return 1;
    return cmp_min_to_min(arg) == 0 && cmp_max_to_max(arg) == 0;
  }
  inline void merge_flags(SEL_ARG *arg) { maybe_flag|=arg->maybe_flag; }
  inline void maybe_smaller() { maybe_flag=1; }
  inline int cmp_min_to_min(SEL_ARG* arg)
  {
    return sel_cmp(field,min_value, arg->min_value, min_flag, arg->min_flag);
  }
  inline int cmp_min_to_max(SEL_ARG* arg)
  {
    return sel_cmp(field,min_value, arg->max_value, min_flag, arg->max_flag);
  }
  inline int cmp_max_to_max(SEL_ARG* arg)
  {
    return sel_cmp(field,max_value, arg->max_value, max_flag, arg->max_flag);
  }
  inline int cmp_max_to_min(SEL_ARG* arg)
  {
    return sel_cmp(field,max_value, arg->min_value, max_flag, arg->min_flag);
  }
  SEL_ARG *clone_and(SEL_ARG* arg)
  {						// Get overlapping range
    char *new_min,*new_max;
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
  SEL_ARG *clone(SEL_ARG *new_parent,SEL_ARG **next);

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
  void store_min(uint length,char **min_key,uint min_key_flag)
  {
    if ((min_flag & GEOM_FLAG) ||
        (!(min_flag & NO_MIN_RANGE) &&
	!(min_key_flag & (NO_MIN_RANGE | NEAR_MIN))))
    {
      if (maybe_null && *min_value)
      {
	**min_key=1;
	bzero(*min_key+1,length);
      }
      else
	memcpy(*min_key,min_value,length+(int) maybe_null);
      (*min_key)+= length+(int) maybe_null;
    }
  }
  void store(uint length,char **min_key,uint min_key_flag,
	     char **max_key, uint max_key_flag)
  {
    if ((min_flag & GEOM_FLAG) ||
        (!(min_flag & NO_MIN_RANGE) &&
	!(min_key_flag & (NO_MIN_RANGE | NEAR_MIN))))
    {
      if (maybe_null && *min_value)
      {
	**min_key=1;
	bzero(*min_key+1,length);
      }
      else
	memcpy(*min_key,min_value,length+(int) maybe_null);
      (*min_key)+= length+(int) maybe_null;
    }
    if (!(max_flag & NO_MAX_RANGE) &&
	!(max_key_flag & (NO_MAX_RANGE | NEAR_MAX)))
    {
      if (maybe_null && *max_value)
      {
	**max_key=1;
	bzero(*max_key+1,length);
      }
      else
	memcpy(*max_key,max_value,length+(int) maybe_null);
      (*max_key)+= length+(int) maybe_null;
    }
  }

  void store_min_key(KEY_PART *key,char **range_key, uint *range_key_flag)
  {
    SEL_ARG *key_tree= first();
    key_tree->store(key[key_tree->part].part_length,
		    range_key,*range_key_flag,range_key,NO_MAX_RANGE);
    *range_key_flag|= key_tree->min_flag;
    if (key_tree->next_key_part &&
	key_tree->next_key_part->part == key_tree->part+1 &&
	!(*range_key_flag & (NO_MIN_RANGE | NEAR_MIN)) &&
	key_tree->next_key_part->type == SEL_ARG::KEY_RANGE)
      key_tree->next_key_part->store_min_key(key,range_key, range_key_flag);
  }

  void store_max_key(KEY_PART *key,char **range_key, uint *range_key_flag)
  {
    SEL_ARG *key_tree= last();
    key_tree->store(key[key_tree->part].part_length,
		    range_key, NO_MIN_RANGE, range_key,*range_key_flag);
    (*range_key_flag)|= key_tree->max_flag;
    if (key_tree->next_key_part &&
	key_tree->next_key_part->part == key_tree->part+1 &&
	!(*range_key_flag & (NO_MAX_RANGE | NEAR_MAX)) &&
	key_tree->next_key_part->type == SEL_ARG::KEY_RANGE)
      key_tree->next_key_part->store_max_key(key,range_key, range_key_flag);
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
  SEL_ARG *clone_tree();
};

class SEL_IMERGE;

class SEL_TREE :public Sql_alloc
{
public:
  enum Type { IMPOSSIBLE, ALWAYS, MAYBE, KEY, KEY_SMALLER } type;
  SEL_TREE(enum Type type_arg) :type(type_arg) {}
  SEL_TREE() :type(KEY) 
  {
    keys_map.clear_all(); 
    bzero((char*) keys,sizeof(keys));
  }
  SEL_ARG *keys[MAX_KEY];
  key_map keys_map;        /* bitmask of non-NULL elements in keys */

  /* 
    Possible ways to read rows using index_merge. The list is non-empty only 
    if type==KEY. Currently can be non empty only if keys_map.is_clear_all().
  */
  List<SEL_IMERGE> merges;
  
  /* The members below are filled/used only after get_mm_tree is done */
  key_map ror_scans_map;   /* bitmask of ROR scan-able elements in keys */
  uint    n_ror_scans;

  struct st_ror_scan_info **ror_scans;     /* list of ROR key scans */
  struct st_ror_scan_info **ror_scans_end; /* last ROR scan */
  /* Note that #records for each key scan is stored in table->quick_rows */
};


typedef struct st_qsel_param {
  THD	*thd;
  TABLE *table;
  KEY_PART *key_parts,*key_parts_end,*key[MAX_KEY];
  MEM_ROOT *mem_root;
  table_map prev_tables,read_tables,current_table;
  uint baseflag, max_key_part, range_count;
    
  uint keys; /* number of keys used in the query */

  /* used_key_no -> table_key_no translation table */
  uint real_keynr[MAX_KEY]; 

  char min_key[MAX_KEY_LENGTH+MAX_FIELD_WIDTH],
    max_key[MAX_KEY_LENGTH+MAX_FIELD_WIDTH];
  bool quick;				// Don't calulate possible keys

  uint fields_bitmap_size;    
  MY_BITMAP needed_fields;    /* bitmask of fields needed by the query */

  key_map *needed_reg;        /* ptr to SQL_SELECT::needed_reg */

  uint *imerge_cost_buff;     /* buffer for index_merge cost estimates */
  uint imerge_cost_buff_size; /* size of the buffer */
  
  bool is_ror_scan; /* true if last checked tree->key can be used for ROR-scan */
} PARAM;

class TABLE_READ_PLAN;
  class TRP_RANGE;
  class TRP_ROR_INTERSECT;
  class TRP_ROR_UNION;
  class TRP_ROR_INDEX_MERGE;

struct st_ror_scan_info;

static SEL_TREE * get_mm_parts(PARAM *param,Field *field,
			       Item_func::Functype type,Item *value,
			       Item_result cmp_type);
static SEL_ARG *get_mm_leaf(PARAM *param,Field *field,KEY_PART *key_part,
			    Item_func::Functype type,Item *value);
static SEL_TREE *get_mm_tree(PARAM *param,COND *cond);

static bool is_key_scan_ror(PARAM *param, uint keynr, uint8 nparts);
static ha_rows check_quick_select(PARAM *param,uint index,SEL_ARG *key_tree);
static ha_rows check_quick_keys(PARAM *param,uint index,SEL_ARG *key_tree,
				char *min_key,uint min_key_flag,
				char *max_key, uint max_key_flag);

QUICK_RANGE_SELECT *get_quick_select(PARAM *param,uint index,
                                     SEL_ARG *key_tree, 
                                     MEM_ROOT *alloc = NULL);
static TRP_RANGE *get_key_scans_params(PARAM *param, SEL_TREE *tree,
                                       bool index_read_must_be_used, 
                                       double read_time);
static
TRP_ROR_INTERSECT *get_best_ror_intersect(const PARAM *param, SEL_TREE *tree,
                                          bool force_index_only, 
                                          double read_time,
                                          bool *are_all_covering);
static
TRP_ROR_INTERSECT *get_best_covering_ror_intersect(PARAM *param, 
                                                   SEL_TREE *tree, 
                                                   double read_time);
static
TABLE_READ_PLAN *get_best_disjunct_quick(PARAM *param, SEL_IMERGE *imerge,
                                         double read_time);
static int get_index_merge_params(PARAM *param, key_map& needed_reg,
                           SEL_IMERGE *imerge, double *read_time, 
                           ha_rows* imerge_rows);
inline double get_index_only_read_time(const PARAM* param, ha_rows records, 
                                       int keynr);

#ifndef DBUG_OFF
static void print_sel_tree(PARAM *param, SEL_TREE *tree, key_map *tree_map,
                           const char *msg);
static void print_ror_scans_arr(TABLE *table, const char *msg, 
                                struct st_ror_scan_info **start, 
                                struct st_ror_scan_info **end);
static void print_rowid(byte* val, int len);
static void print_quick(QUICK_SELECT_I *quick, const key_map *needed_reg);
#endif

static SEL_TREE *tree_and(PARAM *param,SEL_TREE *tree1,SEL_TREE *tree2);
static SEL_TREE *tree_or(PARAM *param,SEL_TREE *tree1,SEL_TREE *tree2);
static SEL_ARG *sel_add(SEL_ARG *key1,SEL_ARG *key2);
static SEL_ARG *key_or(SEL_ARG *key1,SEL_ARG *key2);
static SEL_ARG *key_and(SEL_ARG *key1,SEL_ARG *key2,uint clone_flag);
static bool get_range(SEL_ARG **e1,SEL_ARG **e2,SEL_ARG *root1);
bool get_quick_keys(PARAM *param,QUICK_RANGE_SELECT *quick,KEY_PART *key,
			   SEL_ARG *key_tree,char *min_key,uint min_key_flag,
			   char *max_key,uint max_key_flag);
static bool eq_tree(SEL_ARG* a,SEL_ARG *b);

static SEL_ARG null_element(SEL_ARG::IMPOSSIBLE);
static bool null_part_in_key(KEY_PART *key_part, const char *key, 
                             uint length);
bool sel_trees_can_be_ored(SEL_TREE *tree1, SEL_TREE *tree2, PARAM* param);


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
  int or_sel_tree(PARAM *param, SEL_TREE *tree);
  int or_sel_tree_with_checks(PARAM *param, SEL_TREE *new_tree);
  int or_sel_imerge_with_checks(PARAM *param, SEL_IMERGE* imerge);
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

int SEL_IMERGE::or_sel_tree(PARAM *param, SEL_TREE *tree)
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
  Perform OR operation on this SEL_IMERGE and supplied SEL_TREE new_tree,
  combining new_tree with one of the trees in this SEL_IMERGE if they both
  have SEL_ARGs for the same key.
 
  SYNOPSIS
    or_sel_tree_with_checks()
      param    PARAM from SQL_SELECT::test_quick_select
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

int SEL_IMERGE::or_sel_tree_with_checks(PARAM *param, SEL_TREE *new_tree)
{
  for (SEL_TREE** tree = trees;
       tree != trees_next;
       tree++)
  {
    if (sel_trees_can_be_ored(*tree, new_tree, param))
    {
      *tree = tree_or(param, *tree, new_tree);
      if (!*tree)
        return 1;
      if (((*tree)->type == SEL_TREE::MAYBE) ||
          ((*tree)->type == SEL_TREE::ALWAYS))
        return 1;
      /* SEL_TREE::IMPOSSIBLE is impossible here */
      return 0;
    }
  }

  /* New tree cannot be combined with any of existing trees. */
  return or_sel_tree(param, new_tree);
}


/*
  Perform OR operation on this index_merge and supplied index_merge list.

  RETURN
    0 - OK
    1 - One of conditions in result is always TRUE and this SEL_IMERGE 
        should be discarded.
   -1 - An error occurred
*/

int SEL_IMERGE::or_sel_imerge_with_checks(PARAM *param, SEL_IMERGE* imerge)
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

    If (a_1||b_1) produce a condition that is always true, NULL is returned
    and index_merge is discarded (while it is actually possible to try 
    harder).

    As a consequence of this, choice of keys to do index_merge read may depend
    on the order of conditions in WHERE part of the query.

  RETURN
    0     OK, result is stored in *im1
    other Error, both passed lists are unusable
*/

int imerge_list_or_list(PARAM *param, 
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
    0     OK, result is stored in *im1.
    other Error
  
*/

int imerge_list_or_tree(PARAM *param, 
                        List<SEL_IMERGE> *im1,
                        SEL_TREE *tree)
{
  SEL_IMERGE *imerge;
  List_iterator<SEL_IMERGE> it(*im1);
  while((imerge= it++))
  {
    if (imerge->or_sel_tree_with_checks(param, tree))
      it.remove();
  }
  return im1->is_empty();
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
			table_map read_tables, COND *conds, int *error)
{
  SQL_SELECT *select;
  DBUG_ENTER("make_select");

  *error=0;
  if (!conds)
    DBUG_RETURN(0);
  if (!(select= new SQL_SELECT))
  {
    *error= 1;			// out of memory
    DBUG_RETURN(0);		/* purecov: inspected */
  }
  select->read_tables=read_tables;
  select->const_tables=const_tables;
  select->head=head;
  select->cond=conds;

  if (head->sort.io_cache)
  {
    select->file= *head->sort.io_cache;
    select->records=(ha_rows) (select->file.end_of_file/
			       head->file->ref_length);
    my_free((gptr) (head->sort.io_cache),MYF(0));
    head->sort.io_cache=0;
  }
  DBUG_RETURN(select);
}


SQL_SELECT::SQL_SELECT() :quick(0),cond(0),free_cond(0)
{
  quick_keys.clear_all(); needed_reg.clear_all();
  my_b_clear(&file);
}


SQL_SELECT::~SQL_SELECT()
{
  delete quick;
  if (free_cond)
    delete cond;
  close_cached_file(&file);
}

#undef index					// Fix for Unixware 7

QUICK_SELECT_I::QUICK_SELECT_I()
  :max_used_key_length(0),
   used_key_parts(0)
{}

QUICK_RANGE_SELECT::QUICK_RANGE_SELECT(THD *thd, TABLE *table, uint key_nr, 
                                       bool no_alloc, MEM_ROOT *parent_alloc)
  :dont_free(0),error(0),free_file(0),cur_range(NULL),range(0)
{
  index= key_nr;
  head=  table;
  my_init_dynamic_array(&ranges, sizeof(QUICK_RANGE*), 16, 16);

  if (!no_alloc && !parent_alloc)
  {
    // Allocates everything through the internal memroot
    init_sql_alloc(&alloc, thd->variables.range_alloc_block_size, 0);
    my_pthread_setspecific_ptr(THR_MALLOC,&alloc);
  }
  else
    bzero((char*) &alloc,sizeof(alloc));
  file= head->file;
  record= head->record[0];
}

int QUICK_RANGE_SELECT::init()
{
  return (error= file->index_init(index));
}

QUICK_RANGE_SELECT::~QUICK_RANGE_SELECT()
{  
  DBUG_ENTER("QUICK_RANGE_SELECT::~QUICK_RANGE_SELECT");
  if (!dont_free)
  {
    file->index_end();
    file->extra(HA_EXTRA_NO_KEYREAD);
    delete_dynamic(&ranges); /* ranges are allocated in alloc */ 
    if (free_file) 
    {
      DBUG_PRINT("info", ("Freeing separate handler %p (free=%d)", file,
                          free_file));
      file->reset(); 
      file->close(); 
    } 
    free_root(&alloc,MYF(0)); 
  } 
  DBUG_VOID_RETURN; 
}


QUICK_INDEX_MERGE_SELECT::QUICK_INDEX_MERGE_SELECT(THD *thd_param, 
                                                   TABLE *table)
  :cur_quick_it(quick_selects),pk_quick_select(NULL),unique(NULL),
   thd(thd_param)
{
  index= MAX_KEY;
  head= table;
  reset_called= false;
  bzero(&read_record, sizeof(read_record));
  init_sql_alloc(&alloc,1024,0);
}

int QUICK_INDEX_MERGE_SELECT::init()
{
  cur_quick_it.rewind();
  cur_quick_select= cur_quick_it++;
  return 0;
}

int QUICK_INDEX_MERGE_SELECT::reset()
{
  int result;
  DBUG_ENTER("QUICK_INDEX_MERGE_SELECT::reset");
  if (reset_called)
    DBUG_RETURN(0);

  reset_called= true;
  result = cur_quick_select->reset() || prepare_unique();
  DBUG_RETURN(result);
}

bool 
QUICK_INDEX_MERGE_SELECT::push_quick_back(QUICK_RANGE_SELECT *quick_sel_range)
{
  /* 
    Save quick_select that does scan on clustered primary key as it will be 
    processed separately.
  */
  if (head->file->primary_key_is_clustered() && 
      quick_sel_range->index == head->primary_key)
    pk_quick_select= quick_sel_range;
  else
    return quick_selects.push_back(quick_sel_range);
  return 0;
}

QUICK_INDEX_MERGE_SELECT::~QUICK_INDEX_MERGE_SELECT()
{
  delete unique;
  quick_selects.delete_elements();
  delete pk_quick_select;
  free_root(&alloc,MYF(0));
}


QUICK_ROR_INTERSECT_SELECT::QUICK_ROR_INTERSECT_SELECT(THD *thd_param,
                                                       TABLE *table,
                                                       bool retrieve_full_rows,
                                                       MEM_ROOT *parent_alloc)
  : cpk_quick(NULL), thd(thd_param), reset_called(false), 
    need_to_fetch_row(retrieve_full_rows)
{
  index= MAX_KEY;
  head= table;  
  record= head->record[0];
  if (!parent_alloc)
    init_sql_alloc(&alloc,1024,0);
  else
    bzero(&alloc, sizeof(MEM_ROOT));
  last_rowid= (byte*)alloc_root(parent_alloc? parent_alloc : &alloc, 
                                head->file->ref_length);
}

int QUICK_ROR_INTERSECT_SELECT::init()
{
  /* Check if last_rowid was allocated in ctor */
  return !last_rowid;
}


/*
  Init this quick select to be a ROR child scan. 
  NOTE
    QUICK_ROR_INTERSECT_SELECT::reset() may choose not to call this function
    but reuse its handler object for doing one of range scans. It duplicates
    a part of this function' code.
  RETURN 
    0  ROR child scan initialized, ok to use.
    1  error
*/

int QUICK_RANGE_SELECT::init_ror_child_scan(bool reuse_handler)
{
  handler *save_file= file;
  DBUG_ENTER("QUICK_RANGE_SELECT::init_ror_child_scan");
  
  if (reuse_handler)
  {
    DBUG_PRINT("info", ("Reusing handler %p", file));
    if (file->extra(HA_EXTRA_KEYREAD) ||
        file->extra(HA_EXTRA_RETRIEVE_ALL_COLS) |
        init() || reset())
    {
      DBUG_RETURN(1);
    }
    else
      DBUG_RETURN(0);
  }

  /* Create a separate handler object for this quick select */
  if (free_file)
  {
    /* already have own 'handler' object. */
    DBUG_RETURN(0);
  }
  
  if (!(file= get_new_handler(head, head->db_type)))
    goto failure;
  DBUG_PRINT("info", ("Allocated new handler %p", file));
  if (file->ha_open(head->path, head->db_stat, HA_OPEN_IGNORE_IF_LOCKED))
  {
    /* Caller will free the memory */ 
    goto failure;
  }
  
  if (file->extra(HA_EXTRA_KEYREAD) || 
      file->extra(HA_EXTRA_RETRIEVE_ALL_COLS) ||
      init() || reset())
  {
    file->close();
    goto failure;
  }
  free_file=  true;
  last_rowid= file->ref;
  DBUG_RETURN(0);

failure:
  file= save_file;
  DBUG_RETURN(1);
}

int QUICK_ROR_INTERSECT_SELECT::init_ror_child_scan(bool reuse_handler)
{
  List_iterator_fast<QUICK_RANGE_SELECT> quick_it(quick_selects);
  QUICK_RANGE_SELECT* quick;
  DBUG_ENTER("QUICK_ROR_INTERSECT_SELECT::init_ror_child_scan");

  /* Initialize all merged "children" quick selects */
  quick_it.rewind();
  DBUG_ASSERT(!(need_to_fetch_row && !reuse_handler));
  if (!need_to_fetch_row && reuse_handler)
  {
    quick= quick_it++;
    /* 
      There is no use for this->file. Reuse it for first of merged range 
      selects.
    */
    if (quick->init_ror_child_scan(true))
      DBUG_RETURN(1);
    quick->file->extra(HA_EXTRA_KEYREAD_PRESERVE_FIELDS);
  }
  while((quick= quick_it++))
  {
    if (quick->init_ror_child_scan(false))
      DBUG_RETURN(1);
    quick->file->extra(HA_EXTRA_KEYREAD_PRESERVE_FIELDS);
    /* Share the same record structure in intersect*/
    quick->record= head->record[0];
  }

  if (need_to_fetch_row && head->file->rnd_init())
  {
    DBUG_PRINT("error", ("ROR index_merge rnd_init call failed"));
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}

int QUICK_ROR_INTERSECT_SELECT::reset()
{
  int result;
  DBUG_ENTER("QUICK_ROR_INTERSECT_SELECT::reset");
  result= init_ror_child_scan(true);
  DBUG_RETURN(result);
}

bool 
QUICK_ROR_INTERSECT_SELECT::push_quick_back(QUICK_RANGE_SELECT *quick)
{
  bool res;
  if (head->file->primary_key_is_clustered() && 
      quick->index == head->primary_key)
  {
    cpk_quick= quick;
    res= 0;
  }
  else
    res= quick_selects.push_back(quick);
  return res;
}

QUICK_ROR_INTERSECT_SELECT::~QUICK_ROR_INTERSECT_SELECT()
{  
  DBUG_ENTER("QUICK_ROR_INTERSECT_SELECT::~QUICK_ROR_INTERSECT_SELECT");
  quick_selects.delete_elements(); 
  delete cpk_quick;
  free_root(&alloc,MYF(0));
  DBUG_VOID_RETURN;
}

QUICK_ROR_UNION_SELECT::QUICK_ROR_UNION_SELECT(THD *thd_param,
                                               TABLE *table)
  : thd(thd_param), reset_called(false)
{
  index= MAX_KEY;
  head= table;
  rowid_length= table->file->ref_length;
  record= head->record[0];
  init_sql_alloc(&alloc, thd->variables.range_alloc_block_size, 0);
  my_pthread_setspecific_ptr(THR_MALLOC,&alloc);
}

int QUICK_ROR_UNION_SELECT::init()
{
  if (init_queue(&queue, quick_selects.elements, 0,
                 false , QUICK_ROR_UNION_SELECT::queue_cmp,
                 (void*) this))
  {
    bzero(&queue, sizeof(QUEUE));
    return 1;
  }
  
  if (!(cur_rowid= (byte*)alloc_root(&alloc, 2*head->file->ref_length)))
    return 1;
  prev_rowid= cur_rowid + head->file->ref_length;
  return 0;
}

/*
  Comparison function to be used by priority queue.
  SYNPOSIS
    QUICK_ROR_UNION_SELECT::queue_cmp()
      arg   Pointer to QUICK_ROR_UNION_SELECT
      val1  First merged select
      val2  Second merged select
*/
int QUICK_ROR_UNION_SELECT::queue_cmp(void *arg, byte *val1, byte *val2)
{
  QUICK_ROR_UNION_SELECT *self= (QUICK_ROR_UNION_SELECT*)arg;    
  return self->head->file->cmp_ref(((QUICK_SELECT_I*)val1)->last_rowid,
                                   ((QUICK_SELECT_I*)val2)->last_rowid);
}

int QUICK_ROR_UNION_SELECT::reset()
{
  QUICK_SELECT_I* quick;
  int error;
  DBUG_ENTER("QUICK_ROR_UNION_SELECT::reset");
  have_prev_rowid= false;
  /* 
    Initialize scans for merged quick selects and put all merged quick 
    selects into the queue.
  */
  List_iterator_fast<QUICK_SELECT_I> it(quick_selects);
  while ((quick= it++))
  {
    if (quick->init_ror_child_scan(false))
      DBUG_RETURN(1);    
    if ((error= quick->get_next()))
    {
      if (error == HA_ERR_END_OF_FILE)
        continue;
      else
        DBUG_RETURN(error);
    }
    quick->save_last_pos();
    queue_insert(&queue, (byte*)quick);
  }

  if (head->file->rnd_init())
  {
    DBUG_PRINT("error", ("ROR index_merge rnd_init call failed"));
    DBUG_RETURN(1);
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
  free_root(&alloc,MYF(0));
  DBUG_VOID_RETURN;
}


QUICK_RANGE::QUICK_RANGE()
  :min_key(0),max_key(0),min_length(0),max_length(0),
   flag(NO_MIN_RANGE | NO_MAX_RANGE)
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
  use_count=1; elements=1;
}


inline void SEL_ARG::make_root()
{
  left=right= &null_element;
  color=BLACK;
  next=prev=0;
  use_count=0; elements=1;
}

SEL_ARG::SEL_ARG(Field *f,const char *min_value_arg,const char *max_value_arg)
  :min_flag(0), max_flag(0), maybe_flag(0), maybe_null(f->real_maybe_null()),
   elements(1), use_count(1), field(f), min_value((char*) min_value_arg),
   max_value((char*) max_value_arg), next(0),prev(0),
   next_key_part(0),color(BLACK),type(KEY_RANGE)
{
  left=right= &null_element;
}

SEL_ARG::SEL_ARG(Field *field_,uint8 part_,char *min_value_,char *max_value_,
		 uint8 min_flag_,uint8 max_flag_,uint8 maybe_flag_)
  :min_flag(min_flag_),max_flag(max_flag_),maybe_flag(maybe_flag_),
   part(part_),maybe_null(field_->real_maybe_null()), elements(1),use_count(1),
   field(field_), min_value(min_value_), max_value(max_value_),
   next(0),prev(0),next_key_part(0),color(BLACK),type(KEY_RANGE)
{
  left=right= &null_element;
}

SEL_ARG *SEL_ARG::clone(SEL_ARG *new_parent,SEL_ARG **next_arg)
{
  SEL_ARG *tmp;
  if (type != KEY_RANGE)
  {
    if (!(tmp= new SEL_ARG(type)))
      return 0;					// out of memory
    tmp->prev= *next_arg;			// Link into next/prev chain
    (*next_arg)->next=tmp;
    (*next_arg)= tmp;
  }
  else
  {
    if (!(tmp= new SEL_ARG(field,part, min_value,max_value,
			   min_flag, max_flag, maybe_flag)))
      return 0;					// OOM
    tmp->parent=new_parent;
    tmp->next_key_part=next_key_part;
    if (left != &null_element)
      tmp->left=left->clone(tmp,next_arg);

    tmp->prev= *next_arg;			// Link into next/prev chain
    (*next_arg)->next=tmp;
    (*next_arg)= tmp;

    if (right != &null_element)
      if (!(tmp->right= right->clone(tmp,next_arg)))
	return 0;				// OOM
  }
  increment_use_count(1);
  return tmp;
}

SEL_ARG *SEL_ARG::first()
{
  SEL_ARG *next_arg=this;
  if (!next_arg->left)
    return 0;					// MAYBE_KEY
  while (next_arg->left != &null_element)
    next_arg=next_arg->left;
  return next_arg;
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

static int sel_cmp(Field *field, char *a,char *b,uint8 a_flag,uint8 b_flag)
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
  cmp=field->key_cmp((byte*) a,(byte*) b);
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


SEL_ARG *SEL_ARG::clone_tree()
{
  SEL_ARG tmp_link,*next_arg,*root;
  next_arg= &tmp_link;
  root= clone((SEL_ARG *) 0, &next_arg);
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

  bool is_ror;
  /* Create quck select for this plan */
  virtual QUICK_SELECT_I *make_quick(PARAM *param,
                                     bool retrieve_full_rows,
                                     MEM_ROOT *parent_alloc=NULL) = 0;

  /* Table read plans are allocated on MEM_ROOT and must not be deleted */
  static void *operator new(size_t size, MEM_ROOT *mem_root)
  { return (void*) alloc_root(mem_root, (uint) size); }
  static void operator delete(void *ptr,size_t size) {}
};

class TRP_ROR_INTERSECT;
class TRP_ROR_UNION;
class TRP_INDEX_MERGE;


class TRP_RANGE : public TABLE_READ_PLAN
{
public:
  SEL_ARG *key;
  uint     key_idx; /* key number in param->keys */
  TRP_RANGE(SEL_ARG *key_arg, uint idx_arg) 
   : key(key_arg), key_idx(idx_arg)
  {}
  
  QUICK_SELECT_I *make_quick(PARAM *param, bool retrieve_full_rows,
                             MEM_ROOT *parent_alloc)
  {
    DBUG_ENTER("TRP_RANGE::make_quick");
    QUICK_RANGE_SELECT *quick;
    /* ignore retrieve_full_rows there as it is set/used elsewhere */
    if ((quick= get_quick_select(param, key_idx, key, parent_alloc)))
    {
      quick->records= records;
      quick->read_time= read_cost;
    }
    DBUG_RETURN(quick);
  }
};

class TRP_ROR_INTERSECT : public TABLE_READ_PLAN
{
public:
  QUICK_SELECT_I *make_quick(PARAM *param, bool retrieve_full_rows, 
                             MEM_ROOT *parent_alloc);
  struct st_ror_scan_info **first_scan;
  struct st_ror_scan_info **last_scan;
  bool is_covering; /* true if no row retrieval phase is necessary */
  double index_scan_costs;
};

/* 
  ROR-union is currently never covering.
*/
class TRP_ROR_UNION : public TABLE_READ_PLAN
{
public:
  QUICK_SELECT_I *make_quick(PARAM *param, bool retrieve_full_rows, 
                             MEM_ROOT *parent_alloc);
  TABLE_READ_PLAN **first_ror;  
  TABLE_READ_PLAN **last_ror;
  
};

class TRP_INDEX_MERGE : public TABLE_READ_PLAN
{
public:
  QUICK_SELECT_I *make_quick(PARAM *param, bool retrieve_full_rows, 
                             MEM_ROOT *parent_alloc);
  TRP_RANGE **range_scans;
  TRP_RANGE **range_scans_end;
};


/* 
  Fill param->needed_fields with bitmap of fields used in the query
  NOTE
    Do not put clustered PK members into it as they are implicitly present in
    all keys.
*/

static int fill_used_fields_bitmap(PARAM *param)
{
  TABLE *table= param->table;
  param->fields_bitmap_size= (table->fields/8 + 1);
  uchar *tmp;
  uint pk;
  if (!(tmp= (uchar*)alloc_root(param->mem_root,param->fields_bitmap_size)) ||
      bitmap_init(&param->needed_fields, tmp, param->fields_bitmap_size*8, 
                  false))
    return 1;
  
  bitmap_clear_all(&param->needed_fields);
  for (uint i= 0; i < table->fields; i++)
  {
    if (param->thd->query_id == table->field[i]->query_id)
      bitmap_set_bit(&param->needed_fields, i+1);
  }

  pk= param->table->primary_key;
  if (param->table->file->primary_key_is_clustered() && pk != MAX_KEY)
  {
    KEY_PART_INFO *key_part= param->table->key_info[pk].key_part;
    KEY_PART_INFO *key_part_end= key_part + 
                                 param->table->key_info[pk].key_parts;
    for(;key_part != key_part_end; ++key_part)
    {
      bitmap_clear_bit(&param->needed_fields, key_part->fieldnr);
    }
  }
  return 0;
}


/*
  Test if a key can be used in different ranges

  SYNOPSIS
   SQL_SELECT::test_quick_select(thd,keys_to_use, prev_tables,
                                 limit, force_quick_range)

   Updates the following in the select parameter:
     needed_reg - Bits for keys with may be used if all prev regs are read
     quick      - Parameter to use when reading records.
   
   In the table struct the following information is updated:
     quick_keys - Which keys can be used
     quick_rows - How many rows the key matches

 RETURN VALUES
  -1 if impossible select
   0 if can't use quick_select
   1 if found usable range

 TODO
   check if the function really needs to modify keys_to_use, and change the
   code to pass it by reference if not
*/

int SQL_SELECT::test_quick_select(THD *thd, key_map keys_to_use,
				  table_map prev_tables,
				  ha_rows limit, bool force_quick_range)
{
  uint basflag;
  uint idx;
  double scan_time;
  DBUG_ENTER("test_quick_select");
  DBUG_PRINT("enter",("keys_to_use: %lu  prev_tables: %lu  const_tables: %lu",
		      keys_to_use.to_ulonglong(), (ulong) prev_tables,
		      (ulong) const_tables));

  delete quick;
  quick=0;
  needed_reg.clear_all(); quick_keys.clear_all();
  if (!cond || (specialflag & SPECIAL_SAFE_MODE) && ! force_quick_range ||
      !limit)
    DBUG_RETURN(0); /* purecov: inspected */
  if (!((basflag= head->file->table_flags()) & HA_KEYPOS_TO_RNDPOS) &&
      keys_to_use.is_set_all() || keys_to_use.is_clear_all())
    DBUG_RETURN(0);				/* Not smart database */
  records=head->file->records;
  if (!records)
    records++;					/* purecov: inspected */
  scan_time=(double) records / TIME_FOR_COMPARE+1;
  read_time=(double) head->file->scan_time()+ scan_time + 1.0;
  if (head->force_index)
    scan_time= read_time= DBL_MAX;
  if (limit < records)
    read_time=(double) records+scan_time+1;	// Force to use index
  else if (read_time <= 2.0 && !force_quick_range)
    DBUG_RETURN(0);				/* No need for quick select */

  DBUG_PRINT("info",("Time to scan table: %g", read_time));

  keys_to_use.intersect(head->keys_in_use_for_query);
  if (!keys_to_use.is_clear_all())
  {
    MEM_ROOT *old_root,alloc;
    SEL_TREE *tree;
    KEY_PART *key_parts;
    PARAM param;

    /* set up parameter that is passed to all functions */
    param.thd= thd;
    param.baseflag=basflag;
    param.prev_tables=prev_tables | const_tables;
    param.read_tables=read_tables;
    param.current_table= head->map;
    param.table=head;
    param.keys=0;
    param.mem_root= &alloc;
    param.needed_reg= &needed_reg;

    param.imerge_cost_buff_size= 0;
    

    thd->no_errors=1;				// Don't warn about NULL
    init_sql_alloc(&alloc, thd->variables.range_alloc_block_size, 0);
    if (!(param.key_parts = (KEY_PART*) alloc_root(&alloc,
						   sizeof(KEY_PART)*
						   head->key_parts))
                              || fill_used_fields_bitmap(&param))
    {
      thd->no_errors=0;
      free_root(&alloc,MYF(0));			// Return memory & allocator
      DBUG_RETURN(0);				// Can't use range
    }
    key_parts= param.key_parts;
    old_root=my_pthread_getspecific_ptr(MEM_ROOT*,THR_MALLOC);
    my_pthread_setspecific_ptr(THR_MALLOC,&alloc);
    
    /* 
      Make an array with description of all key parts of all table keys. 
      This is used in get_mm_parts function. 
    */
    for (idx=0 ; idx < head->keys ; idx++)
    {
      if (!keys_to_use.is_set(idx))
	continue;
      KEY *key_info= &head->key_info[idx];
      if (key_info->flags & HA_FULLTEXT)
	continue;    // ToDo: ft-keys in non-ft ranges, if possible   SerG

      param.key[param.keys]=key_parts;
      for (uint part=0 ; part < key_info->key_parts ; part++,key_parts++)
      {
	key_parts->key=param.keys;
	key_parts->part=part;
	key_parts->part_length= key_info->key_part[part].length;
	key_parts->field=    key_info->key_part[part].field;
	key_parts->null_bit= key_info->key_part[part].null_bit;
	if (key_parts->field->type() == FIELD_TYPE_BLOB)
	  key_parts->part_length+=HA_KEY_BLOB_LENGTH;
        key_parts->image_type =
          (key_info->flags & HA_SPATIAL) ? Field::itMBR : Field::itRAW;
      }
      param.real_keynr[param.keys++]=idx;
    }
    param.key_parts_end=key_parts;

    if ((tree=get_mm_tree(&param,cond)))
    {
      if (tree->type == SEL_TREE::IMPOSSIBLE)
      {
	records=0L;			// Return -1 from this function
	read_time= (double) HA_POS_ERROR;
      }
      else if (tree->type == SEL_TREE::KEY ||
               tree->type == SEL_TREE::KEY_SMALLER)
      {
        TABLE_READ_PLAN *best_trp;
        /* 
          It is possible to use a quick select (but maybe it would be slower
          than 'all' table scan).
          Btw, tree type SEL_TREE::INDEX_MERGE was not introduced 
          intentionally.
        */
        if (tree->merges.is_empty())
        {
          double best_read_time= read_time;
          TRP_ROR_INTERSECT *new_trp;
          bool can_build_covering= false;
        
          /* Get best 'range' plan and prepare data for making other plans */
          if ((best_trp= get_key_scans_params(&param, tree, false, 
                                              best_read_time)))
            best_read_time= best_trp->read_cost;
          
          /* 
            Simultaneous key scans and row deletes on several handler
            objects are not allowed so don't use ROR-intersection for 
            table deletes.
          */
          if (thd->lex->sql_command != SQLCOM_DELETE)
          {
            /* 
              Get best non-covering ROR-intersection plan and prepare data for 
              building covering ROR-intersection
            */
            if ((new_trp= get_best_ror_intersect(&param, tree, false, 
                                                 best_read_time, 
                                                 &can_build_covering)))
            {
              best_trp= new_trp;
              best_read_time= best_trp->read_cost;
            }
          
            /* 
              Try constructing covering ROR-intersect only if it looks possible 
              and worth doing.
            */
            if (new_trp && !new_trp->is_covering && can_build_covering &&
                (new_trp= get_best_covering_ror_intersect(&param, tree, 
                                                          best_read_time)))
              best_trp= new_trp;
          }
        }
        else
        {
          /* Try creating index_merge/ROR-union scan. */
          SEL_IMERGE *imerge;
          TABLE_READ_PLAN *best_conj_trp= NULL, *new_conj_trp;
          LINT_INIT(new_conj_trp); /* no empty index_merge lists possible */
          DBUG_PRINT("info",("No range reads possible,"
                             " trying to construct index_merge"));
          
          /* 
            Calculate cost of 'all'+index_only scan if it is possible. 
            It is possible that table can be read in two ways:
             a) 'all' + index_only
             b) index_merge without index_only.
          */
          if (!head->used_keys.is_clear_all())
          {           
            int key_for_use= find_shortest_key(head, &head->used_keys);
            ha_rows total_table_records= (0 == head->file->records)? 1 : 
                                          head->file->records;
            read_time = get_index_only_read_time(&param, total_table_records,
                                                 key_for_use);
            DBUG_PRINT("info", 
                       ("'all'+'using index' scan will be using key %d, "
                        "read time %g", key_for_use, read_time));
          }

          List_iterator_fast<SEL_IMERGE> it(tree->merges);
          while ((imerge= it++))
          {
            new_conj_trp= get_best_disjunct_quick(&param, imerge, read_time);
            if (!best_conj_trp || (new_conj_trp && new_conj_trp->read_cost < 
                                                   best_conj_trp->read_cost))
              best_conj_trp= new_conj_trp;
          }
          best_trp= best_conj_trp;
        }

        my_pthread_setspecific_ptr(THR_MALLOC, old_root);        
        if (best_trp)
        {
          records= best_trp->records;
          if (!(quick= best_trp->make_quick(&param, true)) || quick->init())
          {
            delete quick;
            quick= NULL;
          }
        }        
      }
    }
    my_pthread_setspecific_ptr(THR_MALLOC, old_root);
    free_root(&alloc,MYF(0));			// Return memory & allocator
    thd->no_errors=0;
  }

  DBUG_EXECUTE("info", print_quick(quick, &needed_reg););

  /*
    Assume that if the user is using 'limit' we will only need to scan
    limit rows if we are using a key
  */
  DBUG_RETURN(records ? test(quick) : -1);
}


/* 
  Get cost of 'sweep' full row retrieveal of #records rows.
  RETURN
    0 - ok 
    1 - sweep is more expensive then full table scan.
*/

bool get_sweep_read_cost(const PARAM *param, ha_rows records, double* cost,
                          double index_reads_cost, double max_cost)
{
  if (param->table->file->primary_key_is_clustered())
  {
    *cost= param->table->file->read_time(param->table->primary_key, 
                                         records, records);
  }
  else
  {    
    double n_blocks=
      ceil((double)(longlong)param->table->file->data_file_length / IO_SIZE);
    double busy_blocks=
      n_blocks * (1.0 - pow(1.0 - 1.0/n_blocks, rows2double(records)));
    if (busy_blocks < 1.0)
      busy_blocks= 1.0;
    DBUG_PRINT("info",("sweep: nblocks= %g, busy_blocks=%g index_blocks=%g",
                       n_blocks, busy_blocks, index_reads_cost));
    /*
      Disabled: Bail out if # of blocks to read is bigger than # of blocks in 
      table data file.
    if (max_cost != DBL_MAX  && (busy_blocks+index_reads_cost) >= n_blocks)
      return 1;
    */
    JOIN *join= param->thd->lex->select_lex.join;
    if (!join || join->tables == 1)
    {
      /* No join, assume reading is done in one 'sweep' */
      *cost= busy_blocks*(DISK_SEEK_BASE_COST + 
                          DISK_SEEK_PROP_COST*n_blocks/busy_blocks);
    }
    else
    {
      /* 
        Possibly this is a join with source table being non-last table, so
        assume that disk seeks are random here.
      */
      *cost = busy_blocks;
    }
  }
  DBUG_PRINT("info",("returning cost=%g", *cost));
  return 0;
}


/*
  Get best plan for a SEL_IMERGE disjunctive expression.
  SYNOPSIS
    get_best_disjunct_quick()
      param
      imerge
      read_time Don't create scans with cost > read_time
  RETURN 
    read plan
    NULL - OOM or no read scan could be built.
  
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
     
*/

static
TABLE_READ_PLAN *get_best_disjunct_quick(PARAM *param, SEL_IMERGE *imerge,
                                           double read_time)
{
  SEL_TREE **ptree;
  TRP_INDEX_MERGE *imerge_trp= NULL;
  uint n_child_scans= imerge->trees_next - imerge->trees;
  TRP_RANGE **range_scans;
  TRP_RANGE **cur_child;
  TRP_RANGE **cpk_scan= NULL;
  bool imerge_too_expensive= false;
  double imerge_cost= 0.0;
  ha_rows cpk_scan_records= 0;
  ha_rows non_cpk_scan_records= 0;
  bool pk_is_clustered= param->table->file->primary_key_is_clustered();
  bool all_scans_ror_able= true;
  bool all_scans_rors= true;
  uint unique_calc_buff_size;
  TABLE_READ_PLAN **roru_read_plans;
  TABLE_READ_PLAN **cur_roru_plan;
  double roru_index_costs;
  double blocks_in_index_read;
  ha_rows roru_total_records;
  double roru_intersect_part= 1.0;
  double sweep_cost;
  DBUG_ENTER("get_best_disjunct_quick");
  DBUG_PRINT("info", ("Full table scan cost =%g", read_time));

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
    if (!(*cur_child= get_key_scans_params(param, *ptree, true, read_time)))
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
      continue;
    
    imerge_cost += (*cur_child)->read_cost;
    all_scans_ror_able &= ((*ptree)->n_ror_scans > 0);
    all_scans_rors &= (*cur_child)->is_ror;
    if (pk_is_clustered && 
        param->real_keynr[(*cur_child)->key_idx] == param->table->primary_key)
    {
      cpk_scan= cur_child;
      cpk_scan_records= (*cur_child)->records;
    }
    else
      non_cpk_scan_records += (*cur_child)->records;
  }
  
  DBUG_PRINT("info", ("index_merge scans cost=%g", imerge_cost));
  if (imerge_too_expensive || (imerge_cost > read_time) || 
      (non_cpk_scan_records+cpk_scan_records >= param->table->file->records) &&
      read_time != DBL_MAX)
  {
    /* 
      Bail out if it is obvious that both index_merge and ROR-union will be 
      more expensive
    */
    DBUG_PRINT("info", ("Sum of index_merge scans is more expensive than "
                        "full table scan, bailing out"));
    DBUG_RETURN(NULL); 
  }
  if (all_scans_rors)
  {
    roru_read_plans= (TABLE_READ_PLAN**)range_scans;
    goto skip_to_ror_scan;
  }
  blocks_in_index_read= imerge_cost; 
  if (cpk_scan) 
  { 
    /*
      Add one ROWID comparison for each row retrieved on non-CPK scan.  (it
      is done in QUICK_RANGE_SELECT::row_in_ranges) 
     */ 
    imerge_cost += non_cpk_scan_records / TIME_FOR_COMPARE_ROWID; 
  }

  /* Calculate cost(rowid_to_row_scan) */
  if (get_sweep_read_cost(param, non_cpk_scan_records, &sweep_cost, 
                           blocks_in_index_read, read_time))
    goto build_ror_index_merge;
  imerge_cost += sweep_cost;
  DBUG_PRINT("info",("index_merge cost with rowid-to-row scan: %g", 
                     imerge_cost));

  /* Add Unique operations cost */
  unique_calc_buff_size= 
    Unique::get_cost_calc_buff_size(non_cpk_scan_records, 
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
    Unique::get_use_cost(param->imerge_cost_buff, non_cpk_scan_records,
                         param->table->file->ref_length,
                         param->thd->variables.sortbuff_size);
  DBUG_PRINT("info",("index_merge total cost: %g (wanted: less then %g)", 
                     imerge_cost, read_time));
  if (imerge_cost < read_time)
  {
    if ((imerge_trp= new (param->mem_root)TRP_INDEX_MERGE))
    {
      imerge_trp->read_cost= imerge_cost;
      imerge_trp->records= non_cpk_scan_records + cpk_scan_records;
      imerge_trp->records= min(imerge_trp->records, 
                               param->table->file->records);
      imerge_trp->range_scans= range_scans;
      imerge_trp->range_scans_end= range_scans + n_child_scans;
      read_time= imerge_cost;
    }
  }

build_ror_index_merge:  
  if (!all_scans_ror_able || param->thd->lex->sql_command == SQLCOM_DELETE)
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
    if (!(*cur_roru_plan= get_best_ror_intersect(param, *ptree, false, cost,
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
                           param->table->file->records;
  }

  /* 
    rows to retrieve= 
      SUM(rows_in_scan_i) - table_rows * PROD(rows_in_scan_i / table_rows).
    This is valid because index_merge constructuion guarantees that conditions
    in disjunction do not share key parts.
  */
  roru_total_records -= (ha_rows)(roru_intersect_part*
                                  param->table->file->records); 
  /* ok, got a ROR read plan for each of the disjuncts 
    Calculate cost: 
    cost(index_union_scan(scan_1, ... scan_n)) =
      SUM_i(cost_of_index_only_scan(scan_i)) +
      queue_use_cost(rowid_len, n) +
      cost_of_row_retrieval
    See get_merge_buffers_cost function for queue_use_cost formula derivation.
  */
  
  if (get_sweep_read_cost(param, roru_total_records, &sweep_cost, 
                           roru_index_costs, read_time))
    DBUG_RETURN(NULL);
  double roru_total_cost;
  roru_total_cost= roru_index_costs +  
                   rows2double(roru_total_records)*log(n_child_scans) / 
                   (TIME_FOR_COMPARE_ROWID * M_LN2) +
                   sweep_cost;  
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
  Calculate cost of 'index only' scan for given index and number of records.
    (We can resolve this by only reading through this key.)

  SYNOPSIS
    get_whole_index_read_time()
      param    parameters structure
      records  #of records to read
      keynr    key to read

  NOTES
    It is assumed that we will read trough the whole key range and that all 
    key blocks are half full (normally things are much better).
*/

inline double get_index_only_read_time(const PARAM* param, ha_rows records, 
                                       int keynr)
{
  double read_time;
  uint keys_per_block= (param->table->file->block_size/2/
			(param->table->key_info[keynr].key_length+
			 param->table->file->ref_length) + 1);
  read_time=((double) (records+keys_per_block-1)/
             (double) keys_per_block);
  return read_time; 
}

typedef struct st_ror_scan_info
{
  uint      idx;   /* # of used key in param->keys */
  uint      keynr; /* # of used key in table */
  ha_rows   records;
  SEL_ARG   *sel_arg;
  /* Fields used in the query and covered by this ROR scan */
  MY_BITMAP covered_fields;  
  uint      used_fields_covered;
  int       key_rec_length; /* length of key record with rowid */

  /*
    Array of 
     #rows(keypart_1=c1 AND ... AND key_part_i=c_i) / 
     #rows(keypart_1=c1 AND ... AND key_part_{i+1}=c_{i+1}) values
  */
  double    *key_part_rows;
  double    index_read_cost;
  uint      first_uncovered_field;
  uint      key_components;
}ROR_SCAN_INFO;


/*
  Create ROR_SCAN_INFO* structure for condition sel_arg on key idx.
  SYNOPSIS
    make_ror_scan()
      param
      idx   index of key in param->keys
  RETURN
    NULL - OOM.
*/

static
ROR_SCAN_INFO *make_ror_scan(const PARAM *param, int idx, SEL_ARG *sel_arg)
{
  ROR_SCAN_INFO *ror_scan;
  uchar *bitmap_buf;
  uint keynr;
  DBUG_ENTER("make_ror_scan");
  if (!(ror_scan= (ROR_SCAN_INFO*)alloc_root(param->mem_root,
                                             sizeof(ROR_SCAN_INFO))))
    DBUG_RETURN(NULL);

  ror_scan->idx= idx;
  ror_scan->keynr= keynr= param->real_keynr[idx];
  ror_scan->key_rec_length= param->table->key_info[keynr].key_length + 
                            param->table->file->ref_length;
  ror_scan->sel_arg= sel_arg;
  ror_scan->records= param->table->quick_rows[keynr];
  
  if (!(bitmap_buf= (uchar*)alloc_root(param->mem_root, 
                                      param->fields_bitmap_size)))
    DBUG_RETURN(NULL);
  
  if (bitmap_init(&ror_scan->covered_fields, bitmap_buf,
                  param->fields_bitmap_size*8, false))
    DBUG_RETURN(NULL);
  bitmap_clear_all(&ror_scan->covered_fields);
  if (!(ror_scan->key_part_rows= 
        (double*)alloc_root(param->mem_root, sizeof(double)*
                            param->table->key_info[keynr].key_parts)))
    DBUG_RETURN(NULL);
  
  KEY_PART_INFO *key_part= param->table->key_info[keynr].key_part;
  KEY_PART_INFO *key_part_end= key_part + 
                               param->table->key_info[keynr].key_parts;
  uint n_used_covered= 0;
  for (;key_part != key_part_end; ++key_part)
  {
    if (bitmap_is_set(&param->needed_fields, key_part->fieldnr))
    {
      n_used_covered++;
      bitmap_set_bit(&ror_scan->covered_fields, key_part->fieldnr);
    }
  }
  ror_scan->index_read_cost= 
    get_index_only_read_time(param, param->table->quick_rows[ror_scan->keynr],
                             ror_scan->keynr);
  /* 
    Calculate # rows estimates for 
      (key_part1=c1)
      (key_part1=c1 AND key_part2=c2) 
      ...and so on
  */
  char key_val[MAX_KEY_LENGTH+MAX_FIELD_WIDTH]; /* values of current key */
  char *key_ptr= key_val;
  ha_rows records;
  ha_rows prev_records= param->table->file->records;
  double *rows_diff= ror_scan->key_part_rows;
  key_part= param->table->key_info[keynr].key_part;
  SEL_ARG *arg= ror_scan->sel_arg;
  /*
    We have #rows estimate already for first key part, so do first loop 
    iteration separately:
  */
  arg->store_min(key_part->length, &key_ptr, 0);
  prev_records= param->table->quick_rows[ror_scan->keynr]; 
  *(rows_diff++)= rows2double(prev_records) / param->table->file->records;
  arg=arg->next_key_part; 
  for(; arg; ++key_part, arg= arg->next_key_part)
  {    
    arg->store_min(key_part->length, &key_ptr, 0);
    records= 
      param->table->file->records_in_range(ror_scan->keynr,
                                           (byte*)key_val, key_ptr - key_val,
                                           HA_READ_KEY_EXACT,
                                           (byte*)key_val, key_ptr - key_val,
                                           HA_READ_AFTER_KEY);
    if (records == HA_POS_ERROR)
      return NULL; /* shouldn't happen actually */
    *(rows_diff++)= rows2double(records) / rows2double(prev_records);
    prev_records= records;
  }
  DBUG_RETURN(ror_scan);
}


/*
  Order ROR_SCAN_INFO** by 
    E(#records_matched) * key_record_length
*/
int cmp_ror_scan_info(ROR_SCAN_INFO** a, ROR_SCAN_INFO** b)
{
  double val1= rows2double((*a)->records) * (*a)->key_rec_length;
  double val2= rows2double((*b)->records) * (*b)->key_rec_length;
  return (val1 < val2)? -1: (val1 == val2)? 0 : 1;
}

/*
  Order ROR_SCAN_INFO** by 
   (#covered fields in F desc, 
    #components asc,           
    number of first not covered component asc)
*/
int cmp_ror_scan_info_covering(ROR_SCAN_INFO** a, ROR_SCAN_INFO** b)
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
  
  /* true if covered_fields is a superset of needed_fields */
  bool is_covering;  
  
  double index_scan_costs; /* SUM(cost of 'index-only' scans) */  
  double total_cost;
  /* 
    Fraction of table records that satisfies conditions of all scans.
    This is the number of full records that will be retrieved if a 
    non-index_only index intersection will be employed.
  */
  double records_fract;
  ha_rows index_records; /* sum(#records to look in indexes) */
}ROR_INTERSECT_INFO;


/* Allocate a ROR_INTERSECT and initialize it to contain zero scans */
ROR_INTERSECT_INFO* ror_intersect_init(const PARAM *param, bool is_index_only)
{
  ROR_INTERSECT_INFO *info;
  uchar* buf;
  if (!(info= (ROR_INTERSECT_INFO*)alloc_root(param->mem_root, 
                                              sizeof(ROR_INTERSECT_INFO))))
    return NULL;
  info->param= param;
  info->is_covering= is_index_only;
  info->index_scan_costs= 0.0f;
  info->records_fract= 1.0f;

  if (!(buf= (uchar*)alloc_root(param->mem_root, param->fields_bitmap_size)))
    return NULL;
  if (bitmap_init(&info->covered_fields, buf, param->fields_bitmap_size*8,
                  false))
    return NULL;
  bitmap_clear_all(&info->covered_fields);
  return info;
}

/*
  Check if it makes sense to add a ROR scan to ROR-intersection, and if yes 
  update parameters of ROR-intersection, including its cost.

  RETURN
    true   ROR scan added to ROR-intersection, cost updated.
    false  It doesn't make sense to add this ROR scan to this ROR-intersection.
  
  NOTE 
    Adding a ROR scan to ROR-intersect "makes sense" iff selectivt

    Cost of ROR-intersection is calulated as follows:
     cost= SUM_i(key_scan_cost_i) + cost_of_full_rows_retrieval
 
    if (union of indexes used covers all needed fields)
      cost_of_full_rows_retrieval= 0;
    else
    {
      cost_of_full_rows_retrieval= 
        cost_of_sweep_read(E(rows_to_retrive), rows_in_table);
    }

    E(rows_to_retrive) is calulated as follows: 
    Suppose we have a condition on several keys
    cond=k_11=c_11 AND k_12=c_12 AND ...  // parts of first key 
         k_21=c_21 AND k_22=c_22 AND ...  // parts of second key 
          ...
         k_n1=c_n1 AND k_n3=c_n3 AND ...  (1)
    
    where k_ij may be the same as any k_pq (i.e. keys may have common parts).

    A full row is retrieved iff entire cond holds.

    The recursive procedure for finding P(cond) is as follows:
    
    First step:
    Pick 1st part of 1st key and break conjunction (1) into two parts: 
      cond= (k_11=c_11 AND R)

    Here R may still contain condition(s) equivalent to k_11=c_11. 
    Nevertheless, the following holds:

      P(k_11=c_11 AND R) = P(k_11=c_11) * P(R|k_11=c_11). 

    Mark k_11 as fixed field (and satisfied condition) F, save P(F),
    save R to be cond and proceed to recursion step.

    Recursion step:
    We have set of fixed fields/satisfied conditions) F, probability P(F),
    and remaining conjunction R
    Pick next key part on current key and its condition "k_ij=c_ij".
    We will add "k_ij=c_ij" into F and update P(F).
    Lets denote k_ij as t,  R = t AND R1, where i1 may still contain t. Then

     P((t AND R1)|F) = P(t|F) * P(R1|t|F) = P(t|F) * P(R|(t AND F)) (2)

    (where '|' mean conditional probability, not "or")

    Consider the first multiplier in (2). One of the following holds:
    a) F contains condition on field used in t (i.e. t AND F = F).
      Then P(t|F) = 1

    b) F doesn't contain condition on field used in t. Then F and t are 
     considered independent. 

     P(t|F) = P(t|(fields_before_t_in_key AND other_fields)) = 
          = P(t|fields_before_t_in_key).

     P(t|fields_before_t_in_key)= #distinct_values(fields_before_t_in_key) / 
                                  #distinct_values(fields_before_t_in_key, t)
    
    The second multiplier is calculated by applying this step recusively. 

    This function applies recursion steps for all fixed key members of 
    one key, accumulating sets of covered fields and 
    The very first step described is done as recursion step with 
    P(fixed fields)=1 and empty set of fixed fields.
*/

bool ror_intersect_add(ROR_INTERSECT_INFO *info, ROR_SCAN_INFO* ror_scan)
{
  int i;
  SEL_ARG *sel_arg;
  KEY_PART_INFO *key_part= 
    info->param->table->key_info[ror_scan->keynr].key_part;
  double selectivity_mult= 1.0;
  DBUG_ENTER("ror_intersect_add");  
  DBUG_PRINT("info", ("Current selectivity= %g", info->records_fract)); 
  DBUG_PRINT("info", ("Adding scan on %s", 
                      info->param->table->key_info[ror_scan->keynr].name));
  for(i= 0, sel_arg= ror_scan->sel_arg; sel_arg; 
      i++, sel_arg= sel_arg->next_key_part)
  {
    if (!bitmap_is_set(&info->covered_fields, (key_part + i)->fieldnr))
    {
      /*
        P(t|F) = P(t|(fields_before_t_in_key AND other_fields)) =
               = P(t|fields_before_t_in_key).
      */
      selectivity_mult *= ror_scan->key_part_rows[i];
    }
  }
  if (selectivity_mult == 1.0)
  {
    /* Don't add this scan if it doesn't improve selectivity. */
    DBUG_PRINT("info", ("This scan doesn't improve selectivity.")); 
    DBUG_RETURN(false);
  }
  info->records_fract *= selectivity_mult;

  bitmap_union(&info->covered_fields, &ror_scan->covered_fields);

  ha_rows scan_records= info->param->table->quick_rows[ror_scan->keynr];
  info->index_scan_costs += ror_scan->index_read_cost;

  if (!info->is_covering && bitmap_is_subset(&info->param->needed_fields,
                                             &info->covered_fields))
  {
    DBUG_PRINT("info", ("ROR-intersect is covering now"));
    /* ROR-intersect became covering */ 
    info->is_covering= true;
  }
  
  info->index_records += scan_records;
  info->total_cost= info->index_scan_costs;
  if (!info->is_covering)
  {
    ha_rows table_recs= info->param->table->file->records;
    double sweep_cost;
    get_sweep_read_cost(info->param, 
                         (ha_rows)(table_recs*info->records_fract), 
                         &sweep_cost, info->index_scan_costs, DBL_MAX);

    info->total_cost += sweep_cost;
  }
  DBUG_PRINT("info", ("New selectivity= %g", info->records_fract)); 
  DBUG_PRINT("info", ("New cost= %g, %scovering", info->total_cost, 
                      info->is_covering?"" : "non-"));
  DBUG_RETURN(true);
}

/* 
  Get best ROR-intersection plan using non-covering ROR-intersection search 
  algorithm. The returned plan may be covering.

  SYNOPSIS
    get_best_ror_intersect()
      param
      tree
      force_index_only If true, don't calculate costs of full rows retrieval.
      read_time        Do not return read plans with cost > read_time.
      are_all_covering [out] set to true if union of all scans covers all 
                       fields needed by the query (and it is possible to build
                       a covering ROR-intersection)
  RETURN
    ROR-intersection table read plan 
    NULL if OOM or no plan found.

  NOTE
    get_key_scans_params must be called before for the same SEL_TREE before 
    this function can be called.
    
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
        if (!selectivity(S + first(R) < selectivity(S)))
          continue;

        S= S + first(R);
        R= R - first(R);
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
*/

static
TRP_ROR_INTERSECT *get_best_ror_intersect(const PARAM *param, SEL_TREE *tree,
                                          bool force_index_only, 
                                          double read_time,
                                          bool *are_all_covering)
{
  uint idx;
  double min_cost= read_time;
  DBUG_ENTER("get_best_ror_intersect");

  if (tree->n_ror_scans < 2)
    DBUG_RETURN(NULL);

  /* Collect ROR-able SEL_ARGs and create ROR_SCAN_INFO for each of them */
  ROR_SCAN_INFO **cur_ror_scan;
  if (!(tree->ror_scans= (ROR_SCAN_INFO**)alloc_root(param->mem_root,
                                                     sizeof(ROR_SCAN_INFO*)*
                                                     param->keys)))
    return NULL;
  
  for (idx= 0, cur_ror_scan= tree->ror_scans; idx < param->keys; idx++)
  {
    if (!tree->ror_scans_map.is_set(idx))
      continue;
    if (!(*cur_ror_scan= make_ror_scan(param, idx, tree->keys[idx])))
      return NULL;
    cur_ror_scan++;
  }

  tree->ror_scans_end= cur_ror_scan;
  DBUG_EXECUTE("info",print_ror_scans_arr(param->table, "original", 
                                          tree->ror_scans, 
                                          tree->ror_scans_end););
  /*
    Ok, [ror_scans, ror_scans_end) is array of ptrs to initialized 
    ROR_SCAN_INFOs.
    Get a minimal key scan using an approximate algorithm.
  */
  qsort(tree->ror_scans, tree->n_ror_scans, sizeof(ROR_SCAN_INFO*),
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
  ROR_INTERSECT_INFO *intersect;
  if (!(intersect= ror_intersect_init(param, false)))
    return NULL;
  
  /* [intersect_scans, intersect_scans_best) will hold the best combination */
  ROR_SCAN_INFO **intersect_scans_best= NULL; 
  ha_rows       best_rows;
  bool          is_best_covering;
  double        best_index_scan_costs;
  LINT_INIT(best_rows);       /* protected by intersect_scans_best */
  LINT_INIT(is_best_covering);
  LINT_INIT(best_index_scan_costs);

  cur_ror_scan= tree->ror_scans;
  while (cur_ror_scan != tree->ror_scans_end && !intersect->is_covering)
  {
    /* S= S + first(R); */
    if (ror_intersect_add(intersect, *cur_ror_scan))
      *(intersect_scans_end++)= *cur_ror_scan; 
    /* R= R-first(R); */
    cur_ror_scan++;
    
    if (intersect->total_cost < min_cost)
    {
      /* Local minimum found, save it */
      min_cost= intersect->total_cost;
      best_rows= (ha_rows)(intersect->records_fract* 
                           rows2double(param->table->file->records));
      is_best_covering= intersect->is_covering;
      intersect_scans_best= intersect_scans_end;
      best_index_scan_costs= intersect->index_scan_costs;
    }
  }

  /* Ok, return ROR-intersect plan if we have found one */
  *are_all_covering= intersect->is_covering;
  uint best_num= intersect_scans_best - intersect_scans;  
  TRP_ROR_INTERSECT *trp= NULL;
  if (intersect_scans_best && best_num > 1)
  {
    DBUG_EXECUTE("info",print_ror_scans_arr(param->table,
                                            "used for ROR-intersect",
                                            intersect_scans,
                                            intersect_scans_best););
    if (!(trp= new (param->mem_root) TRP_ROR_INTERSECT))
      DBUG_RETURN(trp);
    if (!(trp->first_scan= 
           (ROR_SCAN_INFO**)alloc_root(param->mem_root, 
                                       sizeof(ROR_SCAN_INFO*)*best_num)))
      DBUG_RETURN(NULL);
    memcpy(trp->first_scan, intersect_scans, best_num*sizeof(ROR_SCAN_INFO*));
    trp->last_scan=  trp->first_scan + best_num;
    trp->is_covering= is_best_covering;
    trp->read_cost= min_cost;
    trp->records= best_rows? best_rows : 1;
    trp->index_scan_costs= best_index_scan_costs;
  }  
  DBUG_RETURN(trp);
}


/*
  Get best covering ROR-intersection.
  SYNOPSIS
    get_best_covering_ror_intersect()
      param
      tree      SEL_TREE
      read_time Dont return table read plans with cost > read_time.

  RETURN 
    Best covering ROR-intersection plan 
    NULL if no plan found.

  NOTE
    get_best_ror_intersect must be called for a tree before calling this
    function for it. 
    This function invalidates tree->ror_scans member values.
  
  The following approximate algorithm is used:
    I=set of all covering indexes
    F=set of all fields to cover
    S={}

    do {
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
  uint nbits= param->fields_bitmap_size*8;

  for (ROR_SCAN_INFO **scan= tree->ror_scans; scan != ror_scans_end; ++scan)
    (*scan)->key_components= 
      param->table->key_info[(*scan)->keynr].key_parts;
    
  /*
    Run covering-ROR-search algorithm.
    Assume set I is [ror_scan .. ror_scans_end) 
  */
  
  /*I=set of all covering indexes */
  ror_scan_mark= tree->ror_scans;
  
  uchar buf[MAX_KEY/8+1];
  MY_BITMAP covered_fields;
  if (bitmap_init(&covered_fields, buf, nbits, false))
    DBUG_RETURN(0);
  bitmap_clear_all(&covered_fields);

  double total_cost= 0.0f;
  ha_rows records=0;
  bool all_covered;  
  
  /* Start will all scans and remove one by one until */
  DBUG_PRINT("info", ("Building covering ROR-intersection"));
  DBUG_EXECUTE("info", print_ror_scans_arr(param->table,
                                           "building covering ROR-I",
                                           ror_scan_mark, ror_scans_end););
  do {
    /*
      Update changed sorting info: 
        #covered fields,
	number of first not covered component 
      Calculate and save these values for each of remaining scans.
    */
    for (ROR_SCAN_INFO **scan= ror_scan_mark; scan != ror_scans_end; ++scan)
    {
      bitmap_subtract(&(*scan)->covered_fields, &covered_fields);
      (*scan)->used_fields_covered= 
        bitmap_bits_set(&(*scan)->covered_fields);
      (*scan)->first_uncovered_field= 
        bitmap_get_first(&(*scan)->covered_fields);
    }

    qsort(ror_scan_mark, ror_scans_end-ror_scan_mark, sizeof(ROR_SCAN_INFO*),
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
    bitmap_union(&covered_fields, &(*ror_scan_mark)->covered_fields);
    all_covered= bitmap_is_subset(&param->needed_fields, &covered_fields);
  } while (!all_covered && (++ror_scan_mark < ror_scans_end));
  
  if (!all_covered)
    DBUG_RETURN(NULL); /* should not happen actually */

  /*
    Ok, [tree->ror_scans .. ror_scan) holds covering index_intersection with
    cost total_cost.
  */
  DBUG_PRINT("info", ("Covering ROR-intersect scans cost: %g", total_cost));
  DBUG_EXECUTE("info", print_ror_scans_arr(param->table,
                                           "creating covering ROR-intersect",
                                           tree->ror_scans, ror_scan_mark););
  
  /* Add priority queue use cost. */
  total_cost += rows2double(records)*log(ror_scan_mark - tree->ror_scans) / 
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
  memcpy(trp->first_scan, ror_scan_mark, best_num*sizeof(ROR_SCAN_INFO*));
  trp->last_scan=  trp->first_scan + best_num;
  trp->is_covering= true;
  trp->read_cost= total_cost;
  trp->records= records;

  DBUG_RETURN(trp);
}


/*
  Get best "range" table read plan for given SEL_TREE. 
  Also update PARAM members and store ROR scans info in the SEL_TREE.
  SYNOPSIS
    get_quick_select_params
      param        parameters from test_quick_select
      tree         make range select for this SEL_TREE 
      index_read_must_be_used if true, assume 'index only' option will be set
                             (except for clustered PK indexes)
      read_time    don't create read plans with cost > read_time.
  RETURN
    Best range read plan 
    NULL if no plan found or error occurred
*/

static TRP_RANGE *get_key_scans_params(PARAM *param, SEL_TREE *tree,
                                       bool index_read_must_be_used, 
                                       double read_time)
{
  int idx;
  SEL_ARG **key,**end, **key_to_read= NULL;
  ha_rows best_records;
  TRP_RANGE* read_plan= NULL;
  bool pk_is_clustered= param->table->file->primary_key_is_clustered();
  DBUG_ENTER("get_key_scans_params");
  LINT_INIT(best_records); /* protected by key_to_read */
  /*
    Note that there may be trees that have type SEL_TREE::KEY but contain no 
    key reads at all, e.g. tree for expression "key1 is not null" where key1 
    is defined as "not null".
  */  
  DBUG_EXECUTE("info", print_sel_tree(param, tree, &tree->keys_map, 
                                      "tree scans"););
  tree->ror_scans_map.clear_all();
  tree->n_ror_scans= 0;
  for (idx= 0,key=tree->keys, end=key+param->keys;
       key != end ;
       key++,idx++)
  {
    ha_rows found_records;
    double found_read_time;
    if (*key)
    {
      uint keynr= param->real_keynr[idx];
      if ((*key)->type == SEL_ARG::MAYBE_KEY ||
          (*key)->maybe_flag)
        param->needed_reg->set_bit(keynr);
      
      bool read_index_only= index_read_must_be_used? true :
                            (bool)param->table->used_keys.is_set(keynr);

      found_records= check_quick_select(param, idx, *key);
      if (param->is_ror_scan)
      {
        tree->n_ror_scans++;
        tree->ror_scans_map.set_bit(idx);
      }
      if (found_records != HA_POS_ERROR && found_records > 2 &&
          read_index_only &&
          (param->table->file->index_flags(keynr) & HA_KEY_READ_ONLY) &&
          !(pk_is_clustered && keynr == param->table->primary_key))
      {
        /* We can resolve this by only reading through this key. */
        found_read_time= get_index_only_read_time(param,found_records,keynr);
      }
      else
      {
        /* 
          cost(read_through_index) = cost(disk_io) + cost(row_in_range_checks)
          The row_in_range check is in QUICK_RANGE_SELECT::cmp_next function.
        */
	found_read_time= (param->table->file->read_time(keynr,
						        param->range_count,
						        found_records)+
			  (double) found_records / TIME_FOR_COMPARE);
      }
      if (read_time > found_read_time && found_records != HA_POS_ERROR
          /*|| read_time == DBL_MAX*/ )
      {        
        read_time=    found_read_time;
        best_records= found_records; 
        key_to_read=  key;
      }

    }
  }

  DBUG_EXECUTE("info", print_sel_tree(param, tree, &tree->ror_scans_map,
                                      "ROR scans"););
  if (key_to_read)
  {
    idx= key_to_read - tree->keys;
    if ((read_plan= new (param->mem_root) TRP_RANGE(*key_to_read, idx)))
    {
      read_plan->records= best_records;
      read_plan->is_ror= tree->ror_scans_map.is_set(idx);
      read_plan->read_cost= read_time;
      DBUG_PRINT("info",("Returning range plan for key %s, cost %g", 
                         param->table->key_info[param->real_keynr[idx]].name,
                         read_plan->read_cost));
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
  for(TRP_RANGE **range_scan= range_scans; range_scan != range_scans_end;
      range_scan++)
  {
    if (!(quick= (QUICK_RANGE_SELECT*)
           ((*range_scan)->make_quick(param, false, &quick_imerge->alloc)))||
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
                                        retrieve_full_rows? (!is_covering):false,
                                        parent_alloc)))
  {
    DBUG_EXECUTE("info", print_ror_scans_arr(param->table, 
                                             "creating ROR-intersect",
                                             first_scan, last_scan););
    alloc= parent_alloc? parent_alloc: &quick_intrsect->alloc;
    for(; first_scan != last_scan;++first_scan)
    {
      if (!(quick= get_quick_select(param, (*first_scan)->idx,
                                    (*first_scan)->sel_arg, alloc)) ||
          quick_intrsect->push_quick_back(quick))
      {
        delete quick_intrsect;
        DBUG_RETURN(NULL);
      }
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
    It is currently impossible to construct a ROR-union that will 
    not retrieve full rows, ingore retrieve_full_rows.
  */
  if ((quick_roru= new QUICK_ROR_UNION_SELECT(param->thd, param->table)))
  {
    for(scan= first_ror; scan != last_ror; scan++)
    {
      if (!(quick= (*scan)->make_quick(param, false, &quick_roru->alloc)) || 
          quick_roru->push_quick_back(quick))
        DBUG_RETURN(NULL);
    }
    quick_roru->records= records;
    quick_roru->read_time= read_cost;
  }
  DBUG_RETURN(quick_roru);
}

/****************************************************************************/
	/* make a select tree of all keys in condition */

static SEL_TREE *get_mm_tree(PARAM *param,COND *cond)
{
  SEL_TREE *tree=0;
  DBUG_ENTER("get_mm_tree");

  if (cond->type() == Item::COND_ITEM)
  {
    List_iterator<Item> li(*((Item_cond*) cond)->argument_list());

    if (((Item_cond*) cond)->functype() == Item_func::COND_AND_FUNC)
    {
      tree=0;
      Item *item;
      while ((item=li++))
      {
	SEL_TREE *new_tree=get_mm_tree(param,item);
	if (param->thd->is_fatal_error)
	  DBUG_RETURN(0);	// out of memory
	tree=tree_and(param,tree,new_tree);
	if (tree && tree->type == SEL_TREE::IMPOSSIBLE)
	  break;
      }
    }
    else
    {						// COND OR
      tree=get_mm_tree(param,li++);
      if (tree)
      {
	Item *item;
	while ((item=li++))
	{
	  SEL_TREE *new_tree=get_mm_tree(param,item);
	  if (!new_tree)
	    DBUG_RETURN(0);	// out of memory
	  tree=tree_or(param,tree,new_tree);
	  if (!tree || tree->type == SEL_TREE::ALWAYS)
	    break;
	}
      }
    }
    DBUG_RETURN(tree);
  }
  /* Here when simple cond */
  if (cond->const_item())
  {
    if (cond->val_int())
      DBUG_RETURN(new SEL_TREE(SEL_TREE::ALWAYS));
    DBUG_RETURN(new SEL_TREE(SEL_TREE::IMPOSSIBLE));
  }

  table_map ref_tables=cond->used_tables();
  if (cond->type() != Item::FUNC_ITEM)
  {						// Should be a field
    if ((ref_tables & param->current_table) ||
	(ref_tables & ~(param->prev_tables | param->read_tables)))
      DBUG_RETURN(0);
    DBUG_RETURN(new SEL_TREE(SEL_TREE::MAYBE));
  }

  Item_func *cond_func= (Item_func*) cond;
  if (cond_func->select_optimize() == Item_func::OPTIMIZE_NONE)
    DBUG_RETURN(0);				// Can't be calculated

  if (cond_func->functype() == Item_func::BETWEEN)
  {
    if (cond_func->arguments()[0]->type() == Item::FIELD_ITEM)
    {
      Field *field=((Item_field*) (cond_func->arguments()[0]))->field;
      Item_result cmp_type=field->cmp_type();
      DBUG_RETURN(tree_and(param,
			   get_mm_parts(param, field,
					Item_func::GE_FUNC,
					cond_func->arguments()[1], cmp_type),
			   get_mm_parts(param, field,
					Item_func::LE_FUNC,
					cond_func->arguments()[2], cmp_type)));
    }
    DBUG_RETURN(0);
  }
  if (cond_func->functype() == Item_func::IN_FUNC)
  {						// COND OR
    Item_func_in *func=(Item_func_in*) cond_func;
    if (func->key_item()->type() == Item::FIELD_ITEM)
    {
      Field *field=((Item_field*) (func->key_item()))->field;
      Item_result cmp_type=field->cmp_type();
      tree= get_mm_parts(param,field,Item_func::EQ_FUNC,
			 func->arguments()[1],cmp_type);
      if (!tree)
	DBUG_RETURN(tree);			// Not key field
      for (uint i=2 ; i < func->argument_count(); i++)
      {
	SEL_TREE *new_tree=get_mm_parts(param,field,Item_func::EQ_FUNC,
					func->arguments()[i],cmp_type);
	tree=tree_or(param,tree,new_tree);
      }
      DBUG_RETURN(tree);
    }
    DBUG_RETURN(0);				// Can't optimize this IN
  }

  if (ref_tables & ~(param->prev_tables | param->read_tables |
		     param->current_table))
    DBUG_RETURN(0);				// Can't be calculated yet
  if (!(ref_tables & param->current_table))
    DBUG_RETURN(new SEL_TREE(SEL_TREE::MAYBE)); // This may be false or true

  /* check field op const */
  /* btw, ft_func's arguments()[0] isn't FIELD_ITEM.  SerG*/
  if (cond_func->arguments()[0]->type() == Item::FIELD_ITEM)
  {
    tree= get_mm_parts(param,
		       ((Item_field*) (cond_func->arguments()[0]))->field,
		       cond_func->functype(),
		       cond_func->arg_count > 1 ? cond_func->arguments()[1] :
		       0,
		       ((Item_field*) (cond_func->arguments()[0]))->field->
		       cmp_type());
  }
  /* check const op field */
  if (!tree &&
      cond_func->have_rev_func() &&
      cond_func->arguments()[1]->type() == Item::FIELD_ITEM)
  {
    DBUG_RETURN(get_mm_parts(param,
			     ((Item_field*)
			      (cond_func->arguments()[1]))->field,
			     ((Item_bool_func2*) cond_func)->rev_functype(),
			     cond_func->arguments()[0],
			     ((Item_field*)
			      (cond_func->arguments()[1]))->field->cmp_type()
			     ));
  }
  DBUG_RETURN(tree);
}


static SEL_TREE *
get_mm_parts(PARAM *param, Field *field, Item_func::Functype type, 
	     Item *value, Item_result cmp_type)
{
  bool ne_func= FALSE;
  DBUG_ENTER("get_mm_parts");
  if (field->table != param->table)
    DBUG_RETURN(0);

  if (type == Item_func::NE_FUNC)
  {
    ne_func= TRUE;
    type= Item_func::LT_FUNC;
  }

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
	sel_arg=get_mm_leaf(param,key_part->field,key_part,type,value);
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
      tree->keys[key_part->key]=sel_add(tree->keys[key_part->key],sel_arg);
      tree->keys_map.set_bit(key_part->key);
    }
  }

  if (ne_func)
  {
    SEL_TREE *tree2= get_mm_parts(param, field, Item_func::GT_FUNC,
                                  value, cmp_type);
    if (tree2)
      tree= tree_or(param,tree,tree2);
  }

  DBUG_RETURN(tree);
}


static SEL_ARG *
get_mm_leaf(PARAM *param, Field *field, KEY_PART *key_part,
	    Item_func::Functype type,Item *value)
{
  uint maybe_null=(uint) field->real_maybe_null(), copies;
  uint field_length=field->pack_length()+maybe_null;
  SEL_ARG *tree;
  char *str, *str2;
  DBUG_ENTER("get_mm_leaf");

  if (type == Item_func::LIKE_FUNC)
  {
    bool like_error;
    char buff1[MAX_FIELD_WIDTH],*min_str,*max_str;
    String tmp(buff1,sizeof(buff1),value->collation.collation),*res;
    uint length,offset,min_length,max_length;

    if (!field->optimize_range(param->real_keynr[key_part->key]))
      DBUG_RETURN(0);				// Can't optimize this
    if (!(res= value->val_str(&tmp)))
      DBUG_RETURN(&null_element);

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
      DBUG_RETURN(0);				// Can only optimize strings

    offset=maybe_null;
    length=key_part->part_length;
    if (field->type() == FIELD_TYPE_BLOB)
    {
      offset+=HA_KEY_BLOB_LENGTH;
      field_length=key_part->part_length-HA_KEY_BLOB_LENGTH;
    }
    else
    {
      if (length < field_length)
	length=field_length;			// Only if overlapping key
      else
	field_length=length;
    }
    length+=offset;
    if (!(min_str= (char*) alloc_root(param->mem_root, length*2)))
      DBUG_RETURN(0);
    max_str=min_str+length;
    if (maybe_null)
      max_str[0]= min_str[0]=0;

    like_error= my_like_range(field->charset(),
                                  res->ptr(),res->length(),
				  wild_prefix,wild_one,wild_many,
                                  field_length, 
				  min_str+offset, max_str+offset,
				  &min_length,&max_length);

    if (like_error)				// Can't optimize with LIKE
      DBUG_RETURN(0);
    if (offset != maybe_null)			// Blob
    {
      int2store(min_str+maybe_null,min_length);
      int2store(max_str+maybe_null,max_length);
    }
    DBUG_RETURN(new SEL_ARG(field,min_str,max_str));
  }

  if (!value)					// IS NULL or IS NOT NULL
  {
    if (field->table->outer_join)		// Can't use a key on this
      DBUG_RETURN(0);
    if (!maybe_null)				// Not null field
      DBUG_RETURN(type == Item_func::ISNULL_FUNC ? &null_element : 0);
    if (!(tree=new SEL_ARG(field,is_null_string,is_null_string)))
      DBUG_RETURN(0);		// out of memory
    if (type == Item_func::ISNOTNULL_FUNC)
    {
      tree->min_flag=NEAR_MIN;		    /* IS NOT NULL ->  X > NULL */
      tree->max_flag=NO_MAX_RANGE;
    }
    DBUG_RETURN(tree);
  }

  if (!field->optimize_range(param->real_keynr[key_part->key]) &&
      type != Item_func::EQ_FUNC &&
      type != Item_func::EQUAL_FUNC)
    DBUG_RETURN(0);				// Can't optimize this

  /*
    We can't always use indexes when comparing a string index to a number
    cmp_type() is checked to allow compare of dates to numbers
  */
  if (field->result_type() == STRING_RESULT &&
      value->result_type() != STRING_RESULT &&
      field->cmp_type() != value->result_type())
    DBUG_RETURN(0);

  if (value->save_in_field(field, 1) > 0)
  {
    /* This happens when we try to insert a NULL field in a not null column */
    DBUG_RETURN(&null_element);			// cmp with NULL is never true
  }
  /* Get local copy of key */
  copies= 1;
  if (field->key_type() == HA_KEYTYPE_VARTEXT)
    copies= 2;
  str= str2= (char*) alloc_root(param->mem_root,
				(key_part->part_length+maybe_null)*copies+1);
  if (!str)
    DBUG_RETURN(0);
  if (maybe_null)
    *str= (char) field->is_real_null();		// Set to 1 if null
  field->get_key_image(str+maybe_null,key_part->part_length,
		       field->charset(),key_part->image_type);
  if (copies == 2)
  {
    /*
      The key is stored as 2 byte length + key
      key doesn't match end space. In other words, a key 'X ' should match
      all rows between 'X' and 'X           ...'
    */
    uint length= uint2korr(str+maybe_null);
    str2= str+ key_part->part_length + maybe_null;
    /* remove end space */
    while (length > 0 && str[length+HA_KEY_BLOB_LENGTH+maybe_null-1] == ' ')
      length--;
    int2store(str+maybe_null, length);
    /* Create key that is space filled */
    memcpy(str2, str, length + HA_KEY_BLOB_LENGTH + maybe_null);
    bfill(str2+ length+ HA_KEY_BLOB_LENGTH +maybe_null,
	  key_part->part_length-length - HA_KEY_BLOB_LENGTH, ' ');
    int2store(str2+maybe_null, key_part->part_length - HA_KEY_BLOB_LENGTH);
  }
  if (!(tree=new SEL_ARG(field,str,str2)))
    DBUG_RETURN(0);		// out of memory

  switch (type) {
  case Item_func::LT_FUNC:
    if (field_is_equal_to_item(field,value))
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
    if (field_is_equal_to_item(field,value))
      tree->min_flag=NEAR_MIN;
    /* fall through */
  case Item_func::GE_FUNC:
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
  DBUG_RETURN(tree);
}


/******************************************************************************
** Tree manipulation functions
** If tree is 0 it means that the condition can't be tested. It refers
** to a non existent table or to a field in current table with isn't a key.
** The different tree flags:
** IMPOSSIBLE:	 Condition is never true
** ALWAYS:	 Condition is always true
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

#define CLONE_KEY1_MAYBE 1
#define CLONE_KEY2_MAYBE 2
#define swap_clone_flag(A) ((A & 1) << 1) | ((A & 2) >> 1)


static SEL_TREE *
tree_and(PARAM *param,SEL_TREE *tree1,SEL_TREE *tree2)
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

  key_map  result_keys;
  result_keys.clear_all();
  /* Join the trees key per key */
  SEL_ARG **key1,**key2,**end;
  for (key1= tree1->keys,key2= tree2->keys,end=key1+param->keys ;
       key1 != end ; key1++,key2++)
  {
    uint flag=0;
    if (*key1 || *key2)
    {
      if (*key1 && !(*key1)->simple_key())
	flag|=CLONE_KEY1_MAYBE;
      if (*key2 && !(*key2)->simple_key())
	flag|=CLONE_KEY2_MAYBE;
      *key1=key_and(*key1,*key2,flag);
      if ((*key1)->type == SEL_ARG::IMPOSSIBLE)
      {
	tree1->type= SEL_TREE::IMPOSSIBLE;
        DBUG_RETURN(tree1);
      }
      result_keys.set_bit(key1 - tree1->keys);
#ifdef EXTRA_DEBUG
      (*key1)->test_use_count(*key1);
#endif
    }
  }
  tree1->keys_map= result_keys;
  /* dispose index_merge if there is a "range" option */
  if (!result_keys.is_clear_all())
  {
    tree1->merges.empty();
    DBUG_RETURN(tree1);
  }

  /* ok, both trees are index_merge trees */
  imerge_list_and_list(&tree1->merges, &tree2->merges);
  DBUG_RETURN(tree1);
}


/*
  Check if two SEL_TREES can be combined into one (i.e. a single key range 
  read can be constructed for "cond_of_tree1 OR cond_of_tree2" ) without 
  using index_merge.
*/

bool sel_trees_can_be_ored(SEL_TREE *tree1, SEL_TREE *tree2, PARAM* param)
{
  key_map common_keys= tree1->keys_map;
  DBUG_ENTER("sel_trees_can_be_ored");
  common_keys.intersect(tree2->keys_map);

  if (common_keys.is_clear_all())
    DBUG_RETURN(false);
  
  /* trees have a common key, check if they refer to same key part */  
  SEL_ARG **key1,**key2;
  for (uint key_no=0; key_no < param->keys; key_no++)
  {
    if (common_keys.is_set(key_no))
    {
      key1= tree1->keys + key_no;
      key2= tree2->keys + key_no;
      if ((*key1)->part == (*key2)->part)
      {
        DBUG_RETURN(true);
      }
    }
  }
  DBUG_RETURN(false);
}

static SEL_TREE *
tree_or(PARAM *param,SEL_TREE *tree1,SEL_TREE *tree2)
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

  SEL_TREE *result= 0;
  key_map  result_keys;
  result_keys.clear_all();
  if (sel_trees_can_be_ored(tree1, tree2, param))
  {
    /* Join the trees key per key */
    SEL_ARG **key1,**key2,**end;
    for (key1= tree1->keys,key2= tree2->keys,end= key1+param->keys ;
         key1 != end ; key1++,key2++)
    {
      *key1=key_or(*key1,*key2);
      if (*key1)
      {
        result=tree1;				// Added to tree1
        result_keys.set_bit(key1 - tree1->keys);
#ifdef EXTRA_DEBUG
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
      SEL_IMERGE *merge;
      /* both trees are "range" trees, produce new index merge structure */
      if (!(result= new SEL_TREE()) || !(merge= new SEL_IMERGE()) ||
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
        result= new SEL_TREE(SEL_TREE::ALWAYS);
      else
        result= tree1;
    }
    else
    {
      /* one tree is index merge tree and another is range tree */
      if (tree1->merges.is_empty())
        swap(SEL_TREE*, tree1, tree2);

      /* add tree2 to tree1->merges, checking if it collapses to ALWAYS */
      if (imerge_list_or_tree(param, &tree1->merges, tree2))
        result= new SEL_TREE(SEL_TREE::ALWAYS);
      else
        result= tree1;
    }
  }
  DBUG_RETURN(result);
}


/* And key trees where key1->part < key2 -> part */

static SEL_ARG *
and_all_keys(SEL_ARG *key1,SEL_ARG *key2,uint clone_flag)
{
  SEL_ARG *next;
  ulong use_count=key1->use_count;

  if (key1->elements != 1)
  {
    key2->use_count+=key1->elements-1;
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
      SEL_ARG *tmp=key_and(next->next_key_part,key2,clone_flag);
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


static SEL_ARG *
key_and(SEL_ARG *key1,SEL_ARG *key2,uint clone_flag)
{
  if (!key1)
    return key2;
  if (!key2)
    return key1;
  if (key1->part != key2->part)
  {
    if (key1->part > key2->part)
    {
      swap(SEL_ARG *,key1,key2);
      clone_flag=swap_clone_flag(clone_flag);
    }
    // key1->part < key2->part
    key1->use_count--;
    if (key1->use_count > 0)
      if (!(key1= key1->clone_tree()))
	return 0;				// OOM
    return and_all_keys(key1,key2,clone_flag);
  }

  if (((clone_flag & CLONE_KEY2_MAYBE) &&
       !(clone_flag & CLONE_KEY1_MAYBE) &&
       key2->type != SEL_ARG::MAYBE_KEY) ||
      key1->type == SEL_ARG::MAYBE_KEY)
  {						// Put simple key in key2
    swap(SEL_ARG *,key1,key2);
    clone_flag=swap_clone_flag(clone_flag);
  }

  // If one of the key is MAYBE_KEY then the found region may be smaller
  if (key2->type == SEL_ARG::MAYBE_KEY)
  {
    if (key1->use_count > 1)
    {
      key1->use_count--;
      if (!(key1=key1->clone_tree()))
	return 0;				// OOM
      key1->use_count++;
    }
    if (key1->type == SEL_ARG::MAYBE_KEY)
    {						// Both are maybe key
      key1->next_key_part=key_and(key1->next_key_part,key2->next_key_part,
				 clone_flag);
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
	return and_all_keys(key1,key2,clone_flag);
      }
      key2->use_count--;			// Key2 doesn't have a tree
    }
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
    SEL_ARG *next=key_and(e1->next_key_part,e2->next_key_part,clone_flag);
    e1->increment_use_count(1);
    e2->increment_use_count(1);
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


static SEL_ARG *
key_or(SEL_ARG *key1,SEL_ARG *key2)
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

  if (key1->part != key2->part)
  {
    key1->free_tree();
    key2->free_tree();
    return 0;					// Can't optimize this
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
      swap(SEL_ARG *,key1,key2);
    }
    else if (!(key1=key1->clone_tree()))
      return 0;					// OOM
  }

  // Add tree at key2 to tree at key1
  bool key2_shared=key2->use_count != 0;
  key1->maybe_flag|=key2->maybe_flag;

  for (key2=key2->first(); key2; )
  {
    SEL_ARG *tmp=key1->find_range(key2);	// Find key1.min <= key2.min
    int cmp;

    if (!tmp)
    {
      tmp=key1->first();			// tmp.min > key2.min
      cmp= -1;
    }
    else if ((cmp=tmp->cmp_max_to_min(key2)) < 0)
    {						// Found tmp.max < key2.min
      SEL_ARG *next=tmp->next;
      if (cmp == -2 && eq_tree(tmp->next_key_part,key2->next_key_part))
      {
	// Join near ranges like tmp.max < 0 and key2.min >= 0
	SEL_ARG *key2_next=key2->next;
	if (key2_shared)
	{
	  if (!(key2=new SEL_ARG(*key2)))
	    return 0;		// out of memory
	  key2->increment_use_count(key1->use_count+1);
	  key2->next=key2_next;			// New copy of key2
	}
	key2->copy_min(tmp);
	if (!(key1=key1->tree_delete(tmp)))
	{					// Only one key in tree
	  key1=key2;
	  key1->make_root();
	  key2=key2_next;
	  break;
	}
      }
      if (!(tmp=next))				// tmp.min > key2.min
	break;					// Copy rest of key2
    }
    if (cmp < 0)
    {						// tmp.min > key2.min
      int tmp_cmp;
      if ((tmp_cmp=tmp->cmp_min_to_max(key2)) > 0) // if tmp.min > key2.max
      {
	if (tmp_cmp == 2 && eq_tree(tmp->next_key_part,key2->next_key_part))
	{					// ranges are connected
	  tmp->copy_min_to_min(key2);
	  key1->merge_flags(key2);
	  if (tmp->min_flag & NO_MIN_RANGE &&
	      tmp->max_flag & NO_MAX_RANGE)
	  {
	    if (key1->maybe_flag)
	      return new SEL_ARG(SEL_ARG::MAYBE_KEY);
	    return 0;
	  }
	  key2->increment_use_count(-1);	// Free not used tree
	  key2=key2->next;
	  continue;
	}
	else
	{
	  SEL_ARG *next=key2->next;		// Keys are not overlapping
	  if (key2_shared)
	  {
	    SEL_ARG *tmp= new SEL_ARG(*key2);	// Must make copy
	    if (!tmp)
	      return 0;				// OOM
	    key1=key1->insert(tmp);
	    key2->increment_use_count(key1->use_count+1);
	  }
	  else
	    key1=key1->insert(key2);		// Will destroy key2_root
	  key2=next;
	  continue;
	}
      }
    }

    // tmp.max >= key2.min && tmp.min <= key.max  (overlapping ranges)
    if (eq_tree(tmp->next_key_part,key2->next_key_part))
    {
      if (tmp->is_same(key2))
      {
	tmp->merge_flags(key2);			// Copy maybe flags
	key2->increment_use_count(-1);		// Free not used tree
      }
      else
      {
	SEL_ARG *last=tmp;
	while (last->next && last->next->cmp_min_to_max(key2) <= 0 &&
	       eq_tree(last->next->next_key_part,key2->next_key_part))
	{
	  SEL_ARG *save=last;
	  last=last->next;
	  key1=key1->tree_delete(save);
	}
	if (last->copy_min(key2) || last->copy_max(key2))
	{					// Full range
	  key1->free_tree();
	  for (; key2 ; key2=key2->next)
	    key2->increment_use_count(-1);	// Free not used tree
	  if (key1->maybe_flag)
	    return new SEL_ARG(SEL_ARG::MAYBE_KEY);
	  return 0;
	}
      }
      key2=key2->next;
      continue;
    }

    if (cmp >= 0 && tmp->cmp_min_to_min(key2) < 0)
    {						// tmp.min <= x < key2.min
      SEL_ARG *new_arg=tmp->clone_first(key2);
      if (!new_arg)
	return 0;				// OOM
      if ((new_arg->next_key_part= key1->next_key_part))
	new_arg->increment_use_count(key1->use_count+1);
      tmp->copy_min_to_min(key2);
      key1=key1->insert(new_arg);
    }

    // tmp.min >= key2.min && tmp.min <= key2.max
    SEL_ARG key(*key2);				// Get copy we can modify
    for (;;)
    {
      if (tmp->cmp_min_to_min(&key) > 0)
      {						// key.min <= x < tmp.min
	SEL_ARG *new_arg=key.clone_first(tmp);
	if (!new_arg)
	  return 0;				// OOM
	if ((new_arg->next_key_part=key.next_key_part))
	  new_arg->increment_use_count(key1->use_count+1);
	key1=key1->insert(new_arg);
      }
      if ((cmp=tmp->cmp_max_to_max(&key)) <= 0)
      {						// tmp.min. <= x <= tmp.max
	tmp->maybe_flag|= key.maybe_flag;
	key.increment_use_count(key1->use_count+1);
	tmp->next_key_part=key_or(tmp->next_key_part,key.next_key_part);
	if (!cmp)				// Key2 is ready
	  break;
	key.copy_max_to_min(tmp);
	if (!(tmp=tmp->next))
	{
	  SEL_ARG *tmp2= new SEL_ARG(key);
	  if (!tmp2)
	    return 0;				// OOM
	  key1=key1->insert(tmp2);
	  key2=key2->next;
	  goto end;
	}
	if (tmp->cmp_min_to_max(&key) > 0)
	{
	  SEL_ARG *tmp2= new SEL_ARG(key);
	  if (!tmp2)
	    return 0;				// OOM
	  key1=key1->insert(tmp2);
	  break;
	}
      }
      else
      {
	SEL_ARG *new_arg=tmp->clone_last(&key); // tmp.min <= x <= key.max
	if (!new_arg)
	  return 0;				// OOM
	tmp->copy_max_to_min(&key);
	tmp->increment_use_count(key1->use_count+1);
	new_arg->next_key_part=key_or(tmp->next_key_part,key.next_key_part);
	key1=key1->insert(new_arg);
	break;
      }
    }
    key2=key2->next;
  }

end:
  while (key2)
  {
    SEL_ARG *next=key2->next;
    if (key2_shared)
    {
      SEL_ARG *tmp=new SEL_ARG(*key2);		// Must make copy
      if (!tmp)
	return 0;
      key2->increment_use_count(key1->use_count+1);
      key1=key1->insert(tmp);
    }
    else
      key1=key1->insert(key2);			// Will destroy key2_root
    key2=next;
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
  SEL_ARG *element,**par,*last_element;

  LINT_INIT(par); LINT_INIT(last_element);
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
** Remove a element from the tree
** This also frees all sub trees that is used by the element
*/

SEL_ARG *
SEL_ARG::tree_delete(SEL_ARG *key)
{
  enum leaf_color remove_color;
  SEL_ARG *root,*nod,**par,*fix_par;
  root=this; this->parent= 0;

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
    return 0;					// Maybe root later
  if (remove_color == BLACK)
    root=rb_delete_fixup(root,nod,fix_par);
  test_rb_tree(root,root->parent);

  root->use_count=this->use_count;		// Fix root counters
  root->elements=this->elements-1;
  root->maybe_flag=this->maybe_flag;
  return root;
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


	/* Test that the proporties for a red-black tree holds */

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


void SEL_ARG::test_use_count(SEL_ARG *root)
{
  if (this == root && use_count != 1)
  {
    sql_print_error("Note: Use_count: Wrong count %lu for root",use_count);
    return;
  }
  if (this->type != SEL_ARG::KEY_RANGE)
    return;
  uint e_count=0;
  for (SEL_ARG *pos=first(); pos ; pos=pos->next)
  {
    e_count++;
    if (pos->next_key_part)
    {
      ulong count=count_key_part_usage(root,pos->next_key_part);
      if (count > pos->next_key_part->use_count)
      {
	sql_print_error("Note: Use_count: Wrong count for key at %lx, %lu should be %lu",
			pos,pos->next_key_part->use_count,count);
	return;
      }
      pos->next_key_part->test_use_count(root);
    }
  }
  if (e_count != elements)
    sql_print_error("Warning: Wrong use count: %u for tree at %lx", e_count,
		    (gptr) this);
}

#endif



/*****************************************************************************
** Check how many records we will find by using the found tree
** NOTE
**  param->table->quick_* and param->range_count (and maybe others) are 
**  updated with data of given key scan.
*****************************************************************************/

static ha_rows
check_quick_select(PARAM *param,uint idx,SEL_ARG *tree)
{
  ha_rows records;
  bool    cpk_scan;
  uint key;
  DBUG_ENTER("check_quick_select");

  if (!tree)
    DBUG_RETURN(HA_POS_ERROR);			// Can't use it
  param->max_key_part=0;
  param->range_count=0;
  key= param->real_keynr[idx];

  if (tree->type == SEL_ARG::IMPOSSIBLE)
    DBUG_RETURN(0L);				// Impossible select. return
  if (tree->type != SEL_ARG::KEY_RANGE || tree->part != 0)
    DBUG_RETURN(HA_POS_ERROR);				// Don't use tree

  enum ha_key_alg key_alg= param->table->key_info[key].algorithm;
  if ((key_alg != HA_KEY_ALG_BTREE) && (key_alg!= HA_KEY_ALG_UNDEF))
  {
    /* Records are not ordered by rowid for other types of indexes. */
    param->is_ror_scan= false;
    cpk_scan= false;
  }
  else
  {
    /*
      Clustered PK scan is a special case, check_quick_keys doesn't recognize
      CPK scans as ROR scans (while actually any CPK scan is a ROR scan).
    */
    cpk_scan= (param->table->primary_key == param->real_keynr[idx]) &&
              param->table->file->primary_key_is_clustered();
    param->is_ror_scan= !cpk_scan; 
  }

  records=check_quick_keys(param,idx,tree,param->min_key,0,param->max_key,0);
  if (records != HA_POS_ERROR)
  {    
    param->table->quick_keys.set_bit(key);
    param->table->quick_rows[key]=records;
    param->table->quick_key_parts[key]=param->max_key_part+1;
    
    if (cpk_scan)
      param->is_ror_scan= true;
  }
  DBUG_RETURN(records);
}


/* 
  SYNOPSIS
    check_quick_keys()
      param
      idx          key to use, its number in list of used keys (that is, 
                   param->real_keynr[idx] holds the key number in table)
      
      key_tree     SEL_ARG tree which cost is calculated.
      min_key      buffer with min key value tuple
      min_key_flag 
      max_key      buffer with max key value tuple
      max_key_flag

  NOTE
    The function does the recursive descent on the tree via left, right, and
    next_key_part edges. The #rows estimates are calculated at the leaf nodes.

    param->min_key and param->max_key are used to hold key segment values.

    The side effects are:
    param->max_key_part is updated to hold the maximum number of key parts used
      in scan minus 1.
    param->range_count is updated.
    param->is_ror_scan is updated.
*/

static ha_rows
check_quick_keys(PARAM *param,uint idx,SEL_ARG *key_tree,
		 char *min_key,uint min_key_flag, char *max_key,
		 uint max_key_flag)
{
  ha_rows records=0,tmp;

  param->max_key_part=max(param->max_key_part,key_tree->part);
  if (key_tree->left != &null_element)
  {
    param->is_ror_scan= false;
    records=check_quick_keys(param,idx,key_tree->left,min_key,min_key_flag,
			     max_key,max_key_flag);
    if (records == HA_POS_ERROR)			// Impossible
      return records;
  }

  uint tmp_min_flag,tmp_max_flag,keynr;
  char *tmp_min_key=min_key,*tmp_max_key=max_key;

  key_tree->store(param->key[idx][key_tree->part].part_length,
		  &tmp_min_key,min_key_flag,&tmp_max_key,max_key_flag);
  uint min_key_length= (uint) (tmp_min_key- param->min_key);
  uint max_key_length= (uint) (tmp_max_key- param->max_key);

  if (key_tree->next_key_part &&
      key_tree->next_key_part->part == key_tree->part+1 &&
      key_tree->next_key_part->type == SEL_ARG::KEY_RANGE)
  {						// const key as prefix
    if (min_key_length == max_key_length &&
	!memcmp(min_key,max_key, (uint) (tmp_max_key - max_key)) &&
	!key_tree->min_flag && !key_tree->max_flag)
    {
      tmp=check_quick_keys(param,idx,key_tree->next_key_part,
			   tmp_min_key, min_key_flag | key_tree->min_flag,
			   tmp_max_key, max_key_flag | key_tree->max_flag);
      goto end;					// Ugly, but efficient
    }
    else
      param->is_ror_scan= false;

    tmp_min_flag=key_tree->min_flag;
    tmp_max_flag=key_tree->max_flag;
    if (!tmp_min_flag)
      key_tree->next_key_part->store_min_key(param->key[idx], &tmp_min_key,
					     &tmp_min_flag);
    if (!tmp_max_flag)
      key_tree->next_key_part->store_max_key(param->key[idx], &tmp_max_key,
					     &tmp_max_flag);
    min_key_length= (uint) (tmp_min_key- param->min_key);
    max_key_length= (uint) (tmp_max_key- param->max_key);
  }
  else
  {
    tmp_min_flag=min_key_flag | key_tree->min_flag;
    tmp_max_flag=max_key_flag | key_tree->max_flag;
  }

  keynr=param->real_keynr[idx];
  param->range_count++;
  if (!tmp_min_flag && ! tmp_max_flag &&
      (uint) key_tree->part+1 == param->table->key_info[keynr].key_parts &&
      (param->table->key_info[keynr].flags & (HA_NOSAME | HA_END_SPACE_KEY)) ==
      HA_NOSAME &&
      min_key_length == max_key_length &&
      !memcmp(param->min_key,param->max_key,min_key_length))
    tmp=1;					// Max one record
  else
  {
    if (param->is_ror_scan)
    {
      if (!(min_key_length == max_key_length &&
            !memcmp(min_key,max_key, (uint) (tmp_max_key - max_key)) &&
            !key_tree->min_flag && !key_tree->max_flag && 
            is_key_scan_ror(param, keynr, key_tree->part + 1)))
        param->is_ror_scan= false;
    }

    if (tmp_min_flag & GEOM_FLAG)
    {
      tmp= param->table->file->
	records_in_range((int) keynr, (byte*)(param->min_key),
			 min_key_length,
			 (ha_rkey_function)(tmp_min_flag ^ GEOM_FLAG),
			 (byte *)NullS, 0, HA_READ_KEY_EXACT);
    }
    else
    {
      tmp=param->table->file->
	records_in_range((int) keynr,
			 (byte*) (!min_key_length ? NullS :
				  param->min_key),
			 min_key_length,
                         tmp_min_flag & NEAR_MIN ?
			  HA_READ_AFTER_KEY : HA_READ_KEY_EXACT,
			 (byte*) (!max_key_length ? NullS :
				  param->max_key),
			 max_key_length,
			 (tmp_max_flag & NEAR_MAX ?
			  HA_READ_BEFORE_KEY : HA_READ_AFTER_KEY));
    }
  }
 end:
  if (tmp == HA_POS_ERROR)			// Impossible range
    return tmp;
  records+=tmp;
  if (key_tree->right != &null_element)
  {
    param->is_ror_scan= false;
    tmp=check_quick_keys(param,idx,key_tree->right,min_key,min_key_flag,
			 max_key,max_key_flag);
    if (tmp == HA_POS_ERROR)
      return tmp;
    records+=tmp;
  }
  return records;
}

/*
  Check if key scan on key keynr with first nparts key parts fixed is a
  ROR scan. This function doesn't handle clustered PK scans or HASH index 
  scans.
*/
static bool is_key_scan_ror(PARAM *param, uint keynr, uint8 nparts)
{
  KEY *table_key= param->table->key_info + keynr;
  KEY_PART_INFO *key_part= table_key->key_part + nparts;
  KEY_PART_INFO *key_part_end= table_key->key_part + 
                               table_key->key_parts;
  
  if (key_part == key_part_end)
    return true;
  uint pk_number= param->table->primary_key;
  if (!param->table->file->primary_key_is_clustered() || pk_number == MAX_KEY)
    return false;

  KEY_PART_INFO *pk_part= param->table->key_info[pk_number].key_part;
  KEY_PART_INFO *pk_part_end= pk_part + 
                              param->table->key_info[pk_number].key_parts;
  for(;(key_part!=key_part_end) && (pk_part != pk_part_end); 
      ++key_part, ++pk_part)
  {
    if (key_part->field != pk_part->field)
      return false; 
  }
  return (key_part == key_part_end);
}


/*
  Create a QUICK_RANGE_SELECT from given key and SEL_ARG tree for that key.
  This uses it's own malloc tree.
  SYNOPSIS
    get_quick_select()
      param 
      idx          index of used key in param->key.
      key_tree     SEL_ARG tree for the used key 
      parent_alloc if not NULL, use it to allocate memory for 
                   quick select data. Otherwise use quick->alloc.

  The caller should call QUICK_SELCT::init for returned quick select
*/
QUICK_RANGE_SELECT *
get_quick_select(PARAM *param,uint idx,SEL_ARG *key_tree,
                 MEM_ROOT *parent_alloc)
{
  QUICK_RANGE_SELECT *quick;
  DBUG_ENTER("get_quick_select");
  if ((quick=new QUICK_RANGE_SELECT(param->thd, param->table,
                                    param->real_keynr[idx],test(parent_alloc),
                                    parent_alloc)))
  {
    if (quick->error ||
	get_quick_keys(param,quick,param->key[idx],key_tree,param->min_key,0,
		       param->max_key,0))
    {
      delete quick;
      quick=0;
    }
    else
    {
      quick->key_parts=(KEY_PART*)
        memdup_root(parent_alloc? parent_alloc : &quick->alloc,
                    (char*) param->key[idx],
                    sizeof(KEY_PART)*
                    param->table->key_info[param->real_keynr[idx]].key_parts);
    }
  }  
  DBUG_RETURN(quick);
}


/*
** Fix this to get all possible sub_ranges
*/
bool
get_quick_keys(PARAM *param,QUICK_RANGE_SELECT *quick,KEY_PART *key,
	       SEL_ARG *key_tree,char *min_key,uint min_key_flag,
	       char *max_key, uint max_key_flag)
{
  QUICK_RANGE *range;
  uint flag;

  if (key_tree->left != &null_element)
  {
    if (get_quick_keys(param,quick,key,key_tree->left,
		       min_key,min_key_flag, max_key, max_key_flag))
      return 1;
  }
  char *tmp_min_key=min_key,*tmp_max_key=max_key;
  key_tree->store(key[key_tree->part].part_length,
		  &tmp_min_key,min_key_flag,&tmp_max_key,max_key_flag);

  if (key_tree->next_key_part &&
      key_tree->next_key_part->part == key_tree->part+1 &&
      key_tree->next_key_part->type == SEL_ARG::KEY_RANGE)
  {						  // const key as prefix
    if (!((tmp_min_key - min_key) != (tmp_max_key - max_key) ||
	  memcmp(min_key,max_key, (uint) (tmp_max_key - max_key)) ||
	  key_tree->min_flag || key_tree->max_flag))
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
	key_tree->next_key_part->store_min_key(key, &tmp_min_key,
					       &tmp_min_flag);
      if (!tmp_max_flag)
	key_tree->next_key_part->store_max_key(key, &tmp_max_key,
					       &tmp_max_flag);
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
      if ((table_key->flags & (HA_NOSAME | HA_END_SPACE_KEY)) == HA_NOSAME &&
	  key->part == table_key->key_parts-1)
      {
	if (!(table_key->flags & HA_NULL_PART_KEY) ||
	    !null_part_in_key(key,
			      param->min_key,
			      (uint) (tmp_min_key - param->min_key)))
	  flag|= UNIQUE_RANGE;
	else
	  flag|= NULL_RANGE;
      }
    }
  }

  /* Get range for retrieving rows in QUICK_SELECT::get_next */
  if (!(range= new QUICK_RANGE(param->min_key,
			       (uint) (tmp_min_key - param->min_key),
			       param->max_key,
			       (uint) (tmp_max_key - param->max_key),
			       flag)))
    return 1;			// out of memory

  set_if_bigger(quick->max_used_key_length,range->min_length);
  set_if_bigger(quick->max_used_key_length,range->max_length);
  set_if_bigger(quick->used_key_parts, (uint) key_tree->part+1);
  if (insert_dynamic(&quick->ranges, (gptr)&range))
    return 1;


 end:
  if (key_tree->right != &null_element)
    return get_quick_keys(param,quick,key,key_tree->right,
			  min_key,min_key_flag,
			  max_key,max_key_flag);
  return 0;
}

/*
  Return 1 if there is only one range and this uses the whole primary key
*/

bool QUICK_RANGE_SELECT::unique_key_range()
{
  if (ranges.elements == 1)
  {
    QUICK_RANGE *tmp= *((QUICK_RANGE**)ranges.buffer);
    if ((tmp->flag & (EQ_RANGE | NULL_RANGE)) == EQ_RANGE)
    {
      KEY *key=head->key_info+index;
      return ((key->flags & (HA_NOSAME | HA_END_SPACE_KEY)) == HA_NOSAME &&
	      key->key_length == tmp->min_length);
    }
  }
  return 0;
}


/* Returns true if any part of the key is NULL */

static bool null_part_in_key(KEY_PART *key_part, const char *key, uint length)
{
  for (const char *end=key+length ; 
       key < end;
       key+= key_part++->part_length)
  {
    if (key_part->null_bit)
    {
      if (*key++)
	return 1;
    }
  }
  return 0;
}

bool QUICK_SELECT_I::check_if_keys_used(List<Item> *fields)
{
  return check_if_key_used(head, index, *fields); 
}

bool QUICK_INDEX_MERGE_SELECT::check_if_keys_used(List<Item> *fields)
{
  QUICK_RANGE_SELECT *quick;
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  while ((quick= it++))
  {
    if (check_if_key_used(head, quick->index, *fields))
      return 1;
  }
  return 0;
}

bool QUICK_ROR_INTERSECT_SELECT::check_if_keys_used(List<Item> *fields)
{
  QUICK_RANGE_SELECT *quick;
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  while ((quick= it++))
  {
    if (check_if_key_used(head, quick->index, *fields))
      return 1;
  }
  return 0;
}

bool QUICK_ROR_UNION_SELECT::check_if_keys_used(List<Item> *fields)
{
  QUICK_SELECT_I *quick;
  List_iterator_fast<QUICK_SELECT_I> it(quick_selects);
  while ((quick= it++))
  {
    if (quick->check_if_keys_used(fields))
      return 1;
  }
  return 0;
}

/****************************************************************************
** Create a QUICK RANGE based on a key
****************************************************************************/

QUICK_RANGE_SELECT *get_quick_select_for_ref(THD *thd, TABLE *table, 
                                             TABLE_REF *ref)
{
  table->file->index_end();			// Remove old cursor
  QUICK_RANGE_SELECT *quick=new QUICK_RANGE_SELECT(thd, table, ref->key, 1);  
  KEY *key_info = &table->key_info[ref->key];
  KEY_PART *key_part;
  QUICK_RANGE *range;
  uint part;

  if (!quick)
    return 0;			/* no ranges found */
  if (quick->init())
  {
    delete quick;
    return 0;
  }

  if (cp_buffer_from_ref(ref))
  {
    if (thd->is_fatal_error)
      goto err;					// out of memory
  }

  if (!(range= new QUICK_RANGE()))
    goto err;			// out of memory

  range->min_key=range->max_key=(char*) ref->key_buff;
  range->min_length=range->max_length=ref->key_length;
  range->flag= ((ref->key_length == key_info->key_length &&
		 (key_info->flags & (HA_NOSAME | HA_END_SPACE_KEY)) ==
		 HA_NOSAME) ? EQ_RANGE : 0);

  if (!(quick->key_parts=key_part=(KEY_PART *)
	alloc_root(&quick->alloc,sizeof(KEY_PART)*ref->key_parts)))
    goto err;

  for (part=0 ; part < ref->key_parts ;part++,key_part++)
  {
    key_part->part=part;
    key_part->field=        key_info->key_part[part].field;
    key_part->part_length=  key_info->key_part[part].length;
    if (key_part->field->type() == FIELD_TYPE_BLOB)
      key_part->part_length+=HA_KEY_BLOB_LENGTH;
    key_part->null_bit=     key_info->key_part[part].null_bit;
  }
  if (!insert_dynamic(&quick->ranges,(gptr)&range))
    return quick;

err:
  delete quick;
  return 0;
}


/*
  Fetch all row ids into unique.

  If table has a clustered primary key that covers all rows (true for bdb 
     and innodb currently) and one of the index_merge scans is a scan on PK,
  then 
    primary key scan rowids are not put into Unique and also 
    rows that will be retrieved by PK scan are not put into Unique
  
  RETURN
    0     OK
    other error
*/

int QUICK_INDEX_MERGE_SELECT::prepare_unique()
{
  int result;
  DBUG_ENTER("QUICK_INDEX_MERGE_SELECT::prepare_unique");
  
  /* We're going to just read rowids. */
  head->file->extra(HA_EXTRA_KEYREAD);

  /* 
    Make innodb retrieve all PK member fields, so 
     * ha_innobase::position (which uses them) call works.
     * We can filter out rows that will be retrieved by clustered PK.
    (This also creates a deficiency - it is possible that we will retrieve
     parts of key that are not used by current query at all.)
  */
  head->file->extra(HA_EXTRA_RETRIEVE_ALL_COLS);

  cur_quick_select->init();

  unique= new Unique(refpos_order_cmp, (void *)head->file,
                     head->file->ref_length,
                     thd->variables.sortbuff_size);
  if (!unique)
    DBUG_RETURN(1);
  do
  {
    while ((result= cur_quick_select->get_next()) == HA_ERR_END_OF_FILE)
    {
      cur_quick_select= cur_quick_it++;
      if (!cur_quick_select)
        break;

      if (cur_quick_select->init())
        DBUG_RETURN(1);

      /* QUICK_RANGE_SELECT::reset never fails */
      cur_quick_select->reset();
    }

    if (result)
    {      
      if (result != HA_ERR_END_OF_FILE)
        DBUG_RETURN(result);
      break;
    }
    
    if (thd->killed)
      DBUG_RETURN(1);
    
    /* skip row if it will be retrieved by clustered PK scan */
    if (pk_quick_select && pk_quick_select->row_in_ranges())
      continue;

    cur_quick_select->file->position(cur_quick_select->record);
    result= unique->unique_add((char*)cur_quick_select->file->ref);    
    if (result)
      DBUG_RETURN(1);

  }while(true);  

  /* ok, all row ids are in Unique */
  result= unique->get(head);
  doing_pk_scan= false;
  init_read_record(&read_record, thd, head, NULL, 1, 1);
  /* index_merge currently doesn't support "using index" at all */
  head->file->extra(HA_EXTRA_NO_KEYREAD);

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

  result= read_record.read_record(&read_record);

  if (result == -1)
  {
    result= HA_ERR_END_OF_FILE;
    end_read_record(&read_record);
    /* All rows from Unique have been retrieved, do a clustered PK scan */
    if(pk_quick_select)
    {
      doing_pk_scan= true;
      if ((result= pk_quick_select->init()))
        DBUG_RETURN(result);
      DBUG_RETURN(pk_quick_select->get_next());
    }
  }

  DBUG_RETURN(result);
}


/*
  NOTES
    Invariant on enter/exit: all intersected selects have retrieved index 
    records with rowid <= some_rowid_val and no intersected select has 
    retrieved any index records with rowid > some_rowid_val.
    We start fresh and loop until we have retrieved the same rowid in each of
    the key scans or we got an error.

    If a Clustered PK scan is present, it is used only to check if row 
    satisfies its conditions (and never used for row retrieval).
*/

int QUICK_ROR_INTERSECT_SELECT::get_next()
{
  List_iterator_fast<QUICK_RANGE_SELECT> quick_it(quick_selects);
  QUICK_RANGE_SELECT* quick;
  int error, cmp;
  uint last_rowid_count=0;
  DBUG_ENTER("QUICK_ROR_INTERSECT_SELECT::get_next");
  
  /* Get a rowid for first quick and save it as a 'candidate' */
  quick= quick_it++;
  if (cpk_quick)
  {
    do {
      error= quick->get_next();
    }while (!error && !cpk_quick->row_in_ranges());
  }
  else
    error= quick->get_next();
  
  if (error)
    DBUG_RETURN(error);

  quick->file->position(quick->record);
  memcpy(last_rowid, quick->file->ref, head->file->ref_length);
  last_rowid_count= 1;
 
  while (last_rowid_count < quick_selects.elements)
  {
    if (!(quick= quick_it++))
    {
      quick_it.rewind();
      quick= quick_it++;
    }
   
    do {
      if ((error= quick->get_next()))
        DBUG_RETURN(error);
      quick->file->position(quick->record);
      cmp= head->file->cmp_ref(quick->file->ref, last_rowid);      
    } while (cmp < 0);

    /* Ok, current select 'caught up' and returned ref >= cur_ref */
    if (cmp > 0)
    {
      /* Found a row with ref > cur_ref. Make it a new 'candidate' */
      if (cpk_quick)
      {
        while (!cpk_quick->row_in_ranges())
        {
          if ((error= quick->get_next()))
            DBUG_RETURN(error);
        }
      }
      memcpy(last_rowid, quick->file->ref, head->file->ref_length);
      last_rowid_count= 1;      
    }
    else
    {
      /* current 'candidate' row confirmed by this select */
      last_rowid_count++;
    }
  }

  /* We get here iff we got the same row ref in all scans. */
  if (need_to_fetch_row)
    error= head->file->rnd_pos(head->record[0], last_rowid);
  DBUG_RETURN(error);
}


/* 
  NOTES
    Enter/exit invariant: 
    For each quick select in the queue a {key,rowid} tuple has been 
    retrieved but the corresponding row hasn't been passed to output.
*/

int QUICK_ROR_UNION_SELECT::get_next()
{
  int error, dup_row;
  QUICK_SELECT_I *quick;
  byte *tmp;
  DBUG_ENTER("QUICK_ROR_UNION_SELECT::get_next");
  
  do
  {
    if (!queue.elements)
      DBUG_RETURN(HA_ERR_END_OF_FILE);
    /* Ok, we have a queue with > 1 scans */

    quick= (QUICK_SELECT_I*)queue_top(&queue);
    memcpy(cur_rowid, quick->last_rowid, rowid_length);

    /* put into queue rowid from the same stream as top element */
    if ((error= quick->get_next()))
    {
      if (error != HA_ERR_END_OF_FILE)
        DBUG_RETURN(error);
      queue_remove(&queue, 0);
    }
    else
    {
      quick->save_last_pos();
      queue_replaced(&queue);
    }
    
    if (!have_prev_rowid)
    {
      /* No rows have been returned yet */
      dup_row= false;
      have_prev_rowid= true;
    }
    else
      dup_row= !head->file->cmp_ref(cur_rowid, prev_rowid);
  }while (dup_row);
  
  tmp= cur_rowid;
  cur_rowid= prev_rowid;
  prev_rowid= tmp;

  error= head->file->rnd_pos(quick->record, prev_rowid);
  DBUG_RETURN(error);
}

	/* get next possible record using quick-struct */

int QUICK_RANGE_SELECT::get_next()
{
  DBUG_ENTER("QUICK_RANGE_SELECT::get_next");

  for (;;)
  {
    int result;
    if (range)
    {						// Already read through key
/*       result=((range->flag & EQ_RANGE) ?
	       file->index_next_same(record, (byte*) range->min_key,
				     range->min_length) :
	       file->index_next(record));
*/
       result=((range->flag & (EQ_RANGE | GEOM_FLAG) ) ?
	       file->index_next_same(record, (byte*) range->min_key,
				     range->min_length) :
	       file->index_next(record));

      if (!result)
      {
	if ((range->flag & GEOM_FLAG) || !cmp_next(*cur_range))
	  DBUG_RETURN(0);
      }
      else if (result != HA_ERR_END_OF_FILE)
	DBUG_RETURN(result);
    }
    
    if (!cur_range)
      range= *(cur_range= (QUICK_RANGE**)ranges.buffer);
    else 
      range=
        (cur_range == ((QUICK_RANGE**)ranges.buffer + ranges.elements - 1))?
         NULL: *(++cur_range);

    if (!range)
      DBUG_RETURN(HA_ERR_END_OF_FILE);		// All ranges used
    if (range->flag & GEOM_FLAG)
    {
      if ((result = file->index_read(record,
				     (byte*) (range->min_key),
				     range->min_length,
				     (ha_rkey_function)(range->flag ^
							GEOM_FLAG))))
      {
        if (result != HA_ERR_KEY_NOT_FOUND)
	  DBUG_RETURN(result);
        range=0;				// Not found, to next range
        continue;
      }
      DBUG_RETURN(0);
    }

    if (range->flag & NO_MIN_RANGE)		// Read first record
    {
      int local_error;
      if ((local_error=file->index_first(record)))
	DBUG_RETURN(local_error);		// Empty table
      if (cmp_next(range) == 0)
	DBUG_RETURN(0);
      range=0;			// No matching records; go to next range
      continue;
    }
    if ((result = file->index_read(record,
				   (byte*) (range->min_key +
					    test(range->flag & GEOM_FLAG)),
				   range->min_length,
				   (range->flag & NEAR_MIN) ?
				   HA_READ_AFTER_KEY:
				   (range->flag & EQ_RANGE) ?
				   HA_READ_KEY_EXACT :
				   HA_READ_KEY_OR_NEXT)))

    {
      if (result != HA_ERR_KEY_NOT_FOUND)
	DBUG_RETURN(result);
      range=0;					// Not found, to next range
      continue;
    }
    if (cmp_next(range) == 0)
    {
      if (range->flag == (UNIQUE_RANGE | EQ_RANGE))
	range=0;				// Stop searching
      DBUG_RETURN(0);				// Found key is in range
    }
    range=0;					// To next range
  }
}


/*
  Compare if found key is over max-value
  Returns 0 if key <= range->max_key
*/

int QUICK_RANGE_SELECT::cmp_next(QUICK_RANGE *range_arg)
{
  if (range_arg->flag & NO_MAX_RANGE)
    return 0;					/* key can't be to large */

  KEY_PART *key_part=key_parts;
  for (char *key=range_arg->max_key, *end=key+range_arg->max_length;
       key < end;
       key+= key_part++->part_length)
  {
    int cmp;
    if (key_part->null_bit)
    {
      if (*key++)
      {
	if (!key_part->field->is_null())
	  return 1;
	continue;
      }
      else if (key_part->field->is_null())
	return 0;
    }
    if ((cmp=key_part->field->key_cmp((byte*) key, key_part->part_length)) < 0)
      return 0;
    if (cmp > 0)
      return 1;
  }
  return (range_arg->flag & NEAR_MAX) ? 1 : 0;		// Exact match
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
    true  if current row will be retrieved by this quick select
    false if not
*/

bool QUICK_RANGE_SELECT::row_in_ranges()
{
  QUICK_RANGE *range;
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
  range= *(QUICK_RANGE**)dynamic_array_ptr(&ranges, mid);
  return (!cmp_next(range) && !cmp_prev(range));
}

/*
  This is a hack: we inherit from QUICK_SELECT so that we can use the
  get_next() interface, but we have to hold a pointer to the original
  QUICK_SELECT because its data are used all over the place.  What
  should be done is to factor out the data that is needed into a base
  class (QUICK_SELECT), and then have two subclasses (_ASC and _DESC)
  which handle the ranges and implement the get_next() function.  But
  for now, this seems to work right at least.
 */

QUICK_SELECT_DESC::QUICK_SELECT_DESC(QUICK_RANGE_SELECT *q, 
                                     uint used_key_parts)
 : QUICK_RANGE_SELECT(*q), rev_it(rev_ranges)
{
  bool not_read_after_key = file->table_flags() & HA_NOT_READ_AFTER_KEY;
  QUICK_RANGE *r;
  
  QUICK_RANGE **pr= (QUICK_RANGE**)ranges.buffer;
  QUICK_RANGE **last_range= pr + ranges.elements;
  for (; pr!=last_range; ++pr)
  {
    r= *pr;
    rev_ranges.push_front(r);
    if (not_read_after_key && range_reads_after_key(r))
    {
      error = HA_ERR_UNSUPPORTED;
      dont_free=1;				// Don't free memory from 'q'
      return;
    }
  }
  /* Remove EQ_RANGE flag for keys that are not using the full key */
  for (r = rev_it++; r; r = rev_it++)
  {
    if ((r->flag & EQ_RANGE) &&
	head->key_info[index].key_length != r->max_length)
      r->flag&= ~EQ_RANGE;
  }
  rev_it.rewind();
  q->dont_free=1;				// Don't free shared mem
  delete q;
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
    if (range)
    {						// Already read through key
      result = ((range->flag & EQ_RANGE)
		? file->index_next_same(record, (byte*) range->min_key,
					range->min_length) :
		file->index_prev(record));
      if (!result)
      {
	if (cmp_prev(*rev_it.ref()) == 0)
	  DBUG_RETURN(0);
      }
      else if (result != HA_ERR_END_OF_FILE)
	DBUG_RETURN(result);
    }

    if (!(range=rev_it++))
      DBUG_RETURN(HA_ERR_END_OF_FILE);		// All ranges used

    if (range->flag & NO_MAX_RANGE)		// Read last record
    {
      int local_error;
      if ((local_error=file->index_last(record)))
	DBUG_RETURN(local_error);		// Empty table
      if (cmp_prev(range) == 0)
	DBUG_RETURN(0);
      range=0;			// No matching records; go to next range
      continue;
    }

    if (range->flag & EQ_RANGE)
    {
      result = file->index_read(record, (byte*) range->max_key,
				range->max_length, HA_READ_KEY_EXACT);
    }
    else
    {
      DBUG_ASSERT(range->flag & NEAR_MAX || range_reads_after_key(range));
#ifndef NOT_IMPLEMENTED_YET
      result=file->index_read(record, (byte*) range->max_key,
			      range->max_length,
			      ((range->flag & NEAR_MAX) ?
			       HA_READ_BEFORE_KEY : HA_READ_PREFIX_LAST_OR_PREV));
#else
      /* Heikki changed Sept 11, 2002: since InnoDB does not store the cursor
	 position if READ_KEY_EXACT is used to a primary key with all
	 key columns specified, we must use below HA_READ_KEY_OR_NEXT,
	 so that InnoDB stores the cursor position and is able to move
	 the cursor one step backward after the search. */

      /* Note: even if max_key is only a prefix, HA_READ_AFTER_KEY will
       * do the right thing - go past all keys which match the prefix */

      result=file->index_read(record, (byte*) range->max_key,
			      range->max_length,
			      ((range->flag & NEAR_MAX) ?
			       HA_READ_KEY_OR_NEXT : HA_READ_AFTER_KEY));
      result = file->index_prev(record);
#endif
    }
    if (result)
    {
      if (result != HA_ERR_KEY_NOT_FOUND)
	DBUG_RETURN(result);
      range=0;					// Not found, to next range
      continue;
    }
    if (cmp_prev(range) == 0)
    {
      if (range->flag == (UNIQUE_RANGE | EQ_RANGE))
	range = 0;				// Stop searching
      DBUG_RETURN(0);				// Found key is in range
    }
    range = 0;					// To next range
  }
}


/*
  Returns 0 if found key is inside range (found key >= range->min_key).
*/

int QUICK_RANGE_SELECT::cmp_prev(QUICK_RANGE *range_arg)
{
  if (range_arg->flag & NO_MIN_RANGE)
    return 0;					/* key can't be to small */

  KEY_PART *key_part = key_parts;
  for (char *key = range_arg->min_key, *end = key + range_arg->min_length;
       key < end;
       key += key_part++->part_length)
  {
    int cmp;
    if (key_part->null_bit)
    {
      // this key part allows null values; NULL is lower than everything else
      if (*key++)
      {
	// the range is expecting a null value
	if (!key_part->field->is_null())
	  return 0;	// not null -- still inside the range
	continue;	// null -- exact match, go to next key part
      }
      else if (key_part->field->is_null())
	return 1;	// null -- outside the range
    }
    if ((cmp = key_part->field->key_cmp((byte*) key,
					key_part->part_length)) > 0)
      return 0;
    if (cmp < 0)
      return 1;
  }
  return (range_arg->flag & NEAR_MIN) ? 1 : 0;		// Exact match
}


/*
 * True if this range will require using HA_READ_AFTER_KEY
   See comment in get_next() about this
 */

bool QUICK_SELECT_DESC::range_reads_after_key(QUICK_RANGE *range_arg)
{
  return ((range_arg->flag & (NO_MAX_RANGE | NEAR_MAX)) ||
	  !(range_arg->flag & EQ_RANGE) ||
	  head->key_info[index].key_length != range_arg->max_length) ? 1 : 0;
}


/* True if we are reading over a key that may have a NULL value */

#ifdef NOT_USED
bool QUICK_SELECT_DESC::test_if_null_range(QUICK_RANGE *range_arg,
					   uint used_key_parts)
{
  uint offset,end;
  KEY_PART *key_part = key_parts,
           *key_part_end= key_part+used_key_parts;

  for (offset= 0,  end = min(range_arg->min_length, range_arg->max_length) ;
       offset < end && key_part != key_part_end ;
       offset += key_part++->part_length)
  {
    uint null_length=test(key_part->null_bit);
    if (!memcmp((char*) range_arg->min_key+offset,
		(char*) range_arg->max_key+offset,
		key_part->part_length + null_length))
    {
      offset+=null_length;
      continue;
    }
    if (null_length && range_arg->min_key[offset])
      return 1;				// min_key is null and max_key isn't
    // Range doesn't cover NULL. This is ok if there is no more null parts
    break;
  }
  /*
    If the next min_range is > NULL, then we can use this, even if
    it's a NULL key
    Example:  SELECT * FROM t1 WHERE a = 2 AND b >0 ORDER BY a DESC,b DESC;

  */
  if (key_part != key_part_end && key_part->null_bit)
  {
    if (offset >= range_arg->min_length || range_arg->min_key[offset])
      return 1;					// Could be null
    key_part++;
  }
  /*
    If any of the key parts used in the ORDER BY could be NULL, we can't
    use the key to sort the data.
  */
  for (; key_part != key_part_end ; key_part++)
    if (key_part->null_bit)
      return 1;					// Covers null part
  return 0;
}
#endif


void QUICK_RANGE_SELECT::fill_keys_and_lengths(String *key_names, 
                                               String *used_lengths)
{
  char buf[64];
  uint length;
  KEY *key_info= head->key_info + index;
  key_names->append(key_info->name);
  length= longlong2str(max_used_key_length, buf, 10) - buf;
  used_lengths->append(buf, length);
}

void QUICK_INDEX_MERGE_SELECT::fill_keys_and_lengths(String *key_names,
                                                     String *used_lengths)
{
  char buf[64];
  uint length;
  QUICK_RANGE_SELECT *quick;
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  while ((quick= it++))
  {
    KEY *key_info= head->key_info + quick->index;
    if (key_names->length())
      key_names->append(',');
    key_names->append(key_info->name);

    if (used_lengths->length())
      used_lengths->append(',');
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

void QUICK_ROR_INTERSECT_SELECT::fill_keys_and_lengths(String *key_names,
                                                       String *used_lengths)
{
  char buf[64];
  uint length;
  bool first= true; 
  QUICK_RANGE_SELECT *quick;
  List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
  while ((quick= it++))
  {
    KEY *key_info= head->key_info + quick->index;
    if (!first)
      key_names->append(',');
    key_names->append(key_info->name);

    if (first)
      first= false;
    else
      used_lengths->append(',');
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

void QUICK_ROR_UNION_SELECT::fill_keys_and_lengths(String *key_names,
                                                   String *used_lengths)
{
  bool first= true; 
  QUICK_SELECT_I *quick;
  List_iterator_fast<QUICK_SELECT_I> it(quick_selects);
  while ((quick= it++))
  {
    if (first)
      first= false;
    else
    {  
      used_lengths->append(',');
      key_names->append(',');
    }
    quick->fill_keys_and_lengths(key_names, used_lengths);
  }
}

#ifndef DBUG_OFF

static void print_sel_tree(PARAM *param, SEL_TREE *tree, key_map *tree_map, 
                           const char *msg)
{
  SEL_ARG **key,**end;
  int idx;
  char buff[1024];
  DBUG_ENTER("print_sel_tree");
  if (! _db_on_)
    DBUG_VOID_RETURN;

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
    tmp.append("(empty)");

  DBUG_PRINT("info", ("SEL_TREE %p (%s) scans:%s", tree, msg, tmp.ptr()));

  DBUG_VOID_RETURN;
}

static void print_ror_scans_arr(TABLE *table, const char *msg,
                                struct st_ror_scan_info **start, 
                                struct st_ror_scan_info **end)
{
  DBUG_ENTER("print_ror_scans");
  if (! _db_on_)
    DBUG_VOID_RETURN;

  char buff[1024];
  String tmp(buff,sizeof(buff),&my_charset_bin);
  tmp.length(0);
  for(;start != end; start++)
  {
    if (tmp.length())
      tmp.append(',');
    tmp.append(table->key_info[(*start)->keynr].name);
  }
  if (!tmp.length())
    tmp.append("(empty)");
  DBUG_PRINT("info", ("ROR key scans (%s): %s", msg, tmp.ptr()));
  DBUG_VOID_RETURN;
}

/*****************************************************************************
** Print a quick range for debugging
** TODO:
** This should be changed to use a String to store each row instead
** of locking the DEBUG stream !
*****************************************************************************/

static void
print_key(KEY_PART *key_part,const char *key,uint used_length)
{
  char buff[1024];
  String tmp(buff,sizeof(buff),&my_charset_bin);

  for (uint length=0;
       length < used_length ;
       length+=key_part->part_length, key+=key_part->part_length, key_part++)
  {
    Field *field=key_part->field;
    if (length != 0)
      fputc('/',DBUG_FILE);
    if (field->real_maybe_null())
    {
      length++;				// null byte is not in part_length
      if (*key++)
      {
	fwrite("NULL",sizeof(char),4,DBUG_FILE);
	continue;
      }
    }
    field->set_key_image((char*) key,key_part->part_length -
			 ((field->type() == FIELD_TYPE_BLOB) ?
			  HA_KEY_BLOB_LENGTH : 0),
			 field->charset());
    field->val_str(&tmp,&tmp);
    fwrite(tmp.ptr(),sizeof(char),tmp.length(),DBUG_FILE);
  }
}

static void print_quick(QUICK_SELECT_I *quick, const key_map *needed_reg)
{
  char buf[MAX_KEY/8+1];
  DBUG_ENTER("print_param");
  if (! _db_on_ || !quick)
    DBUG_VOID_RETURN;
  DBUG_LOCK_FILE;
 
  quick->dbug_dump(0, true);
  fprintf(DBUG_FILE,"other_keys: 0x%s:\n", needed_reg->print(buf));

  DBUG_UNLOCK_FILE;
  DBUG_VOID_RETURN;
}

static void print_rowid(byte* val, int len)
{
  byte *pb;
  DBUG_LOCK_FILE;
  fputc('\"', DBUG_FILE);
  for (pb= val; pb!= val + len; ++pb)
    fprintf(DBUG_FILE, "%c", *pb);
  fprintf(DBUG_FILE, "\", hex: ");

  for (pb= val; pb!= val + len; ++pb)
    fprintf(DBUG_FILE, "%x ", *pb);
  fputc('\n', DBUG_FILE);
  DBUG_UNLOCK_FILE;
}

void QUICK_RANGE_SELECT::dbug_dump(int indent, bool verbose)
{
  fprintf(DBUG_FILE, "%*squick range select, key %s, length: %d\n",
	  indent, "", head->key_info[index].name, max_used_key_length);
  
  if (verbose)
  {
    QUICK_RANGE *range;
    QUICK_RANGE **pr= (QUICK_RANGE**)ranges.buffer;
    QUICK_RANGE **last_range= pr + ranges.elements;    
    for (; pr!=last_range; ++pr)
    {
      fprintf(DBUG_FILE, "%*s", indent + 2, "");
      range= *pr;
      if (!(range->flag & NO_MIN_RANGE))
      {
        print_key(key_parts,range->min_key,range->min_length);
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
        print_key(key_parts,range->max_key,range->max_length);
      }
      fputs("\n",DBUG_FILE);
    }
  }
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

#endif

/*****************************************************************************
** Instansiate templates
*****************************************************************************/

#ifdef __GNUC__
template class List<QUICK_RANGE>;
template class List_iterator<QUICK_RANGE>;
#endif
