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

/**
  Iterator over (record, range_id) pairs that match given key value.
  
  We may need to scan multiple (key_val, range_id) pairs with the same 
  key value. A key value may have multiple matching records, so we'll need to
  produce a cross-product of sets of matching records and range_id-s.
*/

class Key_value_records_iterator
{
  /* Scan parameters */
  DsMrr_impl *dsmrr;
  Lifo_buffer_iterator identical_key_it;
  uchar *last_identical_key_ptr;
  bool get_next_row;
public:
  bool init(DsMrr_impl *dsmrr);

  /*
    Get next (key_val, range_id) pair.
  */
  int get_next();

  void close();
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
*/

class DsMrr_impl
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
  bool use_default_impl; /* TRUE <=> shortcut all calls to default MRR impl */
  /* TRUE <=> need range association, buffers hold {rowid, range_id} pairs */
  bool is_mrr_assoc;
  /* TRUE <=> sort the keys before making index lookups */
  bool do_sort_keys;
  /* TRUE <=> sort rowids and use rnd_pos() to get and return full records */
  bool do_rndpos_scan;

  /*
    (if do_sort_keys==TRUE) don't copy key values, use pointers to them 
    instead.
  */
  bool use_key_pointers;


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

  /* TRUE <=> We can get at most one index tuple for a lookup key */
  bool index_ranges_unique;

  /* TRUE<=> we're in a middle of enumerating records for a key range */
  //bool in_index_range;
  
  /*
    One of the following two is used for key buffer: forward is used when 
    we only need key buffer, backward is used when we need both key and rowid
    buffers.
  */
  Forward_lifo_buffer forward_key_buf;
  Backward_lifo_buffer backward_key_buf;

  /* Buffer to store (key, range_id) pairs */
  Lifo_buffer *key_buffer;
  
  /* Index scan state */
  bool scanning_key_val_iter;
  /* 
    TRUE <=> we've got index tuples/rowids for all keys (need this flag because 
    we may have a situation where we've read everything from the key buffer but 
    haven't finished with getting index tuples for the last key)
  */
  bool index_scan_eof;  
  Key_value_records_iterator kv_it;
  
  /* key_buffer.read() reads to here */
  uchar *cur_index_tuple;

  /* if in_index_range==TRUE: range_id of the range we're enumerating */
  char *cur_range_info;

  /* Initially FALSE, becomes TRUE when we've set key_tuple_xxx members */
  bool know_key_tuple_params;
  uint         key_tuple_length; /* Length of index lookup tuple, in bytes */
  key_part_map key_tuple_map;    /* keyparts used in index lookup tuples */

  /*
    This is 
      = key_tuple_length   if we copy keys to buffer
      = sizeof(void*)      if we're using pointers to materialized keys.
  */
  uint key_size_in_keybuf;
  
  /* = key_size_in_keybuf [ + sizeof(range_assoc_info) ] */
  uint key_buff_elem_size;
  
  /** rnd_pos() scan and rowid buffer-related members **/

  /*
    Buffer to store (rowid, range_id) pairs, or just rowids if 
    is_mrr_assoc==FALSE
  */
  Forward_lifo_buffer rowid_buffer;
  
  /* rowid_buffer.read() will set the following:  */
  uchar *rowid;
  uchar *rowids_range_id;

  uchar *last_identical_rowid;

  bool dsmrr_eof; /* TRUE <=> We have reached EOF when reading index tuples */
  
  /* = h->ref_length  [ + sizeof(range_assoc_info) ] */
  uint rowid_buff_elem_size;
  
  bool choose_mrr_impl(uint keyno, ha_rows rows, uint *flags, uint *bufsz, 
                       COST_VECT *cost);
  bool get_disk_sweep_mrr_cost(uint keynr, ha_rows rows, uint flags, 
                               uint *buffer_size, COST_VECT *cost);
  bool check_cpk_scan(THD *thd, uint keyno, uint mrr_flags);
  static int key_tuple_cmp(void* arg, uchar* key1, uchar* key2);
  static int key_tuple_cmp_reverse(void* arg, uchar* key1, uchar* key2);
  int dsmrr_fill_rowid_buffer();
  void dsmrr_fill_key_buffer();
  int dsmrr_next_from_index(char **range_info);

  void setup_buffer_sizes(key_range *sample_key);
  void reallocate_buffer_space();
  
  static range_seq_t key_buf_seq_init(void *init_param, uint n_ranges, uint flags);
  static uint key_buf_seq_next(range_seq_t rseq, KEY_MULTI_RANGE *range);
  friend class Key_value_records_iterator;
};

/**
  @} (end of group DS-MRR declarations)
*/

