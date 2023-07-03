/*
   Copyright (c) 2017, 2022, Oracle and/or its affiliates.

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

// Implements
#include "storage/ndb/plugin/ndb_metadata.h"

#include <iostream>
#include <memory>
#include <string>
#include <unordered_set>

#include "my_base.h"  // For HA_SM_DISK and HA_SM_MEMORY, fix by bug27309072
#include "sql/dd/dd.h"
#include "sql/dd/impl/properties_impl.h"
#include "sql/dd/object_id.h"
#include "sql/dd/properties.h"
#include "sql/dd/types/column.h"               // dd:Column
#include "sql/dd/types/foreign_key.h"          // dd::Foreign_key
#include "sql/dd/types/foreign_key_element.h"  // dd::Foreign_key_element
#include "sql/dd/types/index.h"                // dd::Index
#include "sql/dd/types/index_element.h"        // dd::Index_element
#include "sql/dd/types/partition.h"
#include "sql/dd/types/table.h"
#include "sql/field.h"  // COLUMN_FORMAT_TYPE_DYNAMIC and _FIXED
#include "storage/ndb/plugin/ndb_dd.h"
#include "storage/ndb/plugin/ndb_dd_client.h"
#include "storage/ndb/plugin/ndb_dd_table.h"
#include "storage/ndb/plugin/ndb_fk_util.h"    // fk_split_name
#include "storage/ndb/plugin/ndb_log.h"        // ndb_log_error
#include "storage/ndb/plugin/ndb_name_util.h"  // ndb_name_is_temp
#include "storage/ndb/plugin/ndb_ndbapi_util.h"
#include "storage/ndb/plugin/ndb_table_guard.h"  // Ndb_table_guard

// Key used for magic flag "explicit_tablespace" in table options
static const char *magic_key_explicit_tablespace = "explicit_tablespace";

// Keys used for flags in table and column options
const char *key_storage = "storage";
const char *key_column_format = "column_format";
const char *key_column_bit_as_char = "treat_bit_as_char";
const char *key_column_not_secondary = "not_secondary";
const char *key_column_is_array = "is_array";
const char *key_column_geom_type = "geom_type";

// Check also partitioning properties
constexpr bool check_partitioning = false;  // disabled

dd::String_type Ndb_metadata::partition_expression() const {
  dd::String_type expr;
  if (m_ndbtab->getFragmentType() == NdbDictionary::Table::HashMapPartition &&
      m_ndbtab->getDefaultNoPartitionsFlag() &&
      m_ndbtab->getFragmentCount() == 0 && m_ndbtab->getLinearFlag() == false) {
    // Default partitioning
    return expr;
  }

  const char *separator = "";
  const int num_columns = m_ndbtab->getNoOfColumns();
  for (int i = 0; i < num_columns; i++) {
    const NdbDictionary::Column *column = m_ndbtab->getColumn(i);
    if (column->getPartitionKey()) {
      expr.append(separator);
      expr.append(column->getName());
      separator = ";";
    }
  }
  return expr;
}

void Ndb_metadata::create_columns(dd::Table *table_def) const {
  const bool hidden_pk = ndb_table_has_hidden_pk(m_ndbtab);

  // Virtual generated columns are a problem since they aren't stored in NDB
  // Dictionary
  for (int i = 0; i < m_ndbtab->getNoOfColumns(); i++) {
    const NdbDictionary::Column *ndb_column = m_ndbtab->getColumn(i);
    if (hidden_pk && !strcmp("$PK", ndb_column->getName())) {
      // Hidden PKs aren't stored in DD. Skip
      continue;
    }
    dd::Column *dd_column = table_def->add_column();
    dd_column->set_name(ndb_column->getName());

    switch (ndb_column->getType()) {
      // Based on create_ndb_column() in ha_ndbcluster.cc
      case NdbDictionary::Column::Tinyint:
        dd_column->set_type(dd::enum_column_types::TINY);
        dd_column->set_unsigned(false);
        dd_column->set_numeric_precision(ndb_column->getPrecision());
        dd_column->set_numeric_scale(ndb_column->getScale());
        break;
      case NdbDictionary::Column::Tinyunsigned:
        dd_column->set_type(dd::enum_column_types::TINY);
        dd_column->set_unsigned(true);
        dd_column->set_numeric_precision(ndb_column->getPrecision());
        dd_column->set_numeric_scale(ndb_column->getScale());
        break;
      case NdbDictionary::Column::Smallint:
        dd_column->set_type(dd::enum_column_types::SHORT);
        dd_column->set_unsigned(false);
        dd_column->set_numeric_precision(ndb_column->getPrecision());
        dd_column->set_numeric_scale(ndb_column->getScale());
        break;
      case NdbDictionary::Column::Smallunsigned:
        dd_column->set_type(dd::enum_column_types::SHORT);
        dd_column->set_unsigned(true);
        dd_column->set_numeric_precision(ndb_column->getPrecision());
        dd_column->set_numeric_scale(ndb_column->getScale());
        break;
      case NdbDictionary::Column::Mediumint:
        dd_column->set_type(dd::enum_column_types::INT24);
        dd_column->set_unsigned(false);
        dd_column->set_numeric_precision(ndb_column->getPrecision());
        dd_column->set_numeric_scale(ndb_column->getScale());
        break;
      case NdbDictionary::Column::Mediumunsigned:
        dd_column->set_type(dd::enum_column_types::INT24);
        dd_column->set_unsigned(true);
        dd_column->set_numeric_precision(ndb_column->getPrecision());
        dd_column->set_numeric_scale(ndb_column->getScale());
        break;
      case NdbDictionary::Column::Int:
        dd_column->set_type(dd::enum_column_types::LONG);
        dd_column->set_unsigned(false);
        dd_column->set_numeric_precision(ndb_column->getPrecision());
        dd_column->set_numeric_scale(ndb_column->getScale());
        break;
      case NdbDictionary::Column::Unsigned:
        dd_column->set_type(dd::enum_column_types::LONG);
        dd_column->set_unsigned(true);
        dd_column->set_numeric_precision(ndb_column->getPrecision());
        dd_column->set_numeric_scale(ndb_column->getScale());
        break;
      case NdbDictionary::Column::Bigint:
        dd_column->set_type(dd::enum_column_types::LONGLONG);
        dd_column->set_unsigned(false);
        dd_column->set_numeric_precision(ndb_column->getPrecision());
        dd_column->set_numeric_scale(ndb_column->getScale());
        break;
      case NdbDictionary::Column::Bigunsigned:
        dd_column->set_type(dd::enum_column_types::LONGLONG);
        dd_column->set_unsigned(true);
        dd_column->set_numeric_precision(ndb_column->getPrecision());
        dd_column->set_numeric_scale(ndb_column->getScale());
        break;
      case NdbDictionary::Column::Float:
        dd_column->set_type(dd::enum_column_types::FLOAT);
        dd_column->set_numeric_precision(ndb_column->getPrecision());
        dd_column->set_numeric_scale(ndb_column->getScale());
        break;
      case NdbDictionary::Column::Double:
        dd_column->set_type(dd::enum_column_types::DOUBLE);
        dd_column->set_numeric_precision(ndb_column->getPrecision());
        dd_column->set_numeric_scale(ndb_column->getScale());
        break;
      case NdbDictionary::Column::Olddecimal:
        dd_column->set_type(dd::enum_column_types::DECIMAL);
        dd_column->set_unsigned(false);
        dd_column->set_numeric_precision(ndb_column->getPrecision());
        dd_column->set_numeric_scale(ndb_column->getScale());
        break;
      case NdbDictionary::Column::Olddecimalunsigned:
        dd_column->set_type(dd::enum_column_types::DECIMAL);
        dd_column->set_unsigned(true);
        dd_column->set_numeric_precision(ndb_column->getPrecision());
        dd_column->set_numeric_scale(ndb_column->getScale());
        break;
      case NdbDictionary::Column::Decimal:
        dd_column->set_type(dd::enum_column_types::NEWDECIMAL);
        dd_column->set_unsigned(false);
        dd_column->set_numeric_precision(ndb_column->getPrecision());
        dd_column->set_numeric_scale(ndb_column->getScale());
        break;
      case NdbDictionary::Column::Decimalunsigned:
        dd_column->set_type(dd::enum_column_types::NEWDECIMAL);
        dd_column->set_unsigned(true);
        dd_column->set_numeric_precision(ndb_column->getPrecision());
        dd_column->set_numeric_scale(ndb_column->getScale());
        break;
      case NdbDictionary::Column::Char:
        dd_column->set_type(dd::enum_column_types::STRING);
        break;
      case NdbDictionary::Column::Varchar:
        dd_column->set_type(dd::enum_column_types::VARCHAR);
        break;
      case NdbDictionary::Column::Binary:
        dd_column->set_type(dd::enum_column_types::STRING);
        break;
      case NdbDictionary::Column::Varbinary:
        dd_column->set_type(dd::enum_column_types::VARCHAR);
        break;
      case NdbDictionary::Column::Datetime:
        dd_column->set_type(dd::enum_column_types::DATETIME);
        break;
      case NdbDictionary::Column::Date:
        dd_column->set_type(dd::enum_column_types::NEWDATE);
        break;
      case NdbDictionary::Column::Blob:
      case NdbDictionary::Column::Text:
        switch (ndb_column->getPartSize()) {
          case 0:
            dd_column->set_type(dd::enum_column_types::TINY_BLOB);
            break;
          case 2000:
            dd_column->set_type(dd::enum_column_types::BLOB);
            break;
          case 4000:
            dd_column->set_type(dd::enum_column_types::MEDIUM_BLOB);
            break;
          case 8100:
            dd_column->set_type(dd::enum_column_types::JSON);
            break;
          default:
            dd_column->set_type(dd::enum_column_types::LONG_BLOB);
            break;
        }
        break;
      case NdbDictionary::Column::Bit:
        dd_column->set_type(dd::enum_column_types::BIT);
        dd_column->options().set(key_column_bit_as_char, false);
        break;
      case NdbDictionary::Column::Longvarchar:
        dd_column->set_type(dd::enum_column_types::VARCHAR);
        break;
      case NdbDictionary::Column::Longvarbinary:
        dd_column->set_type(dd::enum_column_types::VARCHAR);
        break;
      case NdbDictionary::Column::Time:
        dd_column->set_type(dd::enum_column_types::TIME);
        break;
      case NdbDictionary::Column::Year:
        dd_column->set_type(dd::enum_column_types::YEAR);
        dd_column->set_unsigned(true);
        dd_column->set_zerofill(true);
        break;
      case NdbDictionary::Column::Timestamp:
        dd_column->set_type(dd::enum_column_types::TIMESTAMP);
        break;
      case NdbDictionary::Column::Time2:
        dd_column->set_type(dd::enum_column_types::TIME2);
        dd_column->set_datetime_precision(ndb_column->getPrecision());
        break;
      case NdbDictionary::Column::Datetime2:
        dd_column->set_type(dd::enum_column_types::DATETIME2);
        dd_column->set_datetime_precision(ndb_column->getPrecision());
        break;
      case NdbDictionary::Column::Timestamp2:
        dd_column->set_type(dd::enum_column_types::TIMESTAMP2);
        dd_column->set_datetime_precision(ndb_column->getPrecision());
        break;
      default:
        ndb_log_error("Type = %d", ndb_column->getType());
        assert(false);
        break;
    }

    dd_column->set_nullable(ndb_column->getNullable());

    dd_column->set_auto_increment(ndb_column->getAutoIncrement());

    dd_column->set_char_length(ndb_column->getLength());

    unsigned int default_value_length;
    const char *ndb_default_value = static_cast<const char *>(
        ndb_column->getDefaultValue(&default_value_length));
    /*
      Seems like NDB Dictionary doesn't differentiate between no default and
      NULL default. We try and differentiate between the 2 by looking at
      getNullable() and getAutoIncrement()
    */
    dd_column->set_has_no_default(!ndb_column->getNullable() &&
                                  !ndb_default_value &&
                                  !ndb_column->getAutoIncrement());
    if (ndb_default_value) {
      dd_column->set_default_value(
          dd::String_type(ndb_default_value, default_value_length));
    } else {
      dd_column->set_default_value_null(ndb_column->getNullable());
    }

    // Looks like DD expects the value set to this column to be human readable.
    // The actual values from ndb_default_value should be extracted based on the
    // column type and then set. See NdbDictionary::printFormattedValue() and
    // prepare_default_value_string() in dd_table.cc
    if (ndb_default_value) {
      dd_column->set_default_value_utf8(
          dd::String_type(ndb_default_value, default_value_length));
    } else {
      dd_column->set_default_value_utf8_null(ndb_column->getNullable());
    }

    if (ndb_column->getPrimaryKey())
      dd_column->set_column_key(dd::Column::CK_PRIMARY);

    // Column storage is set only for disk storage
    if (ndb_column->getStorageType() == NdbDictionary::Column::StorageTypeDisk)
      dd_column->options().set(key_storage, static_cast<uint32>(HA_SM_DISK));

    // Column format is set only for dynamic
    if (ndb_column->getDynamic())
      dd_column->options().set(key_column_format,
                               static_cast<uint32>(COLUMN_FORMAT_TYPE_DYNAMIC));
  }
}

bool Ndb_metadata::create_indexes(const NdbDictionary::Dictionary *dict,
                                  dd::Table *table_def) const {
  NdbDictionary::Dictionary::List list;
  if (dict->listIndexes(list, *m_ndbtab) != 0) {
    ndb_log_error("Failed to list indexes due to NDB error %u: %s",
                  dict->getNdbError().code, dict->getNdbError().message);
    return false;
  }
  // Sort the list by id so that it matches the order of creation. This doesn't
  // work when the indexes are created during ndb_restore
  list.sortById();

  // Separate indexes into ordered and unique indexes for quick lookup later
  std::unordered_set<std::string> ordered_indexes;
  std::unordered_set<std::string> hash_indexes;
  for (unsigned int i = 0; i < list.count; i++) {
    NdbDictionary::Dictionary::List::Element &element = list.elements[i];
    if (element.state != NdbDictionary::Object::StateOnline) {
      // listIndexes() returns indexes in all states while this function is
      // only interested in indexes that are online and usable. Filtering out
      // indexes in other states is particularly important when metadata is
      // being restored as they may be in StateBuilding indicating that all
      // metadata related to the table hasn't been restored yet.
      continue;
    }
    switch (element.type) {
      case NdbDictionary::Object::UniqueHashIndex:
        hash_indexes.insert(element.name);
        break;
      case NdbDictionary::Object::OrderedIndex:
        ordered_indexes.insert(element.name);
        break;
      default:
        // Unexpected object type
        assert(false);
        return false;
    }
  }

  for (unsigned int i = 0; i < list.count; i++) {
    NdbDictionary::Dictionary::List::Element &element = list.elements[i];
    if (element.state != NdbDictionary::Object::StateOnline) {
      // listIndexes() returns indexes in all states while this function is
      // only interested in indexes that are online and usable. Filtering out
      // indexes in other states is particularly important when metadata is
      // being restored as they may be in StateBuilding indicating that all
      // metadata related to the table hasn't been restored yet. The
      // getIndexGlobal() call below returns an "Index not found" error if
      // the index's state is StateBuilding. This is dealt with by skipping
      // the index altogether in the comparison.
      continue;
    }
    const NdbDictionary::Index *ndb_index =
        dict->getIndexGlobal(element.name, *m_ndbtab);
    if (!ndb_index) {
      ndb_log_error("Failed to open index %s from NDB due to error %u: %s",
                    element.name, dict->getNdbError().code,
                    dict->getNdbError().message);
      return false;
    }
    std::string index_name = ndb_index->getName();
    for (std::string::size_type n = index_name.find("@0047");
         n != std::string::npos; n = index_name.find("@0047")) {
      index_name.replace(n, 5, "/");
    }
    if (ndb_index->getType() == NdbDictionary::Index::OrderedIndex &&
        hash_indexes.find(index_name + "$unique") != hash_indexes.end()) {
      // Unless "USING HASH" is specified, creation of a unique index results in
      // the creation of both an ordered index and a hash index in NDB. Discount
      // the extra ordered index since DD has no notion of it.
      dict->removeIndexGlobal(*ndb_index, 0);
      continue;
    }

    dd::Index *dd_index = table_def->add_index();

    if (ndb_index->getType() == NdbDictionary::Index::UniqueHashIndex) {
      // Extract the actual index name by dropping the $unique suffix
      const std::string::size_type n = index_name.rfind("$unique");
      const std::string real_name = index_name.substr(0, n);
      dd_index->set_name(real_name.c_str());
      // PKs using HASH aren't created in NDB Dictionary so the type can only
      // be IT_UNIQUE
      dd_index->set_type(dd::Index::IT_UNIQUE);
      if (ordered_indexes.find(real_name) == ordered_indexes.end()) {
        dd_index->set_algorithm(dd::Index::IA_HASH);
        dd_index->set_algorithm_explicit(true);
      }
    } else if (ndb_index->getType() == NdbDictionary::Index::OrderedIndex) {
      dd_index->set_name(index_name.c_str());
      if (index_name == "PRIMARY") {
        dd_index->set_type(dd::Index::IT_PRIMARY);
      } else {
        dd_index->set_type(dd::Index::IT_MULTIPLE);
      }
    } else {
      // Unexpected object type
      assert(false);
      dict->removeIndexGlobal(*ndb_index, 0);
      return false;
    }

    dd_index->set_engine("ndbcluster");

    // Cycle through the columns retrieved from NDB Dictionary and add them as
    // index elements to the DD definition
    for (unsigned int j = 0; j < ndb_index->getNoOfColumns(); j++) {
      const dd::Column *column =
          table_def->get_column(ndb_index->getColumn(j)->getName());
      assert(column != nullptr);
      (void)dd_index->add_element(const_cast<dd::Column *>(column));
    }
    dict->removeIndexGlobal(*ndb_index, 0);
  }
  return true;
}

bool Ndb_metadata::create_foreign_keys(Ndb *ndb, dd::Table *table_def) const {
  NdbDictionary::Dictionary::List list;
  const NdbDictionary::Dictionary *dict = ndb->getDictionary();
  if (dict->listDependentObjects(list, *m_ndbtab) != 0) {
    ndb_log_error(
        "Failed to list dependant objects of table %s due to NDB "
        "error %u: %s",
        m_ndbtab->getName(), dict->getNdbError().code,
        dict->getNdbError().message);
    return false;
  }

  std::unordered_set<std::string> fk_created_names;
  for (unsigned int i = 0; i < list.count; i++) {
    NdbDictionary::Dictionary::List::Element element = list.elements[i];
    if (element.type != NdbDictionary::Object::ForeignKey) continue;

    NdbDictionary::ForeignKey ndb_fk;
    if (const_cast<NdbDictionary::Dictionary *>(dict)->getForeignKey(
            ndb_fk, element.name) != 0) {
      ndb_log_error("Failed to get foreign key %s from NDB due to error %u: %s",
                    element.name, dict->getNdbError().code,
                    dict->getNdbError().message);
      return false;
    }

    char parent_db[FN_LEN + 1];
    const char *parent_name = fk_split_name(parent_db, ndb_fk.getParentTable());
    char child_db[FN_LEN + 1];
    const char *child_name = fk_split_name(child_db, ndb_fk.getChildTable());
    // Skip creating FKs for parent tables if it's not a self referential FK
    if (strcmp(child_db, m_dbname) != 0 ||
        strcmp(child_name, m_ndbtab->getName()) != 0)
      continue;

    char fk_name_buffer[FN_LEN + 1];
    const char *fk_name = fk_split_name(fk_name_buffer, ndb_fk.getName());

    // Check if the FK has been created already. This is needed for self
    // referential FKs where two copies of the same FK seems to exist. This
    // occurs during copying ALTER statements where multiple copies of the FK
    // exist quite late in the life cycle when this comparison is done
    if (fk_created_names.find(fk_name) != fk_created_names.end()) {
      continue;
    }
    fk_created_names.insert(fk_name);

    dd::Foreign_key *dd_fk = table_def->add_foreign_key();
    dd_fk->set_name(fk_name);

    if (ndb_fk.getParentIndex() == nullptr) {
      dd_fk->set_unique_constraint_name("PRIMARY");
    } else {
      char index_name_buffer[FN_LEN + 1];
      std::string constraint_name =
          fk_split_name(index_name_buffer, ndb_fk.getParentIndex(), true);
      // Extract the actual index name by dropping the $unique suffix
      const std::string::size_type n = constraint_name.rfind("$unique");
      assert(n != std::string::npos);
      const std::string real_constraint_name = constraint_name.substr(0, n);
      dd_fk->set_unique_constraint_name(real_constraint_name.c_str());
    }

    switch (ndb_fk.getOnUpdateAction()) {
      case NdbDictionary::ForeignKey::FkAction::NoAction:
        dd_fk->set_update_rule(dd::Foreign_key::RULE_NO_ACTION);
        break;
      case NdbDictionary::ForeignKey::FkAction::Restrict:
        dd_fk->set_update_rule(dd::Foreign_key::RULE_RESTRICT);
        break;
      case NdbDictionary::ForeignKey::FkAction::Cascade:
        dd_fk->set_update_rule(dd::Foreign_key::RULE_CASCADE);
        break;
      case NdbDictionary::ForeignKey::FkAction::SetNull:
        dd_fk->set_update_rule(dd::Foreign_key::RULE_SET_NULL);
        break;
      case NdbDictionary::ForeignKey::FkAction::SetDefault:
        dd_fk->set_update_rule(dd::Foreign_key::RULE_SET_DEFAULT);
        break;
      default:
        assert(false);
        return false;
    }

    switch (ndb_fk.getOnDeleteAction()) {
      case NdbDictionary::ForeignKey::FkAction::NoAction:
        dd_fk->set_delete_rule(dd::Foreign_key::RULE_NO_ACTION);
        break;
      case NdbDictionary::ForeignKey::FkAction::Restrict:
        dd_fk->set_delete_rule(dd::Foreign_key::RULE_RESTRICT);
        break;
      case NdbDictionary::ForeignKey::FkAction::Cascade:
        dd_fk->set_delete_rule(dd::Foreign_key::RULE_CASCADE);
        break;
      case NdbDictionary::ForeignKey::FkAction::SetNull:
        dd_fk->set_delete_rule(dd::Foreign_key::RULE_SET_NULL);
        break;
      case NdbDictionary::ForeignKey::FkAction::SetDefault:
        dd_fk->set_delete_rule(dd::Foreign_key::RULE_SET_DEFAULT);
        break;
      default:
        assert(false);
        return false;
    }

    dd_fk->set_referenced_table_catalog_name("def");
    dd_fk->set_referenced_table_schema_name(parent_db);
    dd_fk->set_referenced_table_name(parent_name);

    Ndb_table_guard ndb_table_guard(ndb, parent_db, parent_name);
    const NdbDictionary::Table *parent_table = ndb_table_guard.get_table();
    if (!parent_table) {
      // Failed to open the table from NDB
      ndb_log_error("Got error '%d: %s' from NDB",
                    ndb_table_guard.getNdbError().code,
                    ndb_table_guard.getNdbError().message);
      ndb_log_error("Failed to open table '%s.%s'", parent_db, parent_name);
      return false;
    }
    for (unsigned int j = 0; j < ndb_fk.getChildColumnCount(); j++) {
      // Create FK element(s) for child columns
      dd::Foreign_key_element *fk_element = dd_fk->add_element();
      const dd::Column *column = table_def->get_column(
          m_ndbtab->getColumn(ndb_fk.getChildColumnNo(j))->getName());
      assert(column != nullptr);
      fk_element->set_column(column);

      // Set referenced column which is in the parent table
      fk_element->referenced_column_name(
          parent_table->getColumn(ndb_fk.getParentColumnNo(j))->getName());
    }
  }
  return true;
}

bool Ndb_metadata::create_table_def(Ndb *ndb, dd::Table *table_def) const {
  DBUG_TRACE;

  // name
  const char *table_name = m_ndbtab->getName();
  table_def->set_name(table_name);
  DBUG_PRINT("info", ("table_name: '%s'", table_name));

  // collation_id, default collation for columns
  // Missing in NDB.
  // The collation_id is actually only interesting when adding new columns
  // without specifying collation for the new columns, the new columns will
  // then get their collation from the table. Each existing column which
  // need a collation already have the correct value set as a property
  // on the column
  // table_def->set_collation_id(some_collation_id);

  // engine
  table_def->set_engine("ndbcluster");

  // row_format
  if (m_ndbtab->getForceVarPart() == false) {
    table_def->set_row_format(dd::Table::RF_FIXED);
  } else {
    table_def->set_row_format(dd::Table::RF_DYNAMIC);
  }

  // comment
  // Missing in NDB.
  // Currently contains several NDB_TABLE= properties controlling how
  // the table is created in NDB, most of those should be possible to
  // reverse engineer by looking a the various NDB table properties.
  // The comment may also contains other text which is not stored
  // in NDB.
  // table_def->set_comment(some_comment);

  // se_private_id, se_private_data
  ndb_dd_table_set_spi_and_version(table_def, m_ndbtab->getObjectId(),
                                   m_ndbtab->getObjectVersion());

  // storage
  // no DD API setters or types available -> hardcode
  {
    const NdbDictionary::Column::StorageType type = m_ndbtab->getStorageType();
    switch (type) {
      case NdbDictionary::Column::StorageTypeDisk:
        table_def->options().set(key_storage, HA_SM_DISK);
        break;
      case NdbDictionary::Column::StorageTypeMemory:
        table_def->options().set(key_storage, HA_SM_MEMORY);
        break;
      case NdbDictionary::Column::StorageTypeDefault:
        // Not set
        break;
    }
  }

  if (check_partitioning) {
    // partition_type
    dd::Table::enum_partition_type partition_type = dd::Table::PT_AUTO;
    switch (m_ndbtab->getFragmentType()) {
      case NdbDictionary::Table::UserDefined:
        DBUG_PRINT("info", ("UserDefined"));
        // BY KEY
        partition_type = dd::Table::PT_KEY_55;
        break;
      case NdbDictionary::Table::HashMapPartition:
        DBUG_PRINT("info", ("HashMapPartition"));
        if (m_ndbtab->getFragmentCount() != 0) {
          partition_type = dd::Table::PT_KEY_55;
        }
        break;
      default:
        // ndbcluster uses only two different FragmentType's
        assert(false);
        break;
    }
    table_def->set_partition_type(partition_type);

    // default_partitioning
    table_def->set_default_partitioning(dd::Table::DP_YES);
    // partition_expression
    table_def->set_partition_expression(partition_expression());
    // partition_expression_utf8()
    // table_def->set_partition_expression_utf8();
    // subpartition_type
    // table_def->set_subpartition_type();
    // default_subpartitioning
    // table_def->set_default_subpartitioning();
    // subpartition_expression
    // table_def->set_subpartition_expression();
    // subpartition_expression_utf8
    // table_def->set_subpartition_expression_utf8();
  }

  create_columns(table_def);

  if (!create_indexes(ndb->getDictionary(), table_def)) {
    ndb_log_error("Failed to create indexes");
    return false;
  }

  if (!create_foreign_keys(ndb, table_def)) {
    ndb_log_error("Failed to create foreign keys");
    return false;
  }

  return true;
}

bool Ndb_metadata::lookup_tablespace_id(THD *thd, dd::Table *table_def) {
  DBUG_TRACE;

  Ndb_dd_client dd_client(thd);
  dd_client.disable_auto_rollback();

  // tablespace_id
  // The id of the tablespace in DD.

  if (!ndb_table_has_tablespace(m_ndbtab)) {
    // No tablespace
    return true;
  }

  // Set magic flag telling SHOW CREATE and CREATE LIKE that tablespace
  // was specified for this table
  table_def->options().set(magic_key_explicit_tablespace, true);

  // Lookup tablespace_by name if name is available
  const char *tablespace_name = ndb_table_tablespace_name(m_ndbtab);
  if (tablespace_name) {
    DBUG_PRINT("info", ("tablespace_name: '%s'", tablespace_name));
    dd::Object_id tablespace_id;
    if (!dd_client.lookup_tablespace_id(tablespace_name, &tablespace_id)) {
      ndb_log_error("Failed to look up tablepace id of table %s",
                    m_ndbtab->getName());
      return false;
    }

    table_def->set_tablespace_id(tablespace_id);

    return true;
  }

  // Lookup tablespace_id by object id
  Uint32 object_id, object_version;
  if (m_ndbtab->getTablespace(&object_id, &object_version)) {
    DBUG_PRINT("info", ("tablespace_id: %u, tablespace_version: %u", object_id,
                        object_version));

    // NOTE! Need to store the object id and version of tablespace
    // in se_private_data to be able to lookup a tablespace by object id
    m_compare_tablespace_id = false;  // Skip comparing tablespace_id for now

    return true;
  }

  // Table had tablespace but neither name or id was available -> fail
  assert(false);
  return false;
}

class Compare_context {
 public:
  enum object_type { COLUMN, INDEX, FOREIGN_KEY };

 private:
  std::vector<std::string> diffs;
  void add_diff(const char *property, std::string a, std::string b) {
    std::string diff;
    diff.append("Diff in '")
        .append(property)
        .append("' detected, '")
        .append(a)
        .append("' != '")
        .append(b)
        .append("'");
    diffs.push_back(diff);
  }

  void add_diff(object_type type, const char *name, const char *property,
                std::string a, std::string b) {
    std::string object_type_string;
    switch (type) {
      case COLUMN:
        object_type_string = "column '";
        break;
      case INDEX:
        object_type_string = "index '";
        break;
      case FOREIGN_KEY:
        object_type_string = "foreign key '";
        break;
      default:
        assert(false);
        object_type_string = "";
    }
    std::string diff;
    diff.append("Diff in ")
        .append(object_type_string)
        .append(name)
        .append(".")
        .append(property)
        .append("' detected, '")
        .append(a)
        .append("' != '")
        .append(b)
        .append("'");
    diffs.push_back(diff);
  }

 public:
  void compare(const char *property, dd::String_type a, dd::String_type b) {
    if (a == b) return;
    add_diff(property, a.c_str(), b.c_str());
  }

  void compare(const char *property, unsigned long long a,
               unsigned long long b) {
    if (a == b) return;
    add_diff(property, std::to_string(a), std::to_string(b));
  }

  void compare(object_type type, const char *name, const char *property,
               dd::String_type a, dd::String_type b) {
    if (a == b) return;
    add_diff(type, name, property, a.c_str(), b.c_str());
  }

  void compare(object_type type, const char *name, const char *property,
               unsigned long long a, unsigned long long b) {
    if (a == b) return;
    add_diff(type, name, property, std::to_string(a), std::to_string(b));
  }

  bool equal() {
    if (diffs.size() == 0) return true;

    // Print the list of diffs
    ndb_log_error("Metadata check has failed");
    ndb_log_error(
        "The NDB Dictionary table definition is not identical to the DD table "
        "definition");
    for (const std::string &diff : diffs) ndb_log_error("%s", diff.c_str());

    return false;
  }
};

bool Ndb_metadata::compare_table_def(const dd::Table *t1,
                                     const dd::Table *t2) const {
  DBUG_TRACE;
  Compare_context ctx;

  // name
  // When using lower_case_table_names==2 the table will be
  // created using lowercase in NDB while still be original case in DD.
  ctx.compare("name", t1->name(), ndb_dd_fs_name_case(t2->name()).c_str());

  // collation_id
  // ctx.compare("collation_id", t1->collation_id(), t2->collation_id());

  // tablespace_id (local)
  if (m_compare_tablespace_id) {
    // The id has been looked up from DD
    ctx.compare("tablespace_id", t1->tablespace_id(), t2->tablespace_id());
  } else {
    // It's known that table has tablespace but it could not be
    // looked up(yet), just check that DD definition have tablespace_id
    assert(t2->tablespace_id());
  }

  // Check magic flag "options.explicit_tablespace"
  {
    bool t1_explicit = false;
    bool t2_explicit = false;
    if (t1->options().exists(magic_key_explicit_tablespace)) {
      t1->options().get(magic_key_explicit_tablespace, &t1_explicit);
    }
    if (t2->options().exists(magic_key_explicit_tablespace)) {
      t2->options().get(magic_key_explicit_tablespace, &t2_explicit);
    }
    ctx.compare("options.explicit_tablespace", t1_explicit, t2_explicit);
  }

  // engine
  ctx.compare("engine", t1->engine(), t2->engine());

  // row format
  ctx.compare("row_format", t1->row_format(), t2->row_format());

  // comment
  // ctx.compare("comment", t1->comment(), t2->comment());

  // se_private_id and se_private_data.object_version (local)
  {
    Ndb_dd_handle t1_handle = ndb_dd_table_get_spi_and_version(t1);
    Ndb_dd_handle t2_handle = ndb_dd_table_get_spi_and_version(t2);
    ctx.compare("se_private_id", t1_handle.spi, t2_handle.spi);
    ctx.compare("object_version", t1_handle.version, t2_handle.version);
  }

  // storage
  // No DD API getter or types defined, use uint32
  {
    uint32 t1_storage = UINT_MAX32;
    uint32 t2_storage = UINT_MAX32;
    if (t1->options().exists(key_storage)) {
      t1->options().get(key_storage, &t1_storage);
    }
    if (t2->options().exists(key_storage)) {
      t2->options().get(key_storage, &t2_storage);
    }
    // There's a known bug in tables created in mysql versions <= 5.1.57 where
    // the storage type of the table was not stored in NDB Dictionary but was
    // present in the .frm. Thus, we accept that this is a known mismatch and
    // skip the comparison of this attribute for tables created using earlier
    // versions
    ulong t2_previous_mysql_version = UINT_MAX32;
    if (!ndb_dd_table_get_previous_mysql_version(t2,
                                                 t2_previous_mysql_version) ||
        t2_previous_mysql_version > 50157) {
      ctx.compare("options.storage", t1_storage, t2_storage);
    }
  }

  if (check_partitioning) {
    // partition_type
    ctx.compare("partition_type", t1->partition_type(), t2->partition_type());
    // default_partitioning
    ctx.compare("default_partitioning", t1->default_partitioning(),
                t2->default_partitioning());
    // partition_expression
    ctx.compare("partition_expression", t1->partition_expression(),
                t2->partition_expression());
    // partition_expression_utf8
    ctx.compare("partition_expression_utf8", t1->partition_expression_utf8(),
                t2->partition_expression_utf8());
    // subpartition_type
    ctx.compare("subpartition_type", t1->subpartition_type(),
                t2->subpartition_type());
    // default_subpartitioning
    ctx.compare("default_subpartitioning", t1->default_subpartitioning(),
                t2->default_subpartitioning());
    // subpartition_expression
    ctx.compare("subpartition_expression", t1->subpartition_expression(),
                t2->subpartition_expression());
    // subpartition_expression_utf8
    ctx.compare("subpartition_expression_utf8",
                t1->subpartition_expression_utf8(),
                t2->subpartition_expression_utf8());
  }

  // Column count
  /*
    Diff in 'column_count' detected, '1' != '2'

    Virtual generated columns aren't stored in NDB
  */
  // ctx.compare("column_count", t1->columns().size(), t2->columns().size());

  dd::Table::Column_collection::const_iterator col_it1(t1->columns().begin());
  for (; col_it1 != t1->columns().end(); ++col_it1) {
    const dd::Column *column1 = *col_it1;
    const dd::Column *column2 = t2->get_column(column1->name());
    if (column2 == nullptr) continue;

    // column name
    ctx.compare("column_name", column1->name(), column2->name());

    const char *column_name = column1->name().c_str();

    /*
      Diff in 'column_type' detected, '17' != '29'

      Problem with CHAR(0) columns which are stored as Bit in NDB Dictionary.
      Don't see a way by which the two types can be distinguished currently

      Diff in 'col5.type' detected, '29' != '22'
      Diff in 'flags.type' detected, '29' != '23'

      Problem with enum and set columns that are stored as Char in NDB
      Dictionary

      Diff in 'b.type' detected, '26' != '27'

      b blob comment 'NDB_COLUMN=MAX_BLOB_PART_SIZE'. The partSize is used to
      differentiate between the different types of blobs i.e. TINY_BLOB, BLOB,
      etc. Setting it via comment breaks this
    */
    const dd::enum_column_types column1_type = column1->type();
    const dd::enum_column_types column2_type = column2->type();
    if (column1_type != dd::enum_column_types::BIT &&
        column1_type != dd::enum_column_types::LONG_BLOB &&
        column2_type != dd::enum_column_types::ENUM &&
        column2_type != dd::enum_column_types::SET)
      ctx.compare(Compare_context::COLUMN, column_name, "type",
                  static_cast<unsigned long long>(column1_type),
                  static_cast<unsigned long long>(column2_type));

    ctx.compare(Compare_context::COLUMN, column_name, "nullable",
                column1->is_nullable(), column2->is_nullable());

    /*
      Diff in 'c3.unsigned' detected, '0' != '1' -> FLOAT UNSIGNED
      Diff in 'c16.unsigned' detected, '0' != '1' -> DOUBLE UNSIGNED

      Floats and doubles don't have equivalent unsigned types in NDB
      Dictionary unlike Decimals
    */
    if (column1_type != dd::enum_column_types::FLOAT &&
        column1_type != dd::enum_column_types::DOUBLE)
      ctx.compare(Compare_context::COLUMN, column_name, "unsigned",
                  column1->is_unsigned(), column2->is_unsigned());

    /*
      Diff in 'ushort.zerofill' detected, '0' != '1'

      Doesn't look like NDB Dictionary stores anything related to zerofill
    */
    // ctx.compare(Compare_context::COLUMN, column_name,
    // "zerofill", column1->is_zerofill(),
    //            column2->is_zerofill());

    ctx.compare(Compare_context::COLUMN, column_name, "autoinc",
                column1->is_auto_increment(), column2->is_auto_increment());

    /*
      Diff in column 'misc.ordinal' detected, '2' != '3'

      Ordinal positions aren't the same for tables with generated columns since
      they aren't stored in NDB Dictionary
    */
    // ctx.compare(Compare_context::COLUMN, column_name, "ordinal",
    //             column1->ordinal_position(), column2->ordinal_position());

    /*
      Works for string types such as varchar, char, varbinary but not others:

      Diff in 'column_length' detected, '1' != '11' -> int AUTO_INCREMENT
      Diff in 'column_length' detected, '1' != '10' -> int unsigned
      Diff in 'column_length' detected, '1' != '12' -> float
      Diff in 'column_length' detected, '1' != '10' -> time
      Diff in 'column_length' detected, '1' != '10' -> date
      Diff in 'column_length' detected, '1' != '19' -> datetime
      Diff in 'column_length' detected, '0' != '65535' -> blob
      Diff in 'column_length' detected, '1' != '7' -> decimal
      Diff in 'column_length' detected, '1' != '22' -> timestamp

      Note that the getLength() function is used to obtain the length. There
      are also a number of getSize*() functions but a quick look suggests that
      doesn't contain the values we're looking for either
    */
    if (column1_type == dd::enum_column_types::VARCHAR ||
        column1_type == dd::enum_column_types::VAR_STRING)
      ctx.compare(Compare_context::COLUMN, column_name, "length",
                  column1->char_length(), column2->char_length());

    /*
      Precision is set only decimal types in NDB Dictionary

      Diff in 'column_precision' detected, '0' != '10' -> int
      Diff in 'column_precision' detected, '0' != '10' -> int
      Diff in 'column_precision' detected, '0' != '12' -> float
    */
    if (column1_type == dd::enum_column_types::NEWDECIMAL)
      ctx.compare(Compare_context::COLUMN, column_name, "precision",
                  column1->numeric_precision(), column2->numeric_precision());

    /*
      Diff in 'real_float.scale' detected, '0' != '1
      Diff in 'real_double.scale' detected, '0' != '4'

      Scale isn't stored for float and double types in NDB Dictionary (why?)
    */
    // ctx.compare(Compare_context::COLUMN, column_name, "scale",
    // column1->numeric_scale(),
    //            column2->numeric_scale());

    ctx.compare(Compare_context::COLUMN, column_name, "datetime_precision",
                column1->datetime_precision(), column2->datetime_precision());

    ctx.compare(Compare_context::COLUMN, column_name, "datetime_precision_null",
                column1->is_datetime_precision_null(),
                column2->is_datetime_precision_null());

    /*
      Diff in 'cid.has_no_default' detected, '1' != '0'

      cid smallint(5) unsigned NOT NULL default '0' seemingly breaks the
      assumptions in the code used to determine if a default exists or not.
    */
    // ctx.compare(Compare_context::COLUMN, column_name,
    // "has_no_default", column1->has_no_default(),
    //            column2->has_no_default());

    /*
      Diff in 'b1.default_null' detected, '1' != '0'

      More problems with default values. This occurs during table discovery
    */
    // ctx.compare(Compare_context::COLUMN, column_name,
    // "default_null",
    // column1->is_default_value_null(),
    //            column2->is_default_value_null());

    /*
      Problem with INT NOT NULL with implicit defaults

      Diff in column 'c1.default_value' detected, '' != ''
      Diff in column 'c4.default_value' detected, '' != ''
      Diff in column 'c16.default_value' detected, '' != ''
    */
    // ctx.compare(Compare_context::COLUMN, column_name,
    // "default_value", column1->default_value(),
    //            column2->default_value());

    /*
      Strange that the DD table thinks that these columns have NULL default.
      Especially since the same columns have "is_default_value_null" set to
      false.

      Diff in column 'c1.default_utf8_null' detected, '0' != '1' -> INT
      PRIMARY KEY AUTO_INCREMENT
      Diff in column 'c16.default_utf8_null' detected, '0' != '1' -> INT
      UNSIGNED NOT NULL
    */
    // ctx.compare(Compare_context::COLUMN, column_name,
    // "default_utf8_null",
    //            column1->is_default_value_utf8_null(),
    //            column2->is_default_value_utf8_null());

    // Same issue as above
    // ctx.compare(Compare_context::COLUMN, column_name,
    // "default_utf8_value",
    // column1->default_value_utf8(),
    //            column2->default_value_utf8());

    ctx.compare(Compare_context::COLUMN, column_name, "virtual",
                column1->is_virtual(), column2->is_virtual());

    /*
      Diff in column 'b.generation_expression' detected, '' != '(`a` * 2)'

      Generated expressions aren't stored in NDB Dictionary
    */
    // ctx.compare(Compare_context::COLUMN, column_name,
    //            "generation_expression", column1->generation_expression(),
    //            column2->generation_expression());

    /*
      Diff in column 'b.generation_expression_null' detected, '1' != '0'

      Generated expressions aren't stored in NDB Dictionary
    */
    // ctx.compare(Compare_context::COLUMN, column_name,
    //            "generation_expression_null",
    //            column1->is_generation_expression_null(),
    //            column2->is_generation_expression_null());

    /*
      Diff in column 'b.generation_expression_utf8' detected, '' != '(`a` *
      2)'

      Generated expressions aren't stored in NDB Dictionary
    */
    // ctx.compare(Compare_context::COLUMN, column_name,
    //            "generation_expression_utf8",
    //            column1->generation_expression_utf8(),
    //            column2->generation_expression());

    /*
      Diff in column 'b.generation_expression_utf8_null' detected, '1' != '0'

      Generated expressions aren't stored in NDB Dictionary
    */
    // ctx.compare(Compare_context::COLUMN, column_name,
    //            "generation_expression_utf8_null",
    //            column1->is_generation_expression_utf8_null(),
    //            column2->is_generation_expression_utf8_null());

    /*
      Diff in 'modified.default_option' detected, '' != 'CURRENT_TIMESTAMP'

      NDB Dictionary doesn't store default options related to time?
    */
    // ctx.compare(Compare_context::COLUMN, column_name,
    // "default_option", column1->default_option(),
    //            column2->default_option());

    /*
      Diff in 'column_update_option' detected, '' != 'CURRENT_TIMESTAMP'

      Same as "default_option"
    */
    // ctx.compare(Compare_context::COLUMN, column_name,
    // "update_option", column1->update_option(),
    //            column2->update_option());

    /*
      Diff in 'column_comment' detected, '' != 'NDB_COLUMN=MAX_BLOB_PART_SIZE'
                                         '' != 'NDB_COLUMN=BLOB_INLINE_SIZE'

      Column comments aren't stored in NDB Dictionary
    */
    // ctx.compare(Compare_context::COLUMN, column_name,
    // "comment", column1->comment(),
    //            column2->comment());

    /*
      Diff in 'column_hidden' detected, '1' != '4'.

      Column hidden types are not stored in the NDB Dictionary.
    */
    // ctx.compare(Compare_context::COLUMN, column_name, "hidden",
    // static_cast<unsigned long long>(column1->hidden()), static_cast<unsigned
    // long long>(column2->hidden()));

    // Column options
    const dd::Properties *col1_options = &column1->options();
    const dd::Properties *col2_options = &column2->options();

    // Storage

    /*
      CREATE TABLE t3 (
        a INT STORAGE DISK,
        b INT COLUMN_FORMAT DYNAMIC,
        c BIT(8) NOT NULL
      ) TABLESPACE ts1 ENGINE NDB;

      In the above table, column a will be marked as stored on disk.

      CREATE TABLE t4 (
        a INT PRIMARY KEY,
        b INT NOT NULL
      ) STORAGE DISK TABLESPACE ts1 ENGINE NDB;

      Table t4 is marked as stored on disk. Column b is marked as stored on
      disk in NDB Dictionary but not in DD:

      Diff in 'b.storage' detected, '1' != '4294967295'
    */
    /*uint32 col1_storage = UINT_MAX32;
    uint32 col2_storage = UINT_MAX32;
    if (col1_options->exists(key_storage)) {
      col1_options->get(key_storage, &col1_storage);
    }
    if (col2_options->exists(key_storage)) {
      col2_options->get(key_storage, &col2_storage);
    }
    ctx.compare(Compare_context::COLUMN, column_name, "storage",
                static_cast<unsigned long long>(col1_storage),
                static_cast<unsigned long long>(col2_storage));*/

    // Format
    /*
      Diff in 'c16.format' detected, '2' != '4294967295'

      ALTER TABLE ADD <COLUMN> marks the column as dynamic in NDB Dictionary
      but not in DD

      Diff in 'b.format' detected, '4294967295' != '1'

      b INT COLUMN_FORMAT FIXED causes the above issue. NDB Dictionary only
      has information if it is dynamic or not. It's difficult to differentiate
      between COLUMN_FORMAT_TYPE_DEFAULT and COLUMN_FORMAT_TYPE_FIXED when
      getDynamic() is false

      Diff in 'a.format' detected, '4294967295' != '2'

      a int column_format DYNAMIC STORAGE DISK. Dynamic column with disk
      storage is not supported which results in setDynamic(false) in NDB
      Dictionary but the DD continues to think that the column is dynamic

      Diff in 'a.format' detected, '2' != '4294967295'
      Diff in 'b.format' detected, '2' != '4294967295'

      Setting ROW_FORMAT=DYNAMIC for the table leads to the columns being
      marked as dynamic in NDB Dictionary but not in DD
    */
    /*uint32 col1_format = UINT_MAX32;
    uint32 col2_format = UINT_MAX32;
    if (col1_options->exists(key_column_format)) {
      col1_options->get(key_column_format, &col1_format);
    }
    if (col2_options->exists(key_column_format)) {
      col2_options->get(key_column_format, &col2_format);
    }
    ctx.compare(Compare_context::COLUMN, column_name, "format",
                static_cast<unsigned long long>(col1_format),
                static_cast<unsigned long long>(col2_format));*/

    // Treat bit as char
    /*
      Diff in 'column_treat_bit_as_char_option_exists' detected, '1' != '0'
      Problem with CHAR(0) columns which are stored as Bit in NDB Dictionary.
      Didn't see a way by which the two types can be distinguished currently
    */
    /*const bool col1_bit_as_char_option_exists =
        col1_options->exists(key_column_bit_as_char);
    const bool col2_bit_as_char_option_exists =
        col2_options->exists(key_column_bit_as_char);
    ctx.compare(Compare_context::COLUMN, column_name,
    "treat_bit_as_char_option_exists", col1_bit_as_char_option_exists,
                col2_bit_as_char_option_exists);
    if (col1_bit_as_char_option_exists && col2_bit_as_char_option_exists) {
      bool col1_bit_as_char;
      col1_options->get(key_column_bit_as_char, &col1_bit_as_char);
      bool col2_bit_as_char;
      col2_options->get(key_column_bit_as_char, &col2_bit_as_char);
      ctx.compare(Compare_context::COLUMN, column_name,
    "treat_bit_as_char", col1_bit_as_char, col2_bit_as_char);
    }*/

    // Not secondary
    const bool col1_not_secondary_option_exists =
        col1_options->exists(key_column_not_secondary);
    const bool col2_not_secondary_option_exists =
        col2_options->exists(key_column_not_secondary);
    ctx.compare(Compare_context::COLUMN, column_name,
                "not_secondary_option_exists", col1_not_secondary_option_exists,
                col2_not_secondary_option_exists);
    if (col1_not_secondary_option_exists && col2_not_secondary_option_exists) {
      bool col1_not_secondary;
      col1_options->get(key_column_not_secondary, &col1_not_secondary);
      bool col2_not_secondary;
      col2_options->get(key_column_not_secondary, &col2_not_secondary);
      ctx.compare(Compare_context::COLUMN, column_name, "not_secondary",
                  col1_not_secondary, col2_not_secondary);
    }

    // Is array
    const bool col1_is_array_option_exists =
        col1_options->exists(key_column_is_array);
    const bool col2_is_array_option_exists =
        col2_options->exists(key_column_is_array);
    ctx.compare(Compare_context::COLUMN, column_name, "is_array_option_exists",
                col1_is_array_option_exists, col2_is_array_option_exists);
    if (col1_is_array_option_exists && col2_is_array_option_exists) {
      bool col1_is_array;
      col1_options->get(key_column_is_array, &col1_is_array);
      bool col2_is_array;
      col2_options->get(key_column_is_array, &col2_is_array);
      ctx.compare(Compare_context::COLUMN, column_name, "is_array",
                  col1_is_array, col2_is_array);
    }

    /*
      Diff in 't_point.geom_type' detected, '4294967295' != '1'
      Diff in 't_linestring.geom_type' detected, '4294967295' != '2'
      <snip>
      Diff in 't_geometry.geom_type' detected, '4294967295' != '0'

      Geometry types are stored as blobs in NDB Dictionary with no further
      information about sub-types
    */
    // Geometry type
    /*uint32 col1_geom_type = UINT_MAX32;
    uint32 col2_geom_type = UINT_MAX32;
    if (col1_options->exists(key_column_geom_type)) {
      col1_options->get(key_column_geom_type, &col1_geom_type);
    }
    if (col2_options->exists(key_column_geom_type)) {
      col2_options->get(key_column_geom_type, &col2_geom_type);
    }
    ctx.compare(Compare_context::COLUMN, column_name,
    "geom_type", static_cast<unsigned long long>(col1_geom_type),
                static_cast<unsigned long long>(col2_geom_type));*/

    // SE Private Data skipped for now since we don't store anything for
    // columns

    // SE engine attributes skipped

    /* Should be possible to set once we look at indexes
    ctx.compare(Compare_context::COLUMN, column_name, "key",
    column1->column_key(),
    column2->column_key());*/

    /*
      Diff in 'column_type_utf8' detected, '' != 'int'
      Diff in 'column_type_utf8' detected, '' != 'int unsigned'
      Diff in 'column_type_utf8' detected, '' != 'float'
      Diff in 'column_type_utf8' detected, '' != 'varchar(255)'
      Diff in 'column_type_utf8' detected, '' != 'time'
      Diff in 'column_type_utf8' detected, '' != 'date'
      Diff in 'column_type_utf8' detected, '' != 'datetime'
      Diff in 'column_type_utf8' detected, '' != 'blob'
      Diff in 'column_type_utf8' detected, '' != 'char(30)'
      Diff in 'column_type_utf8' detected, '' != 'varbinary(255)'
      Diff in 'column_type_utf8' detected, '' != 'decimal(5,2)'
      Diff in 'column_type_utf8' detected, '' != 'datetime(6)'
      Diff in 'column_type_utf8' detected, '' != 'timestamp(2)'
      Diff in 'column_type_utf8' detected, '' != 'timestamp'

      For all columns. Need to implement a function that generates the string
      by looking at the types and other details. See
      get_sql_type_by_create_field() in dd_table.cc
    */
    // ctx.compare(Compare_context::COLUMN, column_name,
    // "type_utf8", column1->column_type_utf8(),
    //            column2->column_type_utf8());

    ctx.compare(Compare_context::COLUMN, column_name, "is_array",
                column1->is_array(), column2->is_array());
  }

  // Index count
  /*
    Diff in 'index_count' detected, '0' != '1'

    Every NDB table has a built-in primary key using HASH. In addition to this,
    there's a "companion" ordered index created on the primary key to facilitate
    different kinds of look-up queries. The additional ordered index is not
    created when "using HASH" is explicitly specified which leads to the below
    mismatch.

    There's also a long standing issue with metadata restore using the
    ndb_restore tool where the indexes are not created at the same time as
    tables. This makes the below check prone to failure with restore and
    auto sync/discovery
  */
  // ctx.compare("index_count", t1->indexes().size(), t2->indexes().size());

  dd::Table::Index_collection::const_iterator index_it1(t1->indexes().begin());
  for (; index_it1 != t1->indexes().end(); ++index_it1) {
    const dd::Index *index1 = *index_it1;
    dd::Table::Index_collection::const_iterator index_it2(
        t2->indexes().begin());
    const dd::Index *index2 = nullptr;
    if (index1->name() == (*index_it2)->name()) {
      index2 = *index_it2;
    } else {
      // Order mismatch after the indexes are created using ndb_restore. The
      // sortById() trick doesn't work in such cases
      for (; index_it2 != t2->indexes().end(); ++index_it2) {
        if (index1->name() == (*index_it2)->name()) {
          index2 = *index_it2;
          break;
        }
      }
      if (index2 == nullptr) {
        // Index not found in the DD table. Continue to the next index
        // comparison
        ctx.compare("index_name", index1->name(), "");
        continue;
      }
    }

    // Index name
    ctx.compare("index_name", index1->name(), index2->name());
    const char *index_name = index1->name().c_str();

    // Generated
    /*
      Diff in 'fk2.generated' detected, '0' != '1'

      This occurs when keys are auto-generated to support FKs in cases where
      the user doesn't explicitly create a key on the column
    */
    // ctx.compare(Compare_context::INDEX, index_name,
    // "generated",
    //            index1->is_generated(), index2->is_generated());

    // Hidden
    ctx.compare(Compare_context::INDEX, index_name, "hidden",
                index1->is_hidden(), index2->is_hidden());

    // Comment
    ctx.compare(Compare_context::INDEX, index_name, "comment",
                index1->comment(), index2->comment());

    // Options skipped as they don't correspond to any information stored in
    // NDB Dictionary

    // SE Private Data skipped as nothing stored by NDB for indexes

    // Tablespace ID
    ctx.compare(Compare_context::INDEX, index_name, "tablespace",
                index1->tablespace_id(), index2->tablespace_id());

    // Engine
    ctx.compare(Compare_context::INDEX, index_name, "engine", index1->engine(),
                index2->engine());

    // Type
    /*
      Diff in 'pk.type' detected, '3' != '2'

      Problem seen when a unique index is created on a column on which a
      hidden PK exists. The unique index becomes the PK but this breaks the
      assumption inside create_indexes() that the PK is named "PRIMARY". There
      should be a way to use getNoOfPrimaryKeys() and getPrimaryKey() to reverse
      engineer the name and check for that as well. This is left as part of a
      later task

      CREATE TABLE t5 (
        a INT NOT NULL
      ) ENGINE NDB;
      CREATE UNIQUE INDEX pk ON t5(a);

      The metadata check for the below DDL statements work fine:
      CREATE TABLE t5 (
        a INT PRIMARY KEY,
        b INT NOT NULL
      ) ENGINE NDB;
      CREATE UNIQUE INDEX pk ON t5(b);
    */
    // ctx.compare(Compare_context::INDEX, index_name, "type",
    //            static_cast<unsigned long long>(index1->type()),
    //            static_cast<unsigned long long>(index2->type()));

    // Algorithm

    /*
      CREATE TABLE t1 (a int primary key, b int, unique(b)) engine=ndb;
      This creates two indexes "b" and "b$unique". If someone uses ndb_restore
      or ndb_drop_index to drop one or the other of "b" and "b$unique" (but not
      both), this test will fail.  In general, post-bug#28584066, DD and NDB
      are not required to agree about indexes.
    */
    // ctx.compare(Compare_context::INDEX, index_name, "algorithm",
    //             static_cast<unsigned long long>(index1->algorithm()),
    //             static_cast<unsigned long long>(index2->algorithm()));

    // Explicit algorithm
    /*
      Diff in 'UNIQUE_t0_0.explicit_algo' detected, '0' != '1'

      UNIQUE INDEX UNIQUE_t0_0 USING BTREE is a problem. Doesn't seem a way to
      differentiate between 'UNIQUE INDEX UNIQUE_t0_0' and 'UNIQUE INDEX
      UNIQUE_t0_0 USING BTREE' since both have the same algorithm from an NDB
      Dictionary perspective
    */
    // ctx.compare(Compare_context::INDEX, index_name,
    //            "explicit_algo", index1->is_algorithm_explicit(),
    //            index2->is_algorithm_explicit());

    // Visible
    /*
      Diff in index 'a.visible' detected, '1' != '0'

      No information in NDB Dictionary as to whether an index is invisible
    */
    // ctx.compare(Compare_context::INDEX, index_name, "visible",
    //            index1->is_visible(), index2->is_visible());

    // Engine attributes and Secondary engine attributes skipped

    // Ordinal position
    /*
      Diff in 'index PRIMARY.position' detected, '2' != '1'

      Order mismatch after the indexes are created using ndb_restore. Also
      when an index is created using ALTER TABLE/CREATE INDEX
    */
    // ctx.compare(Compare_context::INDEX, index_name,
    // "position",
    //            index1->ordinal_position(), index2->ordinal_position());

    // Candidate key
    /*
      Diff in 'PRIMARY.candidate_key' detected, '0' != '1'
    */
    // ctx.compare(Compare_context::INDEX, index_name,
    // "candidate_key", index1->is_candidate_key(),
    //            index2->is_candidate_key());

    // Index elements
    // Element count
    ctx.compare(Compare_context::INDEX, index_name, "element_count",
                index1->elements().size(), index2->elements().size());

    dd::Index::Index_elements::const_iterator idx_elem_it1(
        index1->elements().begin());
    for (; idx_elem_it1 != index1->elements().end(); ++idx_elem_it1) {
      const dd::Index_element *idx_element1 = *idx_elem_it1;
      dd::Index::Index_elements::const_iterator idx_elem_it2(
          index2->elements().begin());
      const dd::Index_element *idx_element2 = nullptr;
      if (idx_element1->column().name() == (*idx_elem_it2)->column().name()) {
        idx_element2 = *idx_elem_it2;
      } else {
        for (; idx_elem_it2 != index2->elements().end(); ++idx_elem_it2) {
          if (idx_element1->column().name() ==
              (*idx_elem_it2)->column().name()) {
            idx_element2 = *idx_elem_it2;
            break;
          }
        }
        if (idx_element2 == nullptr) {
          // Index element not found. Continue to the next index element
          // comparison
          ctx.compare(Compare_context::INDEX, index_name, "element.column",
                      idx_element1->column().name(), "");
          continue;
        }
      }
      ctx.compare(Compare_context::INDEX, index_name, "element.column",
                  idx_element1->column().name(), idx_element2->column().name());

      // Ordinal position
      /*
        Diff in index 'c.element.ordinal_position' detected, '1' != '2'
        Diff in index 'c.element.ordinal_position' detected, '2' != '1'

        CREATE TABLE t1 (
          a INT,
          b INT,
          c INT,
          PRIMARY KEY(a,c),
          UNIQUE(c,b)
        ) ENGINE NDB;

        Order in which the elements, i.e. columns, are stored in NDB Dictionary
        don't necessarily match the order specified in the query. In the above
        query, NDB Dictionary returns the columns of the unique index as (b,c).
        This could be circumvented by traversing both sets of columns but the
        ordinal positions will remain mismatched.
      */
      // ctx.compare(Compare_context::INDEX, index_name,
      //              "element.ordinal_position",
      //              idx_element1->ordinal_position(),
      //              idx_element2->ordinal_position());

      // Length
      /*
        Diff in 'PRIMARY.element.length' detected, '4294967295' != '4'

        Nothing in NDB Dictionary to represent length
      */
      // ctx.compare(Compare_context::INDEX, index_name,
      // "element.length", idx_element1->length(),
      //            idx_element2->length());

      // Length null
      /*
        Diff in 'PRIMARY.element.length_null' detected, '1' != '0'

        Nothing in NDB Dictionary to represent length
      */
      // ctx.compare(Compare_context::INDEX, index_name,
      // "element.length_null",
      //            idx_element1->is_length_null(),
      //            idx_element2->is_length_null());

      // Order
      /*
        Diff in 'uk.element.order' detected, '2' != '1'

        Nothing in NDB Dictionary that represents order
      */
      // ctx.compare(Compare_context::INDEX, index_name,
      // "element.order",
      //            static_cast<unsigned long long>(idx_element1->order()),
      //            static_cast<unsigned long long>(idx_element2->order()));

      // Hidden
      // ctx.compare(Compare_context::INDEX, index_name,
      //            "element.hidden", idx_element1->is_hidden(),
      //            idx_element2->is_hidden());

      // Prefix
      /*
        Diff in 'PRIMARY.element.prefix' detected, '1' != '0'

        Nothing in NDB Dictionary that represents prefix
      */
      // ctx.compare(Compare_context::INDEX, index_name,
      // "element.prefix", idx_element1->is_prefix(),
      //            idx_element2->is_prefix());
    }
  }

  // Foreign key count
  /*
    There's also a long standing issue with metadata restore using the
    ndb_restore tool where the indexes are not created at the same time as
    tables. This makes the below check prone to failure with restore and
    auto sync/discovery
  */
  // ctx.compare("fk_count", t1->foreign_keys().size(),
  // t2->foreign_keys().size());

  dd::Table::Foreign_key_collection::const_iterator fk_it1(
      t1->foreign_keys().begin());
  dd::Table::Foreign_key_collection::const_iterator fk_it2(
      t2->foreign_keys().begin());
  for (; fk_it1 != t1->foreign_keys().end(); ++fk_it1, ++fk_it2) {
    const dd::Foreign_key *fk1 = *fk_it1;
    const dd::Foreign_key *fk2 = nullptr;
    if (fk1->name() == (*fk_it2)->name()) {
      fk2 = *fk_it2;
    } else {
      /*
        Mismatch in order when FKs are created by consecutive ALTER statements.
        At first glance, it looks like NDB Dictionary sticks to order of
        creation while DD does not
      */
      dd::Table::Foreign_key_collection::const_iterator fk_it3(
          t2->foreign_keys().begin());
      for (; fk_it3 != t2->foreign_keys().end(); ++fk_it3) {
        if (fk1->name() == (*fk_it3)->name()) {
          fk2 = *fk_it3;
          break;
        }
      }
      if (fk2 == nullptr) {
        // FK not found. Continue to the next FK comparison
        ctx.compare("fk_name", fk1->name(), "");
        continue;
      }
    }

    // Name
    ctx.compare("fk_name", fk1->name(), fk2->name());
    const char *fk_name = fk1->name().c_str();

    // Constraint name
    /*
      Diff in 't1_fk_1.constraint_name' detected, 'PRIMARY' != ''

      Problem with mock tables.

      Diff in foreign key 'fk1.constraint_name' detected, 'PRIMARY' != 'uk1'

      Problem with creating FKs on tables with a unique key but no explicit
      primary key. NDB Dictionary thinks the constraint name is PRIMARY while
      DD thinks it's 'uk1'

      create table t1(
        a int not null,
        b int not null,
        unique key uk1(a),
        unique key uk2(b)
      ) engine=ndb;

      create table t2(
        a int,
        constraint fk1 foreign key (a) references t1(a)
      )engine=ndb
    */
    // ctx.compare(Compare_context::FOREIGN_KEY, fk_name,
    //              "constraint_name", fk1->unique_constraint_name(),
    //              fk2->unique_constraint_name());

    // Update rule
    ctx.compare(Compare_context::FOREIGN_KEY, fk_name, "update_rule",
                static_cast<unsigned long long>(fk1->update_rule()),
                static_cast<unsigned long long>(fk2->update_rule()));

    // Delete rule
    ctx.compare(Compare_context::FOREIGN_KEY, fk_name, "delete_rule",
                static_cast<unsigned long long>(fk1->delete_rule()),
                static_cast<unsigned long long>(fk2->delete_rule()));

    // Ref catalog
    ctx.compare(Compare_context::FOREIGN_KEY, fk_name, "ref_catalog",
                fk1->referenced_table_catalog_name(),
                fk2->referenced_table_catalog_name());

    // Ref schema
    ctx.compare(Compare_context::FOREIGN_KEY, fk_name, "ref_schema",
                fk1->referenced_table_schema_name(),
                fk2->referenced_table_schema_name());

    // Ref table
    /*
      Diff in 't1_fk_1.ref_table' detected, 'NDB$FKM_13_0_t2' != 't2'

      Problem with mock tables.

      Diff in foreign key 'parent_fk_1.ref_table' detected, '#sql2-5c92d-b' !=
      'parent'

      Problem with self referential FKs during copying ALTER statements

      alter table parent
       add foreign key ref2_idx(ref2) references parent (id2),
       algorithm = copy;
    */
    // ctx.compare(Compare_context::FOREIGN_KEY, fk_name,
    //              "ref_table", fk1->referenced_table_name(),
    //              fk2->referenced_table_name());

    // Element count
    ctx.compare(Compare_context::FOREIGN_KEY, fk_name, "element_count",
                fk1->elements().size(), fk2->elements().size());

    dd::Foreign_key::Foreign_key_elements::const_iterator fk_elem1(
        fk1->elements().begin());
    dd::Foreign_key::Foreign_key_elements::const_iterator fk_elem2(
        fk2->elements().begin());
    for (; fk_elem1 != fk1->elements().end(); ++fk_elem1, ++fk_elem2) {
      // Column name
      ctx.compare(Compare_context::FOREIGN_KEY, fk_name, "element.column",
                  (*fk_elem1)->column().name(), (*fk_elem2)->column().name());

      // Referenced column name
      ctx.compare(Compare_context::FOREIGN_KEY, fk_name, "element.ref_column",
                  (*fk_elem1)->referenced_column_name(),
                  (*fk_elem2)->referenced_column_name());

      // Ordinal position
      ctx.compare(Compare_context::FOREIGN_KEY, fk_name,
                  "element.ordinal_position", (*fk_elem1)->ordinal_position(),
                  (*fk_elem2)->ordinal_position());
    }
  }
  return ctx.equal();
}

bool Ndb_metadata::check_partition_info(const dd::Table *dd_table_def) const {
  DBUG_TRACE;
  Compare_context ctx;

  // Compare the partition count of the NDB table with the partition
  // count of the table definition used by the caller
  ctx.compare("partition_count", m_ndbtab->getPartitionCount(),
              dd_table_def->partitions().size());

  // Check if the engine of the partitions are as expected
  for (size_t i = 0; i < dd_table_def->partitions().size(); i++) {
    const dd::Partition *partition = dd_table_def->partitions().at(i);
    ctx.compare("partition_engine", "ndbcluster", partition->engine());
  }

  return ctx.equal();
}

bool Ndb_metadata::check_index_count(const NdbDictionary::Dictionary *dict,
                                     const NdbDictionary::Table *ndbtab,
                                     const dd::Table *dd_table_def) {
  DBUG_TRACE;
  assert(dict != nullptr);
  unsigned int ndb_index_count;
  if (!ndb_table_index_count(dict, ndbtab, ndb_index_count)) {
    ndb_log_error("Failed to get the number of indexes for %s",
                  ndbtab->getName());
    return false;
  }
  size_t dd_index_count = dd_table_def->indexes().size();
  for (size_t i = 0; i < dd_table_def->indexes().size(); i++) {
    const dd::Index *index = dd_table_def->indexes().at(i);
    if (index->type() == dd::Index::IT_PRIMARY &&
        index->algorithm() == dd::Index::IA_HASH) {
      // PKs using hash are a special case since there's no separate index
      // created in NDB
      dd_index_count--;
    }
    if (index->type() == dd::Index::IT_UNIQUE &&
        index->algorithm() == dd::Index::IA_HASH) {
      // In case the table is not created with a primary key, unique keys
      // using hash could be mapped to being a primary key which will once
      // again lead to no separate index created in NDB
      if (ndb_index_count == 0) {
        dd_index_count--;
      }
    }
  }
  return ndb_index_count == dd_index_count;
}

bool Ndb_metadata::compare(THD *thd, Ndb *ndb, const char *dbname,
                           const NdbDictionary::Table *ndbtab,
                           const dd::Table *dd_table_def) {
  Ndb_metadata ndb_metadata(dbname, ndbtab);

  // Allow DBUG keyword to disable the comparison
  if (DBUG_EVALUATE_IF("ndb_metadata_compare_skip", true, false)) {
    return true;  // Compare disabled
  }

  // Transform NDB table to DD table def
  std::unique_ptr<dd::Table> ndb_table_def{dd::create_object<dd::Table>()};
  if (!ndb_metadata.create_table_def(ndb, ndb_table_def.get())) {
    ndb_log_error(
        "Failed to transform the NDB definition of table %s to its "
        "equivalent DD definition",
        ndbtab->getName());
    return false;
  }

  // Lookup tablespace id from DD
  if (!ndb_metadata.lookup_tablespace_id(thd, ndb_table_def.get())) {
    return false;
  }

  // Compare the table definition generated from the NDB table
  // with the table definition used by caller
  if (!ndb_metadata.compare_table_def(ndb_table_def.get(), dd_table_def)) {
    return false;
  }

  // Check the partition information of the table definition used by caller
  if (!ndb_metadata.check_partition_info(dd_table_def)) {
    return false;
  }

  return true;
}
