/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <algorithm>

#include "my_dbug.h"                     // DBUG_PRINT
#include "my_inttypes.h"                 // uint32
#include "sql/dd/cache/dictionary_client.h" // dd::Dictionary_client
#include "sql/dd/collection.h"           // dd::Collection
#include "sql/dd/dd_tablespace.h"        // dd::get_tablespace_name
#include "sql/dd/impl/sdi.h"             // dd::serialize
#include "sql/dd/impl/sdi_utils.h"       // sdi_utils::checked_return
#include "sql/dd/properties.h"           // dd::Properties
#include "sql/dd/string_type.h"          // dd::String_type
#include "sql/dd/types/index.h"          // dd::Index
#include "sql/dd/types/partition.h"      // dd::Partition
#include "sql/dd/types/partition_index.h" // dd::Partition_index
#include "sql/dd/types/schema.h"         // dd::Schema
#include "sql/dd/types/table.h"          // dd::Table
#include "sql/dd/types/tablespace.h"     // dd::Tablespace
#include "sql/handler.h"                 // sdi_set
#include "sql/mdl.h"                     // MDL_key::TABLESPACE
#include "sql/sql_class.h"               // THD


/**
  @file
  @ingroup sdi

  Storage and retrieval of SDIs to/form tablespaces. This allows SDIs
  to be stored transactionally.
*/

using namespace dd::sdi_utils;

namespace {

using DC= dd::cache::Dictionary_client;

// Workaround since we don't get generic lambdas until C++14
struct Apply_to_tsid
{
  template <typename T, typename CLOSURE_TYPE>
  bool operator()(const T &t, CLOSURE_TYPE &&clos)
  {
    return clos(t.tablespace_id());
  }
};

template <typename FTOR_TYPE, typename CLOSURE_TYPE>
bool apply_to_table_graph(const dd::Table &table,
                          FTOR_TYPE &&ftor, CLOSURE_TYPE &&clos)
{
  if (ftor(table, std::forward<CLOSURE_TYPE>(clos)))
  {
    return true;
  }

  for (auto &ix : table.indexes())
  {
    if (ftor(*ix, std::forward<CLOSURE_TYPE>(clos)))
    {
      return true;
    }
  }

  if (table.partitions().empty())
  {
    return false;
  }
  const dd::Partition *part= table.partitions().front();

  if (ftor(*part, std::forward<CLOSURE_TYPE>(clos)))
  {
    return true;
  }

  for (auto &part_ix : part->indexes())
  {
    if (ftor(*part_ix, std::forward<CLOSURE_TYPE>(clos)))
    {
      return true;
    }
  }
  return false;
}


template <class CLOSURE_TYPE>
bool apply_to_tablespaces(THD *thd, const dd::Table &table,
                          CLOSURE_TYPE &&clos)
{
  DC::Auto_releaser tblspcrel(thd->dd_client());
  std::vector<dd::Object_id> tsids;
  return apply_to_table_graph
    (table, Apply_to_tsid{}, [&] (dd::Object_id tsid)
     {
       if (tsid == dd::INVALID_OBJECT_ID)
       {
         return false;
       }
       if (std::find(tsids.begin(), tsids.end(), tsid) != tsids.end())
       {
         // already saved sdi in this tablespace
         return false;
       }
       tsids.push_back(tsid);

       // The tablespace object may not have MDL
       // Need to use acquire_uncached_uncommitted to get name for MDL
       dd::Tablespace *tblspc_= nullptr;
       if (thd->dd_client()->acquire_uncached_uncommitted(tsid, &tblspc_))
       {
         return true;
       }
       if (tblspc_ == nullptr)
       {
         // When dropping a table in an implicit tablespace, the
         // refrenced tablespace may already have been removed. This
         // is ok since this means that the sdis in the tablespace
         // have been removed also. Note that since tsids is only used
         // to check for duplicates, it makes sense to leave tsid
         // there and return false so that we can proceed.
         return false;
       }

       if (mdl_lock(thd, MDL_key::TABLESPACE, "", tblspc_->name(),
                    MDL_INTENTION_EXCLUSIVE))
       {
         return true;
       }

       // Re-acquire after getting lock to make sure it is still there...
       const dd::Tablespace *tblspc= nullptr;
       if (thd->dd_client()->acquire(tsid, &tblspc))
       {
         return true;
       }
       DBUG_ASSERT(tblspc != nullptr);

       return clos(*tblspc);
     });
}


// The follwing enum and utility functions are only needed in
// this translation unit. But maybe it would be better to move them
// together with the sdi_key_t definition in tablespace.h?

enum struct Sdi_type : uint32
{
  SCHEMA,
  TABLE,
  TABLESPACE,
};


dd::sdi_key_t get_sdi_key(const dd::Table &table)
{
  return dd::sdi_key_t {static_cast<uint32>(Sdi_type::TABLE), table.id()};
}

dd::sdi_key_t get_sdi_key(const dd::Tablespace &tablespace)
{
  return dd::sdi_key_t {static_cast<uint32>(Sdi_type::TABLESPACE),
      tablespace.id()};
}

} // namespace

namespace dd {
namespace sdi_tablespace {
bool store_tbl_sdi(THD *thd, const handlerton &hton,
                   const dd::Sdi_type &sdi, const dd::Table &table,
                   const dd::Schema &schema MY_ATTRIBUTE((unused)))
{
  const dd::sdi_key_t key= get_sdi_key(table);

  auto store_sdi= [&] (const dd::Tablespace &tblspc) -> bool
  {
    DBUG_PRINT("ddsdi",("store_sdi_with_schema[](Schema" ENTITY_FMT
                        ", Table" ENTITY_FMT ")",
                        ENTITY_VAL(schema), ENTITY_VAL(table)));
    if (hton.sdi_set(tblspc, &table, &key, sdi.c_str(), sdi.size()))
    {
      return checked_return(true);
    }

    return false;
  };

  return apply_to_tablespaces(thd, table, store_sdi);
}


bool store_tsp_sdi(const handlerton &hton, const Sdi_type &sdi,
                   const Tablespace &tblspc)
{
  dd::sdi_key_t key= get_sdi_key(tblspc);

  DBUG_PRINT("ddsdi",("store_tsp_sdi(" ENTITY_FMT ")", ENTITY_VAL(tblspc)));
  if (hton.sdi_set(tblspc, nullptr, &key, sdi.c_str(), sdi.size()))
  {
    return checked_return(true);
  }
  return false;
}


bool drop_tbl_sdi(THD *thd, const handlerton &hton,
                  const Table &table,
                  const Schema &schema MY_ATTRIBUTE((unused)))
{
  DBUG_PRINT("ddsdi",("store_tbl_sdi(Schema" ENTITY_FMT
                      ", Table" ENTITY_FMT ")",
                      ENTITY_VAL(schema), ENTITY_VAL(table)));

  sdi_key_t sdi_key= get_sdi_key(table);
  return apply_to_tablespaces
    (thd, table,
      [&] (const dd::Tablespace &tblspc)
      {
        if (hton.sdi_delete(tblspc, &table, &sdi_key))
        {
          return checked_return(true);
        }
        return false;
      });
}
} // namespace sdi_tablespace
} // namespace dd
