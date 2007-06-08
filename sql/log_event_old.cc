
#include "mysql_priv.h"
#include "log_event_old.h"
#include "rpl_record_old.h"

#if !defined(MYSQL_CLIENT) && defined(HAVE_REPLICATION)
int
Write_rows_log_event_old::do_prepare_row(THD *thd,
                                         RELAY_LOG_INFO const *rli,
                                         TABLE *table,
                                         uchar const *row_start,
                                         uchar const **row_end)
{
  DBUG_ASSERT(table != NULL);
  DBUG_ASSERT(row_start && row_end);

  int error;
  error= unpack_row_old(const_cast<RELAY_LOG_INFO*>(rli),
                        table, m_width, table->record[0],
                        row_start, &m_cols, row_end, &m_master_reclength,
                        table->write_set, PRE_GA_WRITE_ROWS_EVENT);
  bitmap_copy(table->read_set, table->write_set);
  return error;
}


int
Delete_rows_log_event_old::do_prepare_row(THD *thd,
                                          RELAY_LOG_INFO const *rli,
                                          TABLE *table,
                                          uchar const *row_start,
                                          uchar const **row_end)
{
  int error;
  DBUG_ASSERT(row_start && row_end);
  /*
    This assertion actually checks that there is at least as many
    columns on the slave as on the master.
  */
  DBUG_ASSERT(table->s->fields >= m_width);

  error= unpack_row_old(const_cast<RELAY_LOG_INFO*>(rli),
                        table, m_width, table->record[0],
                        row_start, &m_cols, row_end, &m_master_reclength,
                        table->read_set, PRE_GA_DELETE_ROWS_EVENT);
  /*
    If we will access rows using the random access method, m_key will
    be set to NULL, so we do not need to make a key copy in that case.
   */
  if (m_key)
  {
    KEY *const key_info= table->key_info;

    key_copy(m_key, table->record[0], key_info, 0);
  }

  return error;
}


int Update_rows_log_event_old::do_prepare_row(THD *thd,
                                              RELAY_LOG_INFO const *rli,
                                              TABLE *table,
                                              uchar const *row_start,
                                              uchar const **row_end)
{
  int error;
  DBUG_ASSERT(row_start && row_end);
  /*
    This assertion actually checks that there is at least as many
    columns on the slave as on the master.
  */
  DBUG_ASSERT(table->s->fields >= m_width);

  /* record[0] is the before image for the update */
  error= unpack_row_old(const_cast<RELAY_LOG_INFO*>(rli),
                        table, m_width, table->record[0],
                        row_start, &m_cols, row_end, &m_master_reclength,
                        table->read_set, PRE_GA_UPDATE_ROWS_EVENT);
  row_start = *row_end;
  /* m_after_image is the after image for the update */
  error= unpack_row_old(const_cast<RELAY_LOG_INFO*>(rli),
                        table, m_width, m_after_image,
                        row_start, &m_cols, row_end, &m_master_reclength,
                        table->write_set, PRE_GA_UPDATE_ROWS_EVENT);

  DBUG_DUMP("record[0]", table->record[0], table->s->reclength);
  DBUG_DUMP("m_after_image", m_after_image, table->s->reclength);

  /*
    If we will access rows using the random access method, m_key will
    be set to NULL, so we do not need to make a key copy in that case.
   */
  if (m_key)
  {
    KEY *const key_info= table->key_info;

    key_copy(m_key, table->record[0], key_info, 0);
  }

  return error;
}

#endif
