/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#pragma once

#include <string>
#include "duckdb/main/physical_appender.hpp"
#include "sql/duckdb/duckdb_manager.h"
#include "sql/field.h"  // field

extern ulonglong batch_max_row_count;

class DeltaAppender {
 public:
  int append_row_insert(TABLE *table, ulonglong trx_no,
                        const MY_BITMAP *blob_type_map);

  int append_row_update(TABLE *table, ulonglong trx_no, const uchar *old_row);

  int append_row_delete(TABLE *table, ulonglong trx_no,
                        const uchar *old_row = nullptr);

  static std::string buf_table_name(std::string db, std::string tb) {
    return db + "_rds_buf_" + tb;
  }

  DeltaAppender(THD *thd, std::string db, std::string tb, bool use_tmp_table,
                bool idempotent_flag)
      : m_use_tmp_table(use_tmp_table),
        m_idempotent_flag(idempotent_flag),
        m_schema_name(db),
        m_table_name(tb),
        m_thd(thd) {}

  bool Initialize(TABLE *table);

  /* Append field under mysql format */
  int append_mysql_field(const Field *field,
                         const MY_BITMAP *blob_type_map = nullptr);

  DeltaAppender() = default;

  ~DeltaAppender() { cleanup(); }

  bool flush();

  int flush_partial_batch();

  bool rollback(ulonglong trx_no);

  void cleanup();

 private:
  void generateQuery(std::stringstream &ss, bool delete_flag);

  bool m_use_tmp_table;    // whether this trx need tmp table
  bool m_idempotent_flag;  // whether this trx need commit idempotently

  std::string m_schema_name;
  std::string m_table_name;
  std::string m_tmp_table_name;

  MY_BITMAP m_pk_bitmap;
  std::string m_pk_list{""};
  std::string m_col_list{""};
  bool m_has_gcols{false};

  uint64_t m_row_count{0};
  bool m_has_insert{false};
  bool m_has_update{false};
  bool m_has_delete{false};

  THD *m_thd;

  std::unique_ptr<duckdb::PhysicalAppender> m_appender;
};

class DeltaAppenders {
 public:
  DeltaAppenders() : m_append_infos() {}

  ~DeltaAppenders() = default;

  void delete_appender(std::string &db, std::string &tb);

  bool flush_all(std::string &error_msg);

  void reset_all();

  bool rollback_trx(ulonglong trx_no);

  bool is_empty() { return m_append_infos.empty(); }

  DeltaAppender *get_appender(THD *thd, std::string &db, std::string &tb,
                              bool insert_only, bool idempotent_flag,
                              TABLE *table);

 private:
  int append_mysql_field(duckdb::PhysicalAppender *appender,
                         const Field *field);

 private:
  std::map<std::pair<std::string, std::string>, std::unique_ptr<DeltaAppender>>
      m_append_infos;
};
