/*
   Copyright 2009, 2010 Sun Microsystems, Inc.
   All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

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
    ERR_NoSuchTable = 40,
    ERR_OutOfMemory = 41,
    ERR_ClusterFailure = 42,
    ERR_WrongState = 43
  };

  struct Column
  {
  public:

    enum Type
    {
      String = 1,
      Number = 2,
      Number64 = 3
    } m_type;

    Uint32 m_column_id;
    BaseString m_name;

    Column(const char* name, Uint32 col_id, Type type);
    Column(const Column & col);
    Column & operator=(const Column & col);
  };

  class Table
  {
  public:

    Table(const char *name, Uint32 id);
    Table(const Table& tab);
    const Table & operator=(const Table& tab);
    ~Table();

    const char * getName() const;
    Uint32 getTableId() const;

    bool addColumn(const Column aCol);
    unsigned columns(void) const;
    const Column* getColumn(const unsigned attributeId) const;
    const Column* getColumn(const char * name) const;

  private:
    friend class NdbInfo;
    BaseString m_name;
    Uint32 m_table_id;
    Vector<Column*> m_columns;
  };

  NdbInfo(class Ndb_cluster_connection* connection,
          const char* prefix, const char* dbname = "",
          const char* table_prefix = "");
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
  pthread_mutex_t m_mutex;
  HashMap<BaseString, Table, BaseString_get_key> m_tables;
  Table* m_tables_table;
  Table* m_columns_table;
  BaseString m_prefix;
  BaseString m_dbname;
  BaseString m_table_prefix;
  Uint32 m_id_counter;

  bool addColumn(Uint32 tableId, const Column aCol);

  bool load_ndbinfo_tables();
  bool load_hardcoded_tables(void);
  bool load_tables();
  bool check_tables();
  void flush_tables();

  BaseString mysql_table_name(const char* table_name) const;

};

#include "NdbInfoScanOperation.hpp"
#include "NdbInfoRecAttr.hpp"

#endif
