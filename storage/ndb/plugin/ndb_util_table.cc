/*
   Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

// Implements the interface defined in
#include "storage/ndb/plugin/ndb_util_table.h"

#include <cstring>
#include <memory>
#include <utility>

#include "my_base.h"
#include "my_byteorder.h"  // uint2korr
#include "mysql_version.h"
#include "ndbapi/NdbRecAttr.hpp"  // NdbRecAttr
#include "sql/sql_class.h"        // THD
#include "storage/ndb/plugin/ha_ndbcluster_binlog.h"
#include "storage/ndb/plugin/ndb_dbname_guard.h"
#include "storage/ndb/plugin/ndb_dd_client.h"
#include "storage/ndb/plugin/ndb_dd_table.h"
#include "storage/ndb/plugin/ndb_local_connection.h"
#include "storage/ndb/plugin/ndb_log.h"
#include "storage/ndb/plugin/ndb_ndbapi_util.h"
#include "storage/ndb/plugin/ndb_require.h"
#include "storage/ndb/plugin/ndb_schema_trans_guard.h"
#include "storage/ndb/plugin/ndb_tdc.h"
#include "storage/ndb/plugin/ndb_thd_ndb.h"

class Util_table_creator {
  THD *const m_thd;
  Thd_ndb *const m_thd_ndb;
  Ndb_util_table &m_util_table;
  std::string m_name;

  const char *db_name() const { return m_util_table.db_name(); }
  const char *table_name() const { return m_util_table.table_name(); }

  bool create_or_upgrade_in_NDB(bool upgrade_allowed, bool &reinstall) const;

  bool install_in_DD(bool reinstall);

  bool setup_table_for_binlog() const;

 public:
  Util_table_creator(THD *, Thd_ndb *, Ndb_util_table &);
  Util_table_creator() = delete;
  Util_table_creator(const Util_table_creator &) = delete;

  bool create_or_upgrade(bool upgrade_allowed, bool create_events);
};

Ndb_util_table::Ndb_util_table(Thd_ndb *thd_ndb, std::string db_name,
                               std::string table_name, bool hidden, bool events)
    : m_thd_ndb(thd_ndb),
      m_table_guard(thd_ndb->ndb),
      m_db_name(std::move(db_name)),
      m_table_name(std::move(table_name)),
      m_hidden(hidden),
      m_create_events(events) {}

Ndb_util_table::~Ndb_util_table() {}

const THD *Ndb_util_table::get_thd() const { return m_thd_ndb->get_thd(); }

Ndb *Ndb_util_table::get_ndb() const { return m_thd_ndb->ndb; }

bool Ndb_util_table::create_or_upgrade(THD *thd, bool upgrade_flag) {
  Util_table_creator creator(thd, m_thd_ndb, *this);
  return creator.create_or_upgrade(upgrade_flag, m_create_events);
}

void Ndb_util_table::push_warning(const char *fmt, ...) const {
  // Assemble the message
  char message[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);

  m_thd_ndb->push_warning("[%s.%s] %s", m_db_name.c_str(), m_table_name.c_str(),
                          message);
}

void Ndb_util_table::push_ndb_error_warning(const NdbError &ndb_err) const {
  push_warning("NDB error: %d %s", ndb_err.code, ndb_err.message);
}

bool Ndb_util_table::exists() const {
  // Load up the table definition from NDB dictionary
  Ndb_table_guard ndb_tab(m_thd_ndb->ndb, m_db_name.c_str(),
                          m_table_name.c_str());

  if (ndb_tab.get_table() == nullptr) {
    // Table does not exist in NDB
    return false;
  }

  // Table exists in NDB
  return true;
}

bool Ndb_util_table::open(bool reload_table) {
  if (unlikely(reload_table)) {
    assert(m_table_guard.get_table() != nullptr);
    // Reload the table definition from NDB dictionary
    m_table_guard.invalidate();
    m_table_guard.reinit(m_db_name.c_str(), m_table_name.c_str());
  } else {
    // Load up the table definition from NDB dictionary
    m_table_guard.init(m_db_name.c_str(), m_table_name.c_str());
  }

  const NdbDictionary::Table *tab = m_table_guard.get_table();
  if (!tab) {
    push_warning("Failed to open table %s.%s from NDB", m_db_name.c_str(),
                 m_table_name.c_str());
    return false;
  }

  return true;
}

const NdbDictionary::Table *Ndb_util_table::get_table() const {
  return m_table_guard.get_table();
}

const NdbDictionary::Column *Ndb_util_table::get_column(
    const char *name) const {
  return get_table()->getColumn(name);
}

const NdbDictionary::Column *Ndb_util_table::get_column_by_number(
    Uint32 number) const {
  return get_table()->getColumn(number);
}

bool Ndb_util_table::check_column_exist(const char *name) const {
  if (get_column(name) == nullptr) {
    push_warning("Could not find expected column '%s'", name);
    return false;
  }
  return true;
}

bool Ndb_util_table::check_primary_key(
    const std::vector<const char *> columns) const {
  // Check that the primary key of the table matches the given columns
  int keys = 0;
  for (const char *name : columns) {
    if (!get_column(name)->getPrimaryKey()) {
      push_warning("Column '%s' is not part of primary key", name);
      return false;
    }
    keys++;
  }
  if (keys != get_table()->getNoOfPrimaryKeys()) {
    push_warning("Invalid primary key");
    return false;
  }
  return true;
}

int Ndb_util_table::get_column_max_length(const char *name) const {
  return get_column(name)->getLength();
}

bool Ndb_util_table::check_column_type(const NdbDictionary::Column *col,
                                       NdbDictionary::Column::Type type,
                                       const char *type_name) const {
  if (col->getType() != type) {
    push_warning("Column '%s' must be defined as '%s'", col->getName(),
                 type_name);
    return false;
  }
  return true;
}

bool Ndb_util_table::check_column_minlength(const char *name,
                                            int min_length) const {
  if (get_column(name)->getLength() < min_length) {
    push_warning("Column '%s' is too short, need at least %d bytes", name,
                 min_length);
    return false;
  }
  return true;
}

bool Ndb_util_table::check_column_varbinary(const char *name) const {
  return check_column_type(get_column(name), NdbDictionary::Column::Varbinary,
                           "VARBINARY");
}

bool Ndb_util_table::check_column_varchar(const char *name) const {
  return check_column_type(get_column(name), NdbDictionary::Column::Varchar,
                           "VARCHAR");
}

bool Ndb_util_table::check_column_binary(const char *name) const {
  return check_column_type(get_column(name), NdbDictionary::Column::Binary,
                           "BINARY");
}

bool Ndb_util_table::check_column_unsigned(const char *name) const {
  return check_column_type(get_column(name), NdbDictionary::Column::Unsigned,
                           "INT UNSIGNED ");
}

bool Ndb_util_table::check_column_bigunsigned(const char *name) const {
  return check_column_type(get_column(name), NdbDictionary::Column::Bigunsigned,
                           "BIGINT UNSIGNED");
}

bool Ndb_util_table::check_column_blob(const char *name) const {
  return check_column_type(get_column(name), NdbDictionary::Column::Blob,
                           "BLOB");
}

bool Ndb_util_table::check_column_nullable(const char *name,
                                           bool nullable) const {
  if (get_column(name)->getNullable() != nullable) {
    push_warning("Column '%s' must be defined to %sallow NULL values", name,
                 nullable ? "" : "not ");
    return false;
  }
  return true;
}

bool Ndb_util_table::define_table_add_column(
    NdbDictionary::Table &new_table,
    const NdbDictionary::Column &new_column) const {
  if (new_table.addColumn(new_column) != 0) {
    push_warning("Failed to add column '%s'", new_column.getName());
    return false;
  }
  return true;
}

bool Ndb_util_table::create_indexes(const NdbDictionary::Table &) const {
  // Base class implementation. Override in derived classes to define indexes.
  return true;
}

bool Ndb_util_table::create_index(const NdbDictionary::Table &new_table,
                                  const NdbDictionary::Index &new_index) const {
  NdbDictionary::Dictionary *dict = m_thd_ndb->ndb->getDictionary();
  if (dict->createIndex(new_index, new_table) != 0) {
    push_ndb_error_warning(dict->getNdbError());
    push_warning("Failed to create index '%s'", new_index.getName());
    return false;
  }
  return true;
}

bool Ndb_util_table::create_event_in_NDB(
    const NdbDictionary::Event &new_event) const {
  NdbDictionary::Dictionary *dict = m_thd_ndb->ndb->getDictionary();
  if (dict->createEvent(new_event) != 0) {
    push_ndb_error_warning(dict->getNdbError());
    push_warning("Failed to create event '%s'", new_event.getName());
    return false;
  }
  return true;
}

bool Ndb_util_table::create_primary_ordered_index(
    const NdbDictionary::Table &new_table) const {
  NdbDictionary::Index index("PRIMARY");

  index.setType(NdbDictionary::Index::OrderedIndex);
  index.setLogging(false);

  for (int i = 0; i < new_table.getNoOfPrimaryKeys(); i++) {
    index.addColumnName(new_table.getPrimaryKey(i));
  }
  return create_index(new_table, index);
}

bool Ndb_util_table::create_table_in_NDB(
    const NdbDictionary::Table &new_table) const {
  // Set correct database name on the Ndb object
  const Ndb_dbname_guard db_guard(m_thd_ndb->ndb, m_db_name.c_str());

  NdbDictionary::Dictionary *dict = m_thd_ndb->ndb->getDictionary();
  if (dict->createTable(new_table) != 0) {
    push_ndb_error_warning(dict->getNdbError());
    push_warning("Failed to create table '%s'", new_table.getName());
    return false;
  }
  return true;
}

bool Ndb_util_table::drop_table_in_NDB(
    const NdbDictionary::Table &old_table) const {
  // Set correct database name on the Ndb object
  const Ndb_dbname_guard db_guard(m_thd_ndb->ndb, m_db_name.c_str());
  NdbDictionary::Dictionary *dict = m_thd_ndb->ndb->getDictionary();

  if (!drop_events_in_NDB()) {
    push_warning("Failed to drop events for table '%s'", m_table_name.c_str());
    return false;
  }

  if (dict->dropTableGlobal(old_table) != 0) {
    push_ndb_error_warning(dict->getNdbError());
    push_warning("Failed to drop table '%s'", old_table.getName());
    return false;
  }

  return true;
}

bool Ndb_util_table::drop_event_in_NDB(const char *event_name) const {
  NdbDictionary::Dictionary *dict = m_thd_ndb->ndb->getDictionary();
  if (dict->dropEvent(event_name) != 0) {
    if (dict->getNdbError().code == 4710 || dict->getNdbError().code == 1419) {
      // Failed to drop event but return code says it was
      // because the event didn't exist -> all ok
      return true;
    }
    push_ndb_error_warning(dict->getNdbError());
    push_warning("Failed to drop event '%s'", event_name);
    return false;
  }
  return true;
}

bool Ndb_util_table::create(bool is_upgrade) {
  NdbDictionary::Table new_table(m_table_name.c_str());

  unsigned mysql_version = MYSQL_VERSION_ID;
#ifndef NDEBUG
  if (m_table_name == "ndb_schema" &&
      DBUG_EVALUATE_IF("ndb_schema_skip_create_schema_op_id", true, false)) {
    push_warning("Creating table definition without schema_op_id column");
    mysql_version = 50725;
  }
#endif
  if (!define_table_ndb(new_table, mysql_version)) return false;

  // Start schema transactions for creating table and related schema objects
  Ndb_schema_trans_guard schema_trans(m_thd_ndb,
                                      m_thd_ndb->ndb->getDictionary());
  if (!schema_trans.begin_trans()) return false;

  if (!create_table_in_NDB(new_table)) return false;

  if (!create_indexes(new_table)) return false;

  // End schema transaction
  if (!schema_trans.commit_trans()) return false;

  // Load the new table definition into the Ndb_util_table object
  if (!open(is_upgrade)) return false;

  const NdbDictionary::Table *util_table = get_table();
  if (!create_events_in_NDB(*util_table)) {
    (void)drop_table_in_NDB(*util_table);
    return false;
  }

  if (!post_install()) {
    // Failed to perform post_install actions, attempt to drop the table in
    // order to start all over again from scratch on next retry
    (void)drop_table_in_NDB(*util_table);
    return false;
  }

  return true;
}

bool Ndb_util_table::create_in_DD() {
  Ndb_local_connection mysqld(const_cast<THD *>(get_thd()));
  if (mysqld.create_util_table(define_table_dd())) {
    return false;
  }

  if (!post_install_in_DD()) {
    ndb_log_verbose(1, "Failed to finalize table definition for '%s.%s' in DD",
                    m_db_name.c_str(), m_table_name.c_str());
    return false;
  }

  return true;
}

// Upgrade table
bool Ndb_util_table::upgrade() {
  const NdbDictionary::Table *old_table = get_table();

  // Any additional metadata could be saved before upgrade
  // and then restored later after install
  if (!pre_upgrade()) return false;

  // Drop the old table
  if (!drop_table_in_NDB(*old_table)) return false;

  // Create the new table
  if (!create(true)) return false;

  return true;
}

std::string Ndb_util_table::unpack_varbinary(NdbRecAttr *ndbRecAttr) {
  DBUG_TRACE;
  // Function should be called only on a varbinary column
  assert(ndbRecAttr->getType() == NdbDictionary::Column::Varbinary ||
         ndbRecAttr->getType() == NdbDictionary::Column::Longvarbinary);

  const char *value_start = nullptr;
  size_t value_length = 0;
  ndb_unpack_varchar(ndbRecAttr->getColumn(), 0, &value_start, &value_length,
                     ndbRecAttr->aRef());

  return std::string(value_start, value_length);
}

void Ndb_util_table::pack_varbinary(const char *column_name, const char *src,
                                    char *dst) const {
  // The table has to be loaded before this function is called
  assert(get_table() != nullptr);
  // The type of column should be VARBINARY
  assert(check_column_varbinary(column_name));
  ndb_pack_varchar(get_column(column_name), 0, src, std::strlen(src), dst);
}

std::string Ndb_util_table::unpack_varbinary(const char *column_name,
                                             const char *packed_str) const {
  // The table has to be loaded before this function is called
  assert(get_table() != nullptr);
  // The type of column should be VARBINARY
  assert(check_column_varbinary(column_name));
  const char *unpacked_str = nullptr;
  size_t unpacked_str_length = 0;
  ndb_unpack_varchar(get_column(column_name), 0, &unpacked_str,
                     &unpacked_str_length, packed_str);

  return std::string(unpacked_str, unpacked_str_length);
}

void Ndb_util_table::pack_varchar(const char *column_name, std::string_view src,
                                  char *dst) const {
  // The table has to be loaded before this function is called
  assert(get_table() != nullptr);
  // The type of column should be VARCHAR
  assert(check_column_varchar(get_column(column_name)->getName()));
  ndb_pack_varchar(get_column(column_name), 0, src.data(), src.size(), dst);
}

void Ndb_util_table::pack_varchar(Uint32 column_number, std::string_view src,
                                  char *dst) const {
  // The table has to be loaded before this function is called
  assert(get_table() != nullptr);
  // The type of column should be VARCHAR
  assert(check_column_varchar(get_column_by_number(column_number)->getName()));
  ndb_pack_varchar(get_column_by_number(column_number), 0, src.data(),
                   src.size(), dst);
}

bool Ndb_util_table::unpack_blob_not_null(NdbBlob *ndb_blob_handle,
                                          std::string *blob_value) {
  // Read length of blob
  Uint64 blob_len;
  if (ndb_blob_handle->getLength(blob_len) != 0) {
    return false;
  }
  if (blob_len == 0) {
    // The blob column didn't contain anything, return empty string
    return true;
  }

  // Read the blob content
  Uint32 read_len = static_cast<Uint32>(blob_len);
  std::unique_ptr<char[]> read_buf(new char[read_len]);
  if (ndb_blob_handle->readData(read_buf.get(), read_len) != 0) {
    return false;
  }
  assert(blob_len == read_len);  // Assert that all has been read
  blob_value->assign(read_buf.get(), read_len);

  DBUG_PRINT("unpack_blob", ("str: '%s'", blob_value->c_str()));
  return true;
}

int Ndb_util_table::get_column_num(const char *col_name) const {
  const NdbDictionary::Table *tab = get_table();
  assert(tab != nullptr);
  const NdbDictionary::Column *column = tab->getColumn(col_name);
  assert(column != nullptr);
  return column->getColumnNo();
}

bool Ndb_util_table::delete_all_rows() {
  DBUG_TRACE;

  const NdbDictionary::Table *ndb_table = get_table();
  assert(ndb_table != nullptr);

  NdbError ndb_err;
  Ndb *ndb = m_thd_ndb->ndb;
  const THD *thd = m_thd_ndb->get_thd();
  if (!ndb_table_scan_and_delete_rows(ndb, thd, ndb_table, ndb_err)) {
    push_ndb_error_warning(ndb_err);
    push_warning("Failed to remove all rows from %s.%s", m_db_name.c_str(),
                 m_table_name.c_str());
    return false;
  }
  return true;
}

//
//  Util_table_creator
//

Util_table_creator::Util_table_creator(THD *thd, Thd_ndb *thd_ndb,
                                       Ndb_util_table &util_table)
    : m_thd(thd), m_thd_ndb(thd_ndb), m_util_table(util_table) {
  m_name.append(db_name()).append(".").append(table_name());
}

bool Util_table_creator::create_or_upgrade_in_NDB(bool upgrade_allowed,
                                                  bool &reinstall) const {
  ndb_log_verbose(50, "Checking '%s' table in NDB", m_name.c_str());

  if (m_util_table.exists()) {
    // Table exists already. Upgrade it if required.
    if (!m_util_table.open()) {
      ndb_log_error("Failed to open '%s' table", m_name.c_str());
      return false;
    }

    if (m_util_table.need_upgrade()) {
      ndb_log_info("The '%s' table needs to be upgraded", m_name.c_str());

      if (!upgrade_allowed) {
        ndb_log_info("Upgrade of '%s' table not allowed!", m_name.c_str());
        // Skip upgrading the table and continue with
        // limited functionality
        return true;
      }

      ndb_log_info("Upgrade of '%s' table...", m_name.c_str());
      if (!m_util_table.upgrade()) {
        ndb_log_error("Upgrade of '%s' table failed!", m_name.c_str());
        return false;
      }
      reinstall = true;
      ndb_log_info("Upgrade of '%s' table completed", m_name.c_str());
    }

  } else {
    // Table did not exist. Create it.
    ndb_log_verbose(50, "The '%s' table does not exist, creating...",
                    m_name.c_str());

    // Create the table using NdbApi
    if (!m_util_table.create()) {
      ndb_log_error("Failed to create '%s' table", m_name.c_str());
      return false;
    }
    reinstall = true;

    ndb_log_info("Created '%s' table in NDB", m_name.c_str());
  }

  ndb_log_verbose(50, "The '%s' table is ok in NDB", m_name.c_str());
  return true;
}

bool Util_table_creator::install_in_DD(bool reinstall) {
  DBUG_TRACE;

  ndb_log_verbose(50, "Checking '%s' table in DD", m_name.c_str());

  Ndb_dd_client dd_client(m_thd);

  if (!dd_client.mdl_locks_acquire_exclusive(db_name(), table_name())) {
    ndb_log_error("Failed to MDL lock '%s' table", m_name.c_str());
    return false;
  }

  // There may exist a stale DD definition, occupying the NDB table id
  // or the pair schema.table. Check these and remove.
  const NdbDictionary::Table *ndbtab = m_util_table.get_table();
  const Ndb_dd_handle ndb_handle{ndbtab->getObjectId(),
                                 ndbtab->getObjectVersion()};
  ndbcluster::ndbrequire(ndb_handle.valid());

  const dd::Table *existing = nullptr;
  if (!dd_client.get_table(db_name(), table_name(), &existing)) {
    ndb_log_error("Failed to get '%s' table from DD", m_name.c_str());
    return false;
  }

  if (existing != nullptr) {
    const Ndb_dd_handle dd_handle = ndb_dd_table_get_spi_and_version(existing);
    if (!dd_handle.valid()) {
      ndb_log_error("Failed to extract id and version for table '%s' from DD",
                    m_name.c_str());
      assert(false);
      // Continue and force removal of table definition
      reinstall = true;
    }

    // Check if table need to be reinstalled in DD.
    if (m_util_table.need_reinstall(existing)) {
      ndb_log_info("Table '%s' need reinstall in DD", m_name.c_str());
      reinstall = true;
    }

    // Check if table definition in DD is outdated
    if (!reinstall && ndb_handle == dd_handle) {
      // Existed, didn't need reinstall and version matched
      return true;
    }

    ndb_log_verbose(1, "Removing '%s' from DD", m_name.c_str());
    if (!dd_client.remove_table(db_name(), table_name())) {
      ndb_log_info("Failed to remove '%s' from DD", m_name.c_str());
      return false;
    }
    // Pointer to table not valid after remove
    existing = nullptr;

    // Check if DD table is to be installed with a different id than
    // previously, removing the stale definition if necessary.
    if (dd_handle.spi != ndb_handle.spi) {
      ndb_log_verbose(1, "Removing stale table definition with id %llu from DD",
                      ndb_handle.spi);
      if (!dd_client.remove_table(ndb_handle.spi)) {
        ndb_log_info("Failed to remove table definition with id %llu from DD",
                     ndb_handle.spi);
      }
    }

    /*
      The table existed and was deleted from DD. It's possible
      that someone has tried to use it and thus it might have been
      inserted in the table definition cache. Close the table
      in the table definition cache(tdc).
    */
    ndb_log_verbose(1, "Removing '%s' from table definition cache",
                    m_name.c_str());
    ndb_tdc_close_cached_table(m_thd, db_name(), table_name());
  } else {
    // Remove any stale DD table that may be occupying this
    // ndbcluster-<id> place
    ndb_log_verbose(
        1, "Removing potentially stale table definition with id %llu from DD",
        ndb_handle.spi);
    if (!dd_client.remove_table(ndb_handle.spi)) {
      ndb_log_info(
          "Failed to remove potentially stale table definition %llu from DD",
          ndb_handle.spi);
    }
  }

  dd_client.commit();

  // Create DD table definition
  Thd_ndb::Options_guard thd_ndb_options(m_thd_ndb);
  // Allow creating DD table definition although table already exist in NDB
  thd_ndb_options.set(Thd_ndb::CREATE_UTIL_TABLE);
  // Mark table definition as hidden in DD
  if (m_util_table.is_hidden())
    thd_ndb_options.set(Thd_ndb::CREATE_UTIL_TABLE_HIDDEN);

  if (!m_util_table.create_in_DD()) {
    ndb_log_error("Failed to create table definition for '%s' in DD",
                  m_name.c_str());
    return false;
  }

  return true;
}

bool Util_table_creator::setup_table_for_binlog() const {
  // Acquire exclusive MDL lock on schema and table
  Ndb_dd_client dd_client(m_thd);
  if (!dd_client.mdl_locks_acquire_exclusive(db_name(), table_name())) {
    ndb_log_error("Failed to acquire MDL lock for '%s' table", m_name.c_str());
    m_thd->clear_error();
    return false;
  }

  const dd::Table *table_def;
  if (!dd_client.get_table(db_name(), table_name(), &table_def)) {
    ndb_log_error("Failed to open table definition for '%s' table",
                  m_name.c_str());
    return false;
  }

  const bool skip_error_handling = true;
  // Setup events for this table
  if (ndbcluster_binlog_setup_table(m_thd, m_thd_ndb->ndb, db_name(),
                                    table_name(), table_def,
                                    skip_error_handling)) {
    ndb_log_info("Failed to setup events for '%s' table", m_name.c_str());
    return false;
  }

  return true;
}

bool Util_table_creator::create_or_upgrade(bool upgrade_allowed,
                                           bool create_events) {
  bool reinstall = false;
  if (!create_or_upgrade_in_NDB(upgrade_allowed, reinstall)) {
    return false;
  }

  if (!install_in_DD(reinstall)) {
    return false;
  }

  if (create_events) {
    if (!setup_table_for_binlog()) {
      return false;
    }
  }
  return true;
}
