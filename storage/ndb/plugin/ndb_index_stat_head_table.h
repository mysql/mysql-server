/*
   Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_INDEX_STAT_HEAD_TABLE_H
#define NDB_INDEX_STAT_HEAD_TABLE_H

#include "storage/ndb/plugin/ndb_util_table.h"

class Ndb_index_stat_head_table : public Ndb_util_table {
  Ndb_index_stat_head_table() = delete;
  Ndb_index_stat_head_table(const Ndb_index_stat_head_table &) = delete;

  bool define_table_ndb(NdbDictionary::Table &table,
                        unsigned mysql_version) const override;

 public:
  Ndb_index_stat_head_table(class Thd_ndb *);
  virtual ~Ndb_index_stat_head_table() {}

  bool check_schema() const override;
  bool need_upgrade() const override;
  std::string define_table_dd() const override;
  bool create_events_in_NDB(const NdbDictionary::Table &table) const override;
  bool drop_events_in_NDB() const override { return true; }
};

#endif
