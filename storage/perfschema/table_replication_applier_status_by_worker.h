/*
   Copyright (c) 2013, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


#ifndef TABLE_REPLICATION_APPLIER_STATUS_BY_WORKER_H
#define TABLE_REPLICATION_APPLIER_STATUS_BY_WORKER_H

/**
  @file storage/perfschema/table_replication_applier_status_by_worker.h
  Table replication_applier_status_by_worker (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_engine_table.h"
#include "rpl_mi.h"
#include "mysql_com.h"
#include "rpl_rli_pdb.h"
#include "rpl_msr.h"
#include "rpl_info.h" /*CHANNEL_NAME_LENGTH*/

class Slave_worker;
class Master_info;

/**
  @addtogroup Performance_schema_tables
  @{
*/

#ifndef ENUM_RPL_YES_NO
#define ENUM_RPL_YES_NO
/** enumerated values for service_state of worker thread*/
enum enum_rpl_yes_no {
  PS_RPL_YES= 1, /* service_state= on */
  PS_RPL_NO /* service_state= off */
};
#endif

/*
  A row in worker's table. The fields with string values have an additional
  length field denoted by <field_name>_length.
*/
struct st_row_worker {

  char channel_name[CHANNEL_NAME_LENGTH];
  uint channel_name_length;
  /*
    worker_id is added to the table because thread is killed at STOP SLAVE
    but the status needs to show up, so worker_id is used as a permanent
    identifier.
  */
  ulonglong worker_id;
  ulonglong thread_id;
  uint thread_id_is_null;
  enum_rpl_yes_no service_state;
  char last_seen_transaction[Gtid::MAX_TEXT_LENGTH+1];
  uint last_seen_transaction_length;
  uint last_error_number;
  char last_error_message[MAX_SLAVE_ERRMSG];
  uint last_error_message_length;
  ulonglong last_error_timestamp;
};

/**
  Position in table replication_applier_status_by_worker.
  Index 1 for replication channel.
  Index 2 for worker:
  - position [0] is for Single Thread Slave (Master_info)
  - position [1] .. [N] is for Multi Thread Slave (Slave_worker)
*/
struct pos_replication_applier_status_by_worker : public PFS_double_index
{

  pos_replication_applier_status_by_worker() : PFS_double_index(0, 0)
  {}

  inline void reset(void)
  {
    m_index_1= 0;
    m_index_2= 0;
  }

  inline bool has_more_channels(uint num)
  { return (m_index_1 < num); }

  inline void next_channel(void)
  {
    m_index_1++;
    m_index_2= 0;
  }

  inline void next_worker()
  {
    m_index_2++;
  }

  inline void
  set_channel_after(const pos_replication_applier_status_by_worker *other)
  {
    m_index_1 = other->m_index_1 + 1;
    m_index_2 = 0;
  }
};


/** Table PERFORMANCE_SCHEMA.replication_applier_status_by_worker */
class table_replication_applier_status_by_worker: public PFS_engine_table
{
  typedef pos_replication_applier_status_by_worker pos_t;

private:
  void make_row(Slave_worker *);
  /*
    Master_info to construct a row to display SQL Thread's status
    information in STS mode
  */
  void make_row(Master_info *);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Fields definition. */
  static TABLE_FIELD_DEF m_field_def;
  /** current row*/
  st_row_worker m_row;
  /** True is the current row exists. */
  bool m_row_exists;
  /** Current position. */
  pos_t m_pos;
  /** Next position. */
  pos_t m_next_pos;

protected:
  /**
    Read the current row values.
    @param table            Table handle
    @param buf              row buffer
    @param fields           Table fields
    @param read_all         true if all columns are read.
  */

  virtual int read_row_values(TABLE *table,
                              unsigned char *buf,
                              Field **fields,
                              bool read_all);

  table_replication_applier_status_by_worker();

public:
  ~table_replication_applier_status_by_worker();

  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table* create();
  static ha_rows get_row_count();
  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

};

/** @} */
#endif
