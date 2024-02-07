/*
   Copyright (c) 2009, 2024, Oracle and/or its affiliates.

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

// Implements
#include "NdbInfo.hpp"

#include <Logger.hpp>
#include <algorithm>
#include <vector>
#include "NdbInfoScanNodes.hpp"
#include "NdbInfoScanVirtual.hpp"
#include "ndbapi/ndb_cluster_connection.hpp"

NdbInfo::NdbInfo(class Ndb_cluster_connection *connection, const char *prefix)
    : m_connect_count(connection->get_connect_count()),
      m_min_db_version(0),
      m_connection(connection),
      m_tables_table(nullptr),
      m_columns_table(nullptr),
      m_full_prefix(prefix),
      m_id_counter(0) {
  m_short_prefix.assign(m_full_prefix,
                        m_full_prefix.lastIndexOf(DIR_SEPARATOR[0]) + 1);
}

bool NdbInfo::init(void) {
  if (native_mutex_init(&m_mutex, MY_MUTEX_INIT_FAST)) return false;
  if (!load_hardcoded_tables()) return false;
  if (!NdbInfoScanVirtual::create_virtual_tables(m_virtual_tables))
    return false;
  return true;
}

NdbInfo::~NdbInfo(void) {
  NdbInfoScanVirtual::delete_virtual_tables(m_virtual_tables);
  native_mutex_destroy(&m_mutex);
}

BaseString NdbInfo::mysql_table_name(const Table &table) const {
  DBUG_ENTER("mysql_table_name");
  BaseString mysql_name;
  mysql_name.assfmt(
      "%s%s",
      table.m_use_full_prefix ? m_full_prefix.c_str() : m_short_prefix.c_str(),
      table.m_name.c_str());
  DBUG_PRINT("exit", ("mysql_name: %s", mysql_name.c_str()));
  DBUG_RETURN(mysql_name);
}

bool NdbInfo::load_hardcoded_tables(void) {
  {
    Table tabs("tables", (Uint32)0, 0, true);
    if (!tabs.addColumn(Column("table_id", 0, Column::Number)) ||
        !tabs.addColumn(Column("table_name", 1, Column::String)) ||
        !tabs.addColumn(Column("comment", 2, Column::String)) ||
        !tabs.addColumn(Column("rows_estimate", 3, Column::Number)))
      return false;

    BaseString hash_key = mysql_table_name(tabs);
    if (!m_tables.insert(hash_key.c_str(), tabs)) return false;
    if (!m_tables.search(hash_key.c_str(), &m_tables_table)) return false;
  }

  {
    Table cols("columns", 1, 0, true);
    if (!cols.addColumn(Column("table_id", 0, Column::Number)) ||
        !cols.addColumn(Column("column_id", 1, Column::Number)) ||
        !cols.addColumn(Column("column_name", 2, Column::String)) ||
        !cols.addColumn(Column("column_type", 3, Column::Number)) ||
        !cols.addColumn(Column("comment", 4, Column::String)))
      return false;

    BaseString hash_key = mysql_table_name(cols);
    if (!m_tables.insert(hash_key.c_str(), cols)) return false;
    if (!m_tables.search(hash_key.c_str(), &m_columns_table)) return false;
  }

  return true;
}

bool NdbInfo::addColumn(Uint32 tableId, Column aCol) {
  // Find the table with correct id
  Table *table = nullptr;
  for (auto &key_and_value : m_tables) {
    table = key_and_value.second.get();
    if (table->m_table_id == tableId) break;
  }

  table->addColumn(aCol);

  return true;
}

bool NdbInfo::load_ndbinfo_tables(void) {
  DBUG_ENTER("load_ndbinfo_tables");
  assert(m_tables_table && m_columns_table);

  {
    // Create tables by scanning the TABLES table
    NdbInfoScanOperation *scanOp = nullptr;
    if (createScanOperation(m_tables_table, &scanOp) != 0) DBUG_RETURN(false);

    if (scanOp->readTuples() != 0) {
      releaseScanOperation(scanOp);
      DBUG_RETURN(false);
    }

    const NdbInfoRecAttr *tableIdRes = scanOp->getValue("table_id");
    const NdbInfoRecAttr *tableNameRes = scanOp->getValue("table_name");
    const NdbInfoRecAttr *estRowsRes = scanOp->getValue("rows_estimate");
    if (!tableIdRes || !tableNameRes || !estRowsRes) {
      releaseScanOperation(scanOp);
      DBUG_RETURN(false);
    }

    if (scanOp->execute() != 0) {
      releaseScanOperation(scanOp);
      DBUG_RETURN(false);
    }

    int err;
    while ((err = scanOp->nextResult()) == 1) {
      m_tables_table->m_rows_estimate++;
      Uint32 tableId = tableIdRes->u_32_value();
      const char *tableName = tableNameRes->c_str();
      Uint32 est_rows = 0;
      if (!estRowsRes->isNULL()) est_rows = estRowsRes->u_32_value();
      DBUG_PRINT("info", ("table: '%s', id: %u", tableName, tableId));
      switch (tableId) {
        case 0:
          assert(strcmp(tableName, "tables") == 0);
          break;
        case 1:
          assert(strcmp(tableName, "columns") == 0);
          break;

        default:
          Table table(tableName, tableId, est_rows);
          BaseString hash_key = mysql_table_name(table);
          if (!m_tables.insert(hash_key.c_str(), table)) {
            DBUG_PRINT("error", ("Failed to insert Table('%s', %u)", tableName,
                                 tableId));
            releaseScanOperation(scanOp);
            DBUG_RETURN(false);
          }
      }
    }
    releaseScanOperation(scanOp);

    if (err != 0) DBUG_RETURN(false);
  }

  {
    // Fill tables with columns by scanning the COLUMNS table
    NdbInfoScanOperation *scanOp = nullptr;
    if (createScanOperation(m_columns_table, &scanOp) != 0) DBUG_RETURN(false);

    if (scanOp->readTuples() != 0) {
      releaseScanOperation(scanOp);
      DBUG_RETURN(false);
    }

    const NdbInfoRecAttr *tableIdRes = scanOp->getValue("table_id");
    const NdbInfoRecAttr *columnIdRes = scanOp->getValue("column_id");
    const NdbInfoRecAttr *columnNameRes = scanOp->getValue("column_name");
    const NdbInfoRecAttr *columnTypeRes = scanOp->getValue("column_type");
    if (!tableIdRes || !columnIdRes || !columnNameRes || !columnTypeRes) {
      releaseScanOperation(scanOp);
      DBUG_RETURN(false);
    }
    if (scanOp->execute() != 0) {
      releaseScanOperation(scanOp);
      DBUG_RETURN(false);
    }

    int err;
    while ((err = scanOp->nextResult()) == 1) {
      m_columns_table->m_rows_estimate++;
      Uint32 tableId = tableIdRes->u_32_value();
      Uint32 columnId = columnIdRes->u_32_value();
      const char *columnName = columnNameRes->c_str();
      Uint32 columnType = columnTypeRes->u_32_value();
      DBUG_PRINT("info", ("tableId: %u, columnId: %u, column: '%s', type: %d",
                          tableId, columnId, columnName, columnType));
      switch (tableId) {
        case 0:
        case 1:
          // Ignore columns for TABLES and COLUMNS tables since
          // those are already known(hardcoded)
          break;

        default: {
          Column::Type type;
          switch (columnType) {
            case 1:
              type = Column::String;
              break;
            case 2:
              type = Column::Number;
              break;
            case 3:
              type = Column::Number64;
              break;
            default: {
              DBUG_PRINT("error", ("Unknown columntype: %d", columnType));
              releaseScanOperation(scanOp);
              DBUG_RETURN(false);
            }
          }

          Column column(columnName, columnId, type);

          // Find the table with given id

          if (!addColumn(tableId, column)) {
            DBUG_PRINT("error", ("Failed to add column for %d, %d, '%s', %d)",
                                 tableId, columnId, columnName, columnType));
            releaseScanOperation(scanOp);
            DBUG_RETURN(false);
          }
          break;
        }
      }
    }
    releaseScanOperation(scanOp);

    if (err != 0) DBUG_RETURN(false);
  }
  DBUG_RETURN(true);
}

bool NdbInfo::load_tables() {
  if (!load_ndbinfo_tables() || !load_virtual_tables()) {
    // Remove any dynamic tables that might have been partially created
    flush_tables();
    return false;
  }

  // Consistency check the loaded table list
  {
    std::vector<Uint32> m_table_ids;
    for (auto &key_and_value : m_tables) {
      Table *const tab = key_and_value.second.get();
      // Table id should be valid
      assert(tab->m_table_id != Table::InvalidTableId);
      // Save the table ids for later check
      m_table_ids.push_back(tab->m_table_id);
      // Name should be set
      assert(tab->m_name.length() > 0);
      // There should be columns
      assert(tab->columns() > 0);

      for (unsigned c = 0; c < tab->columns(); c++) {
        // Column id should be consecutievly increasing
        assert(tab->getColumn(c)->m_column_id == c);
        // Name should be set
        assert(tab->getColumn(c)->m_name.length() > 0);
      }
    }

    // There should be as many table ids as tables
    assert(m_table_ids.size() == m_tables.entries());

    // All table ids should be unique
    std::sort(m_table_ids.begin(), m_table_ids.end());
    for (unsigned i = 1; i < m_table_ids.size(); i++) {
      assert(m_table_ids[i - 1] != m_table_ids[i]);
    }
  }

  // After successful load of the tables, set connect count
  // and the min db version of cluster
  m_connect_count = m_connection->get_connect_count();
  m_min_db_version = m_connection->get_min_db_version();
  return true;
}

int NdbInfo::createScanOperation(const Table *table,
                                 NdbInfoScanOperation **ret_scan_op,
                                 Uint32 max_rows, Uint32 max_bytes) {
  if (table->m_virt != nullptr) {
    // The table is a virtual table which does not exist in the data nodes,
    // instead it returns hardcoded values or dynamic information about the
    // cluster which it retrieves using NdbApi functions. Use the special
    // NdbInfoScanVirtual implementation
    NdbInfoScanVirtual *virtual_scan =
        new NdbInfoScanVirtual(m_connection, table, table->m_virt);
    if (!virtual_scan) return ERR_OutOfMemory;

    const int ret = virtual_scan->init();
    if (ret != ERR_NoError) {
      delete virtual_scan;
      return ret;
    }

    *ret_scan_op = virtual_scan;
    return 0;
  }

  Uint32 max_nodes = 0;
  if (table->getTableId() < NUM_HARDCODED_TABLES) {
    // Each db node contains all rows for the table -> scan only one node
    max_nodes = 1;
  }

  NdbInfoScanNodes *scan_op =
      new NdbInfoScanNodes(m_connection, table, max_rows, max_bytes, max_nodes);
  if (!scan_op) return ERR_OutOfMemory;
  // Global id counter, not critical if you get same id on two instances
  // since reference is also part of the unique identifier.
  const int ret = scan_op->init(m_id_counter++);
  if (ret != ERR_NoError) {
    delete scan_op;
    return ret;
  }

  *ret_scan_op = scan_op;

  return 0;
}

void NdbInfo::releaseScanOperation(NdbInfoScanOperation *scan_op) const {
  delete scan_op;
}

void NdbInfo::flush_tables() {
  // Delete all but the hardcoded tables
  for (auto it = m_tables.begin(); it != m_tables.end();) {
    Table *tab = it->second.get();
    if (!(tab == m_tables_table || tab == m_columns_table))
      it = m_tables.erase(it);
    else
      ++it;
  }
  assert(m_tables.entries() == NUM_HARDCODED_TABLES);
}

bool NdbInfo::check_tables() {
  if (unlikely(m_connection->get_connect_count() != m_connect_count ||
               m_connection->get_min_db_version() != m_min_db_version)) {
    // Connect count or min db version of cluster has changed
    //  -> flush the cached table definitions
    flush_tables();
  }
  if (unlikely(m_tables.entries() <= NUM_HARDCODED_TABLES)) {
    // Global table cache is not loaded yet or has been
    // flushed, try to load it
    if (!load_tables()) {
      return false;
    }
  }
  // Make sure that some dynamic tables have been loaded
  assert(m_tables.entries() > NUM_HARDCODED_TABLES);
  return true;
}

int NdbInfo::openTable(const char *table_name,
                       const NdbInfo::Table **table_copy) {
  native_mutex_lock(&m_mutex);

  if (!check_tables()) {
    native_mutex_unlock(&m_mutex);
    return ERR_ClusterFailure;
  }

  Table *tab;
  if (!m_tables.search(table_name, &tab)) {
    // No such table existed
    native_mutex_unlock(&m_mutex);
    return ERR_NoSuchTable;
  }

  // Return a _copy_ of the table
  *table_copy = new Table(*tab);

  native_mutex_unlock(&m_mutex);
  return 0;
}

int NdbInfo::openTable(Uint32 tableId, const NdbInfo::Table **table_copy) {
  native_mutex_lock(&m_mutex);

  if (!check_tables()) {
    native_mutex_unlock(&m_mutex);
    return ERR_ClusterFailure;
  }

  // Find the table with correct id
  const Table *table = nullptr;
  for (auto &key_and_value : m_tables) {
    const Table *tmp = key_and_value.second.get();
    if (tmp->m_table_id == tableId) {
      table = tmp;
      break;
    }
  }
  if (table == nullptr) {
    // No such table existed
    native_mutex_unlock(&m_mutex);
    return ERR_NoSuchTable;
  }

  // Return a _copy_ of the table
  *table_copy = new Table(*table);

  native_mutex_unlock(&m_mutex);
  return 0;
}

void NdbInfo::closeTable(const Table *table) { delete table; }

// Column

NdbInfo::Column::Column(const char *name, Uint32 col_id,
                        NdbInfo::Column::Type type)
    : m_type(type), m_column_id(col_id), m_name(name) {}

NdbInfo::Column::Column(const NdbInfo::Column &col)
    : m_type(col.m_type), m_column_id(col.m_column_id), m_name(col.m_name) {}

// Table

NdbInfo::Table::Table(const char *name, Uint32 id, Uint32 est_rows,
                      bool exact_row_count)
    : m_name(name),
      m_table_id(id),
      m_rows_estimate(est_rows),
      m_exact_row_count(exact_row_count),
      m_use_full_prefix(true),
      m_virt(nullptr) {}

NdbInfo::Table::Table(const char *table_name, const VirtualTable *virt,
                      Uint32 est_rows, bool exact_row_count, TableName prefixed)
    : m_name(table_name),
      m_table_id(InvalidTableId),
      m_rows_estimate(est_rows),
      m_exact_row_count(exact_row_count),
      m_use_full_prefix((prefixed == TableName::WithPrefix)),
      m_virt(virt) {
  assert(virt);  // constructor for virtual tables only
  assert(m_rows_estimate);
  assert(m_exact_row_count || (m_rows_estimate > 2));
}

NdbInfo::Table::Table(const NdbInfo::Table &tab)
    : m_name(tab.m_name),
      m_table_id(tab.m_table_id),
      m_rows_estimate(tab.m_rows_estimate),
      m_exact_row_count(tab.m_exact_row_count),
      m_virt(tab.m_virt) {
  DBUG_ENTER("Table(const Table&");
  for (unsigned i = 0; i < tab.m_columns.size(); i++)
    addColumn(*tab.m_columns[i]);
  DBUG_VOID_RETURN;
}

NdbInfo::Table::~Table() {
  for (unsigned i = 0; i < m_columns.size(); i++) delete m_columns[i];
}

const char *NdbInfo::Table::getName() const { return m_name.c_str(); }

Uint32 NdbInfo::Table::getTableId() const { return m_table_id; }

bool NdbInfo::Table::addColumn(const NdbInfo::Column aCol) {
  NdbInfo::Column *col = new NdbInfo::Column(aCol);
  if (col == nullptr) {
    errno = ENOMEM;
    return false;
  }

  if (m_columns.push_back(col)) {
    delete col;
    return false;
  }
  return true;
}

unsigned NdbInfo::Table::columns(void) const { return m_columns.size(); }

const NdbInfo::Column *NdbInfo::Table::getColumn(
    const unsigned attributeId) const {
  return (attributeId < m_columns.size()) ? m_columns[attributeId] : nullptr;
}

const NdbInfo::Column *NdbInfo::Table::getColumn(const char *name) const {
  DBUG_ENTER("Column::getColumn");
  DBUG_PRINT("info", ("columns: %d", m_columns.size()));
  const NdbInfo::Column *column = nullptr;
  for (uint i = 0; i < m_columns.size(); i++) {
    DBUG_PRINT("info", ("col: %d %s", i, m_columns[i]->m_name.c_str()));
    if (strcmp(m_columns[i]->m_name.c_str(), name) == 0) {
      column = m_columns[i];
      break;
    }
  }
  DBUG_RETURN(column);
}

const VirtualTable *NdbInfo::Table::getVirtualTable() const { return m_virt; }

bool NdbInfo::load_virtual_tables(void) {
  // The virtual tables should already have been created
  assert(m_virtual_tables.size() > 0);

  // Append the virtual tables to the list of tables
  for (size_t i = 0; i < m_virtual_tables.size(); i++) {
    Table *tab = m_virtual_tables[i];
    assert(tab->m_virt);
    const BaseString hash_key = mysql_table_name(*tab);
    if (m_tables.remove(hash_key)) {
      fprintf(stderr,
              "%s NDBAPI FATAL ERROR : NdbInfo : Duplicate table name: %s\n",
              Logger::Timestamp().c_str(), hash_key.c_str());
      abort();
      return false;
    }
    tab->m_table_id = Table::VirtualTableIdBit | i;
    if (!m_tables.insert(hash_key.c_str(), *tab)) return false;
  }

  return true;
}

template class Vector<NdbInfo::Column *>;
