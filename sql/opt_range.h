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
  uint16           key,part,part_length;
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
  ha_rows records;  /* estimate of # of records to be retrieved */
  double  read_time; /* time to perform this retrieval          */
  TABLE   *head;

  /*
    index this quick select uses, or MAX_KEY for quick selects 
    that use several indexes
  */
  uint index;

  /* applicable iff index!= MAX_KEY */
  uint max_used_key_length, used_key_parts;

  QUICK_SELECT_I();
  virtual ~QUICK_SELECT_I(){};
  /* 
    Call init() immediately after creation of quick select. if init() call 
    fails, reset() or get_next() must not be called.
  */
  virtual int  init() = 0;
  
  /* 
    Call reset() before first get_next call. get_next must not be called if 
    reset() call fails.
  */
  virtual int  reset(void) = 0;
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
  };

  /* Get type of this quick select - one of the QS_TYPE_* values */
  virtual int get_type() = 0;

  /*
    Initialize this quick select as a child of a index union or intersection 
    scan. This call replaces init() call.
  */
  virtual int init_ror_child_scan(bool reuse_handler)
  { DBUG_ASSERT(0); return 1; }
  
  virtual void cleanup_ror_child_scan() { DBUG_ASSERT(0); }
  virtual void save_last_pos(){};

  /* 
    Fill key_names with list of keys this quick select used;
    fill used_lenghth with correponding used lengths.
    This is used by select_describe.
  */
  virtual void fill_keys_and_lengths(String *key_names, 
                                     String *used_lengths)=0;

  virtual bool check_if_keys_used(List<Item> *fields);

  /*
    rowid of last row retrieved by this quick select. This is used only 
    when doing ROR-index_merge selects 
  */
  byte    *last_rowid;
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

class QUICK_RANGE_SELECT : public QUICK_SELECT_I
{
protected:
  bool next,dont_free;
public:
  int error;
protected:
  handler *file;
  bool free_file; /* if true, this quick select "owns" file and will free it */

protected:
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
    return 0;
  }
  int init();
  int get_next();
  bool reverse_sorted() { return 0; }
  bool unique_key_range();
  int init_ror_child_scan(bool reuse_handler);
  void save_last_pos()
  {
    file->position(record);
  };
  int get_type() { return QS_TYPE_RANGE; }
  void fill_keys_and_lengths(String *key_names, String *used_lengths);
#ifndef DBUG_OFF
  virtual void dbug_dump(int indent, bool verbose);
#endif
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
  void fill_keys_and_lengths(String *key_names, String *used_lengths);
  bool check_if_keys_used(List<Item> *fields);
#ifndef DBUG_OFF
  virtual void dbug_dump(int indent, bool verbose);
#endif

  bool push_quick_back(QUICK_RANGE_SELECT *quick_sel_range);

  /* range quick selects this index_merge read consists of */
  List<QUICK_RANGE_SELECT> quick_selects;
  
  /* quick select which is currently used for rows retrieval */
  List_iterator_fast<QUICK_RANGE_SELECT> cur_quick_it;
  QUICK_RANGE_SELECT* cur_quick_select;
  
  /* quick select that uses clustered primary key (NULL if none) */
  QUICK_RANGE_SELECT* pk_quick_select;
  
  /* true if this select is currently doing a clustered PK scan */
  bool  doing_pk_scan;
  
  Unique  *unique;
  MEM_ROOT alloc;

  THD *thd;
  int prepare_unique();
  bool reset_called;
  
  /* used to get rows collected in Unique */
  READ_RECORD read_record;
};


/*
  Rowid-Ordered Retrieval (ROR) index intersection quick select.
  This quick select produces an intersection of records returned by several
  QUICK_RANGE_SELECTs that return data ordered by rowid.
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
  void fill_keys_and_lengths(String *key_names, String *used_lengths);
  bool check_if_keys_used(List<Item> *fields);
#ifndef DBUG_OFF
  virtual void dbug_dump(int indent, bool verbose);
#endif
  int init_ror_child_scan(bool reuse_handler);
  bool push_quick_back(QUICK_RANGE_SELECT *quick_sel_range);

  /* range quick selects this intersection consists of */
  List<QUICK_RANGE_SELECT> quick_selects;
  
  QUICK_RANGE_SELECT *cpk_quick;
  MEM_ROOT alloc;
  THD *thd;
  bool reset_called;    
  bool need_to_fetch_row;
};

/*
  Rowid-Ordered Retrieval index union select.

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
  void fill_keys_and_lengths(String *key_names, String *used_lengths);
  bool check_if_keys_used(List<Item> *fields);
#ifndef DBUG_OFF
  virtual void dbug_dump(int indent, bool verbose);
#endif

  bool push_quick_back(QUICK_SELECT_I *quick_sel_range);

  /* range quick selects this index_merge read consists of */
  List<QUICK_SELECT_I> quick_selects;
  
  QUEUE queue;
  MEM_ROOT alloc;

  THD *thd;
  byte *cur_rowid;
  byte *prev_rowid;
  uint rowid_length;
  bool reset_called;
  bool have_prev_rowid;
  
private:
  static int queue_cmp(void *arg, byte *val1, byte *val2);
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

  int init() { return error=file->ft_init(); }
  int get_next() { return error=file->ft_read(record); }
  int get_type() { return QS_TYPE_FULLTEXT; }
};

#endif
