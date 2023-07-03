/*
   Copyright (c) 2013, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef TABLE_REPLICATION_APPLIER_CONFIGURATION_H
#define TABLE_REPLICATION_APPLIER_CONFIGURATION_H

/**
  @file storage/perfschema/table_replication_applier_configuration.h
  Table replication_applier_configuration (declarations).
*/

#include <sys/types.h>
#include <time.h>

#include "my_base.h"
#include "sql/rpl_info.h" /*CHANNEL_NAME_LENGTH*/
#include "sql/rpl_rli.h"  /*enum_require_table_primary_key*/
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/table_helper.h"

class Field;
class Master_info;
class Plugin_table;
struct TABLE;
struct THR_LOCK;

#ifndef ENUM_RPL_YES_NO
#define ENUM_RPL_YES_NO
enum enum_rpl_yes_no { PS_RPL_YES = 1, PS_RPL_NO };
#endif

/**
  @addtogroup performance_schema_tables
  @{
*/

/** A row in the table*/
struct st_row_applier_config {
  char channel_name[CHANNEL_NAME_LENGTH];
  uint channel_name_length;
  time_t desired_delay;
  bool desired_delay_is_set;
  std::string privilege_checks_user;
  enum_rpl_yes_no requires_row_format;
  Relay_log_info::enum_require_table_primary_key
      require_table_primary_key_check;
  Assign_gtids_to_anonymous_transactions_info::enum_type
      assign_gtids_to_anonymous_transactions_type;
  std::string assign_gtids_to_anonymous_transactions_value;
};

class PFS_index_rpl_applier_config : public PFS_engine_index {
 public:
  PFS_index_rpl_applier_config()
      : PFS_engine_index(&m_key), m_key("CHANNEL_NAME") {}

  ~PFS_index_rpl_applier_config() override = default;

  virtual bool match(Master_info *mi);

 private:
  PFS_key_name m_key;
};

/** Table PERFORMANCE_SCHEMA.replication_applier_configuration */
class table_replication_applier_configuration : public PFS_engine_table {
  typedef PFS_simple_index pos_t;

 private:
  int make_row(Master_info *mi);

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row */
  st_row_applier_config m_row;
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

  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;

  table_replication_applier_configuration();

 public:
  ~table_replication_applier_configuration() override;

  /** Table share. */
  static PFS_engine_table_share m_share;
  static PFS_engine_table *create(PFS_engine_table_share *);
  static ha_rows get_row_count();

  void reset_position() override;

  int rnd_next() override;
  int rnd_pos(const void *pos) override;

  int index_init(uint idx, bool sorted) override;
  int index_next() override;

 private:
  PFS_index_rpl_applier_config *m_opened_index;
};

/** @} */
#endif
