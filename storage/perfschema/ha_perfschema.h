/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef HA_PERFSCHEMA_H
#define HA_PERFSCHEMA_H

#include "handler.h"                            /* class handler */

#ifdef USE_PRAGMA_INTERFACE
#pragma interface /* gcc class implementation */
#endif

/**
  @file storage/perfschema/ha_perfschema.h
  Performance schema storage engine (declarations).

  @defgroup Performance_schema_engine Performance Schema Engine
  @ingroup Performance_schema_implementation
  @{
*/
struct PFS_engine_table_share;
class PFS_engine_table;
extern const char *pfs_engine_name;

/** A handler for a PERFORMANCE_SCHEMA table. */
class ha_perfschema : public handler
{
public:
  ha_perfschema(handlerton *hton, TABLE_SHARE *share);

  ~ha_perfschema();

  const char *table_type(void) const { return pfs_engine_name; }

  const char *index_type(uint) { return ""; }

  const char **bas_ext(void) const;

  /** Capabilities of the performance schema tables. */
  ulonglong table_flags(void) const
  {
    /*
      About HA_FAST_KEY_READ:

      The storage engine ::rnd_pos() method is fast to locate records by key,
      so HA_FAST_KEY_READ is technically true, but the record content can be
      overwritten between ::rnd_next() and ::rnd_pos(), because all the P_S
      data is volatile.
      The HA_FAST_KEY_READ flag is not advertised, to force the optimizer
      to cache records instead, to provide more consistent records.
      For example, consider the following statement:
      - select * from P_S.EVENTS_WAITS_HISTORY_LONG where THREAD_ID=<n>
      order by ...
      With HA_FAST_KEY_READ, it can return records where "THREAD_ID=<n>"
      is false, because the where clause was evaluated to true after
      ::rnd_pos(), then the content changed, then the record was fetched by
      key using ::rnd_pos().
      Without HA_FAST_KEY_READ, the optimizer reads all columns and never
      calls ::rnd_pos(), so it is guaranteed to return only thread <n>
      records.
    */
    return HA_NO_TRANSACTIONS | HA_REC_NOT_IN_SEQ | HA_NO_AUTO_INCREMENT |
      HA_BINLOG_ROW_CAPABLE | HA_BINLOG_STMT_CAPABLE | HA_NO_BLOBS;
  }

  /**
    Operations supported by indexes.
    None, there are no indexes.
  */
  ulong index_flags(uint , uint , bool ) const
  { return 0; }

  uint max_supported_record_length(void) const
  { return HA_MAX_REC_LENGTH; }

  uint max_supported_keys(void) const
  { return 0; }

  uint max_supported_key_parts(void) const
  { return 0; }

  uint max_supported_key_length(void) const
  { return 0; }

  ha_rows estimate_rows_upper_bound(void)
  { return HA_POS_ERROR; }

  double scan_time(void)
  { return 1.0; }

  int open(const char *name, int mode, uint test_if_locked);

  int close(void);

  int write_row(uchar *buf);

  void use_hidden_primary_key();

  int update_row(const uchar *old_data, uchar *new_data);

  int rnd_init(bool scan);

  int rnd_end(void);

  int rnd_next(uchar *buf);

  int rnd_pos(uchar *buf, uchar *pos);

  void position(const uchar *record);

  int info(uint);

  int delete_all_rows(void);

  int truncate();

  int delete_table(const char *from);

  int rename_table(const char * from, const char * to);

  int create(const char *name, TABLE *form,
             HA_CREATE_INFO *create_info);

  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);

  virtual uint8 table_cache_type(void)
  { return HA_CACHE_TBL_NOCACHE; }

  virtual my_bool register_query_cache_table
    (THD *, char *, uint , qc_engine_callback *engine_callback, ulonglong *)
  {
    *engine_callback= 0;
    return FALSE;
  }

  virtual void print_error(int error, myf errflags);

private:
  /** MySQL lock */
  THR_LOCK_DATA m_thr_lock;
  /** Performance schema table share for this table handler. */
  const PFS_engine_table_share *m_table_share;
  /** Performance schema table cursor. */
  PFS_engine_table *m_table;
};

/** @} */
#endif

