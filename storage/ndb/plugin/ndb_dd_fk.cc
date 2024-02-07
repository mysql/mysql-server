/*
   Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

// Implements the functions declared in ndb_dd_fk.h
#include "storage/ndb/plugin/ndb_dd_fk.h"

#include "my_dbug.h"
#include "sql/dd/types/foreign_key.h"          // dd::Foreign_key
#include "sql/dd/types/foreign_key_element.h"  // dd::Foreign_key_element
#include "sql/dd/types/table.h"                // dd::Table

bool ndb_dd_fk_set_values_from_ndb(dd::Foreign_key *fk_def,
                                   const dd::Table *dd_child_table,
                                   const NdbDictionary::ForeignKey &ndb_fk,
                                   const NdbDictionary::Table *ndb_child_table,
                                   const NdbDictionary::Table *ndb_parent_table,
                                   const char *parent_schema_name) {
  DBUG_TRACE;

  // Extract the foreign key name from NDB and set it to the DD object
  // The foreign key name is of form <id>/<id>/fk_name in NDB
  std::string fully_qualified_fk_name(ndb_fk.getName());
  std::string::size_type fk_name_begin = fully_qualified_fk_name.rfind('/') + 1;
  std::string fk_name = fully_qualified_fk_name.substr(fk_name_begin);
  fk_def->set_name(fk_name.c_str());

  // Set catalog name
  fk_def->set_referenced_table_catalog_name("def");

  // Set referenced table and schema name
  fk_def->set_referenced_table_schema_name(parent_schema_name);
  fk_def->set_referenced_table_name(ndb_parent_table->getName());

  // Extract unique constraint name from NDB and set it
  if (ndb_fk.getParentIndex() == nullptr) {
    // Parent primary key is the primary index
    fk_def->set_unique_constraint_name("PRIMARY");
  } else {
    // The unique index name will be of form <id>/<id>/<id>/<uk_name>$unique
    std::string parent_index_name(ndb_fk.getParentIndex());
    std::string::size_type begin = parent_index_name.rfind('/') + 1;
    std::string::size_type end = parent_index_name.find("$unique");
    assert(parent_index_name.substr(end).compare("$unique") == 0);

    fk_def->set_unique_constraint_name(
        parent_index_name.substr(begin, end - begin).c_str());
  }

  // Add referencing columns
  const unsigned int number_of_columns = ndb_fk.getParentColumnCount();
  for (unsigned int i = 0; i < number_of_columns; i++) {
    dd::Foreign_key_element *fk_col_def = fk_def->add_element();

    // Set the column foreign key is based on
    int child_col_num = ndb_fk.getChildColumnNo(i);
    assert(child_col_num >= 0);
    const NdbDictionary::Column *ndb_child_col =
        ndb_child_table->getColumn(child_col_num);
    assert(ndb_child_col != nullptr);
    const dd::Column *dd_child_col =
        dd_child_table->get_column(ndb_child_col->getName());
    assert(dd_child_col != nullptr);
    fk_col_def->set_column(dd_child_col);

    // Set the referencing column
    int parent_col_num = ndb_fk.getParentColumnNo(i);
    assert(parent_col_num >= 0);
    const NdbDictionary::Column *ndb_parent_col =
        ndb_parent_table->getColumn(parent_col_num);
    assert(ndb_parent_col != nullptr);
    fk_col_def->referenced_column_name(ndb_parent_col->getName());
  }

  // Set match option. Unused for NDB
  fk_def->set_match_option(dd::Foreign_key::OPTION_NONE);

  // Set update rule
  switch (ndb_fk.getOnUpdateAction()) {
    case NdbDictionary::ForeignKey::NoAction:
      fk_def->set_update_rule(dd::Foreign_key::RULE_NO_ACTION);
      break;
    case NdbDictionary::ForeignKey::Restrict:
      fk_def->set_update_rule(dd::Foreign_key::RULE_RESTRICT);
      break;
    case NdbDictionary::ForeignKey::Cascade:
      fk_def->set_update_rule(dd::Foreign_key::RULE_CASCADE);
      break;
    case NdbDictionary::ForeignKey::SetNull:
      fk_def->set_update_rule(dd::Foreign_key::RULE_SET_NULL);
      break;
    case NdbDictionary::ForeignKey::SetDefault:
      fk_def->set_update_rule(dd::Foreign_key::RULE_SET_DEFAULT);
      break;
    default:
      assert(false);
      return false;
  }

  // Set delete rule
  switch (ndb_fk.getOnDeleteAction()) {
    case NdbDictionary::ForeignKey::NoAction:
      fk_def->set_delete_rule(dd::Foreign_key::RULE_NO_ACTION);
      break;
    case NdbDictionary::ForeignKey::Restrict:
      fk_def->set_delete_rule(dd::Foreign_key::RULE_RESTRICT);
      break;
    case NdbDictionary::ForeignKey::Cascade:
      fk_def->set_delete_rule(dd::Foreign_key::RULE_CASCADE);
      break;
    case NdbDictionary::ForeignKey::SetNull:
      fk_def->set_delete_rule(dd::Foreign_key::RULE_SET_NULL);
      break;
    case NdbDictionary::ForeignKey::SetDefault:
      fk_def->set_delete_rule(dd::Foreign_key::RULE_SET_DEFAULT);
      break;
    default:
      assert(false);
      return false;
  }

  return true;
}
