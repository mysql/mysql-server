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

#include "sql/dd/impl/types/udt_method_impl.h"

#include <stdint.h>

#include <optional>

#include "my_rapidjson_size_t.h"  // IWYU pragma: keep

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

#include "m_string.h"
#include "sql/dd/dd_utility.h"               // normalize_string()
#include "sql/dd/impl/dictionary_impl.h"     // Dictionary_impl
#include "sql/dd/impl/raw/raw_record.h"      // Raw_record
#include "sql/dd/impl/sdi_impl.h"            // sdi read/write functions
#include "sql/dd/impl/tables/schemata.h"     // Schemata::name_collation
#include "sql/dd/impl/tables/udt_methods.h"  // Spatial_reference_sy...
#include "sql/dd/impl/transaction_impl.h"    // Open_dictionary_tables_ctx
#include "sql/dd/impl/utils.h"               // is_string_in_lowercase
#include "string_with_len.h"

namespace dd {
class Sdi_rcontext;
class Sdi_wcontext;
}  // namespace dd

using dd::tables::UDT_Methods;

namespace dd {

///////////////////////////////////////////////////////////////////////////
// UDT_Method_impl implementation.
///////////////////////////////////////////////////////////////////////////

UDT_Method_impl::UDT_Method_impl()
    : m_type_id(INVALID_OBJECT_ID),
      m_specific_name(),
      m_is_static(false),
      m_is_overriding(false),
      m_is_constructor(false),
      m_is_unsigned(false),
      m_char_max_length(0),
      m_char_octet_length(0),
      m_numeric_precision(0),
      m_numeric_scale(0),
      m_numeric_scale_null(true),
      m_datetime_precision(0),
      m_datetime_precision_null(true),
      m_return_type_id(INVALID_OBJECT_ID),
      m_collation_id(INVALID_OBJECT_ID),
      m_is_explicit_collation(0),

      m_created(0),
      m_last_altered(0) {}

bool UDT_Method_impl::validate() const { return false; }

///////////////////////////////////////////////////////////////////////////

bool UDT_Method_impl::restore_attributes(const Raw_record &r) {
  restore_id(r, UDT_Methods::FIELD_ID);
  restore_name(r, UDT_Methods::FIELD_NAME);

  m_type_id = r.read_ref_id(UDT_Methods::FIELD_TYPE_ID);
  m_specific_name = r.read_str(UDT_Methods::FIELD_SPECIFIC_NAME);
  m_is_static = r.read_bool(UDT_Methods::FIELD_IS_STATIC);
  m_is_overriding = r.read_bool(UDT_Methods::FIELD_IS_OVERRIDING);
  m_is_constructor = r.read_bool(UDT_Methods::FIELD_IS_CONSTRUCTOR);

  m_is_unsigned = r.read_bool(UDT_Methods::FIELD_IS_UNSIGNED);
  m_char_max_length = r.read_uint(UDT_Methods::FIELD_CHAR_MAX_LENGTH);
  m_char_octet_length = r.read_uint(UDT_Methods::FIELD_CHAR_OCTET_LENGTH);
  m_numeric_precision = r.read_uint(UDT_Methods::FIELD_NUMERIC_PRECISION);
  m_numeric_scale = r.read_uint(UDT_Methods::FIELD_NUMERIC_SCALE);
  m_numeric_scale_null = r.is_null(UDT_Methods::FIELD_NUMERIC_SCALE);
  m_datetime_precision = r.read_uint(UDT_Methods::FIELD_DATETIME_PRECISION);
  m_datetime_precision_null = r.is_null(UDT_Methods::FIELD_DATETIME_PRECISION);
  m_collation_id = r.read_ref_id(UDT_Methods::FIELD_COLLATION_ID);
  m_is_explicit_collation =
      r.read_bool(UDT_Methods::FIELD_IS_EXPLICIT_COLLATION);

  m_return_type_id = r.read_ref_id(UDT_Methods::FIELD_RETURN_TYPE_ID);

  m_last_altered = r.read_int(UDT_Methods::FIELD_LAST_ALTERED);
  m_created = r.read_int(UDT_Methods::FIELD_CREATED);

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool UDT_Method_impl::store_attributes(Raw_record *r) {
  return store_id(r, UDT_Methods::FIELD_ID) ||
         store_name(r, UDT_Methods::FIELD_NAME) ||
         r->store_ref_id(UDT_Methods::FIELD_TYPE_ID, m_type_id) ||
         r->store(UDT_Methods::FIELD_SPECIFIC_NAME, m_specific_name) ||
         r->store(UDT_Methods::FIELD_IS_STATIC, m_is_static) ||
         r->store(UDT_Methods::FIELD_IS_OVERRIDING, m_is_overriding) ||
         r->store(UDT_Methods::FIELD_IS_CONSTRUCTOR, m_is_constructor) ||

         r->store(UDT_Methods::FIELD_IS_UNSIGNED, m_is_unsigned) ||
         r->store(UDT_Methods::FIELD_CHAR_MAX_LENGTH,
                  static_cast<uint>(m_char_max_length)) ||
         r->store(UDT_Methods::FIELD_CHAR_OCTET_LENGTH,
                  static_cast<uint>(m_char_octet_length)) ||
         r->store(UDT_Methods::FIELD_NUMERIC_PRECISION, m_numeric_precision) ||
         r->store(UDT_Methods::FIELD_NUMERIC_SCALE, m_numeric_scale,
                  m_numeric_scale_null) ||
         r->store(UDT_Methods::FIELD_DATETIME_PRECISION, m_datetime_precision,
                  m_datetime_precision_null) ||
         r->store(UDT_Methods::FIELD_NUMERIC_PRECISION, m_numeric_precision) ||
         r->store_ref_id(UDT_Methods::FIELD_COLLATION_ID, m_collation_id) ||
         r->store(UDT_Methods::FIELD_IS_EXPLICIT_COLLATION,
                  m_is_explicit_collation) ||
         r->store_ref_id(UDT_Methods::FIELD_RETURN_TYPE_ID, m_return_type_id) ||

         r->store(UDT_Methods::FIELD_CREATED, m_created) ||
         r->store(UDT_Methods::FIELD_LAST_ALTERED, m_last_altered);
}

///////////////////////////////////////////////////////////////////////////
static_assert(UDT_Methods::NUMBER_OF_FIELDS == 18,
              "UDT_Methods definition has changed, check if "
              "serialize() and deserialize() need to be updated!");
void UDT_Method_impl::serialize(Sdi_wcontext *wctx, Sdi_writer *w) const {
  w->StartObject();
  Entity_object_impl::serialize(wctx, w);

  write(w, m_type_id, STRING_WITH_LEN("type_id"));
  write(w, m_specific_name, STRING_WITH_LEN("specific_name"));
  write(w, m_is_static, STRING_WITH_LEN("is_static"));
  write(w, m_is_overriding, STRING_WITH_LEN("is_overriding"));
  write(w, m_is_constructor, STRING_WITH_LEN("is_constructor"));
  write(w, m_is_unsigned, STRING_WITH_LEN("is_unsigned"));
  write(w, m_char_max_length, STRING_WITH_LEN("char_max_length"));
  write(w, m_char_octet_length, STRING_WITH_LEN("char_octet_length"));
  write(w, m_numeric_precision, STRING_WITH_LEN("numeric_precision"));
  write(w, m_numeric_scale, STRING_WITH_LEN("numeric_scale"));
  write(w, m_numeric_scale_null, STRING_WITH_LEN("numeric_scale_null"));
  write(w, m_datetime_precision, STRING_WITH_LEN("datetime_precision"));
  write(w, m_datetime_precision_null,
        STRING_WITH_LEN("datetime_precision_null"));
  write(w, m_collation_id, STRING_WITH_LEN("collation_id"));
  write(w, m_is_explicit_collation, STRING_WITH_LEN("is_explicit_collation"));
  write(w, m_return_type_id, STRING_WITH_LEN("return_type_id"));

  write(w, m_last_altered, STRING_WITH_LEN("last_altered"));
  write(w, m_created, STRING_WITH_LEN("created"));
  w->EndObject();
}

///////////////////////////////////////////////////////////////////////////

bool UDT_Method_impl::deserialize(Sdi_rcontext *rctx, const RJ_Value &val) {
  Entity_object_impl::deserialize(rctx, val);

  read(&m_type_id, val, "type_id");
  read(&m_specific_name, val, "specific_name");
  read(&m_is_static, val, "is_static");
  read(&m_is_overriding, val, "is_overriding");
  read(&m_is_constructor, val, "is_constructor");
  read(&m_is_unsigned, val, "is_unsigned");
  read(&m_char_max_length, val, "char_max_length");
  read(&m_char_octet_length, val, "char_octet_length");
  read(&m_numeric_precision, val, "numeric_precision");
  read(&m_numeric_scale, val, "numeric_scale");
  read(&m_numeric_scale_null, val, "numeric_scale_null");
  read(&m_datetime_precision, val, "datetime_precision");
  read(&m_datetime_precision_null, val, "datetime_precision_null");
  read(&m_collation_id, val, "collation_id");
  read(&m_is_explicit_collation, val, "is_explicit_collation");
  read(&m_return_type_id, val, "return_type_id");

  read(&m_last_altered, val, "last_altered");
  read(&m_created, val, "created");

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool UDT_Method::update_id_key(Id_key *key, Object_id id) {
  key->update(id);
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool UDT_Method::update_name_key(Name_key *key, Object_id type_id,
                                 const String_type &name) {
  return UDT_Methods::update_object_key(key, type_id, name);
}

///////////////////////////////////////////////////////////////////////////

const Object_table &UDT_Method_impl::object_table() const {
  return DD_table::instance();
}

///////////////////////////////////////////////////////////////////////////

void UDT_Method_impl::register_tables(Open_dictionary_tables_ctx *otx) {
  otx->add_table<UDT_Methods>();
}

///////////////////////////////////////////////////////////////////////////

void UDT_Method::create_mdl_key(const String_type &schema_name,
                                const String_type &type_name,
                                const String_type & /* method_name */,
                                MDL_key *mdl_key) {
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

  // TODO: MDL_key::UDT_METHOD
  mdl_key->mdl_key_init(MDL_key::UDT_TYPE, schema_name.c_str(),
                        type_name.c_str());
}

}  // namespace dd
