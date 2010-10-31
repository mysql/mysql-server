/**
  @defgroup DS-MRR declarations
  @{
*/

/**
  A Disk-Sweep implementation of MRR Interface (DS-MRR for short)

  This is a "plugin"(*) for storage engines that allows to
    1. When doing index scans, read table rows in rowid order;
    2. when making many index lookups, do them in key order and don't
       lookup the same key value multiple times;
    3. Do both #1 and #2, when applicable.
  These changes are expected to speed up query execution for disk-based 
  storage engines running io-bound loads and "big" queries (ie. queries that
  do joins and enumerate lots of records).

  (*) - only conceptually. No dynamic loading or binary compatibility of any
        kind.

  General scheme of things:
   
      SQL Layer code
       |   |   |
       v   v   v 
      -|---|---|---- handler->multi_range_read_XXX() function calls
       |   |   |
      _____________________________________
     / DS-MRR module                       \
     | (order/de-duplicate lookup keys,    |
     | scan indexes in key order,          |
     | order/de-duplicate rowids,          |
     | retrieve full record reads in rowid |
     | order)                              |
     \_____________________________________/
       |   |   |
      -|---|---|----- handler->read_range_first()/read_range_next(), 
       |   |   |      handler->index_read(), handler->rnd_pos() calls.
       |   |   |
       v   v   v
      Storage engine internals


  Currently DS-MRR is used by MyISAM, InnoDB/XtraDB and Maria storage engines.
  Potentially it can be used with any table handler that has disk-based data
  storage and has better performance when reading data in rowid order.
*/

#include "sql_lifo_buffer.h"

class DsMrr_impl;

class Key_parameters
{
public:
  /* TRUE <=> We can get at most one index tuple for a lookup key */
  bool index_ranges_unique;

  uint         key_tuple_length; /* Length of index lookup tuple, in bytes */
  key_part_map key_tuple_map;    /* keyparts used in index lookup tuples */

  /*
    This is 
      = key_tuple_length   if we copy keys to buffer
      = sizeof(void*)      if we're using pointers to materialized keys.
  */
  uint key_size_in_keybuf;

  /* TRUE <=> don't copy key values, use pointers to them instead.  */
  bool use_key_pointers;
};

/**
  Iterator over (record, range_id) pairs that match given key value.
  
  We may need to scan multiple (key_val, range_id) pairs with the same 
  key value. A key value may have multiple matching records, so we'll need to
  produce a cross-product of sets of matching records and range_id-s.
*/
class Mrr_ordered_index_reader;
class Key_value_records_iterator
{
  /* Scan parameters */
  Key_parameters *param;
  Lifo_buffer_iterator identical_key_it;
  uchar *last_identical_key_ptr;
  bool get_next_row;
  //handler *h;
  /* TRUE <=> We can get at most one index tuple for a lookup key */
  //bool index_ranges_unique;
  
  Mrr_ordered_index_reader *owner;
  /* key_buffer.read() reads to here */
  uchar *cur_index_tuple;
public:
  bool init(Mrr_ordered_index_reader *owner_arg);

  /*
    Get next (key_val, range_id) pair.
  */
  int get_next();

  void close();
  friend class Mrr_ordered_index_reader;
};


/*
  Something that will manage buffers for those that call it
*/
class Buffer_manager
{
public:
  virtual void reset_buffer_sizes()= 0;
  virtual void setup_buffer_sizes(uint key_size_in_keybuf, 
                                  key_part_map key_tuple_map)=0;
  virtual Lifo_buffer* get_key_buffer()= 0;
  virtual ~Buffer_manager(){}
};


/* 
  Abstract MRR execution strategy
  
  An object of this class produces (R, range_info) pairs where R can be an
  index tuple or a table record.

  Getting HA_ERR_END_OF_FILE from get_next() means that the source should be
  re-filled. 
  
  Was:
  if eof() returns true after refill attempt, then the end of 
  stream has been reached and get_next() must not be called anymore.

  Now:
  if refill_buffer() returns HA_ERR_END_OF_FILE that means the stream is 
  really exhausted.

*/

class Mrr_reader 
{
public:
  virtual int get_next(char **range_info) = 0;
  virtual int refill_buffer()=0;
  
  virtual ~Mrr_reader() {}; /* just to remove compiler warning */
};


/* A common base for strategies that do index scans and produce index tuples */
class Mrr_index_reader : public Mrr_reader
{
public:
  handler *h;

  virtual int init(handler *h_arg, RANGE_SEQ_IF *seq_funcs, 
                   void *seq_init_param, uint n_ranges,
                   uint mode, Buffer_manager *buf_manager_arg) = 0;
  virtual bool eof() = 0; 
  virtual uchar *get_rowid_ptr()= 0;
  virtual bool skip_record(char *range_id, uchar *rowid)=0;
};


/*
  A "bypass" strategy that uses default MRR implementation (i.e.
  handler::multi_range_read_XXX() calls) to produce rows.
*/

class Mrr_simple_index_reader : public Mrr_index_reader
{
  int res; 
public:
  int init(handler *h_arg, RANGE_SEQ_IF *seq_funcs, 
           void *seq_init_param, uint n_ranges,
           uint mode, Buffer_manager *buf_manager_arg);
  int get_next(char **range_info);
  int refill_buffer() { return HA_ERR_END_OF_FILE; }
  bool eof() { return test(res); }
  uchar *get_rowid_ptr() { return h->ref; }
  bool skip_record(char *range_id, uchar *rowid)
  {
    return (h->mrr_funcs.skip_record &&
            h->mrr_funcs.skip_record(h->mrr_iter, range_id, rowid));
  }
};



/* 
  A strategy that sorts index lookup keys before scanning the index
*/

class Mrr_ordered_index_reader : public Mrr_index_reader
{
public:
  int init(handler *h_arg, RANGE_SEQ_IF *seq_funcs, 
           void *seq_init_param, uint n_ranges,
           uint mode, Buffer_manager *buf_manager_arg);
  int get_next(char **range_info);
  int refill_buffer();
  bool eof() { return index_scan_eof; }
  uchar *get_rowid_ptr() { return h->ref; }
  
  bool skip_record(char *range_info, uchar *rowid)
  {
    return (mrr_funcs.skip_record &&
            mrr_funcs.skip_record(mrr_iter, range_info, rowid));
  }
private:
  Key_value_records_iterator kv_it;

  bool scanning_key_val_iter;
  
  char *cur_range_info;

  /* Buffer to store (key, range_id) pairs */
  Lifo_buffer *key_buffer;

  Buffer_manager *buf_manager;

  /* Initially FALSE, becomes TRUE when we've set key_tuple_xxx members */
  bool know_key_tuple_params;

 // bool use_key_pointers;
  
  Key_parameters  keypar;
  /* TRUE <=> need range association, buffers hold {rowid, range_id} pairs */
  bool is_mrr_assoc;

  bool no_more_keys;
  RANGE_SEQ_IF mrr_funcs;
  range_seq_t mrr_iter;

  bool auto_refill;

  bool index_scan_eof;

  static int key_tuple_cmp(void* arg, uchar* key1, uchar* key2);
  static int key_tuple_cmp_reverse(void* arg, uchar* key1, uchar* key2);
  //void cleanup();
  
  friend class Key_value_records_iterator; 
  friend class DsMrr_impl;
  friend class Mrr_ordered_rndpos_reader;
};


/* MRR strategy that fetches rowids */

class Mrr_ordered_rndpos_reader : public Mrr_reader 
{
public:
  int init(handler *h, Mrr_index_reader *index_reader, uint mode,
           Lifo_buffer *buf);
  int get_next(char **range_info);
  int refill_buffer();
  void cleanup();
private:
  handler *h;
  
  DsMrr_impl *dsmrr;
  /* This what we get (rowid, range_info) pairs from */
  Mrr_index_reader *index_reader;
  uchar *index_rowid;
  
  /* TRUE <=> need range association, buffers hold {rowid, range_id} pairs */
  bool is_mrr_assoc;

  uchar *last_identical_rowid;
  Lifo_buffer *rowid_buffer;
  
  /* = h->ref_length  [ + sizeof(range_assoc_info) ] */
  //uint rowid_buff_elem_size;

  /* rowid_buffer.read() will set the following:  */
  uchar *rowid;
  uchar *rowids_range_id;
};

class Mrr_reader_factory
{
public:
  Mrr_ordered_rndpos_reader ordered_rndpos_reader;
  Mrr_ordered_index_reader  ordered_index_reader;
  Mrr_simple_index_reader   simple_index_reader;
};

/*
  DS-MRR implementation for one table. Create/use one object of this class for
  each ha_{myisam/innobase/etc} object. That object will be further referred to
  as "the handler"

  DsMrr_impl supports has the following execution strategies:

  - Bypass DS-MRR, pass all calls to default MRR implementation, which is 
    an MRR-to-non-MRR call converter.
  - Key-Ordered Retrieval
  - Rowid-Ordered Retrieval

  DsMrr_impl will use one of the above strategies, or a combination of them, 
  according to the following diagram:

         (mrr function calls)
                |
                +----------------->-----------------+
                |                                   |
     ___________v______________      _______________v________________
    / default: use lookup keys \    / KEY-ORDERED RETRIEVAL:         \
    | (or ranges) in whatever  |    | sort lookup keys and then make | 
    | order they are supplied  |    | index lookups in index order   |
    \__________________________/    \________________________________/
              | |  |                           |    |
      +---<---+ |  +--------------->-----------|----+
      |         |                              |    |
      |         |              +---------------+    |
      |   ______v___ ______    |     _______________v_______________
      |  / default: read   \   |    / ROWID-ORDERED RETRIEVAL:      \
      |  | table records   |   |    | Before reading table records, |
      v  | in random order |   v    | sort their rowids and then    |
      |  \_________________/   |    | read them in rowid order      |
      |         |              |    \_______________________________/
      |         |              |                    |
      |         |              |                    |
      +-->---+  |  +----<------+-----------<--------+
             |  |  |                                
             v  v  v
      (table records and range_ids)

  The choice of strategy depends on MRR scan properties, table properties
  (whether we're scanning clustered primary key), and @@optimizer_switch
  settings.
  
  Key-Ordered Retrieval
  ---------------------
  The idea is: if MRR scan is essentially a series of lookups on 
   
    tbl.key=value1 OR tbl.key=value2 OR ... OR tbl.key=valueN
  
  then it makes sense to collect and order the set of lookup values, i.e.
   
     sort(value1, value2, .. valueN)

  and then do index lookups in index order. This results in fewer index page
  fetch operations, and we also can avoid making multiple index lookups for the
  same value. That is, if value1=valueN we can easily discover that after
  sorting and make one index lookup for them instead of two.

  Rowid-Ordered Retrieval
  -----------------------
  If we do a regular index scan or a series of index lookups, we'll be hitting
  table records at random. For disk-based engines, this is much slower than 
  reading the same records in disk order. We assume that disk ordering of
  rows is the same as ordering of their rowids (which is provided by 
  handler::cmp_ref())
  In order to retrieve records in different order, we must separate index
  scanning and record fetching, that is, MRR scan uses the following steps:

    1. Scan the index (and only index, that is, with HA_EXTRA_KEYREAD on) and 
        fill a buffer with {rowid, range_id} pairs
    2. Sort the buffer by rowid value
    3. for each {rowid, range_id} pair in the buffer
         get record by rowid and return the {record, range_id} pair
    4. Repeat the above steps until we've exhausted the list of ranges we're
       scanning.

  Buffer space management considerations
  --------------------------------------
  With regards to buffer/memory management, MRR interface specifies that 
   - SQL layer provides multi_range_read_init() with buffer of certain size.
   - MRR implementation may use (i.e. have at its disposal till the end of 
     the MRR scan) all of the buffer, or return the unused end of the buffer 
     to SQL layer.

  DS-MRR needs buffer in order to accumulate and sort rowids and/or keys. When
  we need to accumulate/sort only keys (or only rowids), it is fairly trivial.

  When we need to accumulate/sort both keys and rowids, efficient buffer use
  gets complicated. We need to:
   - First, accumulate keys and sort them
   - Then use the keys (smaller values go first) to obtain rowids. A key is not
     needed after we've got matching rowids for it.
   - Make sure that rowids are accumulated at the front of the buffer, so that we
     can return the end part of the buffer to SQL layer, should there be too
     few rowid values to occupy the buffer.

  All of these goals are achieved by using the following scheme:

     |                    |   We get an empty buffer from SQL layer.   

     |                  *-|    
     |               *----|   First, we fill the buffer with keys. Key_buffer
     |            *-------|   part grows from end of the buffer space to start
     |         *----------|   (In this picture, the buffer is big enough to
     |      *-------------|    accomodate all keys and even have some space left)

     |      *=============|   We want to do key-ordered index scan, so we sort
                              the keys

     |-x      *===========|   Then we use the keys get rowids. Rowids are 
     |----x      *========|   stored from start of buffer space towards the end.
     |--------x     *=====|   The part of the buffer occupied with keys
     |------------x   *===|   gradually frees up space for rowids. In this
     |--------------x   *=|   picture we run out of keys before we've ran out
     |----------------x   |   of buffer space (it can be other way as well).

     |================x   |   Then we sort the rowids.
                     
     |                |~~~|   The unused part of the buffer is at the end, so
                              we can return it to the SQL layer.

     |================*       Sorted rowids are then used to read table records 
                              in disk order

*/

class DsMrr_impl : public Buffer_manager
{
public:
  typedef void (handler::*range_check_toggle_func_t)(bool on);

  DsMrr_impl()
    : h2(NULL) {};
  
  void init(handler *h_arg, TABLE *table_arg)
  {
    h= h_arg; 
    table= table_arg;
  }
  int dsmrr_init(handler *h, RANGE_SEQ_IF *seq_funcs, void *seq_init_param, 
                 uint n_ranges, uint mode, HANDLER_BUFFER *buf);
  void dsmrr_close();
  int dsmrr_next(char **range_info);

  ha_rows dsmrr_info(uint keyno, uint n_ranges, uint keys, uint key_parts, 
                     uint *bufsz, uint *flags, COST_VECT *cost);

  ha_rows dsmrr_info_const(uint keyno, RANGE_SEQ_IF *seq, 
                            void *seq_init_param, uint n_ranges, uint *bufsz,
                            uint *flags, COST_VECT *cost);
private:
  /* Buffer to store (key, range_id) pairs */
  Lifo_buffer *key_buffer;

  /*
    The "owner" handler object (the one that is expected to "own" this object
    and call its functions).
  */
  handler *h;
  TABLE *table; /* Always equal to h->table */

  /*
    Secondary handler object. (created when needed, we need it when we need 
    to run both index scan and rnd_pos() scan at the same time)
  */
  handler *h2;
  
  /** Properties of current MRR scan **/

  uint keyno; /* index we're running the scan on */
  /* TRUE <=> need range association, buffers hold {rowid, range_id} pairs */
  bool is_mrr_assoc;
  /* TRUE <=> sort the keys before making index lookups */
  //bool do_sort_keys;
  /* TRUE <=> sort rowids and use rnd_pos() to get and return full records */
  //bool do_rndpos_scan;

  Mrr_reader_factory strategy_factory;
  Mrr_reader *strategy;
  Mrr_index_reader *index_strategy;

  /* The whole buffer space that we're using */
  uchar *full_buf;
  uchar *full_buf_end;
  
  /* 
    When using both rowid and key buffers: the boundary between key and rowid
    parts of the buffer. This is the "original" value, actual memory ranges 
    used by key and rowid parts may be different because of dynamic space 
    reallocation between them.
  */
  uchar *rowid_buffer_end;
 
  /** Index scaning and key buffer-related members **/

  /*
    One of the following two is used for key buffer: forward is used when 
    we only need key buffer, backward is used when we need both key and rowid
    buffers.
  */
  Forward_lifo_buffer forward_key_buf;
  Backward_lifo_buffer backward_key_buf;

  Forward_lifo_buffer rowid_buffer;
  
  /* = key_size_in_keybuf [ + sizeof(range_assoc_info) ] */
  //uint key_buff_elem_size_;
  
  /** rnd_pos() scan and rowid buffer-related members **/

  /*
    Buffer to store (rowid, range_id) pairs, or just rowids if 
    is_mrr_assoc==FALSE
  */
  //Forward_lifo_buffer rowid_buffer;
  
  bool choose_mrr_impl(uint keyno, ha_rows rows, uint *flags, uint *bufsz, 
                       COST_VECT *cost);
  bool get_disk_sweep_mrr_cost(uint keynr, ha_rows rows, uint flags, 
                               uint *buffer_size, COST_VECT *cost);
  bool check_cpk_scan(THD *thd, uint keyno, uint mrr_flags);

  void reallocate_buffer_space();
  
  /* Buffer_manager implementation */
  void setup_buffer_sizes(uint key_size_in_keybuf, key_part_map key_tuple_map);
  void reset_buffer_sizes();
  Lifo_buffer* get_key_buffer() { return key_buffer; }

  friend class Key_value_records_iterator;
  friend class Mrr_ordered_index_reader;
  friend class Mrr_ordered_rndpos_reader;

  int  setup_two_handlers();
  void close_second_handler();
};

/**
  @} (end of group DS-MRR declarations)
*/

