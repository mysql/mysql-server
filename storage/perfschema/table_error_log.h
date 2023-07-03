/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#ifndef TABLE_ERROR_LOG_H
#define TABLE_ERROR_LOG_H

/**
  @file storage/perfschema/table_error_log.h
  TABLE ERROR_LOG (declarations for table,
  indices, and keys).
*/

#include <sys/types.h>
#include <time.h>

#include "my_inttypes.h"
#include "mysql/components/services/log_shared.h"
#include "sql/server_component/log_sink_perfschema.h"
#include "storage/perfschema/cursor_by_error_log.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/table_helper.h"

/**
  @addtogroup performance_schema_error_log
  @{
*/

/*
  These values have a fixed relationship with
  (SYSTEM|ERROR|WARNING|INFORMATION)_LEVEL
  from <my_loglevel.h> and must not be changed
  except in response to changes in that header.
*/
enum enum_prio {
  PS_ERROR_LOG_PRIO_SYSTEM = 1,
  PS_ERROR_LOG_PRIO_ERROR = 2,
  PS_ERROR_LOG_PRIO_WARNING = 3,
  PS_ERROR_LOG_PRIO_NOTE = 4
};

/**
  Key for the LOGGED (timestamp/primary key) column.

  We process these values as ulongongs, so inherit from PFS_key_ulonglong.
  The keys are stored as TIMESTAMP(6) however, so we use a custom reader
  that reads that format and returns a ulonglong.
*/
class PFS_key_error_log_logged : public PFS_key_ulonglong {
 public:
  explicit PFS_key_error_log_logged(const char *name)
      : PFS_key_ulonglong(name) {}

  ~PFS_key_error_log_logged() override = default;

  void read(PFS_key_reader &reader, enum ha_rkey_function find_flag) override {
    m_find_flag = reader.read_timestamp(find_flag, m_is_null, &m_key_value, 6);
  }

  bool match(const log_sink_pfs_event *row);

 private:
  ulonglong m_key_value;  // TIMESTAMP(6) (microsecond precision) as ulonglong
};

/// index on the LOGGED (timestamp/primary key) column
class PFS_index_error_log_by_logged : public PFS_index_error_log {
 public:
  PFS_index_error_log_by_logged()
      : PFS_index_error_log(&m_key), m_key("LOGGED") {}

  ~PFS_index_error_log_by_logged() override = default;

  bool match(log_sink_pfs_event *row) override;

 private:
  PFS_key_error_log_logged m_key;
};

/// key for the THREAD_ID column
class PFS_key_error_log_thread_id : public PFS_key_ulonglong {
 public:
  explicit PFS_key_error_log_thread_id(const char *name)
      : PFS_key_ulonglong(name) {}

  ~PFS_key_error_log_thread_id() override = default;

  bool match(const log_sink_pfs_event *row);
};

/// index on the THREAD_ID column
class PFS_index_error_log_by_thread_id : public PFS_index_error_log {
 public:
  PFS_index_error_log_by_thread_id()
      : PFS_index_error_log(&m_key), m_key("THREAD_ID") {}

  ~PFS_index_error_log_by_thread_id() override = default;

  bool match(log_sink_pfs_event *row) override;

 private:
  PFS_key_error_log_thread_id m_key;
};

/// key for the PRIO column
class PFS_key_error_log_prio : public PFS_key_object_type_enum {
 public:
  explicit PFS_key_error_log_prio(const char *name)
      : PFS_key_object_type_enum(name), m_prio(PS_ERROR_LOG_PRIO_ERROR) {}

  ~PFS_key_error_log_prio() override = default;

  void read(PFS_key_reader &reader, enum ha_rkey_function find_flag) override;

  bool match(const log_sink_pfs_event *row);

 private:
  enum enum_prio m_prio;
};

/// index on the PRIO column
class PFS_index_error_log_by_prio : public PFS_index_error_log {
 public:
  PFS_index_error_log_by_prio() : PFS_index_error_log(&m_key), m_key("PRIO") {}

  ~PFS_index_error_log_by_prio() override = default;

  bool match(log_sink_pfs_event *row) override;

 private:
  PFS_key_error_log_prio m_key;
};

/// key for the ERROR_CODE column
class PFS_index_error_log_by_error_code : public PFS_index_error_log {
 public:
  PFS_index_error_log_by_error_code()
      : PFS_index_error_log(&m_key), m_key("ERROR_CODE") {}

  ~PFS_index_error_log_by_error_code() override = default;

  bool match(log_sink_pfs_event *row) override;

 private:
  PFS_key_name m_key;
};

/// index on the ERROR_CODE column
class PFS_index_error_log_by_subsys : public PFS_index_error_log {
 public:
  PFS_index_error_log_by_subsys()
      : PFS_index_error_log(&m_key), m_key("SUBSYSTEM") {}

  ~PFS_index_error_log_by_subsys() override = default;

  bool match(log_sink_pfs_event *row) override;

 private:
  PFS_key_name m_key;
};

/** Table PERFORMANCE_SCHEMA.ERROR_LOG. */
class table_error_log : public cursor_by_error_log {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  /** Table builder */
  static PFS_engine_table *create(PFS_engine_table_share *);

 protected:
  /** Fill in a row's fields from this class's buffer. */
  int read_row_values(TABLE *table, unsigned char *buf, Field **fields,
                      bool read_all) override;

 protected:
  table_error_log();
  /** Create an index for the column with the ordinal idx. */
  int index_init(uint idx, bool sorted) override;

 public:
  ~table_error_log() override = default;

 private:
  /** Copy an event from the ring-buffer into this class's buffer. */
  int make_row(log_sink_pfs_event *e) override;

  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;

  /** Current row. */
  log_sink_pfs_event m_header;   ///< event-header copied from ring-buffer
  char m_message[LOG_BUFF_MAX];  ///< message (DATA column) from ring-buffer
};

/** @} */

#endif
