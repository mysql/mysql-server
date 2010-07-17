#include "mysql_priv.h"
#include <my_bit.h>
#include "sql_select.h"

/****************************************************************************
 * Default MRR implementation (MRR to non-MRR converter)
 ***************************************************************************/

/**
  Get cost and other information about MRR scan over a known list of ranges

  Calculate estimated cost and other information about an MRR scan for given
  sequence of ranges.

  @param keyno           Index number
  @param seq             Range sequence to be traversed
  @param seq_init_param  First parameter for seq->init()
  @param n_ranges_arg    Number of ranges in the sequence, or 0 if the caller
                         can't efficiently determine it
  @param bufsz    INOUT  IN:  Size of the buffer available for use
                         OUT: Size of the buffer that is expected to be actually
                              used, or 0 if buffer is not needed.
  @param flags    INOUT  A combination of HA_MRR_* flags
  @param cost     OUT    Estimated cost of MRR access

  @note
    This method (or an overriding one in a derived class) must check for
    thd->killed and return HA_POS_ERROR if it is not zero. This is required
    for a user to be able to interrupt the calculation by killing the
    connection/query.

  @retval
    HA_POS_ERROR  Error or the engine is unable to perform the requested
                  scan. Values of OUT parameters are undefined.
  @retval
    other         OK, *cost contains cost of the scan, *bufsz and *flags
                  contain scan parameters.
*/

ha_rows 
handler::multi_range_read_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                     void *seq_init_param, uint n_ranges_arg,
                                     uint *bufsz, uint *flags, COST_VECT *cost)
{
  KEY_MULTI_RANGE range;
  range_seq_t seq_it;
  ha_rows rows, total_rows= 0;
  uint n_ranges=0;
  THD *thd= current_thd;
  
  /* Default MRR implementation doesn't need buffer */
  *bufsz= 0;

  seq_it= seq->init(seq_init_param, n_ranges, *flags);
  while (!seq->next(seq_it, &range))
  {
    if (unlikely(thd->killed != 0))
      return HA_POS_ERROR;
    
    n_ranges++;
    key_range *min_endp, *max_endp;
    if (range.range_flag & GEOM_FLAG)
    {
      /* In this case tmp_min_flag contains the handler-read-function */
      range.start_key.flag= (ha_rkey_function) (range.range_flag ^ GEOM_FLAG);
      min_endp= &range.start_key;
      max_endp= NULL;
    }
    else
    {
      min_endp= range.start_key.length? &range.start_key : NULL;
      max_endp= range.end_key.length? &range.end_key : NULL;
    }
    if ((range.range_flag & UNIQUE_RANGE) && !(range.range_flag & NULL_RANGE))
      rows= 1; /* there can be at most one row */
    else
    {
      if (HA_POS_ERROR == (rows= this->records_in_range(keyno, min_endp, 
                                                        max_endp)))
      {
        /* Can't scan one range => can't do MRR scan at all */
        total_rows= HA_POS_ERROR;
        break;
      }
    }
    total_rows += rows;
  }
  
  if (total_rows != HA_POS_ERROR)
  {
    /* The following calculation is the same as in multi_range_read_info(): */
    *flags |= HA_MRR_USE_DEFAULT_IMPL;
    cost->zero();
    cost->avg_io_cost= 1; /* assume random seeks */
    if ((*flags & HA_MRR_INDEX_ONLY) && total_rows > 2)
      cost->io_count= keyread_read_time(keyno, n_ranges, (uint)total_rows);
    else
      cost->io_count= read_time(keyno, n_ranges, total_rows);
    cost->cpu_cost= (double) total_rows / TIME_FOR_COMPARE + 0.01;
  }
  return total_rows;
}


/**
  Get cost and other information about MRR scan over some sequence of ranges

  Calculate estimated cost and other information about an MRR scan for some
  sequence of ranges.

  The ranges themselves will be known only at execution phase. When this
  function is called we only know number of ranges and a (rough) E(#records)
  within those ranges.

  Currently this function is only called for "n-keypart singlepoint" ranges,
  i.e. each range is "keypart1=someconst1 AND ... AND keypartN=someconstN"

  The flags parameter is a combination of those flags: HA_MRR_SORTED,
  HA_MRR_INDEX_ONLY, HA_MRR_NO_ASSOCIATION, HA_MRR_LIMITS.

  @param keyno           Index number
  @param n_ranges        Estimated number of ranges (i.e. intervals) in the
                         range sequence.
  @param n_rows          Estimated total number of records contained within all
                         of the ranges
  @param bufsz    INOUT  IN:  Size of the buffer available for use
                         OUT: Size of the buffer that will be actually used, or
                              0 if buffer is not needed.
  @param flags    INOUT  A combination of HA_MRR_* flags
  @param cost     OUT    Estimated cost of MRR access

  @retval
    0     OK, *cost contains cost of the scan, *bufsz and *flags contain scan
          parameters.
  @retval
    other Error or can't perform the requested scan
*/

ha_rows handler::multi_range_read_info(uint keyno, uint n_ranges, uint n_rows,
                                       uint key_parts, uint *bufsz, 
                                       uint *flags, COST_VECT *cost)
{
  /* 
    Currently we expect this function to be called only in preparation of scan
    with HA_MRR_SINGLE_POINT property.
  */
  DBUG_ASSERT(*flags | HA_MRR_SINGLE_POINT);

  *bufsz= 0; /* Default implementation doesn't need a buffer */
  *flags |= HA_MRR_USE_DEFAULT_IMPL;

  cost->zero();
  cost->avg_io_cost= 1; /* assume random seeks */

  /* Produce the same cost as non-MRR code does */
  if (*flags & HA_MRR_INDEX_ONLY)
    cost->io_count= keyread_read_time(keyno, n_ranges, n_rows);
  else
    cost->io_count= read_time(keyno, n_ranges, n_rows);
  return 0;
}


/**
  Initialize the MRR scan

  Initialize the MRR scan. This function may do heavyweight scan 
  initialization like row prefetching/sorting/etc (NOTE: but better not do
  it here as we may not need it, e.g. if we never satisfy WHERE clause on
  previous tables. For many implementations it would be natural to do such
  initializations in the first multi_read_range_next() call)

  mode is a combination of the following flags: HA_MRR_SORTED,
  HA_MRR_INDEX_ONLY, HA_MRR_NO_ASSOCIATION 

  @param seq             Range sequence to be traversed
  @param seq_init_param  First parameter for seq->init()
  @param n_ranges        Number of ranges in the sequence
  @param mode            Flags, see the description section for the details
  @param buf             INOUT: memory buffer to be used

  @note
    One must have called index_init() before calling this function. Several
    multi_range_read_init() calls may be made in course of one query.

    Until WL#2623 is done (see its text, section 3.2), the following will 
    also hold:
    The caller will guarantee that if "seq->init == mrr_ranges_array_init"
    then seq_init_param is an array of n_ranges KEY_MULTI_RANGE structures.
    This property will only be used by NDB handler until WL#2623 is done.
     
    Buffer memory management is done according to the following scenario:
    The caller allocates the buffer and provides it to the callee by filling
    the members of HANDLER_BUFFER structure.
    The callee consumes all or some fraction of the provided buffer space, and
    sets the HANDLER_BUFFER members accordingly.
    The callee may use the buffer memory until the next multi_range_read_init()
    call is made, all records have been read, or until index_end() call is
    made, whichever comes first.

  @retval 0  OK
  @retval 1  Error
*/

int
handler::multi_range_read_init(RANGE_SEQ_IF *seq_funcs, void *seq_init_param,
                               uint n_ranges, uint mode, HANDLER_BUFFER *buf)
{
  DBUG_ENTER("handler::multi_range_read_init");
  mrr_iter= seq_funcs->init(seq_init_param, n_ranges, mode);
  mrr_funcs= *seq_funcs;
  mrr_is_output_sorted= test(mode & HA_MRR_SORTED);
  mrr_have_range= FALSE;
  DBUG_RETURN(0);
}


/**
  Get next record in MRR scan

  Default MRR implementation: read the next record

  @param range_info  OUT  Undefined if HA_MRR_NO_ASSOCIATION flag is in effect
                          Otherwise, the opaque value associated with the range
                          that contains the returned record.

  @retval 0      OK
  @retval other  Error code
*/

int handler::multi_range_read_next(char **range_info)
{
  int UNINIT_VAR(result);
  int range_res;
  DBUG_ENTER("handler::multi_range_read_next");

  if (!mrr_have_range)
  {
    mrr_have_range= TRUE;
    goto start;
  }

  do
  {
    /* Save a call if there can be only one row in range. */
    if (mrr_cur_range.range_flag != (UNIQUE_RANGE | EQ_RANGE))
    {
      result= read_range_next();
      /* On success or non-EOF errors jump to the end. */
      if (result != HA_ERR_END_OF_FILE)
        break;
    }
    else
    {
      if (was_semi_consistent_read())
        goto scan_it_again;
      /*
        We need to set this for the last range only, but checking this
        condition is more expensive than just setting the result code.
      */
      result= HA_ERR_END_OF_FILE;
    }

start:
    /* Try the next range(s) until one matches a record. */
    while (!(range_res= mrr_funcs.next(mrr_iter, &mrr_cur_range)))
    {
scan_it_again:
      result= read_range_first(mrr_cur_range.start_key.keypart_map ?
                                 &mrr_cur_range.start_key : 0,
                               mrr_cur_range.end_key.keypart_map ?
                                 &mrr_cur_range.end_key : 0,
                               test(mrr_cur_range.range_flag & EQ_RANGE),
                               mrr_is_output_sorted);
      if (result != HA_ERR_END_OF_FILE)
        break;
    }
  }
  while ((result == HA_ERR_END_OF_FILE) && !range_res);

  *range_info= mrr_cur_range.ptr;
  DBUG_PRINT("exit",("handler::multi_range_read_next result %d", result));
  DBUG_RETURN(result);
}

/****************************************************************************
 * DS-MRR implementation 
 ***************************************************************************/

/**
  DS-MRR: Initialize and start MRR scan

  Initialize and start the MRR scan. Depending on the mode parameter, this
  may use default or DS-MRR implementation.

  @param h               Table handler to be used
  @param key             Index to be used
  @param seq_funcs       Interval sequence enumeration functions
  @param seq_init_param  Interval sequence enumeration parameter
  @param n_ranges        Number of ranges in the sequence.
  @param mode            HA_MRR_* modes to use
  @param buf             INOUT Buffer to use

  @retval 0     Ok, Scan started.
  @retval other Error
*/

int DsMrr_impl::dsmrr_init(handler *h_arg, RANGE_SEQ_IF *seq_funcs, 
                           void *seq_init_param, uint n_ranges, uint mode,
                           HANDLER_BUFFER *buf)
{
  uint elem_size;
  Item *pushed_cond= NULL;
  handler *new_h2= 0;
  DBUG_ENTER("DsMrr_impl::dsmrr_init");

  /*
    index_merge may invoke a scan on an object for which dsmrr_info[_const]
    has not been called, so set the owner handler here as well.
  */
  h= h_arg;
  if (mode & HA_MRR_USE_DEFAULT_IMPL || mode & HA_MRR_SORTED)
  {
    use_default_impl= TRUE;
    const int retval=
      h->handler::multi_range_read_init(seq_funcs, seq_init_param, n_ranges, 
                                        mode, buf);
    DBUG_RETURN(retval);
  }
  use_default_impl= FALSE;
  is_mrr_assoc= !test(mode & HA_MRR_NO_ASSOCIATION);

  // psergey2: split the buffer:
  /*

  psergey2-note: we can't split the buffer here because we don't know how key
  length. we'll only be able to do it when we've got the first range.

  if ((mrr_flags & HA_MRR_SINGLE_POINT) && 
       optimizer_flag(thd, OPTIMIZER_SWITCH_MRR_SORT_KEYS))
  {
    do_sort_keys= TRUE; // will use key buffer to sort keys;
    bool use_key_pointers= test(mrr_flags & HA_MRR_MATERIALIZED_KEYS);
  }
  
  do_rowid_fetch= FALSE;
  if (!doing_cpk_scan && !index_only_read)
  {
    do_rowid_fetch= TRUE; //will use rowid buffer to store/sort rowids, etc
  }


  if (do_sort_keys && do_rowid_fetch)
  {
    split buffer space proportionally
  }
  else
  {
    // give all space to one buffer
    if (do_sort_keys)
    {
      //sort_buffer_start= ...;
    }
    else 
    {
      DBUG_ASSERT(do_rowid_fetch);
      //rowid_buffer_start= ...;
    }
  }
  */
  mrr_buf= buf->buffer;
  mrr_buf_end= buf->buffer_end;

  if (is_mrr_assoc)
    status_var_increment(table->in_use->status_var.ha_multi_range_read_init_count);

  if ((doing_cpk_scan= check_cpk_scan(h->active_index, mode)))
  {
    /* It's a DS-MRR/CPK scan */
    cpk_tuple_length= 0; /* dummy value telling it needs to be inited */
    cpk_have_range= FALSE;
    h->mrr_iter= seq_funcs->init(seq_init_param, n_ranges, mode);
    h->mrr_funcs= *seq_funcs;
    dsmrr_fill_key_buffer();
    
    if (dsmrr_eof)
      buf->end_of_used_area= mrr_buf_last;
    DBUG_RETURN(0); /* nothing could go wrong while filling the buffer */
  }

  /* In regular DS-MRR, buffer stores {rowid, range_id} pairs */
  elem_size= h->ref_length + (int)is_mrr_assoc * sizeof(void*);
  mrr_buf_last= mrr_buf + ((mrr_buf_end - mrr_buf)/ elem_size)* elem_size;
  mrr_buf_end= mrr_buf_last;

  /*
    psergey2: this is only needed when 
      - doing a rowid-to-row scan
      - the buffer wasn't exhausted on the first pass.
  */
  /*
    There can be two cases:
    - This is the first call since index_init(), h2==NULL
       Need to setup h2 then.
    - This is not the first call, h2 is initalized and set up appropriately.
       The caller might have called h->index_init(), need to switch h to
       rnd_pos calls.
  */
  if (!h2)
  {
    /* Create a separate handler object to do rnd_pos() calls. */
    THD *thd= current_thd;
    /*
      ::clone() takes up a lot of stack, especially on 64 bit platforms.
      The constant 5 is an empiric result.
    */
    if (check_stack_overrun(thd, 5*STACK_MIN_SIZE, (uchar*) &new_h2))
      DBUG_RETURN(1);
    DBUG_ASSERT(h->active_index != MAX_KEY);
    uint mrr_keyno= h->active_index;

    /* Create a separate handler object to do rnd_pos() calls. */
    if (!(new_h2= h->clone(thd->mem_root)) || 
        new_h2->ha_external_lock(thd, F_RDLCK))
    {
      delete new_h2;
      DBUG_RETURN(1);
    }

    if (mrr_keyno == h->pushed_idx_cond_keyno)
      pushed_cond= h->pushed_idx_cond;

    /*
      Caution: this call will invoke this->dsmrr_close(). Do not put the
      created secondary table handler into this->h2 or it will delete it.
    */
    if (h->ha_index_end())
    {
      h2=new_h2;
      goto error;
    }

    use_default_impl= FALSE;
    h2= new_h2; /* Ok, now can put it into h2 */
    table->prepare_for_position();
    h2->extra(HA_EXTRA_KEYREAD);
  
    if (h2->ha_index_init(mrr_keyno, FALSE))
      goto error;

    if (pushed_cond)
      h2->idx_cond_push(mrr_keyno, pushed_cond);
  }
  else
  {
    /* 
      We get here when the access alternates betwen MRR scan(s) and non-MRR
      scans.

      Calling h->index_end() will invoke dsmrr_close() for this object,
      which will delete h2. We need to keep it, so save put it away and dont
      let it be deleted:
    */
    handler *save_h2= h2;
    h2= NULL;
    int res= (h->inited == handler::INDEX && h->ha_index_end());
    h2= save_h2;
    if (res)
      goto error;
  }

  if (h2->handler::multi_range_read_init(seq_funcs, seq_init_param, n_ranges, 
                                         mode, buf) ||
      dsmrr_fill_rowid_buffer())
  {
    goto error;
  }
  /*
    If the above call has scanned through all intervals in *seq, then
    adjust *buf to indicate that the remaining buffer space will not be used.
  */
  if (dsmrr_eof) 
    buf->end_of_used_area= mrr_buf_last;

  /*
     h->inited == INDEX may occur when 'range checked for each record' is
     used.
  */
  if ((h->inited != handler::RND) && 
      ((h->inited==handler::INDEX? h->ha_index_end(): FALSE) || 
       (h->ha_rnd_init(FALSE))))
      goto error;

  h->mrr_funcs= *seq_funcs;
  
  DBUG_RETURN(0);
error:
  h2->ha_index_or_rnd_end();
  h2->ha_external_lock(current_thd, F_UNLCK);
  h2->close();
  delete h2;
  h2= NULL;
  DBUG_RETURN(1);
}


void DsMrr_impl::dsmrr_close()
{
  DBUG_ENTER("DsMrr_impl::dsmrr_close");
  if (h2)
  {
    h2->ha_index_or_rnd_end();
    h2->ha_external_lock(current_thd, F_UNLCK);
    h2->close();
    delete h2;
    h2= NULL;
  }
  use_default_impl= TRUE;
  DBUG_VOID_RETURN;
}


static int rowid_cmp(void *h, uchar *a, uchar *b)
{
  return ((handler*)h)->cmp_ref(a, b);
}


/**
  DS-MRR: Fill the buffer with rowids and sort it by rowid

  {This is an internal function of DiskSweep MRR implementation}
  Scan the MRR ranges and collect ROWIDs (or {ROWID, range_id} pairs) into 
  buffer. When the buffer is full or scan is completed, sort the buffer by 
  rowid and return.
  
  The function assumes that rowids buffer is empty when it is invoked. 

  dsmrr_eof is set to indicate whether we've exhausted the list of ranges we're
  scanning.

  psergey2: this func will 'fill the rowid buffer'. If filling the rowid buffer 
  requires that key buffer is filled/sorted first, will do that, too.
  
  @param h  Table handler

  @retval 0      OK, the next portion of rowids is in the buffer,
                 properly ordered
  @retval other  Error
*/

int DsMrr_impl::dsmrr_fill_rowid_buffer()
{
  char *range_info;
  int res;
  DBUG_ENTER("DsMrr_impl::dsmrr_fill_rowid_buffer");
  
  mrr_buf_cur= mrr_buf;
  mrr_buf_next_identical= mrr_buf_cur;
  /*
    psergey2-todo:
      - call here fill/sort key buffer, if needed.

    psergey2-todo: then, get keys either from
      - multi_range_read_next()
      - sorted key buffer
    
    psergey2-todo: if we're traversing an ordered key sequence,
     check if next keys are the same as previous.
     (note that it's easy as ordered sequence allows forward/backward
     navigation so we don't need to buffer things)
  */
  while ((mrr_buf_cur < mrr_buf_end) && 
         !(res= h2->handler::multi_range_read_next(&range_info)))
  {
    KEY_MULTI_RANGE *curr_range= &h2->handler::mrr_cur_range;
    if (h2->mrr_funcs.skip_index_tuple &&
        h2->mrr_funcs.skip_index_tuple(h2->mrr_iter, curr_range->ptr))
      continue;
    

    /* Put rowid, or {rowid, range_id} pair into the buffer */
    h2->position(table->record[0]);
    memcpy(mrr_buf_cur, h2->ref, h2->ref_length);
    mrr_buf_cur += h2->ref_length;

    if (is_mrr_assoc)
    {
      memcpy(mrr_buf_cur, &range_info, sizeof(void*));
      mrr_buf_cur += sizeof(void*);
    }
  }

  if (res && res != HA_ERR_END_OF_FILE)
    DBUG_RETURN(res); 
  dsmrr_eof= test(res == HA_ERR_END_OF_FILE);

  /* Sort the buffer contents by rowid */
  uint elem_size= h->ref_length + (int)is_mrr_assoc * sizeof(void*);
  uint n_rowids= (mrr_buf_cur - mrr_buf) / elem_size;
  
  my_qsort2(mrr_buf, n_rowids, elem_size, (qsort2_cmp)rowid_cmp,
            (void*)h);
  mrr_buf_last= mrr_buf_cur;
  mrr_buf_cur=  mrr_buf;
  DBUG_RETURN(0);
}


/* 
  my_qsort2-compatible function to compare key tuples 
*/

int DsMrr_impl::key_tuple_cmp(void* arg, uchar* key1, uchar* key2)
{
  DsMrr_impl *dsmrr= (DsMrr_impl*)arg;
  TABLE *table= dsmrr->h->table;
  
  KEY_PART_INFO *part= table->key_info[table->s->primary_key].key_part;
  uchar *key1_end= key1 + dsmrr->cpk_tuple_length;

  while (key1 < key1_end)
  {
    Field* f = part->field;
    int len = part->store_length;
    int res = f->cmp(key1, key2);
    if (res)
      return res;
    key1 += len;
    key2 += len;
    part++;
  }
  return 0;
}


/*
  DS-MRR/CPK: Fill the buffer with (lookup_tuple, range_id) pairs and sort
  
  SYNOPSIS
    DsMrr_impl::dsmrr_fill_key_buffer()

  DESCRIPTION
    DS-MRR/CPK: Fill the buffer with (lookup_tuple, range_id) pairs and sort

    dsmrr_eof is set to indicate whether we've exhausted the list of ranges 
    we're scanning.

  psergey2-q: can this be used for filling/sorting key buffer in general case?
   a: yes. 
  qq: can we push sequence iteration init down into here?
*/

void DsMrr_impl::dsmrr_fill_key_buffer()
{
  //psergey2: here, no identicals detection is necessary since we always scan
  //  the unordered sequence.
  int res;
  KEY_MULTI_RANGE cur_range;
  DBUG_ENTER("DsMrr_impl::dsmrr_fill_key_buffer");

  mrr_buf_cur= mrr_buf;
  while ((mrr_buf_cur < mrr_buf_end) && 
         !(res= h->mrr_funcs.next(h->mrr_iter, &cur_range)))
  {
    DBUG_ASSERT(cur_range.range_flag & EQ_RANGE);
    DBUG_ASSERT(!cpk_tuple_length || 
                cpk_tuple_length == cur_range.start_key.length);
    if (!cpk_tuple_length)
    {
      cpk_tuple_length= cur_range.start_key.length;
      cpk_is_unique_scan= test(table->key_info[h->active_index].key_parts == 
                               my_count_bits(cur_range.start_key.keypart_map));
      uint elem_size= cpk_tuple_length + (int)is_mrr_assoc * sizeof(void*);
      mrr_buf_last= mrr_buf + ((mrr_buf_end - mrr_buf)/elem_size) * elem_size;
      mrr_buf_end= mrr_buf_last;
    }

    //psergey2: if keys are materialized, store pointers, not copy keys

    /* Put key, or {key, range_id} pair into the buffer */
    memcpy(mrr_buf_cur, cur_range.start_key.key, cpk_tuple_length);
    mrr_buf_cur += cpk_tuple_length;

    if (is_mrr_assoc)
    {
      memcpy(mrr_buf_cur, &cur_range.ptr, sizeof(void*));
      mrr_buf_cur += sizeof(void*);
    }
  }

  dsmrr_eof= test(res);

  /* Sort the buffer contents by rowid */
  uint elem_size= cpk_tuple_length + (int)is_mrr_assoc * sizeof(void*);
  uint n_rowids= (mrr_buf_cur - mrr_buf) / elem_size;
  
  my_qsort2(mrr_buf, n_rowids, elem_size, 
            (qsort2_cmp)DsMrr_impl::key_tuple_cmp, (void*)this);
  mrr_buf_last= mrr_buf_cur;
  mrr_buf_cur=  mrr_buf;
  DBUG_VOID_RETURN;
}


/*
  DS-MRR/CPK: multi_range_read_next() function

  DESCRIPTION
    DsMrr_impl::dsmrr_next_cpk()
      range_info  OUT  identifier of range that the returned record belongs to

  DESCRIPTION
    DS-MRR/CPK: multi_range_read_next() function. 
    This is similar to DsMrr_impl::dsmrr_next(), the differences are that
     - we get records with index_read(), not with rnd_pos()
     - we may get multiple records for one key (=element of the buffer)
     - unlike dsmrr_fill_rowid_buffer(), dsmrr_fill_key_buffer() never fails.
 
  RETURN
    0                   OK, next record was successfully read
    HA_ERR_END_OF_FILE  End of records
    Other               Some other error

  psergey2-todo: this should detect identical keys.
*/

int DsMrr_impl::dsmrr_next_cpk(char **range_info)
{
  int res;

  while (cpk_have_range)
  {

    if (h->mrr_funcs.skip_record &&
        h->mrr_funcs.skip_record(h->mrr_iter, cpk_saved_range_info, NULL))
    {
      cpk_have_range= FALSE;
      break;
    }

    res= h->index_next_same(table->record[0], mrr_buf_cur, cpk_tuple_length);

    if (h->mrr_funcs.skip_index_tuple &&
        h->mrr_funcs.skip_index_tuple(h->mrr_iter, cpk_saved_range_info))
      continue;

    if (res != HA_ERR_END_OF_FILE)
    {
      if (is_mrr_assoc)
        memcpy(range_info, &cpk_saved_range_info, sizeof(void*));
      return res;
    }

    /* No more records in this range. Exit this loop and go get another range */
    cpk_have_range= FALSE;
  }

  do
  {
    /* First, make sure we have a range at start of the buffer */
    if (mrr_buf_cur == mrr_buf_last)
    {
      if (dsmrr_eof)
      {
        res= HA_ERR_END_OF_FILE;
        goto end;
      }
      dsmrr_fill_key_buffer();
    }
    if (mrr_buf_cur == mrr_buf_last)
    {
      res= HA_ERR_END_OF_FILE;
      goto end;
    }
    
    /* Ok, got the range. Try making a lookup.  */
    uchar *lookup_tuple= mrr_buf_cur;
    mrr_buf_cur += cpk_tuple_length;
    if (is_mrr_assoc)
    {
      memcpy(&cpk_saved_range_info, mrr_buf_cur, sizeof(void*));
      mrr_buf_cur += sizeof(void*) * test(is_mrr_assoc);
    }
      
    if (h->mrr_funcs.skip_record &&
        h->mrr_funcs.skip_record(h->mrr_iter, cpk_saved_range_info, NULL))
      continue;
    
    res= h->index_read(table->record[0], lookup_tuple, cpk_tuple_length, 
                       HA_READ_KEY_EXACT);

    /*
      Check pushed index condition. Performance-wise, it does not make any
      sense to put this call here (the above call has already accessed the full
      record). That's the best I could do, though, because:
      - ha_innobase doesn't support IndexConditionPushdown on clustered PK
      - MRR interface doesn't allow the storage engine to refuse a pushed index
        condition.
      Having this call here is not fully harmless: EXPLAIN shows "pushed index
      condition", which is technically true but doesn't bring the benefits that
      one might expect.
    */
    if (h->mrr_funcs.skip_index_tuple &&
        h->mrr_funcs.skip_index_tuple(h->mrr_iter, cpk_saved_range_info))
      continue;

    if (res && res != HA_ERR_END_OF_FILE)
      goto end;

    if (!res)
    {
      memcpy(range_info, &cpk_saved_range_info, sizeof(void*));
      /* 
        Attempt reading more rows from this range only if there actually can
        be multiple matches:
       */
      cpk_have_range= !cpk_is_unique_scan;
      break;
    }
  } while (true);
 
end:
  return res;
}


/**
  DS-MRR implementation: multi_range_read_next() function

  psergey2-todo: put identical rowid detection code here
    it should always work because rowid sequences are always sorted
*/

int DsMrr_impl::dsmrr_next(char **range_info)
{
  int res;
  uchar *cur_range_info= 0;
  uchar *rowid;

  if (use_default_impl)
    return h->handler::multi_range_read_next(range_info);

  if (doing_cpk_scan)
    return dsmrr_next_cpk(range_info);
  
  if (mrr_buf_next_identical != mrr_buf_cur)
  {
    /*
      There are multiple rowids. Return the record again, now with different
      range_id 
    */
    do 
    {
      if (is_mrr_assoc)
        memcpy(range_info, mrr_buf_next_identical + h->ref_length, sizeof(uchar*));
    } while (!h2->mrr_funcs.skip_record ||
             !h2->mrr_funcs.skip_record(h2->mrr_iter, (char *) range_info, rowid));

    mrr_buf_next_identical += h->ref_length + sizeof(void*) * test(is_mrr_assoc);
    return 0;
  }

  do
  {
    if (mrr_buf_cur == mrr_buf_last)
    {
      if (dsmrr_eof)
      {
        res= HA_ERR_END_OF_FILE;
        goto end;
      }
      res= dsmrr_fill_rowid_buffer();
      if (res)
        goto end;
    }
   
    /* return eof if there are no rowids in the buffer after re-fill attempt */
    if (mrr_buf_cur == mrr_buf_last)
    {
      res= HA_ERR_END_OF_FILE;
      goto end;
    }
    rowid= mrr_buf_cur;

    if (is_mrr_assoc)
      memcpy(&cur_range_info, mrr_buf_cur + h->ref_length, sizeof(uchar**));
    
    size_t element_size= h->ref_length + sizeof(void*) * test(is_mrr_assoc);
    mrr_buf_cur += element_size;
    mrr_buf_next_identical= mrr_buf_cur;

    if (h2->mrr_funcs.skip_record &&
	h2->mrr_funcs.skip_record(h2->mrr_iter, (char *) cur_range_info, rowid))
      continue;
    res= h->ha_rnd_pos(table->record[0], rowid);

    if (res == HA_ERR_RECORD_DELETED)
      continue;
    
    if (0)//(!res)
    {
      /* 
        Note: this implies that SQL layer doesn't touch table->record[0]
        between calls.
      */
      uchar *current_el= mrr_buf_cur - element_size;
      while (mrr_buf_cur != mrr_buf_last && 
             !h2->cmp_ref(current_el, mrr_buf_cur))
      {
        mrr_buf_cur += element_size;
      }
    }
    break;

  } while (true);
 
  if (is_mrr_assoc)
  {
    memcpy(range_info, rowid + h->ref_length, sizeof(void*));
  }
end:
  return res;
}


/**
  DS-MRR implementation: multi_range_read_info() function
*/
ha_rows DsMrr_impl::dsmrr_info(uint keyno, uint n_ranges, uint rows, 
                               uint key_parts,
                               uint *bufsz, uint *flags, COST_VECT *cost)
{  
  ha_rows res;
  uint def_flags= *flags;
  uint def_bufsz= *bufsz;

  /* Get cost/flags/mem_usage of default MRR implementation */
  res= h->handler::multi_range_read_info(keyno, n_ranges, rows, key_parts, 
                                         &def_bufsz, &def_flags, cost);
  DBUG_ASSERT(!res);

  if ((*flags & HA_MRR_USE_DEFAULT_IMPL) || 
      choose_mrr_impl(keyno, rows, &def_flags, &def_bufsz, cost))
  {
    /* Default implementation is choosen */
    DBUG_PRINT("info", ("Default MRR implementation choosen"));
    *flags= def_flags;
    *bufsz= def_bufsz;
  }
  else
  {
    /* *flags and *bufsz were set by choose_mrr_impl */
    DBUG_PRINT("info", ("DS-MRR implementation choosen"));
  }
  return 0;
}


/**
  DS-MRR Implementation: multi_range_read_info_const() function
*/

ha_rows DsMrr_impl::dsmrr_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                 void *seq_init_param, uint n_ranges, 
                                 uint *bufsz, uint *flags, COST_VECT *cost)
{
  ha_rows rows;
  uint def_flags= *flags;
  uint def_bufsz= *bufsz;
  /* Get cost/flags/mem_usage of default MRR implementation */
  rows= h->handler::multi_range_read_info_const(keyno, seq, seq_init_param,
                                                n_ranges, &def_bufsz, 
                                                &def_flags, cost);
  if (rows == HA_POS_ERROR)
  {
    /* Default implementation can't perform MRR scan => we can't either */
    return rows;
  }

  /*
    If HA_MRR_USE_DEFAULT_IMPL has been passed to us, that is an order to
    use the default MRR implementation (we need it for UPDATE/DELETE).
    Otherwise, make a choice based on cost and @@optimizer_use_mrr.
  */
  if ((*flags & HA_MRR_USE_DEFAULT_IMPL) ||
      choose_mrr_impl(keyno, rows, flags, bufsz, cost))
  {
    DBUG_PRINT("info", ("Default MRR implementation choosen"));
    *flags= def_flags;
    *bufsz= def_bufsz;
  }
  else
  {
    /* *flags and *bufsz were set by choose_mrr_impl */
    DBUG_PRINT("info", ("DS-MRR implementation choosen"));
  }
  return rows;
}


/**
  Check if key has partially-covered columns

  We can't use DS-MRR to perform range scans when the ranges are over
  partially-covered keys, because we'll not have full key part values
  (we'll have their prefixes from the index) and will not be able to check
  if we've reached the end the range.

  @param keyno  Key to check

  @todo
    Allow use of DS-MRR in cases where the index has partially-covered
    components but they are not used for scanning.

  @retval TRUE   Yes
  @retval FALSE  No
*/

bool key_uses_partial_cols(TABLE *table, uint keyno)
{
  KEY_PART_INFO *kp= table->key_info[keyno].key_part;
  KEY_PART_INFO *kp_end= kp + table->key_info[keyno].key_parts;
  for (; kp != kp_end; kp++)
  {
    if (!kp->field->part_of_key.is_set(keyno))
      return TRUE;
  }
  return FALSE;
}


/*
  Check if key/flags allow DS-MRR/CPK strategy to be used
  
  SYNOPSIS
   DsMrr_impl::check_cpk_scan()
     keyno      Index that will be used
     mrr_flags  
  
  DESCRIPTION
    Check if key/flags allow DS-MRR/CPK strategy to be used. 
 
  RETURN
    TRUE   DS-MRR/CPK should be used
    FALSE  Otherwise
*/

bool DsMrr_impl::check_cpk_scan(uint keyno, uint mrr_flags)
{
  return test((mrr_flags & HA_MRR_SINGLE_POINT) && 
              !(mrr_flags & HA_MRR_SORTED) && 
              keyno == table->s->primary_key && 
              h->primary_key_is_clustered());
}


/*
  DS-MRR Internals: Choose between Default MRR implementation and DS-MRR

  Make the choice between using Default MRR implementation and DS-MRR.
  This function contains common functionality factored out of dsmrr_info()
  and dsmrr_info_const(). The function assumes that the default MRR
  implementation's applicability requirements are satisfied.

  @param keyno       Index number
  @param rows        E(full rows to be retrieved)
  @param flags  IN   MRR flags provided by the MRR user
                OUT  If DS-MRR is choosen, flags of DS-MRR implementation
                     else the value is not modified
  @param bufsz  IN   If DS-MRR is choosen, buffer use of DS-MRR implementation
                     else the value is not modified
  @param cost   IN   Cost of default MRR implementation
                OUT  If DS-MRR is choosen, cost of DS-MRR scan
                     else the value is not modified

  @retval TRUE   Default MRR implementation should be used
  @retval FALSE  DS-MRR implementation should be used
*/


bool DsMrr_impl::choose_mrr_impl(uint keyno, ha_rows rows, uint *flags,
                                 uint *bufsz, COST_VECT *cost)
{
  COST_VECT dsmrr_cost;
  bool res;
  THD *thd= current_thd;

  doing_cpk_scan= check_cpk_scan(keyno, *flags); 
  if (thd->variables.optimizer_use_mrr == 2 || *flags & HA_MRR_INDEX_ONLY ||
      (keyno == table->s->primary_key && h->primary_key_is_clustered() &&
       !doing_cpk_scan) ||
       key_uses_partial_cols(table, keyno))
  {
    /* Use the default implementation */
    *flags |= HA_MRR_USE_DEFAULT_IMPL;
    return TRUE;
  }

  uint add_len= table->key_info[keyno].key_length + h->ref_length; 
  *bufsz -= add_len;
  if (get_disk_sweep_mrr_cost(keyno, rows, *flags, bufsz, &dsmrr_cost))
    return TRUE;
  *bufsz += add_len;
  
  bool force_dsmrr;
  /* 
    If @@optimizer_use_mrr==force, then set cost of DS-MRR to be minimum of
    DS-MRR and Default implementations cost. This allows one to force use of
    DS-MRR whenever it is applicable without affecting other cost-based
    choices.
  */
  if ((force_dsmrr= (thd->variables.optimizer_use_mrr == 1)) &&
      dsmrr_cost.total_cost() > cost->total_cost())
    dsmrr_cost= *cost;

  if (force_dsmrr || dsmrr_cost.total_cost() <= cost->total_cost())
  {
    *flags &= ~HA_MRR_USE_DEFAULT_IMPL;  /* Use the DS-MRR implementation */
    *flags &= ~HA_MRR_SORTED;          /* We will return unordered output */
    *cost= dsmrr_cost;
    res= FALSE;

    if ((*flags & HA_MRR_SINGLE_POINT) && 
         optimizer_flag(thd, OPTIMIZER_SWITCH_MRR_SORT_KEYS))
      *flags |= HA_MRR_MATERIALIZED_KEYS;
  }
  else
  {
    /* Use the default MRR implementation */
    res= TRUE;
  }
  return res;
}


static void get_sort_and_sweep_cost(TABLE *table, ha_rows nrows, COST_VECT *cost);


/**
  Get cost of DS-MRR scan

  @param keynr              Index to be used
  @param rows               E(Number of rows to be scanned)
  @param flags              Scan parameters (HA_MRR_* flags)
  @param buffer_size INOUT  Buffer size
  @param cost        OUT    The cost

  @retval FALSE  OK
  @retval TRUE   Error, DS-MRR cannot be used (the buffer is too small
                 for even 1 rowid)
*/

bool DsMrr_impl::get_disk_sweep_mrr_cost(uint keynr, ha_rows rows, uint flags,
                                         uint *buffer_size, COST_VECT *cost)
{
  ulong max_buff_entries, elem_size;
  ha_rows rows_in_full_step, rows_in_last_step;
  uint n_full_steps;
  double index_read_cost;

  elem_size= h->ref_length + sizeof(void*) * (!test(flags & HA_MRR_NO_ASSOCIATION));
  max_buff_entries = *buffer_size / elem_size;

  if (!max_buff_entries)
    return TRUE; /* Buffer has not enough space for even 1 rowid */

  /* Number of iterations we'll make with full buffer */
  n_full_steps= (uint)floor(rows2double(rows) / max_buff_entries);
  
  /* 
    Get numbers of rows we'll be processing in 
     - non-last sweep, with full buffer 
     - last iteration, with non-full buffer
  */
  rows_in_full_step= max_buff_entries;
  rows_in_last_step= rows % max_buff_entries;
  
  /* Adjust buffer size if we expect to use only part of the buffer */
  if (n_full_steps)
  {
    get_sort_and_sweep_cost(table, rows, cost);
    cost->multiply(n_full_steps);
  }
  else
  {
    cost->zero();
    *buffer_size= max(*buffer_size, 
                      (size_t)(1.2*rows_in_last_step) * elem_size + 
                      h->ref_length + table->key_info[keynr].key_length);
  }
  
  COST_VECT last_step_cost;
  get_sort_and_sweep_cost(table, rows_in_last_step, &last_step_cost);
  cost->add(&last_step_cost);
 
  if (n_full_steps != 0)
    cost->mem_cost= *buffer_size;
  else
    cost->mem_cost= (double)rows_in_last_step * elem_size;
  
  /* Total cost of all index accesses */
  index_read_cost= h->keyread_read_time(keynr, 1, (double)rows);
  cost->add_io(index_read_cost, 1 /* Random seeks */);
  return FALSE;
}


/* 
  Get cost of one sort-and-sweep step

  SYNOPSIS
    get_sort_and_sweep_cost()
      table       Table being accessed
      nrows       Number of rows to be sorted and retrieved
      cost   OUT  The cost

  DESCRIPTION
    Get cost of these operations:
     - sort an array of #nrows ROWIDs using qsort
     - read #nrows records from table in a sweep.
*/

static 
void get_sort_and_sweep_cost(TABLE *table, ha_rows nrows, COST_VECT *cost)
{
  if (nrows)
  {
    get_sweep_read_cost(table, nrows, FALSE, cost);
    /* Add cost of qsort call: n * log2(n) * cost(rowid_comparison) */
    double cmp_op= rows2double(nrows) * (1.0 / TIME_FOR_COMPARE_ROWID);
    if (cmp_op < 3)
      cmp_op= 3;
    cost->cpu_cost += cmp_op * log2(cmp_op);
  }
  else
    cost->zero();
}


/**
  Get cost of reading nrows table records in a "disk sweep"

  A disk sweep read is a sequence of handler->rnd_pos(rowid) calls that made
  for an ordered sequence of rowids.

  We assume hard disk IO. The read is performed as follows:

   1. The disk head is moved to the needed cylinder
   2. The controller waits for the plate to rotate
   3. The data is transferred

  Time to do #3 is insignificant compared to #2+#1.

  Time to move the disk head is proportional to head travel distance.

  Time to wait for the plate to rotate depends on whether the disk head
  was moved or not. 

  If disk head wasn't moved, the wait time is proportional to distance
  between the previous block and the block we're reading.

  If the head was moved, we don't know how much we'll need to wait for the
  plate to rotate. We assume the wait time to be a variate with a mean of
  0.5 of full rotation time.

  Our cost units are "random disk seeks". The cost of random disk seek is
  actually not a constant, it depends one range of cylinders we're going
  to access. We make it constant by introducing a fuzzy concept of "typical 
  datafile length" (it's fuzzy as it's hard to tell whether it should
  include index file, temp.tables etc). Then random seek cost is:

    1 = half_rotation_cost + move_cost * 1/3 * typical_data_file_length

  We define half_rotation_cost as DISK_SEEK_BASE_COST=0.9.

  @param table             Table to be accessed
  @param nrows             Number of rows to retrieve
  @param interrupted       TRUE <=> Assume that the disk sweep will be
                           interrupted by other disk IO. FALSE - otherwise.
  @param cost         OUT  The cost.
*/

void get_sweep_read_cost(TABLE *table, ha_rows nrows, bool interrupted, 
                         COST_VECT *cost)
{
  DBUG_ENTER("get_sweep_read_cost");

  cost->zero();
  if (table->file->primary_key_is_clustered())
  {
    cost->io_count= table->file->read_time(table->s->primary_key,
                                           (uint) nrows, nrows);
  }
  else
  {
    double n_blocks=
      ceil(ulonglong2double(table->file->stats.data_file_length) / IO_SIZE);
    double busy_blocks=
      n_blocks * (1.0 - pow(1.0 - 1.0/n_blocks, rows2double(nrows)));
    if (busy_blocks < 1.0)
      busy_blocks= 1.0;

    DBUG_PRINT("info",("sweep: nblocks=%g, busy_blocks=%g", n_blocks,
                       busy_blocks));
    cost->io_count= busy_blocks;

    if (!interrupted)
    {
      /* Assume reading is done in one 'sweep' */
      cost->avg_io_cost= (DISK_SEEK_BASE_COST +
                          DISK_SEEK_PROP_COST*n_blocks/busy_blocks);
    }
  }
  DBUG_PRINT("info",("returning cost=%g", cost->total_cost()));
  DBUG_VOID_RETURN;
}


/* **************************************************************************
 * DS-MRR implementation ends
 ***************************************************************************/


