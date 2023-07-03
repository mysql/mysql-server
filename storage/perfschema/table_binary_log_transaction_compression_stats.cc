/*
  Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

/**
  @file storage/perfschema/table_binary_log_transaction_compression_stats.cc
  Table table_binary_log_transaction_compression_stats (implementation).
*/

#include "storage/perfschema/table_binary_log_transaction_compression_stats.h"

#include <assert.h>
#include <cmath>

#include <stddef.h>

#include "my_compiler.h"

#include "sql/binlog/global.h"
#include "sql/binlog/monitoring/context.h"
#include "sql/field.h"
#include "sql/plugin_table.h"
#include "sql/table.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/table_helper.h"
#include "thr_lock.h"

struct st_binary_log_transaction_compression_stats {
  std::vector<binlog::monitoring::Compression_stats *> stats;

  void clear() {
    for (auto *x : stats) delete x;
    stats.clear();
  }

  void update() {
    clear();

    // refresh
    binlog::global_context.monitoring_context()
        .transaction_compression()
        .get_stats(stats);
  }

  void reset() {
    clear();

    binlog::global_context.monitoring_context()
        .transaction_compression()
        .reset();
  }
  ~st_binary_log_transaction_compression_stats() { clear(); }
};

struct st_binary_log_transaction_compression_stats m_rows;

THR_LOCK table_binary_log_transaction_compression_stats::m_table_lock;

Plugin_table table_binary_log_transaction_compression_stats::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "binary_log_transaction_compression_stats",
    /* Definition */
    " LOG_TYPE ENUM('BINARY', 'RELAY') NOT NULL"
    "   COMMENT \"The log type to which the transactions were written.\",\n"

    " COMPRESSION_TYPE VARCHAR(64) NOT NULL\n"
    "   COMMENT \"The transaction compression algorithm used.\",\n"

    " TRANSACTION_COUNTER BIGINT UNSIGNED NOT NULL"
    "   COMMENT \"Number of transactions written to the log\",\n"

    " COMPRESSED_BYTES_COUNTER BIGINT UNSIGNED NOT NULL"
    "   COMMENT \"The total number of bytes compressed.\",\n"

    " UNCOMPRESSED_BYTES_COUNTER BIGINT UNSIGNED NOT NULL"
    "   COMMENT \"The total number of bytes uncompressed.\",\n"

    " COMPRESSION_PERCENTAGE SMALLINT SIGNED NOT NULL"
    "   COMMENT \"The compression ratio as a percentage.\",\n"

    " FIRST_TRANSACTION_ID TEXT"
    "   COMMENT \"The first transaction written.\",\n"

    " FIRST_TRANSACTION_COMPRESSED_BYTES BIGINT UNSIGNED NOT NULL"
    "   COMMENT \"First transaction written compressed bytes.\",\n"

    " FIRST_TRANSACTION_UNCOMPRESSED_BYTES BIGINT UNSIGNED NOT NULL"
    "   COMMENT \"First transaction written uncompressed bytes.\",\n"

    " FIRST_TRANSACTION_TIMESTAMP TIMESTAMP(6)"
    "   COMMENT \"When the first transaction was written.\",\n"

    " LAST_TRANSACTION_ID TEXT"
    "   COMMENT \"The last transaction written.\",\n"

    " LAST_TRANSACTION_COMPRESSED_BYTES BIGINT UNSIGNED NOT NULL"
    "   COMMENT \"Last transaction written compressed bytes.\",\n"

    " LAST_TRANSACTION_UNCOMPRESSED_BYTES BIGINT UNSIGNED NOT NULL"
    "   COMMENT \"Last transaction written uncompressed bytes.\",\n"

    " LAST_TRANSACTION_TIMESTAMP TIMESTAMP(6)"
    "   COMMENT \"When the last transaction was written.\"\n",

    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_binary_log_transaction_compression_stats::m_share =
    {
        &pfs_truncatable_acl,
        &table_binary_log_transaction_compression_stats::create,
        nullptr, /* write_row */
        table_binary_log_transaction_compression_stats::delete_all_rows,
        table_binary_log_transaction_compression_stats::get_row_count,
        sizeof(pos_t), /* ref length */
        &m_table_lock,
        &m_table_def,
        true, /* perpetual */
        PFS_engine_table_proxy(),
        {0},
        false /* m_in_purgatory */
};

PFS_engine_table *table_binary_log_transaction_compression_stats::create(
    PFS_engine_table_share *) {
  return new table_binary_log_transaction_compression_stats();
}

table_binary_log_transaction_compression_stats::
    table_binary_log_transaction_compression_stats()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {}

table_binary_log_transaction_compression_stats::
    ~table_binary_log_transaction_compression_stats() = default;

void table_binary_log_transaction_compression_stats::reset_position() {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

ha_rows table_binary_log_transaction_compression_stats::get_row_count() {
  return m_rows.stats.size();
}

int table_binary_log_transaction_compression_stats::delete_all_rows() {
  m_rows.reset();
  return 0;
}

int table_binary_log_transaction_compression_stats::rnd_next() {
  m_rows.update();
  const uint row_count = get_row_count();
  for (m_pos.set_at(&m_next_pos); m_pos.m_index < row_count; m_pos.next()) {
    m_next_pos.set_after(&m_pos);
    return 0;
  }

  return HA_ERR_END_OF_FILE;
}

int table_binary_log_transaction_compression_stats::rnd_pos(const void *pos) {
  m_rows.update();
  if (get_row_count() == 0) {
    return HA_ERR_END_OF_FILE;
  }

  set_position(pos);
  return 0;
}

int table_binary_log_transaction_compression_stats::read_row_values(
    TABLE *table, unsigned char *buf, Field **fields, bool read_all) {
  Field *f;
  buf[0] = 0;
  auto &row = m_rows.stats[m_pos.m_index];

  std::string first_trx_id, last_trx_id;
  uint64_t first_trx_compressed_bytes{0}, last_trx_compressed_bytes{0};
  uint64_t first_trx_uncompressed_bytes{0}, last_trx_uncompressed_bytes{0};
  uint64_t first_trx_timestamp{0}, last_trx_timestamp{0};

  std::tie(first_trx_id, first_trx_compressed_bytes,
           first_trx_uncompressed_bytes, first_trx_timestamp) =
      row->get_first_transaction_stats();

  std::tie(last_trx_id, last_trx_compressed_bytes, last_trx_uncompressed_bytes,
           last_trx_timestamp) = row->get_last_transaction_stats();

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index())) {
      switch (f->field_index()) {
        case 0: /** LOG_TYPE */
        {
          set_field_enum(f, row->get_log_type());
          break;
        }
        case 1: /** COMPRESSION_TYPE */
        {
          const std::string s_type =
              binary_log::transaction::compression::type_to_string(
                  row->get_type());
          set_field_varchar_utf8mb4(f, s_type.c_str(), s_type.size());
          break;
        }
        case 2: /** TRANSACTION_COUNTER */
          set_field_ulonglong(f, row->get_counter_transactions());
          break;
        case 3: /** COMPRESSED_BYTES_COUNTER */
          set_field_ulonglong(f, row->get_counter_compressed_bytes());
          break;
        case 4: /** UNCOMPRESSED_BYTES_COUNTER */
          set_field_ulonglong(f, row->get_counter_uncompressed_bytes());
          break;
        case 5: /** COMPRESSION_PERCENTAGE */
        {
          auto cbytes =
              static_cast<double>(row->get_counter_compressed_bytes());
          auto ubytes =
              static_cast<double>(row->get_counter_uncompressed_bytes());
          auto compression = 1.0 - (cbytes / ubytes);
          set_field_short(f, static_cast<short>(round(compression * 100)));
          break;
        }
        case 6: /** FIRST_TRANSACTION_ID */
          set_field_blob(f, first_trx_id.c_str(), first_trx_id.size());
          break;
        case 7: /** FIRST_TRANSACTION_COMPRESSED_BYTES */
          set_field_ulonglong(f, first_trx_compressed_bytes);
          break;
        case 8: /** FIRST_TRANSACTION_UNCOMPRESSED_BYTES */
          set_field_ulonglong(f, first_trx_uncompressed_bytes);
          break;
        case 9: /** FIRST_TRANSACTION_TIMESTAMP */
          set_field_timestamp(f, first_trx_timestamp);
          break;
        case 10: /** LAST_TRANSACTION_ID */
          set_field_blob(f, last_trx_id.c_str(), last_trx_id.size());
          break;
        case 11: /** LAST_TRANSACTION_COMPRESSED_BYTES */
          set_field_ulonglong(f, last_trx_compressed_bytes);
          break;
        case 12: /** LAST_TRANSACTION_UNCOMPRESSED_BYTES */
          set_field_ulonglong(f, last_trx_uncompressed_bytes);
          break;
        case 13: /** LAST_TRANSACTION_TIMESTAMP */
          set_field_timestamp(f, last_trx_timestamp);
          break;
        default:
          assert(false); /* purecov: inspected */
      }
    }
  }
  return 0;
}
