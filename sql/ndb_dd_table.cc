/*
   Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

// Implements the functions declared in ndb_dd_table.h
#include "sql/ndb_dd_table.h"

#include <string>

#include "sql/dd/impl/types/partition_impl.h"
#include "sql/dd/properties.h"
#include "sql/dd/types/column.h"
#include "sql/dd/types/partition.h"
#include "sql/dd/types/table.h"

// The key used to store the NDB tables object version in the
// se_private_data field of DD
static const char* object_version_key = "object_version";

void
ndb_dd_table_set_object_id_and_version(dd::Table* table_def,
                                       int object_id, int object_version)
{
  DBUG_ENTER("ndb_dd_table_set_object_id_and_version");
  DBUG_PRINT("enter", ("object_id: %d, object_version: %d",
                       object_id, object_version));

  table_def->set_se_private_id(object_id);
  table_def->se_private_data().set_int32(object_version_key,
                                         object_version);
  DBUG_VOID_RETURN;
}


bool
ndb_dd_table_get_object_id_and_version(const dd::Table* table_def,
                                       int& object_id, int& object_version)
{
  DBUG_ENTER("ndb_dd_table_get_object_id_and_version");

  if (table_def->se_private_id() == dd::INVALID_OBJECT_ID)
  {
    DBUG_PRINT("error", ("Table definition contained an invalid object id"));
    DBUG_RETURN(false);
  }
  object_id = table_def->se_private_id();

  if (!table_def->se_private_data().exists(object_version_key))
  {
    DBUG_PRINT("error", ("Table definition didn't contain property '%s'",
                         object_version_key));
    DBUG_RETURN(false);
  }

  if (table_def->se_private_data().get_int32(object_version_key,
                                             &object_version))
  {
    DBUG_PRINT("error", ("Table definition didn't have a valid number for '%s'",
                         object_version_key));
    DBUG_RETURN(false);
  }

  DBUG_PRINT("exit", ("object_id: %d, object_version: %d",
                      object_id, object_version));

  DBUG_RETURN(true);
}


void
ndb_dd_table_mark_as_hidden(dd::Table* table_def)
{
  DBUG_ENTER("ndb_dd_table_mark_as_hidden");
  DBUG_PRINT("enter", ("table_name: %s", table_def->name().c_str()));

  // Only allow mysql.ndb_schema table to be hidden for now, there are a
  // few hacks elsewehere in these ndb_dd_* files and those need to be
  // hacked to keep the table hidden
  DBUG_ASSERT(table_def->name() == "ndb_schema");

  // Mark it as hidden by SE. I.e "Table which is implicitly
  // created and dropped by SE"
  table_def->set_hidden(dd::Abstract_table::HT_HIDDEN_SE);

  DBUG_VOID_RETURN;
}


dd::String_type ndb_dd_table_get_engine(const dd::Table* table_def)
{
  return table_def->engine();
}

size_t ndb_dd_table_get_num_columns(const dd::Table* table_def)
{
  const dd::Abstract_table::Column_collection& cols = table_def->columns();
  return cols.size();
}

bool ndb_dd_table_is_using_fixed_row_format(const dd::Table* table_def)
{
  return table_def->row_format() == dd::Table::RF_FIXED;
}

dd::String_type ndb_dd_table_get_name(const dd::Table* table_def)
{
  return table_def->name();
}

void
ndb_dd_table_set_row_format(dd::Table* table_def,
                            const bool force_var_part)
{
  if (force_var_part == false)
  {
    table_def->set_row_format(dd::Table::RF_FIXED);
  }
  else
  {
    table_def->set_row_format(dd::Table::RF_DYNAMIC);
  }
}

bool ndb_dd_table_check_partition_count(const dd::Table* table_def,
                                        size_t ndb_num_partitions)
{
  return table_def->partitions().size() == ndb_num_partitions;
}

void ndb_dd_table_fix_partition_count(dd::Table* table_def,
                                      size_t ndb_num_partitions)
{

  DBUG_ENTER("ndb_dd_table_fix_partition_count");
  DBUG_PRINT("enter", ("ndb_num_partitions: %zu", ndb_num_partitions));

  const size_t dd_num_partitions = table_def->partitions()->size();

  if (ndb_num_partitions < dd_num_partitions)
  {
    // Remove extra partitions from DD

    dd::Collection<dd::Partition* >* dd_partitions = table_def->partitions();

    // Check if the extra partitions have been stored in the DD
    // Checking only one of the partitions is sufficient
    const bool partition_object_stored_in_DD =
      dd_partitions->at(ndb_num_partitions)->is_persistent();

    for (size_t i = ndb_num_partitions; i < dd_num_partitions; i++)
    {
      auto partition = dd_partitions->at(ndb_num_partitions);
      dd_partitions->remove(dynamic_cast<dd::Partition_impl *>(partition));
    }

    if (!partition_object_stored_in_DD)
    {
      // This case has to handled differently. When the partitions
      // are removed from the collection above, they are dropped
      // from the DD later. In case the partitions have not
      // been stored in the DD at this point, we can simply
      // clear the removed partitions. If we fail to do so, there'll
      // be a crash when the table definition is stored in the DD.
      // This path is hit for ALTER TABLE as well as when the table
      // is "discovered" from NDB Dictionary and installed into the
      // DD
      dd_partitions->clear_removed_items();
    }
  }
  else if (dd_num_partitions < ndb_num_partitions)
  {
    // Add missing partitions to DD
    for (size_t i = dd_num_partitions; i < ndb_num_partitions; i++)
    {
      dd::Partition *partition_def = table_def->add_partition();
      const std::string partition_name = "p" + std::to_string(i);
      partition_def->set_name(partition_name.c_str());
      partition_def->set_engine(table_def->engine());
      partition_def->set_number(i);
    }
  }

  DBUG_ASSERT(ndb_num_partitions == table_def->partitions()->size());
  DBUG_VOID_RETURN;
}
