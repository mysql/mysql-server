/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#include "sql/sql_cmd_ddl_type.h"
#include "sql/dd/cache/dictionary_client.h"  // Dictionary_client
#include "sql/dd/dd_udt_type.h"
#include "sql/mysqld.h"  // lower_case_table_names
#include "sql/sql_lex.h"
#include "sql/transaction.h"
#include "sql/warn_not_implemented.h"

bool Sql_cmd_create_type::execute(THD *thd) {
  bool rc;

#ifdef WITH_EXPERIMENTAL_UDT
  WARN_NOT_IMPLEMENTED(thd, "Sql_cmd_create_type::execute()");

  if (m_type_ident->db.length == 0) {
    m_type_ident->db = thd->db();

    if (m_type_ident->db.length == 0) {
      my_error(ER_NO_DB_ERROR, MYF(0));
      return true;
    }
  }

  const char *db_name = m_type_ident->db.str;
  const char *type_name = m_type_ident->type.str;

  assert(db_name != nullptr);

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
                   MDL_EXCLUSIVE, MDL_TRANSACTION);

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
  dd::String_type schema_name{m_type_ident->db.str};
  const dd::Schema *existing_schema = nullptr;
  if (dc.acquire(schema_name, &existing_schema)) {
    return true;
  }

  if (existing_schema == nullptr) {
    my_error(ER_NO_SUCH_DB, MYF(0), schema_name.c_str());
    return true;
  }

  // CREATE TYPE

  bool exists;
  if (dd::udt_type_exists(thd->dd_client(), db_name, type_name, &exists)) {
    return true;
  }

  if (exists) {
    my_error(ER_UDT_TYPE_CREATE_EXISTS, MYF(0), db_name, type_name);
    return true;
  }

  if (dd::create_udt_type(thd, *existing_schema, type_name)) {
    return true;
  }

  if (trans_commit_stmt(thd) || trans_commit(thd)) {
    return true;
  }

  my_ok(thd);
  rc = false;
#else
  my_error(ER_NOT_SUPPORTED_YET, MYF(0), "CREATE TYPE");
  rc = true;
#endif

  return rc;
}
