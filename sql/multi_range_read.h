/*
  This file contains declarations for 
   - Disk-Sweep MultiRangeRead (DS-MRR) implementation
*/

/**
  A Disk-Sweep MRR interface implementation

  This implementation makes range (and, in the future, 'ref') scans to read
  table rows in disk sweeps. 
  
  Currently it is used by MyISAM and InnoDB. Potentially it can be used with
  any table handler that has non-clustered indexes and on-disk rows.
*/

class DsMrr_impl
{
public:
  typedef void (handler::*range_check_toggle_func_t)(bool on);

  DsMrr_impl()
    : h2(NULL) {};
  
  /*
    The "owner" handler object (the one that calls dsmrr_XXX functions.
    It is used to retrieve full table rows by calling rnd_pos().
  */
  handler *h;
  TABLE *table; /* Always equal to h->table */
private:
  /* Secondary handler object.  It is used for scanning the index */
  handler *h2;

  /* Buffer to store rowids, or (rowid, range_id) pairs */
  uchar *rowids_buf;
  uchar *rowids_buf_cur;   /* Current position when reading/writing */
  uchar *rowids_buf_last;  /* When reading: end of used buffer space */
  uchar *rowids_buf_end;   /* End of the buffer */

  bool dsmrr_eof; /* TRUE <=> We have reached EOF when reading index tuples */

  /* TRUE <=> need range association, buffer holds {rowid, range_id} pairs */
  bool is_mrr_assoc;

  bool use_default_impl; /* TRUE <=> shortcut all calls to default MRR impl */
public:
  void init(handler *h_arg, TABLE *table_arg)
  {
    h= h_arg; 
    table= table_arg;
  }
  int dsmrr_init(handler *h, RANGE_SEQ_IF *seq_funcs, void *seq_init_param, 
                 uint n_ranges, uint mode, HANDLER_BUFFER *buf);
  void dsmrr_close();
  int dsmrr_fill_buffer();
  int dsmrr_next(char **range_info);

  ha_rows dsmrr_info(uint keyno, uint n_ranges, uint keys, uint *bufsz,
                     uint *flags, COST_VECT *cost);

  ha_rows dsmrr_info_const(uint keyno, RANGE_SEQ_IF *seq, 
                            void *seq_init_param, uint n_ranges, uint *bufsz,
                            uint *flags, COST_VECT *cost);
private:
  bool choose_mrr_impl(uint keyno, ha_rows rows, uint *flags, uint *bufsz, 
                       COST_VECT *cost);
  bool get_disk_sweep_mrr_cost(uint keynr, ha_rows rows, uint flags, 
                               uint *buffer_size, COST_VECT *cost);
};

