/*
   Copyright (c) 2017, 2020, Oracle and/or its affiliates. All rights reserved.

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
        DBUG_ASSERT(false);
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
      dd_column->set_column_key(dd::Column::enum_column_key::CK_PRIMARY);

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
    switch (element.type) {
      case NdbDictionary::Object::UniqueHashIndex:
        hash_indexes.insert(element.name);
        break;
      case NdbDictionary::Object::OrderedIndex:
        ordered_indexes.insert(element.name);
        break;
      default:
        // Unexpected object type
        DBUG_ASSERT(false);
        return false;
    }
  }

  for (unsigned int i = 0; i < list.count; i++) {
    NdbDictionary::Dictionary::List::Element &element = list.elements[i];
    const NdbDictionary::Index *ndb_index =
        dict->getIndexGlobal(element.name, m_ndbtab->getName());
    std::string index_name = ndb_index->getName();
    for (std::string::size_type n = index_name.find("@0047");
         n != std::string::npos; n = index_name.find("@0047")) {
      index_name.replace(n, 5, "/");
    }
    if (ndb_index->getType() == NdbDictionary::Index::OrderedIndex &&
        hash_indexes.find(index_name + "$unique") != hash_indexes.end()) {
      // Unless "USING HASH" is specified, creation of a unique index results in
      // the creation of both an ordered index and a hash index in NDB. Discount
      // the extra ordered index since DD has no notion of it
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
      dd_index->set_type(dd::Index::enum_index_type::IT_UNIQUE);
      if (ordered_indexes.find(real_name) == ordered_indexes.end()) {
        dd_index->set_algorithm(dd::Index::enum_index_algorithm::IA_HASH);
        dd_index->set_algorithm_explicit(true);
      }
    } else if (ndb_index->getType() == NdbDictionary::Index::OrderedIndex) {
      dd_index->set_name(index_name.c_str());
      if (index_name == "PRIMARY") {
        dd_index->set_type(dd::Index::enum_index_type::IT_PRIMARY);
      } else {
        dd_index->set_type(dd::Index::enum_index_type::IT_MULTIPLE);
      }
    } else {
      // Unexpected object type
      DBUG_ASSERT(false);
      return false;
    }

    dd_index->set_engine("ndbcluster");

    // Cycle through the columns retrieved from NDB Dictionary and add them as
    // index elements to the DD definition
    for (unsigned int j = 0; j < ndb_index->getNoOfColumns(); j++) {
      const dd::Column *column =
          table_def->get_column(ndb_index->getColumn(j)->getName());
      DBUG_ASSERT(column != nullptr);
      (void)dd_index->add_element(const_cast<dd::Column *>(column));
    }
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
    if (strcmp(child_db, ndb->getDatabaseName()) != 0 ||
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
      DBUG_ASSERT(n != std::string::npos);
      const std::string real_constraint_name = constraint_name.substr(0, n);
      dd_fk->set_unique_constraint_name(real_constraint_name.c_str());
    }

    switch (ndb_fk.getOnUpdateAction()) {
      case NdbDictionary::ForeignKey::FkAction::NoAction:
        dd_fk->set_update_rule(dd::Foreign_key::enum_rule::RULE_NO_ACTION);
        break;
      case NdbDictionary::ForeignKey::FkAction::Restrict:
        dd_fk->set_update_rule(dd::Foreign_key::enum_rule::RULE_RESTRICT);
        break;
      case NdbDictionary::ForeignKey::FkAction::Cascade:
        dd_fk->set_update_rule(dd::Foreign_key::enum_rule::RULE_CASCADE);
        break;
      case NdbDictionary::ForeignKey::FkAction::SetNull:
        dd_fk->set_update_rule(dd::Foreign_key::enum_rule::RULE_SET_NULL);
        break;
      case NdbDictionary::ForeignKey::FkAction::SetDefault:
        dd_fk->set_update_rule(dd::Foreign_key::enum_rule::RULE_SET_DEFAULT);
        break;
      default:
        DBUG_ASSERT(false);
        return false;
    }

    switch (ndb_fk.getOnDeleteAction()) {
      case NdbDictionary::ForeignKey::FkAction::NoAction:
        dd_fk->set_delete_rule(dd::Foreign_key::enum_rule::RULE_NO_ACTION);
        break;
      case NdbDictionary::ForeignKey::FkAction::Restrict:
        dd_fk->set_delete_rule(dd::Foreign_key::enum_rule::RULE_RESTRICT);
        break;
      case NdbDictionary::ForeignKey::FkAction::Cascade:
        dd_fk->set_delete_rule(dd::Foreign_key::enum_rule::RULE_CASCADE);
        break;
      case NdbDictionary::ForeignKey::FkAction::SetNull:
        dd_fk->set_delete_rule(dd::Foreign_key::enum_rule::RULE_SET_NULL);
        break;
      case NdbDictionary::ForeignKey::FkAction::SetDefault:
        dd_fk->set_delete_rule(dd::Foreign_key::enum_rule::RULE_SET_DEFAULT);
        break;
      default:
        DBUG_ASSERT(false);
        return false;
    }

    dd_fk->set_referenced_table_catalog_name("def");
    dd_fk->set_referenced_table_schema_name(parent_db);
    dd_fk->set_referenced_table_name(parent_name);

    Ndb_table_guard ndb_table_guard(ndb, parent_db, parent_name);
    const NdbDictionary::Table *parent_table = ndb_table_guard.get_table();
    for (unsigned int j = 0; j < ndb_fk.getChildColumnCount(); j++) {
      // Create FK element(s) for child columns
      dd::Foreign_key_element *fk_element = dd_fk->add_element();
      const dd::Column *column = table_def->get_column(
          m_ndbtab->getColumn(ndb_fk.getChildColumnNo(j))->getName());
      DBUG_ASSERT(column != nullptr);
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
  ndb_dd_table_set_object_id_and_version(table_def, m_ndbtab->getObjectId(),
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
        DBUG_ASSERT(false);
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
  DBUG_ASSERT(false);
  return false;
}

class Compare_context {
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
    DBUG_ASSERT(t2->tablespace_id());
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
    int t1_id, t1_version;
    ndb_dd_table_get_object_id_and_version(t1, t1_id, t1_version);
    int t2_id, t2_version;
    ndb_dd_table_get_object_id_and_version(t2, t2_id, t2_version);
    ctx.compare("se_private_id", t1_id, t2_id);
    ctx.compare("object_version", t1_version, t2_version);
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

bool Ndb_metadata::compare_indexes(const NdbDictionary::Dictionary *dict,
                                   const NdbDictionary::Table *ndbtab,
                                   const dd::Table *dd_table_def) {
  DBUG_TRACE;
  DBUG_ASSERT(dict != nullptr);
  unsigned int ndb_index_count;
  if (!ndb_table_index_count(dict, ndbtab, ndb_index_count)) {
    return false;
  }
  size_t dd_index_count = dd_table_def->indexes().size();
  for (size_t i = 0; i < dd_table_def->indexes().size(); i++) {
    const dd::Index *index = dd_table_def->indexes().at(i);
    if (index->type() == dd::Index::enum_index_type::IT_PRIMARY &&
        index->algorithm() == dd::Index::enum_index_algorithm::IA_HASH) {
      // PKs using hash are a special case since there's no separate index
      // created in NDB
      dd_index_count--;
    }
    if (index->type() == dd::Index::enum_index_type::IT_UNIQUE &&
        index->algorithm() == dd::Index::enum_index_algorithm::IA_HASH) {
      // In case the table is not created with a primary key, unique keys
      // using hash could be mapped to being a primary key which will once
      // again lead to no separate index created in NDB
      if (ndb_index_count == 0) {
        dd_index_count--;
      }
    }
  }
  Compare_context ctx;
  ctx.compare("index_count", ndb_index_count, dd_index_count);
  return ctx.equal();
}

bool Ndb_metadata::compare(THD *thd, Ndb *ndb,
                           const NdbDictionary::Table *ndbtab,
                           const dd::Table *dd_table_def) {
  Ndb_metadata ndb_metadata(ndbtab);

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
