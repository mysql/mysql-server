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


#define DB_ACLS \
(UPDATE_ACL | SELECT_ACL | INSERT_ACL | DELETE_ACL | CREATE_ACL | DROP_ACL | \
 GRANT_ACL | REFERENCES_ACL | INDEX_ACL | ALTER_ACL | CREATE_TMP_ACL | LOCK_TABLES_ACL)

#define TABLE_ACLS \
(SELECT_ACL | INSERT_ACL | UPDATE_ACL | DELETE_ACL | CREATE_ACL | DROP_ACL | \
 GRANT_ACL | REFERENCES_ACL | INDEX_ACL | ALTER_ACL)

#define COL_ACLS \
(SELECT_ACL | INSERT_ACL | UPDATE_ACL | REFERENCES_ACL)

#define GLOBAL_ACLS \
(SELECT_ACL | INSERT_ACL | UPDATE_ACL | DELETE_ACL | CREATE_ACL | DROP_ACL | \
 RELOAD_ACL | SHUTDOWN_ACL | PROCESS_ACL | FILE_ACL | GRANT_ACL | \
 REFERENCES_ACL | INDEX_ACL | ALTER_ACL | SHOW_DB_ACL | SUPER_ACL | \
 CREATE_TMP_ACL | LOCK_TABLES_ACL | REPL_SLAVE_ACL | REPL_CLIENT_ACL | \
 EXECUTE_ACL)

#define EXTRA_ACL	(1L << 29)
#define NO_ACCESS	(1L << 30)

/*
  Defines to change the above bits to how things are stored in tables
  This is needed as the 'host' and 'db' table is missing a few privileges
*/

/* Continius bit-segments that needs to be shifted */
#define DB_REL1 (RELOAD_ACL | SHUTDOWN_ACL | PROCESS_ACL | FILE_ACL)
#define DB_REL2 (GRANT_ACL | REFERENCES_ACL)

/* Privileges that needs to be reallocated (in continous chunks) */
#define DB_CHUNK1 (GRANT_ACL | REFERENCES_ACL | INDEX_ACL | ALTER_ACL)
#define DB_CHUNK2 (CREATE_TMP_ACL | LOCK_TABLES_ACL)

#define fix_rights_for_db(A) (((A) & 63) | (((A) & DB_REL1) << 4) | (((A) & DB_REL2) << 6))
#define get_rights_for_db(A) (((A) & 63) | (((A) & DB_CHUNK1) >> 4) | (((A) & DB_CHUNK2) >> 6))
#define fix_rights_for_table(A) (((A) & 63) | (((A) & ~63) << 4))
#define get_rights_for_table(A) (((A) & 63) | (((A) & ~63) >> 4))
#define fix_rights_for_column(A) (((A) & COL_ACLS) | ((A & ~COL_ACLS) << 7))
#define get_rights_for_column(A) (((A) & COL_ACLS) | ((A & ~COL_ACLS) >> 7))

/* prototypes */

my_bool  acl_init(THD *thd, bool dont_read_acl_tables);
void acl_reload(THD *thd);
void acl_free(bool end=0);
ulong acl_get(const char *host, const char *ip, const char *bin_ip,
	      const char *user, const char *db);
ulong acl_getroot(THD *thd, const char *host, const char *ip, const char *user,
		  const char *password,const char *scramble,char **priv_user,
		  bool old_ver, USER_RESOURCES *max);
bool acl_check_host(const char *host, const char *ip);
bool check_change_password(THD *thd, const char *host, const char *user);
bool change_password(THD *thd, const char *host, const char *user,
		     char *password);
int mysql_grant(THD *thd, const char *db, List <LEX_USER> &user_list,
		ulong rights, bool revoke);
int mysql_table_grant(THD *thd, TABLE_LIST *table, List <LEX_USER> &user_list,
		      List <LEX_COLUMN> &column_list, ulong rights,
		      bool revoke);
my_bool grant_init(THD *thd);
void grant_free(void);
void grant_reload(THD *thd);
bool check_grant(THD *thd, ulong want_access, TABLE_LIST *tables,
		 uint show_command=0, bool dont_print_error=0);
bool check_grant_column (THD *thd,TABLE *table, const char *name, uint length,
			 uint show_command=0);
bool check_grant_all_columns(THD *thd, ulong want_access, TABLE *table);
bool check_grant_db(THD *thd,const char *db);
ulong get_table_grant(THD *thd, TABLE_LIST *table);
ulong get_column_grant(THD *thd, TABLE_LIST *table, Field *field);
int mysql_show_grants(THD *thd, LEX_USER *user);
void get_privilege_desc(char *to, uint max_length, ulong access);
void get_mqh(const char *user, const char *host, USER_CONN *uc);
