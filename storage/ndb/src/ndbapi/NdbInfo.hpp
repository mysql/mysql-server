/*
   Copyright (c) 2009, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef NDBINFO_HPP
#define NDBINFO_HPP

#include <ndb_global.h>
#include <ndb_types.h>
#include <ndb_version.h>

#include <util/Vector.hpp>
#include <util/BaseString.hpp>
#include <util/HashMap.hpp>

class NdbInfo
{
public:

  enum Error
  {
    ERR_NoError = 0,
    ERR_NoSuchTable = 4240,
    ERR_OutOfMemory = 4241,
    ERR_ClusterFailure = 4242,
    ERR_WrongState = 4243,
    ERR_VirtScanStart = 4244
  };

  enum class TableName
  {
    WithPrefix,
    NoPrefix
  };

  struct Column
  {
  public:

    const enum Type
    {
      String = 1,
      Number = 2,
      Number64 = 3
    } m_type;

    const Uint32 m_column_id;
    const BaseString m_name;

    Column(const char* name, Uint32 col_id, Type type);
    Column(const Column & col);
    Column & operator=(const Column & col) = delete;
  };

  class Table
  {
  public:
    // Constructor for ndbinfo tables with pre-defined table id
    Table(const char *name, Uint32 id, Uint32 rows_estimate = 0,
          bool exact_row_count = false);
    // Constructor for virtual tables
    Table(const char * table_name, const class VirtualTable* virt,
          Uint32 rows_estimate, bool exact_row_count = true,
          TableName prefixed = TableName::WithPrefix);
    Table(const Table& tab);
    const Table & operator=(const Table& tab) = delete;
    ~Table();

    const char * getName() const;
    static const Uint32 InvalidTableId = ~0;
    Uint32 getTableId() const;
    Uint32 getRowsEstimate() const { return m_rows_estimate; }
    bool rowCountIsExact() const { return m_exact_row_count; }

    bool addColumn(const Column aCol);
    unsigned columns(void) const;
    const Column* getColumn(const unsigned attributeId) const;
    const Column* getColumn(const char * name) const;

    const class VirtualTable* getVirtualTable() const;

  private:
    friend class NdbInfo;
    const BaseString m_name;
    Uint32 m_table_id;
    Uint32 m_rows_estimate;
    bool m_exact_row_count;
    bool m_use_full_prefix;
    Vector<Column*> m_columns;
    const class VirtualTable * m_virt;
  };

  NdbInfo(class Ndb_cluster_connection* connection, const char* prefix);
  bool init(void);
  ~NdbInfo();

  int openTable(const char* table_name, const Table**);
  int openTable(Uint32 tableId, const Table**);
  void closeTable(const Table* table);

  int createScanOperation(const Table*,
                          class NdbInfoScanOperation**,
                          Uint32 max_rows = 256, Uint32 max_bytes = 0);
  void releaseScanOperation(class NdbInfoScanOperation*) const;

private:
  static const size_t NUM_HARDCODED_TABLES = 2;
  unsigned m_connect_count;
  unsigned m_min_db_version;
  class Ndb_cluster_connection* m_connection;
  native_mutex_t m_mutex;
  HashMap<BaseString, Table, BaseString_get_key> m_tables;
  Table* m_tables_table;
  Table* m_columns_table;
  BaseString m_full_prefix;    // "./ndbinfo/ndb@0024"
  BaseString m_short_prefix;   // "./ndbinfo/"
  Uint32 m_id_counter;

  bool addColumn(Uint32 tableId, const Column aCol);

  bool load_ndbinfo_tables();
  bool load_hardcoded_tables(void);
  bool load_virtual_tables(void);
  bool load_tables();
  bool check_tables();
  void flush_tables();

  BaseString mysql_table_name(const NdbInfo::Table &) const;

  Vector<Table*> m_virtual_tables;

};

#include "NdbInfoScanOperation.hpp"
#include "NdbInfoRecAttr.hpp"

#endif
