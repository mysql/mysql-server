/* Copyright (c) 2016, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/dd/impl/types/udt_type_impl.h"

#include <stdint.h>

#include <optional>

#include "my_rapidjson_size_t.h"  // IWYU pragma: keep

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

#include "m_string.h"
#include "sql/dd/dd_utility.h"             // normalize_string()
#include "sql/dd/impl/dictionary_impl.h"   // Dictionary_impl
#include "sql/dd/impl/raw/raw_record.h"    // Raw_record
#include "sql/dd/impl/sdi_impl.h"          // sdi read/write functions
#include "sql/dd/impl/tables/schemata.h"   // Schemata::name_collation
#include "sql/dd/impl/tables/udt_types.h"  // Spatial_reference_sy...
#include "sql/dd/impl/transaction_impl.h"  // Open_dictionary_tables_ctx
#include "sql/dd/impl/utils.h"             // is_string_in_lowercase
#include "string_with_len.h"

namespace dd {
class Sdi_rcontext;
class Sdi_wcontext;
}  // namespace dd

using dd::tables::UDT_Types;

namespace dd {

///////////////////////////////////////////////////////////////////////////
// UDT_Type_impl implementation.
///////////////////////////////////////////////////////////////////////////

bool UDT_Type_impl::validate() const { return false; }

///////////////////////////////////////////////////////////////////////////

bool UDT_Type_impl::restore_attributes(const Raw_record &r) {
  restore_id(r, UDT_Types::FIELD_ID);
  restore_name(r, UDT_Types::FIELD_NAME);

  m_schema_id = r.read_ref_id(UDT_Types::FIELD_SCHEMA_ID);
  m_last_altered = r.read_int(UDT_Types::FIELD_LAST_ALTERED);
  m_created = r.read_int(UDT_Types::FIELD_CREATED);

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool UDT_Type_impl::store_attributes(Raw_record *r) {
  return store_id(r, UDT_Types::FIELD_ID) ||
         store_name(r, UDT_Types::FIELD_NAME) ||
         r->store_ref_id(UDT_Types::FIELD_SCHEMA_ID, m_schema_id) ||
         r->store(UDT_Types::FIELD_CREATED, m_created) ||
         r->store(UDT_Types::FIELD_LAST_ALTERED, m_last_altered);
}

///////////////////////////////////////////////////////////////////////////
static_assert(UDT_Types::NUMBER_OF_FIELDS == 5,
              "UDT_Types definition has changed, check if "
              "serialize() and deserialize() need to be updated!");
void UDT_Type_impl::serialize(Sdi_wcontext *wctx, Sdi_writer *w) const {
  w->StartObject();
  Entity_object_impl::serialize(wctx, w);
  write(w, m_last_altered, STRING_WITH_LEN("last_altered"));
  write(w, m_created, STRING_WITH_LEN("created"));
  w->EndObject();
}

///////////////////////////////////////////////////////////////////////////

bool UDT_Type_impl::deserialize(Sdi_rcontext *rctx, const RJ_Value &val) {
  Entity_object_impl::deserialize(rctx, val);
  read(&m_last_altered, val, "last_altered");
  read(&m_created, val, "created");

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool UDT_Type::update_id_key(Id_key *key, Object_id id) {
  key->update(id);
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool UDT_Type::update_name_key(Name_key *key, Object_id schema_id,
                               const String_type &name) {
  return UDT_Types::update_object_key(key, schema_id, name);
}

///////////////////////////////////////////////////////////////////////////

const Object_table &UDT_Type_impl::object_table() const {
  return DD_table::instance();
}

///////////////////////////////////////////////////////////////////////////

void UDT_Type_impl::register_tables(Open_dictionary_tables_ctx *otx) {
  otx->add_table<UDT_Types>();
}

///////////////////////////////////////////////////////////////////////////

void UDT_Type::create_mdl_key(const String_type &schema_name,
                              const String_type &name, MDL_key *mdl_key) {
#ifndef DEBUG_OFF
  // Make sure schema name is lowercased when lower_case_table_names == 2.
  if (lower_case_table_names == 2)
    assert(is_string_in_lowercase(schema_name,
                                  tables::Schemata::name_collation()));
  DBUG_EXECUTE_IF("simulate_lctn_two_case_for_schema_case_compare", {
    assert((lower_case_table_names == 2) ||
           is_string_in_lowercase(schema_name, &my_charset_utf8mb3_tolower_ci));
  });
#endif

  mdl_key->mdl_key_init(MDL_key::UDT_TYPE, schema_name.c_str(), name.c_str());
}

}  // namespace dd
