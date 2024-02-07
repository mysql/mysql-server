/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef HA_PERFSCHEMA_H
#define HA_PERFSCHEMA_H

#include <sys/types.h>

#include "my_base.h"
#include "my_inttypes.h"
#include "sql/handler.h" /* class handler */
#include "sql/sql_const.h"
#include "thr_lock.h"

class PFS_engine_table;
class THD;
namespace dd {
class Table;
}  // namespace dd
/**
  @file storage/perfschema/ha_perfschema.h
  Performance schema storage engine (declarations).

  @defgroup performance_schema_engine Performance Schema Engine
  @ingroup performance_schema_implementation
  @{
*/
struct PFS_engine_table_share;
struct TABLE;
struct TABLE_SHARE;

/** Name of the performance schema engine. */
extern const char *pfs_engine_name;

/** A handler for a PERFORMANCE_SCHEMA table. */
class ha_perfschema : public handler {
 public:
  /**
    Create a new performance schema table handle on a table.
    @param hton storage engine handler singleton
    @param share table share
  */
  ha_perfschema(handlerton *hton, TABLE_SHARE *share);

  ~ha_perfschema() override;

  const char *table_type() const override { return pfs_engine_name; }

  const char *index_type(uint key_number);

  const char **bas_ext() const;

  /** Capabilities of the performance schema tables. */
  ulonglong table_flags() const override {
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
    return HA_NO_TRANSACTIONS | HA_NO_AUTO_INCREMENT |
           HA_PRIMARY_KEY_REQUIRED_FOR_DELETE | HA_NULL_IN_KEY |
           HA_NULL_PART_KEY;
  }

  /**
    Operations supported by indexes.
  */
  ulong index_flags(uint idx, uint part, bool all_parts) const override;

  enum ha_key_alg get_default_index_algorithm() const override {
    return HA_KEY_ALG_HASH;
  }

  uint max_supported_record_length() const override {
    return HA_MAX_REC_LENGTH;
  }

  uint max_supported_keys() const override { return MAX_KEY; }

  uint max_supported_key_parts() const override { return MAX_REF_PARTS; }

  uint max_supported_key_length() const override { return MAX_KEY_LENGTH; }

  uint max_supported_key_part_length(HA_CREATE_INFO *create_info
                                     [[maybe_unused]]) const override {
    return MAX_KEY_LENGTH;
  }

  int index_init(uint idx, bool sorted) override;
  int index_end() override;
  int index_read(uchar *buf, const uchar *key, uint key_len,
                 enum ha_rkey_function find_flag) override;
  int index_next(uchar *buf) override;
  int index_next_same(uchar *buf, const uchar *key, uint keylen) override;

  ha_rows estimate_rows_upper_bound() override { return HA_POS_ERROR; }

  double scan_time() override { return 1.0; }

  /**
    Open a performance schema table.
    @param name the table to open
    @param mode unused
    @param test_if_locked unused
    @param table_def unused
    @return 0 on success
  */
  int open(const char *name, int mode, uint test_if_locked,
           const dd::Table *table_def) override;

  /**
    Close a table handle.
    @sa open.
  */
  int close() override;

  /**
    Write a row.
    @param buf the row to write
    @return 0 on success
  */
  int write_row(uchar *buf) override;

  void use_hidden_primary_key() override;

  /**
    Update a row.
    @param old_data the row old values
    @param new_data the row new values
    @return 0 on success
  */
  int update_row(const uchar *old_data, uchar *new_data) override;

  /**
    Delete a row.
    @param buf the row to delete
    @return 0 on success
  */
  int delete_row(const uchar *buf) override;

  int rnd_init(bool scan) override;

  /**
    Scan end.
    @sa rnd_init.
  */
  int rnd_end() override;

  /**
    Iterator, fetch the next row.
    @param[out] buf the row fetched.
    @return 0 on success
  */
  int rnd_next(uchar *buf) override;

  /**
    Iterator, fetch the row at a given position.
    @param[out] buf the row fetched.
    @param pos the row position
    @return 0 on success
  */
  int rnd_pos(uchar *buf, uchar *pos) override;

  /**
    Read the row current position.
    @param record the current row
  */
  void position(const uchar *record) override;

  int info(uint) override;

  int delete_all_rows() override;

  int truncate(dd::Table *table_def) override;

  int delete_table(const char *from, const dd::Table *table_def) override;

  int rename_table(const char *from, const char *to,
                   const dd::Table *from_table_def,
                   dd::Table *to_table_def) override;

  int create(const char *name, TABLE *table_arg, HA_CREATE_INFO *create_info,
             dd::Table *table_def) override;

  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type) override;

  void print_error(int error, myf errflags) override;

 private:
  /**
     Check if the caller is a replication thread or the caller is called
     by a client thread executing base64 encoded BINLOG statement.

     In theory, performance schema tables are not supposed to be replicated.
     This is true and enforced starting with MySQL 5.6.10.
     In practice, in previous versions such as MySQL 5.5 (GA) or earlier 5.6
     (non GA) DML on performance schema tables could end up written in the
     binlog,
     both in STATEMENT and ROW format.
     While these records are not supposed to be there, they are found when:
     - performing replication from a 5.5 master to a 5.6 slave during
       upgrades
     - performing replication from 5.5 (performance_schema enabled)
       to a 5.6 slave
     - performing point in time recovery in 5.6 with old archived logs.

     This API detects when the code calling the performance schema storage
     engine is a slave thread or whether the code calling is the client thread
     executing a BINLOG statement.

     This API acts as a late filter for the above mentioned cases.

     For ROW format, @see Rows_log_event::do_apply_event()

  */
  bool is_executed_by_slave() const;

  /** MySQL lock */
  THR_LOCK_DATA m_thr_lock;
  /** Performance schema table share for this table handler. */
  PFS_engine_table_share *m_table_share;
  /** Performance schema table cursor. */
  PFS_engine_table *m_table;
};

/** @} (end of group performance_schema_engine) */
#endif
