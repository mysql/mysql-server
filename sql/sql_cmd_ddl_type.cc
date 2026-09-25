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

bool Sql_cmd_ddl_type::use_default_db(THD *thd) {
  if (m_type_ident->db.length == 0) {
    m_type_ident->db = thd->db();

    if (m_type_ident->db.length == 0) {
      my_error(ER_NO_DB_ERROR, MYF(0));
      return true;
    }
  }

  return false;
}

bool Sql_cmd_ddl_type::check_privileges(THD * /* thd */) {
  // TODO:
  // - check CREATE TYPE privilege on DB.TYPE
  // - check ALTER TYPE privilege on DB.TYPE
  // - check DROP TYPE privilege on DB.TYPE

  return false;
}

bool Sql_cmd_ddl_type::acquire_mdl_schema(THD *thd) {
  const char *db_name = m_type_ident->db.str;

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

  return false;
}

bool Sql_cmd_ddl_type::acquire_mdl_type(THD *thd) {
  const char *db_name = m_type_ident->db.str;
  const char *type_name = m_type_ident->type.str;

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

  return false;
}

const dd::Schema *Sql_cmd_ddl_type::acquire_dd_schema(THD *thd) {
  const dd::Schema *schema;

  dd::cache::Dictionary_client &dc = *thd->dd_client();
  dd::String_type schema_name{m_type_ident->db.str};
  if (dc.acquire(schema_name, &schema)) {
    return nullptr;
  }

  return schema;
}

const dd::UDT_Type *Sql_cmd_ddl_type::acquire_dd_type(THD *thd) {
  const dd::UDT_Type *udt;

  dd::cache::Dictionary_client &dc = *thd->dd_client();
  dd::String_type schema_name{m_type_ident->db.str};
  dd::String_type type_name{m_type_ident->type.str};
  if (dc.acquire(schema_name, type_name, &udt)) {
    return nullptr;
  }

  return udt;
}

bool Sql_cmd_create_type::execute(THD *thd) {
#ifdef WITH_EXPERIMENTAL_UDT
  WARN_NOT_IMPLEMENTED(thd, "Sql_cmd_create_type::execute()");

  if (use_default_db(thd)) {
    return true;
  }

  if (check_privileges(thd)) {
    return true;
  }

  if (acquire_mdl_schema(thd)) {
    return true;
  }

  if (acquire_mdl_type(thd)) {
    return true;
  }

  const dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const char *db_name = m_type_ident->db.str;
  const char *type_name = m_type_ident->type.str;

  const dd::Schema *schema = acquire_dd_schema(thd);
  if (schema == nullptr) {
    my_error(ER_NO_SUCH_DB, MYF(0), db_name);
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

  if (dd::create_udt_type(thd, *schema, type_name)) {
    return true;
  }

  if (trans_commit_stmt(thd) || trans_commit(thd)) {
    return true;
  }

  my_ok(thd);
  return false;
#else
  my_error(ER_NOT_SUPPORTED_YET, MYF(0), "CREATE TYPE");
  return true;
#endif
}

bool Sql_cmd_drop_type::execute(THD *thd) {
#ifdef WITH_EXPERIMENTAL_UDT
  WARN_NOT_IMPLEMENTED(thd, "Sql_cmd_drop_type::execute()");

  if (use_default_db(thd)) {
    return true;
  }

  if (check_privileges(thd)) {
    return true;
  }

  if (acquire_mdl_schema(thd)) {
    return true;
  }

  if (acquire_mdl_type(thd)) {
    return true;
  }

  const dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const char *db_name = m_type_ident->db.str;
  const char *type_name = m_type_ident->type.str;

  const dd::Schema *schema = acquire_dd_schema(thd);
  if (schema == nullptr) {
    my_error(ER_NO_SUCH_DB, MYF(0), db_name);
    return true;
  }

  const dd::UDT_Type *udt = acquire_dd_type(thd);

  if ((udt == nullptr) && !m_if_exists) {
    my_error(ER_UDT_TYPE_DROP_EXISTS, MYF(0), db_name, type_name);
    return true;
  }

  if (udt != nullptr) {
    if (dd::drop_udt_type(thd, *udt)) {
      return true;
    }
  }

  if (trans_commit_stmt(thd) || trans_commit(thd)) {
    return true;
  }

  my_ok(thd);
  return false;
#else
  my_error(ER_NOT_SUPPORTED_YET, MYF(0), "DROP TYPE");
  return true;
#endif
}
