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


/* classes to use when handling where clause */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

#include "procedure.h"
#include <myisam.h>

typedef struct keyuse_t {
  TABLE *table;
  Item	*val;				/* or value if no field */
  table_map used_tables;
  uint	key, keypart, optimize;
  key_part_map keypart_map;
  ha_rows      ref_table_rows;
} KEYUSE;

class store_key;

typedef struct st_table_ref
{
  bool		key_err;
  uint          key_parts;                // num of ...
  uint          key_length;               // length of key_buff
  int           key;                      // key no
  byte          *key_buff;                // value to look for with key
  byte          *key_buff2;               // key_buff+key_length
  store_key     **key_copy;               //
  Item          **items;                  // val()'s for each keypart
  table_map	depend_map;		  // Table depends on these tables.
  byte          *null_ref_key;		  // null byte position in the key_buf.
  					  // used for REF_OR_NULL optimization.
} TABLE_REF;

/*
** CACHE_FIELD and JOIN_CACHE is used on full join to cache records in outer
** table
*/


typedef struct st_cache_field {
  char *str;
  uint length,blob_length;
  Field_blob *blob_field;
  bool strip;
} CACHE_FIELD;


typedef struct st_join_cache {
  uchar *buff,*pos,*end;
  uint records,record_nr,ptr_record,fields,length,blobs;
  CACHE_FIELD *field,**blob_ptr;
  SQL_SELECT *select;
} JOIN_CACHE;


/*
** The structs which holds the join connections and join states
*/
enum join_type { JT_UNKNOWN,JT_SYSTEM,JT_CONST,JT_EQ_REF,JT_REF,JT_MAYBE_REF,
		 JT_ALL, JT_RANGE, JT_NEXT, JT_FT, JT_REF_OR_NULL,
		 JT_UNIQUE_SUBQUERY, JT_INDEX_SUBQUERY, JT_INDEX_MERGE};

class JOIN;

typedef int (*Next_select_func)(JOIN *,struct st_join_table *,bool);
typedef int (*Read_record_func)(struct st_join_table *tab);


typedef struct st_join_table {
  TABLE		*table;
  KEYUSE	*keyuse;			/* pointer to first used key */
  SQL_SELECT	*select;
  COND		*select_cond;
  QUICK_SELECT_I *quick;
  Item		*on_expr;       /* associated on expression                  */
  st_join_table *first_inner;   /* first inner table for including outerjoin */
  bool           found;         /* true after all matches or null complement */
  bool           not_null_compl;/* true before null complement is added      */
  st_join_table *last_inner;    /* last table table for embedding outer join */
  st_join_table *first_upper;  /* first inner table for embedding outer join */
  st_join_table *first_unmatched; /* used for optimization purposes only     */
  const char	*info;
  Read_record_func read_first_record;
  Next_select_func next_select;
  READ_RECORD	read_record;
  double	worst_seeks;
  key_map	const_keys;			/* Keys with constant part */
  key_map	checked_keys;			/* Keys checked in find_best */
  key_map	needed_reg;
  key_map       keys;                           /* all keys with can be used */
  ha_rows	records,found_records,read_time;
  table_map	dependent,key_dependent;
  uint		use_quick,index;
  uint		status;				// Save status for cache
  uint		used_fields,used_fieldlength,used_blobs;
  enum join_type type;
  bool		cached_eq_ref_table,eq_ref_table,not_used_in_distinct;
  TABLE_REF	ref;
  JOIN_CACHE	cache;
  JOIN		*join;

  void cleanup();
} JOIN_TAB;


typedef struct st_position			/* Used in find_best */
{
  double records_read;
  double read_time;
  JOIN_TAB *table;
  KEYUSE *key;
} POSITION;

typedef struct st_rollup
{
  enum State { STATE_NONE, STATE_INITED, STATE_READY };
  State state;
  Item *item_null;
  Item ***ref_pointer_arrays;
  List<Item> *fields;
} ROLLUP;


class JOIN :public Sql_alloc
{
 public:
  JOIN_TAB *join_tab,**best_ref;
  JOIN_TAB **map2table;    // mapping between table indexes and JOIN_TABs
  JOIN_TAB *join_tab_save; // saved join_tab for subquery reexecution
  TABLE    **table,**all_tables,*sort_by_table;
  uint	   tables,const_tables;
  uint	   send_group_parts;
  bool	   sort_and_group,first_record,full_join,group, no_field_update;
  bool	   do_send_rows;
  table_map const_table_map,found_const_table_map,outer_join;
  ha_rows  send_records,found_records,examined_rows,row_limit, select_limit;
  /*
    Used to fetch no more than given amount of rows per one 
    fetch operation of server side cursor.
    The value is checked in end_send and end_send_group in fashion, similar
    to offset_limit_cnt:
      - fetch_limit= HA_POS_ERROR if there is no cursor.
      - when we open a cursor, we set fetch_limit to 0,
      - on each fetch iteration we add num_rows to fetch to fetch_limit
  */
  ha_rows  fetch_limit;
  POSITION positions[MAX_TABLES+1],best_positions[MAX_TABLES+1];
  double   best_read;
  List<Item> *fields;
  List<Item_buff> group_fields, group_fields_cache;
  TABLE    *tmp_table;
  // used to store 2 possible tmp table of SELECT
  TABLE    *exec_tmp_table1, *exec_tmp_table2;
  THD	   *thd;
  Item_sum  **sum_funcs, ***sum_funcs_end;
  /* second copy of sumfuncs (for queries with 2 temporary tables */
  Item_sum  **sum_funcs2, ***sum_funcs_end2;
  Procedure *procedure;
  Item	    *having;
  Item      *tmp_having; // To store having when processed temporary table
  Item      *having_history; // Store having for explain
  uint	    select_options;
  select_result *result;
  TMP_TABLE_PARAM tmp_table_param;
  MYSQL_LOCK *lock;
  // unit structure (with global parameters) for this select
  SELECT_LEX_UNIT *unit;
  // select that processed
  SELECT_LEX *select_lex;
  
  JOIN *tmp_join; // copy of this JOIN to be used with temporary tables
  ROLLUP rollup;				// Used with rollup

  bool select_distinct;				// Set if SELECT DISTINCT

  /*
    simple_xxxxx is set if ORDER/GROUP BY doesn't include any references
    to other tables than the first non-constant table in the JOIN.
    It's also set if ORDER/GROUP BY is empty.
  */
  bool simple_order, simple_group;
  /*
    Is set only in case if we have a GROUP BY clause
    and no ORDER BY after constant elimination of 'order'.
  */
  bool no_order;
  /* Is set if we have a GROUP BY and we have ORDER BY on a constant. */
  bool          skip_sort_order;

  bool need_tmp, hidden_group_fields, buffer_result;
  DYNAMIC_ARRAY keyuse;
  Item::cond_result cond_value;
  List<Item> all_fields; // to store all fields that used in query
  //Above list changed to use temporary table
  List<Item> tmp_all_fields1, tmp_all_fields2, tmp_all_fields3;
  //Part, shared with list above, emulate following list
  List<Item> tmp_fields_list1, tmp_fields_list2, tmp_fields_list3;
  List<Item> &fields_list; // hold field list passed to mysql_select
  int error;

  ORDER *order, *group_list, *proc_param; //hold parameters of mysql_select
  COND *conds;                            // ---"---
  Item *conds_history;                    // store WHERE for explain
  TABLE_LIST *tables_list;           //hold 'tables' parameter of mysql_select
  List<TABLE_LIST> *join_list;       // list of joined tables in reverse order
  SQL_SELECT *select;                //created in optimisation phase
  JOIN_TAB *return_tab;              //used only for outer joins
  Item **ref_pointer_array; //used pointer reference for this select
  // Copy of above to be used with different lists
  Item **items0, **items1, **items2, **items3, **current_ref_pointer_array;
  uint ref_pointer_array_size; // size of above in bytes
  const char *zero_result_cause; // not 0 if exec must return zero result
  
  bool union_part; // this subselect is part of union 
  bool optimized; // flag to avoid double optimization in EXPLAIN

  JOIN(THD *thd_arg, List<Item> &fields_arg, ulong select_options_arg,
       select_result *result_arg)
    :fields_list(fields_arg)
  {
    init(thd_arg, fields_arg, select_options_arg, result_arg);
  }
  
  void init(THD *thd_arg, List<Item> &fields_arg, ulong select_options_arg,
       select_result *result_arg)
  {
    join_tab= join_tab_save= 0;
    table= 0;
    tables= 0;
    const_tables= 0;
    join_list= 0;
    sort_and_group= 0;
    first_record= 0;
    do_send_rows= 1;
    send_records= 0;
    found_records= 0;
    fetch_limit= HA_POS_ERROR;
    examined_rows= 0;
    exec_tmp_table1= 0;
    exec_tmp_table2= 0;
    thd= thd_arg;
    sum_funcs= sum_funcs2= 0;
    procedure= 0;
    having= tmp_having= having_history= 0;
    select_options= select_options_arg;
    result= result_arg;
    lock= thd_arg->lock;
    select_lex= 0; //for safety
    tmp_join= 0;
    select_distinct= test(select_options & SELECT_DISTINCT);
    no_order= 0;
    simple_order= 0;
    simple_group= 0;
    skip_sort_order= 0;
    need_tmp= 0;
    hidden_group_fields= 0; /*safety*/
    buffer_result= test(select_options & OPTION_BUFFER_RESULT) &&
      !test(select_options & OPTION_FOUND_ROWS);
    all_fields= fields_arg;
    fields_list= fields_arg;
    error= 0;
    select= 0;
    return_tab= 0;
    ref_pointer_array= items0= items1= items2= items3= 0;
    ref_pointer_array_size= 0;
    zero_result_cause= 0;
    optimized= 0;

    fields_list= fields_arg;
    bzero((char*) &keyuse,sizeof(keyuse));
    tmp_table_param.copy_field=0;
    tmp_table_param.end_write_records= HA_POS_ERROR;
    rollup.state= ROLLUP::STATE_NONE;
  }

  int prepare(Item ***rref_pointer_array, TABLE_LIST *tables, uint wind_num,
	      COND *conds, uint og_num, ORDER *order, ORDER *group,
	      Item *having, ORDER *proc_param, SELECT_LEX *select,
	      SELECT_LEX_UNIT *unit);
  int optimize();
  int reinit();
  void exec();
  int cleanup();
  void restore_tmp();
  bool alloc_func_list();
  bool make_sum_func_list(List<Item> &all_fields, List<Item> &send_fields,
			  bool before_group_by);

  inline void set_items_ref_array(Item **ptr)
  {
    memcpy((char*) ref_pointer_array, (char*) ptr, ref_pointer_array_size);
    current_ref_pointer_array= ptr;
  }
  inline void init_items_ref_array()
  {
    items0= ref_pointer_array + all_fields.elements;
    memcpy(items0, ref_pointer_array, ref_pointer_array_size);
    current_ref_pointer_array= items0;
  }

  bool rollup_init();
  bool rollup_make_fields(List<Item> &all_fields, List<Item> &fields,
			  Item_sum ***func);
  int rollup_send_data(uint idx);
  bool test_in_subselect(Item **where);
  void join_free(bool full);
  void clear();
  bool save_join_tab();
  bool send_row_on_empty_set()
  {
    return (do_send_rows && tmp_table_param.sum_func_count != 0 &&
	    !group_list);
  }
  bool change_result(select_result *result);
};


/*
  Server-side cursor (now stands only for basic read-only cursor)
  See class implementation in sql_select.cc
*/

class Cursor: public Sql_alloc, public Item_arena
{
  JOIN *join;
  SELECT_LEX_UNIT *unit;

  TABLE *open_tables;
  MYSQL_LOCK *lock;
  TABLE *derived_tables;
  /* List of items created during execution */
  ulong query_id;
public:
  select_send result;

  /* Temporary implementation as now we replace THD state by value */
  /* Save THD state into cursor */
  void init_from_thd(THD *thd);
  /* Restore THD from cursor to continue cursor execution */
  void init_thd(THD *thd);
  /* bzero cursor state in THD */
  void reset_thd(THD *thd);

  int open(JOIN *join);
  int fetch(ulong num_rows);
  void reset() { join= 0; }
  bool is_open() const { return join != 0; }
  void close();

  void set_unit(SELECT_LEX_UNIT *unit_arg) { unit= unit_arg; }
  Cursor() :join(0), unit(0) {}
  ~Cursor();
};


typedef struct st_select_check {
  uint const_ref,reg_ref;
} SELECT_CHECK;

extern const char *join_type_str[];
void TEST_join(JOIN *join);

/* Extern functions in sql_select.cc */
bool store_val_in_field(Field *field,Item *val);
TABLE *create_tmp_table(THD *thd,TMP_TABLE_PARAM *param,List<Item> &fields,
			ORDER *group, bool distinct, bool save_sum_fields,
			ulong select_options, ha_rows rows_limit,
			char* alias);
void free_tmp_table(THD *thd, TABLE *entry);
void count_field_types(TMP_TABLE_PARAM *param, List<Item> &fields,
		       bool reset_with_sum_func);
bool setup_copy_fields(THD *thd, TMP_TABLE_PARAM *param,
		       Item **ref_pointer_array,
		       List<Item> &new_list1, List<Item> &new_list2,
		       uint elements, List<Item> &fields);
void copy_fields(TMP_TABLE_PARAM *param);
void copy_funcs(Item **func_ptr);
bool create_myisam_from_heap(THD *thd, TABLE *table, TMP_TABLE_PARAM *param,
			     int error, bool ignore_last_dupp_error);
uint find_shortest_key(TABLE *table, const key_map *usable_keys);

/* functions from opt_sum.cc */
int opt_sum_query(TABLE_LIST *tables, List<Item> &all_fields,COND *conds);

/* from sql_delete.cc, used by opt_range.cc */
extern "C" int refpos_order_cmp(void* arg, const void *a,const void *b);

/* class to copying an field/item to a key struct */

class store_key :public Sql_alloc
{
 protected:
  Field *to_field;				// Store data here
  char *null_ptr;
  char err;
 public:
  store_key(THD *thd, Field *field_arg, char *ptr, char *null, uint length)
    :null_ptr(null),err(0)
  {
    if (field_arg->type() == FIELD_TYPE_BLOB)
      to_field=new Field_varstring(ptr, length, (uchar*) null, 1, 
				   Field::NONE, field_arg->field_name,
				   field_arg->table, field_arg->charset());
    else
    {
      to_field=field_arg->new_field(&thd->mem_root,field_arg->table);
      if (to_field)
	to_field->move_field(ptr, (uchar*) null, 1);
    }
  }
  virtual ~store_key() {}			/* Not actually needed */
  virtual bool copy()=0;
  virtual const char *name() const=0;
};


class store_key_field: public store_key
{
  Copy_field copy_field;
  const char *field_name;
 public:
  store_key_field(THD *thd, Field *to_field_arg, char *ptr, char *null_ptr_arg,
		  uint length, Field *from_field, const char *name_arg)
    :store_key(thd, to_field_arg,ptr,
	       null_ptr_arg ? null_ptr_arg : from_field->maybe_null() ? &err
	       : NullS,length), field_name(name_arg)
  {
    if (to_field)
    {
      copy_field.set(to_field,from_field,0);
    }
  }
  bool copy()
  {
    copy_field.do_copy(&copy_field);
    return err != 0;
  }
  const char *name() const { return field_name; }
};


class store_key_item :public store_key
{
 protected:
  Item *item;
public:
  store_key_item(THD *thd, Field *to_field_arg, char *ptr, char *null_ptr_arg,
		 uint length, Item *item_arg)
    :store_key(thd, to_field_arg,ptr,
	       null_ptr_arg ? null_ptr_arg : item_arg->maybe_null ?
	       &err : NullS, length), item(item_arg)
  {}
  bool copy()
  {
    return item->save_in_field_no_warnings(to_field, 1) || err != 0;
  }
  const char *name() const { return "func"; }
};


class store_key_const_item :public store_key_item
{
  bool inited;
public:
  store_key_const_item(THD *thd, Field *to_field_arg, char *ptr,
		       char *null_ptr_arg, uint length,
		       Item *item_arg)
    :store_key_item(thd, to_field_arg,ptr,
		    null_ptr_arg ? null_ptr_arg : item_arg->maybe_null ?
		    &err : NullS, length, item_arg), inited(0)
  {
  }
  bool copy()
  {
    if (!inited)
    {
      inited=1;
      if (item->save_in_field(to_field, 1))
	err= 1;
    }
    return err != 0;
  }
  const char *name() const { return "const"; }
};

bool cp_buffer_from_ref(TABLE_REF *ref);
bool error_if_full_join(JOIN *join);
int report_error(TABLE *table, int error);
int safe_index_read(JOIN_TAB *tab);
