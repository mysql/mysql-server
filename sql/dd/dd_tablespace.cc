/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "dd_tablespace.h"

#include "sql_class.h"                        // THD
#include "transaction.h"                      // trans_commit

#include "dd/dd.h"                            // dd::create_object
#include "dd/iterator.h"                      // dd::Iterator
#include "dd/cache/dictionary_client.h"       // dd::cache::Dictionary_client
#include "dd/properties.h"                    // dd::Properties
#include "dd/types/object_type.h"             // dd::Object_type
#include "dd/types/partition.h"               // dd::Partition
#include "dd/types/table.h"                   // dd::Table
#include "dd/types/tablespace.h"              // dd::Tablespace
#include "dd/types/tablespace_file.h"         // dd::Tablespace_file

namespace dd {

bool
fill_table_and_parts_tablespace_names(THD *thd,
                                      const char *db_name,
                                      const char *table_name,
                                      Tablespace_hash_set *tablespace_set)
{
  // Get hold of the dd::Table object.
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Table *table_obj= NULL;
  if (thd->dd_client()->acquire<dd::Table>(db_name, table_name, &table_obj))
  {
    // Error is reported by the dictionary subsystem.
    return(true);
  }

  if (table_obj == NULL)
  {
    /*
      A non-existing table is a perfectly valid scenario, e.g. for
      statements using the 'IF EXISTS' clause. Thus, we cannot throw
      an error in this situation, we just continue returning succuss.
    */
    return false;
  }

  // Add the tablespace name used by dd::Table.
  const char *tablespace= NULL;
  if(!get_tablespace_name<dd::Table>(
       thd, table_obj, &tablespace, thd->mem_root))
  {
    if (tablespace &&
        tablespace_set->insert(const_cast<char*>(tablespace)))
      return true;
  }

  /*
    Add tablespaces used by partition/subpartition definitions
    Note that dd::Table::partitions() gives use both partitions
    and sub-partitions.
   */
  if (table_obj->partition_type() != dd::Table::PT_NONE)
  {
    // Iterate through tablespace names used by partition.
    std::unique_ptr<dd::Partition_const_iterator>
      part_it(table_obj->partitions());
    const dd::Partition *part_obj;
    std::string ts_name;
    while ((part_obj= part_it->next()) != NULL)
    {
      const char *tablespace= NULL;
      if(!get_tablespace_name<dd::Partition>(
           thd, part_obj, &tablespace, thd->mem_root))
      {
        if (tablespace &&
            tablespace_set->insert(const_cast<char*>(tablespace)))
          return true;
      }

    }
  }

  return false;
}


template <typename T>
bool get_tablespace_name(THD *thd, const T *obj,
                         const char **tablespace_name,
                         MEM_ROOT *mem_root)
{
  //
  // Read Tablespace
  //
  std::string name;

  if (obj->tablespace_id() != dd::INVALID_OBJECT_ID)
  {
    /*
      We get here, when we have InnoDB or NDB table in a tablespace
      which is not one of special 'innodb_%' tablespaces.

      We cannot take MDL lock as we don't know the tablespace name.
      Without a MDL lock we cannot acquire a object placing it in DD
      cache. So we are acquiring the object uncached.

      Note that in theory the fact that we are opening a table in
      some tablespace means that this tablespace can't be dropped
      or created concurrently, so in theory we hold implicit IS
      lock on tablespace (similarly to how it happens for schemas).
    */
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
    const dd::Tablespace* tablespace= NULL;
    if (thd->dd_client()->acquire_uncached<dd::Tablespace>(
                            obj->tablespace_id(), &tablespace))
    {
      // acquire() always fails with a error being reported.
      return true;
    }

    // Report error if tablespace not found.
    if (!tablespace)
    {
      my_error(ER_INVALID_DD_OBJECT_ID, MYF(0), obj->tablespace_id());
      return true;
    }

    name= tablespace->name();
    delete tablespace;
  }
  else
  {
    /*
      If user has specified special tablespace name like 'innodb_%'
      then we read it from tablespace options.
    */
    const dd::Properties *table_options= &obj->options();
    table_options->get("tablespace", name);
  }

  *tablespace_name= NULL;
  if (!name.empty() && !(*tablespace_name= strmake_root(mem_root,
                                             name.c_str(),
                                             name.length() + 1)))
  {
    return true;
  }

  return false;
}


bool create_tablespace(THD *thd, st_alter_tablespace *ts_info,
                       handlerton *hton)
{
  DBUG_ENTER("dd_create_tablespace");

  // Check if same tablespace already exists.
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Tablespace* ts= NULL;
  if (thd->dd_client()->acquire<dd::Tablespace>(
                          ts_info->tablespace_name, &ts))
  {
    // Error is reported by the dictionary subsystem.
    DBUG_RETURN(true);
  }
  if (ts)
  {
    my_error(ER_TABLESPACE_EXISTS, MYF(0), ts_info->tablespace_name);
    DBUG_RETURN(true);
  }

  // Create new tablespace.
  std::unique_ptr<dd::Tablespace> tablespace(dd::create_object<dd::Tablespace>());

  // Set tablespace name
  tablespace->set_name(ts_info->tablespace_name);

  // Engine type
  tablespace->set_engine(ha_resolve_storage_engine_name(hton));

  // Comment
  if (ts_info->ts_comment)
    tablespace->set_comment(ts_info->ts_comment);

  // Add datafile
  dd::Tablespace_file *tsf_obj= tablespace->add_file();
  tsf_obj->set_filename(ts_info->data_file_name);

  Disable_gtid_state_update_guard disabler(thd);

  // Write changes to dictionary.
  if (thd->dd_client()->store(tablespace.get()))
  {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    DBUG_RETURN(true);
  }

  DBUG_RETURN(trans_commit_stmt(thd) ||
              trans_commit(thd));
}


bool drop_tablespace(THD *thd, st_alter_tablespace *ts_info,
                     handlerton *hton)
{
  DBUG_ENTER("dd_drop_tablespace");

  // Acquire tablespace.
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Tablespace* tablespace= NULL;
  if (thd->dd_client()->acquire<dd::Tablespace>(
                          ts_info->tablespace_name, &tablespace))
  {
    // Error is reported by the dictionary subsystem.
    DBUG_RETURN(true);
  }
  if (!tablespace)
  {
    my_error(ER_TABLESPACE_MISSING_WITH_NAME, MYF(0), ts_info->tablespace_name);
    DBUG_RETURN(true);
  }

  Disable_gtid_state_update_guard disabler(thd);

  // Drop tablespace
  if (thd->dd_client()->drop(
             const_cast<dd::Tablespace*>(tablespace)))
  {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    DBUG_RETURN(true);
  }

  DBUG_RETURN(trans_commit_stmt(thd) ||
              trans_commit(thd));
}

// ALTER TABLESPACE is only supported by NDB for now.
/* purecov: begin deadcode */
bool alter_tablespace(THD *thd, st_alter_tablespace *ts_info,
                      handlerton *hton)
{
  DBUG_ENTER("dd_alter_tablespace");

  // Acquire tablespace.
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Tablespace* tablespace= NULL;
  if (thd->dd_client()->acquire<dd::Tablespace>(
                          ts_info->tablespace_name, &tablespace))
  {
    // Error is reported by the dictionary subsystem.
    DBUG_RETURN(true);
  }
  if (!tablespace)
  {
    my_error(ER_TABLESPACE_MISSING_WITH_NAME, MYF(0), ts_info->tablespace_name);
    DBUG_RETURN(true);
  }

  switch (ts_info->ts_alter_tablespace_type)
  {

  // Add data file into a tablespace.
  case ALTER_TABLESPACE_ADD_FILE:
  {
    dd::Tablespace_file *tsf_obj=
      const_cast<dd::Tablespace*>(tablespace)->add_file();

    tsf_obj->set_filename(ts_info->data_file_name);
    break;
  }

  // Drop data file from a tablespace.
  case ALTER_TABLESPACE_DROP_FILE:
    if (const_cast<dd::Tablespace*>(tablespace)->remove_file(
                                                   ts_info->data_file_name))
    {
      my_error(ER_WRONG_FILE_NAME, MYF(0), ts_info->data_file_name, 0, "");
      DBUG_RETURN(true);
    }
    break;

  default:
    my_error(ER_UNKNOWN_ERROR, MYF(0));
    DBUG_RETURN(true);
  }

  Disable_gtid_state_update_guard disabler(thd);

  // Write changes to dictionary.
  if (thd->dd_client()->store(
             const_cast<dd::Tablespace*>(tablespace)))
  {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    DBUG_RETURN(true);
  }

  DBUG_RETURN(trans_commit_stmt(thd) ||
              trans_commit(thd));
}
/* purecov: end */

} // namespace dd
