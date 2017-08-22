/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


#include "dd/cache/dictionary_client.h"  // dd::Dictionary_client
#include "dd/dd_tablespace.h"            // dd::get_tablespace_name
#include "dd/impl/sdi.h"                 // dd::serialize
#include "dd/impl/sdi_utils.h"           // sdi_utils::checked_return
#include "dd/properties.h"               // dd::Properties
#include "dd/string_type.h"              // dd::String_type
#include "dd/types/schema.h"             // dd::Schema
#include "dd/types/table.h"              // dd::Table
#include "dd/types/tablespace.h"         // dd::Tablespace
#include "handler.h"
#include "mdl.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "sql_class.h"                   // THD

/**
  @file
  @ingroup sdi

  Storage and retrieval of SDIs to/form tablespaces. This allows SDIs
  to be stored transactionally.
*/

using namespace dd::sdi_utils;

namespace {
bool is_valid(const dd::Tablespace *ts MY_ATTRIBUTE((unused)))
{
  // return ts && ts->se_private_data().exists("id");
  // TODO: WL#9538  Remove this when SDI is enabled for InnoDB
  return false;
}

bool lock_tablespace(THD *thd, const dd::Table *table)
{
  // Need to the tablespace name in order to obtain MDL.
  const char *tablespace_name= nullptr;
  if (dd::get_tablespace_name(thd, table, &tablespace_name, thd->mem_root))
  {
    checked_return(true);
  }

  if (tablespace_name == nullptr)
  {
    return false; // Ok as not all tables have tablespace objects in
    // the dd yet. See wl#7141
  }

  // MDL is needed to acquire object into the dd cache,
  return checked_return
    (mdl_lock(thd, MDL_key::TABLESPACE, "", tablespace_name,
                  MDL_INTENTION_EXCLUSIVE));
}

bool acquire_tablespace(THD *thd, const dd::Table *table,
                        const dd::Tablespace **tablespace)
{
  if (table->tablespace_id() == dd::INVALID_OBJECT_ID)
  {
    *tablespace= nullptr;
    return false;
  }

  if (lock_tablespace(thd, table))
  {
    return checked_return(true);
  }

  if (thd->dd_client()->acquire(table->tablespace_id(), tablespace))
  {
    checked_return(true);
  }
  return false;
}


// FIXME: The follwing enum and utility functions are only needed in
// this translation unit. But maybe it would be better to move them
// together with the sdi_key_t definition in tablespace.h?

enum struct Sdi_type : uint32
{
  SCHEMA,
    TABLE,
    TABLESPACE,
};

dd::sdi_key_t get_sdi_key(const dd::Schema &schema)
{
  return dd::sdi_key_t {schema.id(), static_cast<uint32>(Sdi_type::SCHEMA)};
}

dd::sdi_key_t get_sdi_key(const dd::Table &table)
{
  return dd::sdi_key_t {table.id(), static_cast<uint32>(Sdi_type::TABLE)};
}

dd::sdi_key_t get_sdi_key(const dd::Tablespace &tablespace)
{
  return dd::sdi_key_t {tablespace.id(), static_cast<uint32>(Sdi_type::TABLESPACE)};
}

bool operator==(const dd::sdi_key_t &a, const dd::sdi_key_t &b)
{
  return a.id == b.id && a.type == b.type;
}


}

namespace dd {
namespace sdi_tablespace {
bool store(THD *thd, handlerton *hton, const MYSQL_LEX_CSTRING &sdi,
           const Schema *schema, const Table *table)
{
  dd::cache::Dictionary_client::Auto_releaser scope_releaser(thd->dd_client());
  const Tablespace *tablespace= nullptr;
  if (acquire_tablespace(thd, table, &tablespace))
  {
    return checked_return(true);
  }
  // FIXME: Cannot call sdi hton api until the se_private_data
  // contains an id key - will assert. Needs wl#7141
  if (!is_valid(tablespace))
  {
    return false;
  }
  const dd::sdi_key_t schema_key= get_sdi_key(*schema);
  return checked_return(hton->sdi_set(*tablespace, &schema_key,
                                      sdi.str, sdi.length)) ||
    checked_return(hton->sdi_flush(*tablespace));
}

bool store(THD *thd, handlerton *hton, const MYSQL_LEX_CSTRING &sdi,
           const dd::Table *table, const dd::Schema *schema)
{
  if (table->se_private_id() == INVALID_OBJECT_ID)
  {
    // OK this is a preliminary store of the object - before SE has
    // added SE-specific data. Cannot, and should not, store sdi at
    // this point. TODO: Push this check down to SE.
    return false;
  }

  dd::cache::Dictionary_client::Auto_releaser scope_releaser(thd->dd_client());

  // FIXME: Iterate across dictionary to see if this is the first
  // store of a table in this schema in this tablespace?
  const dd::Tablespace *tablespace= nullptr;
  if (acquire_tablespace(thd, table, &tablespace))
  {
    checked_return(true);
  }
  // FIXME: Cannot call sdi hton api until the se_private_data
  // contains an id key - will assert. Needs wl#7141
  if (!is_valid(tablespace))
  {
    return false;
  }

  dd::sdi_vector_t sdikeys;
  if (hton->sdi_get_keys(*tablespace, sdikeys, 0/*FIXME: is this correct? */))
  {
    return false;
  }

  const dd::sdi_key_t schema_key= get_sdi_key(*schema);
  bool schema_sdi_stored= false;
  for (const auto &k : sdikeys.m_vec)
  {
    if (k == schema_key)
    {
      schema_sdi_stored= true;
      break;
    }
  }

  if (!schema_sdi_stored)
  {
    dd::sdi_t schema_sdi= dd::serialize(*schema);
    if (hton->sdi_set(*tablespace, &schema_key, schema_sdi.c_str(),
                     schema_sdi.size()))
    {
      return true;
    }
  }

  const dd::sdi_key_t table_key= get_sdi_key(*table);
  if (hton->sdi_set(*tablespace, &table_key, sdi.str,
                    sdi.length))
  {
    return true;
  }

  // FIXME: For update - need to delete the SDI with the old key?
  return checked_return(hton->sdi_flush(*tablespace)); // FIXME: copy_num=0
}


bool store(handlerton *hton, const MYSQL_LEX_CSTRING &sdi,
           const Tablespace *tablespace)
{
  if (!tablespace->se_private_data().exists("id"))
  {
    return false; // FIXME - needs wl#7141
  }
  const dd::sdi_key_t key= get_sdi_key(*tablespace);
  if (hton->sdi_set(*tablespace, &key, sdi.str, sdi.length))
  {
    return true;
  }
  // FIXME: Delete SDI with old key on update?
  return checked_return(hton->sdi_flush(*tablespace)); // FIXME: copy_num=0
}


bool remove(THD *thd, handlerton *hton, const Schema *schema,
            const Table *table)
{
  dd::cache::Dictionary_client::Auto_releaser scope_releaser(thd->dd_client());
  const Tablespace *tablespace= nullptr;
  if (acquire_tablespace(thd, table, &tablespace))
  {
    return checked_return(true);
  }
  // FIXME: Cannot call sdi hton api until the se_private_data
  // contains an id key - will assert. Needs wl#7141
  if (!is_valid(tablespace))
  {
    return false;
  }
  const dd::sdi_key_t schema_key= get_sdi_key(*schema);
  return checked_return(hton->sdi_delete(*tablespace, &schema_key)) ||
    checked_return(hton->sdi_flush(*tablespace));
}



bool remove(THD *thd, handlerton *hton, const Table *table,
            const Schema *schema)
{
  // Find the number of tables in schema s which belong to the same
  // tablespace as table t. Note that we assume that both t and s
  // have a suitable MDL. It should then not be necessary to
  // acquire MDL for each schema component since they cannot be
  // concurrently modified in a way that affects the reference count.

// Commented out for now as it creates additional commits
//     std::unique_ptr<dd::Iterator<const dd::Abstract_table> > iter;
//     if (dc.fetch_schema_components(s, &iter))
//     {
//       return true;
//     }

  int schema_refs_in_ts= 0;
//     const Object_id ts_id= table->tablespace_id();
//     const dd::Abstract_table *at;
//     while ((at= iter->next()) != nullptr)
//     {
//       const Table *tbl= dynamic_cast<const Table*>(at);
//       if (!tbl)
//       {
//         continue;
//       }

//       if (table->tablespace_id() == ts_id)
//       {
//         ++schema_refs_in_ts;
//       }
//     }

  // Acquire the tablespace object in order to access the SDIs in it.
  dd::cache::Dictionary_client::Auto_releaser scope_releaser(thd->dd_client());
  const dd::Tablespace *tablespace= nullptr;
  if (acquire_tablespace(thd, table, &tablespace))
  {
    return checked_return(true);
  }
  // FIXME: Cannot call sdi hton api until the se_private_data
  // contains an id key - will assert. Needs wl#7141
  if (!is_valid(tablespace))
  {
    return false;
  }

  // If t is the last table in the tablespace which belongs to schema s,
  // s' SDI must also be deleted from the tablespace.
  if (schema_refs_in_ts == 1)
  {
    sdi_key_t schema_sdi_key= get_sdi_key(*schema);
    if (hton->sdi_delete(*tablespace, &schema_sdi_key))
    {
      return true;
    }
  }

  // Finally t's SDI can be deleted from the tablespace.
  sdi_key_t sdi_key= get_sdi_key(*table);
  if (hton->sdi_delete(*tablespace, &sdi_key))
  {
    return true;
  }
  return checked_return(hton->sdi_flush(*tablespace)); // FIXME: copy_num=0
}

bool remove(handlerton *hton, const Tablespace *tablespace)
{
  DBUG_ASSERT(hton->db_type == DB_TYPE_INNODB);
  DBUG_ASSERT(hton->sdi_set != nullptr);
  if (!tablespace->se_private_data().exists("id"))
  {
    return false; // FIXME; Needs wl#7141
  }
  const dd::sdi_key_t key= get_sdi_key(*tablespace);
  if (hton->sdi_delete(*tablespace, &key))
  {
    return true;
  }
  return checked_return(hton->sdi_flush(*tablespace)); // FIXME: copy_num=0
}
}
}
