/****************************************************************************
 * MRR implementation: use DS-MRR, essentially copied from MyISAM
 ***************************************************************************/

int ha_tokudb::multi_range_read_init(RANGE_SEQ_IF *seq, void *seq_init_param,
                                     uint n_ranges, uint mode, 
                                     HANDLER_BUFFER *buf)
{
  return ds_mrr.dsmrr_init(this, seq, seq_init_param, n_ranges, mode, buf);
}

int ha_tokudb::multi_range_read_next(char **range_info)
{
  return ds_mrr.dsmrr_next(range_info);
}

ha_rows ha_tokudb::multi_range_read_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                               void *seq_init_param, 
                                               uint n_ranges, uint *bufsz,
                                               uint *flags, Cost_estimate *cost)
{
  /*
    This call is here because there is no location where this->table would
    already be known.
    TODO: consider moving it into some per-query initialization call.
  */
  ds_mrr.init(this, table);
  return ds_mrr.dsmrr_info_const(keyno, seq, seq_init_param, n_ranges, bufsz,
                                 flags, cost);
}

ha_rows ha_tokudb::multi_range_read_info(uint keyno, uint n_ranges, uint keys,
                                         uint *bufsz, uint *flags,
                                         Cost_estimate *cost)
{
  ds_mrr.init(this, table);
  return ds_mrr.dsmrr_info(keyno, n_ranges, keys, bufsz, flags, cost);
}

