/*
   Copyright (c) 2000, 2010, Oracle and/or its affiliates.

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


/* classes to use when handling where clause */

#ifndef _opt_range_h
#define _opt_range_h

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

#include "thr_malloc.h"                         /* sql_memdup */
#include "records.h"                            /* READ_RECORD */
#include "queues.h"                             /* QUEUE */
/*
  It is necessary to include set_var.h instead of item.h because there
  are dependencies on include order for set_var.h and item.h. This
  will be resolved later.
*/
#include "sql_class.h"                          // set_var.h: THD
#include "set_var.h"                            /* Item */

class JOIN;
class Item_sum;

typedef struct st_key_part {
  uint16           key,part;
  /* See KEY_PART_INFO for meaning of the next two: */
  uint16           store_length, length;
  uint8            null_bit;
  /*
    Keypart flags (0 when this structure is used by partition pruning code
    for fake partitioning index description)
  */
  uint8 flag;
  Field            *field;
  Field::imagetype image_type;
} KEY_PART;


/*
  A "MIN_TUPLE < tbl.key_tuple < MAX_TUPLE" interval. 
  
  One of endpoints may be absent. 'flags' member has flags which tell whether
  the endpoints are '<' or '<='.
*/
class QUICK_RANGE :public Sql_alloc {
 public:
  uchar *min_key,*max_key;
  uint16 min_length,max_length,flag;
  key_part_map min_keypart_map, // bitmap of used keyparts in min_key
               max_keypart_map; // bitmap of used keyparts in max_key
#ifdef HAVE_valgrind
  uint16 dummy;					/* Avoid warnings on 'flag' */
#endif
  QUICK_RANGE();				/* Full range */
  QUICK_RANGE(const uchar *min_key_arg, uint min_length_arg,
              key_part_map min_keypart_map_arg,
	      const uchar *max_key_arg, uint max_length_arg,
              key_part_map max_keypart_map_arg,
	      uint flag_arg)
    : min_key((uchar*) sql_memdup(min_key_arg,min_length_arg+1)),
      max_key((uchar*) sql_memdup(max_key_arg,max_length_arg+1)),
      min_length((uint16) min_length_arg),
      max_length((uint16) max_length_arg),
      flag((uint16) flag_arg),
      min_keypart_map(min_keypart_map_arg),
      max_keypart_map(max_keypart_map_arg)
    {
#ifdef HAVE_valgrind
      dummy=0;
#endif
    }

  /**
     Initalizes a key_range object for communication with storage engine. 

     This function facilitates communication with the Storage Engine API by
     translating the minimum endpoint of the interval represented by this
     QUICK_RANGE into an index range endpoint specifier for the engine.

     @param Pointer to an uninitialized key_range C struct.

     @param prefix_length The length of the search key prefix to be used for
     lookup.
     
     @param keypart_map A set (bitmap) of keyparts to be used.
  */
  void make_min_endpoint(key_range *kr, uint prefix_length, 
                         key_part_map keypart_map) {
    make_min_endpoint(kr);
    kr->length= min(kr->length, prefix_length);
    kr->keypart_map&= keypart_map;
  }
  
  /**
     Initalizes a key_range object for communication with storage engine. 

     This function facilitates communication with the Storage Engine API by
     translating the minimum endpoint of the interval represented by this
     QUICK_RANGE into an index range endpoint specifier for the engine.

     @param Pointer to an uninitialized key_range C struct.
  */
  void make_min_endpoint(key_range *kr) {
    kr->key= (const uchar*)min_key;
    kr->length= min_length;
    kr->keypart_map= min_keypart_map;
    kr->flag= ((flag & NEAR_MIN) ? HA_READ_AFTER_KEY :
               (flag & EQ_RANGE) ? HA_READ_KEY_EXACT : HA_READ_KEY_OR_NEXT);
  }

  /**
     Initalizes a key_range object for communication with storage engine. 

     This function facilitates communication with the Storage Engine API by
     translating the maximum endpoint of the interval represented by this
     QUICK_RANGE into an index range endpoint specifier for the engine.

     @param Pointer to an uninitialized key_range C struct.

     @param prefix_length The length of the search key prefix to be used for
     lookup.
     
     @param keypart_map A set (bitmap) of keyparts to be used.
  */
  void make_max_endpoint(key_range *kr, uint prefix_length, 
                         key_part_map keypart_map) {
    make_max_endpoint(kr);
    kr->length= min(kr->length, prefix_length);
    kr->keypart_map&= keypart_map;
  }

  /**
     Initalizes a key_range object for communication with storage engine. 

     This function facilitates communication with the Storage Engine API by
     translating the maximum endpoint of the interval represented by this
     QUICK_RANGE into an index range endpoint specifier for the engine.

     @param Pointer to an uninitialized key_range C struct.
  */
  void make_max_endpoint(key_range *kr) {
    kr->key= (const uchar*)max_key;
    kr->length= max_length;
    kr->keypart_map= max_keypart_map;
    /*
      We use READ_AFTER_KEY here because if we are reading on a key
      prefix we want to find all keys with this prefix
    */
    kr->flag= (flag & NEAR_MAX ? HA_READ_BEFORE_KEY : HA_READ_AFTER_KEY);
  }
};


/*
  Quick select interface.
  This class is a parent for all QUICK_*_SELECT and FT_SELECT classes.

  The usage scenario is as follows:
  1. Create quick select
    quick= new QUICK_XXX_SELECT(...);

  2. Perform lightweight initialization. This can be done in 2 ways:
  2.a: Regular initialization
    if (quick->init())
    {
      //the only valid action after failed init() call is delete
      delete quick;
    }
  2.b: Special initialization for quick selects merged by QUICK_ROR_*_SELECT
    if (quick->init_ror_merged_scan())
      delete quick;

  3. Perform zero, one, or more scans.
    while (...)
    {
      // initialize quick select for scan. This may allocate
      // buffers and/or prefetch rows.
      if (quick->reset())
      {
        //the only valid action after failed reset() call is delete
        delete quick;
        //abort query
      }

      // perform the scan
      do
      {
        res= quick->get_next();
      } while (res && ...)
    }

  4. Delete the select:
    delete quick;
  
  NOTE 
    quick select doesn't use Sql_alloc/MEM_ROOT allocation because "range
    checked for each record" functionality may create/destroy
    O(#records_in_some_table) quick selects during query execution.
*/

class QUICK_SELECT_I
{
public:
  ha_rows records;  /* estimate of # of records to be retrieved */
  double  read_time; /* time to perform this retrieval          */
  TABLE   *head;
  /*
    Index this quick select uses, or MAX_KEY for quick selects
    that use several indexes
  */
  uint index;

  /*
    Total length of first used_key_parts parts of the key.
    Applicable if index!= MAX_KEY.
  */
  uint max_used_key_length;

  /*
    Max. number of (first) key parts this quick select uses for retrieval.
    eg. for "(key1p1=c1 AND key1p2=c2) OR key1p1=c2" used_key_parts == 2.
    Applicable if index!= MAX_KEY.

    For QUICK_GROUP_MIN_MAX_SELECT it includes MIN/MAX argument keyparts.
  */
  uint used_key_parts;

  QUICK_SELECT_I();
  virtual ~QUICK_SELECT_I(){};

  /*
    Do post-constructor initialization.
    SYNOPSIS
      init()

    init() performs initializations that should have been in constructor if
    it was possible to return errors from constructors. The join optimizer may
    create and then delete quick selects without retrieving any rows so init()
    must not contain any IO or CPU intensive code.

    If init() call fails the only valid action is to delete this quick select,
    reset() and get_next() must not be called.

    RETURN
      0      OK
      other  Error code
  */
  virtual int  init() = 0;

  /*
    Initialize quick select for row retrieval.
    SYNOPSIS
      reset()

    reset() should be called when it is certain that row retrieval will be
    necessary. This call may do heavyweight initialization like buffering first
    N records etc. If reset() call fails get_next() must not be called.
    Note that reset() may be called several times if 
     * the quick select is executed in a subselect
     * a JOIN buffer is used
    
    RETURN
      0      OK
      other  Error code
  */
  virtual int  reset(void) = 0;

  virtual int  get_next() = 0;   /* get next record to retrieve */

  /* Range end should be called when we have looped over the whole index */
  virtual void range_end() {}

  virtual bool reverse_sorted() = 0;
  virtual bool unique_key_range() { return false; }

  /*
    Request that this quick select produces sorted output. Not all quick
    selects can do it, the caller is responsible for calling this function
    only for those quick selects that can.
  */
  virtual void need_sorted_output() = 0;
  enum {
    QS_TYPE_RANGE = 0,
    QS_TYPE_INDEX_INTERSECT = 1,
    QS_TYPE_INDEX_MERGE = 2,
    QS_TYPE_RANGE_DESC = 3,
    QS_TYPE_FULLTEXT   = 4,
    QS_TYPE_ROR_INTERSECT = 5,
    QS_TYPE_ROR_UNION = 6,
    QS_TYPE_GROUP_MIN_MAX = 7
  };

  /* Get type of this quick select - one of the QS_TYPE_* values */
  virtual int get_type() = 0;

  /*
    Initialize this quick select as a merged scan inside a ROR-union or a ROR-
    intersection scan. The caller must not additionally call init() if this
    function is called.
    SYNOPSIS
      init_ror_merged_scan()
        reuse_handler  If true, the quick select may use table->handler,
                       otherwise it must create and use a separate handler
                       object.
    RETURN
      0     Ok
      other Error
  */
  virtual int init_ror_merged_scan(bool reuse_handler, MEM_ROOT *alloc)
  { DBUG_ASSERT(0); return 1; }

  /*
    Save ROWID of last retrieved row in file->ref. This used in ROR-merging.
  */
  virtual void save_last_pos(){};
  
  void add_key_and_length(String *key_names,
                          String *used_lengths,
                          bool *first);

  /*
    Append comma-separated list of keys this quick select uses to key_names;
    append comma-separated list of corresponding used lengths to used_lengths.
    This is used by select_describe.
  */
  virtual void add_keys_and_lengths(String *key_names,
                                    String *used_lengths)=0;

  void add_key_name(String *str, bool *first);

  /*
    Append text representation of quick select structure (what and how is
    merged) to str. The result is added to "Extra" field in EXPLAIN output.
    This function is implemented only by quick selects that merge other quick
    selects output and/or can produce output suitable for merging.
  */
  virtual void add_info_string(String *str) {}

  /*
    Return 1 if any index used by this quick select
    uses field which is marked in passed bitmap.
  */
  virtual bool is_keys_used(const MY_BITMAP *fields);

  /**
    Simple sanity check that the quick select has been set up
    correctly. Function is overridden by quick selects that merge
    indices.
   */
  virtual bool is_valid() { return index != MAX_KEY; };

  /*
    rowid of last row retrieved by this quick select. This is used only when
    doing ROR-index_merge selects
  */
  uchar    *last_rowid;

  /*
    Table record buffer used by this quick select.
  */
  uchar    *record;

  virtual void replace_handler(handler *new_file)
  {
    DBUG_ASSERT(0); /* Only supported in QUICK_RANGE_SELECT */
  }

#ifndef DBUG_OFF
  /*
    Print quick select information to DBUG_FILE. Caller is responsible
    for locking DBUG_FILE before this call and unlocking it afterwards.
  */
  virtual void dbug_dump(int indent, bool verbose)= 0;
#endif

  /*
    Returns a QUICK_SELECT with reverse order of to the index.
  */
  virtual QUICK_SELECT_I *make_reverse(uint used_key_parts_arg) { return NULL; }
};


struct st_qsel_param;
class PARAM;
class SEL_ARG;


/*
  MRR range sequence, array<QUICK_RANGE> implementation: sequence traversal
  context.
*/
typedef struct st_quick_range_seq_ctx
{
  QUICK_RANGE **first;
  QUICK_RANGE **cur;
  QUICK_RANGE **last;
} QUICK_RANGE_SEQ_CTX;

range_seq_t quick_range_seq_init(void *init_param, uint n_ranges, uint flags);
bool quick_range_seq_next(range_seq_t rseq, KEY_MULTI_RANGE *range);


/*
  Quick select that does a range scan on a single key. The records are
  returned in key order.
*/
class QUICK_RANGE_SELECT : public QUICK_SELECT_I
{
protected:
  /* true if we enabled key only reads */
  bool doing_key_read;
  handler *file;

  /* Members to deal with case when this quick select is a ROR-merged scan */
  bool in_ror_merged_scan;
  MY_BITMAP column_bitmap;
  bool free_file;   /* TRUE <=> this->file is "owned" by this quick select */

  /* Range pointers to be used when not using MRR interface */
  /* Members needed to use the MRR interface */
  QUICK_RANGE_SEQ_CTX qr_traversal_ctx;
public:
  uint mrr_flags; /* Flags to be used with MRR interface */
protected:
  uint mrr_buf_size; /* copy from thd->variables.mrr_buff_size */  
  HANDLER_BUFFER *mrr_buf_desc; /* the handler buffer */

  /* Info about index we're scanning */
  
  DYNAMIC_ARRAY ranges;     /* ordered array of range ptrs */
  QUICK_RANGE **cur_range;  /* current element in ranges  */
  
  QUICK_RANGE *last_range;
  
  KEY_PART *key_parts;
  KEY_PART_INFO *key_part_info;
  
  bool dont_free; /* Used by QUICK_SELECT_DESC */

  int cmp_next(QUICK_RANGE *range);
  int cmp_prev(QUICK_RANGE *range);
  bool row_in_ranges();
public:
  MEM_ROOT alloc;

  QUICK_RANGE_SELECT(THD *thd, TABLE *table,uint index_arg,bool no_alloc,
                     MEM_ROOT *parent_alloc, bool *create_err);
  ~QUICK_RANGE_SELECT();
  
  void need_sorted_output();
  int init();
  int reset(void);
  int get_next();
  void range_end();
  int get_next_prefix(uint prefix_length, uint group_key_parts, 
                      uchar *cur_prefix);
  bool reverse_sorted() { return 0; }
  bool unique_key_range();
  int init_ror_merged_scan(bool reuse_handler, MEM_ROOT *alloc);
  void save_last_pos()
  { file->position(record); }
  int get_type() { return QS_TYPE_RANGE; }
  void add_keys_and_lengths(String *key_names, String *used_lengths);
  void add_info_string(String *str);
#ifndef DBUG_OFF
  void dbug_dump(int indent, bool verbose);
#endif
  virtual void replace_handler(handler *new_file) { file= new_file; }
  QUICK_SELECT_I *make_reverse(uint used_key_parts_arg);
private:
  /* Default copy ctor used by QUICK_SELECT_DESC */
  friend class TRP_ROR_INTERSECT;
  friend
  QUICK_RANGE_SELECT *get_quick_select_for_ref(THD *thd, TABLE *table,
                                               struct st_table_ref *ref,
                                               ha_rows records);
  friend bool get_quick_keys(PARAM *param, QUICK_RANGE_SELECT *quick, 
                             KEY_PART *key, SEL_ARG *key_tree, 
                             uchar *min_key, uint min_key_flag,
                             uchar *max_key, uint max_key_flag);
  friend QUICK_RANGE_SELECT *get_quick_select(PARAM*,uint idx,
                                              SEL_ARG *key_tree,
                                              uint mrr_flags,
                                              uint mrr_buf_size,
                                              MEM_ROOT *alloc);
  friend class QUICK_SELECT_DESC;
  friend class QUICK_INDEX_SORT_SELECT;
  friend class QUICK_INDEX_MERGE_SELECT;
  friend class QUICK_ROR_INTERSECT_SELECT;
  friend class QUICK_INDEX_INTERSECT_SELECT;
  friend class QUICK_GROUP_MIN_MAX_SELECT;
  friend bool quick_range_seq_next(range_seq_t rseq, KEY_MULTI_RANGE *range);
  friend range_seq_t quick_range_seq_init(void *init_param,
                                          uint n_ranges, uint flags);
  friend 
  int read_keys_and_merge_scans(THD *thd, TABLE *head,
                                List<QUICK_RANGE_SELECT> quick_selects,
                                QUICK_RANGE_SELECT *pk_quick_select,
                                READ_RECORD *read_record,
                                bool intersection,
                                key_map *filtered_scans,
                                Unique **unique_ptr);

};


class QUICK_RANGE_SELECT_GEOM: public QUICK_RANGE_SELECT
{
public:
  QUICK_RANGE_SELECT_GEOM(THD *thd, TABLE *table, uint index_arg,
                          bool no_alloc, MEM_ROOT *parent_alloc, 
                          bool *create_err)
    :QUICK_RANGE_SELECT(thd, table, index_arg, no_alloc, parent_alloc,
    create_err)
    {};
  virtual int get_next();
};


/*
  QUICK_INDEX_SORT_SELECT is the base class for the common functionality of:
  - QUICK_INDEX_MERGE_SELECT, access based on multi-index merge/union 
  - QUICK_INDEX_INTERSECT_SELECT, access based on  multi-index intersection 
    

    QUICK_INDEX_SORT_SELECT uses
     * QUICK_RANGE_SELECTs to get rows
     * Unique class
       - to remove duplicate rows for QUICK_INDEX_MERGE_SELECT
       - to intersect rows for QUICK_INDEX_INTERSECT_SELECT

  INDEX MERGE OPTIMIZER
    Current implementation doesn't detect all cases where index merge could
    be used, in particular:

     * index_merge+'using index' is not supported

     * If WHERE part contains complex nested AND and OR conditions, some ways
       to retrieve rows using index merge will not be considered. The choice
       of read plan may depend on the order of conjuncts/disjuncts in WHERE
       part of the query, see comments near imerge_list_or_list and
       SEL_IMERGE::or_sel_tree_with_checks functions for details.

     * There is no "index_merge_ref" method (but index merge on non-first
       table in join is possible with 'range checked for each record').


  ROW RETRIEVAL ALGORITHM

    index merge/intersection uses Unique class for duplicates removal. 
    index merge/intersection takes advantage of Clustered Primary Key (CPK)
    if the table has one.
    The index merge/intersection algorithm consists of two phases:

    Phase 1 
    (implemented by a QUICK_INDEX_MERGE_SELECT::read_keys_and_merge call):

    prepare()
    {
      activate 'index only';
      while(retrieve next row for non-CPK scan)
      {
        if (there is a CPK scan and row will be retrieved by it)
          skip this row;
        else
          put its rowid into Unique;
      }
      deactivate 'index only';
    }

    Phase 2 
    (implemented as sequence of QUICK_INDEX_MERGE_SELECT::get_next calls):

    fetch()
    {
      retrieve all rows from row pointers stored in Unique
      (merging/intersecting them);
      free Unique;
      if (! intersection) 
        retrieve all rows for CPK scan;
    }
*/

class QUICK_INDEX_SORT_SELECT : public QUICK_SELECT_I
{
protected:
  Unique *unique;
public:
  QUICK_INDEX_SORT_SELECT(THD *thd, TABLE *table);
  ~QUICK_INDEX_SORT_SELECT();

  int  init();
  void need_sorted_output() { DBUG_ASSERT(0); /* Can't do it */ }
  int  reset(void);
  bool reverse_sorted() { return false; }
  bool unique_key_range() { return false; }
  bool is_keys_used(const MY_BITMAP *fields);
#ifndef DBUG_OFF
  void dbug_dump(int indent, bool verbose);
#endif

  bool push_quick_back(QUICK_RANGE_SELECT *quick_sel_range);

  /* range quick selects this index merge/intersect consists of */
  List<QUICK_RANGE_SELECT> quick_selects;

  /* quick select that uses clustered primary key (NULL if none) */
  QUICK_RANGE_SELECT* pk_quick_select;

  MEM_ROOT alloc;
  THD *thd;
  virtual bool is_valid()
  {
    List_iterator_fast<QUICK_RANGE_SELECT> it(quick_selects);
    QUICK_RANGE_SELECT *quick;
    bool valid= true;
    while ((quick= it++))
    {
      if (!quick->is_valid())
      {
        valid= false;
        break;
      }
    }
    return valid;
  }
  virtual int read_keys_and_merge()= 0;
  /* used to get rows collected in Unique */
  READ_RECORD read_record;
};



class QUICK_INDEX_MERGE_SELECT : public QUICK_INDEX_SORT_SELECT
{
private:
  /* true if this select is currently doing a clustered PK scan */
  bool  doing_pk_scan;
protected:
  int read_keys_and_merge();

public:
  QUICK_INDEX_MERGE_SELECT(THD *thd, TABLE *table)
    :QUICK_INDEX_SORT_SELECT(thd, table) {}

  int get_next();
  int get_type() { return QS_TYPE_INDEX_MERGE; }
  void add_keys_and_lengths(String *key_names, String *used_lengths);
  void add_info_string(String *str);
};

class QUICK_INDEX_INTERSECT_SELECT : public QUICK_INDEX_SORT_SELECT
{
protected:
  int read_keys_and_merge();

public:
  QUICK_INDEX_INTERSECT_SELECT(THD *thd, TABLE *table)
    :QUICK_INDEX_SORT_SELECT(thd, table) {}

  key_map filtered_scans;
  int get_next();
  int get_type() { return QS_TYPE_INDEX_INTERSECT; }
  void add_keys_and_lengths(String *key_names, String *used_lengths);
  void add_info_string(String *str);
};


/*
  Rowid-Ordered Retrieval (ROR) index intersection quick select.
  This quick select produces intersection of row sequences returned
  by several QUICK_RANGE_SELECTs it "merges".

  All merged QUICK_RANGE_SELECTs must return rowids in rowid order.
  QUICK_ROR_INTERSECT_SELECT will return rows in rowid order, too.

  All merged quick selects retrieve {rowid, covered_fields} tuples (not full
  table records).
  QUICK_ROR_INTERSECT_SELECT retrieves full records if it is not being used
  by QUICK_ROR_INTERSECT_SELECT and all merged quick selects together don't
  cover needed all fields.

  If one of the merged quick selects is a Clustered PK range scan, it is
  used only to filter rowid sequence produced by other merged quick selects.
*/

class QUICK_ROR_INTERSECT_SELECT : public QUICK_SELECT_I
{
public:
  QUICK_ROR_INTERSECT_SELECT(THD *thd, TABLE *table,
                             bool retrieve_full_rows,
                             MEM_ROOT *parent_alloc);
  ~QUICK_ROR_INTERSECT_SELECT();

  int  init();
  void need_sorted_output() { DBUG_ASSERT(0); /* Can't do it */ }
  int  reset(void);
  int  get_next();
  bool reverse_sorted() { return false; }
  bool unique_key_range() { return false; }
  int get_type() { return QS_TYPE_ROR_INTERSECT; }
  void add_keys_and_lengths(String *key_names, String *used_lengths);
  void add_info_string(String *str);
  bool is_keys_used(const MY_BITMAP *fields);
#ifndef DBUG_OFF
  void dbug_dump(int indent, bool verbose);
#endif
  int init_ror_merged_scan(bool reuse_handler, MEM_ROOT *alloc);
  bool push_quick_back(MEM_ROOT *alloc, QUICK_RANGE_SELECT *quick_sel_range);

  class QUICK_SELECT_WITH_RECORD : public Sql_alloc
  {
  public:
    QUICK_RANGE_SELECT *quick;
    uchar *key_tuple;
    ~QUICK_SELECT_WITH_RECORD() { delete quick; }
  };

  /*
    Range quick selects this intersection consists of, not including
    cpk_quick.
  */
  List<QUICK_SELECT_WITH_RECORD> quick_selects;

  virtual bool is_valid()
  {
    List_iterator_fast<QUICK_SELECT_WITH_RECORD> it(quick_selects);
    QUICK_SELECT_WITH_RECORD *quick;
    bool valid= true;
    while ((quick= it++))
    {
      if (!quick->quick->is_valid())
      {
        valid= false;
        break;
      }
    }
    return valid;
  }

  /*
    Merged quick select that uses Clustered PK, if there is one. This quick
    select is not used for row retrieval, it is used for row retrieval.
  */
  QUICK_RANGE_SELECT *cpk_quick;

  MEM_ROOT alloc; /* Memory pool for this and merged quick selects data. */
  THD *thd;       /* current thread */
  bool need_to_fetch_row; /* if true, do retrieve full table records. */
  /* in top-level quick select, true if merged scans where initialized */
  bool scans_inited; 
};


/*
  Rowid-Ordered Retrieval index union select.
  This quick select produces union of row sequences returned by several
  quick select it "merges".

  All merged quick selects must return rowids in rowid order.
  QUICK_ROR_UNION_SELECT will return rows in rowid order, too.

  All merged quick selects are set not to retrieve full table records.
  ROR-union quick select always retrieves full records.

*/

class QUICK_ROR_UNION_SELECT : public QUICK_SELECT_I
{
public:
  QUICK_ROR_UNION_SELECT(THD *thd, TABLE *table);
  ~QUICK_ROR_UNION_SELECT();

  int  init();
  void need_sorted_output() { DBUG_ASSERT(0); /* Can't do it */ }
  int  reset(void);
  int  get_next();
  bool reverse_sorted() { return false; }
  bool unique_key_range() { return false; }
  int get_type() { return QS_TYPE_ROR_UNION; }
  void add_keys_and_lengths(String *key_names, String *used_lengths);
  void add_info_string(String *str);
  bool is_keys_used(const MY_BITMAP *fields);
#ifndef DBUG_OFF
  void dbug_dump(int indent, bool verbose);
#endif

  bool push_quick_back(QUICK_SELECT_I *quick_sel_range);

  List<QUICK_SELECT_I> quick_selects; /* Merged quick selects */

  virtual bool is_valid()
  {
    List_iterator_fast<QUICK_SELECT_I> it(quick_selects);
    QUICK_SELECT_I *quick;
    bool valid= true;
    while ((quick= it++))
    {
      if (!quick->is_valid())
      {
        valid= false;
        break;
      }
    }
    return valid;
  }

  QUEUE queue;    /* Priority queue for merge operation */
  MEM_ROOT alloc; /* Memory pool for this and merged quick selects data. */

  THD *thd;             /* current thread */
  uchar *cur_rowid;      /* buffer used in get_next() */
  uchar *prev_rowid;     /* rowid of last row returned by get_next() */
  bool have_prev_rowid; /* true if prev_rowid has valid data */
  uint rowid_length;    /* table rowid length */
private:
  bool scans_inited; 
};


/*
  Index scan for GROUP-BY queries with MIN/MAX aggregate functions.

  This class provides a specialized index access method for GROUP-BY queries
  of the forms:

       SELECT A_1,...,A_k, [B_1,...,B_m], [MIN(C)], [MAX(C)]
         FROM T
        WHERE [RNG(A_1,...,A_p ; where p <= k)]
         [AND EQ(B_1,...,B_m)]
         [AND PC(C)]
         [AND PA(A_i1,...,A_iq)]
       GROUP BY A_1,...,A_k;

    or

       SELECT DISTINCT A_i1,...,A_ik
         FROM T
        WHERE [RNG(A_1,...,A_p ; where p <= k)]
         [AND PA(A_i1,...,A_iq)];

  where all selected fields are parts of the same index.
  The class of queries that can be processed by this quick select is fully
  specified in the description of get_best_trp_group_min_max() in opt_range.cc.

  The get_next() method directly produces result tuples, thus obviating the
  need to call end_send_group() because all grouping is already done inside
  get_next().

  Since one of the requirements is that all select fields are part of the same
  index, this class produces only index keys, and not complete records.
*/

class QUICK_GROUP_MIN_MAX_SELECT : public QUICK_SELECT_I
{
private:
  handler * const file;   /* The handler used to get data. */
  JOIN *join;            /* Descriptor of the current query */
  KEY  *index_info;      /* The index chosen for data access */
  uchar *record;          /* Buffer where the next record is returned. */
  uchar *tmp_record;      /* Temporary storage for next_min(), next_max(). */
  uchar *group_prefix;    /* Key prefix consisting of the GROUP fields. */
  const uint group_prefix_len; /* Length of the group prefix. */
  uint group_key_parts;  /* A number of keyparts in the group prefix */
  uchar *last_prefix;     /* Prefix of the last group for detecting EOF. */
  bool have_min;         /* Specify whether we are computing */
  bool have_max;         /*   a MIN, a MAX, or both.         */
  bool have_agg_distinct;/*   aggregate_function(DISTINCT ...).  */
  bool seen_first_key;   /* Denotes whether the first key was retrieved.*/
  bool doing_key_read;   /* true if we enabled key only reads */

  KEY_PART_INFO *min_max_arg_part; /* The keypart of the only argument field */
                                   /* of all MIN/MAX functions.              */
  uint min_max_arg_len;  /* The length of the MIN/MAX argument field */
  uchar *key_infix;       /* Infix of constants from equality predicates. */
  uint key_infix_len;
  DYNAMIC_ARRAY min_max_ranges; /* Array of range ptrs for the MIN/MAX field. */
  uint real_prefix_len; /* Length of key prefix extended with key_infix. */
  uint real_key_parts;  /* A number of keyparts in the above value.      */
  List<Item_sum> *min_functions;
  List<Item_sum> *max_functions;
  List_iterator<Item_sum> *min_functions_it;
  List_iterator<Item_sum> *max_functions_it;
  /* 
    Use index scan to get the next different key instead of jumping into it 
    through index read 
  */
  bool is_index_scan; 
public:
  /*
    The following two members are public to allow easy access from
    TRP_GROUP_MIN_MAX::make_quick()
  */
  MEM_ROOT alloc; /* Memory pool for this and quick_prefix_select data. */
  QUICK_RANGE_SELECT *quick_prefix_select;/* For retrieval of group prefixes. */
private:
  int  next_prefix();
  int  next_min_in_range();
  int  next_max_in_range();
  int  next_min();
  int  next_max();
  void update_min_result();
  void update_max_result();
public:
  QUICK_GROUP_MIN_MAX_SELECT(TABLE *table, JOIN *join, bool have_min,
                             bool have_max, bool have_agg_distinct,
                             KEY_PART_INFO *min_max_arg_part,
                             uint group_prefix_len, uint group_key_parts,
                             uint used_key_parts, KEY *index_info, uint
                             use_index, double read_cost, ha_rows records, uint
                             key_infix_len, uchar *key_infix, MEM_ROOT
                             *parent_alloc, bool is_index_scan);
  ~QUICK_GROUP_MIN_MAX_SELECT();
  bool add_range(SEL_ARG *sel_range);
  void update_key_stat();
  void adjust_prefix_ranges();
  bool alloc_buffers();
  int init();
  void need_sorted_output() { /* always do it */ }
  int reset();
  int get_next();
  bool reverse_sorted() { return false; }
  bool unique_key_range() { return false; }
  int get_type() { return QS_TYPE_GROUP_MIN_MAX; }
  void add_keys_and_lengths(String *key_names, String *used_lengths);
#ifndef DBUG_OFF
  void dbug_dump(int indent, bool verbose);
#endif
  bool is_agg_distinct() { return have_agg_distinct; }
  virtual void append_loose_scan_type(String *str) 
  {
    if (is_index_scan)
      str->append(STRING_WITH_LEN(" (scanning)"));
  }
};


class QUICK_SELECT_DESC: public QUICK_RANGE_SELECT
{
public:
  QUICK_SELECT_DESC(QUICK_RANGE_SELECT *q, uint used_key_parts);
  int get_next();
  bool reverse_sorted() { return 1; }
  int get_type() { return QS_TYPE_RANGE_DESC; }
  QUICK_SELECT_I *make_reverse(uint used_key_parts_arg)
  {
    return this; // is already reverse sorted
  }
private:
  bool range_reads_after_key(QUICK_RANGE *range);
  int reset(void) { rev_it.rewind(); return QUICK_RANGE_SELECT::reset(); }
  List<QUICK_RANGE> rev_ranges;
  List_iterator<QUICK_RANGE> rev_it;
  uint used_key_parts;
};


class SQL_SELECT :public Sql_alloc {
 public:
  QUICK_SELECT_I *quick;	// If quick-select used
  COND		*cond;		// where condition

  /*
    When using Index Condition Pushdown: condition that we've had before
    extracting and pushing index condition.
    In other cases, NULL.
  */
  Item *pre_idx_push_select_cond;
  TABLE	*head;
  IO_CACHE file;		// Positions to used records
  ha_rows records;		// Records in use if read from file
  double read_time;		// Time to read rows
  key_map quick_keys;		// Possible quick keys
  key_map needed_reg;		// Possible quick keys after prev tables.
  table_map const_tables,read_tables;
  bool	free_cond; /* Currently not used and always FALSE */

  SQL_SELECT();
  ~SQL_SELECT();
  void cleanup();
  void set_quick(QUICK_SELECT_I *new_quick) { delete quick; quick= new_quick; }
  bool check_quick(THD *thd, bool force_quick_range, ha_rows limit)
  {
    key_map tmp;
    tmp.set_all();
    return test_quick_select(thd, tmp, 0, limit, force_quick_range, FALSE) < 0;
  }
  /* 
    RETURN
      0   if record must be skipped <-> (cond && cond->val_int() == 0)
     -1   if error
      1   otherwise
  */   
  inline int skip_record(THD *thd)
  {
    int rc= test(!cond || cond->val_int());
    if (thd->is_error())
      rc= -1;
    return rc;
  }
  int test_quick_select(THD *thd, key_map keys, table_map prev_tables,
			ha_rows limit, bool force_quick_range, 
                        bool ordered_output);
};


class FT_SELECT: public QUICK_RANGE_SELECT 
{
public:
  FT_SELECT(THD *thd, TABLE *table, uint key, bool *create_err) :
      QUICK_RANGE_SELECT (thd, table, key, 1, NULL, create_err) 
  { (void) init(); }
  ~FT_SELECT() { file->ft_end(); }
  int init() { return file->ft_init(); }
  int reset() { return 0; }
  int get_next() { return file->ha_ft_read(record); }
  int get_type() { return QS_TYPE_FULLTEXT; }
};

FT_SELECT *get_ft_select(THD *thd, TABLE *table, uint key);
QUICK_RANGE_SELECT *get_quick_select_for_ref(THD *thd, TABLE *table,
                                             struct st_table_ref *ref,
                                             ha_rows records);
SQL_SELECT *make_select(TABLE *head, table_map const_tables,
			table_map read_tables, COND *conds,
                        bool allow_null_cond,  int *error);

#ifdef WITH_PARTITION_STORAGE_ENGINE
bool prune_partitions(THD *thd, TABLE *table, Item *pprune_cond);
void store_key_image_to_rec(Field *field, uchar *ptr, uint len);
#endif

extern String null_string;

#endif
