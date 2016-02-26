/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "dd/impl/tables/tables.h"

#include "dd/dd.h"                         // dd::create_object
#include "dd/impl/transaction_impl.h"      // dd::Open_dictionary_tables_ctx
#include "dd/impl/raw/object_keys.h"       // dd::Item_name_key
#include "dd/impl/raw/raw_record.h"        // dd::Raw_record
#include "dd/impl/raw/raw_table.h"         // dd::Raw_table
#include "dd/types/view.h"                 // dd::View

namespace dd {
namespace tables {

///////////////////////////////////////////////////////////////////////////

Dictionary_object *Tables::create_dictionary_object(
  const Raw_record &r) const
{
  enum_table_type table_type=
    static_cast<enum_table_type>(r.read_int(FIELD_TYPE));

  if (table_type == enum_table_type::BASE_TABLE)
    return dd::create_object<Table>();
  else
    return dd::create_object<View>();
}

///////////////////////////////////////////////////////////////////////////

bool Tables::update_object_key(Item_name_key *key,
                               Object_id schema_id,
                               const std::string &table_name)
{
  char buf[NAME_LEN + 1];
  key->update(FIELD_SCHEMA_ID, schema_id, FIELD_NAME,
              Object_table_definition_impl::fs_name_case(table_name, buf));
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Tables::update_aux_key(Se_private_id_key *key,
                            const std::string &engine,
                            ulonglong se_private_id)
{
  const int SE_PRIVATE_ID_INDEX_ID= 2;
  key->update(SE_PRIVATE_ID_INDEX_ID,
              FIELD_ENGINE,
              engine,
              FIELD_SE_PRIVATE_ID,
              se_private_id);
  return false;
}

///////////////////////////////////////////////////////////////////////////

/* purecov: begin deadcode */
Object_key *Tables::create_se_private_key(
  const std::string &engine,
  ulonglong se_private_id)
{
  const int SE_PRIVATE_ID_INDEX_ID= 2;

  return
    new (std::nothrow) Se_private_id_key(
      SE_PRIVATE_ID_INDEX_ID,
      FIELD_ENGINE,
      engine,
      FIELD_SE_PRIVATE_ID,
      se_private_id);
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

Object_key *Tables::create_key_by_schema_id(
  Object_id schema_id)
{
  return new (std::nothrow) Parent_id_range_key(1, FIELD_SCHEMA_ID, schema_id);
}

///////////////////////////////////////////////////////////////////////////

ulonglong Tables::read_se_private_id(const Raw_record &r)
{
  return r.read_uint(Tables::FIELD_SE_PRIVATE_ID, -1);
}

///////////////////////////////////////////////////////////////////////////

/**
  @brief
  Retrieve max se_private_id for a given engine name
  stored in mysql.tables DD tables.

  @param       otx     The context for opening the DD tables.
  @param       engine  The engine name within which we get max se_private_id.
  @param [out] max_id  The resulting max id found.
*/
/* purecov: begin deadcode */
bool Tables::max_se_private_id(Open_dictionary_tables_ctx *otx,
                               const std::string &engine,
                               ulonglong *max_id)
{
  std::unique_ptr<Object_key> key(
    create_se_private_key(engine, INVALID_OBJECT_ID));

  Raw_table *t= otx->get_table(table_name());
  DBUG_ASSERT(t);

  // Find record by the object-key.
  *max_id= 0;
  std::unique_ptr<Raw_record> r;
  if (t->find_last_record(*key, r))
    return true;

  if (r.get())
    *max_id= read_se_private_id(*r.get());

  return false;
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

}
}
