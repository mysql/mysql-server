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
	bzero(*min_key+1,length-1);
      }
      else
	memcpy(*min_key,min_value,length);
      (*min_key)+= length;
    }
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
    }
  }

  void store_min_key(KEY_PART *key,char **range_key, uint *range_key_flag)
  {
    SEL_ARG *key_tree= first();
    key_tree->store(key[key_tree->part].store_length,
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
    key_tree->store(key[key_tree->part].store_length,
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


class SEL_TREE :public Sql_alloc
{
public:
  enum Type { IMPOSSIBLE, ALWAYS, MAYBE, KEY, KEY_SMALLER } type;
  SEL_TREE(enum Type type_arg) :type(type_arg) {}
  SEL_TREE() :type(KEY) { bzero((char*) keys,sizeof(keys));}
  SEL_ARG *keys[MAX_KEY];
};


typedef struct st_qsel_param {
  THD	*thd;
  TABLE *table;
  KEY_PART *key_parts,*key_parts_end,*key[MAX_KEY];
  MEM_ROOT *mem_root;
  table_map prev_tables,read_tables,current_table;
  uint baseflag, keys, max_key_part, range_count;
  uint real_keynr[MAX_KEY];
  char min_key[MAX_KEY_LENGTH+MAX_FIELD_WIDTH],
    max_key[MAX_KEY_LENGTH+MAX_FIELD_WIDTH];
  bool quick;				// Don't calulate possible keys
  COND *cond;
} PARAM;

static SEL_TREE * get_mm_parts(PARAM *param,COND *cond_func,Field *field,
			       Item_func::Functype type,Item *value,
			       Item_result cmp_type);
static SEL_ARG *get_mm_leaf(PARAM *param,COND *cond_func,Field *field,
			    KEY_PART *key_part,
			    Item_func::Functype type,Item *value);
static SEL_TREE *get_mm_tree(PARAM *param,COND *cond);
static ha_rows check_quick_select(PARAM *param,uint index,SEL_ARG *key_tree);
static ha_rows check_quick_keys(PARAM *param,uint index,SEL_ARG *key_tree,
				char *min_key,uint min_key_flag,
				char *max_key, uint max_key_flag);

static QUICK_SELECT *get_quick_select(PARAM *param,uint index,
				      SEL_ARG *key_tree);
#ifndef DBUG_OFF
static void print_quick(QUICK_SELECT *quick,const key_map* needed_reg);
#endif
static SEL_TREE *tree_and(PARAM *param,SEL_TREE *tree1,SEL_TREE *tree2);
static SEL_TREE *tree_or(PARAM *param,SEL_TREE *tree1,SEL_TREE *tree2);
static SEL_ARG *sel_add(SEL_ARG *key1,SEL_ARG *key2);
static SEL_ARG *key_or(SEL_ARG *key1,SEL_ARG *key2);
static SEL_ARG *key_and(SEL_ARG *key1,SEL_ARG *key2,uint clone_flag);
static bool get_range(SEL_ARG **e1,SEL_ARG **e2,SEL_ARG *root1);
static bool get_quick_keys(PARAM *param,QUICK_SELECT *quick,KEY_PART *key,
			   SEL_ARG *key_tree,char *min_key,uint min_key_flag,
			   char *max_key,uint max_key_flag);
static bool eq_tree(SEL_ARG* a,SEL_ARG *b);

static SEL_ARG null_element(SEL_ARG::IMPOSSIBLE);
static bool null_part_in_key(KEY_PART *key_part, const char *key, uint length);

/***************************************************************************
** Basic functions for SQL_SELECT and QUICK_SELECT
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

QUICK_SELECT::QUICK_SELECT(THD *thd, TABLE *table, uint key_nr, bool no_alloc)
  :dont_free(0),sorted(0),error(0),index(key_nr),max_used_key_length(0),
   used_key_parts(0), head(table), it(ranges),range(0)
{
  if (!no_alloc)
  {
    // Allocates everything through the internal memroot
    init_sql_alloc(&alloc, thd->variables.range_alloc_block_size, 0);
    my_pthread_setspecific_ptr(THR_MALLOC,&alloc);
  }
  else
    bzero((char*) &alloc,sizeof(alloc));
  file=head->file;
  record=head->record[0];
  init();
}

QUICK_SELECT::~QUICK_SELECT()
{
  if (!dont_free)
  {
    if (file->inited)
      file->ha_index_end();
    free_root(&alloc,MYF(0));
  }
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
  tmp->color= color;
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
  if (keys_to_use.is_clear_all())
    DBUG_RETURN(0);
  records=head->file->records;
  if (!records)
    records++;					/* purecov: inspected */
  scan_time=(double) records / TIME_FOR_COMPARE+1;
  read_time=(double) head->file->scan_time()+ scan_time + 1.1;
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
    KEY *key_info;
    PARAM param;

    /* set up parameter that is passed to all functions */
    param.thd= thd;
    param.baseflag=head->file->table_flags();
    param.prev_tables=prev_tables | const_tables;
    param.read_tables=read_tables;
    param.current_table= head->map;
    param.table=head;
    param.keys=0;
    param.mem_root= &alloc;
    thd->no_errors=1;				// Don't warn about NULL
    init_sql_alloc(&alloc, thd->variables.range_alloc_block_size, 0);
    if (!(param.key_parts = (KEY_PART*) alloc_root(&alloc,
						   sizeof(KEY_PART)*
						   head->key_parts)))
    {
      thd->no_errors=0;
      free_root(&alloc,MYF(0));			// Return memory & allocator
      DBUG_RETURN(0);				// Can't use range
    }
    key_parts= param.key_parts;
    old_root=my_pthread_getspecific_ptr(MEM_ROOT*,THR_MALLOC);
    my_pthread_setspecific_ptr(THR_MALLOC,&alloc);

    key_info= head->key_info;
    for (idx=0 ; idx < head->keys ; idx++, key_info++)
    {
      KEY_PART_INFO *key_part_info;
      if (!keys_to_use.is_set(idx))
	continue;
      if (key_info->flags & HA_FULLTEXT)
	continue;    // ToDo: ft-keys in non-ft ranges, if possible   SerG

      param.key[param.keys]=key_parts;
      key_part_info= key_info->key_part;
      for (uint part=0 ; part < key_info->key_parts ;
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
      }
      param.real_keynr[param.keys++]=idx;
    }
    param.key_parts_end=key_parts;

    if ((tree=get_mm_tree(&param,cond)))
    {
      if (tree->type == SEL_TREE::IMPOSSIBLE)
      {
	records=0L;				// Return -1 from this function
	read_time= (double) HA_POS_ERROR;
      }
      else if (tree->type == SEL_TREE::KEY ||
	       tree->type == SEL_TREE::KEY_SMALLER)
      {
	SEL_ARG **key,**end,**best_key=0;


	for (idx=0,key=tree->keys, end=key+param.keys ;
	     key != end ;
	     key++,idx++)
	{
	  ha_rows found_records;
	  double found_read_time;
	  if (*key)
	  {
	    uint keynr= param.real_keynr[idx];
	    if ((*key)->type == SEL_ARG::MAYBE_KEY ||
		(*key)->maybe_flag)
	        needed_reg.set_bit(keynr);

	    found_records=check_quick_select(&param, idx, *key);
	    if (found_records != HA_POS_ERROR && found_records > 2 &&
		head->used_keys.is_set(keynr) &&
		(head->file->index_flags(keynr, param.max_key_part, 1) &
                 HA_KEYREAD_ONLY))
	    {
	      /*
		We can resolve this by only reading through this key.
		Assume that we will read trough the whole key range
		and that all key blocks are half full (normally things are
		much better).
	      */
	      uint keys_per_block= (head->file->block_size/2/
				    (head->key_info[keynr].key_length+
				     head->file->ref_length) + 1);
	      found_read_time=((double) (found_records+keys_per_block-1)/
			       (double) keys_per_block);
	    }
	    else
	      found_read_time= (head->file->read_time(keynr,
						      param.range_count,
						      found_records)+
				(double) found_records / TIME_FOR_COMPARE);
            DBUG_PRINT("info",("read_time: %g  found_read_time: %g",
                               read_time, found_read_time));
	    if (read_time > found_read_time && found_records != HA_POS_ERROR)
	    {
	      read_time=found_read_time;
	      records=found_records;
	      best_key=key;
	    }
	  }
	}
	if (best_key && records)
	{
	  if ((quick=get_quick_select(&param,(uint) (best_key-tree->keys),
				      *best_key)))
	  {
	    quick->records=records;
	    quick->read_time=read_time;
	  }
	}
      }
    }
    free_root(&alloc,MYF(0));			// Return memory & allocator
    my_pthread_setspecific_ptr(THR_MALLOC,old_root);
    thd->no_errors=0;
  }
  DBUG_EXECUTE("info",print_quick(quick,&needed_reg););
  /*
    Assume that if the user is using 'limit' we will only need to scan
    limit rows if we are using a key
  */
  DBUG_RETURN(records ? test(quick) : -1);
}

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

  param->cond= cond;

  if (cond_func->functype() == Item_func::BETWEEN)
  {
    if (cond_func->arguments()[0]->type() == Item::FIELD_ITEM)
    {
      Field *field=((Item_field*) (cond_func->arguments()[0]))->field;
      Item_result cmp_type=field->cmp_type();
      DBUG_RETURN(tree_and(param,
			   get_mm_parts(param, cond_func, field,
					Item_func::GE_FUNC,
					cond_func->arguments()[1], cmp_type),
			   get_mm_parts(param, cond_func, field,
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
      tree= get_mm_parts(param,cond_func,field,Item_func::EQ_FUNC,
			 func->arguments()[1],cmp_type);
      if (!tree)
	DBUG_RETURN(tree);			// Not key field
      for (uint i=2 ; i < func->argument_count(); i++)
      {
	SEL_TREE *new_tree=get_mm_parts(param,cond_func,field,
					Item_func::EQ_FUNC,
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
    tree= get_mm_parts(param, cond_func,
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
    DBUG_RETURN(get_mm_parts(param, cond_func,
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
get_mm_parts(PARAM *param, COND *cond_func, Field *field,
	     Item_func::Functype type, 
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
      tree->keys[key_part->key]=sel_add(tree->keys[key_part->key],sel_arg);
    }
  }

  if (ne_func)
  {
    SEL_TREE *tree2= get_mm_parts(param, cond_func,
				  field, Item_func::GT_FUNC,
                                  value, cmp_type);
    if (tree2)
      tree= tree_or(param,tree,tree2);
  }
  DBUG_RETURN(tree);
}


static SEL_ARG *
get_mm_leaf(PARAM *param, COND *conf_func, Field *field, KEY_PART *key_part,
	    Item_func::Functype type,Item *value)
{
  uint maybe_null=(uint) field->real_maybe_null(), copies;
  uint field_length=field->pack_length()+maybe_null;
  SEL_ARG *tree;
  char *str, *str2;
  DBUG_ENTER("get_mm_leaf");

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

  /*
    We can't use an index when comparing strings of 
    different collations 
  */
  if (field->result_type() == STRING_RESULT &&
      value->result_type() == STRING_RESULT &&
      key_part->image_type == Field::itRAW &&
      ((Field_str*)field)->charset() != conf_func->compare_collation())
    DBUG_RETURN(0);

  if (type == Item_func::LIKE_FUNC)
  {
    bool like_error;
    char buff1[MAX_FIELD_WIDTH],*min_str,*max_str;
    String tmp(buff1,sizeof(buff1),value->collation.collation),*res;
    uint length,offset,min_length,max_length;

    if (!field->optimize_range(param->real_keynr[key_part->key],
                               key_part->part))
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
    if (!(min_str= (char*) alloc_root(param->mem_root, length*2)))
      DBUG_RETURN(0);
    max_str=min_str+length;
    if (maybe_null)
      max_str[0]= min_str[0]=0;

    like_error= my_like_range(field->charset(),
			      res->ptr(), res->length(),
			      ((Item_func_like*)(param->cond))->escape,
			      wild_one, wild_many,
			      field_length-maybe_null,
			      min_str+offset, max_str+offset,
			      &min_length, &max_length);
    if (like_error)				// Can't optimize with LIKE
      DBUG_RETURN(0);

    if (offset != maybe_null)			// Blob
    {
      int2store(min_str+maybe_null,min_length);
      int2store(max_str+maybe_null,max_length);
    }
    DBUG_RETURN(new SEL_ARG(field,min_str,max_str));
  }

  if (!field->optimize_range(param->real_keynr[key_part->key],
                             key_part->part) &&
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
 
  if (value->save_in_field(field, 1) < 0)
  {
    /* This happens when we try to insert a NULL field in a not null column */
    DBUG_RETURN(&null_element);			// cmp with NULL is never true
  }
  /* Get local copy of key */
  copies= 1;
  if (field->key_type() == HA_KEYTYPE_VARTEXT)
    copies= 2;
  str= str2= (char*) alloc_root(param->mem_root,
				(key_part->store_length)*copies+1);
  if (!str)
    DBUG_RETURN(0);
  if (maybe_null)
    *str= (char) field->is_real_null();		// Set to 1 if null
  field->get_key_image(str+maybe_null, key_part->length,
		       field->charset(), key_part->image_type);
  if (copies == 2)
  {
    /*
      The key is stored as 2 byte length + key
      key doesn't match end space. In other words, a key 'X ' should match
      all rows between 'X' and 'X           ...'
    */
    uint length= uint2korr(str+maybe_null);
    str2= str+ key_part->store_length;
    /* remove end space */
    while (length > 0 && str[length+HA_KEY_BLOB_LENGTH+maybe_null-1] == ' ')
      length--;
    int2store(str+maybe_null, length);
    /* Create key that is space filled */
    memcpy(str2, str, length + HA_KEY_BLOB_LENGTH + maybe_null);
    my_fill_8bit(field->charset(),
		 str2+ length+ HA_KEY_BLOB_LENGTH +maybe_null,
		 key_part->length-length, ' ');
    int2store(str2+maybe_null, key_part->length);
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
      if (*key1 && (*key1)->type == SEL_ARG::IMPOSSIBLE)
      {
	tree1->type= SEL_TREE::IMPOSSIBLE;
#ifdef EXTRA_DEBUG
        (*key1)->test_use_count(*key1);
#endif
	break;
      }
    }
  }
  DBUG_RETURN(tree1);
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

  /* Join the trees key per key */
  SEL_ARG **key1,**key2,**end;
  SEL_TREE *result=0;
  for (key1= tree1->keys,key2= tree2->keys,end=key1+param->keys ;
       key1 != end ; key1++,key2++)
  {
    *key1=key_or(*key1,*key2);
    if (*key1)
    {
      result=tree1;				// Added to tree1
#ifdef EXTRA_DEBUG
      (*key1)->test_use_count(*key1);
#endif
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
      swap_variables(SEL_ARG *, key1, key2);
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
    swap_variables(SEL_ARG *, key1, key2);
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

  if ((key1->min_flag | key2->min_flag) & GEOM_FLAG)
  {
    key1->free_tree();
    key2->free_tree();
    return 0;					// Can't optimize this
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

  if (key1->part != key2->part || 
      (key1->min_flag | key2->min_flag) & GEOM_FLAG)
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
      swap_variables(SEL_ARG *,key1,key2);
    }
    if (key1->use_count > 0 || !(key1=key1->clone_tree()))
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
	    SEL_ARG *cpy= new SEL_ARG(*key2);	// Must make copy
	    if (!cpy)
	      return 0;				// OOM
	    key1=key1->insert(cpy);
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
	/* Increment key count as it may be used for next loop */
	key.increment_use_count(1);
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
  uint e_count=0;
  if (this == root && use_count != 1)
  {
    sql_print_information("Use_count: Wrong count %lu for root",use_count);
    return;
  }
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
	sql_print_information("Use_count: Wrong count for key at %lx, %lu should be %lu",
			pos,pos->next_key_part->use_count,count);
	return;
      }
      pos->next_key_part->test_use_count(root);
    }
  }
  if (e_count != elements)
    sql_print_warning("Wrong use count: %u (should be %u) for tree at %lx",
		    e_count, elements, (gptr) this);
}

#endif



/*****************************************************************************
** Check how many records we will find by using the found tree
*****************************************************************************/

static ha_rows
check_quick_select(PARAM *param,uint idx,SEL_ARG *tree)
{
  ha_rows records;
  DBUG_ENTER("check_quick_select");

  if (!tree)
    DBUG_RETURN(HA_POS_ERROR);			// Can't use it
  param->max_key_part=0;
  param->range_count=0;
  if (tree->type == SEL_ARG::IMPOSSIBLE)
    DBUG_RETURN(0L);				// Impossible select. return
  if (tree->type != SEL_ARG::KEY_RANGE || tree->part != 0)
    DBUG_RETURN(HA_POS_ERROR);				// Don't use tree
  records=check_quick_keys(param,idx,tree,param->min_key,0,param->max_key,0);
  if (records != HA_POS_ERROR)
  {
    uint key=param->real_keynr[idx];
    param->table->quick_keys.set_bit(key);
    param->table->quick_rows[key]=records;
    param->table->quick_key_parts[key]=param->max_key_part+1;
  }
  DBUG_PRINT("exit", ("Records: %lu", (ulong) records));
  DBUG_RETURN(records);
}


static ha_rows
check_quick_keys(PARAM *param,uint idx,SEL_ARG *key_tree,
		 char *min_key,uint min_key_flag, char *max_key,
		 uint max_key_flag)
{
  ha_rows records=0,tmp;

  param->max_key_part=max(param->max_key_part,key_tree->part);
  if (key_tree->left != &null_element)
  {
    records=check_quick_keys(param,idx,key_tree->left,min_key,min_key_flag,
			     max_key,max_key_flag);
    if (records == HA_POS_ERROR)			// Impossible
      return records;
  }

  uint tmp_min_flag,tmp_max_flag,keynr;
  char *tmp_min_key=min_key,*tmp_max_key=max_key;

  key_tree->store(param->key[idx][key_tree->part].store_length,
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
    if (tmp_min_flag & GEOM_FLAG)
    {
      key_range min_range;
      min_range.key=    (byte*) param->min_key;
      min_range.length= min_key_length;
      /* In this case tmp_min_flag contains the handler-read-function */
      min_range.flag=   (ha_rkey_function) (tmp_min_flag ^ GEOM_FLAG);

      tmp= param->table->file->records_in_range(keynr, &min_range,
                                                (key_range*) 0);
    }
    else
    {
      key_range min_range, max_range;

      min_range.key=    (byte*) param->min_key;
      min_range.length= min_key_length;
      min_range.flag=   (tmp_min_flag & NEAR_MIN ? HA_READ_AFTER_KEY :
                         HA_READ_KEY_EXACT);
      max_range.key=    (byte*) param->max_key;
      max_range.length= max_key_length;
      max_range.flag=   (tmp_max_flag & NEAR_MAX ?
                         HA_READ_BEFORE_KEY : HA_READ_AFTER_KEY);
      tmp=param->table->file->records_in_range(keynr,
                                               (min_key_length ? &min_range :
                                                (key_range*) 0),
                                               (max_key_length ? &max_range :
                                                (key_range*) 0));
    }
  }
 end:
  if (tmp == HA_POS_ERROR)			// Impossible range
    return tmp;
  records+=tmp;
  if (key_tree->right != &null_element)
  {
    tmp=check_quick_keys(param,idx,key_tree->right,min_key,min_key_flag,
			 max_key,max_key_flag);
    if (tmp == HA_POS_ERROR)
      return tmp;
    records+=tmp;
  }
  return records;
}


/****************************************************************************
** change a tree to a structure to be used by quick_select
** This uses it's own malloc tree
****************************************************************************/

static QUICK_SELECT *
get_quick_select(PARAM *param,uint idx,SEL_ARG *key_tree)
{
  QUICK_SELECT *quick;
  DBUG_ENTER("get_quick_select");

  if (param->table->key_info[param->real_keynr[idx]].flags & HA_SPATIAL)
    quick=new QUICK_SELECT_GEOM(param->thd, param->table, param->real_keynr[idx],
				0);
  else
    quick=new QUICK_SELECT(param->thd, param->table, param->real_keynr[idx]);

  if (quick)
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
	memdup_root(&quick->alloc,(char*) param->key[idx],
		   sizeof(KEY_PART)*
		   param->table->key_info[param->real_keynr[idx]].key_parts);
    }
  }
  DBUG_RETURN(quick);
}


/*
** Fix this to get all possible sub_ranges
*/

static bool
get_quick_keys(PARAM *param,QUICK_SELECT *quick,KEY_PART *key,
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
  key_tree->store(key[key_tree->part].store_length,
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
  if (!(range= new QUICK_RANGE((const char *) param->min_key,
			       (uint) (tmp_min_key - param->min_key),
			       (const char *) param->max_key,
			       (uint) (tmp_max_key - param->max_key),
			       flag)))
    return 1;			// out of memory

  set_if_bigger(quick->max_used_key_length,range->min_length);
  set_if_bigger(quick->max_used_key_length,range->max_length);
  set_if_bigger(quick->used_key_parts, (uint) key_tree->part+1);
  quick->ranges.push_back(range);

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

bool QUICK_SELECT::unique_key_range()
{
  if (ranges.elements == 1)
  {
    QUICK_RANGE *tmp;
    if (((tmp=ranges.head())->flag & (EQ_RANGE | NULL_RANGE)) == EQ_RANGE)
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
       key+= key_part++->store_length)
  {
    if (key_part->null_bit && *key)
      return 1;
  }
  return 0;
}


/****************************************************************************
  Create a QUICK RANGE based on a key
****************************************************************************/

QUICK_SELECT *get_quick_select_for_ref(THD *thd, TABLE *table, TABLE_REF *ref)
{
  MEM_ROOT *old_root= my_pthread_getspecific_ptr(MEM_ROOT*, THR_MALLOC);
  QUICK_SELECT *quick= new QUICK_SELECT(thd, table, ref->key);
  KEY *key_info = &table->key_info[ref->key];
  KEY_PART *key_part;
  QUICK_RANGE *range;
  uint part;

  if (!quick)
    return 0;			/* no ranges found */
  if (cp_buffer_from_ref(ref))
  {
    if (thd->is_fatal_error)
      goto err;					// out of memory
    goto ok;                                    // empty range
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
    key_part->length=  	    key_info->key_part[part].length;
    key_part->store_length= key_info->key_part[part].store_length;
    key_part->null_bit=     key_info->key_part[part].null_bit;
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
    if (!(null_range= new QUICK_RANGE((char*)ref->key_buff, ref->key_length,
				      (char*)ref->key_buff, ref->key_length,
				      EQ_RANGE)))
      goto err;
    *ref->null_ref_key= 0;		// Clear null byte
    if (quick->ranges.push_back(null_range))
      goto err;
  }

ok:
  my_pthread_setspecific_ptr(THR_MALLOC, old_root);
  return quick;

err:
  my_pthread_setspecific_ptr(THR_MALLOC, old_root);
  delete quick;
  return 0;
}

	/* get next possible record using quick-struct */

int QUICK_SELECT::get_next()
{
  DBUG_ENTER("get_next");

  for (;;)
  {
    int result;
    key_range start_key, end_key;
    if (range)
    {
      // Already read through key
      result= file->read_range_next();
      if (result != HA_ERR_END_OF_FILE)
	DBUG_RETURN(result);
    }

    if (!(range= it++))
      DBUG_RETURN(HA_ERR_END_OF_FILE);		// All ranges used

    start_key.key=    (const byte*) range->min_key;
    start_key.length= range->min_length;
    start_key.flag=   ((range->flag & NEAR_MIN) ? HA_READ_AFTER_KEY :
		       (range->flag & EQ_RANGE) ?
		       HA_READ_KEY_EXACT : HA_READ_KEY_OR_NEXT);
    end_key.key=      (const byte*) range->max_key;
    end_key.length=   range->max_length;
    /*
      We use READ_AFTER_KEY here because if we are reading on a key
      prefix we want to find all keys with this prefix
    */
    end_key.flag=     (range->flag & NEAR_MAX ? HA_READ_BEFORE_KEY :
		       HA_READ_AFTER_KEY);

    result= file->read_range_first(range->min_length ? &start_key : 0,
				   range->max_length ? &end_key : 0,
                                   test(range->flag & EQ_RANGE),
				   sorted);
    if (range->flag == (UNIQUE_RANGE | EQ_RANGE))
      range=0;				// Stop searching

    if (result != HA_ERR_END_OF_FILE)
      DBUG_RETURN(result);
    range=0;				// No matching rows; go to next range
  }
}


/* Get next for geometrical indexes */

int QUICK_SELECT_GEOM::get_next()
{
  DBUG_ENTER(" QUICK_SELECT_GEOM::get_next");

  for (;;)
  {
    int result;
    if (range)
    {
      // Already read through key
      result= file->index_next_same(record, (byte*) range->min_key,
				    range->min_length);
      if (result != HA_ERR_END_OF_FILE)
	DBUG_RETURN(result);
    }

    if (!(range= it++))
      DBUG_RETURN(HA_ERR_END_OF_FILE);		// All ranges used

    result= file->index_read(record,
			     (byte*) range->min_key,
			     range->min_length,
			     (ha_rkey_function)(range->flag ^ GEOM_FLAG));
    if (result != HA_ERR_KEY_NOT_FOUND)
      DBUG_RETURN(result);
    range=0;				// Not found, to next range
  }
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

QUICK_SELECT_DESC::QUICK_SELECT_DESC(QUICK_SELECT *q, uint used_key_parts)
  : QUICK_SELECT(*q), rev_it(rev_ranges)
{
  QUICK_RANGE *r;

  it.rewind();
  for (r = it++; r; r = it++)
  {
    rev_ranges.push_front(r);
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
      result=file->index_read(record, (byte*) range->max_key,
			      range->max_length,
			      ((range->flag & NEAR_MAX) ?
			       HA_READ_BEFORE_KEY : HA_READ_PREFIX_LAST_OR_PREV));
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

int QUICK_SELECT_DESC::cmp_prev(QUICK_RANGE *range_arg)
{
  int cmp;
  if (range_arg->flag & NO_MIN_RANGE)
    return 0;					/* key can't be to small */

  cmp= key_cmp(key_part_info, (byte*) range_arg->min_key,
               range_arg->min_length);
  if (cmp > 0 || cmp == 0 && !(range_arg->flag & NEAR_MIN))
    return 0;
  return 1;                                     // outside of range
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
  uint offset, end;
  KEY_PART *key_part = key_parts,
           *key_part_end= key_part+used_key_parts;

  for (offset= 0,  end = min(range_arg->min_length, range_arg->max_length) ;
       offset < end && key_part != key_part_end ;
       offset+= key_part++->store_length)
  {
    if (!memcmp((char*) range_arg->min_key+offset,
		(char*) range_arg->max_key+offset,
		key_part->store_length))
      continue;

    if (key_part->null_bit && range_arg->min_key[offset])
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


/*****************************************************************************
** Print a quick range for debugging
** TODO:
** This should be changed to use a String to store each row instead
** of locking the DEBUG stream !
*****************************************************************************/

#ifndef DBUG_OFF

static void
print_key(KEY_PART *key_part,const char *key,uint used_length)
{
  char buff[1024];
  const char *key_end= key+used_length;
  String tmp(buff,sizeof(buff),&my_charset_bin);
  uint store_length;

  for (; key < key_end; key+=store_length, key_part++)
  {
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
    field->set_key_image((char*) key, key_part->length, field->charset());
    field->val_str(&tmp);
    fwrite(tmp.ptr(),sizeof(char),tmp.length(),DBUG_FILE);
    if (key+store_length < key_end)
      fputc('/',DBUG_FILE);
  }
}


static void print_quick(QUICK_SELECT *quick,const key_map* needed_reg)
{
  QUICK_RANGE *range;
  char buf[MAX_KEY/8+1];
  DBUG_ENTER("print_param");
  if (! _db_on_ || !quick)
    DBUG_VOID_RETURN;

  List_iterator<QUICK_RANGE> li(quick->ranges);
  DBUG_LOCK_FILE;
  fprintf(DBUG_FILE,"Used quick_range on key: %d (other_keys: 0x%s):\n",
	  quick->index, needed_reg->print(buf));
  while ((range=li++))
  {
    if (!(range->flag & NO_MIN_RANGE))
    {
      print_key(quick->key_parts,range->min_key,range->min_length);
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
      print_key(quick->key_parts,range->max_key,range->max_length);
    }
    fputs("\n",DBUG_FILE);
  }
  DBUG_UNLOCK_FILE;
  DBUG_VOID_RETURN;
}

#endif

/*****************************************************************************
** Instansiate templates
*****************************************************************************/

#ifdef __GNUC__
template class List<QUICK_RANGE>;
template class List_iterator<QUICK_RANGE>;
#endif
