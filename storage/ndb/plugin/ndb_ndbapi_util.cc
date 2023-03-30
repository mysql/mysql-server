/*
   Copyright (c) 2011, 2023, Oracle and/or its affiliates.

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

#include "storage/ndb/plugin/ndb_ndbapi_util.h"

#include <charconv>

#include <string.h>  // memcpy

#include "my_byteorder.h"
#include "my_dbug.h"
#include "storage/ndb/plugin/ndb_name_util.h"  // ndb_name_is_temp
#include "storage/ndb/plugin/ndb_require.h"    // ndbrequire
#include "storage/ndb/plugin/ndb_retry.h"      // ndb_trans_retry

void ndb_pack_varchar(const NdbDictionary::Table *ndbtab, unsigned column_index,
                      char (&buf)[512], const char *str, size_t sz) {
  // Get the column, cast to int to help compiler choose
  // the "const int" overload rather than "const char*"
  const NdbDictionary::Column *col =
      ndbtab->getColumn(static_cast<int>(column_index));

  assert(col->getLength() <= (int)sizeof(buf));

  switch (col->getArrayType()) {
    case NdbDictionary::Column::ArrayTypeFixed:
      memcpy(buf, str, sz);
      break;
    case NdbDictionary::Column::ArrayTypeShortVar:
      *(uchar *)buf = (uchar)sz;
      memcpy(buf + 1, str, sz);
      break;
    case NdbDictionary::Column::ArrayTypeMediumVar:
      int2store(buf, (uint16)sz);
      memcpy(buf + 2, str, sz);
      break;
  }
}

void ndb_pack_varchar(const NdbDictionary::Column *col, size_t offset,
                      const char *str, size_t str_length, char *buf) {
  buf += offset;
  switch (col->getArrayType()) {
    case NdbDictionary::Column::ArrayTypeFixed:
      memcpy(buf, str, str_length);
      break;
    case NdbDictionary::Column::ArrayTypeShortVar:
      *(uchar *)buf = (uchar)str_length;
      memcpy(buf + 1, str, str_length);
      break;
    case NdbDictionary::Column::ArrayTypeMediumVar:
      int2store(buf, (uint16)str_length);
      memcpy(buf + 2, str, str_length);
      break;
  }
}

void ndb_unpack_varchar(const NdbDictionary::Column *col, size_t offset,
                        const char **str, size_t *str_length, const char *buf) {
  buf += offset;

  switch (col->getArrayType()) {
    case NdbDictionary::Column::ArrayTypeFixed:
      *str_length = col->getLength();
      *str = buf;
      break;
    case NdbDictionary::Column::ArrayTypeShortVar: {
      const unsigned char len1byte = static_cast<unsigned char>(buf[0]);
      *str_length = len1byte;
      *str = buf + 1;
    } break;
    case NdbDictionary::Column::ArrayTypeMediumVar: {
      const unsigned short len2byte = uint2korr(buf);
      *str_length = len2byte;
      *str = buf + 2;
    } break;
  }
}

Uint32 ndb_get_extra_metadata_version(const NdbDictionary::Table *ndbtab) {
  DBUG_TRACE;

  Uint32 version;
  void *unpacked_data;
  Uint32 unpacked_length;
  const int get_result =
      ndbtab->getExtraMetadata(version, &unpacked_data, &unpacked_length);
  if (get_result != 0) {
    // Could not get extra metadata, return 0
    return 0;
  }

  free(unpacked_data);

  return version;
}

bool ndb_table_get_serialized_metadata(const NdbDictionary::Table *ndbtab,
                                       std::string &serialized_metadata) {
  Uint32 version;
  void *unpacked_data;
  Uint32 unpacked_len;
  const int get_result =
      ndbtab->getExtraMetadata(version, &unpacked_data, &unpacked_len);
  if (get_result != 0) return false;

  if (version != 2) {
    free(unpacked_data);
    return false;
  }

  serialized_metadata.assign(static_cast<const char *>(unpacked_data),
                             unpacked_len);
  free(unpacked_data);
  return true;
}

bool ndb_table_has_blobs(const NdbDictionary::Table *ndbtab) {
  const int num_columns = ndbtab->getNoOfColumns();
  for (int i = 0; i < num_columns; i++) {
    const NdbDictionary::Column::Type column_type =
        ndbtab->getColumn(i)->getType();
    if (column_type == NdbDictionary::Column::Blob ||
        column_type == NdbDictionary::Column::Text) {
      // Found at least one blob column, the table has blobs
      return true;
    }
  }
  return false;
}

bool ndb_table_has_hidden_pk(const NdbDictionary::Table *ndbtab) {
  const char *hidden_pk_name = "$PK";
  if (ndbtab->getNoOfPrimaryKeys() == 1) {
    const NdbDictionary::Column *ndbcol = ndbtab->getColumn(hidden_pk_name);
    if (ndbcol && ndbcol->getType() == NdbDictionary::Column::Bigunsigned &&
        ndbcol->getLength() == 1 && ndbcol->getNullable() == false &&
        ndbcol->getPrimaryKey() == true && ndbcol->getAutoIncrement() == true &&
        ndbcol->getDefaultValue() == nullptr) {
      return true;
    }
  }
  return false;
}

bool ndb_table_has_tablespace(const NdbDictionary::Table *ndbtab) {
  // NOTE! There is a slight ambiguity in the NdbDictionary::Table.
  // Depending on whether it has been retrieved from NDB or created
  // by user as part of defining a new table in NDB, different methods
  // need to be used for determining if table has tablespace

  if (ndb_table_tablespace_name(ndbtab) != nullptr) {
    // Has tablespace
    return true;
  }

  if (ndbtab->getTablespace()) {
    // Retrieved from NDB, the tablespace id and version
    // are available in the table definition -> has tablespace.
    // NOTE! Fetching the name would require another roundtrip to NDB
    return true;
  }

  // Neither name or id of tablespace is set -> no tablespace
  return false;
}

const char *ndb_table_tablespace_name(const NdbDictionary::Table *ndbtab) {
  // NOTE! The getTablespaceName() returns zero length string
  // to indicate no tablespace
  const char *tablespace_name = ndbtab->getTablespaceName();
  if (strlen(tablespace_name) == 0) {
    // Just the zero length name, no tablespace name
    return nullptr;
  }
  return tablespace_name;
}

std::string ndb_table_tablespace_name(NdbDictionary::Dictionary *dict,
                                      const NdbDictionary::Table *ndbtab) {
  // NOTE! The getTablespaceName() returns zero length string
  // to indicate no tablespace
  std::string tablespace_name = ndbtab->getTablespaceName();
  if (tablespace_name.empty()) {
    // Just the zero length name, no tablespace name
    // Try and retrieve it using the id as a fallback mechanism
    Uint32 tablespace_id;
    if (ndbtab->getTablespace(&tablespace_id)) {
      const NdbDictionary::Tablespace ts = dict->getTablespace(tablespace_id);
      if (!ndb_dict_check_NDB_error(dict)) {
        tablespace_name = ts.getName();
      }
    }
  }
  return tablespace_name;
}

bool ndb_dict_check_NDB_error(NdbDictionary::Dictionary *dict) {
  return (dict->getNdbError().code != 0);
}

bool ndb_get_logfile_group_names(const NdbDictionary::Dictionary *dict,
                                 std::unordered_set<std::string> &lfg_names) {
  NdbDictionary::Dictionary::List lfg_list;
  if (dict->listObjects(lfg_list, NdbDictionary::Object::LogfileGroup) != 0) {
    return false;
  }

  for (uint i = 0; i < lfg_list.count; i++) {
    NdbDictionary::Dictionary::List::Element &elmt = lfg_list.elements[i];
    lfg_names.insert(elmt.name);
  }
  return true;
}

bool ndb_get_tablespace_names(
    const NdbDictionary::Dictionary *dict,
    std::unordered_set<std::string> &tablespace_names) {
  NdbDictionary::Dictionary::List tablespace_list;
  if (dict->listObjects(tablespace_list, NdbDictionary::Object::Tablespace) !=
      0) {
    return false;
  }

  for (uint i = 0; i < tablespace_list.count; i++) {
    NdbDictionary::Dictionary::List::Element &elmt =
        tablespace_list.elements[i];
    tablespace_names.insert(elmt.name);
  }
  return true;
}

bool ndb_get_table_names_in_schema(
    const NdbDictionary::Dictionary *dict, const std::string &schema_name,
    std::unordered_set<std::string> *table_names,
    std::unordered_set<std::string> *temp_names) {
  NdbDictionary::Dictionary::List list;
  if (dict->listObjects(list, NdbDictionary::Object::UserTable) != 0) {
    return false;
  }

  for (uint i = 0; i < list.count; i++) {
    NdbDictionary::Dictionary::List::Element &elmt = list.elements[i];

    if (schema_name != elmt.database) {
      continue;
    }

    if (ndb_name_is_blob_prefix(elmt.name) ||
        ndb_name_is_fk_mock_prefix(elmt.name)) {
      continue;
    }

    // Skip temporary named tables if container for them is not provided
    const bool is_temp = ndb_name_is_temp(elmt.name);
    if (temp_names == nullptr && is_temp) {
      continue;
    }

    if (schema_name == "mysql" &&
        (strcmp(elmt.name, "ndb_schema") == 0 ||
         strcmp(elmt.name, "ndb_schema_result") == 0 ||
         strcmp(elmt.name, "ndb_apply_status") == 0 ||
         strcmp(elmt.name, "ndb_sql_metadata") == 0 ||
         strcmp(elmt.name, "ndb_index_stat_head") == 0 ||
         strcmp(elmt.name, "ndb_index_stat_sample") == 0)) {
      // Skip NDB utility tables. They are marked as hidden in the DD and are
      // specially handled by the binlog thread.
      continue;
    }

    if (elmt.state == NdbDictionary::Object::StateOnline ||
        elmt.state == NdbDictionary::Object::ObsoleteStateBackup ||
        elmt.state == NdbDictionary::Object::StateBuilding) {
      // Only return the table if they're already usable i.e. StateOnline or
      // StateBackup or if they're expected to be usable soon which is denoted
      // by StateBuilding
      is_temp ? temp_names->insert(elmt.name) : table_names->insert(elmt.name);
    }
  }
  return true;
}

bool ndb_get_undofile_names(NdbDictionary::Dictionary *dict,
                            const std::string &logfile_group_name,
                            std::vector<std::string> *undofile_names) {
  NdbDictionary::Dictionary::List undofile_list;
  if (dict->listObjects(undofile_list, NdbDictionary::Object::Undofile) != 0) {
    return false;
  }

  for (uint i = 0; i < undofile_list.count; i++) {
    NdbDictionary::Dictionary::List::Element &elmt = undofile_list.elements[i];
    NdbDictionary::Undofile uf = dict->getUndofile(-1, elmt.name);
    if (logfile_group_name.compare(uf.getLogfileGroup()) == 0) {
      undofile_names->push_back(elmt.name);
    }
  }
  return true;
}

bool ndb_get_datafile_names(NdbDictionary::Dictionary *dict,
                            const std::string &tablespace_name,
                            std::vector<std::string> *datafile_names) {
  NdbDictionary::Dictionary::List datafile_list;
  if (dict->listObjects(datafile_list, NdbDictionary::Object::Datafile) != 0) {
    return false;
  }

  for (uint i = 0; i < datafile_list.count; i++) {
    NdbDictionary::Dictionary::List::Element &elmt = datafile_list.elements[i];
    NdbDictionary::Datafile df = dict->getDatafile(-1, elmt.name);
    if (tablespace_name.compare(df.getTablespace()) == 0) {
      datafile_names->push_back(elmt.name);
    }
  }
  return true;
}

bool ndb_get_database_names_in_dictionary(
    NdbDictionary::Dictionary *dict,
    std::unordered_set<std::string> *database_names) {
  DBUG_TRACE;

  /* Get all the list of tables from NDB and read the database names */
  NdbDictionary::Dictionary::List list;
  if (dict->listObjects(list, NdbDictionary::Object::UserTable) != 0)
    return false;

  for (uint i = 0; i < list.count; i++) {
    NdbDictionary::Dictionary::List::Element &elmt = list.elements[i];

    /* Skip the table if it is not in an expected state
       or if it is a temporary or blob table.*/
    if ((elmt.state != NdbDictionary::Object::StateOnline &&
         elmt.state != NdbDictionary::Object::StateBuilding) ||
        ndb_name_is_temp(elmt.name) || ndb_name_is_blob_prefix(elmt.name) ||
        ndb_name_is_fk_mock_prefix(elmt.name)) {
      DBUG_PRINT("debug", ("Skipping table %s.%s", elmt.database, elmt.name));
      continue;
    }
    DBUG_PRINT("debug", ("Found %s.%s in NDB", elmt.database, elmt.name));

    database_names->insert(elmt.database);
  }
  return true;
}

bool ndb_database_exists(NdbDictionary::Dictionary *dict,
                         const std::string &database_name, bool &exists) {
  // Get list of tables from NDB and read database names
  NdbDictionary::Dictionary::List list;
  if (dict->listObjects(list, NdbDictionary::Object::UserTable) != 0)
    return false;
  for (uint i = 0; i < list.count; i++) {
    NdbDictionary::Dictionary::List::Element &elmt = list.elements[i];
    // Skip the table if it is not in an expected state or if it is a temporary
    // or blob table
    if ((elmt.state != NdbDictionary::Object::StateOnline &&
         elmt.state != NdbDictionary::Object::StateBuilding) ||
        ndb_name_is_temp(elmt.name) || ndb_name_is_blob_prefix(elmt.name) ||
        ndb_name_is_fk_mock_prefix(elmt.name)) {
      continue;
    }
    if (database_name == elmt.database) {
      exists = true;
      return true;
    }
  }
  exists = false;
  return true;
}

bool ndb_logfile_group_exists(NdbDictionary::Dictionary *dict,
                              const std::string &logfile_group_name,
                              bool &exists) {
  NdbDictionary::LogfileGroup lfg =
      dict->getLogfileGroup(logfile_group_name.c_str());
  const int dict_error_code = dict->getNdbError().code;
  if (dict_error_code == 0) {
    exists = true;
    return true;
  }
  if (dict_error_code == 723) {
    exists = false;
    return true;
  }
  return false;
}

bool ndb_tablespace_exists(NdbDictionary::Dictionary *dict,
                           const std::string &tablespace_name, bool &exists) {
  NdbDictionary::Tablespace tablespace =
      dict->getTablespace(tablespace_name.c_str());
  const int dict_error_code = dict->getNdbError().code;
  if (dict_error_code == 0) {
    exists = true;
    return true;
  }
  if (dict_error_code == 723) {
    exists = false;
    return true;
  }
  return false;
}

bool ndb_table_exists(NdbDictionary::Dictionary *dict,
                      const std::string &db_name, const std::string &table_name,
                      bool &exists) {
  NdbDictionary::Dictionary::List list;
  if (dict->listObjects(list, NdbDictionary::Object::UserTable) != 0) {
    // List objects failed
    return false;
  }
  for (unsigned int i = 0; i < list.count; i++) {
    NdbDictionary::Dictionary::List::Element &elmt = list.elements[i];
    if (db_name == elmt.database && table_name == elmt.name &&
        (elmt.state == NdbDictionary::Object::StateOnline ||
         elmt.state == NdbDictionary::Object::ObsoleteStateBackup ||
         elmt.state == NdbDictionary::Object::StateBuilding)) {
      exists = true;
      return true;
    }
  }
  exists = false;
  return true;
}

bool ndb_get_logfile_group_id_and_version(NdbDictionary::Dictionary *dict,
                                          const std::string &logfile_group_name,
                                          int &id, int &version) {
  NdbDictionary::LogfileGroup lfg =
      dict->getLogfileGroup(logfile_group_name.c_str());
  if (dict->getNdbError().code != 0) {
    return false;
  }
  id = lfg.getObjectId();
  version = lfg.getObjectVersion();
  return true;
}

bool ndb_get_tablespace_id_and_version(NdbDictionary::Dictionary *dict,
                                       const std::string &tablespace_name,
                                       int &id, int &version) {
  NdbDictionary::Tablespace ts = dict->getTablespace(tablespace_name.c_str());
  if (dict->getNdbError().code != 0) {
    return false;
  }
  id = ts.getObjectId();
  version = ts.getObjectVersion();
  return true;
}

bool ndb_table_index_count(const NdbDictionary::Dictionary *dict,
                           const NdbDictionary::Table *ndbtab,
                           unsigned int &index_count) {
  NdbDictionary::Dictionary::List list;
  if (dict->listIndexes(list, *ndbtab) != 0) {
    // List indexes failed
    return false;
  }
  // Separate indexes into ordered and unique indexes
  std::unordered_set<std::string> ordered_indexes;
  std::unordered_set<std::string> unique_indexes;
  for (uint i = 0; i < list.count; i++) {
    NdbDictionary::Dictionary::List::Element &elmt = list.elements[i];
    if (elmt.state != NdbDictionary::Object::StateOnline) {
      // listIndexes() returns indexes in all states while this function is
      // only interested in indexes that are online and usable. Filtering out
      // indexes in other states is particularly important when metadata is
      // being restored as they may be in StateBuilding indicating that all
      // metadata related to the table hasn't been restored yet
      continue;
    }
    switch (elmt.type) {
      case NdbDictionary::Object::UniqueHashIndex:
        unique_indexes.insert(elmt.name);
        break;
      case NdbDictionary::Object::OrderedIndex:
        ordered_indexes.insert(elmt.name);
        break;
      default:
        // Unexpected object type
        return false;
    }
  }
  index_count = ordered_indexes.size();
  // Iterate through the ordered indexes and check if any of them are
  // "companion" ordered indexes. This is required since creating a unique key
  // leads to 2 indexes being created - a unique hash index (of the form
  // <index_name>$unique) and a companion ordered index. Note that this is not
  // the case for hash based unique indexes which have no companion ordered
  // index
  for (auto &ordered_index : ordered_indexes) {
    const std::string unique_index = ordered_index + "$unique";
    if (unique_indexes.find(unique_index) != unique_indexes.end()) {
      index_count--;
    }
  }
  index_count += unique_indexes.size();
  return true;
}

bool ndb_table_have_unique_or_fk(const NdbDictionary::Dictionary *dict,
                                 const NdbDictionary::Table *ndbtab,
                                 bool &found) {
  DBUG_TRACE;
  NdbDictionary::Dictionary::List list;
  if (dict->listDependentObjects(list, *ndbtab) != 0) {
    // List dependent failed
    return false;
  }
  for (uint i = 0; i < list.count; i++) {
    NdbDictionary::Dictionary::List::Element &elmt = list.elements[i];
    DBUG_PRINT("info", ("elmt[%d]: %d '%s'", i, elmt.type, elmt.name));

    if (elmt.type == NdbDictionary::Object::UniqueHashIndex) {
      DBUG_PRINT("info", ("Found UniqueHashIndex"));
      found = true;
      return true;
    }

    if (elmt.type == NdbDictionary::Object::ForeignKey) {
      DBUG_PRINT("info", ("Found ForeignKey"));
      found = true;
      return true;
    }
  }

  DBUG_PRINT("info", ("No unique or fk found"));
  return true;
}

bool ndb_table_scan_and_delete_rows(
    Ndb *ndb, const THD *thd, const NdbDictionary::Table *ndb_table,
    NdbError &ndb_err,
    const std::function<void(NdbScanFilter &)> &ndb_scan_filter_defn) {
  DBUG_TRACE;
  unsigned deleted = 0;

  // Function for scanning and deleting all rows returned
  std::function<const NdbError *(NdbTransaction *)> scan_and_delete_func =
      [&](NdbTransaction *trans) -> const NdbError * {
    NdbScanOperation *scan_op = trans->getNdbScanOperation(ndb_table);
    if (scan_op == nullptr) return &trans->getNdbError();

    if (scan_op->readTuples(NdbOperation::LM_Exclusive) != 0)
      return &scan_op->getNdbError();

    // Define the scan filters if the caller has provided definition
    if (ndb_scan_filter_defn) {
      NdbScanFilter scan_filter(scan_op);
      ndb_scan_filter_defn(scan_filter);
      if (scan_filter.getNdbError().code != 0) {
        // error when scan filter was defined
        return &scan_filter.getNdbError();
      }
    }

    // Start the scan
    if (trans->execute(NdbTransaction::NoCommit) != 0)
      return &trans->getNdbError();

    // Loop through all rows
    bool fetch = true;
    while (true) {
      const int r = scan_op->nextResult(fetch);
      if (r < 0) {
        // Failed to fetch next row
        return &scan_op->getNdbError();
      }
      fetch = false;  // Don't fetch more until nextResult returns 2

      switch (r) {
        case 0:  // Found row
          DBUG_PRINT("info", ("Deleting row"));
          if (scan_op->deleteCurrentTuple() != 0) {
            // Failed to delete row
            return &scan_op->getNdbError();
          }
          deleted++;
          continue;

        case 1:
          DBUG_PRINT("info", ("No more rows"));
          // No more rows, commit the transaction
          if (trans->execute(NdbTransaction::Commit) != 0) {
            // Failed to commit
            return &trans->getNdbError();
          }
          DBUG_PRINT("info", ("Deleted %u rows", deleted));
          return nullptr;

        case 2:
          // Need to fetch more rows, first send the deletes
          DBUG_PRINT("info", ("Need to fetch more rows"));
          if (deleted > 0) {
            DBUG_PRINT("info", ("Sending deletes"));
            if (trans->execute(NdbTransaction::NoCommit) != 0) {
              // Failed to send
              return &trans->getNdbError();
            }
          }
          fetch = true;  // Fetch more rows
          continue;
      }
    }
    // Never reached
    ndbcluster::ndbrequire(false);
    return nullptr;
  };

  return ndb_trans_retry(ndb, thd, ndb_err, scan_and_delete_func);
}

bool ndb_get_parent_table_ids_in_dictionary(
    const NdbDictionary::Dictionary *dict,
    std::unordered_set<unsigned> &table_ids) {
  DBUG_TRACE;

  NdbDictionary::Dictionary::List list;
  if (dict->listObjects(list, NdbDictionary::Object::ForeignKey) != 0) {
    // List objects failed
    return false;
  }

  for (unsigned i = 0; i < list.count; i++) {
    const NdbDictionary::Dictionary::List::Element &elmt = list.elements[i];
    DBUG_PRINT("info", ("elmt[%d]: %d '%s'", i, elmt.id, elmt.name));

    // Extract parent_id from name format "<parent_id>/<child_id>/<name>"
    unsigned parent_id = 0;
    std::from_chars(elmt.name, elmt.name + 10, parent_id);
    if (parent_id == 0) {
      // unexpected format, skip
      assert(false);
      continue;
    }

    DBUG_PRINT("info", ("parent_id: %d", parent_id));
    table_ids.insert(parent_id);
  }
  return true;
}

#ifndef NDEBUG
#include "storage/ndb/plugin/ndb_table_guard.h"

bool ndb_dump_NDB_tables(Ndb *ndb) {
  // Get list of tables from NDB and read database names

  NdbDictionary::Dictionary::List list;
  if (ndb->getDictionary()->listObjects(list,
                                        NdbDictionary::Object::UserTable) != 0)
    return true;

  fprintf(stderr, "ndb_dump_NDB_tables\n");
  fprintf(stderr, "| table_id | db_name | table_name | object_version |\n");

  for (uint i = 0; i < list.count; i++) {
    NdbDictionary::Dictionary::List::Element &elmt = list.elements[i];
    DBUG_PRINT("info", ("elmt[%d]: %d '%s'", i, elmt.id, elmt.name));

    const int table_id = elmt.id;
    const char *table_database = elmt.database;
    assert(elmt.schema == std::string("def"));  // always "<db>/def/<name>"
    const char *table_name = elmt.name;

    Ndb_table_guard ndbtab_g(ndb, table_database, table_name);
    const NdbDictionary::Table *ndbtab = ndbtab_g.get_table();
    if (!ndbtab) {
      DBUG_PRINT("info",
                 ("failed to open table %s.%s", table_database, table_name));
      // skip tables that cannot be opened with Ndb_table_guard from server
      continue;
    }
    assert(table_id == ndbtab->getObjectId());  // same as listed
    const unsigned table_version = ndbtab->getObjectVersion();

    fprintf(stderr, "| %d | %s | %s | %u |\n", table_id, table_database,
            table_name, table_version);
  }
  return false;
}
#endif
