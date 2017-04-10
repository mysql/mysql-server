/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef AUTH_COMMON_INCLUDED
#define AUTH_COMMON_INCLUDED

#include "my_config.h"

#include <stddef.h>
#include <sys/types.h>
#include <set>
#include <utility>
#include <vector>

#include "auth_acls.h"                          /* ACL information */
#include "lex_string.h"
#include "m_string.h"
#include "my_command.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "sql_string.h"                         /* String */
#include "template_utils.h"
#include "thr_malloc.h"
#include <mysql/components/service.h>
#include <mysql/components/my_service.h>
#include <mysql/components/services/dynamic_privilege.h>
#include "dynamic_privileges_impl.h"

#include <functional>

/* Forward Declarations */
class Alter_info;
class Field_iterator_table_ref;
class LEX_COLUMN;
class THD;
template <class T> class List;

typedef struct st_grant_internal_info GRANT_INTERNAL_INFO;
typedef struct st_lex_user LEX_USER;
typedef struct st_ha_create_information HA_CREATE_INFO;
class Item;
struct GRANT_INFO;

typedef struct user_conn USER_CONN;
class Security_context;
struct TABLE;
struct TABLE_LIST;

/** user, host tuple which reference either acl_cache or g_default_roles */
typedef std::pair< LEX_CSTRING, LEX_CSTRING > Auth_id_ref;
typedef std::vector< Auth_id_ref >  List_of_auth_id_refs;

bool operator<(const Auth_id_ref &a, const Auth_id_ref &b);

/* Classes */

enum ACL_internal_access_result
{
  /**
    Access granted for all the requested privileges,
    do not use the grant tables.
    This flag is used only for the INFORMATION_SCHEMA privileges,
    for compatibility reasons.
  */
  ACL_INTERNAL_ACCESS_GRANTED,
  /** Access denied, do not use the grant tables. */
  ACL_INTERNAL_ACCESS_DENIED,
  /** No decision yet, use the grant tables. */
  ACL_INTERNAL_ACCESS_CHECK_GRANT
};

/**
  Per internal table ACL access rules.
  This class is an interface.
  Per table(s) specific access rule should be implemented in a subclass.
  @sa ACL_internal_schema_access
*/
class ACL_internal_table_access
{
public:
  ACL_internal_table_access()
  {}

  virtual ~ACL_internal_table_access()
  {}

  /**
    Check access to an internal table.
    When a privilege is granted, this method add the requested privilege
    to save_priv.
    @param want_access the privileges requested
    @param [in, out] save_priv the privileges granted
    @return
      @retval ACL_INTERNAL_ACCESS_GRANTED All the requested privileges
      are granted, and saved in save_priv.
      @retval ACL_INTERNAL_ACCESS_DENIED At least one of the requested
      privileges was denied.
      @retval ACL_INTERNAL_ACCESS_CHECK_GRANT No requested privilege
      was denied, and grant should be checked for at least one
      privilege. Requested privileges that are granted, if any, are saved
      in save_priv.
  */
  virtual ACL_internal_access_result check(ulong want_access,
                                           ulong *save_priv) const= 0;
};

/**
  Per internal schema ACL access rules.
  This class is an interface.
  Each per schema specific access rule should be implemented
  in a different subclass, and registered.
  Per schema access rules can control:
  - every schema privileges on schema.*
  - every table privileges on schema.table
  @sa ACL_internal_schema_registry
*/
class ACL_internal_schema_access
{
public:
  ACL_internal_schema_access()
  {}

  virtual ~ACL_internal_schema_access()
  {}

  /**
    Check access to an internal schema.
    @param want_access the privileges requested
    @param [in, out] save_priv the privileges granted
    @return
      @retval ACL_INTERNAL_ACCESS_GRANTED All the requested privileges
      are granted, and saved in save_priv.
      @retval ACL_INTERNAL_ACCESS_DENIED At least one of the requested
      privileges was denied.
      @retval ACL_INTERNAL_ACCESS_CHECK_GRANT No requested privilege
      was denied, and grant should be checked for at least one
      privilege. Requested privileges that are granted, if any, are saved
      in save_priv.
  */
  virtual ACL_internal_access_result check(ulong want_access,
                                           ulong *save_priv) const= 0;

  /**
    Search for per table ACL access rules by table name.
    @param name the table name
    @return per table access rules, or NULL
  */
  virtual const ACL_internal_table_access *lookup(const char *name) const= 0;
};

/**
  A registry for per internal schema ACL.
  An 'internal schema' is a database schema maintained by the
  server implementation, such as 'performance_schema' and 'INFORMATION_SCHEMA'.
*/
class ACL_internal_schema_registry
{
public:
  static void register_schema(const LEX_STRING &name,
                              const ACL_internal_schema_access *access);
  static const ACL_internal_schema_access *lookup(const char *name);
};

/**
  Extension of ACL_internal_schema_access for Information Schema
*/
class IS_internal_schema_access : public ACL_internal_schema_access
{
public:
  IS_internal_schema_access()
  {}

  ~IS_internal_schema_access()
  {}

  ACL_internal_access_result check(ulong want_access,
                                   ulong *save_priv) const;

  const ACL_internal_table_access *lookup(const char *name) const;
};

/* Data Structures */

extern const char *command_array[];
extern uint        command_lengths[];

enum mysql_db_table_field
{
  MYSQL_DB_FIELD_HOST = 0,
  MYSQL_DB_FIELD_DB,
  MYSQL_DB_FIELD_USER,
  MYSQL_DB_FIELD_SELECT_PRIV,
  MYSQL_DB_FIELD_INSERT_PRIV,
  MYSQL_DB_FIELD_UPDATE_PRIV,
  MYSQL_DB_FIELD_DELETE_PRIV,
  MYSQL_DB_FIELD_CREATE_PRIV,
  MYSQL_DB_FIELD_DROP_PRIV,
  MYSQL_DB_FIELD_GRANT_PRIV,
  MYSQL_DB_FIELD_REFERENCES_PRIV,
  MYSQL_DB_FIELD_INDEX_PRIV,
  MYSQL_DB_FIELD_ALTER_PRIV,
  MYSQL_DB_FIELD_CREATE_TMP_TABLE_PRIV,
  MYSQL_DB_FIELD_LOCK_TABLES_PRIV,
  MYSQL_DB_FIELD_CREATE_VIEW_PRIV,
  MYSQL_DB_FIELD_SHOW_VIEW_PRIV,
  MYSQL_DB_FIELD_CREATE_ROUTINE_PRIV,
  MYSQL_DB_FIELD_ALTER_ROUTINE_PRIV,
  MYSQL_DB_FIELD_EXECUTE_PRIV,
  MYSQL_DB_FIELD_EVENT_PRIV,
  MYSQL_DB_FIELD_TRIGGER_PRIV,
  MYSQL_DB_FIELD_COUNT
};

enum mysql_user_table_field
{
  MYSQL_USER_FIELD_HOST= 0,
  MYSQL_USER_FIELD_USER,
  MYSQL_USER_FIELD_SELECT_PRIV,
  MYSQL_USER_FIELD_INSERT_PRIV,
  MYSQL_USER_FIELD_UPDATE_PRIV,
  MYSQL_USER_FIELD_DELETE_PRIV,
  MYSQL_USER_FIELD_CREATE_PRIV,
  MYSQL_USER_FIELD_DROP_PRIV,
  MYSQL_USER_FIELD_RELOAD_PRIV,
  MYSQL_USER_FIELD_SHUTDOWN_PRIV,
  MYSQL_USER_FIELD_PROCESS_PRIV,
  MYSQL_USER_FIELD_FILE_PRIV,
  MYSQL_USER_FIELD_GRANT_PRIV,
  MYSQL_USER_FIELD_REFERENCES_PRIV,
  MYSQL_USER_FIELD_INDEX_PRIV,
  MYSQL_USER_FIELD_ALTER_PRIV,
  MYSQL_USER_FIELD_SHOW_DB_PRIV,
  MYSQL_USER_FIELD_SUPER_PRIV,
  MYSQL_USER_FIELD_CREATE_TMP_TABLE_PRIV,
  MYSQL_USER_FIELD_LOCK_TABLES_PRIV,
  MYSQL_USER_FIELD_EXECUTE_PRIV,
  MYSQL_USER_FIELD_REPL_SLAVE_PRIV,
  MYSQL_USER_FIELD_REPL_CLIENT_PRIV,
  MYSQL_USER_FIELD_CREATE_VIEW_PRIV,
  MYSQL_USER_FIELD_SHOW_VIEW_PRIV,
  MYSQL_USER_FIELD_CREATE_ROUTINE_PRIV,
  MYSQL_USER_FIELD_ALTER_ROUTINE_PRIV,
  MYSQL_USER_FIELD_CREATE_USER_PRIV,
  MYSQL_USER_FIELD_EVENT_PRIV,
  MYSQL_USER_FIELD_TRIGGER_PRIV,
  MYSQL_USER_FIELD_CREATE_TABLESPACE_PRIV,
  MYSQL_USER_FIELD_SSL_TYPE,
  MYSQL_USER_FIELD_SSL_CIPHER,
  MYSQL_USER_FIELD_X509_ISSUER,
  MYSQL_USER_FIELD_X509_SUBJECT,
  MYSQL_USER_FIELD_MAX_QUESTIONS,
  MYSQL_USER_FIELD_MAX_UPDATES,
  MYSQL_USER_FIELD_MAX_CONNECTIONS,
  MYSQL_USER_FIELD_MAX_USER_CONNECTIONS,
  MYSQL_USER_FIELD_PLUGIN,
  MYSQL_USER_FIELD_AUTHENTICATION_STRING,
  MYSQL_USER_FIELD_PASSWORD_EXPIRED,
  MYSQL_USER_FIELD_PASSWORD_LAST_CHANGED,
  MYSQL_USER_FIELD_PASSWORD_LIFETIME,
  MYSQL_USER_FIELD_ACCOUNT_LOCKED,
  MYSQL_USER_FIELD_CREATE_ROLE_PRIV,
  MYSQL_USER_FIELD_DROP_ROLE_PRIV,
  MYSQL_USER_FIELD_COUNT
};

enum mysql_proxies_priv_table_feild
{
  MYSQL_PROXIES_PRIV_FIELD_HOST = 0,
  MYSQL_PROXIES_PRIV_FIELD_USER,
  MYSQL_PROXIES_PRIV_FIELD_PROXIED_HOST,
  MYSQL_PROXIES_PRIV_FIELD_PROXIED_USER,
  MYSQL_PROXIES_PRIV_FIELD_WITH_GRANT,
  MYSQL_PROXIES_PRIV_FIELD_GRANTOR,
  MYSQL_PROXIES_PRIV_FIELD_TIMESTAMP,
  MYSQL_PROXIES_PRIV_FIELD_COUNT
};

enum mysql_procs_priv_table_field
{
  MYSQL_PROCS_PRIV_FIELD_HOST = 0,
  MYSQL_PROCS_PRIV_FIELD_DB,
  MYSQL_PROCS_PRIV_FIELD_USER,
  MYSQL_PROCS_PRIV_FIELD_ROUTINE_NAME,
  MYSQL_PROCS_PRIV_FIELD_ROUTINE_TYPE,
  MYSQL_PROCS_PRIV_FIELD_GRANTOR,
  MYSQL_PROCS_PRIV_FIELD_PROC_PRIV,
  MYSQL_PROCS_PRIV_FIELD_TIMESTAMP,
  MYSQL_PROCS_PRIV_FIELD_COUNT
};

enum mysql_columns_priv_table_field
{
  MYSQL_COLUMNS_PRIV_FIELD_HOST = 0,
  MYSQL_COLUMNS_PRIV_FIELD_DB,
  MYSQL_COLUMNS_PRIV_FIELD_USER,
  MYSQL_COLUMNS_PRIV_FIELD_TABLE_NAME,
  MYSQL_COLUMNS_PRIV_FIELD_COLUMN_NAME,
  MYSQL_COLUMNS_PRIV_FIELD_TIMESTAMP,
  MYSQL_COLUMNS_PRIV_FIELD_COLUMN_PRIV,
  MYSQL_COLUMNS_PRIV_FIELD_COUNT
};

enum mysql_tables_priv_table_field
{
  MYSQL_TABLES_PRIV_FIELD_HOST = 0,
  MYSQL_TABLES_PRIV_FIELD_DB,
  MYSQL_TABLES_PRIV_FIELD_USER,
  MYSQL_TABLES_PRIV_FIELD_TABLE_NAME,
  MYSQL_TABLES_PRIV_FIELD_GRANTOR,
  MYSQL_TABLES_PRIV_FIELD_TIMESTAMP,
  MYSQL_TABLES_PRIV_FIELD_TABLE_PRIV,
  MYSQL_TABLES_PRIV_FIELD_COLUMN_PRIV,
  MYSQL_TABLES_PRIV_FIELD_COUNT
};

enum mysql_role_edges_table_field
{
  MYSQL_ROLE_EDGES_FIELD_FROM_HOST = 0,
  MYSQL_ROLE_EDGES_FIELD_FROM_USER,
  MYSQL_ROLE_EDGES_FIELD_TO_HOST,
  MYSQL_ROLE_EDGES_FIELD_TO_USER,
  MYSQL_ROLE_EDGES_FIELD_WITH_ADMIN_OPTION,
  MYSQL_ROLE_EDGES_FIELD_COUNT
};

enum mysql_default_roles_table_field
{
  MYSQL_DEFAULT_ROLES_FIELD_HOST = 0,
  MYSQL_DEFAULT_ROLES_FIELD_USER,
  MYSQL_DEFAULT_ROLES_FIELD_DEFAULT_ROLE_HOST,
  MYSQL_DEFAULT_ROLES_FIELD_DEFAULT_ROLE_USER,
  MYSQL_DEFAULT_ROLES_FIELD_COUNT
};

/* When we run mysql_upgrade we must make sure that the server can be run
   using previous mysql.user table schema during acl_load.

   Acl_load_user_table_schema is a common interface for the current and the
                              previous mysql.user table schema.
 */
class Acl_load_user_table_schema
{
public:
  virtual uint host_idx()= 0;
  virtual uint user_idx()= 0;
  virtual uint password_idx()= 0;
  virtual uint select_priv_idx()= 0;
  virtual uint insert_priv_idx()= 0;
  virtual uint update_priv_idx()= 0;
  virtual uint delete_priv_idx()= 0;
  virtual uint create_priv_idx()= 0;
  virtual uint drop_priv_idx()= 0;
  virtual uint reload_priv_idx()= 0;
  virtual uint shutdown_priv_idx()= 0;
  virtual uint process_priv_idx()= 0;
  virtual uint file_priv_idx()= 0;
  virtual uint grant_priv_idx()= 0;
  virtual uint references_priv_idx()= 0;
  virtual uint index_priv_idx()= 0;
  virtual uint alter_priv_idx()= 0;
  virtual uint show_db_priv_idx()= 0;
  virtual uint super_priv_idx()= 0;
  virtual uint create_tmp_table_priv_idx()= 0;
  virtual uint lock_tables_priv_idx()= 0;
  virtual uint execute_priv_idx()= 0;
  virtual uint repl_slave_priv_idx()= 0;
  virtual uint repl_client_priv_idx()= 0;
  virtual uint create_view_priv_idx()= 0;
  virtual uint show_view_priv_idx()= 0;
  virtual uint create_routine_priv_idx()= 0;
  virtual uint alter_routine_priv_idx()= 0;
  virtual uint create_user_priv_idx()= 0;
  virtual uint event_priv_idx()= 0;
  virtual uint trigger_priv_idx()= 0;
  virtual uint create_tablespace_priv_idx()= 0;
  virtual uint create_role_priv_idx()= 0;
  virtual uint drop_role_priv_idx()= 0;
  virtual uint ssl_type_idx()= 0;
  virtual uint ssl_cipher_idx()= 0;
  virtual uint x509_issuer_idx()= 0;
  virtual uint x509_subject_idx()= 0;
  virtual uint max_questions_idx()= 0;
  virtual uint max_updates_idx()= 0;
  virtual uint max_connections_idx()= 0;
  virtual uint max_user_connections_idx()= 0;
  virtual uint plugin_idx()= 0;
  virtual uint authentication_string_idx()= 0;
  virtual uint password_expired_idx()= 0;
  virtual uint password_last_changed_idx()= 0;
  virtual uint password_lifetime_idx()= 0;
  virtual uint account_locked_idx()= 0;

  virtual ~Acl_load_user_table_schema() {}
};

/*
  This class describes indices for the current mysql.user table schema.
 */
class Acl_load_user_table_current_schema : public Acl_load_user_table_schema
{
public:
  uint host_idx() { return MYSQL_USER_FIELD_HOST; }
  uint user_idx() { return MYSQL_USER_FIELD_USER; }
  //not available
  uint password_idx() { DBUG_ASSERT(0); return MYSQL_USER_FIELD_COUNT; }
  uint select_priv_idx() { return MYSQL_USER_FIELD_SELECT_PRIV; }
  uint insert_priv_idx() { return MYSQL_USER_FIELD_INSERT_PRIV; }
  uint update_priv_idx() { return MYSQL_USER_FIELD_UPDATE_PRIV; }
  uint delete_priv_idx() { return MYSQL_USER_FIELD_DELETE_PRIV; }
  uint create_priv_idx() { return MYSQL_USER_FIELD_CREATE_PRIV; }
  uint drop_priv_idx() { return MYSQL_USER_FIELD_DROP_PRIV; }
  uint reload_priv_idx() { return MYSQL_USER_FIELD_RELOAD_PRIV; }
  uint shutdown_priv_idx() { return MYSQL_USER_FIELD_SHUTDOWN_PRIV; }
  uint process_priv_idx() { return MYSQL_USER_FIELD_PROCESS_PRIV; }
  uint file_priv_idx() { return MYSQL_USER_FIELD_FILE_PRIV; }
  uint grant_priv_idx() { return MYSQL_USER_FIELD_GRANT_PRIV; }
  uint references_priv_idx() { return MYSQL_USER_FIELD_REFERENCES_PRIV; }
  uint index_priv_idx() { return MYSQL_USER_FIELD_INDEX_PRIV; }
  uint alter_priv_idx() { return MYSQL_USER_FIELD_ALTER_PRIV; }
  uint show_db_priv_idx() { return MYSQL_USER_FIELD_SHOW_DB_PRIV; }
  uint super_priv_idx() { return MYSQL_USER_FIELD_SUPER_PRIV; }
  uint create_role_priv_idx() { return MYSQL_USER_FIELD_CREATE_ROLE_PRIV; }
  uint drop_role_priv_idx() { return MYSQL_USER_FIELD_DROP_ROLE_PRIV; }
  uint create_tmp_table_priv_idx()
  {
    return MYSQL_USER_FIELD_CREATE_TMP_TABLE_PRIV;
  }
  uint lock_tables_priv_idx() { return MYSQL_USER_FIELD_LOCK_TABLES_PRIV; }
  uint execute_priv_idx() { return MYSQL_USER_FIELD_EXECUTE_PRIV; }
  uint repl_slave_priv_idx() { return MYSQL_USER_FIELD_REPL_SLAVE_PRIV; }
  uint repl_client_priv_idx() { return MYSQL_USER_FIELD_REPL_CLIENT_PRIV; }
  uint create_view_priv_idx() { return MYSQL_USER_FIELD_CREATE_VIEW_PRIV; }
  uint show_view_priv_idx() { return MYSQL_USER_FIELD_SHOW_VIEW_PRIV; }
  uint create_routine_priv_idx()
  {
    return MYSQL_USER_FIELD_CREATE_ROUTINE_PRIV;
  }
  uint alter_routine_priv_idx() { return MYSQL_USER_FIELD_ALTER_ROUTINE_PRIV; }
  uint create_user_priv_idx() { return MYSQL_USER_FIELD_CREATE_USER_PRIV; }
  uint event_priv_idx() { return MYSQL_USER_FIELD_EVENT_PRIV; }
  uint trigger_priv_idx() { return MYSQL_USER_FIELD_TRIGGER_PRIV; }
  uint create_tablespace_priv_idx()
  {
    return MYSQL_USER_FIELD_CREATE_TABLESPACE_PRIV;
  }
  uint ssl_type_idx() { return MYSQL_USER_FIELD_SSL_TYPE; }
  uint ssl_cipher_idx() { return MYSQL_USER_FIELD_SSL_CIPHER; }
  uint x509_issuer_idx() { return MYSQL_USER_FIELD_X509_ISSUER; }
  uint x509_subject_idx() { return MYSQL_USER_FIELD_X509_SUBJECT; }
  uint max_questions_idx() { return MYSQL_USER_FIELD_MAX_QUESTIONS; }
  uint max_updates_idx() { return MYSQL_USER_FIELD_MAX_UPDATES; }
  uint max_connections_idx() { return MYSQL_USER_FIELD_MAX_CONNECTIONS; }
  uint max_user_connections_idx()
  {
    return MYSQL_USER_FIELD_MAX_USER_CONNECTIONS;
  }
  uint plugin_idx() { return MYSQL_USER_FIELD_PLUGIN; }
  uint authentication_string_idx()
  {
    return MYSQL_USER_FIELD_AUTHENTICATION_STRING;
  }
  uint password_expired_idx() { return MYSQL_USER_FIELD_PASSWORD_EXPIRED; }
  uint password_last_changed_idx()
  {
    return MYSQL_USER_FIELD_PASSWORD_LAST_CHANGED;
  }
  uint password_lifetime_idx() { return MYSQL_USER_FIELD_PASSWORD_LIFETIME; }
  uint account_locked_idx() { return MYSQL_USER_FIELD_ACCOUNT_LOCKED; }
};

/*
  This class describes indices for the old mysql.user table schema.
 */
class Acl_load_user_table_old_schema : public Acl_load_user_table_schema
{
public:
  enum mysql_user_table_field_56
  {
    MYSQL_USER_FIELD_HOST_56= 0,
    MYSQL_USER_FIELD_USER_56,
    MYSQL_USER_FIELD_PASSWORD_56,
    MYSQL_USER_FIELD_SELECT_PRIV_56,
    MYSQL_USER_FIELD_INSERT_PRIV_56,
    MYSQL_USER_FIELD_UPDATE_PRIV_56,
    MYSQL_USER_FIELD_DELETE_PRIV_56,
    MYSQL_USER_FIELD_CREATE_PRIV_56,
    MYSQL_USER_FIELD_DROP_PRIV_56,
    MYSQL_USER_FIELD_RELOAD_PRIV_56,
    MYSQL_USER_FIELD_SHUTDOWN_PRIV_56,
    MYSQL_USER_FIELD_PROCESS_PRIV_56,
    MYSQL_USER_FIELD_FILE_PRIV_56,
    MYSQL_USER_FIELD_GRANT_PRIV_56,
    MYSQL_USER_FIELD_REFERENCES_PRIV_56,
    MYSQL_USER_FIELD_INDEX_PRIV_56,
    MYSQL_USER_FIELD_ALTER_PRIV_56,
    MYSQL_USER_FIELD_SHOW_DB_PRIV_56,
    MYSQL_USER_FIELD_SUPER_PRIV_56,
    MYSQL_USER_FIELD_CREATE_TMP_TABLE_PRIV_56,
    MYSQL_USER_FIELD_LOCK_TABLES_PRIV_56,
    MYSQL_USER_FIELD_EXECUTE_PRIV_56,
    MYSQL_USER_FIELD_REPL_SLAVE_PRIV_56,
    MYSQL_USER_FIELD_REPL_CLIENT_PRIV_56,
    MYSQL_USER_FIELD_CREATE_VIEW_PRIV_56,
    MYSQL_USER_FIELD_SHOW_VIEW_PRIV_56,
    MYSQL_USER_FIELD_CREATE_ROUTINE_PRIV_56,
    MYSQL_USER_FIELD_ALTER_ROUTINE_PRIV_56,
    MYSQL_USER_FIELD_CREATE_USER_PRIV_56,
    MYSQL_USER_FIELD_EVENT_PRIV_56,
    MYSQL_USER_FIELD_TRIGGER_PRIV_56,
    MYSQL_USER_FIELD_CREATE_TABLESPACE_PRIV_56,
    MYSQL_USER_FIELD_SSL_TYPE_56,
    MYSQL_USER_FIELD_SSL_CIPHER_56,
    MYSQL_USER_FIELD_X509_ISSUER_56,
    MYSQL_USER_FIELD_X509_SUBJECT_56,
    MYSQL_USER_FIELD_MAX_QUESTIONS_56,
    MYSQL_USER_FIELD_MAX_UPDATES_56,
    MYSQL_USER_FIELD_MAX_CONNECTIONS_56,
    MYSQL_USER_FIELD_MAX_USER_CONNECTIONS_56,
    MYSQL_USER_FIELD_PLUGIN_56,
    MYSQL_USER_FIELD_AUTHENTICATION_STRING_56,
    MYSQL_USER_FIELD_PASSWORD_EXPIRED_56,
    MYSQL_USER_FIELD_COUNT_56
  };

  uint host_idx() { return MYSQL_USER_FIELD_HOST_56; }
  uint user_idx() { return MYSQL_USER_FIELD_USER_56; }
  uint password_idx() { return MYSQL_USER_FIELD_PASSWORD_56; }
  uint select_priv_idx() { return MYSQL_USER_FIELD_SELECT_PRIV_56; }
  uint insert_priv_idx() { return MYSQL_USER_FIELD_INSERT_PRIV_56; }
  uint update_priv_idx() { return MYSQL_USER_FIELD_UPDATE_PRIV_56; }
  uint delete_priv_idx() { return MYSQL_USER_FIELD_DELETE_PRIV_56; }
  uint create_priv_idx() { return MYSQL_USER_FIELD_CREATE_PRIV_56; }
  uint drop_priv_idx() { return MYSQL_USER_FIELD_DROP_PRIV_56; }
  uint reload_priv_idx() { return MYSQL_USER_FIELD_RELOAD_PRIV_56; }
  uint shutdown_priv_idx() { return MYSQL_USER_FIELD_SHUTDOWN_PRIV_56; }
  uint process_priv_idx() { return MYSQL_USER_FIELD_PROCESS_PRIV_56; }
  uint file_priv_idx() { return MYSQL_USER_FIELD_FILE_PRIV_56; }
  uint grant_priv_idx() { return MYSQL_USER_FIELD_GRANT_PRIV_56; }
  uint references_priv_idx() { return MYSQL_USER_FIELD_REFERENCES_PRIV_56; }
  uint index_priv_idx() { return MYSQL_USER_FIELD_INDEX_PRIV_56; }
  uint alter_priv_idx() { return MYSQL_USER_FIELD_ALTER_PRIV_56; }
  uint show_db_priv_idx() { return MYSQL_USER_FIELD_SHOW_DB_PRIV_56; }
  uint super_priv_idx() { return MYSQL_USER_FIELD_SUPER_PRIV_56; }
  uint create_tmp_table_priv_idx()
  {
    return MYSQL_USER_FIELD_CREATE_TMP_TABLE_PRIV_56;
  }
  uint lock_tables_priv_idx() { return MYSQL_USER_FIELD_LOCK_TABLES_PRIV_56; }
  uint execute_priv_idx() { return MYSQL_USER_FIELD_EXECUTE_PRIV_56; }
  uint repl_slave_priv_idx() { return MYSQL_USER_FIELD_REPL_SLAVE_PRIV_56; }
  uint repl_client_priv_idx() { return MYSQL_USER_FIELD_REPL_CLIENT_PRIV_56; }
  uint create_view_priv_idx() { return MYSQL_USER_FIELD_CREATE_VIEW_PRIV_56; }
  uint show_view_priv_idx() { return MYSQL_USER_FIELD_SHOW_VIEW_PRIV_56; }
  uint create_routine_priv_idx()
  {
    return MYSQL_USER_FIELD_CREATE_ROUTINE_PRIV_56;
  }
  uint alter_routine_priv_idx()
  {
    return MYSQL_USER_FIELD_ALTER_ROUTINE_PRIV_56;
  }
  uint create_user_priv_idx() { return MYSQL_USER_FIELD_CREATE_USER_PRIV_56; }
  uint event_priv_idx() { return MYSQL_USER_FIELD_EVENT_PRIV_56; }
  uint trigger_priv_idx() { return MYSQL_USER_FIELD_TRIGGER_PRIV_56; }
  uint create_tablespace_priv_idx()
  {
    return MYSQL_USER_FIELD_CREATE_TABLESPACE_PRIV_56;
  }
  uint ssl_type_idx() { return MYSQL_USER_FIELD_SSL_TYPE_56; }
  uint ssl_cipher_idx() { return MYSQL_USER_FIELD_SSL_CIPHER_56; }
  uint x509_issuer_idx() { return MYSQL_USER_FIELD_X509_ISSUER_56; }
  uint x509_subject_idx() { return MYSQL_USER_FIELD_X509_SUBJECT_56; }
  uint max_questions_idx() { return MYSQL_USER_FIELD_MAX_QUESTIONS_56; }
  uint max_updates_idx() { return MYSQL_USER_FIELD_MAX_UPDATES_56; }
  uint max_connections_idx() { return MYSQL_USER_FIELD_MAX_CONNECTIONS_56; }
  uint max_user_connections_idx()
  {
    return MYSQL_USER_FIELD_MAX_USER_CONNECTIONS_56;
  }
  uint plugin_idx() { return MYSQL_USER_FIELD_PLUGIN_56; }
  uint authentication_string_idx()
  {
    return MYSQL_USER_FIELD_AUTHENTICATION_STRING_56;
  }
  uint password_expired_idx() { return MYSQL_USER_FIELD_PASSWORD_EXPIRED_56; }

  //those fields are not available in 5.6 db schema
  uint password_last_changed_idx() { return MYSQL_USER_FIELD_COUNT_56; }
  uint password_lifetime_idx() { return MYSQL_USER_FIELD_COUNT_56; }
  uint account_locked_idx() { return MYSQL_USER_FIELD_COUNT_56; }
  uint create_role_priv_idx() { return MYSQL_USER_FIELD_COUNT_56; }
  uint drop_role_priv_idx() { return MYSQL_USER_FIELD_COUNT_56; }
};


class Acl_load_user_table_schema_factory
{
public:
  virtual Acl_load_user_table_schema* get_user_table_schema(TABLE *table)
  {
    return is_old_user_table_schema(table) ?
      implicit_cast<Acl_load_user_table_schema*>
      (new Acl_load_user_table_old_schema()) :
      implicit_cast<Acl_load_user_table_schema*>
      (new Acl_load_user_table_current_schema());
  }

  virtual bool is_old_user_table_schema(TABLE* table);
  virtual ~Acl_load_user_table_schema_factory() {}
};

extern bool mysql_user_table_is_in_short_password_format;
extern bool disconnect_on_expired_password;
extern const char *any_db;	// Special symbol for check_access
/** controls the extra checks on plugin availability for mysql.user records */

extern bool validate_user_plugins;

/* Function Declarations */

/* sql_authentication */

int set_default_auth_plugin(char *plugin_name, size_t plugin_name_length);
void acl_log_connect(const char *user, const char *host, const char *auth_as,
	const char *db, THD *thd,
enum enum_server_command command);
int acl_authenticate(THD *thd, enum_server_command command);
bool acl_check_host(THD *thd, const char *host, const char *ip);

/*
  User Attributes are the once which are defined during CREATE/ALTER/GRANT
  statement. These attributes are divided into following catagories.
*/

#define NONE_ATTR               0L
#define DEFAULT_AUTH_ATTR       (1L << 0)    /* update defaults auth */
#define PLUGIN_ATTR             (1L << 1)    /* update plugin */
                                             /* authentication_string */
#define SSL_ATTR                (1L << 2)    /* ex: SUBJECT,CIPHER.. */
#define RESOURCE_ATTR           (1L << 3)    /* ex: MAX_QUERIES_PER_HOUR.. */
#define PASSWORD_EXPIRE_ATTR    (1L << 4)    /* update password expire col */
#define ACCESS_RIGHTS_ATTR      (1L << 5)    /* update privileges */
#define ACCOUNT_LOCK_ATTR       (1L << 6)    /* update account lock status */

/* rewrite CREATE/ALTER/GRANT user */
void mysql_rewrite_create_alter_user(THD *thd, String *rlb,
                                     std::set<LEX_USER *> *users_not_to_log= NULL,
                                     bool for_binlog= false);
void mysql_rewrite_grant(THD *thd, String *rlb);
void mysql_rewrite_set_password(THD *thd, String *rlb,
                                std::set<LEX_USER *> *users,
                                bool for_binlog= false);

/* sql_user */
void append_user(THD *thd, String *str, LEX_USER *user,
                 bool comma, bool ident);
void append_user_new(THD *thd, String *str, LEX_USER *user, bool comma);
int check_change_password(THD *thd, const char *host, const char *user,
                          const char *password, size_t password_len);
bool change_password(THD *thd, const char *host, const char *user,
                     char *password);
bool mysql_create_user(THD *thd, List <LEX_USER> &list, bool if_not_exists,
                       bool is_role);
bool mysql_alter_user(THD *thd, List <LEX_USER> &list, bool if_exists);
bool mysql_drop_user(THD *thd, List <LEX_USER> &list, bool if_exists);
bool mysql_rename_user(THD *thd, List <LEX_USER> &list);

/* sql_auth_cache */
int wild_case_compare(CHARSET_INFO *cs, const char *str,const char *wildstr);
int wild_case_compare(CHARSET_INFO *cs, const char *str, size_t str_len,
                      const char *wildstr, size_t wildstr_len);
bool hostname_requires_resolving(const char *hostname);
bool acl_init(bool dont_read_acl_tables);
void acl_free(bool end=0);
bool acl_reload(THD *thd);
bool check_engine_type_for_acl_table(THD *thd);
bool grant_init(bool skip_grant_tables);
void grant_free(void);
bool grant_reload(THD *thd);
bool roles_init_from_tables(THD *thd);
ulong acl_get(THD *thd, const char *host, const char *ip,
              const char *user, const char *db, bool db_is_pattern);
bool is_acl_user(THD *thd, const char *host, const char *user);
bool acl_getroot(THD *thd, Security_context *sctx, char *user,
                 char *host, char *ip, const char *db);
bool check_acl_tables_intact(THD *thd);

/* sql_authorization */
bool has_grant_role_privilege(THD *thd);
bool has_revoke_role_privilege(THD *thd);
int mysql_set_active_role_none(THD *thd);
int mysql_set_role_default(THD *thd);
int mysql_set_active_role_all(THD *thd, const List <LEX_USER> *except_users);
int mysql_set_active_role(THD *thd, const List<LEX_USER > *role_list);
bool mysql_grant(THD *thd, const char *db, List <LEX_USER> &list,
                 ulong rights, bool revoke_grant, bool is_proxy,
                 const List<LEX_CSTRING > &dynamic_privilege,
                 bool grant_all_current_privileges);
bool mysql_routine_grant(THD *thd, TABLE_LIST *table, bool is_proc,
                         List <LEX_USER> &user_list, ulong rights,
                         bool revoke, bool write_to_binlog);
int mysql_table_grant(THD *thd, TABLE_LIST *table, List <LEX_USER> &user_list,
                       List <LEX_COLUMN> &column_list, ulong rights,
                       bool revoke);
bool check_grant(THD *thd, ulong want_access, TABLE_LIST *tables,
                 bool any_combination_will_do, uint number, bool no_errors);
bool check_grant_column (THD *thd, GRANT_INFO *grant,
                         const char *db_name, const char *table_name,
                         const char *name, size_t length,
                         Security_context *sctx, ulong want_privilege);
bool check_column_grant_in_table_ref(THD *thd, TABLE_LIST * table_ref,
                                     const char *name, size_t length,
                                     ulong want_privilege);
bool check_grant_all_columns(THD *thd, ulong want_access,
                             Field_iterator_table_ref *fields);
bool check_grant_routine(THD *thd, ulong want_access,
                         TABLE_LIST *procs, bool is_proc, bool no_error);
bool check_grant_db(THD *thd,const char *db);
bool acl_check_proxy_grant_access(THD *thd, const char *host, const char *user,
                                  bool with_grant);
void get_privilege_desc(char *to, uint max_length, ulong access);
void get_mqh(THD *thd, const char *user, const char *host, USER_CONN *uc);
ulong get_table_grant(THD *thd, TABLE_LIST *table);
ulong get_column_grant(THD *thd, GRANT_INFO *grant,
                       const char *db_name, const char *table_name,
                       const char *field_name);
bool mysql_show_grants(THD *, LEX_USER *,
                       const List_of_auth_id_refs &);
bool mysql_show_create_user(THD *thd, LEX_USER *user);
bool mysql_revoke_all(THD *thd, List <LEX_USER> &list);
bool sp_revoke_privileges(THD *thd, const char *sp_db, const char *sp_name,
                          bool is_proc);
bool sp_grant_privileges(THD *thd, const char *sp_db, const char *sp_name,
                         bool is_proc);
void fill_effective_table_privileges(THD *thd, GRANT_INFO *grant,
                                     const char *db, const char *table);
int fill_schema_user_privileges(THD *thd, TABLE_LIST *tables, Item *cond);
int fill_schema_schema_privileges(THD *thd, TABLE_LIST *tables, Item *cond);
int fill_schema_table_privileges(THD *thd, TABLE_LIST *tables, Item *cond);
int fill_schema_column_privileges(THD *thd, TABLE_LIST *tables, Item *cond);
const ACL_internal_schema_access *
get_cached_schema_access(GRANT_INTERNAL_INFO *grant_internal_info,
                         const char *schema_name);

bool lock_tables_precheck(THD *thd, TABLE_LIST *tables);
bool create_table_precheck(THD *thd, TABLE_LIST *tables,
                           TABLE_LIST *create_table);
bool check_fk_parent_table_access(THD *thd,
                                  HA_CREATE_INFO *create_info,
                                  Alter_info *alter_info);
bool check_readonly(THD *thd, bool err_if_readonly);
void err_readonly(THD *thd);

bool is_secure_transport(int vio_type);

bool check_one_table_access(THD *thd, ulong privilege, TABLE_LIST *tables);
bool check_single_table_access(THD *thd, ulong privilege,
                           TABLE_LIST *tables, bool no_errors);
bool check_routine_access(THD *thd, ulong want_access, const char *db,
                          char *name, bool is_proc, bool no_errors);
bool check_some_access(THD *thd, ulong want_access, TABLE_LIST *table);
bool check_some_routine_access(THD *thd, const char *db, const char *name, bool is_proc);
bool check_access(THD *thd, ulong want_access, const char *db, ulong *save_priv,
                  GRANT_INTERNAL_INFO *grant_internal_info,
                  bool dont_check_global_grants, bool no_errors);
bool check_table_access(THD *thd, ulong requirements,TABLE_LIST *tables,
                        bool any_combination_of_privileges_will_do,
                        uint number,
                        bool no_errors);
bool mysql_grant_role(THD *thd, const List <LEX_USER > *users,
                      const List <LEX_USER > *roles,
                      bool with_admin_opt);
bool mysql_revoke_role(THD *thd, const List <LEX_USER > *users,
                       const List <LEX_USER > *roles);
void get_default_roles(const Auth_id_ref &user, List_of_auth_id_refs *list);
bool mysql_alter_user_set_default_roles(THD *thd, LEX_USER *user,
                       const List_of_auth_id_refs &authids);
bool mysql_alter_user_set_default_roles_all(THD *thd, LEX_USER *user);
bool is_granted_table_access(THD *thd, ulong required_acl,
                             TABLE_LIST *table);
bool mysql_clear_default_roles(THD *thd, LEX_USER *user);
void roles_graphml(THD *thd, String *);
bool has_grant_role_privilege(THD *thd, const LEX_CSTRING &role_name,
                              const LEX_CSTRING &role_host);
Auth_id_ref create_authid_from(const LEX_USER *user);
void append_identifier(String *packet, const char *name, size_t length);
void append_identifier_with_q(int q, String *packet, const char *name,
                              size_t length);
bool is_role_id(LEX_USER *authid);
void shutdown_acl_cache();
bool is_granted_role(LEX_CSTRING user, LEX_CSTRING host,
                     LEX_CSTRING role, LEX_CSTRING role_host);
bool check_show_access(THD *thd, TABLE_LIST *table);
bool check_global_access(THD *thd, ulong want_access);

/* sql_user_table */
void commit_and_close_mysql_tables(THD *thd);

typedef enum ssl_artifacts_status
{
  SSL_ARTIFACTS_NOT_FOUND= 0,
  SSL_ARTIFACTS_VIA_OPTIONS,
  SSL_ARTIFACT_TRACES_FOUND,
  SSL_ARTIFACTS_AUTO_DETECTED
} ssl_artifacts_status;

ulong get_global_acl_cache_size();

#if defined(HAVE_OPENSSL) && !defined(HAVE_YASSL)
extern bool opt_auto_generate_certs;
bool do_auto_cert_generation(ssl_artifacts_status auto_detection_status);
#endif /* HAVE_OPENSSL && !HAVE_YASSL */

#define DEFAULT_SSL_CA_CERT     "ca.pem"
#define DEFAULT_SSL_CA_KEY      "ca-key.pem"
#define DEFAULT_SSL_SERVER_CERT "server-cert.pem"
#define DEFAULT_SSL_SERVER_KEY  "server-key.pem"

#endif /* AUTH_COMMON_INCLUDED */

