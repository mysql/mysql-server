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
  uint	key,keypart;
  table_map used_tables;
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
		 JT_ALL, JT_RANGE, JT_NEXT, JT_FT};

class JOIN;

typedef struct st_join_table {
  TABLE		*table;
  KEYUSE	*keyuse;			/* pointer to first used key */
  SQL_SELECT	*select;
  COND		*select_cond;
  QUICK_SELECT	*quick;
  Item		*on_expr;
  const char	*info;
  int		(*read_first_record)(struct st_join_table *tab);
  int		(*next_select)(JOIN *,struct st_join_table *,bool);
  READ_RECORD	read_record;
  double	worst_seeks;
  key_map	const_keys;			/* Keys with constant part */
  key_map	checked_keys;			/* Keys checked in find_best */
  key_map	needed_reg;
  ha_rows	records,found_records,read_time;
  table_map	dependent,key_dependent;
  uint		keys;				/* all keys with can be used */
  uint		use_quick,index;
  uint		status;				// Save status for cache
  uint		used_fields,used_fieldlength,used_blobs;
  enum join_type type;
  bool		cached_eq_ref_table,eq_ref_table,not_used_in_distinct;
  TABLE_REF	ref;
  JOIN_CACHE	cache;
  JOIN		*join;
} JOIN_TAB;


typedef struct st_position {			/* Used in find_best */
  double records_read;
  JOIN_TAB *table;
  KEYUSE *key;
} POSITION;


/* Param to create temporary tables when doing SELECT:s */

class TMP_TABLE_PARAM {
 public:
  List<Item> copy_funcs;
  Copy_field *copy_field;
  byte	    *group_buff;
  Item_result_field **funcs;
  MI_COLUMNDEF *recinfo,*start_recinfo;
  KEY *keyinfo;
  ha_rows end_write_records;
  uint	copy_field_count,field_count,sum_func_count,func_count;
  uint  hidden_field_count;
  uint	group_parts,group_length;
  uint	quick_group;
  bool  using_indirect_summary_function;

  TMP_TABLE_PARAM() :copy_field(0), group_parts(0), group_length(0)
  {}
  ~TMP_TABLE_PARAM()
  {
    cleanup();
  }
  inline void cleanup(void)
  {
    if (copy_field)				/* Fix for Intel compiler */
    {
      delete [] copy_field;
      copy_field=0;
    }
  }
};


class JOIN {
 public:
  JOIN_TAB *join_tab,**best_ref,**map2table;
  TABLE    **table,**all_tables,*sort_by_table;
  uint	   tables,const_tables;
  uint	   send_group_parts;
  bool	   sort_and_group,first_record,full_join,group, no_field_update;
  table_map const_table_map,outer_join;
  ha_rows  send_records,found_records,examined_rows,row_limit;
  POSITION positions[MAX_TABLES+1],best_positions[MAX_TABLES+1];
  double   best_read;
  List<Item> *fields;
  List<Item_buff> group_fields;
  TABLE    *tmp_table;
  THD	   *thd;
  Item_sum  **sum_funcs;
  Procedure *procedure;
  Item	    *having;
  uint	    select_options;
  select_result *result;
  TMP_TABLE_PARAM tmp_table_param;
  MYSQL_LOCK *lock;
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
			bool allow_distinct_limit, uint select_options);
void free_tmp_table(THD *thd, TABLE *entry);
void count_field_types(TMP_TABLE_PARAM *param, List<Item> &fields,
		       bool reset_with_sum_func);
bool setup_copy_fields(TMP_TABLE_PARAM *param,List<Item> &fields);
void copy_fields(TMP_TABLE_PARAM *param);
void copy_funcs(Item_result_field **func_ptr);
bool create_myisam_from_heap(TABLE *table, TMP_TABLE_PARAM *param, int error,
			     bool ignore_last_dupp_error);

/* functions from opt_sum.cc */
int opt_sum_query(TABLE_LIST *tables, List<Item> &all_fields,COND *conds);


/* class to copying an field/item to a key struct */

class store_key :public Sql_alloc
{
 protected:
  Field *to_field;				// Store data here
  Field *key_field;				// Copy of key field
  char *null_ptr;
  char err;
 public:
  store_key(Field *field_arg, char *ptr, char *null, uint length)
    :null_ptr(null),err(0)
  {
    if (field_arg->type() == FIELD_TYPE_BLOB)
      to_field=new Field_varstring(ptr, length, (uchar*) null, 1, 
				   Field::NONE, field_arg->field_name,
				   field_arg->table, field_arg->binary());
    else
    {
      to_field=field_arg->new_field(field_arg->table);
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
  store_key_field(Field *to_field_arg, char *ptr, char *null_ptr_arg,
		  uint length, Field *from_field, const char *name_arg)
    :store_key(to_field_arg,ptr,
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
  store_key_item(Field *to_field_arg, char *ptr, char *null_ptr_arg,
		 uint length, Item *item_arg)
    :store_key(to_field_arg,ptr,
	       null_ptr_arg ? null_ptr_arg : item_arg->maybe_null ?
	       &err : NullS, length), item(item_arg)
  {}
  bool copy()
  {
    item->save_in_field(to_field);
    return err != 0;
  }
  const char *name() const { return "func"; }
};


class store_key_const_item :public store_key_item
{
  bool inited;
public:
  store_key_const_item(Field *to_field_arg, char *ptr,
		       char *null_ptr_arg, uint length,
		       Item *item_arg)
    :store_key_item(to_field_arg,ptr,
		    null_ptr_arg ? null_ptr_arg : item_arg->maybe_null ?
		    &err : NullS, length, item_arg), inited(0)
  {
  }
  bool copy()
  {
    if (!inited)
    {
      inited=1;
      item->save_in_field(to_field);
    }
    return err != 0;
  }
  const char *name() const { return "const"; }
};

bool cp_buffer_from_ref(TABLE_REF *ref);
