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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "storage/ndb/plugin/ndb_sql_metadata_table.h"

#include <sstream>

#include "storage/ndb/plugin/ndb_log.h"
#include "storage/ndb/plugin/ndb_sleep.h"  // ndb_retry_sleep
#include "storage/ndb/plugin/ndb_thd_ndb.h"

Ndb_sql_metadata_table::Ndb_sql_metadata_table(Thd_ndb *thd_ndb)
    : Ndb_util_table(thd_ndb, "mysql", "ndb_sql_metadata", true, false) {}

bool Ndb_sql_metadata_table::define_table_ndb(NdbDictionary::Table &new_table,
                                              unsigned) const {
  static const char *COL_TYPE = "type";
  static const char *COL_NAME = "name";
  static const char *COL_SEQUENCE = "seq";
  static const char *COL_NOTE = "note";
  static const char *COL_TEXT = "sql_ddl_text";

  // Allow later online add column
  new_table.setForceVarPart(true);

  // Allow table to be read+write also in single user mode
  new_table.setSingleUserMode(NdbDictionary::Table::SingleUserModeReadWrite);

  {
    // `type` smallint(6) NOT NULL
    NdbDictionary::Column col_type(COL_TYPE);
    col_type.setType(NdbDictionary::Column::Smallint);
    col_type.setNullable(false);
    col_type.setPrimaryKey(true);
    if (!define_table_add_column(new_table, col_type)) return false;
  }

  {
    // `name` varbinary(400) NOT NULL
    NdbDictionary::Column col_name(COL_NAME);
    col_name.setType(NdbDictionary::Column::Longvarbinary);
    col_name.setLength(400);
    col_name.setNullable(false);
    col_name.setPrimaryKey(true);
    if (!define_table_add_column(new_table, col_name)) return false;
  }

  {
    // `seq` smallint(6) unsigned NOT NULL
    NdbDictionary::Column col_seq(COL_SEQUENCE);
    col_seq.setType(NdbDictionary::Column::Smallunsigned);
    col_seq.setNullable(false);
    col_seq.setPrimaryKey(true);
    if (!define_table_add_column(new_table, col_seq)) return false;
  }

  {
    // `note` int unsigned
    NdbDictionary::Column col_note(COL_NOTE);
    col_note.setType(NdbDictionary::Column::Unsigned);
    col_note.setNullable(true);
    if (!define_table_add_column(new_table, col_note)) return false;
  }

  {
    // `sql_ddl_text` varbinary(12000)
    NdbDictionary::Column col_text(COL_TEXT);
    col_text.setType(NdbDictionary::Column::Longvarbinary);
    col_text.setNullable(false);
    col_text.setLength(12000);
    if (!define_table_add_column(new_table, col_text)) return false;
  }

  return true;
}

bool Ndb_sql_metadata_table::create_indexes(
    const NdbDictionary::Table &table) const {
  return create_primary_ordered_index(table);
}

bool Ndb_sql_metadata_table::check_schema() const { return true; }

bool Ndb_sql_metadata_table::need_upgrade() const { return false; }

std::string Ndb_sql_metadata_table::define_table_dd() const {
  std::stringstream ss;
  ss << "CREATE TABLE " << db_name() << "." << table_name() << "(\n";
  ss << "type smallint not null,"
        "name varbinary(400) NOT NULL,"
        "seq smallint unsigned not null,"
        "note int unsigned,"
        "sql_ddl_text varbinary(12000),"
        "PRIMARY KEY (type,name,seq)"
     << ") ENGINE=ndbcluster";
  return ss.str();
}

Ndb_sql_metadata_table::~Ndb_sql_metadata_table() {}

// Ndb_sql_metadata_api

/*  Map the table.
    Determine record sizes for a key record, a partial row record,
    and a full row record. Create NdbRecords for the hash primary key,
    ordered index, partial row, and full row.
    After setup() is called, the API's getters and setters become usable.
    Returns true on success.
*/
void Ndb_sql_metadata_api::setup(NdbDictionary::Dictionary *dict,
                                 const NdbDictionary::Table *table) {
  m_record_layout.addColumn(table->getColumn("type"));
  m_record_layout.addColumn(table->getColumn("name"));
  m_record_layout.addColumn(table->getColumn("seq"));
  m_key_record_size = m_record_layout.record_size;

  m_record_layout.addColumn(table->getColumn("note"));
  m_note_record_size = m_record_layout.record_size;

  m_record_layout.addColumn(table->getColumn("sql_ddl_text"));
  m_full_record_size = m_record_layout.record_size;

  m_row_rec = dict->createRecord(table, m_record_layout.record_specs,
                                 5,  // ALL FIVE COLUMNS
                                 sizeof(m_record_layout.record_specs[0]));
  m_note_rec = dict->createRecord(table, m_record_layout.record_specs,
                                  4,  // FIRST FOUR COLUMNS
                                  sizeof(m_record_layout.record_specs[0]));
  m_hash_key_rec = dict->createRecord(table, m_record_layout.record_specs,
                                      3,  // FIRST THREE COLUMNS
                                      sizeof(m_record_layout.record_specs[0]));

  const NdbDictionary::Index *primary = dict->getIndexGlobal("PRIMARY", *table);
  /* NDB can be started with __at_restart_skip_indexes as a one-time recovery
     measure in case of corruption. In this case, primary == nullptr.
     Do not handle that condition here; it is detected later by testing
     isInitialized().
  */
  if (primary) {
    m_ordered_index_rec =
        dict->createRecord(primary, table, m_record_layout.record_specs,
                           3,  // FIRST THREE COLUMNS
                           sizeof(m_record_layout.record_specs[0]));
    dict->removeIndexGlobal(*primary, false);
  } else {
    ndb_log_error(
        "Failed to setup PRIMARY index of ndb_sql_metadata, error %u: %s",
        dict->getNdbError().code, dict->getNdbError().message);
  }
}

void Ndb_sql_metadata_api::clear(NdbDictionary::Dictionary *dict) {
  if (m_full_record_size) {
    dict->releaseRecord(m_row_rec);
    m_row_rec = nullptr;
    dict->releaseRecord(m_note_rec);
    m_note_rec = nullptr;
    dict->releaseRecord(m_hash_key_rec);
    m_hash_key_rec = nullptr;
    m_key_record_size = 0;
    m_note_record_size = 0;
    m_full_record_size = 0;
  }

  if (m_ordered_index_rec) {
    dict->releaseRecord(m_ordered_index_rec);
    m_ordered_index_rec = nullptr;
  }

  m_record_layout.clear();
}

/* The row { TYPE_LOCK, "snapshot", 0 } may be used as a lock.
   Private internal method to create the row if it does not exist
   takes an already-open transaction.
*/
void Ndb_sql_metadata_api::writeSnapshotLockRow(NdbTransaction *tx) {
  char row[16384];
  initRowBuffer(row);
  setType(row, TYPE_LOCK);
  setName(row, "snapshot");
  setSeq(row, 0);
  setNote(row, nullptr);
  setSql(row, "");

  (void)tx->writeTuple(keyNdbRecord(), row, rowNdbRecord(), row);
}

/*
   Initialize the lock by assuring that the lock row is present in the table.
   Retry on temporary errors.
*/
const NdbError &Ndb_sql_metadata_api::initializeSnapshotLock(Ndb *ndb) {
  char key[512], row[512];
  initRowBuffer(key);
  setType(key, TYPE_LOCK);
  setName(key, "snapshot");
  setSeq(key, 0);

  const NdbError *err = nullptr;
  for (int retries = 10; retries > 0; retries--) {
    NdbTransaction *tx = ndb->startTransaction();
    err = &ndb->getNdbError();
    if (tx != nullptr) {
      const NdbOperation *read_op =
          tx->readTuple(keyNdbRecord(), key, noteNdbRecord(), row,
                        NdbOperation::LM_CommittedRead);
      tx->execute(NoCommit);

      assert(read_op);
      if (read_op && (read_op->getNdbError().code == 626)) {  // row not found
        writeSnapshotLockRow(tx);
        tx->execute(Commit);
      }
      tx->close();
      err = &tx->getNdbError();
    }
    switch (err->status) {
      case NdbError::TemporaryError:
        ndb_trans_retry_sleep();
        break;  // (break from switch; continue through loop)
      case NdbError::Success:
      case NdbError::PermanentError:
      case NdbError::UnknownResult:
        return *err;
    }
  }
  return *err;
}

/* Try to acquire an exclusive lock on the row { TYPE_LOCK, "snapshot", 0 }.
   Do not retry on a lock wait timeout, since this may lead to deadlock.
   The thread that wants the lock is the same thread that must do the work
   that would allow another server to release the lock.
*/
const NdbError &Ndb_sql_metadata_api::acquireSnapshotLock(Ndb *ndb,
                                                          NdbTransaction *&tx) {
  char key[512], row[512];
  initRowBuffer(key);
  setType(key, TYPE_LOCK);
  setName(key, "snapshot");
  setSeq(key, 0);

  const NdbError *err = nullptr;
  for (int retries = 10; retries > 0;) {
    tx = ndb->startTransaction();
    err = &ndb->getNdbError();
    if (tx != nullptr) {
      const NdbOperation *read_op =
          tx->readTuple(keyNdbRecord(), key, noteNdbRecord(), row,
                        NdbOperation::LM_Exclusive);
      tx->execute(NoCommit);
      assert(read_op);
      if (read_op && (read_op->getNdbError().code == 626)) {
        /* Someone has deleted the lock row, maybe using ndb_delete_all.
           Reinitialize the lock row and retry.
        */
        writeSnapshotLockRow(tx);
        tx->execute(Commit);
        tx->close();
        err = &tx->getNdbError();
        if (err->status == NdbError::Success) continue;
      } else {
        err = &tx->getNdbError();
      }
    }

    // If the lock was acquired, return with the NdbTransaction still open.
    if (err->status == NdbError::Success) return *err;

    ndb->closeTransaction(tx);

    // Check for lock wait timeout or a hard error.
    if (err->code == 266 || err->status != NdbError::TemporaryError) break;

    // The error is a temporary error that is not a lock wait timeout.  Retry.
    if (--retries > 0) ndb_trans_retry_sleep();
  }
  tx = nullptr;
  return *err;
}
