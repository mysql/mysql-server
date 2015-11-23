/* Copyright (c) 2015 Oracle and/or its affiliates. All rights reserved.

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

#include "storage_adapter.h"

#include "debug_sync.h"                       // DEBUG_SYNC
#include "sql_class.h"                        // THD

#include "dd/types/abstract_table.h"          // Abstract_table
#include "dd/types/charset.h"                 // Charset
#include "dd/types/collation.h"               // Collation
#include "dd/types/schema.h"                  // Schema
#include "dd/types/table.h"                   // Table
#include "dd/types/tablespace.h"              // Tablespace
#include "dd/types/view.h"                    // View
#include "dd/types/dictionary_object.h"       // Dictionary_object
#include "dd/types/dictionary_object_table.h" // Dictionary_object_table
#include "dd/impl/transaction_impl.h"         // Transaction_ro
#include "dd/impl/raw/raw_table.h"            // Raw_table
#include "dd/impl/raw/raw_record.h"           // Raw_record
#include "dd/impl/raw/object_keys.h"          // Primary_id_key
#include "dd/impl/types/weak_object_impl.h"   // Weak_object_impl

namespace dd {
namespace cache {

#ifndef DBUG_OFF
bool Storage_adapter::s_use_fake_storage= false;
#endif

// Get a dictionary object from persistent storage.
template <typename K, typename T>
bool Storage_adapter::get(THD *thd, const K &key, const T **object)
{
#ifndef DBUG_OFF
  if (s_use_fake_storage)
    return fake_instance()->fake_get(thd, key, object);
#endif

  DBUG_ASSERT(object);
  *object= NULL;

  // Start a DD transaction to get the object.
  Transaction_ro trx(thd);
  trx.otx.register_tables<T>();

  if (trx.otx.open_tables())
  {
    DBUG_ASSERT(thd->is_error() || thd->killed);
    return true;
  }

  const Dictionary_object_table &table= T::OBJECT_TABLE();
  // Get main object table.
  Raw_table *t= trx.otx.get_table(table.name());

  // Find record by the object-id.
  std::unique_ptr<Raw_record> r;
  if (t->find_record(key, r))
  {
    DBUG_ASSERT(thd->is_error() || thd->killed);
    return true;
  }

  // Restore the object from the record.
  Dictionary_object *new_object= NULL;
  if (r.get() && table.restore_object_from_record(&trx.otx, *r.get(), &new_object))
  {
    DBUG_ASSERT(thd->is_error() || thd->killed);
    return true;
  }

  // Delete the new object if dynamic cast fails.
  if (new_object)
  {
    // Here, a failing dynamic cast is not a legitimate situation.
    // In production, we report an error.
    *object= dynamic_cast<T*>(new_object);
    if (!*object)
    {
      /* purecov: begin inspected */
      my_error(ER_INVALID_DD_OBJECT, MYF(0), new_object->name().c_str());
      delete new_object;
      DBUG_ASSERT(false);
      return true;
      /* purecov: end */
    }
  }

  return false;
}


// Drop a dictionary object from persistent storage.
template <typename T>
bool Storage_adapter::drop(THD *thd, T *object)
{
#ifndef DBUG_OFF
  if (s_use_fake_storage)
    return fake_instance()->fake_drop(thd, object);
#endif

  if (object->impl()->validate())
  {
    DBUG_ASSERT(thd->is_error() || thd->killed);
    return true;
  }

  // Drop the object from the dd tables. We need to switch transaction ctx to do this.
  Update_dictionary_tables_ctx ctx(thd);
  ctx.otx.register_tables<T>();
  DEBUG_SYNC(thd, "before_dropping_dd_object");

  if (ctx.otx.open_tables() || object->impl()->drop(&ctx.otx))
  {
    DBUG_ASSERT(thd->is_error() || thd->killed);
    return true;
  }

  return false;
}


// Store a dictionary object to persistent storage.
template <typename T>
bool Storage_adapter::store(THD *thd, T *object)
{
#ifndef DBUG_OFF
  if (s_use_fake_storage)
    return fake_instance()->fake_store(thd, object);
#endif

  if (object->impl()->validate())
  {
    DBUG_ASSERT(thd->is_error() || thd->killed);
    return true;
  }

  // Store the object into the dd tables. We need to switch transaction ctx to do this.
  Update_dictionary_tables_ctx ctx(thd);
  ctx.otx.register_tables<T>();
  DEBUG_SYNC(thd, "before_storing_dd_object");

  if (ctx.otx.open_tables() || object->impl()->store(&ctx.otx))
  {
    DBUG_ASSERT(thd->is_error() || thd->killed);
    return true;
  }

  return false;
}


// Explicitly instantiate the type for the various usages.
template bool Storage_adapter::get<Abstract_table::id_key_type,
                                Abstract_table>
       (THD *, const Abstract_table::id_key_type &, const Abstract_table **);
template bool Storage_adapter::get<Abstract_table::name_key_type,
                                Abstract_table>
       (THD *, const Abstract_table::name_key_type &, const Abstract_table **);
template bool Storage_adapter::get<Abstract_table::aux_key_type,
                                Abstract_table>
       (THD *, const Abstract_table::aux_key_type &, const Abstract_table **);
template bool Storage_adapter::drop(THD *, Abstract_table *);
template bool Storage_adapter::store(THD *, Abstract_table *);
template bool Storage_adapter::drop(THD *, Table*);
template bool Storage_adapter::store(THD *, Table*);
template bool Storage_adapter::drop(THD *, View*);
template bool Storage_adapter::store(THD *, View*);

template bool Storage_adapter::get<Charset::id_key_type, Charset>
       (THD *, const Charset::id_key_type &, const Charset **);
template bool Storage_adapter::get<Charset::name_key_type, Charset>
       (THD *, const Charset::name_key_type &, const Charset **);
template bool Storage_adapter::get<Charset::aux_key_type, Charset>
       (THD *, const Charset::aux_key_type &, const Charset **);
template bool Storage_adapter::drop(THD *, Charset*);
template bool Storage_adapter::store(THD *, Charset*);

template bool Storage_adapter::get<Collation::id_key_type, Collation>
       (THD *, const Collation::id_key_type &, const Collation **);
template bool Storage_adapter::get<Collation::name_key_type, Collation>
       (THD *, const Collation::name_key_type &, const Collation **);
template bool Storage_adapter::get<Collation::aux_key_type, Collation>
       (THD *, const Collation::aux_key_type &, const Collation **);
template bool Storage_adapter::drop(THD *, Collation*);
template bool Storage_adapter::store(THD *, Collation*);

template bool Storage_adapter::get<Schema::id_key_type, Schema>
       (THD *, const Schema::id_key_type &, const Schema **);
template bool Storage_adapter::get<Schema::name_key_type, Schema>
       (THD *, const Schema::name_key_type &, const Schema **);
template bool Storage_adapter::get<Schema::aux_key_type, Schema>
       (THD *, const Schema::aux_key_type &, const Schema **);
template bool Storage_adapter::drop(THD *, Schema*);
template bool Storage_adapter::store(THD *, Schema*);

template bool Storage_adapter::get<Tablespace::id_key_type, Tablespace>
       (THD *, const Tablespace::id_key_type &, const Tablespace **);
template bool Storage_adapter::get<Tablespace::name_key_type, Tablespace>
       (THD *, const Tablespace::name_key_type &, const Tablespace **);
template bool Storage_adapter::get<Tablespace::aux_key_type, Tablespace>
       (THD *, const Tablespace::aux_key_type &, const Tablespace **);
template bool Storage_adapter::drop(THD *, Tablespace*);
template bool Storage_adapter::store(THD *, Tablespace*);

} // namespace cache
} // namespace dd
