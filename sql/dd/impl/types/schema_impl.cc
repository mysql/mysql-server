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

#include "dd/impl/types/schema_impl.h"

#include <memory>

#include "dd/dd.h"                         // create_object
#include "dd/impl/dictionary_impl.h"       // Dictionary_impl
#include "dd/impl/raw/object_keys.h"       // Primary_id_key
#include "dd/impl/raw/raw_record.h"        // Raw_record
#include "dd/impl/sdi_impl.h"              // sdi read/write functions
#include "dd/impl/tables/schemata.h"       // Schemata
#include "dd/impl/transaction_impl.h"      // Open_dictionary_tables_ctx
#include "dd/impl/types/object_table_definition_impl.h"
#include "dd/types/event.h"                // Event
#include "dd/types/function.h"             // Function
#include "dd/types/procedure.h"            // Procedure
#include "dd/types/table.h"
#include "dd/types/view.h"                 // View
#include "m_string.h"
#include "mdl.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_decimal.h"
#include "my_sys.h"
#include "my_time.h"
#include "mysql_com.h"
#include "mysqld_error.h"                  // ER_*
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "sql_class.h"                     // THD
#include "system_variables.h"
#include "tztime.h"                        // Time_zone

namespace dd {
class Sdi_rcontext;
class Sdi_wcontext;
namespace tables {
class Tables;
}  // namespace tables
}  // namespace dd


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
static_assert(Schemata::FIELD_LAST_ALTERED==5,
              "Schemata definition has changed. Review (de)ser memfuns!");
void
Schema_impl::serialize(Sdi_wcontext *wctx, Sdi_writer *w) const
{
  w->StartObject();
  Entity_object_impl::serialize(wctx, w);
  write(w, m_default_collation_id, STRING_WITH_LEN("default_collation_id"));
  write(w, m_created, STRING_WITH_LEN("created"));
  write(w, m_last_altered, STRING_WITH_LEN("last_altered"));
  w->EndObject();
}

///////////////////////////////////////////////////////////////////////////

bool
Schema_impl::deserialize(Sdi_rcontext *rctx, const RJ_Value &val)
{
  Entity_object_impl::deserialize(rctx, val);
  read(&m_default_collation_id, val, "default_collation_id");
  read(&m_created, val, "created");
  read(&m_last_altered, val, "last_altered");
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Schema::update_id_key(id_key_type *key, Object_id id)
{
  key->update(id);
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Schema::update_name_key(name_key_type *key,
                             const String_type &name)
{
  return Schemata::update_object_key(key,
                      Dictionary_impl::instance()->default_catalog_id(),
                      name);
}

///////////////////////////////////////////////////////////////////////////

Event *Schema_impl::create_event(THD *thd) const
{
  std::unique_ptr<Event> f(dd::create_object<Event>());
  f->set_schema_id(this->id());

  // Get statement start time.
  MYSQL_TIME curtime;
  thd->variables.time_zone->gmt_sec_to_TIME(&curtime,
                                            thd->query_start_in_secs());
  ulonglong ull_curtime= TIME_to_ulonglong_datetime(&curtime);

  f->set_created(ull_curtime);
  f->set_last_altered(ull_curtime);

  return f.release();
}

///////////////////////////////////////////////////////////////////////////

Function *Schema_impl::create_function(THD *thd) const
{
  std::unique_ptr<Function> f(dd::create_object<Function>());
  f->set_schema_id(this->id());

  // Get statement start time.
  MYSQL_TIME curtime;
  thd->variables.time_zone->gmt_sec_to_TIME(&curtime,
                                            thd->query_start_in_secs());
  ulonglong ull_curtime= TIME_to_ulonglong_datetime(&curtime);

  f->set_created(ull_curtime);
  f->set_last_altered(ull_curtime);

  return f.release();
}

///////////////////////////////////////////////////////////////////////////

Procedure *Schema_impl::create_procedure(THD *thd) const
{
  std::unique_ptr<Procedure> p(dd::create_object<Procedure>());
  p->set_schema_id(this->id());

  // Get statement start time.
  MYSQL_TIME curtime;
  thd->variables.time_zone->gmt_sec_to_TIME(&curtime,
                                            thd->query_start_in_secs());
  ulonglong ull_curtime= TIME_to_ulonglong_datetime(&curtime);

  p->set_created(ull_curtime);
  p->set_last_altered(ull_curtime);

  return p.release();
}

///////////////////////////////////////////////////////////////////////////

Table *Schema_impl::create_table(THD *thd) const
{
  // Creating tables requires an IX meta data lock on the schema name.
#ifndef DBUG_OFF
  char name_buf[NAME_LEN + 1];
  DBUG_ASSERT(thd->mdl_context.owns_equal_or_stronger_lock(
                MDL_key::SCHEMA,
                dd::Object_table_definition_impl::
                  fs_name_case(name(), name_buf),
                "",
                MDL_INTENTION_EXCLUSIVE));
#endif

  std::unique_ptr<Table> t(dd::create_object<Table>());
  t->set_schema_id(this->id());
  t->set_collation_id(default_collation_id());

  // Get statement start time.
  MYSQL_TIME curtime;
  thd->variables.time_zone->gmt_sec_to_TIME(&curtime,
                                            thd->query_start_in_secs());
  ulonglong ull_curtime= TIME_to_ulonglong_datetime(&curtime);

  // Set new table start time.
  t->set_created(ull_curtime);
  t->set_last_altered(ull_curtime);

  return t.release();
}

///////////////////////////////////////////////////////////////////////////

View *Schema_impl::create_view(THD *thd) const
{
  // Creating views requires an IX meta data lock on the schema name.
#ifndef DBUG_OFF
  char name_buf[NAME_LEN + 1];
  DBUG_ASSERT(thd->mdl_context.owns_equal_or_stronger_lock(
                MDL_key::SCHEMA,
                dd::Object_table_definition_impl::
                  fs_name_case(name(), name_buf),
                "",
                MDL_INTENTION_EXCLUSIVE));
#endif

  std::unique_ptr<View> v(dd::create_object<View>());
  v->set_schema_id(this->id());

  // Get statement start time.
  MYSQL_TIME curtime;
  thd->variables.time_zone->gmt_sec_to_TIME(&curtime,
                                            thd->query_start_in_secs());
  ulonglong ull_curtime= TIME_to_ulonglong_datetime(&curtime);

  v->set_created(ull_curtime);
  v->set_last_altered(ull_curtime);

  return v.release();
}

///////////////////////////////////////////////////////////////////////////

View *Schema_impl::create_system_view(THD *thd MY_ATTRIBUTE((unused))) const
{
  // Creating system views requires an IX meta data lock on the schema name.
#ifndef DBUG_OFF
  char name_buf[NAME_LEN + 1];
  DBUG_ASSERT(thd->mdl_context.owns_equal_or_stronger_lock(
                MDL_key::SCHEMA,
                dd::Object_table_definition_impl::
                  fs_name_case(name(), name_buf),
                "",
                MDL_INTENTION_EXCLUSIVE));
#endif

  std::unique_ptr<View> v(dd::create_object<View>());
  v->set_system_view(true);
  v->set_schema_id(this->id());

  // Get statement start time.
  MYSQL_TIME curtime;
  thd->variables.time_zone->gmt_sec_to_TIME(&curtime,
                                            thd->query_start_in_secs());
  ulonglong ull_curtime= TIME_to_ulonglong_datetime(&curtime);

  v->set_created(ull_curtime);
  v->set_last_altered(ull_curtime);


  return v.release();
}

///////////////////////////////////////////////////////////////////////////
//Schema_type implementation.
///////////////////////////////////////////////////////////////////////////

void Schema_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Schemata>();
}

///////////////////////////////////////////////////////////////////////////

}
