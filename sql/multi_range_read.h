/*
  This file contains declarations for Disk-Sweep MultiRangeRead (DS-MRR) 
  implementation
*/

/**
  A Disk-Sweep implementation of MRR Interface (DS-MRR for short)

  This is a "plugin"(*) for storage engines that allows make index scans 
  read table rows in rowid order. For disk-based storage engines, this is
  faster than reading table rows in whatever-SQL-layer-makes-calls-in order.

  (*) - only conceptually. No dynamic loading or binary compatibility of any
        kind.

  General scheme of things:
   
      SQL Layer code
       |   |   |
      -v---v---v---- handler->multi_range_read_XXX() function calls
       |   |   |
      ____________________________________
     / DS-MRR module                      \
     |  (scan indexes, order rowids, do    |
     |   full record reads in rowid order) |
     \____________________________________/
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


/*
  DS-MRR implementation for one table. Create/use one object of this class for
  each ha_{myisam/innobase/etc} object. That object will be further referred to
  as "the handler"

  There are actually three strategies
   S1. Bypass DS-MRR, pass all calls to default implementation (i.e. to
      MRR-to-non-MRR calls converter)
   S2. Regular DS-MRR 
   S3. DS-MRR/CPK for doing scans on clustered primary keys.

  S1 is used for cases which DS-MRR is unable to handle for some reason.

  S2 is the actual DS-MRR. The basic algorithm is as follows:
    1. Scan the index (and only index, that is, with HA_EXTRA_KEYREAD on) and 
        fill the buffer with {rowid, range_id} pairs
    2. Sort the buffer by rowid
    3. for each {rowid, range_id} pair in the buffer
         get record by rowid and return the {record, range_id} pair
    4. Repeat the above steps until we've exhausted the list of ranges we're
       scanning.

  S3 is the variant of DS-MRR for use with clustered primary keys (or any
  clustered index). The idea is that in clustered index it is sufficient to 
  access the index in index order, and we don't need an intermediate steps to
  get rowid (like step #1 in S2).

   DS-MRR/CPK's basic algorithm is as follows:
    1. Collect a number of ranges (=lookup keys)
    2. Sort them so that they follow in index order.
    3. for each {lookup_key, range_id} pair in the buffer 
       get record(s) matching the lookup key and return {record, range_id} pairs
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
    The "owner" handler object (the one that calls dsmrr_XXX functions.
    It is used to retrieve full table rows by calling rnd_pos().
  */
  handler *h;
  TABLE *table; /* Always equal to h->table */

  /* Secondary handler object.  It is used for scanning the index */
  handler *h2;

  /* Buffer to store rowids, or (rowid, range_id) pairs */
  uchar *mrr_buf;
  uchar *mrr_buf_cur;   /* Current position when reading/writing */
  uchar *mrr_buf_last;  /* When reading: end of used buffer space */
  uchar *mrr_buf_end;   /* End of the buffer */

  bool dsmrr_eof; /* TRUE <=> We have reached EOF when reading index tuples */

  /* TRUE <=> need range association, buffer holds {rowid, range_id} pairs */
  bool is_mrr_assoc;

  bool use_default_impl; /* TRUE <=> shortcut all calls to default MRR impl */

  bool doing_cpk_scan; /* TRUE <=> DS-MRR/CPK variant is used */

  /** DS-MRR/CPK variables start */

  /* Length of lookup tuple being used, in bytes */
  uint cpk_tuple_length;
  /*
    TRUE <=> We're scanning on a full primary key (and not on prefix), and so 
    can get max. one match for each key 
  */
  bool cpk_is_unique_scan;
  /* TRUE<=> we're in a middle of enumerating records from a range */ 
  bool cpk_have_range;
  /* Valid if cpk_have_range==TRUE: range_id of the range we're enumerating */
  char *cpk_saved_range_info;

  bool choose_mrr_impl(uint keyno, ha_rows rows, uint *flags, uint *bufsz, 
                       COST_VECT *cost);
  bool get_disk_sweep_mrr_cost(uint keynr, ha_rows rows, uint flags, 
                               uint *buffer_size, COST_VECT *cost);
  bool check_cpk_scan(uint keyno, uint mrr_flags);
  static int key_tuple_cmp(void* arg, uchar* key1, uchar* key2);
  int dsmrr_fill_buffer();
  void dsmrr_fill_buffer_cpk();
  int dsmrr_next_cpk(char **range_info);
};

