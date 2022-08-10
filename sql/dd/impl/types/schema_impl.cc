/* Copyright (c) 2014, 2022, Oracle and/or its affiliates.

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

#include "sql/dd/impl/types/schema_impl.h"

#include <assert.h>
#include <memory>

#include "my_rapidjson_size_t.h"  // IWYU pragma: keep

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

#include "m_string.h"
#include "my_compiler.h"

#include "my_sys.h"
#include "my_time.h"
#include "mysql_com.h"
#include "mysqld_error.h"                         // ER_*
#include "sql/dd/dd.h"                            // create_object
#include "sql/dd/impl/bootstrap/bootstrap_ctx.h"  // dd::bootstrap::DD_bootstrap_ctx
#include "sql/dd/impl/dictionary_impl.h"          // Dictionary_impl
#include "sql/dd/impl/raw/raw_record.h"           // Raw_record
#include "sql/dd/impl/sdi_impl.h"                 // sdi read/write functions
#include "sql/dd/impl/tables/schemata.h"          // Schemata
#include "sql/dd/impl/transaction_impl.h"         // Open_dictionary_tables_ctx
#include "sql/dd/impl/types/object_table_definition_impl.h"
#include "sql/dd/impl/utils.h"       // dd::my_time_t_to_ull_datetime()
#include "sql/dd/types/event.h"      // Event
#include "sql/dd/types/function.h"   // Function
#include "sql/dd/types/procedure.h"  // Procedure
#include "sql/dd/types/table.h"
#include "sql/dd/types/view.h"  // View
#include "sql/histograms/value_map.h"
#include "sql/mdl.h"
#include "sql/sql_class.h"  // THD
#include "sql/system_variables.h"
#include "sql/tztime.h"  // Time_zone

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

static const std::set<String_type> default_valid_option_keys = {"read_only"};

///////////////////////////////////////////////////////////////////////////
// Schema_impl implementation.
///////////////////////////////////////////////////////////////////////////

Schema_impl::Schema_impl()
    : m_se_private_data(),
      m_options(default_valid_option_keys),
      m_default_collation_id(INVALID_OBJECT_ID) {}

bool Schema_impl::validate() const {
  if (m_default_collation_id == INVALID_OBJECT_ID) {
    my_error(ER_INVALID_DD_OBJECT, MYF(0), DD_table::instance().name().c_str(),
             "Default collation ID is not set");
    return true;
  }

  return false;
}

/////////////////////////////////////////////////////////////////////////

bool Schema_impl::read_only() const {
  bool state = false;
  if (options().exists("read_only") && options().get("read_only", &state)) {
    return false;
  }
  return state;
}

/////////////////////////////////////////////////////////////////////////

void Schema_impl::set_read_only(bool state) {
  options().set("read_only", state);
}

///////////////////////////////////////////////////////////////////////////

bool Schema_impl::restore_attributes(const Raw_record &r) {
  restore_id(r, Schemata::FIELD_ID);
  restore_name(r, Schemata::FIELD_NAME);

  m_created = r.read_int(Schemata::FIELD_CREATED);
  m_last_altered = r.read_int(Schemata::FIELD_LAST_ALTERED);

  m_default_collation_id = r.read_ref_id(Schemata::FIELD_DEFAULT_COLLATION_ID);

  // m_default_encryption is added in 80016
  if (bootstrap::DD_bootstrap_ctx::instance().is_dd_upgrade_from_before(
          bootstrap::DD_VERSION_80016)) {
    m_default_encryption = enum_encryption_type::ET_NO;
  } else {
    m_default_encryption = static_cast<enum_encryption_type>(
        r.read_int(Schemata::FIELD_DEFAULT_ENCRYPTION));
  }

  // m_se_private_data is added in 80017
  if (bootstrap::DD_bootstrap_ctx::instance().is_dd_upgrade_from_before(
          bootstrap::DD_VERSION_80017)) {
    set_se_private_data("");
  } else {
    set_se_private_data(r.read_str(Schemata::FIELD_SE_PRIVATE_DATA, ""));
  }

  set_options(r.read_str(Schemata::FIELD_OPTIONS, ""));
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Schema_impl::store_attributes(Raw_record *r) {
  Object_id default_catalog_id =
      Dictionary_impl::instance()->default_catalog_id();

  // Store m_default_encryption only if we're not upgrading
  if (!bootstrap::DD_bootstrap_ctx::instance().is_dd_upgrade_from_before(
          bootstrap::DD_VERSION_80016) &&
      r->store(Schemata::FIELD_DEFAULT_ENCRYPTION,
               static_cast<int>(m_default_encryption))) {
    return true;
  }

  // Store m_se_private_data only if we're not upgrading from before 8.0.17
  if (!bootstrap::DD_bootstrap_ctx::instance().is_dd_upgrade_from_before(
          bootstrap::DD_VERSION_80017) &&
      r->store(Schemata::FIELD_SE_PRIVATE_DATA, m_se_private_data)) {
    return true;
  }

  return store_id(r, Schemata::FIELD_ID) ||
         store_name(r, Schemata::FIELD_NAME) ||
         r->store(Schemata::FIELD_CATALOG_ID, default_catalog_id) ||
         r->store_ref_id(Schemata::FIELD_DEFAULT_COLLATION_ID,
                         m_default_collation_id) ||
         r->store(Schemata::FIELD_CREATED, m_created) ||
         r->store(Schemata::FIELD_LAST_ALTERED, m_last_altered) ||
         r->store(Schemata::FIELD_OPTIONS, m_options);
}

///////////////////////////////////////////////////////////////////////////

bool Schema::update_id_key(Id_key *key, Object_id id) {
  key->update(id);
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Schema::update_name_key(Name_key *key, const String_type &name) {
  return Schemata::update_object_key(
      key, Dictionary_impl::instance()->default_catalog_id(), name);
}

///////////////////////////////////////////////////////////////////////////

Event *Schema_impl::create_event(THD *thd) const {
  std::unique_ptr<Event> f(dd::create_object<Event>());
  f->set_schema_id(this->id());

  // Get statement start time.
  ulonglong ull_curtime =
      dd::my_time_t_to_ull_datetime(thd->query_start_in_secs());

  f->set_created(ull_curtime);
  f->set_last_altered(ull_curtime);

  return f.release();
}

///////////////////////////////////////////////////////////////////////////

Function *Schema_impl::create_function(THD *thd) const {
  std::unique_ptr<Function> f(dd::create_object<Function>());
  f->set_schema_id(this->id());

  // Get statement start time.
  ulonglong ull_curtime =
      dd::my_time_t_to_ull_datetime(thd->query_start_in_secs());

  f->set_created(ull_curtime);
  f->set_last_altered(ull_curtime);

  return f.release();
}

///////////////////////////////////////////////////////////////////////////

Procedure *Schema_impl::create_procedure(THD *thd) const {
  std::unique_ptr<Procedure> p(dd::create_object<Procedure>());
  p->set_schema_id(this->id());

  // Get statement start time.
  ulonglong ull_curtime =
      dd::my_time_t_to_ull_datetime(thd->query_start_in_secs());

  p->set_created(ull_curtime);
  p->set_last_altered(ull_curtime);

  return p.release();
}

///////////////////////////////////////////////////////////////////////////

Table *Schema_impl::create_table(THD *thd) const {
// Creating tables requires an IX meta data lock on the schema name.
#ifndef NDEBUG
  char name_buf[NAME_LEN + 1];
  assert(thd->mdl_context.owns_equal_or_stronger_lock(
      MDL_key::SCHEMA,
      dd::Object_table_definition_impl::fs_name_case(name(), name_buf), "",
      MDL_INTENTION_EXCLUSIVE));
#endif

  std::unique_ptr<Table> t(dd::create_object<Table>());
  t->set_schema_id(this->id());
  t->set_collation_id(default_collation_id());

  // Get statement start time.
  ulonglong ull_curtime =
      dd::my_time_t_to_ull_datetime(thd->query_start_in_secs());

  // Set new table start time.
  t->set_created(ull_curtime);
  t->set_last_altered(ull_curtime);

  return t.release();
}

///////////////////////////////////////////////////////////////////////////

View *Schema_impl::create_view(THD *thd) const {
// Creating views requires an IX meta data lock on the schema name.
#ifndef NDEBUG
  char name_buf[NAME_LEN + 1];
  assert(thd->mdl_context.owns_equal_or_stronger_lock(
      MDL_key::SCHEMA,
      dd::Object_table_definition_impl::fs_name_case(name(), name_buf), "",
      MDL_INTENTION_EXCLUSIVE));
#endif

  std::unique_ptr<View> v(dd::create_object<View>());
  v->set_schema_id(this->id());

  // Get statement start time.
  ulonglong ull_curtime =
      dd::my_time_t_to_ull_datetime(thd->query_start_in_secs());

  v->set_created(ull_curtime);
  v->set_last_altered(ull_curtime);

  return v.release();
}

///////////////////////////////////////////////////////////////////////////

View *Schema_impl::create_system_view(THD *thd [[maybe_unused]]) const {
// Creating system views requires an IX meta data lock on the schema name.
#ifndef NDEBUG
  char name_buf[NAME_LEN + 1];
  assert(thd->mdl_context.owns_equal_or_stronger_lock(
      MDL_key::SCHEMA,
      dd::Object_table_definition_impl::fs_name_case(name(), name_buf), "",
      MDL_INTENTION_EXCLUSIVE));
#endif

  std::unique_ptr<View> v(dd::create_object<View>());
  v->set_system_view(true);
  v->set_schema_id(this->id());

  // Get statement start time.
  ulonglong ull_curtime =
      dd::my_time_t_to_ull_datetime(thd->query_start_in_secs());

  v->set_created(ull_curtime);
  v->set_last_altered(ull_curtime);

  return v.release();
}

///////////////////////////////////////////////////////////////////////////

const Object_table &Schema_impl::object_table() const {
  return DD_table::instance();
}

///////////////////////////////////////////////////////////////////////////

void Schema_impl::register_tables(Open_dictionary_tables_ctx *otx) {
  otx->add_table<Schemata>();
}

///////////////////////////////////////////////////////////////////////////

}  // namespace dd
