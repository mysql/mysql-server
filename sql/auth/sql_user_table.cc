/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.

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

#include "sql/auth/sql_user_table.h"

#include "my_config.h"

#include <stddef.h>
#include <string.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <sys/types.h>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>

#include "lex_string.h"
#include "m_ctype.h"
#include "m_string.h" /* STRING_WITH_LEN */
#include "map_helpers.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_dbug.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/components/services/log_shared.h"
#include "mysql/psi/mysql_statement.h"
#include "mysql_com.h"
#include "mysql_time.h"
#include "mysqld_error.h"
#include "sql/auth/acl_change_notification.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_common.h"
#include "sql/auth/auth_internal.h"
#include "sql/auth/sql_auth_cache.h"
#include "sql/auth/sql_authentication.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/binlog.h" /* mysql_bin_log.is_open() */
#include "sql/debug_sync.h"
#include "sql/error_handler.h" /* Internal_error_handler */
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item_func.h" /* mqh_used */
#include "sql/key.h"       /* key_copy, key_cmp_if_same */
                           /* key_restore */
#include "sql/log.h"       /* log_*() */
#include "sql/mdl.h"
#include "sql/mysqld.h"
#include "sql/rpl_filter.h" /* rpl_filter */
#include "sql/rpl_rli.h"    /* class Relay_log_info */
#include "sql/sql_base.h"   /* close_thread_tables */
#include "sql/sql_class.h"
#include "sql/sql_connect.h"
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
/* trans_commit_implicit */
#include "sql/sql_parse.h" /* stmt_causes_implicit_commit */
#include "sql/sql_rewrite.h"
#include "sql/sql_table.h"  /* write_bin_log */
#include "sql/sql_update.h" /* compare_records */
#include "sql/system_variables.h"
#include "sql/table.h"       /* TABLE_FIELD_TYPE */
#include "sql/transaction.h" /* trans_commit_stmt */
#include "sql/tztime.h"
#include "sql_string.h"
#include "thr_lock.h"
#include "typelib.h"
#include "violite.h"

/* Acl table names. Keep in sync with ACL_TABLES */
static const int MAX_ACL_TABLE_NAMES = 10;
static_assert(MAX_ACL_TABLE_NAMES == ACL_TABLES::LAST_ENTRY,
              "Keep number of table names in sync with ACL table enum");

static const char *ACL_TABLE_NAMES[MAX_ACL_TABLE_NAMES] = {
    "user",          "db",
    "tables_priv",   "columns_priv",
    "procs_priv",    "proxies_priv",
    "role_edges",    "default_roles",
    "global_grants", "password_history"};

static const TABLE_FIELD_TYPE mysql_db_table_fields[MYSQL_DB_FIELD_COUNT] = {
    {{STRING_WITH_LEN("Host")}, {STRING_WITH_LEN("char(255)")}, {nullptr, 0}},
    {{STRING_WITH_LEN("Db")}, {STRING_WITH_LEN("char(64)")}, {nullptr, 0}},
    {{STRING_WITH_LEN("User")},
     {STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")")},
     {nullptr, 0}},
    {{STRING_WITH_LEN("Select_priv")},
     {STRING_WITH_LEN("enum('N','Y')")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("Insert_priv")},
     {STRING_WITH_LEN("enum('N','Y')")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("Update_priv")},
     {STRING_WITH_LEN("enum('N','Y')")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("Delete_priv")},
     {STRING_WITH_LEN("enum('N','Y')")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("Create_priv")},
     {STRING_WITH_LEN("enum('N','Y')")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("Drop_priv")},
     {STRING_WITH_LEN("enum('N','Y')")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("Grant_priv")},
     {STRING_WITH_LEN("enum('N','Y')")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("References_priv")},
     {STRING_WITH_LEN("enum('N','Y')")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("Index_priv")},
     {STRING_WITH_LEN("enum('N','Y')")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("Alter_priv")},
     {STRING_WITH_LEN("enum('N','Y')")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("Create_tmp_table_priv")},
     {STRING_WITH_LEN("enum('N','Y')")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("Lock_tables_priv")},
     {STRING_WITH_LEN("enum('N','Y')")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("Create_view_priv")},
     {STRING_WITH_LEN("enum('N','Y')")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("Show_view_priv")},
     {STRING_WITH_LEN("enum('N','Y')")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("Create_routine_priv")},
     {STRING_WITH_LEN("enum('N','Y')")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("Alter_routine_priv")},
     {STRING_WITH_LEN("enum('N','Y')")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("Execute_priv")},
     {STRING_WITH_LEN("enum('N','Y')")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("Event_priv")},
     {STRING_WITH_LEN("enum('N','Y')")},
     {STRING_WITH_LEN("utf8mb3")}},
    {{STRING_WITH_LEN("Trigger_priv")},
     {STRING_WITH_LEN("enum('N','Y')")},
     {STRING_WITH_LEN("utf8mb3")}}};

static const TABLE_FIELD_TYPE mysql_user_table_fields[MYSQL_USER_FIELD_COUNT] =
    {{{STRING_WITH_LEN("Host")}, {STRING_WITH_LEN("char(255)")}, {nullptr, 0}},
     {{STRING_WITH_LEN("User")},
      {STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")")},
      {nullptr, 0}},
     {{STRING_WITH_LEN("Select_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Insert_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Update_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Delete_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Create_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Drop_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Reload_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Shutdown_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Process_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("File_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Grant_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("References_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Index_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Alter_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Show_db_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Super_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Create_tmp_table_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Lock_tables_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Execute_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Repl_slave_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Repl_client_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Create_view_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Show_view_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Create_routine_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Alter_routine_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Create_user_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Event_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Trigger_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Create_tablespace_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("ssl_type")},
      {STRING_WITH_LEN("enum('','ANY','X509','SPECIFIED')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("ssl_cipher")}, {STRING_WITH_LEN("blob")}, {nullptr, 0}},
     {{STRING_WITH_LEN("x509_issuer")},
      {STRING_WITH_LEN("blob")},
      {nullptr, 0}},
     {{STRING_WITH_LEN("x509_subject")},
      {STRING_WITH_LEN("blob")},
      {nullptr, 0}},
     {{STRING_WITH_LEN("max_questions")},
      {STRING_WITH_LEN("int")},
      {nullptr, 0}},
     {{STRING_WITH_LEN("max_updates")}, {STRING_WITH_LEN("int")}, {nullptr, 0}},
     {{STRING_WITH_LEN("max_connections")},
      {STRING_WITH_LEN("int")},
      {nullptr, 0}},
     {{STRING_WITH_LEN("max_user_connections")},
      {STRING_WITH_LEN("int")},
      {nullptr, 0}},
     {{STRING_WITH_LEN("plugin")}, {STRING_WITH_LEN("char(64)")}, {nullptr, 0}},
     {{STRING_WITH_LEN("authentication_string")},
      {STRING_WITH_LEN("text")},
      {nullptr, 0}},
     {{STRING_WITH_LEN("password_expired")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("password_last_changed")},
      {STRING_WITH_LEN("timestamp")},
      {nullptr, 0}},
     {{STRING_WITH_LEN("password_lifetime")},
      {STRING_WITH_LEN("smallint")},
      {nullptr, 0}},
     {{STRING_WITH_LEN("account_locked")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Create_role_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Drop_role_priv")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("Password_reuse_history")},
      {STRING_WITH_LEN("smallint")},
      {nullptr, 0}},
     {{STRING_WITH_LEN("Password_reuse_time")},
      {STRING_WITH_LEN("smallint")},
      {nullptr, 0}},
     {{STRING_WITH_LEN("Password_require_current")},
      {STRING_WITH_LEN("enum('N','Y')")},
      {STRING_WITH_LEN("utf8mb3")}},
     {{STRING_WITH_LEN("User_attributes")},
      {STRING_WITH_LEN("json")},
      {nullptr, 0}}};

static const TABLE_FIELD_TYPE
    mysql_proxies_priv_table_fields[MYSQL_PROXIES_PRIV_FIELD_COUNT] = {
        {{STRING_WITH_LEN("Host")},
         {STRING_WITH_LEN("char(255)")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("User")},
         {STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("Proxied_host")},
         {STRING_WITH_LEN("char(255)")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("Proxied_user")},
         {STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("With_grant")},
         {STRING_WITH_LEN("tinyint")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("Grantor")},
         {STRING_WITH_LEN("varchar(288)")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("Timestamp")},
         {STRING_WITH_LEN("timestamp")},
         {nullptr, 0}}};

static const TABLE_FIELD_TYPE
    mysql_procs_priv_table_fields[MYSQL_PROCS_PRIV_FIELD_COUNT] = {
        {{STRING_WITH_LEN("Host")},
         {STRING_WITH_LEN("char(255)")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("Db")}, {STRING_WITH_LEN("char(64)")}, {nullptr, 0}},
        {{STRING_WITH_LEN("User")},
         {STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("Routine_name")},
         {STRING_WITH_LEN("char(64)")},
         {STRING_WITH_LEN("utf8mb3")}},
        {{STRING_WITH_LEN("Routine_type")},
         {STRING_WITH_LEN("enum('FUNCTION','PROCEDURE')")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("Grantor")},
         {STRING_WITH_LEN("varchar(288)")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("Proc_priv")},
         {STRING_WITH_LEN("set('Execute','Alter Routine','Grant')")},
         {STRING_WITH_LEN("utf8mb3")}},
        {{STRING_WITH_LEN("Timestamp")},
         {STRING_WITH_LEN("timestamp")},
         {nullptr, 0}}};

static const TABLE_FIELD_TYPE
    mysql_columns_priv_table_fields[MYSQL_COLUMNS_PRIV_FIELD_COUNT] = {
        {{STRING_WITH_LEN("Host")},
         {STRING_WITH_LEN("char(255)")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("Db")}, {STRING_WITH_LEN("char(64)")}, {nullptr, 0}},
        {{STRING_WITH_LEN("User")},
         {STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("Table_name")},
         {STRING_WITH_LEN("char(64)")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("Column_name")},
         {STRING_WITH_LEN("char(64)")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("Timestamp")},
         {STRING_WITH_LEN("timestamp")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("Column_priv")},
         {STRING_WITH_LEN("set('Select','Insert','Update','References')")},
         {STRING_WITH_LEN("utf8mb3")}}};

static const TABLE_FIELD_TYPE
    mysql_tables_priv_table_fields[MYSQL_TABLES_PRIV_FIELD_COUNT] = {
        {{STRING_WITH_LEN("Host")},
         {STRING_WITH_LEN("char(255)")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("Db")}, {STRING_WITH_LEN("char(64)")}, {nullptr, 0}},
        {{STRING_WITH_LEN("User")},
         {STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("Table_name")},
         {STRING_WITH_LEN("char(64)")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("Grantor")},
         {STRING_WITH_LEN("varchar(288)")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("Timestamp")},
         {STRING_WITH_LEN("timestamp")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("Table_priv")},
         {STRING_WITH_LEN("set('Select','Insert','Update','Delete','Create',"
                          "'Drop','Grant','References','Index','Alter',"
                          "'Create View','Show view','Trigger')")},
         {STRING_WITH_LEN("utf8mb3")}},
        {{STRING_WITH_LEN("Column_priv")},
         {STRING_WITH_LEN("set('Select','Insert','Update','References')")},
         {STRING_WITH_LEN("utf8mb3")}}};

static const TABLE_FIELD_TYPE
    mysql_role_edges_table_fields[MYSQL_ROLE_EDGES_FIELD_COUNT] = {
        {{STRING_WITH_LEN("FROM_HOST")},
         {STRING_WITH_LEN("char(255)")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("FROM_USER")},
         {STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("TO_HOST")},
         {STRING_WITH_LEN("char(255)")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("TO_USER")},
         {STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("WITH_ADMIN_OPTION")},
         {STRING_WITH_LEN("enum('N','Y')")},
         {STRING_WITH_LEN("utf8mb3")}}};

static const TABLE_FIELD_TYPE
    mysql_default_roles_table_fields[MYSQL_DEFAULT_ROLES_FIELD_COUNT] = {
        {{STRING_WITH_LEN("HOST")},
         {STRING_WITH_LEN("char(255)")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("USER")},
         {STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("DEFAULT_ROLE_HOST")},
         {STRING_WITH_LEN("char(255)")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("DEFAULT_ROLE_USER")},
         {STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")")},
         {nullptr, 0}}};

static const TABLE_FIELD_TYPE
    mysql_password_history_table_fields[MYSQL_PASSWORD_HISTORY_FIELD_COUNT] = {
        {{STRING_WITH_LEN("Host")},
         {STRING_WITH_LEN("char(255)")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("User")},
         {STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("Password_timestamp")},
         {STRING_WITH_LEN("timestamp")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("Password")},
         {STRING_WITH_LEN("text")},
         {nullptr, 0}}};

static const TABLE_FIELD_TYPE
    mysql_dynamic_priv_table_fields[MYSQL_DYNAMIC_PRIV_FIELD_COUNT] = {
        {{STRING_WITH_LEN("USER")},
         {STRING_WITH_LEN("char(" USERNAME_CHAR_LENGTH_STR ")")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("HOST")},
         {STRING_WITH_LEN("char(255)")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("PRIV")},
         {STRING_WITH_LEN("char(32)")},
         {nullptr, 0}},
        {{STRING_WITH_LEN("WITH_GRANT_OPTION")},
         {STRING_WITH_LEN("enum('N','Y')")},
         {nullptr, 0}}};

/** keep in sync with @ref ACL_TABLES */
const TABLE_FIELD_DEF Acl_table_intact::mysql_acl_table_defs[] = {
    {MYSQL_USER_FIELD_COUNT, mysql_user_table_fields},
    {MYSQL_DB_FIELD_COUNT, mysql_db_table_fields},
    {MYSQL_TABLES_PRIV_FIELD_COUNT, mysql_tables_priv_table_fields},
    {MYSQL_COLUMNS_PRIV_FIELD_COUNT, mysql_columns_priv_table_fields},
    {MYSQL_PROCS_PRIV_FIELD_COUNT, mysql_procs_priv_table_fields},
    {MYSQL_PROXIES_PRIV_FIELD_COUNT, mysql_proxies_priv_table_fields},
    {MYSQL_ROLE_EDGES_FIELD_COUNT, mysql_role_edges_table_fields},
    {MYSQL_DEFAULT_ROLES_FIELD_COUNT, mysql_default_roles_table_fields},
    {MYSQL_DYNAMIC_PRIV_FIELD_COUNT, mysql_dynamic_priv_table_fields},
    {MYSQL_PASSWORD_HISTORY_FIELD_COUNT, mysql_password_history_table_fields}};

static bool acl_tables_setup_for_write_and_acquire_mdl(THD *thd,
                                                       Table_ref *tables);
/**
  A helper function to commit statement transaction and close
  ACL tables after reading some data from them as part of FLUSH
  PRIVILEGES statement or during server initialization.

  @note We assume that we have only read from the tables so commit
        can't fail. @sa close_mysql_tables().

  @note This function also rollbacks the transaction if rollback was
        requested (e.g. as result of deadlock).
*/
void commit_and_close_mysql_tables(THD *thd) {
  /* Transaction rollback request by SE is unlikely. Still we handle it. */
  if (thd->transaction_rollback_request) {
    trans_rollback_stmt(thd);
    trans_rollback_implicit(thd);
  } else {
#ifndef NDEBUG
    bool res =
#endif
        /*
          In @@autocommit=0 mode we have both statement and multi-statement
          transactions started here. Since we don't want storage engine locks
          to stay around after metadata locks are released by
          close_mysql_tables() we need to do implicit commit here.
        */
        trans_commit_stmt(thd) || trans_commit_implicit(thd);
    assert(res == false);
  }

  close_mysql_tables(thd);
}

extern bool initialized;

/*
  Get all access bits from table after fieldnr

  IMPLEMENTATION
  We know that the access privileges ends when there is no more fields
  or the field is not an enum with two elements.

  SYNOPSIS
    get_access()
    form        an open table to read privileges from.
                The record should be already read in table->record[0]
    fieldnr     number of the first privilege (that is ENUM('N','Y') field
    next_field  on return - number of the field next to the last ENUM
                (unless next_field == 0)

  RETURN VALUE
    privilege mask
*/

ulong get_access(TABLE *form, uint fieldnr, uint *next_field) {
  ulong access_bits = 0, bit;
  char buff[2];
  String res(buff, sizeof(buff), &my_charset_latin1);
  Field **pos;

  for (pos = form->field + fieldnr, bit = 1;
       *pos && (*pos)->real_type() == MYSQL_TYPE_ENUM &&
       ((Field_enum *)(*pos))->typelib->count == 2;
       pos++, fieldnr++, bit <<= 1) {
    (*pos)->val_str(&res);
    if (my_toupper(&my_charset_latin1, res[0]) == 'Y') access_bits |= bit;
  }
  if (next_field) *next_field = fieldnr;
  return access_bits;
}

/**

  Notify handlerton(s) that privileges have changed

  Interested handlertons may use this notification to update
  its own privilege structures as well as propagating
  the changing query to other destinations.

*/
Acl_change_notification::Acl_change_notification(
    THD *thd, enum_sql_command op, const List<LEX_USER> *users,
    std::set<LEX_USER *> *rewrite_users, const List<LEX_CSTRING> *dynamic_privs)
    : db{thd->db().str, thd->db().length},
      operation(op),
      users(users ? *users : empty_users),
      rewrite_user_params(rewrite_users),
      dynamic_privs(dynamic_privs ? *dynamic_privs : empty_dynamic_privs) {}

void acl_notify_htons(THD *thd, enum_sql_command operation,
                      const List<LEX_USER> *users,
                      std::set<LEX_USER *> *rewrite_users,
                      const List<LEX_CSTRING> *dynamic_privs) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("db: %s query: '%s'", thd->db().str, thd->query().str));
  Acl_change_notification notice(thd, operation, users, rewrite_users,
                                 dynamic_privs);
  ha_acl_notify(thd, &notice);
}

/**
  Commit or rollback ACL statement (and transaction),
  close tables which it has opened and release metadata locks.

  @note In case of failure to commit transaction we try to restore correct
        state of in-memory structures by reloading privileges.

  @retval False - Success.
  @retval True  - Error.
*/

static bool acl_end_trans_and_close_tables(THD *thd,
                                           bool rollback_transaction) {
  bool result;

  /*
    Try to commit a transaction even if we had some failures.

    Without this step changes to privilege tables will be rolled back at the
    end of mysql_execute_command() in the presence of error, leaving on-disk
    and in-memory descriptions of privileges out of sync and making behavior
    of ACL statements for transactional tables incompatible with legacy
    behavior.

    We need to commit both statement and normal transaction to make behavior
    consistent with both autocommit on and off.

    It is safe to do so since ACL statement always do implicit commit at the
    end of statement.
  */
  assert(stmt_causes_implicit_commit(thd, CF_IMPLICIT_COMMIT_END));

  /*
    ACL DDL operations must acquire IX Backup Lock in order to be mutually
    exclusive with LOCK INSTANCE FOR BACKUP.
  */
  assert(thd->mdl_context.owns_equal_or_stronger_lock(
      MDL_key::BACKUP_LOCK, "", "", MDL_INTENTION_EXCLUSIVE));

  if (rollback_transaction) {
    /*
      Transaction rollback request by SE is unlikely. Still let us
      handle it and also do ACL reload if it happens.
    */
    result = trans_rollback_stmt(thd);
    result |= trans_rollback_implicit(thd);
  } else {
    result = trans_commit_stmt(thd);
    result |= trans_commit_implicit(thd);
  }
  close_thread_tables(thd);
  if (result || rollback_transaction) {
    /*
      Try to bring in-memory structures back in sync with on-disk data if we
      have failed to commit our changes.
    */
    DBUG_EXECUTE_IF("wl14084_acl_ddl_error_before_cache_reload_trigger_timeout",
                    sleep(2););
    reload_acl_caches(thd, true);
    close_thread_tables(thd);
  }
  thd->mdl_context.release_transactional_locks();

  return result;
}

/*
  Function to handle rewriting and bin logging of ACL ddl.
  Assumption : Error if any has already been raised.

  Effect : In case of rollback, acl caches will be reloaded.

  @param thd                    Handle to THD object.
                                Required for query rewriting
  @param transactional_table    Nature of ACL tables
  @param extra_users            Users which were not processed
  @param rewrite_params         Information required for query rewrite
  @param extra error            Used in cases where error handler
                                is suppressed.
  @param write_to_binlog        Skip writing to binlog.
                                Used for routine grants while
                                creating routine.

  @returns status of log and commit
    @retval 0 Successfully committed. Optionally : written to binlog.
    @retval 1 If an error is raised at any stage
*/

bool log_and_commit_acl_ddl(THD *thd, bool transactional_tables,
                            std::set<LEX_USER *> *extra_users, /* = NULL */
                            Rewrite_params *rewrite_params,    /* = NULL */
                            bool extra_error,                  /* = true */
                            bool write_to_binlog) {            /* = true */
  bool result = false;
  DBUG_TRACE;
  assert(thd);
  result = thd->is_error() || extra_error || thd->transaction_rollback_request;
  /* Write to binlog and textlogs only if there is no error */
  if (!result) {
    String rlb;
    /*
      We're requesting a rewrite with instrumentation. This will change
      the value on the THD and those seen in instrumentation.
    */
    mysql_rewrite_acl_query(thd, rlb, Consumer_type::BINLOG, rewrite_params);
    if (write_to_binlog) {
      LEX_CSTRING query;
      enum_sql_command command;
      size_t num_extra_users = extra_users ? extra_users->size() : 0;
      command = thd->lex->sql_command;
      if (mysql_bin_log.is_open()) {
        query.str = thd->rewritten_query().length()
                        ? thd->rewritten_query().ptr()
                        : thd->query().str;

        query.length = thd->rewritten_query().length()
                           ? thd->rewritten_query().length()
                           : thd->query().length;

        /* Write to binary log */
        result = (write_bin_log(thd, false, query.str, query.length,
                                transactional_tables) != 0)
                     ? true
                     : false;
        /*
          Log warning about extra users in case of
          CREATE USER IF NOT EXISTS/ALTER USER IF EXISTS
        */
        if ((command == SQLCOM_CREATE_USER || command == SQLCOM_ALTER_USER) &&
            !result && num_extra_users) {
          String warn_user;
          bool comma = false;
          bool log_warning = false;
          for (LEX_USER *extra_user : *extra_users) {
            /*
              Consider for warning if one of the following is true:
              1. If SQLCOM_CREATE_USER, IDENTIFIED WITH clause is not used
              2. If SQLCOM_ALTER_USER, IDENTIFIED WITH clause is not used
              but IDENTIFIED BY is used.
            */
            if (!extra_user->first_factor_auth_info
                     .uses_identified_with_clause &&
                (command == SQLCOM_CREATE_USER ||
                 extra_user->first_factor_auth_info
                     .uses_identified_by_clause)) {
              log_user(thd, &warn_user, extra_user, comma);
              comma = true;
              log_warning = true;
            }
          }
          if (log_warning)
            LogErr(WARNING_LEVEL,
                   (command == SQLCOM_CREATE_USER)
                       ? ER_SQL_USER_TABLE_CREATE_WARNING
                       : ER_SQL_USER_TABLE_ALTER_WARNING,
                   default_auth_plugin_name.str, warn_user.c_ptr_safe());

          warn_user.mem_free();
        }
      }
    }
    /*
      Rewrite query in the thd again for the consistent logging for all consumer
      type TEXTLOG later on. For instance: Audit logs.

      We're requesting a rewrite with instrumentation. This will change
      (back) the value on the THD and those seen in instrumentation.
    */
    mysql_rewrite_acl_query(thd, rlb, Consumer_type::TEXTLOG, rewrite_params);
  }

  if (acl_end_trans_and_close_tables(thd, result)) result = true;

  return result;
}

static void get_grantor(THD *thd, char *grantor) {
  const char *user = thd->security_context()->user().str;
  const char *host = thd->security_context()->host_or_ip().str;

  if (thd->slave_thread && thd->has_invoker()) {
    user = thd->get_invoker_user().str;
    host = thd->get_invoker_host().str;
  }
  strxmov(grantor, user, "@", host, NullS);
}

/**
  Take a handler error and generate the mysql error ER_ACL_OPERATION_FAILED
  containing original text of HA error.

  @param  handler_error  an error number resulted from storage engine
*/

void acl_print_ha_error(int handler_error) {
  char buffer[MYSYS_ERRMSG_SIZE];

  my_strerror(buffer, sizeof(buffer), handler_error);
  my_error(ER_ACL_OPERATION_FAILED, MYF(0), handler_error, buffer);
}

/**
  change grants in the mysql.db table.

  @param thd          Current thread execution context.
  @param table        Pointer to a TABLE object for opened mysql.db table.
  @param db           Database name of table for which column privileges are
                      modified.
  @param combo        Pointer to a LEX_USER object containing info about a user
                      being processed.
  @param rights       Database level grant.
  @param revoke_grant Set to true if this is a REVOKE command.

  @return  Operation result
    @retval  0    OK.
    @retval  1    Error in handling current user entry but still can continue
                  processing subsequent user specified in the ACL statement.
    @retval  < 0  Error.
*/

int replace_db_table(THD *thd, TABLE *table, const char *db,
                     const LEX_USER &combo, ulong rights, bool revoke_grant) {
  uint i;
  ulong priv, store_rights;
  bool old_row_exists = false;
  int error;
  char what = (revoke_grant) ? 'N' : 'Y';
  uchar user_key[MAX_KEY_LENGTH];
  Acl_table_intact table_intact(thd);
  DBUG_TRACE;
  assert(initialized);
  if (table_intact.check(table, ACL_TABLES::TABLE_DB)) return -1;

  /* Check if there is such a user in user table in memory? */
  if (!find_acl_user(combo.host.str, combo.user.str, false)) {
    my_error(ER_PASSWORD_NO_MATCH, MYF(0));
    return 1;
  }

  table->use_all_columns();
  table->field[0]->store(combo.host.str, combo.host.length,
                         system_charset_info);
  table->field[1]->store(db, strlen(db), system_charset_info);
  table->field[2]->store(combo.user.str, combo.user.length,
                         system_charset_info);
  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);
  error = table->file->ha_index_read_idx_map(table->record[0], 0, user_key,
                                             HA_WHOLE_KEY, HA_READ_KEY_EXACT);
  assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
         error != HA_ERR_LOCK_DEADLOCK);
  assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
         error != HA_ERR_LOCK_WAIT_TIMEOUT);
  DBUG_EXECUTE_IF("wl7158_replace_db_table_1", error = HA_ERR_LOCK_DEADLOCK;);
  if (error) {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      goto table_error;

    if (what == 'N') {  // no row, no revoke
      /*
        Return 1 as an indication that expected error occurred during
        handling of REVOKE statement for an unknown user.
      */
      if (report_missing_user_grant_message(thd, true, combo.user.str,
                                            combo.host.str, nullptr,
                                            ER_NONEXISTING_GRANT))
        return 1;
      else
        return 0;
    }
    old_row_exists = false;
    restore_record(table, s->default_values);
    table->field[0]->store(combo.host.str, combo.host.length,
                           system_charset_info);
    table->field[1]->store(db, strlen(db), system_charset_info);
    table->field[2]->store(combo.user.str, combo.user.length,
                           system_charset_info);
  } else {
    old_row_exists = true;
    store_record(table, record[1]);
  }

  store_rights = get_rights_for_db(rights);
  for (i = 3, priv = 1; i < table->s->fields; i++, priv <<= 1) {
    if (priv & store_rights)  // do it if priv is chosen
      table->field[i]->store(&what, 1,
                             &my_charset_latin1);  // set requested privileges
  }
  rights = get_access(table, 3, nullptr);
  rights = fix_rights_for_db(rights);

  if (old_row_exists) {
    /* update old existing row */
    if (rights) {
      error = table->file->ha_update_row(table->record[1], table->record[0]);
      assert(error != HA_ERR_FOUND_DUPP_KEY);
      assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
             error != HA_ERR_LOCK_DEADLOCK);
      assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
             error != HA_ERR_LOCK_WAIT_TIMEOUT);
      DBUG_EXECUTE_IF("wl7158_replace_db_table_2",
                      error = HA_ERR_LOCK_DEADLOCK;);
      if (error && error != HA_ERR_RECORD_IS_THE_SAME) goto table_error;
    } else /* must have been a revoke of all privileges */
    {
      error = table->file->ha_delete_row(table->record[1]);
      assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
             error != HA_ERR_LOCK_DEADLOCK);
      assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
             error != HA_ERR_LOCK_WAIT_TIMEOUT);
      DBUG_EXECUTE_IF("wl7158_replace_db_table_3",
                      error = HA_ERR_LOCK_DEADLOCK;);
      if (error) goto table_error; /* purecov: deadcode */
    }
  } else if (rights) {
    error = table->file->ha_write_row(table->record[0]);
    assert(error != HA_ERR_FOUND_DUPP_KEY);
    assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
           error != HA_ERR_LOCK_DEADLOCK);
    assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
           error != HA_ERR_LOCK_WAIT_TIMEOUT);
    DBUG_EXECUTE_IF("wl7158_replace_db_table_4", error = HA_ERR_LOCK_DEADLOCK;);
    if (error) {
      if (!table->file->is_ignorable_error(error))
        goto table_error; /* purecov: deadcode */
    }
  }

  clear_and_init_db_cache();  // Clear privilege cache
  if (old_row_exists)
    acl_update_db(combo.user.str, combo.host.str, db, rights);
  else if (rights)
    acl_insert_db(combo.user.str, combo.host.str, db, rights);
  return 0;

table_error:
  acl_print_ha_error(error);

  return -1;
}

/**
  Insert, update or remove a record in the mysql.proxies_priv table.

  @param thd             The current thread.
  @param table           Pointer to a TABLE object for opened
                         mysql.proxies_priv table.
  @param user            Information about user being handled.
  @param proxied_user    Information about proxied user being handled.
  @param with_grant_arg  True if a  user is allowed to execute GRANT,
                         else false.
  @param revoke_grant    Set to true if this is REVOKE command.

  @return  Operation result.
  @retval  0    OK.
  @retval  1    Error in handling current user entry but still can continue
                processing subsequent user specified in the ACL statement.
  @retval  < 0  Error.
*/

int replace_proxies_priv_table(THD *thd, TABLE *table, const LEX_USER *user,
                               const LEX_USER *proxied_user,
                               bool with_grant_arg, bool revoke_grant) {
  bool old_row_exists = false;
  int error;
  uchar user_key[MAX_KEY_LENGTH];
  ACL_PROXY_USER new_grant;
  char grantor[USER_HOST_BUFF_SIZE];
  Acl_table_intact table_intact(thd);

  DBUG_TRACE;
  assert(initialized);
  if (table_intact.check(table, ACL_TABLES::TABLE_PROXIES_PRIV)) return -1;

  /* Check if there is such a user in user table in memory? */
  if (!find_acl_user(user->host.str, user->user.str, false)) {
    my_error(ER_PASSWORD_NO_MATCH, MYF(0));
    return 1;
  }

  table->use_all_columns();
  ACL_PROXY_USER::store_pk(table, user->host, user->user, proxied_user->host,
                           proxied_user->user);

  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);

  get_grantor(thd, grantor);
  error = table->file->ha_index_init(0, true);
  DBUG_EXECUTE_IF("wl7158_replace_proxies_priv_table_1",
                  table->file->ha_index_end();
                  error = HA_ERR_LOCK_DEADLOCK;);
  if (error) {
    acl_print_ha_error(error);
    DBUG_PRINT("info", ("ha_index_init error"));
    return -1;
  }

  error = table->file->ha_index_read_map(table->record[0], user_key,
                                         HA_WHOLE_KEY, HA_READ_KEY_EXACT);
  assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
         error != HA_ERR_LOCK_DEADLOCK);
  assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
         error != HA_ERR_LOCK_WAIT_TIMEOUT);
  DBUG_EXECUTE_IF("wl7158_replace_proxies_priv_table_2",
                  error = HA_ERR_LOCK_DEADLOCK;);
  if (error) {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      goto table_error;
    DBUG_PRINT("info", ("Row not found"));
    if (revoke_grant) {  // no row, no revoke
      table->file->ha_index_end();
      if (report_missing_user_grant_message(thd, true, user->user.str,
                                            user->host.str, nullptr,
                                            ER_NONEXISTING_GRANT))
        return 1;
      else
        return 0;
    }
    old_row_exists = false;
    restore_record(table, s->default_values);
    ACL_PROXY_USER::store_data_record(table, user->host, user->user,
                                      proxied_user->host, proxied_user->user,
                                      with_grant_arg, grantor);
  } else {
    DBUG_PRINT("info", ("Row found"));
    old_row_exists = true;
    store_record(table, record[1]);  // copy original row
    ACL_PROXY_USER::store_with_grant(table, with_grant_arg);
  }

  if (old_row_exists) {
    /* update old existing row */
    if (!revoke_grant) {
      error = table->file->ha_update_row(table->record[1], table->record[0]);
      assert(error != HA_ERR_FOUND_DUPP_KEY);
      assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
             error != HA_ERR_LOCK_DEADLOCK);
      assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
             error != HA_ERR_LOCK_WAIT_TIMEOUT);
      DBUG_EXECUTE_IF("wl7158_replace_proxies_priv_table_3",
                      error = HA_ERR_LOCK_DEADLOCK;);
      if (error && error != HA_ERR_RECORD_IS_THE_SAME) goto table_error;
    } else {
      error = table->file->ha_delete_row(table->record[1]);
      assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
             error != HA_ERR_LOCK_DEADLOCK);
      assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
             error != HA_ERR_LOCK_WAIT_TIMEOUT);
      DBUG_EXECUTE_IF("wl7158_replace_proxies_priv_table_4",
                      error = HA_ERR_LOCK_DEADLOCK;);
      if (error) goto table_error;
    }
  } else {
    error = table->file->ha_write_row(table->record[0]);
    assert(error != HA_ERR_FOUND_DUPP_KEY);
    assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
           error != HA_ERR_LOCK_DEADLOCK);
    assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
           error != HA_ERR_LOCK_WAIT_TIMEOUT);
    DBUG_EXECUTE_IF("wl7158_replace_proxies_priv_table_5",
                    error = HA_ERR_LOCK_DEADLOCK;);
    if (error) {
      DBUG_PRINT("info", ("error inserting the row"));
      if (!table->file->is_ignorable_error(error)) goto table_error;
    }
  }

  clear_and_init_db_cache();  // Clear privilege cache
  if (old_row_exists) {
    new_grant.init(user->host.str, user->user.str, proxied_user->host.str,
                   proxied_user->user.str, with_grant_arg);
    acl_update_proxy_user(&new_grant, revoke_grant);
  } else {
    new_grant.init(&global_acl_memory, user->host.str, user->user.str,
                   proxied_user->host.str, proxied_user->user.str,
                   with_grant_arg);
    acl_insert_proxy_user(&new_grant);
  }

  table->file->ha_index_end();
  return 0;

  /* This could only happen if the grant tables got corrupted */
table_error:
  DBUG_PRINT("info", ("table error"));
  acl_print_ha_error(error);

  DBUG_PRINT("info", ("aborting replace_proxies_priv_table"));
  table->file->ha_index_end();
  return -1;
}

/**
  Update record in the table mysql.columns_priv

  @param thd          Current thread execution context.
  @param g_t          Pointer to a cached table grant object
  @param table        Pointer to a TABLE object for open mysql.columns_priv
                      table
  @param combo        Pointer to a LEX_USER object containing info about a user
                      being processed
  @param columns      List of columns to give/revoke grant
  @param db           Database name of table for which column privileges are
                      modified
  @param table_name   Name of table for which column privileges are modified
  @param rights       Table level grant
  @param revoke_grant Set to true if this is a REVOKE command

  @return  Operation result
    @retval  0    OK.
    @retval  < 0  System error or storage engine error happen
    @retval  > 0  Error in handling current user entry but still can continue
                  processing subsequent user specified in the ACL statement.
*/

int replace_column_table(THD *thd, GRANT_TABLE *g_t, TABLE *table,
                         const LEX_USER &combo, List<LEX_COLUMN> &columns,
                         const char *db, const char *table_name, ulong rights,
                         bool revoke_grant) {
  int result = 0;
  int error;
  uchar key[MAX_KEY_LENGTH];
  uint key_prefix_length;
  Acl_table_intact table_intact(thd);
  DBUG_TRACE;

  if (table_intact.check(table, ACL_TABLES::TABLE_COLUMNS_PRIV)) return -1;

  KEY_PART_INFO *key_part = table->key_info->key_part;

  table->use_all_columns();
  table->field[0]->store(combo.host.str, combo.host.length,
                         system_charset_info);
  table->field[1]->store(db, strlen(db), system_charset_info);
  table->field[2]->store(combo.user.str, combo.user.length,
                         system_charset_info);
  table->field[3]->store(table_name, strlen(table_name), system_charset_info);

  /* Get length of 4 first key parts */
  key_prefix_length = (key_part[0].store_length + key_part[1].store_length +
                       key_part[2].store_length + key_part[3].store_length);
  key_copy(key, table->record[0], table->key_info, key_prefix_length);

  rights &= COL_ACLS;  // Only ACL for columns

  /* first fix privileges for all columns in column list */

  List_iterator<LEX_COLUMN> iter(columns);
  class LEX_COLUMN *column;
  error = table->file->ha_index_init(0, true);
  DBUG_EXECUTE_IF("wl7158_replace_column_table_1", table->file->ha_index_end();
                  error = HA_ERR_LOCK_DEADLOCK;);
  if (error) {
    acl_print_ha_error(error);
    return -1;
  }

  while ((column = iter++)) {
    ulong privileges = column->rights;
    bool old_row_exists = false;
    uchar user_key[MAX_KEY_LENGTH];

    key_restore(table->record[0], key, table->key_info, key_prefix_length);
    table->field[4]->store(column->column.ptr(), column->column.length(),
                           system_charset_info);
    /* Get key for the first 4 columns */
    key_copy(user_key, table->record[0], table->key_info,
             table->key_info->key_length);

    error = table->file->ha_index_read_map(table->record[0], user_key,
                                           HA_WHOLE_KEY, HA_READ_KEY_EXACT);
    assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
           error != HA_ERR_LOCK_DEADLOCK);
    assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
           error != HA_ERR_LOCK_WAIT_TIMEOUT);
    DBUG_EXECUTE_IF("wl7158_replace_column_table_2",
                    error = HA_ERR_LOCK_DEADLOCK;);
    if (error) {
      if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE) {
        acl_print_ha_error(error);
        result = -1;
        goto end;
      }

      if (revoke_grant) {
        if (report_missing_user_grant_message(thd, true, combo.user.str,
                                              combo.host.str, table_name,
                                              ER_NONEXISTING_TABLE_GRANT))
          result = 1;
        continue; /* purecov: inspected */
      }
      old_row_exists = false;
      restore_record(table, s->default_values);  // Get empty record
      key_restore(table->record[0], key, table->key_info, key_prefix_length);
      table->field[4]->store(column->column.ptr(), column->column.length(),
                             system_charset_info);
    } else {
      ulong tmp = (ulong)table->field[6]->val_int();
      tmp = fix_rights_for_column(tmp);

      if (revoke_grant)
        privileges = tmp & ~(privileges | rights);
      else
        privileges |= tmp;
      old_row_exists = true;
      store_record(table, record[1]);  // copy original row
    }

    my_timeval tm;
    tm = thd->query_start_timeval_trunc(0);
    table->field[5]->store_timestamp(&tm);

    table->field[6]->store((longlong)get_rights_for_column(privileges), true);

    if (old_row_exists) {
      GRANT_COLUMN *grant_column;
      if (privileges) {
        error = table->file->ha_update_row(table->record[1], table->record[0]);
        assert(error != HA_ERR_FOUND_DUPP_KEY);
        assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
               error != HA_ERR_LOCK_DEADLOCK);
        assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
               error != HA_ERR_LOCK_WAIT_TIMEOUT);
        DBUG_EXECUTE_IF("wl7158_replace_column_table_3",
                        error = HA_ERR_LOCK_DEADLOCK;);
        if (error && error != HA_ERR_RECORD_IS_THE_SAME) {
          acl_print_ha_error(error);
          result = -1;
          goto end;
        }
      } else {
        error = table->file->ha_delete_row(table->record[1]);
        assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
               error != HA_ERR_LOCK_DEADLOCK);
        assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
               error != HA_ERR_LOCK_WAIT_TIMEOUT);
        DBUG_EXECUTE_IF("wl7158_replace_column_table_4",
                        error = HA_ERR_LOCK_DEADLOCK;);
        if (error) {
          acl_print_ha_error(error);
          result = -1;
          goto end;
        }
      }
      grant_column = column_hash_search(g_t, column->column.ptr(),
                                        column->column.length());
      if (grant_column)                     // Should always be true
        grant_column->rights = privileges;  // Update hash
    } else                                  // new grant
    {
      GRANT_COLUMN *grant_column;
      error = table->file->ha_write_row(table->record[0]);
      assert(error != HA_ERR_FOUND_DUPP_KEY);
      assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
             error != HA_ERR_LOCK_DEADLOCK);
      assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
             error != HA_ERR_LOCK_WAIT_TIMEOUT);
      DBUG_EXECUTE_IF("wl7158_replace_column_table_5",
                      error = HA_ERR_LOCK_DEADLOCK;);
      if (error) {
        acl_print_ha_error(error);
        result = -1;
        goto end;
      }
      grant_column =
          new (thd->mem_root) GRANT_COLUMN(column->column, privileges);
      g_t->hash_columns.emplace(
          grant_column->column,
          unique_ptr_destroy_only<GRANT_COLUMN>(grant_column));
    }
  }

  /*
    If revoke of privileges on the table level, remove all such privileges
    for all columns
  */

  if (revoke_grant) {
    uchar user_key[MAX_KEY_LENGTH];
    key_copy(user_key, table->record[0], table->key_info, key_prefix_length);
    error = table->file->ha_index_read_map(table->record[0], user_key,
                                           (key_part_map)15, HA_READ_KEY_EXACT);
    assert(error != HA_ERR_FOUND_DUPP_KEY);
    assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
           error != HA_ERR_LOCK_DEADLOCK);
    assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
           error != HA_ERR_LOCK_WAIT_TIMEOUT);
    DBUG_EXECUTE_IF("wl7158_replace_column_table_6",
                    error = HA_ERR_LOCK_DEADLOCK;);
    if (error) {
      if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE) {
        acl_print_ha_error(error);
        result = -1;
      }
      goto end;
    }

    /* Scan through all rows with the same host,db,user and table */
    do {
      ulong privileges = (ulong)table->field[6]->val_int();
      privileges = fix_rights_for_column(privileges);
      store_record(table, record[1]);

      if (privileges & rights)  // is in this record the priv to be revoked ??
      {
        GRANT_COLUMN *grant_column = nullptr;
        char colum_name_buf[HOSTNAME_LENGTH + 1];
        String column_name(colum_name_buf, sizeof(colum_name_buf),
                           system_charset_info);

        privileges &= ~rights;
        table->field[6]->store((longlong)get_rights_for_column(privileges),
                               true);
        table->field[4]->val_str(&column_name);
        grant_column =
            column_hash_search(g_t, column_name.ptr(), column_name.length());
        if (privileges) {
          error =
              table->file->ha_update_row(table->record[1], table->record[0]);
          assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
                 error != HA_ERR_LOCK_DEADLOCK);
          assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
                 error != HA_ERR_LOCK_WAIT_TIMEOUT);
          if (error &&
              error != HA_ERR_RECORD_IS_THE_SAME) { /* purecov: deadcode */
            acl_print_ha_error(error);              // deadcode
            result = -1;                            /* purecov: deadcode */
            goto end;                               /* purecov: deadcode */
          }
          if (grant_column) grant_column->rights = privileges;  // Update hash
        } else {
          error = table->file->ha_delete_row(table->record[1]);
          assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
                 error != HA_ERR_LOCK_DEADLOCK);
          assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
                 error != HA_ERR_LOCK_WAIT_TIMEOUT);
          DBUG_EXECUTE_IF("wl7158_replace_column_table_7",
                          error = HA_ERR_LOCK_DEADLOCK;);
          if (error) {
            acl_print_ha_error(error);
            result = -1;
            goto end;
          }
          if (grant_column)
            erase_specific_element(&g_t->hash_columns, grant_column->column,
                                   grant_column);
        }
      }
      error = table->file->ha_index_next(table->record[0]);
      assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
             error != HA_ERR_LOCK_DEADLOCK);
      assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
             error != HA_ERR_LOCK_WAIT_TIMEOUT);
      DBUG_EXECUTE_IF("wl7158_replace_column_table_8",
                      error = HA_ERR_LOCK_DEADLOCK;);
      if (error) {
        if (error != HA_ERR_END_OF_FILE) {
          acl_print_ha_error(error);
          result = -1;
        }
        goto end;
      }
    } while (!key_cmp_if_same(table, key, 0, key_prefix_length));
  }

end:
  table->file->ha_index_end();
  return result;
}

/**
  Search and create/update a record for requested table privileges.

  @param thd     The current thread.
  @param grant_table  Cached info about table/columns privileges.
  @param deleted_grant_table  If non-nullptr and grant is removed from
    column cache, it is returned here instead of being destroyed.
  @param table   Pointer to a TABLE object for open mysql.tables_priv table.
  @param combo   User information.
  @param db      Database name of table to give grant.
  @param table_name  Name of table to give grant.
  @param rights  Table privileges to set/update.
  @param col_rights   Column privileges to set/update.
  @param revoke_grant  Set to true if a REVOKE command is executed.

  @return  Operation result
    @retval  0    OK.
    @retval  < 0  System error or storage engine error happen.
    @retval  1    No entry for request.

*/

int replace_table_table(THD *thd, GRANT_TABLE *grant_table,
                        std::unique_ptr<GRANT_TABLE, Destroy_only<GRANT_TABLE>>
                            *deleted_grant_table,
                        TABLE *table, const LEX_USER &combo, const char *db,
                        const char *table_name, ulong rights, ulong col_rights,
                        bool revoke_grant) {
  char grantor[USER_HOST_BUFF_SIZE];
  int old_row_exists = 1;
  int error = 0;
  ulong store_table_rights, store_col_rights;
  uchar user_key[MAX_KEY_LENGTH];
  Acl_table_intact table_intact(thd);
  DBUG_TRACE;

  if (table_intact.check(table, ACL_TABLES::TABLE_TABLES_PRIV)) return -1;

  get_grantor(thd, grantor);
  /*
    The following should always succeed as new users are created before
    this function is called!
  */
  if (!find_acl_user(combo.host.str, combo.user.str, false)) {
    my_error(ER_PASSWORD_NO_MATCH, MYF(0)); /* purecov: deadcode */
    return 1;                               /* purecov: deadcode */
  }

  table->use_all_columns();
  restore_record(table, s->default_values);  // Get empty record
  table->field[0]->store(combo.host.str, combo.host.length,
                         system_charset_info);
  table->field[1]->store(db, strlen(db), system_charset_info);
  table->field[2]->store(combo.user.str, combo.user.length,
                         system_charset_info);
  table->field[3]->store(table_name, strlen(table_name), system_charset_info);
  store_record(table, record[1]);  // store at pos 1
  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);
  error = table->file->ha_index_read_idx_map(table->record[0], 0, user_key,
                                             HA_WHOLE_KEY, HA_READ_KEY_EXACT);
  assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
         error != HA_ERR_LOCK_DEADLOCK);
  assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
         error != HA_ERR_LOCK_WAIT_TIMEOUT);
  DBUG_EXECUTE_IF("wl7158_replace_table_table_1",
                  error = HA_ERR_LOCK_DEADLOCK;);
  if (error) {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      goto table_error;
    /*
      The following should never happen as we first check the in memory
      grant tables for the user.  There is however always a small change that
      the user has modified the grant tables directly.
    */
    if (revoke_grant) {  // no row, no revoke
      if (report_missing_user_grant_message(thd, true, combo.user.str,
                                            combo.host.str, table_name,
                                            ER_NONEXISTING_TABLE_GRANT))
        return 1;
      else
        return 0;
    }
    old_row_exists = 0;
    restore_record(table, record[1]);  // Get saved record
  }

  store_table_rights = get_rights_for_table(rights);
  store_col_rights = get_rights_for_column(col_rights);
  if (old_row_exists) {
    ulong j, k;
    store_record(table, record[1]);
    j = (ulong)table->field[6]->val_int();
    k = (ulong)table->field[7]->val_int();

    if (revoke_grant) {
      /* column rights are already fixed in mysql_table_grant */
      store_table_rights = j & ~store_table_rights;
    } else {
      store_table_rights |= j;
      store_col_rights |= k;
    }
  }

  table->field[4]->store(grantor, strlen(grantor), system_charset_info);

  my_timeval tm;
  tm = thd->query_start_timeval_trunc(0);
  table->field[5]->store_timestamp(&tm);

  table->field[6]->store((longlong)store_table_rights, true);
  table->field[7]->store((longlong)store_col_rights, true);
  rights = fix_rights_for_table(store_table_rights);
  col_rights = fix_rights_for_column(store_col_rights);

  if (old_row_exists) {
    if (store_table_rights || store_col_rights) {
      error = table->file->ha_update_row(table->record[1], table->record[0]);
      assert(error != HA_ERR_FOUND_DUPP_KEY);
      assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
             error != HA_ERR_LOCK_DEADLOCK);
      assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
             error != HA_ERR_LOCK_WAIT_TIMEOUT);
      DBUG_EXECUTE_IF("wl7158_replace_table_table_2",
                      error = HA_ERR_LOCK_DEADLOCK;);
      if (error && error != HA_ERR_RECORD_IS_THE_SAME) goto table_error;
    } else {
      error = table->file->ha_delete_row(table->record[1]);
      assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
             error != HA_ERR_LOCK_DEADLOCK);
      assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
             error != HA_ERR_LOCK_WAIT_TIMEOUT);
      if (error) goto table_error; /* purecov: deadcode */
    }
  } else {
    error = table->file->ha_write_row(table->record[0]);
    assert(error != HA_ERR_FOUND_DUPP_KEY);
    assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
           error != HA_ERR_LOCK_DEADLOCK);
    assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
           error != HA_ERR_LOCK_WAIT_TIMEOUT);
    DBUG_EXECUTE_IF("wl7158_replace_table_table_3",
                    error = HA_ERR_LOCK_DEADLOCK;);
    if (!table->file->is_ignorable_error(error)) goto table_error;
  }

  if (rights | col_rights) {
    grant_table->privs = rights;
    grant_table->cols = col_rights;
  } else {
    // Find this specific grant in column_priv_hash.
    auto it_range = column_priv_hash->equal_range(grant_table->hash_key);
    for (auto it = it_range.first; it != it_range.second; ++it) {
      if (it->second.get() == grant_table) {
        if (deleted_grant_table != nullptr) {
          // Caller now takes ownership.
          *deleted_grant_table = std::move(it->second);
        }
        column_priv_hash->erase(it);
        break;
      }
    }
  }
  return 0;

table_error:
  acl_print_ha_error(error);
  return -1;
}

/**
  Search and create/update a record for the routine requested.

  @param thd     The current thread.
  @param grant_name  Cached info about stored routine.
  @param table   Pointer to a TABLE object for open mysql.procs_priv table.
  @param combo   User information.
  @param db      Database name for stored routine.
  @param routine_name  Name for stored routine.
  @param is_proc  True for stored procedure, false for stored function.
  @param rights  Rights requested.
  @param revoke_grant  Set to true if a REVOKE command is executed.

  @return  Operation result
    @retval  0    OK.
    @retval  < 0  System error or storage engine error happen
    @retval  > 0  Error in handling current routine entry but still can continue
                  processing subsequent user specified in the ACL statement.
*/

int replace_routine_table(THD *thd, GRANT_NAME *grant_name, TABLE *table,
                          const LEX_USER &combo, const char *db,
                          const char *routine_name, bool is_proc, ulong rights,
                          bool revoke_grant) {
  char grantor[USER_HOST_BUFF_SIZE];
  int old_row_exists = 1;
  int error = 0;
  ulong store_proc_rights;
  Acl_table_intact table_intact(thd);
  uchar user_key[MAX_KEY_LENGTH];
  DBUG_TRACE;

  if (!initialized) {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    return -1;
  }

  if (table_intact.check(table, ACL_TABLES::TABLE_PROCS_PRIV)) return -1;

  get_grantor(thd, grantor);
  /*
    New users are created before this function is called.

    There may be some cases where a routine's definer is removed but the
    routine remains.
  */

  table->use_all_columns();
  restore_record(table, s->default_values);  // Get empty record
  table->field[0]->store(combo.host.str, combo.host.length, &my_charset_latin1);
  table->field[1]->store(db, strlen(db), &my_charset_latin1);
  table->field[2]->store(combo.user.str, combo.user.length, &my_charset_latin1);
  table->field[3]->store(routine_name, strlen(routine_name),
                         &my_charset_latin1);
  table->field[4]->store((is_proc ? to_longlong(enum_sp_type::PROCEDURE)
                                  : to_longlong(enum_sp_type::FUNCTION)),
                         true);
  store_record(table, record[1]);  // store at pos 1
  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);
  error = table->file->ha_index_read_idx_map(table->record[0], 0, user_key,
                                             HA_WHOLE_KEY, HA_READ_KEY_EXACT);

  assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
         error != HA_ERR_LOCK_DEADLOCK);
  assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
         error != HA_ERR_LOCK_WAIT_TIMEOUT);
  DBUG_EXECUTE_IF("wl7158_replace_routine_table_1",
                  error = HA_ERR_LOCK_DEADLOCK;);
  if (error) {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      goto table_error;
    /*
      The following should never happen as we first check the in memory
      grant tables for the user.  There is however always a small change that
      the user has modified the grant tables directly.
    */
    if (revoke_grant) {  // no row, no revoke
      if (report_missing_user_grant_message(thd, true, combo.user.str,
                                            combo.host.str, routine_name,
                                            ER_NONEXISTING_PROC_GRANT))
        return 1;
      else
        return 0;
    }
    old_row_exists = 0;
    restore_record(table, record[1]);  // Get saved record
  }

  store_proc_rights = get_rights_for_procedure(rights);
  if (old_row_exists) {
    ulong j;
    store_record(table, record[1]);
    j = (ulong)table->field[6]->val_int();

    if (revoke_grant) {
      /* column rights are already fixed in mysql_table_grant */
      store_proc_rights = j & ~store_proc_rights;
    } else {
      store_proc_rights |= j;
    }
  }

  table->field[5]->store(grantor, strlen(grantor), &my_charset_latin1);
  table->field[6]->store((longlong)store_proc_rights, true);

  my_timeval tm;
  tm = thd->query_start_timeval_trunc(0);
  table->field[7]->store_timestamp(&tm);

  rights = fix_rights_for_procedure(store_proc_rights);

  if (old_row_exists) {
    if (store_proc_rights) {
      error = table->file->ha_update_row(table->record[1], table->record[0]);
      assert(error != HA_ERR_FOUND_DUPP_KEY);
      assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
             error != HA_ERR_LOCK_DEADLOCK);
      assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
             error != HA_ERR_LOCK_WAIT_TIMEOUT);
      DBUG_EXECUTE_IF("wl7158_replace_routine_table_2",
                      error = HA_ERR_LOCK_DEADLOCK;);
      if (error && error != HA_ERR_RECORD_IS_THE_SAME) goto table_error;
    } else {
      error = table->file->ha_delete_row(table->record[1]);
      assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
             error != HA_ERR_LOCK_DEADLOCK);
      assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
             error != HA_ERR_LOCK_WAIT_TIMEOUT);
      DBUG_EXECUTE_IF("wl7158_replace_routine_table_3",
                      error = HA_ERR_LOCK_DEADLOCK;);
      if (error) goto table_error;
    }
  } else {
    error = table->file->ha_write_row(table->record[0]);
    assert(error != HA_ERR_FOUND_DUPP_KEY);
    assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
           error != HA_ERR_LOCK_DEADLOCK);
    assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
           error != HA_ERR_LOCK_WAIT_TIMEOUT);
    DBUG_EXECUTE_IF("wl7158_replace_routine_table_4",
                    error = HA_ERR_LOCK_DEADLOCK;);
    if (!table->file->is_ignorable_error(error)) goto table_error;
  }

  if (rights) {
    grant_name->privs = rights;
  } else {
    erase_specific_element(
        is_proc ? proc_priv_hash.get() : func_priv_hash.get(),
        grant_name->hash_key, grant_name);
  }
  return 0;

table_error:
  acl_print_ha_error(error);
  return -1;
}

/**
  Construct Table_ref array for ACL tables.

  @param [in, out] tables         Table_ref array
  @param [in]      lock_type      Read or Write
  @param [in]      mdl_type       MDL to be used
*/
static void acl_tables_setup(Table_ref *tables, thr_lock_type lock_type,
                             enum_mdl_type mdl_type) {
  int idx = 0;
  for (idx = 0; idx < ACL_TABLES::LAST_ENTRY; ++idx) {
    tables[idx] = Table_ref("mysql", ACL_TABLE_NAMES[idx], lock_type, mdl_type);
  }

  if (lock_type <= TL_READ_NO_INSERT) {
    /*
      tables new to 8.0 are optional when
      reading as mysql_upgrade must work
    */
    tables[ACL_TABLES::TABLE_PASSWORD_HISTORY].open_strategy =
        Table_ref::OPEN_IF_EXISTS;
    tables[ACL_TABLES::TABLE_ROLE_EDGES].open_strategy =
        Table_ref::OPEN_IF_EXISTS;
    tables[ACL_TABLES::TABLE_DEFAULT_ROLES].open_strategy =
        Table_ref::OPEN_IF_EXISTS;
    tables[ACL_TABLES::TABLE_DYNAMIC_PRIV].open_strategy =
        Table_ref::OPEN_IF_EXISTS;
  }

  for (idx = 0; idx < ACL_TABLES::LAST_ENTRY - 1; ++idx) {
    (tables + idx)->next_local = (tables + idx)->next_global = tables + idx + 1;
    (tables + idx)->open_type = OT_BASE_ONLY;
  }
  /* Last table */
  (tables + idx)->next_global = nullptr;
  (tables + idx)->open_type = OT_BASE_ONLY;
}

/**
  Setup ACL tables to be opened in read mode.

  Prepare references to all of the grant tables in the order of the
  @ref ACL_TABLES enum.

  @param [in, out] tables Table handles
*/
void acl_tables_setup_for_read(Table_ref *tables) {
  /*
    This function is called in following cases:
    1. check validity of ACL tables (schema, storage engine etc)
    2. Load data from ACL tables to in-memory caches

    Operation MUST be able to run concurrently with a simple SELECT queries

    Following describes which all MDLs are taken and why.

    - MDL_SHARED_READ_ONLY is taken on each of the ACL tables. This
      lock prevents following operations from other connections due
      to the fact that MDL_SHARED_READ_ONLY conflicts with MDL_SHARED_WRITE:
      - DMLs that may modify ACL tables
      - DDLs that may modify ACL tables (ALTER/DROP etc)
      - ACL DDLs
  */
  acl_tables_setup(tables, TL_READ, MDL_SHARED_READ_ONLY);
}

/** Internal_error_handler subclass to suppress ER_LOCK_DEADLOCK error */
class acl_tables_setup_for_write_and_acquire_mdl_error_handler
    : public Internal_error_handler {
 public:
  acl_tables_setup_for_write_and_acquire_mdl_error_handler()
      : m_hit_deadlock(false) {}
  /**
    Handle an error condition

    @param [in] thd           THD handle
    @param [in] sql_errno     Error raised by MDL subsystem
    @param [in] sqlstate      SQL state. Unused.
    @param [in] level         Severity level. Unused.
    @param [in] msg           Message string. Unused.
  */

  virtual bool handle_condition(THD *thd [[maybe_unused]], uint sql_errno,
                                const char *sqlstate [[maybe_unused]],
                                Sql_condition::enum_severity_level *level
                                [[maybe_unused]],
                                const char *msg [[maybe_unused]]) override {
    m_hit_deadlock = (sql_errno == ER_LOCK_DEADLOCK);
    return m_hit_deadlock;
  }

  bool hit_deadlock() { return m_hit_deadlock; }
  void reset_hit_deadlock() { m_hit_deadlock = false; }

 private:
  bool m_hit_deadlock;
};

/**
  Setup ACL tables to be opened in write mode.

  Prepare references to all of the grant tables in the order of the
  @ref ACL_TABLES enum.

  Obtain locks on required MDLs upfront.

  @param [in]      thd    THD handle
  @param [in, out] tables Table handles

  @returns Status of MDL acquisition
    @retval false OK
    @retval true  Error
*/
static bool acl_tables_setup_for_write_and_acquire_mdl(THD *thd,
                                                       Table_ref *tables) {
  /*
    This function is called perform an ACL DDL operation

    Operation MUST be able to run concurrently with a simple SELECT queries

    The operation:
    a. Modifies one or more tables specified below
    b. Updates one or more in-memory caches used for
       authentication/authorization

    Thus, operation MUST not be able to run concurrently with:
    a. A INSERT/UPDATE/DELETE operation on tables used by ACL DDls
    b. FLUSH TABLES WITH READ LOCKS
    c. FLUSH PRIVILEGES

    Following describes which all MDLs are taken and why.

    - MDL_INTENTION_EXCLUSIVE for MDL_key::GLOBAL at global level. This
    - MDL_INTENTION_EXCLUSIVE for MDL_Key::BACKUP_LOCK at global level
    - MDL_SHARED_READ_ONLY for MDL_key::TABLE for each ACL table
    - MDL_SHARED_WRITE for MDL_key::TABLE for each ACL table

    Collectively, these locks prevent following operations from other
    connections:
    - DMLs that may modify ACL tables
    - DDLs that may modify ACL tables (ALTER/DROP etc)
    - ACL DDLs
    - FLUSH PRIVILEGES
    - FLUSH TABLES WITH READ LOCK
    - LOCK TABLE
  */

  /* MDL does not matter because we are setting flag to ignore it */
  acl_tables_setup(tables, TL_WRITE, MDL_SHARED_WRITE);

  MDL_request_list mdl_requests;

  if (thd->global_read_lock.can_acquire_protection()) return true;
  MDL_request *global_ix_request = new (thd->mem_root) MDL_request;
  if (global_ix_request == nullptr) {
    my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR), sizeof(MDL_request));
    return true;
  }

  MDL_REQUEST_INIT(global_ix_request, MDL_key::GLOBAL, "", "",
                   MDL_INTENTION_EXCLUSIVE, MDL_TRANSACTION);
  mdl_requests.push_front(global_ix_request);

  MDL_request *backup_request = new (thd->mem_root) MDL_request;
  if (backup_request == nullptr) {
    my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR), sizeof(MDL_request));
    return true;
  }

  MDL_REQUEST_INIT(backup_request, MDL_key::BACKUP_LOCK, "", "",
                   MDL_INTENTION_EXCLUSIVE, MDL_TRANSACTION);
  mdl_requests.push_front(backup_request);

  for (int idx = 0; idx < ACL_TABLES::LAST_ENTRY; ++idx) {
    MDL_request *sro_request = new (thd->mem_root) MDL_request;
    if (sro_request == nullptr) {
      my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR), sizeof(MDL_request));
      return true;
    }
    MDL_REQUEST_INIT(sro_request, MDL_key::TABLE, (tables + idx)->db,
                     (tables + idx)->table_name, MDL_SHARED_READ_ONLY,
                     MDL_TRANSACTION);
    mdl_requests.push_front(sro_request);

    MDL_request *sw_request = new (thd->mem_root) MDL_request;
    if (sw_request == nullptr) {
      my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR), sizeof(MDL_request));
      return true;
    }
    MDL_REQUEST_INIT(sw_request, MDL_key::TABLE, (tables + idx)->db,
                     (tables + idx)->table_name, MDL_SHARED_WRITE,
                     MDL_TRANSACTION);
    mdl_requests.push_front(sw_request);
  }

  acl_tables_setup_for_write_and_acquire_mdl_error_handler handler;
  thd->push_internal_handler(&handler);
  bool mdl_acquire_error = false;
  for (;;) {
    handler.reset_hit_deadlock();
    /*
      MDL_context::acquire_locks sorts all lock requests.
      Due to this, a situation may arise that two or more
      competing threads will acquire:
      - IX lock at global level
      - Backup lock
      - SRO on first table according to sorting done

      Next, they will all compete for SW on the same table.

      At this point, MDL's deadlock detection would kick in
      and evict all but one thread.

      In a standalone server, this may result into victim
      threads receiving ER_DEADLOCK_ERROR. This may not be
      a big problem because user can always retry. However,
      in a secondary node, applier threads can not
      tolerate such error.

      Thus, we run a infinite loop here. Essentially - do not
      give up in case a deadlock is encountered. Repeat until
      all required locks are obtained. We do this by crafting
      a special handler and pushing it on top for the duration
      of the loop. This error handler will suppress
      ER_DEADLOCK_ERROR.

      MDL_context::acquire_locks() will make sure that lock
      acquisition is atomic. So we don't have to worry about
      cleanup in case the function fails.

      Note that even with custom error handlers,
      MDL_context::acquire_locks() will return failure in case
      of ER_DEADLOCK_ERROR. However, there won't be any error
      set in THD thatnks to custom error handler.
    */
    mdl_acquire_error = thd->mdl_context.acquire_locks(
        &mdl_requests, thd->variables.lock_wait_timeout);
    /* Retry acquire_locks only if there was a deadlock */
    if (mdl_acquire_error && handler.hit_deadlock())
      continue;
    else
      break;
  }
  thd->pop_internal_handler();

  return mdl_acquire_error;
}

/**
  Open the grant tables.

  @param          thd                   The current thread.
  @param[in,out]  tables                Array of ACL_TABLES::LAST_ENTRY
                                        table list elements
                                        which will be used for opening tables.
  @param[out]     transactional_tables  Set to true if one of grant tables is
                                        transactional, false otherwise.

  @retval  1    Skip GRANT handling during replication.
  @retval  0    OK.
  @retval  < 0  Error.

  @note  IX Backup Lock is implicitly acquired as side effect of calling
         this function.
*/

int open_grant_tables(THD *thd, Table_ref *tables, bool *transactional_tables) {
  DBUG_TRACE;

  if (!initialized) {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    return -1;
  }

  *transactional_tables = false;

  DEBUG_SYNC(thd, "wl14084_acl_ddl_before_mdl_acquisition");

  if (acl_tables_setup_for_write_and_acquire_mdl(thd, tables)) return -1;

  /*
    GRANT and REVOKE are applied the slave in/exclusion rules as they are
    some kind of updates to the mysql.% tables.
  */
  if (thd->slave_thread && thd->rli_slave->rpl_filter->is_on()) {
    /*
      The tables must be marked "updating" so that tables_ok() takes them into
      account in tests.
    */
    for (auto i = 0; i < ACL_TABLES::LAST_ENTRY; i++) tables[i].updating = true;

    if (!(thd->sp_runtime_ctx ||
          thd->rli_slave->rpl_filter->tables_ok(nullptr, tables))) {
      thd->mdl_context.release_transactional_locks();
      return 1;
    }

    for (auto i = 0; i < ACL_TABLES::LAST_ENTRY; i++)
      tables[i].updating = false;
  }

  uint flags = MYSQL_OPEN_HAS_MDL_LOCK | MYSQL_LOCK_IGNORE_TIMEOUT |
               MYSQL_OPEN_IGNORE_FLUSH;
  if (open_and_lock_tables(thd, tables,
                           flags)) {  // This should never happen
    thd->mdl_context.release_transactional_locks();
    return -1;
  }
  DEBUG_SYNC(thd, "wl14084_after_table_locks");
  DBUG_EXECUTE_IF("wl14084_acl_ddl_after_table_lock_trigger_timeout",
                  sleep(2););
  if (check_engine_type_for_acl_table(tables, true) ||
      check_acl_tables_intact(thd, tables)) {
    commit_and_close_mysql_tables(thd);
    return -1;
  }

  for (uint i = 0; i < ACL_TABLES::LAST_ENTRY; ++i)
    *transactional_tables =
        (*transactional_tables ||
         (tables[i].table && tables[i].table->file->has_transactions()));

  return 0;
}

/**
  Modify a privilege table.

  @param  table                 The table to modify.
  @param  host_field            The host name field.
  @param  user_field            The user name field.
  @param  user_to               The new name for the user if to be renamed,
                                NULL otherwise.

  @note
  Update user/host in the current record if user_to is not NULL.
  Delete the current record if user_to is NULL.

  @retval  0    OK.
  @retval  != 0  Error.
*/

static int modify_grant_table(TABLE *table, Field *host_field,
                              Field *user_field, LEX_USER *user_to) {
  int error;
  DBUG_TRACE;

  if (user_to) {
    /* rename */
    store_record(table, record[1]);
    host_field->store(user_to->host.str, user_to->host.length,
                      system_charset_info);
    user_field->store(user_to->user.str, user_to->user.length,
                      system_charset_info);
    error = table->file->ha_update_row(table->record[1], table->record[0]);
    assert(error != HA_ERR_FOUND_DUPP_KEY);
    assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
           error != HA_ERR_LOCK_DEADLOCK);
    assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
           error != HA_ERR_LOCK_WAIT_TIMEOUT);
    DBUG_EXECUTE_IF("wl7158_modify_grant_table_1",
                    error = HA_ERR_LOCK_DEADLOCK;);
  } else {
    /* delete */
    error = table->file->ha_delete_row(table->record[0]);
    assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
           error != HA_ERR_LOCK_DEADLOCK);
    assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
           error != HA_ERR_LOCK_WAIT_TIMEOUT);
    DBUG_EXECUTE_IF("wl7158_modify_grant_table_2",
                    error = HA_ERR_LOCK_DEADLOCK;);
  }

  return error;
}

/**
  Handle a privilege table.
  @param  tables              The array with the four open tables.
  @param  table_no            The number of the table to handle (0..4).
  @param  drop                If user_from is to be dropped.
  @param  user_from           The the user to be searched/dropped/renamed.
  @param  user_to             The new name for the user if to be renamed, NULL
                              otherwise.

  This function scans through following tables:
  mysql.user, mysql.db, mysql.tables_priv, mysql.columns_priv,
  mysql.procs_priv, mysql.proxies_priv.
  For all above tables, we do an index scan and then iterate over the
  found records do following:
  Delete from grant table if drop is true.
  Update in grant table if drop is false and user_to is not NULL.
  Search in grant table if drop is false and user_to is NULL.

  @return  Operation result
    @retval  0    OK, but no record matched.
    @retval  < 0  Error.
    @retval  > 0  At least one record matched.
*/

int handle_grant_table(THD *, Table_ref *tables, ACL_TABLES table_no, bool drop,
                       LEX_USER *user_from, LEX_USER *user_to) {
  int result = 0;
  int error = 0;
  TABLE *table = tables[table_no].table;
  Field *host_field = table->field[0];
  Field *user_field = table->field[table_no && table_no != 5 ? 2 : 1];

  uchar user_key[MAX_KEY_LENGTH];
  uint key_prefix_length;
  DBUG_TRACE;

  table->use_all_columns();
  DBUG_PRINT("info",
             ("read table: '%s'  search: '%s'@'%s'", table->s->table_name.str,
              user_from->user.str, user_from->host.str));
  host_field->store(user_from->host.str, user_from->host.length,
                    system_charset_info);
  user_field->store(user_from->user.str, user_from->user.length,
                    system_charset_info);

  key_prefix_length = (table->key_info->key_part[0].store_length +
                       table->key_info->key_part[1].store_length);
  key_copy(user_key, table->record[0], table->key_info, key_prefix_length);

  if ((error = table->file->ha_index_init(0, true))) {
    acl_print_ha_error(error);
    table->file->ha_rnd_end();
    result = -1;
    return result;
  }

  error = table->file->ha_index_read_map(table->record[0], user_key,
                                         (key_part_map)3, HA_READ_KEY_EXACT);
  assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
         error != HA_ERR_LOCK_DEADLOCK);
  assert(table->file->ht->db_type == DB_TYPE_NDBCLUSTER ||
         error != HA_ERR_LOCK_WAIT_TIMEOUT);
  DBUG_EXECUTE_IF("wl7158_handle_grant_table_1", error = HA_ERR_LOCK_DEADLOCK;);

  if (error) {
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE) {
      acl_print_ha_error(error);
      result = -1;
    }
  } else {
    /*
      Iterate over range of records returned as part of index search done based
      on user and host values.
    */
    while (!error) {
      /* If requested, delete or update the record. */
      if (drop || user_to)
        error = modify_grant_table(table, host_field, user_field, user_to);
      if (error) {
        result = -1;
        break;
      } else {
        result = 1;
      }
      DBUG_PRINT("info", ("read result: %d", result));
      /* fetch next record */
      error = table->file->ha_index_next_same(table->record[0], user_key,
                                              key_prefix_length);
    }
    if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE &&
        error != HA_ERR_RECORD_IS_THE_SAME) {
      acl_print_ha_error(error);
      result = -1;
    }
  }
  if (table->file->inited != handler::NONE) table->file->ha_index_end();

  return result;
}

/**
  Check that every ACL table has a supported storage engine (InnoDB).
  Report error if table's engine type is not supported.

  @param tables       Pointer to TABLES_LIST of ACL tables to check.
  @param report_error If true report error to the client/diagnostic
                      area, otherwise write a warning to the error log.

  @return bool
    @retval false OK
    @retval true  some of ACL tables has an unsupported engine type.
*/

bool check_engine_type_for_acl_table(Table_ref *tables, bool report_error) {
  bool invalid_table_found = false;

  for (Table_ref *t = tables; t; t = t->next_local) {
    if (t->table && !t->table->file->ht->is_supported_system_table(
                        t->db, t->table_name, true)) {
      invalid_table_found = true;
      if (report_error) {
        my_error(ER_UNSUPPORTED_ENGINE, MYF(0),
                 ha_resolve_storage_engine_name(t->table->file->ht), t->db,
                 t->table_name);
        // No need to check further.
        break;
      } else {
        LogErr(WARNING_LEVEL, ER_SYSTEM_TABLES_NOT_SUPPORTED_BY_STORAGE_ENGINE,
               ha_resolve_storage_engine_name(t->table->file->ht), t->db,
               t->table_name);
      }
    }
  }

  return invalid_table_found;
}

/**
  Check if given table name is a ACL table name.

  @param name   Table name.

  @return bool
    @retval true  If it is a ACL table, otherwise false.
*/

bool is_acl_table_name(const char *name) {
  for (int i = 0; i < MAX_ACL_TABLE_NAMES; i++) {
    if (!my_strcasecmp(system_charset_info, ACL_TABLE_NAMES[i], name))
      return true;
  }
  return false;
}

#ifndef NDEBUG
/**
  Check if given TABLE* is a ACL table name.

  @param table   TABLE object.

  @return bool
    @retval true  If it is a ACL table, otherwise false.
*/

bool is_acl_table(const TABLE *table) {
  return (!my_strcasecmp(system_charset_info, MYSQL_SCHEMA_NAME.str,
                         table->s->db.str) &&
          is_acl_table_name(table->s->table_name.str));
}
#endif
