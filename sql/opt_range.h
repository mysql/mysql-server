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

#ifndef _opt_range_h
#define _opt_range_h

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

#define NO_MIN_RANGE	1
#define NO_MAX_RANGE	2
#define NEAR_MIN	4
#define NEAR_MAX	8
#define UNIQUE_RANGE	16
#define EQ_RANGE	32
#define NULL_RANGE	64
#define GEOM_FLAG      128


typedef struct st_key_part {
  uint16           key,part, store_length, length;
  uint8            null_bit;
  Field            *field;
  Field::imagetype image_type;
} KEY_PART;


class QUICK_RANGE :public Sql_alloc {
 public:
  char *min_key,*max_key;
  uint16 min_length,max_length,flag;
#ifdef HAVE_purify
  uint16 dummy;					/* Avoid warnings on 'flag' */
#endif
  QUICK_RANGE();				/* Full range */
  QUICK_RANGE(const char *min_key_arg,uint min_length_arg,
	      const char *max_key_arg,uint max_length_arg,
	      uint flag_arg)
    : min_key((char*) sql_memdup(min_key_arg,min_length_arg+1)),
      max_key((char*) sql_memdup(max_key_arg,max_length_arg+1)),
      min_length((uint16) min_length_arg),
      max_length((uint16) max_length_arg),
      flag((uint16) flag_arg)
    {
#ifdef HAVE_purify
      dummy=0;
#endif
    }
};


/*
  Quick select interface.
  This class is a parent for all QUICK_*_SELECT and FT_SELECT classes.
*/

class QUICK_SELECT_I
{
public:
  bool sorted;
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
    Note that reset() may be called several times if this quick select 
    executes in a subselect.
    RETURN
      0      OK
      other  Error code
  */
  virtual int  reset(void) = 0;

  /* Range end should be called when we have looped over the whole index */
  virtual void range_end() {}
  virtual int  get_next() = 0;   /* get next record to retrieve */
  virtual bool reverse_sorted() = 0;
  virtual bool unique_key_range() { return false; }

  enum {
    QS_TYPE_RANGE = 0,
    QS_TYPE_INDEX_MERGE = 1,
    QS_TYPE_RANGE_DESC = 2,
    QS_TYPE_FULLTEXT   = 3,
    QS_TYPE_ROR_INTERSECT = 4,
    QS_TYPE_ROR_UNION = 5,
    QS_TYPE_GROUP_MIN_MAX = 6
  };

  /* Get type of this quick select - one of the QS_TYPE_* values */
  virtual int get_type() = 0;

  /*
    Initialize this quick select as a merged scan inside a ROR-union or a ROR-
    intersection scan. The caller must not additionally call init() if this
    function is called.
    SYNOPSIS
      init_ror_merged_scan()
        reuse_handler If true, the quick select may use table->handler, otherwise
                      it must create and use a separate handler object.
    RETURN
      0     Ok
      other Error
  */
  virtual int init_ror_merged_scan(bool reuse_handler)
  { DBUG_ASSERT(0); return 1; }

  /*
    Save ROWID of last retrieved row in file->ref. This used in ROR-merging.
  */
  virtual void save_last_pos(){};

  /*
    Append comma-separated list of keys this quick select uses to key_names;
    append comma-separated list of corresponding used lengths to used_lengths.
    This is used by select_describe.
  */
  virtual void add_keys_and_lengths(String *key_names,
                                    String *used_lengths)=0;

  /*
    Append text representation of quick select structure (what and how is
    merged) to str. The result is added to "Extra" field in EXPLAIN output.
    This function is implemented only by quick selects that merge other quick
    selects output and/or can produce output suitable for merging.
  */
  virtual void add_info_string(String *str) {};
  /*
    Return 1 if any index used by this quick select
     a) uses field that is listed in passed field list or
     b) is automatically updated (like a timestamp)
  */
  virtual bool check_if_keys_used(List<Item> *fields);

  /*
    rowid of last row retrieved by this quick select. This is used only when
    doing ROR-index_merge selects
  */
  byte    *last_rowid;

  /*
    Table record buffer used by this quick select.
  */
  byte    *record;
#ifndef DBUG_OFF
  /*
    Print quick select information to DBUG_FILE. Caller is responsible
    for locking DBUG_FILE before this call and unlocking it afterwards.
  */
  virtual void dbug_dump(int indent, bool verbose)= 0;
#endif
};


struct st_qsel_param;
class SEL_ARG;

/*
  Quick select that does a range scan on a single key. The records are
  returned in key order.
*/
class QUICK_RANGE_SELECT : public QUICK_SELECT_I
{
protected:
  bool next,dont_free;
public:
  int error;
protected:
  handler *file;
  /*
    If true, this quick select has its "own" handler object which should be
    closed no later then this quick select is deleted.
  */
  bool free_file;

protected:
  friend class TRP_ROR_INTERSECT;
  friend
  QUICK_RANGE_SELECT *get_quick_select_for_ref(THD *thd, TABLE *table,
                                               struct st_table_ref *ref);
  friend bool get_quick_keys(struct st_qsel_param *param,
                             QUICK_RANGE_SELECT *quick,KEY_PART *key,
                             SEL_ARG *key_tree,
                             char *min_key, uint min_key_flag,
                             char *max_key, uint max_key_flag);
  friend QUICK_RANGE_SELECT *get_quick_select(struct st_qsel_param*,uint idx,
                                              SEL_ARG *key_tree,
                                              MEM_ROOT *alloc);
  friend class QUICK_SELECT_DESC;
  friend class QUICK_INDEX_MERGE_SELECT;
  friend class QUICK_ROR_INTERSECT_SELECT;

  DYNAMIC_ARRAY ranges;     /* ordered array of range ptrs */
  QUICK_RANGE **cur_range;  /* current element in ranges  */

  QUICK_RANGE *range;
  KEY_PART *key_parts;
  KEY_PART_INFO *key_part_info;
  int cmp_next(QUICK_RANGE *range);
  int cmp_prev(QUICK_RANGE *range);
  bool row_in_ranges();
public:
  MEM_ROOT alloc;

  QUICK_RANGE_SELECT(THD *thd, TABLE *table,uint index_arg,bool no_alloc=0,
                     MEM_ROOT *parent_alloc=NULL);
  ~QUICK_RANGE_SELECT();

  int reset(void)
  {
    next=0;
    range= NULL;
    cur_range= NULL;
    /*
      Note: in opt_range.cc there are places where it is assumed that this
      function always succeeds 
    */
    return 0;
  }
  int init();
  int get_next();
  void range_end();
  int get_next_prefix(uint prefix_length, byte *cur_prefix);
  bool reverse_sorted() { return 0; }
  bool unique_key_range();
  int init_ror_merged_scan(bool reuse_handler);
  void save_last_pos()
  { file->position(record); }
  int get_type() { return QS_TYPE_RANGE; }
  void add_keys_and_lengths(String *key_names, String *used_lengths);
  void add_info_string(String *str);
#ifndef DBUG_OFF
  void dbug_dump(int indent, bool verbose);
#endif
};


class QUICK_RANGE_SELECT_GEOM: public QUICK_RANGE_SELECT
{
public:
  QUICK_RANGE_SELECT_GEOM(THD *thd, TABLE *table, uint index_arg,
                          bool no_alloc, MEM_ROOT *parent_alloc)
    :QUICK_RANGE_SELECT(thd, table, index_arg, no_alloc, parent_alloc)
    {};
  virtual int get_next();
};


/*
  QUICK_INDEX_MERGE_SELECT - index_merge access method quick select.

    QUICK_INDEX_MERGE_SELECT uses
     * QUICK_RANGE_SELECTs to get rows
     * Unique class to remove duplicate rows

  INDEX MERGE OPTIMIZER
    Current implementation doesn't detect all cases where index_merge could
    be used, in particular:
     * index_merge will never be used if range scan is possible (even if
       range scan is more expensive)

     * index_merge+'using index' is not supported (this the consequence of
       the above restriction)

     * If WHERE part contains complex nested AND and OR conditions, some ways
       to retrieve rows using index_merge will not be considered. The choice
       of read plan may depend on the order of conjuncts/disjuncts in WHERE
       part of the query, see comments near imerge_list_or_list and
       SEL_IMERGE::or_sel_tree_with_checks functions for details.

     * There is no "index_merge_ref" method (but index_merge on non-first
       table in join is possible with 'range checked for each record').

    See comments around SEL_IMERGE class and test_quick_select for more
    details.

  ROW RETRIEVAL ALGORITHM

    index_merge uses Unique class for duplicates removal.  index_merge takes
    advantage of Clustered Primary Key (CPK) if the table has one.
    The index_merge algorithm consists of two phases:

    Phase 1 (implemented in QUICK_INDEX_MERGE_SELECT::prepare_unique):
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

    Phase 2 (implemented as sequence of QUICK_INDEX_MERGE_SELECT::get_next
    calls):

    fetch()
    {
      retrieve all rows from row pointers stored in Unique;
      free Unique;
      retrieve all rows for CPK scan;
    }
*/

class QUICK_INDEX_MERGE_SELECT : public QUICK_SELECT_I
{
public:
  QUICK_INDEX_MERGE_SELECT(THD *thd, TABLE *table);
  ~QUICK_INDEX_MERGE_SELECT();

  int  init();
  int  reset(void);
  int  get_next();
  bool reverse_sorted() { return false; }
  bool unique_key_range() { return false; }
  int get_type() { return QS_TYPE_INDEX_MERGE; }
  void add_keys_and_lengths(String *key_names, String *used_lengths);
  void add_info_string(String *str);
  bool check_if_keys_used(List<Item> *fields);
#ifndef DBUG_OFF
  void dbug_dump(int indent, bool verbose);
#endif

  bool push_quick_back(QUICK_RANGE_SELECT *quick_sel_range);

  /* range quick selects this index_merge read consists of */
  List<QUICK_RANGE_SELECT> quick_selects;

  /* quick select that uses clustered primary key (NULL if none) */
  QUICK_RANGE_SELECT* pk_quick_select;

  /* true if this select is currently doing a clustered PK scan */
  bool  doing_pk_scan;

  MEM_ROOT alloc;
  THD *thd;
  int read_keys_and_merge();

  /* used to get rows collected in Unique */
  READ_RECORD read_record;
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
  int  reset(void);
  int  get_next();
  bool reverse_sorted() { return false; }
  bool unique_key_range() { return false; }
  int get_type() { return QS_TYPE_ROR_INTERSECT; }
  void add_keys_and_lengths(String *key_names, String *used_lengths);
  void add_info_string(String *str);
  bool check_if_keys_used(List<Item> *fields);
#ifndef DBUG_OFF
  void dbug_dump(int indent, bool verbose);
#endif
  int init_ror_merged_scan(bool reuse_handler);
  bool push_quick_back(QUICK_RANGE_SELECT *quick_sel_range);

  /*
    Range quick selects this intersection consists of, not including
    cpk_quick.
  */
  List<QUICK_RANGE_SELECT> quick_selects;

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
  int  reset(void);
  int  get_next();
  bool reverse_sorted() { return false; }
  bool unique_key_range() { return false; }
  int get_type() { return QS_TYPE_ROR_UNION; }
  void add_keys_and_lengths(String *key_names, String *used_lengths);
  void add_info_string(String *str);
  bool check_if_keys_used(List<Item> *fields);
#ifndef DBUG_OFF
  void dbug_dump(int indent, bool verbose);
#endif

  bool push_quick_back(QUICK_SELECT_I *quick_sel_range);

  List<QUICK_SELECT_I> quick_selects; /* Merged quick selects */

  QUEUE queue;    /* Priority queue for merge operation */
  MEM_ROOT alloc; /* Memory pool for this and merged quick selects data. */

  THD *thd;             /* current thread */
  byte *cur_rowid;      /* buffer used in get_next() */
  byte *prev_rowid;     /* rowid of last row returned by get_next() */
  bool have_prev_rowid; /* true if prev_rowid has valid data */
  uint rowid_length;    /* table rowid length */
private:
  static int queue_cmp(void *arg, byte *val1, byte *val2);
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
  handler *file;         /* The handler used to get data. */
  JOIN *join;            /* Descriptor of the current query */
  KEY  *index_info;      /* The index chosen for data access */
  byte *record;          /* Buffer where the next record is returned. */
  byte *tmp_record;      /* Temporary storage for next_min(), next_max(). */
  byte *group_prefix;    /* Key prefix consisting of the GROUP fields. */
  uint group_prefix_len; /* Length of the group prefix. */
  byte *last_prefix;     /* Prefix of the last group for detecting EOF. */
  bool have_min;         /* Specify whether we are computing */
  bool have_max;         /*   a MIN, a MAX, or both.         */
  bool seen_first_key;   /* Denotes whether the first key was retrieved.*/
  KEY_PART_INFO *min_max_arg_part; /* The keypart of the only argument field */
                                   /* of all MIN/MAX functions.              */
  uint min_max_arg_len;  /* The length of the MIN/MAX argument field */
  byte *key_infix;       /* Infix of constants from equality predicates. */
  uint key_infix_len;
  DYNAMIC_ARRAY min_max_ranges; /* Array of range ptrs for the MIN/MAX field. */
  uint real_prefix_len; /* Length of key prefix extended with key_infix. */
  List<Item_sum> *min_functions;
  List<Item_sum> *max_functions;
  List_iterator<Item_sum> *min_functions_it;
  List_iterator<Item_sum> *max_functions_it;
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
                             bool have_max, KEY_PART_INFO *min_max_arg_part,
                             uint group_prefix_len, uint used_key_parts,
                             KEY *index_info, uint use_index, double read_cost,
                             ha_rows records, uint key_infix_len,
                             byte *key_infix, MEM_ROOT *parent_alloc);
  ~QUICK_GROUP_MIN_MAX_SELECT();
  bool add_range(SEL_ARG *sel_range);
  void update_key_stat();
  bool alloc_buffers();
  int init();
  int reset();
  int get_next();
  bool reverse_sorted() { return false; }
  bool unique_key_range() { return false; }
  int get_type() { return QS_TYPE_GROUP_MIN_MAX; }
  void add_keys_and_lengths(String *key_names, String *used_lengths);
#ifndef DBUG_OFF
  void dbug_dump(int indent, bool verbose);
#endif
};


class QUICK_SELECT_DESC: public QUICK_RANGE_SELECT
{
public:
  QUICK_SELECT_DESC(QUICK_RANGE_SELECT *q, uint used_key_parts);
  int get_next();
  bool reverse_sorted() { return 1; }
  int get_type() { return QS_TYPE_RANGE_DESC; }
private:
  bool range_reads_after_key(QUICK_RANGE *range);
#ifdef NOT_USED
  bool test_if_null_range(QUICK_RANGE *range, uint used_key_parts);
#endif
  int reset(void) { next=0; rev_it.rewind(); return 0; }
  List<QUICK_RANGE> rev_ranges;
  List_iterator<QUICK_RANGE> rev_it;
};


class SQL_SELECT :public Sql_alloc {
 public:
  QUICK_SELECT_I *quick;	// If quick-select used
  COND		*cond;		// where condition
  TABLE	*head;
  IO_CACHE file;		// Positions to used records
  ha_rows records;		// Records in use if read from file
  double read_time;		// Time to read rows
  key_map quick_keys;		// Possible quick keys
  key_map needed_reg;		// Possible quick keys after prev tables.
  table_map const_tables,read_tables;
  bool	free_cond;

  SQL_SELECT();
  ~SQL_SELECT();
  void cleanup();
  bool check_quick(THD *thd, bool force_quick_range, ha_rows limit)
  { return test_quick_select(thd, key_map(~0), 0, limit, force_quick_range) < 0; }
  inline bool skip_record() { return cond ? cond->val_int() == 0 : 0; }
  int test_quick_select(THD *thd, key_map keys, table_map prev_tables,
			ha_rows limit, bool force_quick_range=0);
};


class FT_SELECT: public QUICK_RANGE_SELECT {
public:
  FT_SELECT(THD *thd, TABLE *table, uint key) :
      QUICK_RANGE_SELECT (thd, table, key, 1) { init(); }
  ~FT_SELECT() { file->ft_end(); }
  int init() { return error=file->ft_init(); }
  int get_next() { return error=file->ft_read(record); }
  int get_type() { return QS_TYPE_FULLTEXT; }
};

QUICK_RANGE_SELECT *get_quick_select_for_ref(THD *thd, TABLE *table,
                                             struct st_table_ref *ref);
#endif
