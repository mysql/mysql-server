/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

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
/*
  don't forget to update
  1. static struct show_privileges_st sys_privileges[]
  2. static const char *command_array[] and static uint command_lengths[]
  3. mysql_create_system_tables.sh, mysql_fix_privilege_tables.sql
  4. acl_init() or whatever - to define behaviour for old privilege tables
  5. sql_yacc.yy - for GRANT/REVOKE to work
*/
#define EXTRA_ACL	(1L << 29)
#define NO_ACCESS	(1L << 30)

#define DB_ACLS \
(UPDATE_ACL | SELECT_ACL | INSERT_ACL | DELETE_ACL | CREATE_ACL | DROP_ACL | \
 GRANT_ACL | REFERENCES_ACL | INDEX_ACL | ALTER_ACL | CREATE_TMP_ACL | \
 LOCK_TABLES_ACL | EXECUTE_ACL | CREATE_VIEW_ACL | SHOW_VIEW_ACL | \
 CREATE_PROC_ACL | ALTER_PROC_ACL)

#define TABLE_ACLS \
(SELECT_ACL | INSERT_ACL | UPDATE_ACL | DELETE_ACL | CREATE_ACL | DROP_ACL | \
 GRANT_ACL | REFERENCES_ACL | INDEX_ACL | ALTER_ACL | CREATE_VIEW_ACL | \
 SHOW_VIEW_ACL)

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
 ALTER_PROC_ACL | CREATE_USER_ACL)

#define DEFAULT_CREATE_PROC_ACLS \
(ALTER_PROC_ACL | EXECUTE_ACL)

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

#define fix_rights_for_db(A)  (((A)       & DB_CHUNK0) | \
			      (((A) << 4) & DB_CHUNK1) | \
			      (((A) << 6) & DB_CHUNK2) | \
			      (((A) << 9) & DB_CHUNK3) | \
			      (((A) << 2) & DB_CHUNK4))
#define get_rights_for_db(A)  (((A) & DB_CHUNK0)       | \
			      (((A) & DB_CHUNK1) >> 4) | \
			      (((A) & DB_CHUNK2) >> 6) | \
			      (((A) & DB_CHUNK3) >> 9) | \
			      (((A) & DB_CHUNK4) >> 2))
#define TBL_CHUNK0 DB_CHUNK0
#define TBL_CHUNK1 DB_CHUNK1
#define TBL_CHUNK2 (CREATE_VIEW_ACL | SHOW_VIEW_ACL)
#define fix_rights_for_table(A) (((A)        & TBL_CHUNK0) | \
                                (((A) <<  4) & TBL_CHUNK1) | \
                                (((A) << 11) & TBL_CHUNK2))
#define get_rights_for_table(A) (((A) & TBL_CHUNK0)        | \
                                (((A) & TBL_CHUNK1) >>  4) | \
                                (((A) & TBL_CHUNK2) >> 11))
#define fix_rights_for_column(A) (((A) & 7) | (((A) & ~7) << 8))
#define get_rights_for_column(A) (((A) & 7) | ((A) >> 8))
#define fix_rights_for_procedure(A) ((((A) << 18) & EXECUTE_ACL) | \
				     (((A) << 23) & ALTER_PROC_ACL) | \
				     (((A) << 8) & GRANT_ACL))
#define get_rights_for_procedure(A) ((((A) & EXECUTE_ACL) >> 18) |  \
				     (((A) & ALTER_PROC_ACL) >> 23) | \
				     (((A) & GRANT_ACL) >> 8))

/* Classes */

struct acl_host_and_ip
{
  char *hostname;
  long ip,ip_mask;                      // Used with masked ip:s
};


class ACL_ACCESS {
public:
  ulong sort;
  ulong access;
};


/* ACL_HOST is used if no host is specified */

class ACL_HOST :public ACL_ACCESS
{
public:
  acl_host_and_ip host;
  char *db;
};


class ACL_USER :public ACL_ACCESS
{
public:
  acl_host_and_ip host;
  uint hostname_length;
  USER_RESOURCES user_resource;
  char *user;
  uint8 salt[SCRAMBLE_LENGTH+1];       // scrambled password in binary form
  uint8 salt_len;        // 0 - no password, 4 - 3.20, 8 - 3.23, 20 - 4.1.1 
  enum SSL_type ssl_type;
  const char *ssl_cipher, *x509_issuer, *x509_subject;
};


class ACL_DB :public ACL_ACCESS
{
public:
  acl_host_and_ip host;
  char *user,*db;
};

/* prototypes */

bool hostname_requires_resolving(const char *hostname);
my_bool  acl_init(bool dont_read_acl_tables);
my_bool acl_reload(THD *thd);
void acl_free(bool end=0);
ulong acl_get(const char *host, const char *ip,
	      const char *user, const char *db, my_bool db_is_pattern);
int acl_getroot(THD *thd, USER_RESOURCES *mqh, const char *passwd,
                uint passwd_len);
int acl_getroot_no_password(THD *thd);
bool acl_check_host(const char *host, const char *ip);
bool check_change_password(THD *thd, const char *host, const char *user,
                           char *password, uint password_len);
bool change_password(THD *thd, const char *host, const char *user,
		     char *password);
bool mysql_grant(THD *thd, const char *db, List <LEX_USER> &user_list,
                 ulong rights, bool revoke);
bool mysql_table_grant(THD *thd, TABLE_LIST *table, List <LEX_USER> &user_list,
                       List <LEX_COLUMN> &column_list, ulong rights,
                       bool revoke);
bool mysql_routine_grant(THD *thd, TABLE_LIST *table, bool is_proc,
			 List <LEX_USER> &user_list, ulong rights,
			 bool revoke, bool no_error);
ACL_USER *check_acl_user(LEX_USER *user_name, uint *acl_acl_userdx);
my_bool grant_init();
void grant_free(void);
my_bool grant_reload(THD *thd);
bool check_grant(THD *thd, ulong want_access, TABLE_LIST *tables,
		 uint show_command, uint number, bool dont_print_error);
bool check_grant_column (THD *thd, GRANT_INFO *grant,
			 const char *db_name, const char *table_name,
			 const char *name, uint length, uint show_command=0);
bool check_grant_all_columns(THD *thd, ulong want_access, GRANT_INFO *grant,
                             const char* db_name, const char *table_name,
                             Field_iterator *fields);
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
bool mysql_revoke_all(THD *thd, List <LEX_USER> &list);
void fill_effective_table_privileges(THD *thd, GRANT_INFO *grant,
                                     const char *db, const char *table);
bool sp_revoke_privileges(THD *thd, const char *sp_db, const char *sp_name,
                          bool is_proc);
bool sp_grant_privileges(THD *thd, const char *sp_db, const char *sp_name,
                         bool is_proc);
bool check_routine_level_acl(THD *thd, const char *db, const char *name,
                             bool is_proc);

#ifdef NO_EMBEDDED_ACCESS_CHECKS
#define check_grant(A,B,C,D,E,F) 0
#define check_grant_db(A,B) 0
#endif
