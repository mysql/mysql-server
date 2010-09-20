/*
  This file contains declarations for Disk-Sweep MultiRangeRead (DS-MRR) 
  implementation
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


/*
  An in-memory buffer used by DS-MRR implementation. 
  - The buffer contains fixed-size elements. The elements are either atomic
    byte sequences or pairs.
  - The buffer resides in memory provided by the user. It is possible to
     = dynamically (ie. between write operations) add ajacent memory space to
       the buffer
     = dynamically remove unused space from the buffer.
  - Buffer can be set to be either "forward" or "backward". 

  The intent of the last two properties is to allow to have two buffers on
  adjacent memory space, one is being read from (and so its space shrinks)
  while the other is being written to (and so it needs more and more space).

  Illustration of forward buffer operation:

                         +-- next read will read from here
                         |
                         |               +-- next write will write to here
                         v               v
        *--------------*===============*----------------*
        |       ^      |          ^    |                |
        |       |      read_pos   |    write_pos        |
        start   |                 |                     end
                |                 |            
              usused space         user data
  
  For reverse buffer, start/end have the same meaning, but reading and 
  writing is done from end to start.
*/

class SimpleBuffer
{
public:

  enum enum_direction {
    BACKWARD=-1, /* buffer is filled/read from bigger to smaller memory addresses */
    FORWARD=1  /* buffer is filled/read from smaller to bigger memory addresses */
  };

private:
  enum_direction direction;

  uchar *start; /* points to start of buffer space */
  uchar *end;   /* points to just beyond the end of buffer space */
  /*
    Forward buffer: points to the start of the data that will be read next
    Backward buffer: points to just beyond the end of the data that will be 
    read next.
  */
  uchar *read_pos;
  /*
    Forward buffer: points to just after the end of the used area.
    Backward buffer: points to the start of used area.
  */
  uchar *write_pos;

  /* 
    Data to be written. write() call will assume that (*write_ptr1) points to 
    write_size1 bytes of data to be written.
    If write_ptr2!=NULL then the buffer stores pairs, and (*write_ptr2) points
    to write_size2 bytes of data that form the second component.
  */
  uchar **write_ptr1;
  size_t write_size1;
  uchar **write_ptr2;
  size_t write_size2;

  /*
    read() will do reading by storing pointer to read data into *read_ptr1 (if
    the buffer stores atomic elements), or into {*read_ptr1, *read_ptr2} (if
    the buffer stores pairs).
  */
  //TODO if write_size1 == read_size1 why have two variables??
  uchar **read_ptr1;
  size_t read_size1;
  uchar **read_ptr2;
  size_t read_size2;

public:
  /* Write-mode functions */
  void setup_writing(uchar **data1, size_t len1, 
                     uchar **data2, size_t len2);
  void reset_for_writing();
  bool can_write();
  void write();

  /* Read-mode functions */
  bool is_empty() { return used_size() == 0; }
  void setup_reading(uchar **data1, size_t len1, 
                     uchar **data2, size_t len2);
  bool read();

  /* Misc functions */
  void sort(qsort2_cmp cmp_func, void *cmp_func_arg);
  bool is_reverse() { return direction == BACKWARD; }
  uchar *end_of_space();

  /* Buffer space control functions */
  void set_buffer_space(uchar *start_arg, uchar *end_arg, enum_direction direction_arg) 
  {
    start= start_arg;
    end= end_arg;
    direction= direction_arg;
    TRASH(start, end - start);
    reset_for_writing();
  }

  /*
    Stop using/return the unneded space (the one that we have already wrote 
    to read from).
  */
  void remove_unused_space(uchar **unused_start, uchar **unused_end)
  {
    if (direction == 1)
    {
      *unused_start= start;
      *unused_end= read_pos;
      start= read_pos;
    }
    else
    {
      *unused_start= read_pos;
      *unused_end= end;
      end= read_pos;
    }
  }

  void flip()
  {
    uchar *tmp= read_pos;
    read_pos= write_pos;
    write_pos= tmp;
    direction= (direction == FORWARD)? BACKWARD: FORWARD;
  }

  void grow(uchar *unused_start, uchar *unused_end)
  {
    /*
      Passed memory area can be meaningfully used for growing the buffer if:
      - it is adjacent to buffer space we're using
      - it is on the end towards which we grow.
    */
    DBUG_ASSERT(unused_end >= unused_start);
    TRASH(unused_start, unused_end - unused_start);
    if (direction == 1 && end == unused_start)
    {
      end= unused_end;
    }
    else if (direction == -1 && start == unused_end)
    {
      start= unused_start;
    }
    else
      DBUG_ASSERT(0); /* Attempt to grow buffer in wrong direction */
  }
  
  /*
    An iterator to do look at what we're about to read from the buffer without
    actually reading it.
  */
  class PeekIterator
  {
    SimpleBuffer *buf; /* The buffer we're iterating over*/
    /*
      if buf->direction==FORWARD  : pointer to what to return next
      if buf->direction==BACKWARD : pointer to the end of what is to be 
                                   returned next
    */
    uchar *pos;
  public:
    /* 
      Initialize the iterator. After intiialization, the first read_next() call
      will read what buf_arg->read() would read.
    */
    void init(SimpleBuffer *buf_arg)
    {
      buf= buf_arg;
      pos= buf->read_pos;
    }
    
    /*
      Read the next value. The calling convention is the same as buf->read()
      has.

      RETURN
        FALSE - Ok
        TRUE  - EOF, reached the end of the buffer
    */
    bool read_next()
    {
      /* 
        Always read the first component first (if the buffer is backwards, we
        have written the second component first).
      */
      uchar *res;
      if ((res= get_next(buf->read_size1)))
      {
        *(buf->read_ptr1)= res;
        if (buf->read_ptr2)
          *buf->read_ptr2= get_next(buf->read_size2);
        return FALSE;
      }
      return TRUE; /* EOF */
    }
  private:
    /* Return pointer to next chunk of nbytes bytes and avance over it */
    uchar *get_next(size_t nbytes)
    {
      if (buf->direction == 1)
      {
        if (pos + nbytes > buf->write_pos)
          return NULL;
        uchar *res= pos;
        pos += nbytes;
        return res;
      }
      else
      {
        if (pos - nbytes < buf->write_pos)
          return NULL;
        pos -= nbytes;
        return pos;
      }
    }
  };

private:
  bool have_space_for(size_t bytes);
  /* Return pointer to start of the memory area that is occupied by the data */
  uchar *used_area() { return (direction == FORWARD)? read_pos : write_pos; }
  size_t used_size();

  void write(const uchar *data, size_t bytes);
  uchar *read(size_t bytes);
  bool have_data(size_t bytes);
};


/*
  DS-MRR implementation for one table. Create/use one object of this class for
  each ha_{myisam/innobase/etc} object. That object will be further referred to
  as "the handler"

  DsMrr_impl has the following execution strategies:
   S1. Bypass DS-MRR, pass all calls to default MRR implementation (i.e. to
      MRR-to-non-MRR calls converter)
   S2. Sort Keys
   S3. Sort Rowids

  psergey-TODO.

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
    The "owner" handler object (the one that is expected to "own" this object
    and call its functions).
  */
  handler *h;
  TABLE *table; /* Always equal to h->table */

  /*
    Secondary handler object. (created when needed, we need it when we need 
    to run both index scan and rnd_pos() at the same time)
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
    When using both rowid and key buffers: the bound between key and rowid
    parts of the buffer. This is the "original" value, actual memory ranges 
    used by key and rowid parts may be different because of dynamic space 
    reallocation between them.
  */
  uchar *rowid_buffer_end;
 

  /** Index scaning and key buffer-related members **/
  
  /* TRUE <=> We can get at most one index tuple for a lookup key */
  bool index_ranges_unique;

  /* TRUE<=> we're in a middle of enumerating records for a key range */
  bool in_index_range;
  
  /* Buffer to store (key, range_id) pairs */
  SimpleBuffer key_buffer;
   
  /* key_buffer.read() reads */
  uchar *cur_index_tuple;

  /* if in_index_range==TRUE: range_id of the range we're enumerating */
  char *cur_range_info;

  /* 
    TRUE <=> we've got index tuples/rowids for all keys (need this flag because 
    we may have a situation where we've read everything from the key buffer but 
    haven't finished with getting index tuples for the last key)
  */
  bool key_eof;

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
  
  /* 
    TRUE <=> we're doing key-ordered index scan and right now several
    subsequent key values are the same as the one we've already retrieved and
    returned index tuple for.
  */
  bool in_identical_keys_range;

  /* range_id of the first of the identical keys */
  char *first_identical_range_info;

  /* Pointer to the last of the identical key values */
  uchar *last_identical_key_ptr;

  /* 
    key_buffer iterator for walking the identical key range (we need to
    enumerate the set of (identical_key, range_id) pairs multiple times,
    and do that by walking from current buffer read position until we get
    last_identical_key_ptr.
  */
  SimpleBuffer::PeekIterator identical_key_it;


  /** rnd_pos() scan and rowid buffer-related members **/

  /*
    Buffer to store (rowid, range_id) pairs, or just rowids if 
    is_mrr_assoc==FALSE
  */
  SimpleBuffer rowid_buffer;
  
  /* rowid_buffer.read() will set the following:  */
  uchar *rowid;
  uchar *rowids_range_id;
  
  /*
    not-NULL: we're traversing a group of (rowid, range_id) pairs with
              identical rowid values, and this is the pointer to the last one.
    NULL: we're not in the group of indentical rowids.
  */
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
  int dsmrr_fill_rowid_buffer();
  void dsmrr_fill_key_buffer();
  int dsmrr_next_from_index(char **range_info);

  void setup_buffer_sizes(key_range *sample_key);
  void reallocate_buffer_space();

  static range_seq_t key_buf_seq_init(void *init_param, uint n_ranges, uint flags);
  static uint key_buf_seq_next(range_seq_t rseq, KEY_MULTI_RANGE *range);
};


