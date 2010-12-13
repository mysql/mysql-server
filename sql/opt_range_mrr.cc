
/****************************************************************************
  MRR Range Sequence Interface implementation that walks a SEL_ARG* tree.
 ****************************************************************************/

/* MRR range sequence, SEL_ARG* implementation: stack entry */
typedef struct st_range_seq_entry 
{
  /* 
    Pointers in min and max keys. They point to right-after-end of key
    images. The 0-th entry has these pointing to key tuple start.
  */
  uchar *min_key, *max_key;
  
  /* 
    Flags, for {keypart0, keypart1, ... this_keypart} subtuple.
    min_key_flag may have NULL_RANGE set.
  */
  uint min_key_flag, max_key_flag;
  
  /* Number of key parts */
  uint min_key_parts, max_key_parts;
  SEL_ARG *key_tree;
} RANGE_SEQ_ENTRY;


/*
  MRR range sequence, SEL_ARG* implementation: SEL_ARG graph traversal context
*/
typedef struct st_sel_arg_range_seq
{
  uint keyno;      /* index of used tree in SEL_TREE structure */
  uint real_keyno; /* Number of the index in tables */
  PARAM *param;
  SEL_ARG *start; /* Root node of the traversed SEL_ARG* graph */
  
  RANGE_SEQ_ENTRY stack[MAX_REF_PARTS];
  int i; /* Index of last used element in the above array */
  
  bool at_start; /* TRUE <=> The traversal has just started */
} SEL_ARG_RANGE_SEQ;


/*
  Range sequence interface, SEL_ARG* implementation: Initialize the traversal

  SYNOPSIS
    init()
      init_params  SEL_ARG tree traversal context
      n_ranges     [ignored] The number of ranges obtained 
      flags        [ignored] HA_MRR_SINGLE_POINT, HA_MRR_FIXED_KEY

  RETURN
    Value of init_param
*/

range_seq_t sel_arg_range_seq_init(void *init_param, uint n_ranges, uint flags)
{
  SEL_ARG_RANGE_SEQ *seq= (SEL_ARG_RANGE_SEQ*)init_param;
  seq->at_start= TRUE;
  seq->stack[0].key_tree= NULL;
  seq->stack[0].min_key= seq->param->min_key;
  seq->stack[0].min_key_flag= 0;
  seq->stack[0].min_key_parts= 0;

  seq->stack[0].max_key= seq->param->max_key;
  seq->stack[0].max_key_flag= 0;
  seq->stack[0].max_key_parts= 0;
  seq->i= 0;
  return init_param;
}


static void step_down_to(SEL_ARG_RANGE_SEQ *arg, SEL_ARG *key_tree)
{
  RANGE_SEQ_ENTRY *cur= &arg->stack[arg->i+1];
  RANGE_SEQ_ENTRY *prev= &arg->stack[arg->i];
  
  cur->key_tree= key_tree;
  cur->min_key= prev->min_key;
  cur->max_key= prev->max_key;
  cur->min_key_parts= prev->min_key_parts;
  cur->max_key_parts= prev->max_key_parts;

  uint16 stor_length= arg->param->key[arg->keyno][key_tree->part].store_length;
  cur->min_key_parts += key_tree->store_min(stor_length, &cur->min_key,
                                            prev->min_key_flag);
  cur->max_key_parts += key_tree->store_max(stor_length, &cur->max_key,
                                            prev->max_key_flag);

  cur->min_key_flag= prev->min_key_flag | key_tree->min_flag;
  cur->max_key_flag= prev->max_key_flag | key_tree->max_flag;

  if (key_tree->is_null_interval())
    cur->min_key_flag |= NULL_RANGE;
  (arg->i)++;
}


/*
  Range sequence interface, SEL_ARG* implementation: get the next interval
  
  SYNOPSIS
    sel_arg_range_seq_next()
      rseq        Value returned from sel_arg_range_seq_init
      range  OUT  Store information about the range here

  DESCRIPTION
    This is "get_next" function for Range sequence interface implementation
    for SEL_ARG* tree.

  IMPLEMENTATION
    The traversal also updates those param members:
      - is_ror_scan
      - range_count
      - max_key_part

  RETURN
    FALSE  Ok
    TRUE   No more ranges in the sequence
*/

bool sel_arg_range_seq_next(range_seq_t rseq, KEY_MULTI_RANGE *range)
{
  SEL_ARG *key_tree;
  SEL_ARG_RANGE_SEQ *seq= (SEL_ARG_RANGE_SEQ*)rseq;
  if (seq->at_start)
  {
    key_tree= seq->start;
    seq->at_start= FALSE;
    goto walk_up_n_right;
  }

  key_tree= seq->stack[seq->i].key_tree;
  /* Ok, we're at some "full tuple" position in the tree */
 
  /* Step down if we can */
  if (key_tree->next && key_tree->next != &null_element)
  {
    //step down; (update the tuple, we'll step right and stay there)
    seq->i--;
    step_down_to(seq, key_tree->next);
    key_tree= key_tree->next;
    seq->param->is_ror_scan= FALSE;
    goto walk_right_n_up;
  }

  /* Ok, can't step down, walk left until we can step down */
  while (1)
  {
    if (seq->i == 1) // can't step left
      return 1;
    /* Step left */
    seq->i--;
    key_tree= seq->stack[seq->i].key_tree;

    /* Step down if we can */
    if (key_tree->next && key_tree->next != &null_element)
    {
      // Step down; update the tuple
      seq->i--;
      step_down_to(seq, key_tree->next);
      key_tree= key_tree->next;
      break;
    }
  }

  /*
    Ok, we've stepped down from the path to previous tuple.
    Walk right-up while we can
  */
walk_right_n_up:
  while (key_tree->next_key_part && key_tree->next_key_part != &null_element && 
         key_tree->next_key_part->part == key_tree->part + 1 &&
         key_tree->next_key_part->type == SEL_ARG::KEY_RANGE)
  {
    {
      RANGE_SEQ_ENTRY *cur= &seq->stack[seq->i];
      uint min_key_length= cur->min_key - seq->param->min_key;
      uint max_key_length= cur->max_key - seq->param->max_key;
      uint len= cur->min_key - cur[-1].min_key;
      if (!(min_key_length == max_key_length &&
            !memcmp(cur[-1].min_key, cur[-1].max_key, len) &&
            !key_tree->min_flag && !key_tree->max_flag))
      {
        seq->param->is_ror_scan= FALSE;
        if (!key_tree->min_flag)
          cur->min_key_parts += 
            key_tree->next_key_part->store_min_key(seq->param->key[seq->keyno],
                                                   &cur->min_key,
                                                   &cur->min_key_flag);
        if (!key_tree->max_flag)
          cur->max_key_parts += 
            key_tree->next_key_part->store_max_key(seq->param->key[seq->keyno],
                                                   &cur->max_key,
                                                   &cur->max_key_flag);
        break;
      }
    }
  
    /*
      Ok, current atomic interval is in form "t.field=const" and there is
      next_key_part interval. Step right, and walk up from there.
    */
    key_tree= key_tree->next_key_part;

walk_up_n_right:
    while (key_tree->prev && key_tree->prev != &null_element)
    {
      /* Step up */
      key_tree= key_tree->prev;
    }
    step_down_to(seq, key_tree);
  }

  /* Ok got a tuple */
  RANGE_SEQ_ENTRY *cur= &seq->stack[seq->i];
  uint min_key_length= cur->min_key - seq->param->min_key;
  
  range->ptr= (char*)(int)(key_tree->part);
  if (cur->min_key_flag & GEOM_FLAG)
  {
    range->range_flag= cur->min_key_flag;

    /* Here minimum contains also function code bits, and maximum is +inf */
    range->start_key.key=    seq->param->min_key;
    range->start_key.length= min_key_length;
    range->start_key.flag=  (ha_rkey_function) (cur->min_key_flag ^ GEOM_FLAG);
  }
  else
  {
    range->range_flag= cur->min_key_flag | cur->max_key_flag;
    
    range->start_key.key=    seq->param->min_key;
    range->start_key.length= cur->min_key - seq->param->min_key;
    range->start_key.keypart_map= make_prev_keypart_map(cur->min_key_parts);
    range->start_key.flag= (cur->min_key_flag & NEAR_MIN ? HA_READ_AFTER_KEY : 
                                                           HA_READ_KEY_EXACT);

    range->end_key.key=    seq->param->max_key;
    range->end_key.length= cur->max_key - seq->param->max_key;
    range->end_key.flag= (cur->max_key_flag & NEAR_MAX ? HA_READ_BEFORE_KEY : 
                                                         HA_READ_AFTER_KEY);
    range->end_key.keypart_map= make_prev_keypart_map(cur->max_key_parts);

    if (!(cur->min_key_flag & ~NULL_RANGE) && !cur->max_key_flag &&
        (uint)key_tree->part+1 == seq->param->table->key_info[seq->real_keyno].key_parts &&
        (seq->param->table->key_info[seq->real_keyno].flags & HA_NOSAME) &&
        range->start_key.length == range->end_key.length &&
        !memcmp(seq->param->min_key,seq->param->max_key,range->start_key.length))
      range->range_flag= UNIQUE_RANGE | (cur->min_key_flag & NULL_RANGE);
      
    if (seq->param->is_ror_scan)
    {
      /*
        If we get here, the condition on the key was converted to form
        "(keyXpart1 = c1) AND ... AND (keyXpart{key_tree->part - 1} = cN) AND
          somecond(keyXpart{key_tree->part})"
        Check if
          somecond is "keyXpart{key_tree->part} = const" and
          uncovered "tail" of KeyX parts is either empty or is identical to
          first members of clustered primary key.
      */
      if (!(!(cur->min_key_flag & ~NULL_RANGE) && !cur->max_key_flag &&
            (range->start_key.length == range->end_key.length) &&
            !memcmp(range->start_key.key, range->end_key.key, range->start_key.length) &&
            is_key_scan_ror(seq->param, seq->real_keyno, key_tree->part + 1)))
        seq->param->is_ror_scan= FALSE;
    }
  }
  seq->param->range_count++;
  seq->param->max_key_part=max(seq->param->max_key_part,key_tree->part);
  return 0;
}

/****************************************************************************
  MRR Range Sequence Interface implementation that walks array<QUICK_RANGE>
 ****************************************************************************/

/*
  Range sequence interface implementation for array<QUICK_RANGE>: initialize
  
  SYNOPSIS
    quick_range_seq_init()
      init_param  Caller-opaque paramenter: QUICK_RANGE_SELECT* pointer
      n_ranges    Number of ranges in the sequence (ignored)
      flags       MRR flags (currently not used) 

  RETURN
    Opaque value to be passed to quick_range_seq_next
*/

range_seq_t quick_range_seq_init(void *init_param, uint n_ranges, uint flags)
{
  QUICK_RANGE_SELECT *quick= (QUICK_RANGE_SELECT*)init_param;
  quick->qr_traversal_ctx.first=  (QUICK_RANGE**)quick->ranges.buffer;
  quick->qr_traversal_ctx.cur=    (QUICK_RANGE**)quick->ranges.buffer;
  quick->qr_traversal_ctx.last=   quick->qr_traversal_ctx.cur + 
                                  quick->ranges.elements;
  return &quick->qr_traversal_ctx;
}


/*
  Range sequence interface implementation for array<QUICK_RANGE>: get next
  
  SYNOPSIS
    quick_range_seq_next()
      rseq        Value returned from quick_range_seq_init
      range  OUT  Store information about the range here

  RETURN
    0  Ok
    1  No more ranges in the sequence
*/

bool quick_range_seq_next(range_seq_t rseq, KEY_MULTI_RANGE *range)
{
  QUICK_RANGE_SEQ_CTX *ctx= (QUICK_RANGE_SEQ_CTX*)rseq;

  if (ctx->cur == ctx->last)
    return 1; /* no more ranges */

  QUICK_RANGE *cur= *(ctx->cur);
  cur->make_min_endpoint(&range->start_key);
  cur->make_max_endpoint(&range->end_key);
  range->range_flag= cur->flag;
  ctx->cur++;
  return 0;
}


