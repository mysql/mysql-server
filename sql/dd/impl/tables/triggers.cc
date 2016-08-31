/* Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.

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

#include "dd/impl/tables/triggers.h"

#include <memory>
#include <new>

#include "dd/impl/object_key.h"
#include "dd/impl/raw/object_keys.h"   // dd::Global_name_key
#include "dd/impl/raw/raw_record.h"    // dd::Raw_record
#include "dd/impl/raw/raw_table.h"     // dd::Raw_table
#include "dd/impl/transaction_impl.h"  // Transaction_ro
#include "dd/types/table.h"
#include "handler.h"
#include "my_dbug.h"

class THD;

namespace dd {
namespace tables {

///////////////////////////////////////////////////////////////////////////

/* purecov: begin deadcode */
Object_key *Triggers::create_key_by_schema_id(Object_id schema_id)
{
  const int INDEX_NO= 1;
  return new (std::nothrow) Parent_id_range_key(
                              INDEX_NO, FIELD_SCHEMA_ID, schema_id);
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

Object_key *Triggers::create_key_by_table_id(Object_id table_id)
{
  const int INDEX_NO= 2;
  return new (std::nothrow) Parent_id_range_key(
                              INDEX_NO, FIELD_TABLE_ID, table_id);
}

///////////////////////////////////////////////////////////////////////////

Object_key *Triggers::create_key_by_trigger_name(Object_id schema_id,
                                                 const char *trigger_name)
{
  return new (std::nothrow) Item_name_key(FIELD_SCHEMA_ID, schema_id,
                                          FIELD_NAME, trigger_name);
}

///////////////////////////////////////////////////////////////////////////

Object_id Triggers::read_table_id(const Raw_record &r)
{
  return r.read_uint(FIELD_TABLE_ID, -1);
}

///////////////////////////////////////////////////////////////////////////

bool Triggers::get_trigger_table_id(THD *thd,
                                    Object_id schema_id,
                                    const String_type &trigger_name,
                                    Object_id *oid)
{
  DBUG_ENTER("Triggers::get_trigger_table_id");

  Transaction_ro trx(thd, ISO_READ_COMMITTED);
  trx.otx.register_tables<dd::Table>();
  if (trx.otx.open_tables())
    DBUG_RETURN(true);

  DBUG_ASSERT(oid != nullptr);
  *oid= INVALID_OBJECT_ID;

  const std::unique_ptr<Object_key> key(
    create_key_by_trigger_name(schema_id, trigger_name.c_str()));

  Raw_table *table= trx.otx.get_table(table_name());
  DBUG_ASSERT(table != nullptr);

  // Find record by the object-key.
  std::unique_ptr<Raw_record> record;
  if (table->find_record(*key, record))
    DBUG_RETURN(true);

  if (record.get())
    *oid= read_table_id(*record.get());

  DBUG_RETURN(false);
}

///////////////////////////////////////////////////////////////////////////

} // namespace tables
} // namespace dd
