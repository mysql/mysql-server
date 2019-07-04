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

// Implements
#include "sql/ndb_metadata.h"

#include <memory>
#include <string>
#include <iostream>

#include "sql/dd/dd.h"
#include "sql/dd/object_id.h"
#include "sql/dd/properties.h"
#include "sql/dd/impl/properties_impl.h"
#include "sql/dd/types/partition.h"
#include "sql/dd/types/table.h"
#include "sql/ndb_dd_client.h"
#include "sql/ndb_dd_table.h"
#include "sql/ndb_ndbapi_util.h"

#include "my_base.h" // For HA_SM_DISK and HA_SM_MEMORY, fix by bug27309072

// Key used for magic flag "explicit_tablespace" in table options
static const char* magic_key_explicit_tablespace  = "explicit_tablespace";

// Key used for flag "storage" in table options
const char* key_storage = "storage";

// Check also partitioning properties
constexpr bool check_partitioning = false; // disabled

dd::String_type
Ndb_metadata::partition_expression()
{
  dd::String_type expr;
  if (m_ndbtab->getFragmentType() == NdbDictionary::Table::HashMapPartition &&
      m_ndbtab->getDefaultNoPartitionsFlag() &&
      m_ndbtab->getFragmentCount() == 0 &&
      m_ndbtab->getLinearFlag() == false)
  {
    // Default partitioning
    return expr;
  }

  const char* separator = "";
  const int num_columns = m_ndbtab->getNoOfColumns();
  for (int i = 0; i < num_columns; i++)
  {
    const NdbDictionary::Column* column = m_ndbtab->getColumn(i);
    if (column->getPartitionKey())
    {
      expr.append(separator);
      expr.append(column->getName());
      separator = ";";
    }
  }
  return expr;
}


bool
Ndb_metadata::create_table_def(dd::Table* table_def)
{
  DBUG_ENTER("Ndb_metadata::create_table_def");

  // name
  const char* table_name = m_ndbtab->getName();
  table_def->set_name(table_name);
  DBUG_PRINT("info", ("table_name: '%s'", table_name));

  // collation_id, default collation for columns
  // Missing in NDB.
  // The collation_id is actually only interesting when adding new columns
  // without specifying collation for the new columns, the new columns will
  // then get their collation from the table. Each existing column which
  // need a collation already have the correct value set as a property
  // on the column
  //table_def->set_collation_id(some_collation_id);

  // engine
  table_def->set_engine("ndbcluster");

  // row_format
  if (m_ndbtab->getForceVarPart() == false)
  {
    table_def->set_row_format(dd::Table::RF_FIXED);
  }
  else
  {
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
  ndb_dd_table_set_object_id_and_version(table_def,
                                         m_ndbtab->getObjectId(),
                                         m_ndbtab->getObjectVersion());

  // storage
  // no DD API setters or types available -> hardcode
  {
    const NdbDictionary::Column::StorageType type =
      m_ndbtab->getStorageType();
    switch (type)
    {
      case NdbDictionary::Column::StorageTypeDisk:
        table_def->options().set(key_storage,
                                 HA_SM_DISK);
        break;
      case NdbDictionary::Column::StorageTypeMemory:
         table_def->options().set(key_storage,
                                  HA_SM_MEMORY);
         break;
      case NdbDictionary::Column::StorageTypeDefault:
        // Not set
        break;
    }
  }

  if (check_partitioning)
  {
    // partition_type
    dd::Table::enum_partition_type partition_type = dd::Table::PT_AUTO;
    switch (m_ndbtab->getFragmentType())
    {
    case NdbDictionary::Table::UserDefined:
      DBUG_PRINT("info", ("UserDefined"));
      // BY KEY
      partition_type = dd::Table::PT_KEY_55;
      break;
    case NdbDictionary::Table::HashMapPartition:
      DBUG_PRINT("info", ("HashMapPartition"));
      if (m_ndbtab->getFragmentCount() != 0)
      {
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

  DBUG_RETURN(true);
}


bool
Ndb_metadata::lookup_tablespace_id(THD* thd, dd::Table* table_def)
{
  DBUG_ENTER("Ndb_metadata::lookup_tablespace_id");

  Ndb_dd_client dd_client(thd);
  dd_client.disable_auto_rollback();

  // tablespace_id
  // The id of the tablespace in DD.

  if (!ndb_table_has_tablespace(m_ndbtab))
  {
    // No tablespace
    DBUG_RETURN(true);
  }

  // Set magic flag telling SHOW CREATE and CREATE LIKE that tablespace
  // was specified for this table
  table_def->options().set(magic_key_explicit_tablespace, true);

  // Lookup tablespace_by name if name is available
  const char* tablespace_name = ndb_table_tablespace_name(m_ndbtab);
  if (tablespace_name)
  {
    DBUG_PRINT("info", ("tablespace_name: '%s'", tablespace_name));
    dd::Object_id tablespace_id;
    if (!dd_client.lookup_tablespace_id(tablespace_name, &tablespace_id))
    {
      // Failed
      DBUG_RETURN(false);
    }

    table_def->set_tablespace_id(tablespace_id);

    DBUG_RETURN(true);
  }

  // Lookup tablespace_id by object id
  Uint32 object_id, object_version;
  if (m_ndbtab->getTablespace(&object_id, &object_version))
  {
    DBUG_PRINT("info", ("tablespace_id: %u, tablespace_version: %u",
                        object_id, object_version));

    // NOTE! Need to store the object id and version of tablespace
    // in se_private_data to be able to lookup a tablespace by object id
    m_compare_tablespace_id = false; // Skip comparing tablespace_id for now

    DBUG_RETURN(true);
  }

  // Table had tablespace but neither name or id was available -> fail
  DBUG_ASSERT(false);
  DBUG_RETURN(false);
}


bool Ndb_metadata::compare_table_def(const dd::Table* t1, const dd::Table* t2)
{
  DBUG_ENTER("Ndb_metadata::compare_table_def");

  class Compare_context {
    std::vector<std::string> diffs;
    void add_diff(const char* property, std::string a,
                  std::string b)
    {
      std::string diff;
      diff.append("Diff in '").append(property).append("' detected, '")
          .append(a).append("' != '").append(b).append("'");
      diffs.push_back(diff);
    }

  public:

    void compare(const char* property,
                 dd::String_type a, dd::String_type b)
    {
      if (a == b)
        return;
      add_diff(property, a.c_str(), b.c_str());
    }

    void compare(const char* property,
                 unsigned long long a, unsigned long long b)
    {
      if (a == b)
        return;
       add_diff(property, std::to_string(a), std::to_string(b));
    }

    bool equal(){
      if (diffs.size() == 0)
        return true;

      // Print the list of diffs
      for (std::string diff : diffs)
        std::cout << diff << std::endl;

      return false;
    }
  } ctx;


  // name
  // When using lower_case_table_names==2 the table will be
  // created using lowercase in NDB while still be original case in DD
  // this causes a slight diff here. Workaround by skip comparing the
  // name until BUG#27307793
  //ctx.compare("name", t1->name(), t2->name());

  // collation_id
  // ctx.compare("collation_id", t1->collation_id(), t2->collation_id());

  // tablespace_id (local)
  if (m_compare_tablespace_id)
  {
    // The id has been looked up from DD
    ctx.compare("tablespace_id", t1->tablespace_id(), t2->tablespace_id());
  }
  else
  {
    // It's known that table has tablespace but it could not be
    // looked up(yet), just check that DD definition have tablespace_id
    DBUG_ASSERT(t1->tablespace_id());
  }

  // Check magic flag "options.explicit_tablespace"
  {
    bool t1_explicit = false;
    bool t2_explicit= false;
    if (t1->options().exists(magic_key_explicit_tablespace))
    {
      t1->options().get(magic_key_explicit_tablespace, &t1_explicit);
    }
    if (t2->options().exists(magic_key_explicit_tablespace))
    {
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
    if (t1->options().exists(key_storage))
    {
      t1->options().get(key_storage, &t1_storage);
    }
    if (t2->options().exists(key_storage))
    {
      t2->options().get(key_storage, &t2_storage);
    }
    // There's a known bug in tables created in mysql versions <= 5.1.57 where
    // the storage type of the table was not stored in NDB Dictionary but was
    // present in the .frm. Thus, we accept that this is a known mismatch and
    // skip the comparison of this attribute for tables created using earlier
    // versions
    ulong t1_previous_mysql_version = UINT_MAX32;
    if (!ndb_dd_table_get_previous_mysql_version(t1, t1_previous_mysql_version)
        || t1_previous_mysql_version > 50157)
    {
      ctx.compare("options.storage", t1_storage, t2_storage);
    }
  }

  if (check_partitioning)
  {
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


  if (ctx.equal())
    DBUG_RETURN(true); // Tables are identical
  DBUG_RETURN(false);
}


bool Ndb_metadata::check_partition_info(const dd::Table* table_def)
{
  DBUG_ENTER("Ndb_metadata::check_partition_info");

  // Compare the partition count of the NDB table with the partition
  // count of the table definition used by the caller
  const size_t dd_num_partitions = table_def->partitions().size();
  const size_t ndb_num_partitions = m_ndbtab->getPartitionCount();
  if (ndb_num_partitions != dd_num_partitions)
  {
    std::cout << "Diff in 'partition count' detected, '"
              <<  std::to_string(ndb_num_partitions)
              << "' != '" << std::to_string(dd_num_partitions)
              << "'" << std::endl;
    DBUG_RETURN(false);
  }

  // Check if the engine of the partitions are as expected
  std::vector<std::string> diffs;
  for (size_t i = 0; i < dd_num_partitions; i++)
  {
    auto partition = table_def->partitions().at(i);
    // engine
    if (table_def->engine() != partition->engine())
    {
      std::string diff;
      diff.append("Diff in 'engine' for partition '")
          .append(partition->name().c_str()).append("' detected, '")
          .append(table_def->engine().c_str()).append("' != '")
          .append(partition->engine().c_str()).append("'");
      diffs.push_back(diff);
    }
  }

  if (diffs.size() != 0)
  {
    // Print the list of diffs
    for (std::string diff : diffs)
    {
      std::cout << diff << std::endl;
    }
    DBUG_RETURN(false);
  }

  DBUG_RETURN(true);

}


bool Ndb_metadata::compare(THD* thd,
                           const NdbDictionary::Table* m_ndbtab,
                           const dd::Table* table_def)
{
  Ndb_metadata ndb_metadata(m_ndbtab);

  // Transform NDB table to DD table def
  std::unique_ptr<dd::Table> ndb_table_def{dd::create_object<dd::Table>()};
  if (!ndb_metadata.create_table_def(ndb_table_def.get()))
  {
    DBUG_ASSERT(false);
    return false;
  }

  // Lookup tablespace id from DD
  if (!ndb_metadata.lookup_tablespace_id(thd, ndb_table_def.get()))
  {
    DBUG_ASSERT(false);
    return false;
  }

  // Compare the table definition generated from the NDB table
  // with the table definition used by caller
  if (!ndb_metadata.compare_table_def(table_def, ndb_table_def.get()))
  {
    DBUG_ASSERT(false);
    return false;
  }

  // Check the partition information of the table definition used by caller
  if (!ndb_metadata.check_partition_info(table_def))
  {
    DBUG_ASSERT(false);
    return false;
  }

  return true;
}


