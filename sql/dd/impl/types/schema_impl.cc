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

#include "dd/impl/types/schema_impl.h"

#include "mysql_time.h"                    // MYSQL_TIME
#include "current_thd.h"                   // current_thd
#include "mysqld_error.h"                  // ER_*
#include "sql_class.h"                     // THD
#include "tztime.h"                        // Time_zone

#include "dd/dd.h"                         // create_object
#include "dd/impl/dictionary_impl.h"       // Dictionary_impl
#include "dd/impl/transaction_impl.h"      // Open_dictionary_tables_ctx
#include "dd/impl/raw/object_keys.h"       // Primary_id_key
#include "dd/impl/raw/raw_record.h"        // Raw_record
#include "dd/impl/tables/schemata.h"       // Schemata
#include "dd/impl/tables/tables.h"         // Tables
#include "dd/types/view.h"                 // View

#include <memory>

using dd::tables::Schemata;
using dd::tables::Tables;

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Schema implementation.
///////////////////////////////////////////////////////////////////////////

const Dictionary_object_table &Schema::OBJECT_TABLE()
{
  return Schemata::instance();
}

///////////////////////////////////////////////////////////////////////////

const Object_type &Schema::TYPE()
{
  static Schema_type s_instance;
  return s_instance;
}

///////////////////////////////////////////////////////////////////////////
// Schema_impl implementation.
///////////////////////////////////////////////////////////////////////////

bool Schema_impl::validate() const
{
  if (m_default_collation_id == INVALID_OBJECT_ID)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Schema_impl::OBJECT_TABLE().name().c_str(),
             "Default collation ID is not set");
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Schema_impl::restore_attributes(const Raw_record &r)
{
  restore_id(r, Schemata::FIELD_ID);
  restore_name(r, Schemata::FIELD_NAME);

  m_created= r.read_int(Schemata::FIELD_CREATED);
  m_last_altered= r.read_int(Schemata::FIELD_LAST_ALTERED);

  m_default_collation_id=
    r.read_ref_id(
      Schemata::FIELD_DEFAULT_COLLATION_ID);

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Schema_impl::store_attributes(Raw_record *r)
{
  Object_id default_catalog_id= Dictionary_impl::instance()->default_catalog_id();

  return store_id(r, Schemata::FIELD_ID) ||
         store_name(r, Schemata::FIELD_NAME) ||
         r->store(Schemata::FIELD_CATALOG_ID, default_catalog_id) ||
         r->store_ref_id(Schemata::FIELD_DEFAULT_COLLATION_ID,
                         m_default_collation_id) ||
         r->store(Schemata::FIELD_CREATED, m_created) ||
         r->store(Schemata::FIELD_LAST_ALTERED, m_last_altered);
}

///////////////////////////////////////////////////////////////////////////

void
Schema_impl::serialize(WriterVariant *wv) const
{

}

void
Schema_impl::deserialize(const RJ_Document *d)
{

}

///////////////////////////////////////////////////////////////////////////

bool Schema::update_id_key(id_key_type *key, Object_id id)
{
  key->update(id);
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Schema::update_name_key(name_key_type *key,
                             const std::string &name)
{
  return Schemata::update_object_key(key,
                      Dictionary_impl::instance()->default_catalog_id(),
                      name);
}

///////////////////////////////////////////////////////////////////////////

Table *Schema_impl::create_table()
{
  std::unique_ptr<Table> t(dd::create_object<Table>());
  t->set_schema_id(this->id());
  t->set_collation_id(default_collation_id());

  // Get statement start time.
  THD *thd= current_thd;
  MYSQL_TIME curtime;
  thd->variables.time_zone->gmt_sec_to_TIME(&curtime, thd->query_start());
  ulonglong ull_curtime= TIME_to_ulonglong_datetime(&curtime);

  // Set new table start time.
  t->set_created(ull_curtime);
  t->set_last_altered(ull_curtime);

  return t.release();
}

///////////////////////////////////////////////////////////////////////////

View *Schema_impl::create_view()
{
  std::unique_ptr<View> v(dd::create_object<View>());
  v->set_schema_id(this->id());

  // Get statement start time.
  THD *thd= current_thd;
  MYSQL_TIME curtime;
  thd->variables.time_zone->gmt_sec_to_TIME(&curtime, thd->query_start());
  ulonglong ull_curtime= TIME_to_ulonglong_datetime(&curtime);

  v->set_created(ull_curtime);
  v->set_last_altered(ull_curtime);

  return v.release();
}

///////////////////////////////////////////////////////////////////////////

View *Schema_impl::create_system_view()
{
  std::unique_ptr<View> v(dd::create_object<View>());
  v->set_system_view(true);
  v->set_schema_id(this->id());

  return v.release();
}

///////////////////////////////////////////////////////////////////////////
// Schema_type implementation.
///////////////////////////////////////////////////////////////////////////

void Schema_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Schemata>();
}

///////////////////////////////////////////////////////////////////////////

}
