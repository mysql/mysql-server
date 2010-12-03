#ifndef SQL_SELECT_INCLUDED
#define SQL_SELECT_INCLUDED

/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


/**
  @file

  @brief
  classes to use when handling where clause
*/

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

#include "procedure.h"
#include <myisam.h>
#include "sql_array.h"                        /* Array */
#include "records.h"                          /* READ_RECORD */
#include "opt_range.h"                /* SQL_SELECT, QUICK_SELECT_I */


/* Values in optimize */
#define KEY_OPTIMIZE_EXISTS		1
#define KEY_OPTIMIZE_REF_OR_NULL	2

/**
  Information about usage of an index to satisfy an equality condition.

  @note such objects are stored in DYNAMIC_ARRAY which uses sizeof(), so keep
  this class as POD as possible.
*/
class Key_use {
public:
  Key_use(TABLE *table_arg, Item *val_arg, table_map used_tables_arg,
          uint key_arg, uint keypart_arg, uint optimize_arg,
          key_part_map keypart_map_arg, ha_rows ref_table_rows_arg,
          bool null_rejecting_arg, bool *cond_guard_arg,
          uint sj_pred_no_arg) :
  table(table_arg), val(val_arg), used_tables(used_tables_arg),
  key(key_arg), keypart(keypart_arg), optimize(optimize_arg),
  keypart_map(keypart_map_arg), ref_table_rows(ref_table_rows_arg),
  null_rejecting(null_rejecting_arg), cond_guard(cond_guard_arg),
  sj_pred_no(sj_pred_no_arg)
  {}
  TABLE *table;            ///< table owning the index
  Item	*val;              ///< other side of the equality, or value if no field
  table_map used_tables;   ///< tables used on other side of equality
  uint key;                ///< number of index
  uint keypart;            ///< used part of the index
  uint optimize;           ///< 0, or KEY_OPTIMIZE_*
  key_part_map keypart_map;       ///< like keypart, but as a bitmap
  ha_rows      ref_table_rows;    ///< Estimate of how many rows for a key value
  /**
    If true, the comparison this value was created from will not be
    satisfied if val has NULL 'value'.
    Not used if the index is fulltext (such index cannot be used for
    equalities).
  */
  bool null_rejecting;
  /**
    !NULL - This Key_use was created from an equality that was wrapped into
            an Item_func_trig_cond. This means the equality (and validity of
            this Key_use element) can be turned on and off. The on/off state
            is indicted by the pointed value:
              *cond_guard == TRUE <=> equality condition is on
              *cond_guard == FALSE <=> equality condition is off

    NULL  - Otherwise (the source equality can't be turned off)

    Not used if the index is fulltext (such index cannot be used for
    equalities).
  */
  bool *cond_guard;
  /**
     0..64    <=> This was created from semi-join IN-equality # sj_pred_no.
     MAX_UINT  Otherwise

     Not used if the index is fulltext (such index cannot be used for
     semijoin).
  */
  uint         sj_pred_no;
};

class store_key;

typedef struct st_table_ref : public Sql_alloc
{
  bool		key_err;
  /** True if something was read into buffer in join_read_key.  */
  bool          has_record;
  uint          key_parts;                ///< num of ...
  uint          key_length;               ///< length of key_buff
  int           key;                      ///< key no
  uchar         *key_buff;                ///< value to look for with key
  uchar         *key_buff2;               ///< key_buff+key_length
  store_key     **key_copy;               //
  Item          **items;                  ///< val()'s for each keypart
  /*  
    Array of pointers to trigger variables. Some/all of the pointers may be
    NULL.  The ref access can be used iff
    
      for each used key part i, (!cond_guards[i] || *cond_guards[i]) 

    This array is used by subquery code. The subquery code may inject
    triggered conditions, i.e. conditions that can be 'switched off'. A ref 
    access created from such condition is not valid when at least one of the 
    underlying conditions is switched off (see subquery code for more details)
  */
  bool          **cond_guards;
  /**
    (null_rejecting & (1<<i)) means the condition is '=' and no matching
    rows will be produced if items[i] IS NULL (see add_not_null_conds())
  */
  key_part_map  null_rejecting;
  table_map	depend_map;		  ///< Table depends on these tables.
  /* null byte position in the key_buf. Used for REF_OR_NULL optimization */
  uchar          *null_ref_key;
  /*
    The number of times the record associated with this key was used
    in the join.
  */
  ha_rows       use_count;

  /*
    TRUE <=> disable the "cache" as doing lookup with the same key value may
    produce different results (because of Index Condition Pushdown)
  */
  bool          disable_cache;

  st_table_ref()
    : key_err(TRUE),
      has_record(FALSE),
      key_parts(0),
      key_length(0),
      key(-1),
      key_buff(NULL),
      key_buff2(NULL),
      key_copy(NULL),
      items(NULL),
      cond_guards(NULL),
      null_rejecting(0),
      depend_map(0),
      null_ref_key(NULL),
      use_count(0),
      disable_cache(FALSE)
  {
  }

  /**
    @returns whether the reference contains NULL values which could never give
    a match.
  */
  bool impossible_null_ref() const
  {
    if (null_rejecting != 0)
    {
      for (uint i= 0 ; i < key_parts ; i++)
      {
        if ((null_rejecting & 1 << i) && items[i]->is_null())
          return TRUE;
      }
    }
    return FALSE;
  }

} TABLE_REF;


/*
  The structs which holds the join connections and join states
*/
enum join_type { JT_UNKNOWN,JT_SYSTEM,JT_CONST,JT_EQ_REF,JT_REF,JT_MAYBE_REF,
		 JT_ALL, JT_RANGE, JT_NEXT, JT_FT, JT_REF_OR_NULL,
		 JT_UNIQUE_SUBQUERY, JT_INDEX_SUBQUERY, JT_INDEX_MERGE};

class JOIN;

enum enum_nested_loop_state
{
  NESTED_LOOP_KILLED= -2, NESTED_LOOP_ERROR= -1,
  NESTED_LOOP_OK= 0, NESTED_LOOP_NO_MORE_ROWS= 1,
  NESTED_LOOP_QUERY_LIMIT= 3, NESTED_LOOP_CURSOR_LIMIT= 4
};


/* Values for JOIN_TAB::packed_info */
#define TAB_INFO_HAVE_VALUE 1
#define TAB_INFO_USING_INDEX 2
#define TAB_INFO_USING_WHERE 4
#define TAB_INFO_FULL_SCAN_ON_NULL 8

class JOIN_CACHE;
class SJ_TMP_TABLE;

typedef enum_nested_loop_state
(*Next_select_func)(JOIN *, struct st_join_table *, bool);
Next_select_func setup_end_select_func(JOIN *join);
int rr_sequential(READ_RECORD *info);

#define SJ_OPT_NONE 0
#define SJ_OPT_DUPS_WEEDOUT 1
#define SJ_OPT_LOOSE_SCAN   2
#define SJ_OPT_FIRST_MATCH  3
#define SJ_OPT_MATERIALIZE_LOOKUP  4
#define SJ_OPT_MATERIALIZE_SCAN  5

inline bool sj_is_materialize_strategy(uint strategy)
{
  return strategy >= SJ_OPT_MATERIALIZE_LOOKUP;
}

/** 
    Bits describing quick select type
*/
enum quick_type { QS_NONE, QS_RANGE, QS_DYNAMIC_RANGE};

typedef struct st_join_table : public Sql_alloc
{
  st_join_table();

  TABLE		*table;
  Key_use	*keyuse;			/**< pointer to first used key */
  SQL_SELECT	*select;
  Item		*select_cond;
  QUICK_SELECT_I *quick;
  Item	       **on_expr_ref;   /**< pointer to the associated on expression   */
  COND_EQUAL    *cond_equal;    /**< multiple equalities for the on expression */
  st_join_table *first_inner;   /**< first inner table for including outerjoin */
  bool           found;         /**< true after all matches or null complement */
  bool           not_null_compl;/**< true before null complement is added      */
  st_join_table *last_inner;    /**< last table table for embedding outer join */
  st_join_table *first_upper;  /**< first inner table for embedding outer join */
  st_join_table *first_unmatched; /**< used for optimization purposes only     */
  /* 
    The value of select_cond before we've attempted to do Index Condition
    Pushdown. We may need to restore everything back if we first choose one
    index but then reconsider (see test_if_skip_sort_order() for such
    scenarios).
    NULL means no index condition pushdown was performed.
  */
  Item          *pre_idx_push_select_cond;
  
  /* Special content for EXPLAIN 'Extra' column or NULL if none */
  const char	*info;
  /* 
    Bitmap of TAB_INFO_* bits that encodes special line for EXPLAIN 'Extra'
    column, or 0 if there is no info.
  */
  uint          packed_info;

  READ_RECORD::Setup_func read_first_record;
  //TODO try to unify with save_read_first_record below
  READ_RECORD::Setup_func saved_read_first_record;
  Next_select_func next_select;
  READ_RECORD	read_record;
  /* 
    Currently the following two fields are used only for a [NOT] IN subquery
    if it is executed by an alternative full table scan when the left operand of
    the subquery predicate is evaluated to NULL.
  */  
  READ_RECORD::Setup_func save_read_first_record;/* to save read_first_record */
  READ_RECORD::Read_func save_read_record;/* to save read_record.read_record */
  double	worst_seeks;
  key_map	const_keys;			/**< Keys with constant part */
  key_map	checked_keys;			/**< Keys checked */
  key_map	needed_reg;
  key_map       keys;                           /**< all keys with can be used */

  /* Either #rows in the table or 1 for const table.  */
  ha_rows	records;
  /*
    Number of records that will be scanned (yes scanned, not returned) by the
    best 'independent' access method, i.e. table scan or QUICK_*_SELECT)
  */
  ha_rows       found_records;
  /*
    Cost of accessing the table using "ALL" or range/index_merge access
    method (but not 'index' for some reason), i.e. this matches method which
    E(#records) is in found_records.
  */
  ha_rows       read_time;
  
  table_map	dependent,key_dependent;
  uint		index;
  uint		status;				///< Save status for cache
  uint		used_fields,used_fieldlength,used_blobs;
  uint          used_null_fields;
  uint          used_rowid_fields;
  uint          used_uneven_bit_fields;
  enum quick_type use_quick;
  enum join_type type;
  bool		cached_eq_ref_table,eq_ref_table,not_used_in_distinct;
  /* TRUE <=> index-based access method must return records in order */
  bool		sorted;
  /* 
    If it's not 0 the number stored this field indicates that the index
    scan has been chosen to access the table data and we expect to scan 
    this number of rows for the table.
  */ 
  ha_rows       limit; 
  TABLE_REF	ref;
  /** Join cache type (same as return code of check_join_cache_level() */
  uint          use_join_cache;
  JOIN_CACHE	*cache;
  /*
    Index condition for BKA access join
  */
  Item          *cache_idx_cond;
  SQL_SELECT    *cache_select;
  JOIN		*join;

  /* SemiJoinDuplicateElimination variables: */
  /*
    Embedding SJ-nest (may be not the direct parent), or NULL if none.
    This variable holds the result of table pullout.
  */
  TABLE_LIST    *emb_sj_nest;

  /**
    Boundaries of semijoin inner tables around this table. Valid only once
    final QEP has been chosen. Depending on the strategy, they may define an
    interval (all tables inside are inner of a semijoin) or
    not. last_sj_inner_tab is not set for Duplicates Weedout.
  */
  struct st_join_table *first_sj_inner_tab;
  struct st_join_table *last_sj_inner_tab;

  /* Variables for semi-join duplicate elimination */
  SJ_TMP_TABLE  *flush_weedout_table;
  SJ_TMP_TABLE  *check_weed_out_table;
  
  /*
    If set, means we should stop join enumeration after we've got the first
    match and return to the specified join tab. May point to
    join->join_tab[-1] which means stop join execution after the first
    match.
  */
  struct st_join_table  *do_firstmatch;
 
  /* 
     ptr  - We're doing a LooseScan, this join tab is the first (i.e. 
            "driving") join tab), and ptr points to the last join tab
            handled by the strategy. loosescan_match_tab->found_match
            should be checked to see if the current value group had a match.
     NULL - Not doing a loose scan on this join tab.
  */
  struct st_join_table *loosescan_match_tab;

  /* Buffer to save index tuple to be able to skip duplicates */
  uchar *loosescan_buf;
  
  /* Length of key tuple (depends on #keyparts used) to store in the above */
  uint loosescan_key_len;

  /* Used by LooseScan. TRUE<=> there has been a matching record combination */
  bool found_match;
  
  /*
    Used by DuplicateElimination. tab->table->ref must have the rowid
    whenever we have a current record.
  */
  int  keep_current_rowid;

  /* NestedOuterJoins: Bitmap of nested joins this table is part of */
  nested_join_map embedding_map;

  void cleanup();
  inline bool is_using_loose_index_scan()
  {
    return (select && select->quick &&
            (select->quick->get_type() ==
             QUICK_SELECT_I::QS_TYPE_GROUP_MIN_MAX));
  }
  bool is_using_agg_loose_index_scan ()
  {
    return (is_using_loose_index_scan() &&
            ((QUICK_GROUP_MIN_MAX_SELECT *)select->quick)->is_agg_distinct());
  }
  /* SemiJoinDuplicateElimination: reserve space for rowid */
  bool check_rowid_field()
  {
    if (keep_current_rowid && !used_rowid_fields)
    {
      used_rowid_fields= 1;
      used_fieldlength+= table->file->ref_length;
    }
    return test(used_rowid_fields);
  }
  bool is_inner_table_of_outer_join()
  {
    return first_inner != NULL;
  }
  bool is_single_inner_of_semi_join()
  {
    return first_sj_inner_tab == this && last_sj_inner_tab == this;
  }
  bool is_single_inner_of_outer_join()
  {
    return first_inner == this && first_inner->last_inner == this;
  }
  bool is_first_inner_for_outer_join()
  {
    return first_inner && first_inner == this;
  }
  void set_select_cond(Item *to, uint line)
  {
    DBUG_PRINT("info", ("select_cond changes %p -> %p at line %u tab %p",
                        select_cond, to, line, this));
    select_cond= to;
  }
  Item *set_cond(Item *new_cond, uint line)
  {
    Item *tmp_select_cond= select_cond;
    set_select_cond(new_cond, line);
    if (select)
      select->cond= new_cond;
    return tmp_select_cond;
  }
  uint get_sj_strategy() const;
} JOIN_TAB;


inline
st_join_table::st_join_table()
  : table(NULL),
    keyuse(NULL),
    select(NULL),
    select_cond(NULL),
    quick(NULL),
    on_expr_ref(NULL),
    cond_equal(NULL),
    first_inner(NULL),
    found(FALSE),
    not_null_compl(FALSE),
    last_inner(NULL),
    first_upper(NULL),
    first_unmatched(NULL),
    pre_idx_push_select_cond(NULL),
    info(NULL),
    packed_info(0),
    read_first_record(NULL),
    next_select(NULL),
    read_record(),
    save_read_first_record(NULL),
    save_read_record(NULL),
    worst_seeks(0.0),
    const_keys(),
    checked_keys(),
    needed_reg(),
    keys(),

    records(0),
    found_records(0),
    read_time(0),

    dependent(0),
    key_dependent(0),
    index(0),
    status(0),
    used_fields(0),
    used_fieldlength(0),
    used_blobs(0),
    used_null_fields(0),
    used_rowid_fields(0),
    used_uneven_bit_fields(0),
    use_quick(QS_NONE),
    type(JT_UNKNOWN),
    cached_eq_ref_table(FALSE),
    eq_ref_table(FALSE),
    not_used_in_distinct(FALSE),
    sorted(FALSE),

    limit(0),
    ref(),
    use_join_cache(FALSE),
    cache(NULL),

    cache_idx_cond(NULL),
    cache_select(NULL),
    join(NULL),

    emb_sj_nest(NULL),
    first_sj_inner_tab(NULL),
    last_sj_inner_tab(NULL),

    flush_weedout_table(NULL),
    check_weed_out_table(NULL),
    do_firstmatch(NULL),
    loosescan_match_tab(NULL),
    loosescan_buf(NULL),
    loosescan_key_len(0),
    found_match(FALSE),

    keep_current_rowid(0),
    embedding_map(0)
{
  /**
    @todo Add constructor to READ_RECORD.
    All users do init_read_record(), which does bzero(),
    rather than invoking a constructor.
  */
  bzero(&read_record, sizeof(read_record));
}



/* 
  Categories of data fields of variable length written into join cache buffers.
  The value of any of these fields is written into cache together with the
  prepended length of the value.     
*/
#define CACHE_BLOB      1        /* blob field  */
#define CACHE_STRIPPED  2        /* field stripped of trailing spaces */
#define CACHE_VARSTR1   3        /* short string value (length takes 1 byte) */ 
#define CACHE_VARSTR2   4        /* long string value (length takes 2 bytes) */

/*
  The CACHE_FIELD structure used to describe fields of records that
  are written into a join cache buffer from record buffers and backward.
*/
typedef struct st_cache_field {
  uchar *str;   /**< buffer from/to where the field is to be copied */ 
  uint length;  /**< maximal number of bytes to be copied from/to str */
  /* 
    Field object for the moved field
    (0 - for a flag field, see JOIN_CACHE::create_flag_fields).
  */
  Field *field;
  uint type;    /**< category of the of the copied field (CACHE_BLOB et al.) */
  /* 
    The number of the record offset value for the field in the sequence
    of offsets placed after the last field of the record. These
    offset values are used to access fields referred to from other caches.
    If the value is 0 then no offset for the field is saved in the
    trailing sequence of offsets.
  */ 
  uint referenced_field_no; 
  TABLE *get_rowid; /**< only for ROWID fields used for Duplicate Elimination */
  /* The remaining structure fields are used as containers for temp values */
  uint blob_length; /**< length of the blob to be copied */
  uint offset;      /**< field offset to be saved in cache buffer */
} CACHE_FIELD;


/*
  JOIN_CACHE is the base class to support the implementations of both
  Blocked-Based Nested Loops (BNL) Join Algorithm and Batched Key Access (BKA)
  Join Algorithm. The first algorithm is supported by the derived class
  JOIN_CACHE_BNL, while the second algorithm is supported by the derived
  class JOIN_CACHE_BKA.
  These two algorithms have a lot in common. Both algorithms first
  accumulate the records of the left join operand in a join buffer and
  then search for matching rows of the second operand for all accumulated
  records.
  For the first algorithm this strategy saves on logical I/O operations:
  the entire set of records from the join buffer requires only one look-through
  the records provided by the second operand. 
  For the second algorithm the accumulation of records allows to optimize
  fetching rows of the second operand from disk for some engines (MyISAM, 
  InnoDB), or to minimize the number of round-trips between the Server and
  the engine nodes (NDB Cluster).        
*/ 

class JOIN_CACHE :public Sql_alloc
{

private:

  /* Size of the offset of a record from the cache */   
  uint size_of_rec_ofs;    
  /* Size of the length of a record in the cache */
  uint size_of_rec_len;
  /* Size of the offset of a field within a record in the cache */   
  uint size_of_fld_ofs;

protected:
       
  /* 3 functions below actually do not use the hidden parameter 'this' */ 

  /* Calculate the number of bytes used to store an offset value */
  uint offset_size(uint len)
  { return (len < 256 ? 1 : len < 256*256 ? 2 : 4); }

  /* Get the offset value that takes ofs_sz bytes at the position ptr */
  ulong get_offset(uint ofs_sz, uchar *ptr)
  {
    switch (ofs_sz) {
    case 1: return uint(*ptr);
    case 2: return uint2korr(ptr);
    case 4: return uint4korr(ptr);
    }
    return 0;
  }

  /* Set the offset value ofs that takes ofs_sz bytes at the position ptr */ 
  void store_offset(uint ofs_sz, uchar *ptr, ulong ofs)
  {
    switch (ofs_sz) {
    case 1: *ptr= (uchar) ofs; return;
    case 2: int2store(ptr, (uint16) ofs); return;
    case 4: int4store(ptr, (uint32) ofs); return;
    }
  }
  
  /* 
    The total maximal length of the fields stored for a record in the cache.
    For blob fields only the sizes of the blob lengths are taken into account. 
  */
  uint length;

  /* 
    Representation of the executed multi-way join through which all needed
    context can be accessed.  
  */   
  JOIN *join;  

  /* 
    Cardinality of the range of join tables whose fields can be put into the
    cache. (A table from the range not necessarily contributes to the cache.)
  */
  uint tables;

  /* 
    The total number of flag and data fields that can appear in a record
    written into the cache. Fields with null values are always skipped 
    to save space. 
  */
  uint fields;

  /* 
    The total number of flag fields in a record put into the cache. They are
    used for table null bitmaps, table null row flags, and an optional match
    flag. Flag fields go before other fields in a cache record with the match
    flag field placed always at the very beginning of the record.
  */
  uint flag_fields;

  /* The total number of blob fields that are written into the cache */ 
  uint blobs;

  /* 
    The total number of fields referenced from field descriptors for other join
    caches. These fields are used to construct key values to access matching
    rows with index lookups. Currently the fields can be referenced only from
    descriptors for bka caches. However they may belong to a cache of any type.
  */   
  uint referenced_fields;
   
  /* 
    The current number of already created data field descriptors.
    This number can be useful for implementations of the init methods.  
  */
  uint data_field_count; 

  /* 
    The current number of already created pointers to the data field
    descriptors. This number can be useful for implementations of
    the init methods.  
  */
  uint data_field_ptr_count; 
  /* 
    Array of the descriptors of fields containing 'fields' elements.
    These are all fields that are stored for a record in the cache. 
  */
  CACHE_FIELD *field_descr;

  /* 
    Array of pointers to the blob descriptors that contains 'blobs' elements.
  */
  CACHE_FIELD **blob_ptr;

  /* 
    This flag indicates that records written into the join buffer contain
    a match flag field. The flag must be set by the init method. 
  */
  bool with_match_flag; 
  /*
    This flag indicates that any record is prepended with the length of the
    record which allows us to skip the record or part of it without reading.
  */
  bool with_length;

  /* 
    The maximal number of bytes used for a record representation in
    the cache excluding the space for blob data. 
    For future derived classes this representation may contains some
    redundant info such as a key value associated with the record.     
  */
  uint pack_length;
  /* 
    The value of pack_length incremented by the total size of all 
    pointers of a record in the cache to the blob data. 
  */
  uint pack_length_with_blob_ptrs;

  /* Pointer to the beginning of the join buffer */
  uchar *buff;         
  /* 
    Size of the entire memory allocated for the join buffer.
    Part of this memory may be reserved for the auxiliary buffer.
  */ 
  ulong buff_size;
  /* Size of the auxiliary buffer. */ 
  ulong aux_buff_size;

  /* The number of records put into the join buffer */ 
  uint records;

  /* 
    Pointer to the current position in the join buffer.
    This member is used both when writing to buffer and
    when reading from it.
  */
  uchar *pos;
  /* 
    Pointer to the first free position in the join buffer,
    right after the last record into it.
  */
  uchar *end_pos; 

  /* 
    Pointer to the beginning of first field of the current read/write record
    from the join buffer. The value is adjusted by the get_record/put_record
    functions.
  */
  uchar *curr_rec_pos;
  /* 
    Pointer to the beginning of first field of the last record
    from the join buffer.
  */
  uchar *last_rec_pos;

  /* 
    Flag is set if the blob data for the last record in the join buffer
    is in record buffers rather than in the join cache.
  */
  bool last_rec_blob_data_is_in_rec_buff;

  /* 
    Pointer to the position to the current record link. 
    Record links are used only with linked caches. Record links allow to set
    connections between parts of one join record that are stored in different
    join buffers.
    In the simplest case a record link is just a pointer to the beginning of
    the record stored in the buffer.
    In a more general case a link could be a reference to an array of pointers
    to records in the buffer.   */
  uchar *curr_rec_link;

  /** Cached value of calc_check_only_first_match(join_tab) */
  bool check_only_first_match;

  void calc_record_fields();     
  int alloc_fields(uint external_fields);
  void create_flag_fields();
  void create_remaining_fields(bool all_read_fields);
  void set_constants();
  int alloc_buffer();

  uint get_size_of_rec_offset() { return size_of_rec_ofs; }
  uint get_size_of_rec_length() { return size_of_rec_len; }
  uint get_size_of_fld_offset() { return size_of_fld_ofs; }

  uchar *get_rec_ref(uchar *ptr)
  {
    return buff+get_offset(size_of_rec_ofs, ptr-size_of_rec_ofs);
  }
  ulong get_rec_length(uchar *ptr)
  { 
    return (ulong) get_offset(size_of_rec_len, ptr);
  }
  ulong get_fld_offset(uchar *ptr)
  { 
    return (ulong) get_offset(size_of_fld_ofs, ptr);
  }

  void store_rec_ref(uchar *ptr, uchar* ref)
  {
    store_offset(size_of_rec_ofs, ptr-size_of_rec_ofs, (ulong) (ref-buff));
  }

  void store_rec_length(uchar *ptr, ulong len)
  {
    store_offset(size_of_rec_len, ptr, len);
  }
  void store_fld_offset(uchar *ptr, ulong ofs)
  {
    store_offset(size_of_fld_ofs, ptr, ofs);
  }

  /* Write record fields and their required offsets into the join buffer */ 
  uint write_record_data(uchar *link, bool *is_full);

  /* 
    This method must determine for how much the auxiliary buffer should be
    incremented when a new record is added to the join buffer.
    If no auxiliary buffer is needed the function should return 0.
  */
  virtual uint aux_buffer_incr() { return 0; }

  /* Shall calculate how much space is remaining in the join buffer */ 
  virtual ulong rem_space() 
  { 
    return max(buff_size-(end_pos-buff)-aux_buff_size,0);
  }

  /* Shall skip record from the join buffer if its match flag is on */
  virtual bool skip_record_if_match();

  /*  Read all flag and data fields of a record from the join buffer */
  int read_all_record_fields();
  
  /* Read all flag fields of a record from the join buffer */
  uint read_flag_fields();

  /* Read a data record field from the join buffer */
  uint read_record_field(CACHE_FIELD *copy, bool last_record);

  /* Read a referenced field from the join buffer */
  bool read_referenced_field(CACHE_FIELD *copy, uchar *rec_ptr, uint *len);

  /* 
    True if rec_ptr points to the record whose blob data stay in
    record buffers
  */
  bool blob_data_is_in_rec_buff(uchar *rec_ptr)
  {
    return rec_ptr == last_rec_pos && last_rec_blob_data_is_in_rec_buff;
  }

  /* Find matches from the next table for records from the join buffer */   
  virtual enum_nested_loop_state join_matching_records(bool skip_last)=0;

  /* Add null complements for unmatched outer records from buffer */
  virtual enum_nested_loop_state join_null_complements(bool skip_last);

  /* Restore the fields of the last record from the join buffer */
  virtual void restore_last_record();

  /*Set match flag for a record in join buffer if it has not been set yet */
  bool set_match_flag_if_none(JOIN_TAB *first_inner, uchar *rec_ptr);

  enum_nested_loop_state generate_full_extensions(uchar *rec_ptr);

  /* Check matching to a partial join record from the join buffer */
  virtual bool check_match(uchar *rec_ptr);

  /** @returns whether we should check only the first match for this table */
  bool calc_check_only_first_match(const JOIN_TAB *t) const
  {
    return (t->last_sj_inner_tab == t &&
            t->get_sj_strategy() == SJ_OPT_FIRST_MATCH) ||
      (t->first_inner && t->first_inner->last_inner == t &&
       t->table->reginfo.not_exists_optimize);
  }

public:

  /* Table to be joined with the partial join records from the cache */ 
  JOIN_TAB *join_tab;

  /* Pointer to the previous join cache if there is any */
  JOIN_CACHE *prev_cache;
  /* Pointer to the next join cache if there is any */
  JOIN_CACHE *next_cache;

  /* Shall initialize the join cache structure */ 
  virtual int init()=0;  

  /* The function shall return TRUE only for BKA caches */
  virtual bool is_key_access() { return FALSE; }

  /* Shall reset the join buffer for reading/writing */
  virtual void reset(bool for_writing);

  /* 
    This function shall add a record into the join buffer and return TRUE
    if it has been decided that it should be the last record in the buffer.
  */ 
  virtual bool put_record();

  /* 
    This function shall read the next record into the join buffer and return
    TRUE if there is no more next records.
  */ 
  virtual bool get_record();

  /* 
    This function shall read the record at the position rec_ptr
    in the join buffer
  */ 
  virtual void get_record_by_pos(uchar *rec_ptr);

  /* Shall return the value of the match flag for the positioned record */
  virtual bool get_match_flag_by_pos(uchar *rec_ptr);

  /* Shall return the position of the current record */
  virtual uchar *get_curr_rec() { return curr_rec_pos; }

  /* Shall set the current record link */
  virtual void set_curr_rec_link(uchar *link) { curr_rec_link= link; }

  /* Shall return the current record link */
  virtual uchar *get_curr_rec_link()
  { 
    return (curr_rec_link ? curr_rec_link : get_curr_rec());
  }
     
  /* Join records from the join buffer with records from the next join table */    
  enum_nested_loop_state join_records(bool skip_last);

  virtual ~JOIN_CACHE() {}
  void reset_join(JOIN *j) { join= j; }
  void free()
  { 
    my_free(buff);
    buff= 0;
  }   

  /** Bits describing cache's type @sa check_join_cache_usage() */
  enum {NON_INCREMENTAL_BUFFER= 1,
        ALG_NONE= 0, ALG_BNL= 2, ALG_BKA= 4, ALG_BKA_UNIQUE= 8};

  friend class JOIN_CACHE_BNL;
  friend class JOIN_CACHE_BKA;
  friend class JOIN_CACHE_BKA_UNIQUE;
};

class JOIN_CACHE_BNL :public JOIN_CACHE
{

protected:

  /* Using BNL find matches from the next table for records from join buffer */
  enum_nested_loop_state join_matching_records(bool skip_last);

public:

  /* 
    This constructor creates an unlinked BNL join cache. The cache is to be
    used to join table 'tab' to the result of joining the previous tables 
    specified by the 'j' parameter.
  */   
  JOIN_CACHE_BNL(JOIN *j, JOIN_TAB *tab)
  { 
    join= j;
    join_tab= tab;
    prev_cache= next_cache= 0;
  }

  /* 
    This constructor creates a linked BNL join cache. The cache is to be 
    used to join table 'tab' to the result of joining the previous tables 
    specified by the 'j' parameter. The parameter 'prev' specifies the previous
    cache object to which this cache is linked.
  */   
  JOIN_CACHE_BNL(JOIN *j, JOIN_TAB *tab, JOIN_CACHE *prev)
  { 
    join= j;
    join_tab= tab;
    prev_cache= prev;
    next_cache= 0;
    if (prev)
      prev->next_cache= this;
  }

  /* Initialize the BNL cache */       
  int init();

};

class JOIN_CACHE_BKA :public JOIN_CACHE
{
protected:

  /* Flag to to be passed to the MRR interface */ 
  uint mrr_mode;

  /* MRR buffer assotiated with this join cache */
  HANDLER_BUFFER mrr_buff;

  /* Shall initialize the MRR buffer */
  virtual void init_mrr_buff()
  {
    mrr_buff.buffer= end_pos;
    mrr_buff.buffer_end= buff+buff_size;
  }

  /*
    The number of the cache fields that are used in building keys to access
    the table join_tab
  */
  uint local_key_arg_fields;
  /* 
    The total number of the fields in the previous caches that are used
    in building keys t access the table join_tab
  */
  uint external_key_arg_fields;

  /* 
    This flag indicates that the key values will be read directly from the join
    buffer. It will save us building key values in the key buffer.
  */
  bool use_emb_key;
  /* The length of an embedded key value */ 
  uint emb_key_length;

  /* Check the possibility to read the access keys directly from join buffer */  
  bool check_emb_key_usage();

  /* Calculate the increment of the MM buffer for a record write */
  uint aux_buffer_incr();

  /* Using BKA find matches from the next table for records from join buffer */
  enum_nested_loop_state join_matching_records(bool skip_last);

  /* Prepare to search for records that match records from the join buffer */
  enum_nested_loop_state init_join_matching_records(RANGE_SEQ_IF *seq_funcs,
                                                    uint ranges);

  /* Finish searching for records that match records from the join buffer */
  enum_nested_loop_state end_join_matching_records(enum_nested_loop_state rc);

public:
  
  /* 
    This constructor creates an unlinked BKA join cache. The cache is to be
    used to join table 'tab' to the result of joining the previous tables 
    specified by the 'j' parameter.
    The MRR mode initially is set to 'flags'.
  */   
  JOIN_CACHE_BKA(JOIN *j, JOIN_TAB *tab, uint flags)
  { 
    join= j;
    join_tab= tab;
    prev_cache= next_cache= 0;
    mrr_mode= flags;
  }

  /* 
    This constructor creates a linked BKA join cache. The cache is to be 
    used to join table 'tab' to the result of joining the previous tables 
    specified by the 'j' parameter. The parameter 'prev' specifies the cache
    object to which this cache is linked.
    The MRR mode initially is set to 'flags'.
  */   
  JOIN_CACHE_BKA(JOIN *j, JOIN_TAB *tab, uint flags,  JOIN_CACHE* prev)
  { 
    join= j;
    join_tab= tab;
    prev_cache= prev;
    next_cache= 0;
    if (prev)
      prev->next_cache= this;
    mrr_mode= flags;
  }

  /* Initialize the BKA cache */       
  int init();

  bool is_key_access() { return TRUE; }

  /* Shall get the key built over the next record from the join buffer */
  virtual uint get_next_key(uchar **key);

  /* Check if the record combination matches the index condition */
  bool skip_index_tuple(range_seq_t rseq, char *range_info);
};

/*
  The class JOIN_CACHE_BKA_UNIQUE supports the variant of the BKA join algorithm
  that submits only distinct keys to the MRR interface. The records in the join
  buffer of a cache of this class that have the same access key are linked into
  a chain attached to a key entry structure that either itself contains the key
  value, or, in the case when the keys are embedded, refers to its occurance in
  one of the records from the chain.
  To build the chains with the same keys a hash table is employed. It is placed
  at the very end of the join buffer. The array of hash entries is allocated
  first at the very bottom of the join buffer, then go key entries. A hash entry
  contains a header of the list of the key entries with the same hash value. 
  Each key entry is a structure of the following type:
    struct st_join_cache_key_entry {
      union { 
        uchar[] value;
        cache_ref *value_ref; // offset from the beginning of the buffer
      } hash_table_key;
      key_ref next_key; // offset backward from the beginning of hash table
      cache_ref *last_rec // offset from the beginning of the buffer
    }
  The references linking the records in a chain are always placed at the very
  beginning of the record info stored in the join buffer. The records are 
  linked in a circular list. A new record is always added to the end of this 
  list. When a key is passed to the MRR interface it can be passed either with
  an association link containing a reference to the header of the record chain
  attached to the corresponding key entry in the hash table, or without any
  association link. When the next record is returned by a call to the MRR 
  function multi_range_read_next without any association (because if was not
  passed  together with the key) then the key value is extracted from the
  returned record and searched for it in the hash table. If there is any records
  with such key the chain of them will be yielded as the result of this search.

  The following picture represents a typical layout for the info stored in the
  join buffer of a join cache object of the JOIN_CACHE_BKA_UNIQUE class.
    
  buff
  V
  +----------------------------------------------------------------------------+
  |     |[*]record_1_1|                                                        |
  |     ^ |                                                                    |
  |     | +--------------------------------------------------+                 |
  |     |                           |[*]record_2_1|          |                 |
  |     |                           ^ |                      V                 |
  |     |                           | +------------------+   |[*]record_1_2|   |
  |     |                           +--------------------+-+   |               |
  |+--+ +---------------------+                          | |   +-------------+ |
  ||  |                       |                          V |                 | |
  |||[*]record_3_1|         |[*]record_1_3|              |[*]record_2_2|     | |
  ||^                       ^                            ^                   | |
  ||+----------+            |                            |                   | |
  ||^          |            |<---------------------------+-------------------+ |
  |++          | | ... mrr  |   buffer ...           ... |     |               |
  |            |            |                            |                     |
  |      +-----+--------+   |                      +-----|-------+             |
  |      V     |        |   |                      V     |       |             |
  ||key_3|[/]|[*]|      |   |                |key_2|[/]|[*]|     |             |
  |                   +-+---|-----------------------+            |             |
  |                   V |   |                       |            |             |
  |             |key_1|[*]|[*]|         |   | ... |[*]|   ...  |[*]|  ...  |   |
  +----------------------------------------------------------------------------+
                                        ^           ^            ^
                                        |           i-th entry   j-th entry
                                        hash table

  i-th hash entry:
    circular record chain for key_1:
      record_1_1
      record_1_2
      record_1_3 (points to record_1_1)
    circular record chain for key_3:
      record_3_1 (points to itself)

  j-th hash entry:
    circular record chain for key_2:
      record_2_1
      record_2_2 (points to record_2_1)

*/

class JOIN_CACHE_BKA_UNIQUE :public JOIN_CACHE_BKA
{

private:

  /* Size of the offset of a key entry in the hash table */
  uint size_of_key_ofs;

  /* 
    Length of a key value.
    It is assumed that all key values have the same length.
  */
  uint key_length;
  /* 
    Length of the key entry in the hash table.
    A key entry either contains the key value, or it contains a reference
    to the key value if use_emb_key flag is set for the cache.
  */ 
  uint key_entry_length;
 
  /* The beginning of the hash table in the join buffer */
  uchar *hash_table;
  /* Number of hash entries in the hash table */
  uint hash_entries;

  /* Number of key entries in the hash table (number of distinct keys) */
  uint key_entries;

  /* The position of the last key entry in the hash table */
  uchar *last_key_entry;

  /* The position of the currently retrieved key entry in the hash table */
  uchar *curr_key_entry;

  /* 
    The offset of the record fields from the beginning of the record
    representation. The record representation starts with a reference to
    the next record in the key record chain followed by the length of
    the trailing record data followed by a reference to the record segment
     in the previous cache, if any, followed by the record fields.
  */ 
  uint rec_fields_offset;
  /* The offset of the data fields from the beginning of the record fields */
  uint data_fields_offset;
  
  uint get_hash_idx(uchar* key, uint key_len);

  void cleanup_hash_table();
  
protected:

  uint get_size_of_key_offset() { return size_of_key_ofs; }

  /* 
    Get the position of the next_key_ptr field pointed to by 
    a linking reference stored at the position key_ref_ptr. 
    This reference is actually the offset backward from the
    beginning of hash table.
  */  
  uchar *get_next_key_ref(uchar *key_ref_ptr)
  {
    return hash_table-get_offset(size_of_key_ofs, key_ref_ptr);
  }

  /* 
    Store the linking reference to the next_key_ptr field at 
    the position key_ref_ptr. The position of the next_key_ptr
    field is pointed to by ref. The stored reference is actually
    the offset backward from the beginning of the hash table.
  */  
  void store_next_key_ref(uchar *key_ref_ptr, uchar *ref)
  {
    store_offset(size_of_key_ofs, key_ref_ptr, (ulong) (hash_table-ref));
  }     
  
  /* 
    Check whether the reference to the next_key_ptr field at the position
    key_ref_ptr contains  a nil value.
  */
  bool is_null_key_ref(uchar *key_ref_ptr)
  {
    ulong nil= 0;
    return memcmp(key_ref_ptr, &nil, size_of_key_ofs ) == 0;
  } 

  /* 
    Set the reference to the next_key_ptr field at the position
    key_ref_ptr equal to nil.
  */
  void store_null_key_ref(uchar *key_ref_ptr)
  {
    ulong nil= 0;
    store_offset(size_of_key_ofs, key_ref_ptr, nil);
  } 

  uchar *get_next_rec_ref(uchar *ref_ptr)
  {
    return buff+get_offset(get_size_of_rec_offset(), ref_ptr);
  }

  void store_next_rec_ref(uchar *ref_ptr, uchar *ref)
  {
    store_offset(get_size_of_rec_offset(), ref_ptr, (ulong) (ref-buff));
  }     
 
  /*
    Get the position of the embedded key value for the current
    record pointed to by get_curr_rec().
  */ 
  uchar *get_curr_emb_key()
  {
    return get_curr_rec()+data_fields_offset;
  }

  /*
    Get the position of the embedded key value pointed to by a reference
    stored at ref_ptr. The stored reference is actually the offset from
    the beginning of the join buffer.
  */  
  uchar *get_emb_key(uchar *ref_ptr)
  {
    return buff+get_offset(get_size_of_rec_offset(), ref_ptr);
  }

  /* 
    Store the reference to an embedded key at the position key_ref_ptr.
    The position of the embedded key is pointed to by ref. The stored
    reference is actually the offset from the beginning of the join buffer.
  */  
  void store_emb_key_ref(uchar *ref_ptr, uchar *ref)
  {
    store_offset(get_size_of_rec_offset(), ref_ptr, (ulong) (ref-buff));
  }
  
  /* 
    Calculate how much space in the buffer would not be occupied by
    records, key entries and additional memory for the MMR buffer.
  */ 
  ulong rem_space() 
  { 
    return max(last_key_entry-end_pos-aux_buff_size,0);
  }

  /* 
    Initialize the MRR buffer allocating some space within the join buffer.
    The entire space between the last record put into the join buffer and the
    last key entry added to the hash table is used for the MRR buffer.
  */
  void init_mrr_buff()
  {
    mrr_buff.buffer= end_pos;
    mrr_buff.buffer_end= last_key_entry;
  }

  /* Skip record from JOIN_CACHE_BKA_UNIQUE buffer if its match flag is on */
  bool skip_record_if_match();

  /* Using BKA_UNIQUE find matches for records from join buffer */
  enum_nested_loop_state join_matching_records(bool skip_last);

  /* Search for a key in the hash table of the join buffer */
  bool key_search(uchar *key, uint key_len, uchar **key_ref_ptr);

  virtual bool check_match(uchar *rec_ptr);

public:

  /* 
    This constructor creates an unlinked BKA_UNIQUE join cache. The cache is
    to be used to join table 'tab' to the result of joining the previous tables 
    specified by the 'j' parameter.
    The MRR mode initially is set to 'flags'.
  */   
  JOIN_CACHE_BKA_UNIQUE(JOIN *j, JOIN_TAB *tab, uint flags)
    :JOIN_CACHE_BKA(j, tab, flags) {}

  /* 
    This constructor creates a linked BKA_UNIQUE join cache. The cache is
    to be used to join table 'tab' to the result of joining the previous tables 
    specified by the 'j' parameter. The parameter 'prev' specifies the cache
    object to which this cache is linked.
    The MRR mode initially is set to 'flags'.
  */   
  JOIN_CACHE_BKA_UNIQUE(JOIN *j, JOIN_TAB *tab, uint flags,  JOIN_CACHE* prev)
    :JOIN_CACHE_BKA(j, tab, flags, prev) {}

  /* Initialize the BKA_UNIQUE cache */       
  int init();

  /* Reset the JOIN_CACHE_BKA_UNIQUE  buffer for reading/writing */
  void reset(bool for_writing);

  /* Add a record into the JOIN_CACHE_BKA_UNIQUE buffer */
  bool put_record();

  /* Read the next record from the JOIN_CACHE_BKA_UNIQUE buffer */
  bool get_record();

  /*
    Shall check whether all records in a key chain have 
    their match flags set on
  */   
  virtual bool check_all_match_flags_for_key(uchar *key_chain_ptr);

  uint get_next_key(uchar **key); 
  
  /* Get the head of the record chain attached to the current key entry */ 
  uchar *get_curr_key_chain()
  {
    return get_next_rec_ref(curr_key_entry+key_entry_length-
                            get_size_of_rec_offset());
  }
  
  /* Check if the record combination matches the index condition */
  bool skip_index_tuple(range_seq_t rseq, char *range_info);
};


enum_nested_loop_state sub_select_cache(JOIN *join, JOIN_TAB *join_tab, bool
                                        end_of_records);
enum_nested_loop_state sub_select(JOIN *join,JOIN_TAB *join_tab, bool
                                  end_of_records);
enum_nested_loop_state end_send_group(JOIN *join, JOIN_TAB *join_tab,
                                      bool end_of_records);
enum_nested_loop_state end_write_group(JOIN *join, JOIN_TAB *join_tab,
                                       bool end_of_records);
enum_nested_loop_state sub_select_sjm(JOIN *join, JOIN_TAB *join_tab, 
                                      bool end_of_records);


/**
  A position of table within a join order. This structure is primarily used
  as a part of join->positions and join->best_positions arrays.

  One POSITION element contains information about:
   - Which table is accessed
   - Which access method was chosen
      = Its cost and #of output records
   - Semi-join strategy choice. Note that there are two different
     representation formats:
      1. The one used during join optimization
      2. The one used at plan refinement/code generation stage.
      We call fix_semijoin_strategies_for_picked_join_order() to switch
      between #1 and #2. See that function's comment for more details.

   - Semi-join optimization state. When we're running join optimization, 
     we main a state for every semi-join strategy which are various
     variables that tell us if/at which point we could consider applying the
     strategy.  
     The variables are really a function of join prefix but they are too
     expensive to re-caclulate for every join prefix we consider, so we
     maintain current state in join->positions[#tables_in_prefix]. See
     advance_sj_state() for details.
*/

typedef struct st_position : public Sql_alloc
{
  /*
    The "fanout" -  number of output rows that will be produced (after
    pushed down selection condition is applied) per each row combination of
    previous tables.
  */
  double records_read;

  /* 
    Cost accessing the table in course of the entire complete join execution,
    i.e. cost of one access method use (e.g. 'range' or 'ref' scan ) times 
    number the access method will be invoked.
  */
  double read_time;
  JOIN_TAB *table;

  /*
    NULL  -  'index' or 'range' or 'index_merge' or 'ALL' access is used.
    Other - [eq_]ref[_or_null] access is used. Pointer to {t.keypart1 = expr}
  */
  Key_use *key;

  /* If ref-based access is used: bitmap of tables this table depends on  */
  table_map ref_depend_map;
  bool use_join_buffer; 
  
  
  /* These form a stack of partial join order costs and output sizes */
  COST_VECT prefix_cost;
  double    prefix_record_count;

  /*
    Current optimization state: Semi-join strategy to be used for this
    and preceding join tables.
    
    Join optimizer sets this for the *last* join_tab in the
    duplicate-generating range. That is, in order to interpret this field, 
    one needs to traverse join->[best_]positions array from right to left.
    When you see a join table with sj_strategy!= SJ_OPT_NONE, some other
    field (depending on the strategy) tells how many preceding positions 
    this applies to. The values of covered_preceding_positions->sj_strategy
    must be ignored.
  */
  uint sj_strategy;
  /*
    Valid only after fix_semijoin_strategies_for_picked_join_order() call:
    if sj_strategy!=SJ_OPT_NONE, this is the number of subsequent tables that
    are covered by the specified semi-join strategy
  */
  uint n_sj_tables;

  /**
    Bitmap of semi-join inner tables that are in the join prefix and for
    which there's no provision yet for how to eliminate semi-join duplicates
    which they produce.
  */
  table_map dups_producing_tables;

/* LooseScan strategy members */

  /* The first (i.e. driving) table we're doing loose scan for */
  uint        first_loosescan_table;
  /* 
     Tables that need to be in the prefix before we can calculate the cost
     of using LooseScan strategy.
  */
  table_map   loosescan_need_tables;

  /*
    keyno  -  Planning to do LooseScan on this key. If keyuse is NULL then 
              this is a full index scan, otherwise this is a ref+loosescan
              scan (and keyno matches the KEUSE's)
    MAX_KEY - Not doing a LooseScan
  */
  uint loosescan_key;  // final (one for strategy instance )
  uint loosescan_parts; /* Number of keyparts to be kept distinct */
  
/* FirstMatch strategy */
  /*
    Index of the first inner table that we intend to handle with this
    strategy
  */
  uint first_firstmatch_table;
  /*
    Tables that were not in the join prefix when we've started considering 
    FirstMatch strategy.
  */
  table_map first_firstmatch_rtbl;
  /* 
    Tables that need to be in the prefix before we can calculate the cost
    of using FirstMatch strategy.
   */
  table_map firstmatch_need_tables;

/* Duplicate Weedout strategy */
  /* The first table that the strategy will need to handle */
  uint  first_dupsweedout_table;
  /*
    Tables that we will need to have in the prefix to do the weedout step
    (all inner and all outer that the involved semi-joins are correlated with)
  */
  table_map dupsweedout_tables;

/* SJ-Materialization-Scan strategy */
  /* The last inner table (valid once we're after it) */
  uint      sjm_scan_last_inner;
  /*
    Tables that we need to have in the prefix to calculate the correct cost.
    Basically, we need all inner tables and outer tables mentioned in the
    semi-join's ON expression so we can correctly account for fanout.
  */
  table_map sjm_scan_need_tables;
} POSITION;


typedef struct st_rollup
{
  enum State { STATE_NONE, STATE_INITED, STATE_READY };
  State state;
  Item_null_result **null_items;
  Item ***ref_pointer_arrays;
  List<Item> *fields;
} ROLLUP;


/*
  Temporary table used by semi-join DuplicateElimination strategy

  This consists of the temptable itself and data needed to put records
  into it. The table's DDL is as follows:

    CREATE TABLE tmptable (col VARCHAR(n) BINARY, PRIMARY KEY(col));

  where the primary key can be replaced with unique constraint if n exceeds
  the limit (as it is always done for query execution-time temptables).

  The record value is a concatenation of rowids of tables from the join we're
  executing. If a join table is on the inner side of the outer join, we
  assume that its rowid can be NULL and provide means to store this rowid in
  the tuple.
*/

class SJ_TMP_TABLE : public Sql_alloc
{
public:
  /*
    Array of pointers to tables whose rowids compose the temporary table
    record.
  */
  class TAB
  {
  public:
    JOIN_TAB *join_tab;
    uint rowid_offset;
    ushort null_byte;
    uchar null_bit;
  };
  TAB *tabs;
  TAB *tabs_end;
  
  /* 
    is_confluent==TRUE means this is a special case where the temptable record
    has zero length (and presence of a unique key means that the temptable can
    have either 0 or 1 records). 
    In this case we don't create the physical temptable but instead record
    its state in SJ_TMP_TABLE::have_confluent_record.
  */
  bool is_confluent;

  /* 
    When is_confluent==TRUE: the contents of the table (whether it has the
    record or not).
  */
  bool have_confluent_row;
  
  /* table record parameters */
  uint null_bits;
  uint null_bytes;
  uint rowid_len;

  /* The temporary table itself (NULL means not created yet) */
  TABLE *tmp_table;

  /*
    These are the members we got from temptable creation code. We'll need
    them if we'll need to convert table from HEAP to MyISAM/Maria.
  */
  MI_COLUMNDEF *start_recinfo;
  MI_COLUMNDEF *recinfo;

  /* Pointer to next table (next->start_idx > this->end_idx) */
  SJ_TMP_TABLE *next; 
};


class JOIN :public Sql_alloc
{
  JOIN(const JOIN &rhs);                        /**< not implemented */
  JOIN& operator=(const JOIN &rhs);             /**< not implemented */
public:
  JOIN_TAB *join_tab,**best_ref;
  JOIN_TAB **map2table;    ///< mapping between table indexes and JOIN_TABs
  JOIN_TAB *join_tab_save; ///< saved join_tab for subquery reexecution
  TABLE    **table,**all_tables;
  /*
    The table which has an index that allows to produce the requried ordering.
    A special value of 0x1 means that the ordering will be produced by
    passing 1st non-const table to filesort(). NULL means no such table exists.
  */
  TABLE    *sort_by_table;
  uint	   tables;        /* Number of tables in the join */
  uint     outer_tables;  /* Number of tables that are not inside semijoin */
  uint     const_tables;
  uint	   send_group_parts;
  /**
    Indicates that grouping will be performed on the result set during
    query execution. This field belongs to query execution.

    @see make_group_fields, alloc_group_fields, JOIN::exec
  */
  bool     sort_and_group; 
  bool     first_record,full_join, no_field_update;
  bool	   group;          /**< If query contains GROUP BY clause */
  bool	   do_send_rows;
  table_map const_table_map,found_const_table_map;
  /*
     Bitmap of all inner tables from outer joins
  */
  table_map outer_join;
  /* Number of records produced after join + group operation */
  ha_rows  send_records;
  ha_rows found_records,examined_rows,row_limit, select_limit;
  /**
    Used to fetch no more than given amount of rows per one
    fetch operation of server side cursor.
    The value is checked in end_send and end_send_group in fashion, similar
    to offset_limit_cnt:
      - fetch_limit= HA_POS_ERROR if there is no cursor.
      - when we open a cursor, we set fetch_limit to 0,
      - on each fetch iteration we add num_rows to fetch to fetch_limit
  */
  ha_rows  fetch_limit;
  /* Finally picked QEP. This is result of join optimization */
  POSITION *best_positions;

/******* Join optimization state members start *******/
  /*
    pointer - we're doing optimization for a semi-join materialization nest.
    NULL    - otherwise
  */
  TABLE_LIST *emb_sjm_nest;
  
  /* Current join optimization state */
  POSITION *positions;
  
  /*
    Bitmap of nested joins embedding the position at the end of the current 
    partial join (valid only during join optimizer run).
  */
  nested_join_map cur_embedding_map;
  
  /*
    Bitmap of inner tables of semi-join nests that have a proper subset of
    their tables in the current join prefix. That is, of those semi-join
    nests that have their tables both in and outside of the join prefix.
  */
  table_map cur_sj_inner_tables;

  /* We also maintain a stack of join optimization states in * join->positions[] */
/******* Join optimization state members end *******/


  Next_select_func first_select;
  /*
    The cost of best complete join plan found so far during optimization,
    after optimization phase - cost of picked join order (not taking into
    account the changes made by test_if_skip_sort_order()).
  */
  double   best_read;
  ha_rows  best_rowcount;
  List<Item> *fields;
  List<Cached_item> group_fields, group_fields_cache;
  TABLE    *tmp_table;
  /// used to store 2 possible tmp table of SELECT
  TABLE    *exec_tmp_table1, *exec_tmp_table2;
  THD	   *thd;
  Item_sum  **sum_funcs, ***sum_funcs_end;
  /** second copy of sumfuncs (for queries with 2 temporary tables */
  Item_sum  **sum_funcs2, ***sum_funcs_end2;
  Procedure *procedure;
  ulonglong  select_options;
  select_result *result;
  TMP_TABLE_PARAM tmp_table_param;
  MYSQL_LOCK *lock;
  /// unit structure (with global parameters) for this select
  SELECT_LEX_UNIT *unit;
  /// select that processed
  SELECT_LEX *select_lex;
  /** 
    TRUE <=> optimizer must not mark any table as a constant table.
    This is needed for subqueries in form "a IN (SELECT .. UNION SELECT ..):
    when we optimize the select that reads the results of the union from a
    temporary table, we must not mark the temp. table as constant because
    the number of rows in it may vary from one subquery execution to another.
  */
  bool no_const_tables; 
  
  /**
    Copy of this JOIN to be used with temporary tables.

    tmp_join is used when the JOIN needs to be "reusable" (e.g. in a subquery
    that gets re-executed several times) and we know will use temporary tables
    for materialization. The materialization to a temporary table overwrites the
    JOIN structure to point to the temporary table after the materialization is
    done. This is where tmp_join is used : it's a copy of the JOIN before the
    materialization and is used in restoring before re-execution by overwriting
    the current JOIN structure with the saved copy.
    Because of this we should pay extra care of not freeing up helper structures
    that are referenced by the original contents of the JOIN. We can check for
    this by making sure the "current" join is not the temporary copy, e.g.
    !tmp_join || tmp_join != join
 
    We should free these sub-structures at JOIN::destroy() if the "current" join
    has a copy is not that copy.
  */
  JOIN *tmp_join;
  ROLLUP rollup;				///< Used with rollup

  bool select_distinct;				///< Set if SELECT DISTINCT
  /**
    If we have the GROUP BY statement in the query,
    but the group_list was emptied by optimizer, this
    flag is TRUE.
    It happens when fields in the GROUP BY are from
    constant table
  */
  bool group_optimized_away;

  /*
    simple_xxxxx is set if ORDER/GROUP BY doesn't include any references
    to other tables than the first non-constant table in the JOIN.
    It's also set if ORDER/GROUP BY is empty.
    Used for deciding for or against using a temporary table to compute 
    GROUP/ORDER BY.
  */
  bool simple_order, simple_group;
  /**
    Is set only in case if we have a GROUP BY clause
    and no ORDER BY after constant elimination of 'order'.
  */
  bool no_order;
  /** Is set if we have a GROUP BY and we have ORDER BY on a constant. */
  bool          skip_sort_order;

  bool need_tmp, hidden_group_fields;
  DYNAMIC_ARRAY keyuse;
  List<Item> all_fields; ///< to store all fields that used in query
  ///Above list changed to use temporary table
  List<Item> tmp_all_fields1, tmp_all_fields2, tmp_all_fields3;
  ///Part, shared with list above, emulate following list
  List<Item> tmp_fields_list1, tmp_fields_list2, tmp_fields_list3;
  List<Item> &fields_list; ///< hold field list passed to mysql_select
  List<Item> procedure_fields_list;
  int error;

  ORDER *order, *group_list, *proc_param; //hold parameters of mysql_select
  /** 
    JOIN::having is initially equal to select_lex->having, but may
    later be changed by optimizations performed by JOIN.
    The relationship between the JOIN::having condition and the
    associated variable select_lex->having_value is so that
    having_value can be:
     - COND_UNDEF if a having clause was not specified in the query or
       if it has not been optimized yet
     - COND_TRUE if the having clause is always true, in which case
       JOIN::having is set to NULL.
     - COND_FALSE if the having clause is impossible, in which case
       JOIN::having is set to NULL
     - COND_OK otherwise, meaning that the having clause needs to be
       further evaluated
    All of the above also applies to the conds/select_lex->cond_value
    pair.
  */
  Item       *conds;                      ///< The where clause item tree
  Item       *having;                     ///< The having clause item tree
  Item       *conds_history;              ///< store WHERE for explain
  Item       *having_history;             ///< Store having for explain
  Item       *tmp_having; ///< To store having when processed temporary table
  TABLE_LIST *tables_list;           ///<hold 'tables' parameter of mysql_select
  List<TABLE_LIST> *join_list;       ///< list of joined tables in reverse order
  COND_EQUAL *cond_equal;
  /*
    Join tab to return to. Points to an element of join->join_tab array, or to
    join->join_tab[-1].
    This is used at execution stage to shortcut join enumeration. Currently
    shortcutting is done to handle outer joins or handle semi-joins with
    FirstMatch strategy.
  */
  JOIN_TAB *return_tab;
  Item **ref_pointer_array; ///<used pointer reference for this select
  // Copy of above to be used with different lists
  Item **items0, **items1, **items2, **items3, **current_ref_pointer_array;
  uint ref_pointer_array_size; ///< size of above in bytes
  const char *zero_result_cause; ///< not 0 if exec must return zero result
  
  bool union_part; ///< this subselect is part of union 
  bool optimized; ///< flag to avoid double optimization in EXPLAIN
  
  Array<Item_exists_subselect> sj_subselects;

  /* Temporary tables used to weed-out semi-join duplicates */
  List<TABLE> sj_tmp_tables;
  List<Semijoin_mat_exec> sjm_exec_list;

  /* 
    storage for caching buffers allocated during query execution. 
    These buffers allocations need to be cached as the thread memory pool is
    cleared only at the end of the execution of the whole query and not caching
    allocations that occur in repetition at execution time will result in 
    excessive memory usage.
    Note: make_simple_join always creates an execution plan that accesses
    a single table, thus it is sufficient to have a one-element array for
    table_reexec.
  */  
  SORT_FIELD *sortorder;                        // make_unireg_sortorder()
  TABLE *table_reexec[1];                       // make_simple_join()
  JOIN_TAB *join_tab_reexec;                    // make_simple_join()
  /* end of allocation caching storage */
  /**
    The set of those tables whose fields are referenced in the select list of
    this select level.
  */
  table_map used_tables;

  JOIN(THD *thd_arg, List<Item> &fields_arg, ulonglong select_options_arg,
       select_result *result_arg)
    :fields_list(fields_arg), sj_subselects(thd_arg->mem_root, 4)
  {
    init(thd_arg, fields_arg, select_options_arg, result_arg);
  }

  void init(THD *thd_arg, List<Item> &fields_arg, ulonglong select_options_arg,
       select_result *result_arg)
  {
    join_tab= join_tab_save= 0;
    all_tables= 0;
    tables= 0;
    const_tables= 0;
    join_list= 0;
    implicit_grouping= FALSE;
    sort_and_group= 0;
    first_record= 0;
    do_send_rows= 1;
    send_records= 0;
    found_records= 0;
    fetch_limit= HA_POS_ERROR;
    examined_rows= 0;
    exec_tmp_table1= 0;
    exec_tmp_table2= 0;
    sortorder= 0;
    table_reexec[0]= 0;
    join_tab_reexec= 0;
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
    error= 0;
    return_tab= 0;
    ref_pointer_array= items0= items1= items2= items3= 0;
    ref_pointer_array_size= 0;
    zero_result_cause= 0;
    optimized= 0;
    cond_equal= 0;
    group_optimized_away= 0;
    used_tables= 0;

    all_fields= fields_arg;
    if (&fields_list != &fields_arg)      /* Avoid valgrind-warning */
      fields_list= fields_arg;
    bzero((char*) &keyuse,sizeof(keyuse));
    tmp_table_param.init();
    tmp_table_param.end_write_records= HA_POS_ERROR;
    rollup.state= ROLLUP::STATE_NONE;

    no_const_tables= FALSE;
    /* can help debugging (makes smaller test cases): */
    DBUG_EXECUTE_IF("no_const_tables",no_const_tables= TRUE;);
    first_select= sub_select;
  }

  int prepare(Item ***rref_pointer_array, TABLE_LIST *tables, uint wind_num,
	      Item *conds, uint og_num, ORDER *order, ORDER *group,
	      Item *having, ORDER *proc_param, SELECT_LEX *select,
	      SELECT_LEX_UNIT *unit);
  int optimize();
  int reinit();
  void exec();
  int destroy();
  void restore_tmp();
  bool alloc_func_list();
  bool flatten_subqueries();
  bool setup_subquery_materialization();
  bool make_sum_func_list(List<Item> &all_fields, List<Item> &send_fields,
			  bool before_group_by, bool recompute= FALSE);

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
  bool rollup_process_const_fields();
  bool rollup_make_fields(List<Item> &all_fields, List<Item> &fields,
			  Item_sum ***func);
  int rollup_send_data(uint idx);
  int rollup_write_data(uint idx, TABLE *table);
  void remove_subq_pushed_predicates(Item **where);
  /**
    Release memory and, if possible, the open tables held by this execution
    plan (and nested plans). It's used to release some tables before
    the end of execution in order to increase concurrency and reduce
    memory consumption.
  */
  void join_free();
  /** Cleanup this JOIN, possibly for reuse */
  void cleanup(bool full);
  void clear();
  bool save_join_tab();
  bool init_save_join_tab();
  bool send_row_on_empty_set()
  {
    return (do_send_rows && tmp_table_param.sum_func_count != 0 &&
	    !group_list && select_lex->having_value != Item::COND_FALSE);
  }
  bool change_result(select_result *result);
  bool is_top_level_join() const
  {
    return (unit == &thd->lex->unit && (unit->fake_select_lex == 0 ||
                                        select_lex == unit->fake_select_lex));
  }
  void cache_const_exprs();
  void drop_unused_derived_keys();
  /* 
    Return the table for which an index scan can be used to satisfy 
    the sort order needed by the ORDER BY/(implicit) GROUP BY clause 
  */
  JOIN_TAB *get_sort_by_join_tab()
  {
    return (!sort_by_table || skip_sort_order ||
            ((group || tmp_table_param.sum_func_count) && !group_list)) ?
              NULL : join_tab+const_tables;
  }
private:
  /**
    TRUE if the query contains an aggregate function but has no GROUP
    BY clause. 
  */
  bool implicit_grouping; 
  bool make_simple_join(JOIN *join, TABLE *tmp_table);
  void cleanup_item_list(List<Item> &items) const;
};


typedef struct st_select_check {
  uint const_ref,reg_ref;
} SELECT_CHECK;

extern const char *join_type_str[];

/* Extern functions in sql_select.cc */
bool store_val_in_field(Field *field, Item *val, enum_check_fields check_flag);
TABLE *create_tmp_table(THD *thd,TMP_TABLE_PARAM *param,List<Item> &fields,
			ORDER *group, bool distinct, bool save_sum_fields,
			ulonglong select_options, ha_rows rows_limit,
			const char* alias);
void free_tmp_table(THD *thd, TABLE *entry);
void count_field_types(SELECT_LEX *select_lex, TMP_TABLE_PARAM *param, 
                       List<Item> &fields, bool reset_with_sum_func);
bool setup_copy_fields(THD *thd, TMP_TABLE_PARAM *param,
		       Item **ref_pointer_array,
		       List<Item> &new_list1, List<Item> &new_list2,
		       uint elements, List<Item> &fields);
void copy_fields(TMP_TABLE_PARAM *param);
bool copy_funcs(Item **func_ptr, const THD *thd);
bool create_myisam_from_heap(THD *thd, TABLE *table,
                             MI_COLUMNDEF *start_recinfo,
                             MI_COLUMNDEF **recinfo, 
                             int error, bool ignore_last_dup,
                             bool *is_duplicate);
uint find_shortest_key(TABLE *table, const key_map *usable_keys);
Field* create_tmp_field_from_field(THD *thd, Field* org_field,
                                   const char *name, TABLE *table,
                                   Item_field *item, uint convert_blob_length);
                                                                      
bool is_indexed_agg_distinct(JOIN *join, List<Item_field> *out_args);

/* functions from opt_sum.cc */
bool simple_pred(Item_func *func_item, Item **args, bool *inv_order);
int opt_sum_query(TABLE_LIST *tables, List<Item> &all_fields,Item *conds);

/* from sql_delete.cc, used by opt_range.cc */
extern "C" int refpos_order_cmp(void* arg, const void *a,const void *b);

/** class to copying an field/item to a key struct */

class store_key :public Sql_alloc
{
public:
  bool null_key; /* TRUE <=> the value of the key has a null part */
  enum store_key_result { STORE_KEY_OK, STORE_KEY_FATAL, STORE_KEY_CONV };
  store_key(THD *thd, Field *field_arg, uchar *ptr, uchar *null, uint length)
    :null_key(0), null_ptr(null), err(0)
  {
    if (field_arg->type() == MYSQL_TYPE_BLOB
        || field_arg->type() == MYSQL_TYPE_GEOMETRY)
    {
      /* 
        Key segments are always packed with a 2 byte length prefix.
        See mi_rkey for details.
      */
      to_field= new Field_varstring(ptr, length, 2, null, 1, 
                                    Field::NONE, field_arg->field_name,
                                    field_arg->table->s, field_arg->charset());
      to_field->init(field_arg->table);
    }
    else
      to_field=field_arg->new_key_field(thd->mem_root, field_arg->table,
                                        ptr, null, 1);
  }
  virtual ~store_key() {}			/** Not actually needed */
  virtual const char *name() const=0;

  /**
    @brief sets ignore truncation warnings mode and calls the real copy method

    @details this function makes sure truncation warnings when preparing the
    key buffers don't end up as errors (because of an enclosing INSERT/UPDATE).
  */
  enum store_key_result copy()
  {
    enum store_key_result result;
    THD *thd= to_field->table->in_use;
    enum_check_fields saved_count_cuted_fields= thd->count_cuted_fields;
    sql_mode_t sql_mode= thd->variables.sql_mode;
    thd->variables.sql_mode&= ~(MODE_NO_ZERO_IN_DATE | MODE_NO_ZERO_DATE);

    thd->count_cuted_fields= CHECK_FIELD_IGNORE;

    result= copy_inner();

    thd->count_cuted_fields= saved_count_cuted_fields;
    thd->variables.sql_mode= sql_mode;

    return result;
  }

 protected:
  Field *to_field;				// Store data here
  uchar *null_ptr;
  uchar err;

  virtual enum store_key_result copy_inner()=0;
};


class store_key_field: public store_key
{
  Copy_field copy_field;
  const char *field_name;
 public:
  store_key_field(THD *thd, Field *to_field_arg, uchar *ptr,
                  uchar *null_ptr_arg,
		  uint length, Field *from_field, const char *name_arg)
    :store_key(thd, to_field_arg,ptr,
	       null_ptr_arg ? null_ptr_arg : from_field->maybe_null() ? &err
	       : (uchar*) 0, length), field_name(name_arg)
  {
    if (to_field)
    {
      copy_field.set(to_field,from_field,0);
    }
  }
  const char *name() const { return field_name; }

 protected: 
  enum store_key_result copy_inner()
  {
    TABLE *table= copy_field.to_field->table;
    my_bitmap_map *old_map= dbug_tmp_use_all_columns(table,
                                                     table->write_set);
    copy_field.do_copy(&copy_field);
    dbug_tmp_restore_column_map(table->write_set, old_map);
    null_key= to_field->is_null();
    return err != 0 ? STORE_KEY_FATAL : STORE_KEY_OK;
  }
};


class store_key_item :public store_key
{
 protected:
  Item *item;
public:
  store_key_item(THD *thd, Field *to_field_arg, uchar *ptr,
                 uchar *null_ptr_arg, uint length, Item *item_arg)
    :store_key(thd, to_field_arg, ptr,
	       null_ptr_arg ? null_ptr_arg : item_arg->maybe_null ?
	       &err : (uchar*) 0, length), item(item_arg)
  {}
  const char *name() const { return "func"; }

 protected:  
  enum store_key_result copy_inner()
  {
    TABLE *table= to_field->table;
    my_bitmap_map *old_map= dbug_tmp_use_all_columns(table,
                                                     table->write_set);
    int res= item->save_in_field(to_field, 1);
    /*
     Item::save_in_field() may call Item::val_xxx(). And if this is a subquery
     we need to check for errors executing it and react accordingly
    */
    if (!res && table->in_use->is_error())
      res= 1; /* STORE_KEY_FATAL */
    dbug_tmp_restore_column_map(table->write_set, old_map);
    null_key= to_field->is_null() || item->null_value;
    return ((err != 0 || res < 0 || res > 2) ? STORE_KEY_FATAL : 
            (store_key_result) res);
  }
};


class store_key_const_item :public store_key_item
{
  bool inited;
public:
  store_key_const_item(THD *thd, Field *to_field_arg, uchar *ptr,
		       uchar *null_ptr_arg, uint length,
		       Item *item_arg)
    :store_key_item(thd, to_field_arg,ptr,
		    null_ptr_arg ? null_ptr_arg : item_arg->maybe_null ?
		    &err : (uchar*) 0, length, item_arg), inited(0)
  {
  }
  const char *name() const { return "const"; }

protected:  
  enum store_key_result copy_inner()
  {
    int res;
    if (!inited)
    {
      inited=1;
      TABLE *table= to_field->table;
      my_bitmap_map *old_map= dbug_tmp_use_all_columns(table,
                                                       table->write_set);
      if ((res= item->save_in_field(to_field, 1)))
      {       
        if (!err)
          err= res < 0 ? 1 : res; /* 1=STORE_KEY_FATAL */
      }
      /*
        Item::save_in_field() may call Item::val_xxx(). And if this is a subquery
        we need to check for errors executing it and react accordingly
        */
      if (!err && to_field->table->in_use->is_error())
        err= 1; /* STORE_KEY_FATAL */
      dbug_tmp_restore_column_map(table->write_set, old_map);
    }
    null_key= to_field->is_null() || item->null_value;
    return (err > 2 ? STORE_KEY_FATAL : (store_key_result) err);
  }
};

bool cp_buffer_from_ref(THD *thd, TABLE *table, TABLE_REF *ref);
bool error_if_full_join(JOIN *join);
int report_error(TABLE *table, int error);
int safe_index_read(JOIN_TAB *tab);
Item *remove_eq_conds(THD *thd, Item *cond, Item::cond_result *cond_value);
int get_quick_record(SQL_SELECT *select);
SORT_FIELD * make_unireg_sortorder(ORDER *order, uint *length,
                                  SORT_FIELD *sortorder);
int setup_order(THD *thd, Item **ref_pointer_array, TABLE_LIST *tables,
		List<Item> &fields, List <Item> &all_fields, ORDER *order);
int setup_group(THD *thd, Item **ref_pointer_array, TABLE_LIST *tables,
		List<Item> &fields, List<Item> &all_fields, ORDER *order,
		bool *hidden_group_fields);
bool fix_inner_refs(THD *thd, List<Item> &all_fields, SELECT_LEX *select,
                   Item **ref_pointer_array, ORDER *group_list= NULL);

bool handle_select(THD *thd, LEX *lex, select_result *result,
                   ulong setup_tables_done_option);
bool mysql_select(THD *thd, Item ***rref_pointer_array,
                  TABLE_LIST *tables, uint wild_num,  List<Item> &list,
                  Item *conds, uint og_num, ORDER *order, ORDER *group,
                  Item *having, ORDER *proc_param, ulonglong select_type, 
                  select_result *result, SELECT_LEX_UNIT *unit, 
                  SELECT_LEX *select_lex);
void free_underlaid_joins(THD *thd, SELECT_LEX *select);
bool mysql_explain_union(THD *thd, SELECT_LEX_UNIT *unit,
                         select_result *result);
Field *create_tmp_field(THD *thd, TABLE *table,Item *item, Item::Type type,
			Item ***copy_func, Field **from_field,
                        Field **def_field,
			bool group, bool modify_item,
			bool table_cant_handle_bit_fields,
                        bool make_copy_field,
                        uint convert_blob_length);

/*
  General routine to change field->ptr of a NULL-terminated array of Field
  objects. Useful when needed to call val_int, val_str or similar and the
  field data is not in table->record[0] but in some other structure.
  set_key_field_ptr changes all fields of an index using a key_info object.
  All methods presume that there is at least one field to change.
*/

TABLE *create_virtual_tmp_table(THD *thd, List<Create_field> &field_list);


int test_if_item_cache_changed(List<Cached_item> &list);
void calc_used_field_length(THD *thd, JOIN_TAB *join_tab);
int join_init_read_record(JOIN_TAB *tab);
int do_sj_dups_weedout(THD *thd, SJ_TMP_TABLE *sjtbl); 
inline bool optimizer_flag(THD *thd, uint flag)
{ 
  return (thd->variables.optimizer_switch & flag);
}

uint get_index_for_order(ORDER *order, TABLE *table, SQL_SELECT *select,
                         ha_rows limit, bool *need_sort, bool *reverse);
ORDER *simple_remove_const(ORDER *order, Item *where);
bool const_expression_in_where(Item *cond, Item *comp_item,
                               Field *comp_field= NULL,
                               Item **const_item= NULL);
bool open_tmp_table(TABLE *table);
bool create_myisam_tmp_table(TABLE *table, KEY *keyinfo,
                             MI_COLUMNDEF *start_recinfo,
                             MI_COLUMNDEF **recinfo,
                             ulonglong options, my_bool big_tables);
#endif /* SQL_SELECT_INCLUDED */
