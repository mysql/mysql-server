#ifndef SQL_ACL_INCLUDED
#define SQL_ACL_INCLUDED

/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"                          /* NO_EMBEDDED_ACCESS_CHECKS */
#include "violite.h"                            /* SSL_type */
#include "sql_class.h"                          /* LEX_COLUMN */

#define SELECT_ACL	(1L << 0)
#define INSERT_ACL	(1L << 1)
#define UPDATE_ACL	(1L << 2)
#define DELETE_ACL	(1L << 3)
#define CREATE_ACL	(1L << 4)
#define DROP_ACL	(1L << 5)
#define RELOAD_ACL	(1L << 6)
#define SHUTDOWN_ACL	(1L << 7)
#define PROCESS_ACL	(1L << 8)
#define FILE_ACL	(1L << 9)
#define GRANT_ACL	(1L << 10)
#define REFERENCES_ACL	(1L << 11)
#define INDEX_ACL	(1L << 12)
#define ALTER_ACL	(1L << 13)
#define SHOW_DB_ACL	(1L << 14)
#define SUPER_ACL	(1L << 15)
#define CREATE_TMP_ACL	(1L << 16)
#define LOCK_TABLES_ACL	(1L << 17)
#define EXECUTE_ACL	(1L << 18)
#define REPL_SLAVE_ACL	(1L << 19)
#define REPL_CLIENT_ACL	(1L << 20)
#define CREATE_VIEW_ACL	(1L << 21)
#define SHOW_VIEW_ACL	(1L << 22)
#define CREATE_PROC_ACL	(1L << 23)
#define ALTER_PROC_ACL  (1L << 24)
#define CREATE_USER_ACL (1L << 25)
#define EVENT_ACL       (1L << 26)
#define TRIGGER_ACL     (1L << 27)
#define CREATE_TABLESPACE_ACL (1L << 28)
/*
  don't forget to update
  1. static struct show_privileges_st sys_privileges[]
  2. static const char *command_array[] and static uint command_lengths[]
  3. mysql_system_tables.sql and mysql_system_tables_fix.sql
  4. acl_init() or whatever - to define behaviour for old privilege tables
  5. sql_yacc.yy - for GRANT/REVOKE to work
*/
#define NO_ACCESS	(1L << 30)
#define DB_ACLS \
(UPDATE_ACL | SELECT_ACL | INSERT_ACL | DELETE_ACL | CREATE_ACL | DROP_ACL | \
 GRANT_ACL | REFERENCES_ACL | INDEX_ACL | ALTER_ACL | CREATE_TMP_ACL | \
 LOCK_TABLES_ACL | EXECUTE_ACL | CREATE_VIEW_ACL | SHOW_VIEW_ACL | \
 CREATE_PROC_ACL | ALTER_PROC_ACL | EVENT_ACL | TRIGGER_ACL)

#define TABLE_ACLS \
(SELECT_ACL | INSERT_ACL | UPDATE_ACL | DELETE_ACL | CREATE_ACL | DROP_ACL | \
 GRANT_ACL | REFERENCES_ACL | INDEX_ACL | ALTER_ACL | CREATE_VIEW_ACL | \
 SHOW_VIEW_ACL | TRIGGER_ACL)

#define COL_ACLS \
(SELECT_ACL | INSERT_ACL | UPDATE_ACL | REFERENCES_ACL)

#define PROC_ACLS \
(ALTER_PROC_ACL | EXECUTE_ACL | GRANT_ACL)

#define SHOW_PROC_ACLS \
(ALTER_PROC_ACL | EXECUTE_ACL | CREATE_PROC_ACL)

#define GLOBAL_ACLS \
(SELECT_ACL | INSERT_ACL | UPDATE_ACL | DELETE_ACL | CREATE_ACL | DROP_ACL | \
 RELOAD_ACL | SHUTDOWN_ACL | PROCESS_ACL | FILE_ACL | GRANT_ACL | \
 REFERENCES_ACL | INDEX_ACL | ALTER_ACL | SHOW_DB_ACL | SUPER_ACL | \
 CREATE_TMP_ACL | LOCK_TABLES_ACL | REPL_SLAVE_ACL | REPL_CLIENT_ACL | \
 EXECUTE_ACL | CREATE_VIEW_ACL | SHOW_VIEW_ACL | CREATE_PROC_ACL | \
 ALTER_PROC_ACL | CREATE_USER_ACL | EVENT_ACL | TRIGGER_ACL | \
 CREATE_TABLESPACE_ACL)

#define DEFAULT_CREATE_PROC_ACLS \
(ALTER_PROC_ACL | EXECUTE_ACL)

#define SHOW_CREATE_TABLE_ACLS \
(SELECT_ACL | INSERT_ACL | UPDATE_ACL | DELETE_ACL | \
 CREATE_ACL | DROP_ACL | ALTER_ACL | INDEX_ACL | \
 TRIGGER_ACL | REFERENCES_ACL | GRANT_ACL | CREATE_VIEW_ACL | SHOW_VIEW_ACL)

/**
  Table-level privileges which are automatically "granted" to everyone on
  existing temporary tables (CREATE_ACL is necessary for ALTER ... RENAME).
*/
#define TMP_TABLE_ACLS \
(SELECT_ACL | INSERT_ACL | UPDATE_ACL | DELETE_ACL | CREATE_ACL | DROP_ACL | \
 INDEX_ACL | ALTER_ACL)

/*
  Defines to change the above bits to how things are stored in tables
  This is needed as the 'host' and 'db' table is missing a few privileges
*/

/* Privileges that needs to be reallocated (in continous chunks) */
#define DB_CHUNK0 (SELECT_ACL | INSERT_ACL | UPDATE_ACL | DELETE_ACL | \
                   CREATE_ACL | DROP_ACL)
#define DB_CHUNK1 (GRANT_ACL | REFERENCES_ACL | INDEX_ACL | ALTER_ACL)
#define DB_CHUNK2 (CREATE_TMP_ACL | LOCK_TABLES_ACL)
#define DB_CHUNK3 (CREATE_VIEW_ACL | SHOW_VIEW_ACL | \
		   CREATE_PROC_ACL | ALTER_PROC_ACL )
#define DB_CHUNK4 (EXECUTE_ACL)
#define DB_CHUNK5 (EVENT_ACL | TRIGGER_ACL)

#define fix_rights_for_db(A)  (((A)       & DB_CHUNK0) | \
			      (((A) << 4) & DB_CHUNK1) | \
			      (((A) << 6) & DB_CHUNK2) | \
			      (((A) << 9) & DB_CHUNK3) | \
			      (((A) << 2) & DB_CHUNK4))| \
                              (((A) << 9) & DB_CHUNK5)
#define get_rights_for_db(A)  (((A) & DB_CHUNK0)       | \
			      (((A) & DB_CHUNK1) >> 4) | \
			      (((A) & DB_CHUNK2) >> 6) | \
			      (((A) & DB_CHUNK3) >> 9) | \
			      (((A) & DB_CHUNK4) >> 2))| \
                              (((A) & DB_CHUNK5) >> 9)
#define TBL_CHUNK0 DB_CHUNK0
#define TBL_CHUNK1 DB_CHUNK1
#define TBL_CHUNK2 (CREATE_VIEW_ACL | SHOW_VIEW_ACL)
#define TBL_CHUNK3 TRIGGER_ACL
#define fix_rights_for_table(A) (((A)        & TBL_CHUNK0) | \
                                (((A) <<  4) & TBL_CHUNK1) | \
                                (((A) << 11) & TBL_CHUNK2) | \
                                (((A) << 15) & TBL_CHUNK3))
#define get_rights_for_table(A) (((A) & TBL_CHUNK0)        | \
                                (((A) & TBL_CHUNK1) >>  4) | \
                                (((A) & TBL_CHUNK2) >> 11) | \
                                (((A) & TBL_CHUNK3) >> 15))
#define fix_rights_for_column(A) (((A) & 7) | (((A) & ~7) << 8))
#define get_rights_for_column(A) (((A) & 7) | ((A) >> 8))
#define fix_rights_for_procedure(A) ((((A) << 18) & EXECUTE_ACL) | \
				     (((A) << 23) & ALTER_PROC_ACL) | \
				     (((A) << 8) & GRANT_ACL))
#define get_rights_for_procedure(A) ((((A) & EXECUTE_ACL) >> 18) |  \
				     (((A) & ALTER_PROC_ACL) >> 23) | \
				     (((A) & GRANT_ACL) >> 8))

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
  MYSQL_USER_FIELD_PASSWORD,
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
  MYSQL_USER_FIELD_COUNT
};

extern const TABLE_FIELD_DEF mysql_db_table_def;
extern bool mysql_user_table_is_in_short_password_format;
extern my_bool disconnect_on_expired_password;
extern const char *command_array[];
extern uint        command_lengths[];


/* prototypes */

bool hostname_requires_resolving(const char *hostname);
void append_user(THD *thd, String *str, LEX_USER *user, bool comma,
                 bool passwd);
my_bool  acl_init(bool dont_read_acl_tables);
my_bool acl_reload(THD *thd);
void acl_free(bool end=0);
ulong acl_get(const char *host, const char *ip,
	      const char *user, const char *db, my_bool db_is_pattern);
int acl_authenticate(THD *thd, uint com_change_user_pkt_len);
bool acl_getroot(Security_context *sctx, char *user, char *host,
                 char *ip, char *db);
bool acl_check_host(const char *host, const char *ip);
int check_change_password(THD *thd, const char *host, const char *user,
                           char *password, uint password_len);
bool change_password(THD *thd, const char *host, const char *user,
		     char *password);
bool mysql_grant(THD *thd, const char *db, List <LEX_USER> &user_list,
                 ulong rights, bool revoke, bool is_proxy);
int mysql_table_grant(THD *thd, TABLE_LIST *table, List <LEX_USER> &user_list,
                       List <LEX_COLUMN> &column_list, ulong rights,
                       bool revoke);
bool mysql_routine_grant(THD *thd, TABLE_LIST *table, bool is_proc,
			 List <LEX_USER> &user_list, ulong rights,
			 bool revoke, bool write_to_binlog);
my_bool grant_init();
void grant_free(void);
my_bool grant_reload(THD *thd);
bool check_grant(THD *thd, ulong want_access, TABLE_LIST *tables,
                 bool any_combination_will_do, uint number, bool no_errors);
bool check_grant_column (THD *thd, GRANT_INFO *grant,
			 const char *db_name, const char *table_name,
			 const char *name, uint length, Security_context *sctx);
bool check_column_grant_in_table_ref(THD *thd, TABLE_LIST * table_ref,
                                     const char *name, uint length);
bool check_grant_all_columns(THD *thd, ulong want_access, 
                             Field_iterator_table_ref *fields);
bool check_grant_routine(THD *thd, ulong want_access,
			 TABLE_LIST *procs, bool is_proc, bool no_error);
bool check_grant_db(THD *thd,const char *db);
ulong get_table_grant(THD *thd, TABLE_LIST *table);
ulong get_column_grant(THD *thd, GRANT_INFO *grant,
                       const char *db_name, const char *table_name,
                       const char *field_name);
bool mysql_show_grants(THD *thd, LEX_USER *user);
void get_privilege_desc(char *to, uint max_length, ulong access);
void get_mqh(const char *user, const char *host, USER_CONN *uc);
bool mysql_create_user(THD *thd, List <LEX_USER> &list);
bool mysql_drop_user(THD *thd, List <LEX_USER> &list);
bool mysql_rename_user(THD *thd, List <LEX_USER> &list);
bool mysql_user_password_expire(THD *thd, List <LEX_USER> &list);
bool mysql_revoke_all(THD *thd, List <LEX_USER> &list);
void fill_effective_table_privileges(THD *thd, GRANT_INFO *grant,
                                     const char *db, const char *table);
bool sp_revoke_privileges(THD *thd, const char *sp_db, const char *sp_name,
                          bool is_proc);
bool sp_grant_privileges(THD *thd, const char *sp_db, const char *sp_name,
                         bool is_proc);
bool check_routine_level_acl(THD *thd, const char *db, const char *name,
                             bool is_proc);
bool is_acl_user(const char *host, const char *user);
int fill_schema_user_privileges(THD *thd, TABLE_LIST *tables, Item *cond);
int fill_schema_schema_privileges(THD *thd, TABLE_LIST *tables, Item *cond);
int fill_schema_table_privileges(THD *thd, TABLE_LIST *tables, Item *cond);
int fill_schema_column_privileges(THD *thd, TABLE_LIST *tables, Item *cond);
int wild_case_compare(CHARSET_INFO *cs, const char *str,const char *wildstr);
int digest_password(THD *thd, LEX_USER *user_record);
int check_password_strength(String *password);
int check_password_policy(String *password);
#ifdef NO_EMBEDDED_ACCESS_CHECKS
#define check_grant(A,B,C,D,E,F) 0
#define check_grant_db(A,B) 0
#endif
void close_acl_tables(THD *thd);

/**
  Result of an access check for an internal schema or table.
  Internal ACL checks are always performed *before* using
  the grant tables.
  This mechanism enforces that the server implementation has full
  control on its internal tables.
  Depending on the internal check result, the server implementation
  can choose to:
  - always allow access,
  - always deny access,
  - delegate the decision to the database administrator,
  by using the grant tables.
*/
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
  static void register_schema(const LEX_STRING *name,
                              const ACL_internal_schema_access *access);
  static const ACL_internal_schema_access *lookup(const char *name);
};

const ACL_internal_schema_access *
get_cached_schema_access(GRANT_INTERNAL_INFO *grant_internal_info,
                         const char *schema_name);

const ACL_internal_table_access *
get_cached_table_access(GRANT_INTERNAL_INFO *grant_internal_info,
                        const char *schema_name,
                        const char *table_name);

bool acl_check_proxy_grant_access (THD *thd, const char *host, const char *user,
                                   bool with_grant);

void init_default_auth_plugin();
int set_default_auth_plugin(char *, int);

#endif /* SQL_ACL_INCLUDED */
