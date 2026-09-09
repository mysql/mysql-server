/*
   Copyright (c) 2000, 2026, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "sql/sql_user_defined_type.h"

#include <assert.h>

/* HAVE_PSI_*_INTERFACE */
#include "my_psi_config.h"  // IWYU pragma: keep

#include "dd/object_id.h"
#include "decimal.h"
#include "field_types.h"  // enum_field_types
#include "lex_string.h"
#include "sql/dd/cache/dictionary_client.h"  // dd::cache::Dictionary_client
#include "sql/dd/dd_udt_type.h"
#include "sql/mysqld.h"   // lower_case_table_names
#include "sql/sql_lex.h"  // Type_ident

#include "sql/warn_not_implemented.h"

bool resolve_type_descriptor(THD *thd, TypeDescriptor *td) {
  assert(td != nullptr);
  const Type_ident *type_ident = td->m_type_ident;

  if (type_ident == nullptr) {
    // Builtin type, nothing to resolve.
    return false;
  }

  assert(td->m_type == MYSQL_TYPE_INVALID);

  const char *db_name = type_ident->db.str;
  const char *type_name = type_ident->type.str;

  assert(db_name != nullptr);
  assert(type_name != nullptr);

  // MDL LOCK (SCHEMA)

  /*
    When creating the schema, we must lock the schema name without case (for
    correct MDL locking) when l_c_t_n == 2.
  */
  char name_buf[NAME_LEN + 1];
  const char *lock_db_name = db_name;
  if (lower_case_table_names == 2) {
    my_stpcpy(name_buf, db_name);
    my_casedn_str(&my_charset_utf8mb3_tolower_ci, name_buf);
    lock_db_name = name_buf;
  }

  if (lock_schema_name(thd, lock_db_name)) {
    return true;
  }

  // MDL LOCK (TYPE)

  MDL_request mdl_request;
  MDL_REQUEST_INIT(&mdl_request, MDL_key::UDT_TYPE, db_name, type_name,
                   MDL_INTENTION_EXCLUSIVE, MDL_TRANSACTION);

  /*
    Acquire the lock request created above, and check if
    acquisition fails (e.g. timeout or deadlock).
  */
  if (thd->mdl_context.acquire_lock(&mdl_request,
                                    thd->variables.lock_wait_timeout)) {
    assert(thd->is_system_thread() || thd->killed || thd->is_error());
    return true;
  }

  // DD LOOK UP

  const dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  dd::cache::Dictionary_client &dc = *thd->dd_client();
  dd::String_type schema_name{type_ident->db.str};
  const dd::Schema *existing_schema = nullptr;
  if (dc.acquire(schema_name, &existing_schema)) {
    return true;
  }

  if (existing_schema == nullptr) {
    my_error(ER_NO_SUCH_DB, MYF(0), schema_name.c_str());
    return true;
  }

  // LOOKUP TYPE

  dd::String_type dd_type_name{type_ident->type.str};
  const dd::UDT_Type *obj = nullptr;

  if (dc.acquire(schema_name, dd_type_name, &obj)) {
    return true;
  }

  if (obj == nullptr) {
    my_error(ER_NO_SUCH_UDT_TYPE, MYF(0), schema_name.c_str(),
             dd_type_name.c_str());
    return true;
  }

  WARN_NOT_IMPLEMENTED(thd, "resolve_type_descriptor()");

  fprintf(stderr, "resolve_type_descriptor() use type\n");

#ifdef NEVER
  // FIXME: forged CHAR(13)
  td->m_type = MYSQL_TYPE_STRING;
  td->m_type_flags = 0;
  td->m_length = "13";
  td->m_dec = nullptr;
  td->m_charset = &my_charset_utf8mb4_0900_ai_ci;
  td->m_has_explicit_collation = false;
  td->m_geo_type = 0;
  td->m_internal_list = nullptr;
#endif

  // FIXME: forged BINARY(16)
  td->m_type = MYSQL_TYPE_BLOB;
  td->m_type_flags = 0;
  td->m_length = "16";
  td->m_dec = nullptr;
  td->m_charset = &my_charset_bin;
  td->m_has_explicit_collation = false;
  td->m_geo_type = 0;
  td->m_internal_list = nullptr;

  // FIXME, use type from dd::UDT_Type.

  return false;
}
