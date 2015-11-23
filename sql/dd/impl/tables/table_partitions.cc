/* Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

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

#include "dd/impl/tables/table_partitions.h"

#include "dd/impl/transaction_impl.h"    // dd::Transaction_ro
#include "dd/impl/raw/object_keys.h"     // dd::Parent_id_range_key
#include "dd/impl/raw/raw_record.h"      // dd::Raw_record
#include "dd/impl/raw/raw_table.h"       // dd::Raw_table

namespace dd {
namespace tables {

///////////////////////////////////////////////////////////////////////////

Object_key *Table_partitions::create_key_by_table_id(Object_id table_id)
{
  return new (std::nothrow) Parent_id_range_key(1, FIELD_TABLE_ID, table_id);
}

///////////////////////////////////////////////////////////////////////////

/* purecov: begin deadcode */
Object_id Table_partitions::read_table_id(const Raw_record &r)
{
  return r.read_uint(Table_partitions::FIELD_TABLE_ID, -1);
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

/* purecov: begin deadcode */
Object_key *Table_partitions::create_se_private_key(
  const std::string &engine,
  ulonglong se_private_id)
{
  const int SE_PRIVATE_ID_INDEX_ID= 3;
  const int ENGINE_COLUMN_NO=5;
  const int SE_PRIVATE_ID_COLUMN_NO= 9;

  return
    new (std::nothrow) Se_private_id_key(
      SE_PRIVATE_ID_INDEX_ID,
      ENGINE_COLUMN_NO,
      engine,
      SE_PRIVATE_ID_COLUMN_NO,
      se_private_id);
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

/* purecov: begin deadcode */
bool Table_partitions::get_partition_table_id(
  THD *thd,
  const std::string &engine,
  ulonglong se_private_id,
  Object_id *oid)
{
  DBUG_ENTER("Table_partitions::get_partition_table_id");

  Transaction_ro trx(thd);
  trx.otx.register_tables<dd::Table>();
  if (trx.otx.open_tables())
    return true;

  DBUG_ASSERT(oid);
  *oid= INVALID_OBJECT_ID;

  const std::unique_ptr<Object_key> k(
    create_se_private_key(engine, se_private_id));

  Raw_table *t= trx.otx.get_table(table_name());
  DBUG_ASSERT(t);

  // Find record by the object-key.
  std::unique_ptr<Raw_record> r;
  if (t->find_record(*k, r))
  {
    DBUG_RETURN(true);
  }

  if (r.get())
    *oid= read_table_id(*r.get());

  DBUG_RETURN(false);
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

}
}
