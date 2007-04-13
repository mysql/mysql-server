#ifndef RPL_RECORD_OLD_H
#define RPL_RECORD_OLD_H

#ifndef MYSQL_CLIENT
my_size_t pack_row_old(THD *thd, TABLE *table, MY_BITMAP const* cols,
                       byte *row_data, const byte *record);

#ifdef HAVE_REPLICATION
int unpack_row_old(RELAY_LOG_INFO *rli,
                   TABLE *table, uint const colcnt, byte *record,
                   char const *row, MY_BITMAP const *cols,
                   char const **row_end, ulong *master_reclength,
                   MY_BITMAP* const rw_set,
                   Log_event_type const event_type);
#endif
#endif
#endif
