/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/*
  The privileges are saved in the following tables:
  mysql/user	 ; super user who are allowed to do almost anything
  mysql/host	 ; host privileges. This is used if host is empty in mysql/db.
  mysql/db	 ; database privileges / user

  data in tables is sorted according to how many not-wild-cards there is
  in the relevant fields. Empty strings comes last.
*/

#include "mysql_priv.h"
#include "hash_filo.h"
#include <m_ctype.h>
#include <stdarg.h>
#include "sp_head.h"
#include "sp.h"

static const
TABLE_FIELD_TYPE mysql_db_table_fields[MYSQL_DB_FIELD_COUNT] = {
  {
    { C_STRING_WITH_LEN("Host") },            
    { C_STRING_WITH_LEN("char(60)") },
    {NULL, 0}
  }, 
  {
    { C_STRING_WITH_LEN("Db") },            
    { C_STRING_WITH_LEN("char(64)") },
    {NULL, 0}
  }, 
  {
    { C_STRING_WITH_LEN("User") },
    { C_STRING_WITH_LEN("char(16)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("Select_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Insert_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Update_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Delete_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Create_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Drop_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Grant_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("References_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Index_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Alter_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Create_tmp_table_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Lock_tables_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Create_view_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Show_view_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Create_routine_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Alter_routine_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Execute_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Event_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("Trigger_priv") },
    { C_STRING_WITH_LEN("enum('N','Y')") },
    { C_STRING_WITH_LEN("utf8") }
  }
};

const TABLE_FIELD_DEF
  mysql_db_table_def= {MYSQL_DB_FIELD_COUNT, mysql_db_table_fields};

#ifndef NO_EMBEDDED_ACCESS_CHECKS

#define FIRST_NON_YN_FIELD 26

class acl_entry :public hash_filo_element
{
public:
  ulong access;
  uint16 length;
  char key[1];					// Key will be stored here
};


static uchar* acl_entry_get_key(acl_entry *entry, size_t *length,
                                my_bool not_used __attribute__((unused)))
{
  *length=(uint) entry->length;
  return (uchar*) entry->key;
}

#define IP_ADDR_STRLEN (3+1+3+1+3+1+3)
#define ACL_KEY_LENGTH (IP_ADDR_STRLEN+1+NAME_LEN+1+USERNAME_LENGTH+1)

static DYNAMIC_ARRAY acl_hosts,acl_users,acl_dbs;
static MEM_ROOT mem, memex;
static bool initialized=0;
static bool allow_all_hosts=1;
static HASH acl_check_hosts, column_priv_hash, proc_priv_hash, func_priv_hash;
static DYNAMIC_ARRAY acl_wild_hosts;
static hash_filo *acl_cache;
static uint grant_version=0; /* Version of priv tables. incremented by acl_load */
static ulong get_access(TABLE *form,uint fieldnr, uint *next_field=0);
static int acl_compare(ACL_ACCESS *a,ACL_ACCESS *b);
static ulong get_sort(uint count,...);
static void init_check_host(void);
static void rebuild_check_host(void);
static ACL_USER *find_acl_user(const char *host, const char *user,
                               my_bool exact);
static bool update_user_table(THD *thd, TABLE *table,
                              const char *host, const char *user,
			      const char *new_password, uint new_password_len);
static void update_hostname(acl_host_and_ip *host, const char *hostname);
static bool compare_hostname(const acl_host_and_ip *host,const char *hostname,
			     const char *ip);
static my_bool acl_load(THD *thd, TABLE_LIST *tables);
static my_bool grant_load(THD *thd, TABLE_LIST *tables);
static inline void get_grantor(THD *thd, char* grantor);
/*
 Enumeration of various ACL's and Hashes used in handle_grant_struct()
*/
enum enum_acl_lists
{
  USER_ACL= 0,
  DB_ACL,
  COLUMN_PRIVILEGES_HASH,
  PROC_PRIVILEGES_HASH,
  FUNC_PRIVILEGES_HASH
};
/*
  Convert scrambled password to binary form, according to scramble type, 
  Binary form is stored in user.salt.
*/

static
void
set_user_salt(ACL_USER *acl_user, const char *password, uint password_len)
{
  if (password_len == SCRAMBLED_PASSWORD_CHAR_LENGTH)
  {
    get_salt_from_password(acl_user->salt, password);
    acl_user->salt_len= SCRAMBLE_LENGTH;
  }
  else if (password_len == SCRAMBLED_PASSWORD_CHAR_LENGTH_323)
  {
    get_salt_from_password_323((ulong *) acl_user->salt, password);
    acl_user->salt_len= SCRAMBLE_LENGTH_323;
  }
  else
    acl_user->salt_len= 0;
}

/*
  This after_update function is used when user.password is less than
  SCRAMBLE_LENGTH bytes.
*/

static void restrict_update_of_old_passwords_var(THD *thd,
                                                 enum_var_type var_type)
{
  if (var_type == OPT_GLOBAL)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    global_system_variables.old_passwords= 1;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    thd->variables.old_passwords= 1;
}


/*
  Initialize structures responsible for user/db-level privilege checking and
  load privilege information for them from tables in the 'mysql' database.

  SYNOPSIS
    acl_init()
      dont_read_acl_tables  TRUE if we want to skip loading data from
                            privilege tables and disable privilege checking.

  NOTES
    This function is mostly responsible for preparatory steps, main work
    on initialization and grants loading is done in acl_reload().

  RETURN VALUES
    0	ok
    1	Could not initialize grant's
*/

my_bool acl_init(bool dont_read_acl_tables)
{
  THD  *thd;
  my_bool return_val;
  DBUG_ENTER("acl_init");

  acl_cache= new hash_filo(ACL_CACHE_SIZE, 0, 0,
                           (hash_get_key) acl_entry_get_key,
                           (hash_free_key) free,
                           &my_charset_utf8_bin);
  if (dont_read_acl_tables)
  {
    DBUG_RETURN(0); /* purecov: tested */
  }

  /*
    To be able to run this from boot, we allocate a temporary THD
  */
  if (!(thd=new THD))
    DBUG_RETURN(1); /* purecov: inspected */
  thd->thread_stack= (char*) &thd;
  thd->store_globals();
  lex_start(thd);
  /*
    It is safe to call acl_reload() since acl_* arrays and hashes which
    will be freed there are global static objects and thus are initialized
    by zeros at startup.
  */
  return_val= acl_reload(thd);
  delete thd;
  /* Remember that we don't have a THD */
  my_pthread_setspecific_ptr(THR_THD,  0);
  DBUG_RETURN(return_val);
}


/*
  Initialize structures responsible for user/db-level privilege checking
  and load information about grants from open privilege tables.

  SYNOPSIS
    acl_load()
      thd     Current thread
      tables  List containing open "mysql.host", "mysql.user" and
              "mysql.db" tables.

  RETURN VALUES
    FALSE  Success
    TRUE   Error
*/

static my_bool acl_load(THD *thd, TABLE_LIST *tables)
{
  TABLE *table;
  READ_RECORD read_record_info;
  my_bool return_val= TRUE;
  bool check_no_resolve= specialflag & SPECIAL_NO_RESOLVE;
  char tmp_name[NAME_LEN+1];
  int password_length;
  ulong old_sql_mode= thd->variables.sql_mode;
  DBUG_ENTER("acl_load");

  thd->variables.sql_mode&= ~MODE_PAD_CHAR_TO_FULL_LENGTH;

  grant_version++; /* Privileges updated */

  acl_cache->clear(1);				// Clear locked hostname cache

  init_sql_alloc(&mem, ACL_ALLOC_BLOCK_SIZE, 0);
  init_read_record(&read_record_info,thd,table= tables[0].table,NULL,1,0, 
                   FALSE);
  table->use_all_columns();
  VOID(my_init_dynamic_array(&acl_hosts,sizeof(ACL_HOST),20,50));
  while (!(read_record_info.read_record(&read_record_info)))
  {
    ACL_HOST host;
    update_hostname(&host.host,get_field(&mem, table->field[0]));
    host.db=	 get_field(&mem, table->field[1]);
    if (lower_case_table_names && host.db)
    {
      /*
        convert db to lower case and give a warning if the db wasn't
        already in lower case
      */
      (void) strmov(tmp_name, host.db);
      my_casedn_str(files_charset_info, host.db);
      if (strcmp(host.db, tmp_name) != 0)
        sql_print_warning("'host' entry '%s|%s' had database in mixed "
                          "case that has been forced to lowercase because "
                          "lower_case_table_names is set. It will not be "
                          "possible to remove this privilege using REVOKE.",
                          host.host.hostname ? host.host.hostname : "",
                          host.db ? host.db : "");
    }
    host.access= get_access(table,2);
    host.access= fix_rights_for_db(host.access);
    host.sort=	 get_sort(2,host.host.hostname,host.db);
    if (check_no_resolve && hostname_requires_resolving(host.host.hostname))
    {
      sql_print_warning("'host' entry '%s|%s' "
		      "ignored in --skip-name-resolve mode.",
			host.host.hostname ? host.host.hostname : "",
			host.db ? host.db : "");
      continue;
    }
#ifndef TO_BE_REMOVED
    if (table->s->fields == 8)
    {						// Without grant
      if (host.access & CREATE_ACL)
	host.access|=REFERENCES_ACL | INDEX_ACL | ALTER_ACL | CREATE_TMP_ACL;
    }
#endif
    VOID(push_dynamic(&acl_hosts,(uchar*) &host));
  }
  my_qsort((uchar*) dynamic_element(&acl_hosts,0,ACL_HOST*),acl_hosts.elements,
	   sizeof(ACL_HOST),(qsort_cmp) acl_compare);
  end_read_record(&read_record_info);
  freeze_size(&acl_hosts);

  init_read_record(&read_record_info,thd,table=tables[1].table,NULL,1,0,FALSE);
  table->use_all_columns();
  VOID(my_init_dynamic_array(&acl_users,sizeof(ACL_USER),50,100));
  password_length= table->field[2]->field_length /
    table->field[2]->charset()->mbmaxlen;
  if (password_length < SCRAMBLED_PASSWORD_CHAR_LENGTH_323)
  {
    sql_print_error("Fatal error: mysql.user table is damaged or in "
                    "unsupported 3.20 format.");
    goto end;
  }

  DBUG_PRINT("info",("user table fields: %d, password length: %d",
		     table->s->fields, password_length));

  pthread_mutex_lock(&LOCK_global_system_variables);
  if (password_length < SCRAMBLED_PASSWORD_CHAR_LENGTH)
  {
    if (opt_secure_auth)
    {
      pthread_mutex_unlock(&LOCK_global_system_variables);
      sql_print_error("Fatal error: mysql.user table is in old format, "
                      "but server started with --secure-auth option.");
      goto end;
    }
    sys_old_passwords.after_update= restrict_update_of_old_passwords_var;
    if (global_system_variables.old_passwords)
      pthread_mutex_unlock(&LOCK_global_system_variables);
    else
    {
      global_system_variables.old_passwords= 1;
      pthread_mutex_unlock(&LOCK_global_system_variables);
      sql_print_warning("mysql.user table is not updated to new password format; "
                        "Disabling new password usage until "
                        "mysql_fix_privilege_tables is run");
    }
    thd->variables.old_passwords= 1;
  }
  else
  {
    sys_old_passwords.after_update= 0;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }

  allow_all_hosts=0;
  while (!(read_record_info.read_record(&read_record_info)))
  {
    ACL_USER user;
    update_hostname(&user.host, get_field(&mem, table->field[0]));
    user.user= get_field(&mem, table->field[1]);
    if (check_no_resolve && hostname_requires_resolving(user.host.hostname))
    {
      sql_print_warning("'user' entry '%s@%s' "
                        "ignored in --skip-name-resolve mode.",
			user.user ? user.user : "",
			user.host.hostname ? user.host.hostname : "");
      continue;
    }

    const char *password= get_field(thd->mem_root, table->field[2]);
    uint password_len= password ? strlen(password) : 0;
    set_user_salt(&user, password, password_len);
    if (user.salt_len == 0 && password_len != 0)
    {
      switch (password_len) {
      case 45: /* 4.1: to be removed */
        sql_print_warning("Found 4.1 style password for user '%s@%s'. "
                          "Ignoring user. "
                          "You should change password for this user.",
                          user.user ? user.user : "",
                          user.host.hostname ? user.host.hostname : "");
        break;
      default:
        sql_print_warning("Found invalid password for user: '%s@%s'; "
                          "Ignoring user", user.user ? user.user : "",
                           user.host.hostname ? user.host.hostname : "");
        break;
      }
    }
    else                                        // password is correct
    {
      uint next_field;
      user.access= get_access(table,3,&next_field) & GLOBAL_ACLS;
      /*
        if it is pre 5.0.1 privilege table then map CREATE privilege on
        CREATE VIEW & SHOW VIEW privileges
      */
      if (table->s->fields <= 31 && (user.access & CREATE_ACL))
        user.access|= (CREATE_VIEW_ACL | SHOW_VIEW_ACL);

      /*
        if it is pre 5.0.2 privilege table then map CREATE/ALTER privilege on
        CREATE PROCEDURE & ALTER PROCEDURE privileges
      */
      if (table->s->fields <= 33 && (user.access & CREATE_ACL))
        user.access|= CREATE_PROC_ACL;
      if (table->s->fields <= 33 && (user.access & ALTER_ACL))
        user.access|= ALTER_PROC_ACL;

      /*
        pre 5.0.3 did not have CREATE_USER_ACL
      */
      if (table->s->fields <= 36 && (user.access & GRANT_ACL))
        user.access|= CREATE_USER_ACL;


      /*
        if it is pre 5.1.6 privilege table then map CREATE privilege on
        CREATE|ALTER|DROP|EXECUTE EVENT
      */
      if (table->s->fields <= 37 && (user.access & SUPER_ACL))
        user.access|= EVENT_ACL;

      /*
        if it is pre 5.1.6 privilege then map TRIGGER privilege on CREATE.
      */
      if (table->s->fields <= 38 && (user.access & SUPER_ACL))
        user.access|= TRIGGER_ACL;

      user.sort= get_sort(2,user.host.hostname,user.user);
      user.hostname_length= (user.host.hostname ?
                             (uint) strlen(user.host.hostname) : 0);

      /* Starting from 4.0.2 we have more fields */
      if (table->s->fields >= 31)
      {
        char *ssl_type=get_field(thd->mem_root, table->field[next_field++]);
        if (!ssl_type)
          user.ssl_type=SSL_TYPE_NONE;
        else if (!strcmp(ssl_type, "ANY"))
          user.ssl_type=SSL_TYPE_ANY;
        else if (!strcmp(ssl_type, "X509"))
          user.ssl_type=SSL_TYPE_X509;
        else  /* !strcmp(ssl_type, "SPECIFIED") */
          user.ssl_type=SSL_TYPE_SPECIFIED;

        user.ssl_cipher=   get_field(&mem, table->field[next_field++]);
        user.x509_issuer=  get_field(&mem, table->field[next_field++]);
        user.x509_subject= get_field(&mem, table->field[next_field++]);

        char *ptr = get_field(thd->mem_root, table->field[next_field++]);
        user.user_resource.questions=ptr ? atoi(ptr) : 0;
        ptr = get_field(thd->mem_root, table->field[next_field++]);
        user.user_resource.updates=ptr ? atoi(ptr) : 0;
        ptr = get_field(thd->mem_root, table->field[next_field++]);
        user.user_resource.conn_per_hour= ptr ? atoi(ptr) : 0;
        if (user.user_resource.questions || user.user_resource.updates ||
            user.user_resource.conn_per_hour)
          mqh_used=1;

        if (table->s->fields >= 36)
        {
          /* Starting from 5.0.3 we have max_user_connections field */
          ptr= get_field(thd->mem_root, table->field[next_field++]);
          user.user_resource.user_conn= ptr ? atoi(ptr) : 0;
        }
        else
          user.user_resource.user_conn= 0;
      }
      else
      {
        user.ssl_type=SSL_TYPE_NONE;
        bzero((char *)&(user.user_resource),sizeof(user.user_resource));
#ifndef TO_BE_REMOVED
        if (table->s->fields <= 13)
        {						// Without grant
          if (user.access & CREATE_ACL)
            user.access|=REFERENCES_ACL | INDEX_ACL | ALTER_ACL;
        }
        /* Convert old privileges */
        user.access|= LOCK_TABLES_ACL | CREATE_TMP_ACL | SHOW_DB_ACL;
        if (user.access & FILE_ACL)
          user.access|= REPL_CLIENT_ACL | REPL_SLAVE_ACL;
        if (user.access & PROCESS_ACL)
          user.access|= SUPER_ACL | EXECUTE_ACL;
#endif
      }
      VOID(push_dynamic(&acl_users,(uchar*) &user));
      if (!user.host.hostname ||
	  (user.host.hostname[0] == wild_many && !user.host.hostname[1]))
        allow_all_hosts=1;			// Anyone can connect
    }
  }
  my_qsort((uchar*) dynamic_element(&acl_users,0,ACL_USER*),acl_users.elements,
	   sizeof(ACL_USER),(qsort_cmp) acl_compare);
  end_read_record(&read_record_info);
  freeze_size(&acl_users);

  init_read_record(&read_record_info,thd,table=tables[2].table,NULL,1,0,FALSE);
  table->use_all_columns();
  VOID(my_init_dynamic_array(&acl_dbs,sizeof(ACL_DB),50,100));
  while (!(read_record_info.read_record(&read_record_info)))
  {
    ACL_DB db;
    update_hostname(&db.host,get_field(&mem, table->field[MYSQL_DB_FIELD_HOST]));
    db.db=get_field(&mem, table->field[MYSQL_DB_FIELD_DB]);
    if (!db.db)
    {
      sql_print_warning("Found an entry in the 'db' table with empty database name; Skipped");
      continue;
    }
    db.user=get_field(&mem, table->field[MYSQL_DB_FIELD_USER]);
    if (check_no_resolve && hostname_requires_resolving(db.host.hostname))
    {
      sql_print_warning("'db' entry '%s %s@%s' "
		        "ignored in --skip-name-resolve mode.",
		        db.db,
			db.user ? db.user : "",
			db.host.hostname ? db.host.hostname : "");
      continue;
    }
    db.access=get_access(table,3);
    db.access=fix_rights_for_db(db.access);
    if (lower_case_table_names)
    {
      /*
        convert db to lower case and give a warning if the db wasn't
        already in lower case
      */
      (void)strmov(tmp_name, db.db);
      my_casedn_str(files_charset_info, db.db);
      if (strcmp(db.db, tmp_name) != 0)
      {
        sql_print_warning("'db' entry '%s %s@%s' had database in mixed "
                          "case that has been forced to lowercase because "
                          "lower_case_table_names is set. It will not be "
                          "possible to remove this privilege using REVOKE.",
		          db.db,
			  db.user ? db.user : "",
			  db.host.hostname ? db.host.hostname : "");
      }
    }
    db.sort=get_sort(3,db.host.hostname,db.db,db.user);
#ifndef TO_BE_REMOVED
    if (table->s->fields <=  9)
    {						// Without grant
      if (db.access & CREATE_ACL)
	db.access|=REFERENCES_ACL | INDEX_ACL | ALTER_ACL;
    }
#endif
    VOID(push_dynamic(&acl_dbs,(uchar*) &db));
  }
  my_qsort((uchar*) dynamic_element(&acl_dbs,0,ACL_DB*),acl_dbs.elements,
	   sizeof(ACL_DB),(qsort_cmp) acl_compare);
  end_read_record(&read_record_info);
  freeze_size(&acl_dbs);
  init_check_host();

  initialized=1;
  return_val= FALSE;

end:
  thd->variables.sql_mode= old_sql_mode;
  DBUG_RETURN(return_val);
}


void acl_free(bool end)
{
  free_root(&mem,MYF(0));
  delete_dynamic(&acl_hosts);
  delete_dynamic(&acl_users);
  delete_dynamic(&acl_dbs);
  delete_dynamic(&acl_wild_hosts);
  hash_free(&acl_check_hosts);
  if (!end)
    acl_cache->clear(1); /* purecov: inspected */
  else
  {
    delete acl_cache;
    acl_cache=0;
  }
}


/*
  Forget current user/db-level privileges and read new privileges
  from the privilege tables.

  SYNOPSIS
    acl_reload()
      thd  Current thread

  NOTE
    All tables of calling thread which were open and locked by LOCK TABLES
    statement will be unlocked and closed.
    This function is also used for initialization of structures responsible
    for user/db-level privilege checking.

  RETURN VALUE
    FALSE  Success
    TRUE   Failure
*/

my_bool acl_reload(THD *thd)
{
  TABLE_LIST tables[3];
  DYNAMIC_ARRAY old_acl_hosts,old_acl_users,old_acl_dbs;
  MEM_ROOT old_mem;
  bool old_initialized;
  my_bool return_val= TRUE;
  DBUG_ENTER("acl_reload");

  if (thd->locked_tables)
  {					// Can't have locked tables here
    thd->lock=thd->locked_tables;
    thd->locked_tables=0;
    close_thread_tables(thd);
  }

  /*
    To avoid deadlocks we should obtain table locks before
    obtaining acl_cache->lock mutex.
  */
  bzero((char*) tables, sizeof(tables));
  tables[0].alias= tables[0].table_name= (char*) "host";
  tables[1].alias= tables[1].table_name= (char*) "user";
  tables[2].alias= tables[2].table_name= (char*) "db";
  tables[0].db=tables[1].db=tables[2].db=(char*) "mysql";
  tables[0].next_local= tables[0].next_global= tables+1;
  tables[1].next_local= tables[1].next_global= tables+2;
  tables[0].lock_type=tables[1].lock_type=tables[2].lock_type=TL_READ;
  tables[0].skip_temporary= tables[1].skip_temporary=
    tables[2].skip_temporary= TRUE;

  if (simple_open_n_lock_tables(thd, tables))
  {
    /*
      Execution might have been interrupted; only print the error message
      if an error condition has been raised.
    */
    if (thd->main_da.is_error())
      sql_print_error("Fatal error: Can't open and lock privilege tables: %s",
                      thd->main_da.message());
    goto end;
  }

  if ((old_initialized=initialized))
    VOID(pthread_mutex_lock(&acl_cache->lock));

  old_acl_hosts=acl_hosts;
  old_acl_users=acl_users;
  old_acl_dbs=acl_dbs;
  old_mem=mem;
  delete_dynamic(&acl_wild_hosts);
  hash_free(&acl_check_hosts);

  if ((return_val= acl_load(thd, tables)))
  {					// Error. Revert to old list
    DBUG_PRINT("error",("Reverting to old privileges"));
    acl_free();				/* purecov: inspected */
    acl_hosts=old_acl_hosts;
    acl_users=old_acl_users;
    acl_dbs=old_acl_dbs;
    mem=old_mem;
    init_check_host();
  }
  else
  {
    free_root(&old_mem,MYF(0));
    delete_dynamic(&old_acl_hosts);
    delete_dynamic(&old_acl_users);
    delete_dynamic(&old_acl_dbs);
  }
  if (old_initialized)
    VOID(pthread_mutex_unlock(&acl_cache->lock));
end:
  close_thread_tables(thd);
  DBUG_RETURN(return_val);
}


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

static ulong get_access(TABLE *form, uint fieldnr, uint *next_field)
{
  ulong access_bits=0,bit;
  char buff[2];
  String res(buff,sizeof(buff),&my_charset_latin1);
  Field **pos;

  for (pos=form->field+fieldnr, bit=1;
       *pos && (*pos)->real_type() == MYSQL_TYPE_ENUM &&
	 ((Field_enum*) (*pos))->typelib->count == 2 ;
       pos++, fieldnr++, bit<<=1)
  {
    (*pos)->val_str(&res);
    if (my_toupper(&my_charset_latin1, res[0]) == 'Y')
      access_bits|= bit;
  }
  if (next_field)
    *next_field=fieldnr;
  return access_bits;
}


/*
  Return a number which, if sorted 'desc', puts strings in this order:
    no wildcards
    wildcards
    empty string
*/

static ulong get_sort(uint count,...)
{
  va_list args;
  va_start(args,count);
  ulong sort=0;

  /* Should not use this function with more than 4 arguments for compare. */
  DBUG_ASSERT(count <= 4);

  while (count--)
  {
    char *start, *str= va_arg(args,char*);
    uint chars= 0;
    uint wild_pos= 0;           /* first wildcard position */

    if ((start= str))
    {
      for (; *str ; str++)
      {
        if (*str == wild_prefix && str[1])
          str++;
        else if (*str == wild_many || *str == wild_one)
        {
          wild_pos= (uint) (str - start) + 1;
          break;
        }
        chars= 128;                             // Marker that chars existed
      }
    }
    sort= (sort << 8) + (wild_pos ? min(wild_pos, 127) : chars);
  }
  va_end(args);
  return sort;
}


static int acl_compare(ACL_ACCESS *a,ACL_ACCESS *b)
{
  if (a->sort > b->sort)
    return -1;
  if (a->sort < b->sort)
    return 1;
  return 0;
}


/*
  Seek ACL entry for a user, check password, SSL cypher, and if
  everything is OK, update THD user data and USER_RESOURCES struct.

  IMPLEMENTATION
   This function does not check if the user has any sensible privileges:
   only user's existence and  validity is checked.
   Note, that entire operation is protected by acl_cache_lock.

  SYNOPSIS
    acl_getroot()
    thd         thread handle. If all checks are OK,
                thd->security_ctx->priv_user/master_access are updated.
                thd->security_ctx->host/ip/user are used for checks.
    mqh         user resources; on success mqh is reset, else
                unchanged
    passwd      scrambled & crypted password, received from client
                (to check): thd->scramble or thd->scramble_323 is
                used to decrypt passwd, so they must contain
                original random string,
    passwd_len  length of passwd, must be one of 0, 8,
                SCRAMBLE_LENGTH_323, SCRAMBLE_LENGTH
    'thd' and 'mqh' are updated on success; other params are IN.
  
  RETURN VALUE
    0  success: thd->priv_user, thd->priv_host, thd->master_access, mqh are
       updated
    1  user not found or authentication failure
    2  user found, has long (4.1.1) salt, but passwd is in old (3.23) format.
   -1  user found, has short (3.23) salt, but passwd is in new (4.1.1) format.
*/

int acl_getroot(THD *thd, USER_RESOURCES  *mqh,
                const char *passwd, uint passwd_len)
{
  ulong user_access= NO_ACCESS;
  int res= 1;
  ACL_USER *acl_user= 0;
  Security_context *sctx= thd->security_ctx;
  DBUG_ENTER("acl_getroot");

  if (!initialized)
  {
    /* 
      here if mysqld's been started with --skip-grant-tables option.
    */
    sctx->skip_grants();
    bzero((char*) mqh, sizeof(*mqh));
    DBUG_RETURN(0);
  }

  VOID(pthread_mutex_lock(&acl_cache->lock));

  /*
    Find acl entry in user database. Note, that find_acl_user is not the same,
    because it doesn't take into account the case when user is not empty,
    but acl_user->user is empty
  */

  for (uint i=0 ; i < acl_users.elements ; i++)
  {
    ACL_USER *acl_user_tmp= dynamic_element(&acl_users,i,ACL_USER*);
    if (!acl_user_tmp->user || !strcmp(sctx->user, acl_user_tmp->user))
    {
      if (compare_hostname(&acl_user_tmp->host, sctx->host, sctx->ip))
      {
        /* check password: it should be empty or valid */
        if (passwd_len == acl_user_tmp->salt_len)
        {
          if (acl_user_tmp->salt_len == 0 ||
              (acl_user_tmp->salt_len == SCRAMBLE_LENGTH ?
              check_scramble(passwd, thd->scramble, acl_user_tmp->salt) :
              check_scramble_323(passwd, thd->scramble,
                                 (ulong *) acl_user_tmp->salt)) == 0)
          {
            acl_user= acl_user_tmp;
            res= 0;
          }
        }
        else if (passwd_len == SCRAMBLE_LENGTH &&
                 acl_user_tmp->salt_len == SCRAMBLE_LENGTH_323)
          res= -1;
        else if (passwd_len == SCRAMBLE_LENGTH_323 &&
                 acl_user_tmp->salt_len == SCRAMBLE_LENGTH)
          res= 2;
        /* linear search complete: */
        break;
      }
    }
  }
  /*
    This was moved to separate tree because of heavy HAVE_OPENSSL case.
    If acl_user is not null, res is 0.
  */

  if (acl_user)
  {
    /* OK. User found and password checked continue validation */
#ifdef HAVE_OPENSSL
    Vio *vio=thd->net.vio;
    SSL *ssl= (SSL*) vio->ssl_arg;
    X509 *cert;
#endif

    /*
      At this point we know that user is allowed to connect
      from given host by given username/password pair. Now
      we check if SSL is required, if user is using SSL and
      if X509 certificate attributes are OK
    */
    switch (acl_user->ssl_type) {
    case SSL_TYPE_NOT_SPECIFIED:		// Impossible
    case SSL_TYPE_NONE:				// SSL is not required
      user_access= acl_user->access;
      break;
#ifdef HAVE_OPENSSL
    case SSL_TYPE_ANY:				// Any kind of SSL is ok
      if (vio_type(vio) == VIO_TYPE_SSL)
	user_access= acl_user->access;
      break;
    case SSL_TYPE_X509: /* Client should have any valid certificate. */
      /*
	Connections with non-valid certificates are dropped already
	in sslaccept() anyway, so we do not check validity here.

	We need to check for absence of SSL because without SSL
	we should reject connection.
      */
      if (vio_type(vio) == VIO_TYPE_SSL &&
	  SSL_get_verify_result(ssl) == X509_V_OK &&
	  (cert= SSL_get_peer_certificate(ssl)))
      {
	user_access= acl_user->access;
        X509_free(cert);
      }
      break;
    case SSL_TYPE_SPECIFIED: /* Client should have specified attrib */
      /*
	We do not check for absence of SSL because without SSL it does
	not pass all checks here anyway.
	If cipher name is specified, we compare it to actual cipher in
	use.
      */
      if (vio_type(vio) != VIO_TYPE_SSL ||
	  SSL_get_verify_result(ssl) != X509_V_OK)
	break;
      if (acl_user->ssl_cipher)
      {
	DBUG_PRINT("info",("comparing ciphers: '%s' and '%s'",
			   acl_user->ssl_cipher,SSL_get_cipher(ssl)));
	if (!strcmp(acl_user->ssl_cipher,SSL_get_cipher(ssl)))
	  user_access= acl_user->access;
	else
	{
	  if (global_system_variables.log_warnings)
	    sql_print_information("X509 ciphers mismatch: should be '%s' but is '%s'",
			      acl_user->ssl_cipher,
			      SSL_get_cipher(ssl));
	  break;
	}
      }
      /* Prepare certificate (if exists) */
      DBUG_PRINT("info",("checkpoint 1"));
      if (!(cert= SSL_get_peer_certificate(ssl)))
      {
	user_access=NO_ACCESS;
	break;
      }
      DBUG_PRINT("info",("checkpoint 2"));
      /* If X509 issuer is specified, we check it... */
      if (acl_user->x509_issuer)
      {
        DBUG_PRINT("info",("checkpoint 3"));
        char *ptr = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        DBUG_PRINT("info",("comparing issuers: '%s' and '%s'",
			   acl_user->x509_issuer, ptr));
        if (strcmp(acl_user->x509_issuer, ptr))
        {
          if (global_system_variables.log_warnings)
            sql_print_information("X509 issuer mismatch: should be '%s' "
			      "but is '%s'", acl_user->x509_issuer, ptr);
          free(ptr);
          X509_free(cert);
          user_access=NO_ACCESS;
          break;
        }
        user_access= acl_user->access;
        free(ptr);
      }
      DBUG_PRINT("info",("checkpoint 4"));
      /* X509 subject is specified, we check it .. */
      if (acl_user->x509_subject)
      {
        char *ptr= X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        DBUG_PRINT("info",("comparing subjects: '%s' and '%s'",
                           acl_user->x509_subject, ptr));
        if (strcmp(acl_user->x509_subject,ptr))
        {
          if (global_system_variables.log_warnings)
            sql_print_information("X509 subject mismatch: should be '%s' but is '%s'",
                            acl_user->x509_subject, ptr);
          free(ptr);
          X509_free(cert);
          user_access=NO_ACCESS;
          break;
        }
        user_access= acl_user->access;
        free(ptr);
      }
      /* Deallocate the X509 certificate. */
      X509_free(cert);
      break;
#else  /* HAVE_OPENSSL */
    default:
      /*
        If we don't have SSL but SSL is required for this user the 
        authentication should fail.
      */
      break;
#endif /* HAVE_OPENSSL */
    }
    sctx->master_access= user_access;
    sctx->priv_user= acl_user->user ? sctx->user : (char *) "";
    *mqh= acl_user->user_resource;

    if (acl_user->host.hostname)
      strmake(sctx->priv_host, acl_user->host.hostname, MAX_HOSTNAME - 1);
    else
      *sctx->priv_host= 0;
  }
  VOID(pthread_mutex_unlock(&acl_cache->lock));
  DBUG_RETURN(res);
}


/*
  This is like acl_getroot() above, but it doesn't check password,
  and we don't care about the user resources.

  SYNOPSIS
    acl_getroot_no_password()
      sctx               Context which should be initialized
      user               user name
      host               host name
      ip                 IP
      db                 current data base name

  RETURN
    FALSE  OK
    TRUE   Error
*/

bool acl_getroot_no_password(Security_context *sctx, char *user, char *host,
                             char *ip, char *db)
{
  int res= 1;
  uint i;
  ACL_USER *acl_user= 0;
  DBUG_ENTER("acl_getroot_no_password");

  DBUG_PRINT("enter", ("Host: '%s', Ip: '%s', User: '%s', db: '%s'",
                       (host ? host : "(NULL)"), (ip ? ip : "(NULL)"),
                       user, (db ? db : "(NULL)")));
  sctx->user= user;
  sctx->host= host;
  sctx->ip= ip;
  sctx->host_or_ip= host ? host : (ip ? ip : "");

  if (!initialized)
  {
    /*
      here if mysqld's been started with --skip-grant-tables option.
    */
    sctx->skip_grants();
    DBUG_RETURN(FALSE);
  }

  VOID(pthread_mutex_lock(&acl_cache->lock));

  sctx->master_access= 0;
  sctx->db_access= 0;
  sctx->priv_user= (char *) "";
  *sctx->priv_host= 0;

  /*
     Find acl entry in user database.
     This is specially tailored to suit the check we do for CALL of
     a stored procedure; user is set to what is actually a
     priv_user, which can be ''.
  */
  for (i=0 ; i < acl_users.elements ; i++)
  {
    ACL_USER *acl_user_tmp= dynamic_element(&acl_users,i,ACL_USER*);
    if ((!acl_user_tmp->user && !user[0]) ||
        (acl_user_tmp->user && strcmp(user, acl_user_tmp->user) == 0))
    {
      if (compare_hostname(&acl_user_tmp->host, host, ip))
      {
        acl_user= acl_user_tmp;
        res= 0;
        break;
      }
    }
  }

  if (acl_user)
  {
    for (i=0 ; i < acl_dbs.elements ; i++)
    {
      ACL_DB *acl_db= dynamic_element(&acl_dbs, i, ACL_DB*);
      if (!acl_db->user ||
	  (user && user[0] && !strcmp(user, acl_db->user)))
      {
	if (compare_hostname(&acl_db->host, host, ip))
	{
	  if (!acl_db->db || (db && !wild_compare(db, acl_db->db, 0)))
	  {
	    sctx->db_access= acl_db->access;
	    break;
	  }
	}
      }
    }
    sctx->master_access= acl_user->access;
    sctx->priv_user= acl_user->user ? user : (char *) "";

    if (acl_user->host.hostname)
      strmake(sctx->priv_host, acl_user->host.hostname, MAX_HOSTNAME - 1);
    else
      *sctx->priv_host= 0;
  }
  VOID(pthread_mutex_unlock(&acl_cache->lock));
  DBUG_RETURN(res);
}

static uchar* check_get_key(ACL_USER *buff, size_t *length,
                            my_bool not_used __attribute__((unused)))
{
  *length=buff->hostname_length;
  return (uchar*) buff->host.hostname;
}


static void acl_update_user(const char *user, const char *host,
			    const char *password, uint password_len,
			    enum SSL_type ssl_type,
			    const char *ssl_cipher,
			    const char *x509_issuer,
			    const char *x509_subject,
			    USER_RESOURCES  *mqh,
			    ulong privileges)
{
  safe_mutex_assert_owner(&acl_cache->lock);

  for (uint i=0 ; i < acl_users.elements ; i++)
  {
    ACL_USER *acl_user=dynamic_element(&acl_users,i,ACL_USER*);
    if ((!acl_user->user && !user[0]) ||
	(acl_user->user && !strcmp(user,acl_user->user)))
    {
      if ((!acl_user->host.hostname && !host[0]) ||
	  (acl_user->host.hostname &&
	  !my_strcasecmp(system_charset_info, host, acl_user->host.hostname)))
      {
	acl_user->access=privileges;
	if (mqh->specified_limits & USER_RESOURCES::QUERIES_PER_HOUR)
	  acl_user->user_resource.questions=mqh->questions;
	if (mqh->specified_limits & USER_RESOURCES::UPDATES_PER_HOUR)
	  acl_user->user_resource.updates=mqh->updates;
	if (mqh->specified_limits & USER_RESOURCES::CONNECTIONS_PER_HOUR)
	  acl_user->user_resource.conn_per_hour= mqh->conn_per_hour;
	if (mqh->specified_limits & USER_RESOURCES::USER_CONNECTIONS)
	  acl_user->user_resource.user_conn= mqh->user_conn;
	if (ssl_type != SSL_TYPE_NOT_SPECIFIED)
	{
	  acl_user->ssl_type= ssl_type;
	  acl_user->ssl_cipher= (ssl_cipher ? strdup_root(&mem,ssl_cipher) :
				 0);
	  acl_user->x509_issuer= (x509_issuer ? strdup_root(&mem,x509_issuer) :
				  0);
	  acl_user->x509_subject= (x509_subject ?
				   strdup_root(&mem,x509_subject) : 0);
	}
	if (password)
	  set_user_salt(acl_user, password, password_len);
        /* search complete: */
	break;
      }
    }
  }
}


static void acl_insert_user(const char *user, const char *host,
			    const char *password, uint password_len,
			    enum SSL_type ssl_type,
			    const char *ssl_cipher,
			    const char *x509_issuer,
			    const char *x509_subject,
			    USER_RESOURCES *mqh,
			    ulong privileges)
{
  ACL_USER acl_user;

  safe_mutex_assert_owner(&acl_cache->lock);

  acl_user.user=*user ? strdup_root(&mem,user) : 0;
  update_hostname(&acl_user.host, *host ? strdup_root(&mem, host): 0);
  acl_user.access=privileges;
  acl_user.user_resource = *mqh;
  acl_user.sort=get_sort(2,acl_user.host.hostname,acl_user.user);
  acl_user.hostname_length=(uint) strlen(host);
  acl_user.ssl_type= (ssl_type != SSL_TYPE_NOT_SPECIFIED ?
		      ssl_type : SSL_TYPE_NONE);
  acl_user.ssl_cipher=	ssl_cipher   ? strdup_root(&mem,ssl_cipher) : 0;
  acl_user.x509_issuer= x509_issuer  ? strdup_root(&mem,x509_issuer) : 0;
  acl_user.x509_subject=x509_subject ? strdup_root(&mem,x509_subject) : 0;

  set_user_salt(&acl_user, password, password_len);

  VOID(push_dynamic(&acl_users,(uchar*) &acl_user));
  if (!acl_user.host.hostname ||
      (acl_user.host.hostname[0] == wild_many && !acl_user.host.hostname[1]))
    allow_all_hosts=1;		// Anyone can connect /* purecov: tested */
  my_qsort((uchar*) dynamic_element(&acl_users,0,ACL_USER*),acl_users.elements,
	   sizeof(ACL_USER),(qsort_cmp) acl_compare);

  /* Rebuild 'acl_check_hosts' since 'acl_users' has been modified */
  rebuild_check_host();
}


static void acl_update_db(const char *user, const char *host, const char *db,
			  ulong privileges)
{
  safe_mutex_assert_owner(&acl_cache->lock);

  for (uint i=0 ; i < acl_dbs.elements ; i++)
  {
    ACL_DB *acl_db=dynamic_element(&acl_dbs,i,ACL_DB*);
    if ((!acl_db->user && !user[0]) ||
	(acl_db->user &&
	!strcmp(user,acl_db->user)))
    {
      if ((!acl_db->host.hostname && !host[0]) ||
	  (acl_db->host.hostname &&
          !strcmp(host, acl_db->host.hostname)))
      {
	if ((!acl_db->db && !db[0]) ||
	    (acl_db->db && !strcmp(db,acl_db->db)))
	{
	  if (privileges)
	    acl_db->access=privileges;
	  else
	    delete_dynamic_element(&acl_dbs,i);
	}
      }
    }
  }
}


/*
  Insert a user/db/host combination into the global acl_cache

  SYNOPSIS
    acl_insert_db()
    user		User name
    host		Host name
    db			Database name
    privileges		Bitmap of privileges

  NOTES
    acl_cache->lock must be locked when calling this
*/

static void acl_insert_db(const char *user, const char *host, const char *db,
			  ulong privileges)
{
  ACL_DB acl_db;
  safe_mutex_assert_owner(&acl_cache->lock);
  acl_db.user=strdup_root(&mem,user);
  update_hostname(&acl_db.host, *host ? strdup_root(&mem,host) : 0);
  acl_db.db=strdup_root(&mem,db);
  acl_db.access=privileges;
  acl_db.sort=get_sort(3,acl_db.host.hostname,acl_db.db,acl_db.user);
  VOID(push_dynamic(&acl_dbs,(uchar*) &acl_db));
  my_qsort((uchar*) dynamic_element(&acl_dbs,0,ACL_DB*),acl_dbs.elements,
	   sizeof(ACL_DB),(qsort_cmp) acl_compare);
}



/*
  Get privilege for a host, user and db combination

  as db_is_pattern changes the semantics of comparison,
  acl_cache is not used if db_is_pattern is set.
*/

ulong acl_get(const char *host, const char *ip,
              const char *user, const char *db, my_bool db_is_pattern)
{
  ulong host_access= ~(ulong)0, db_access= 0;
  uint i;
  size_t key_length, copy_length;
  char key[ACL_KEY_LENGTH],*tmp_db,*end;
  acl_entry *entry;
  DBUG_ENTER("acl_get");
  
  copy_length= (size_t) (strlen(ip ? ip : "") +
                 strlen(user ? user : "") +
                 strlen(db ? db : ""));
  /*
    Make sure that strmov() operations do not result in buffer overflow.
  */
  if (copy_length >= ACL_KEY_LENGTH)
    DBUG_RETURN(0);

  VOID(pthread_mutex_lock(&acl_cache->lock));
  end=strmov((tmp_db=strmov(strmov(key, ip ? ip : "")+1,user)+1),db);
  if (lower_case_table_names)
  {
    my_casedn_str(files_charset_info, tmp_db);
    db=tmp_db;
  }
  key_length= (size_t) (end-key);
  if (!db_is_pattern && (entry=(acl_entry*) acl_cache->search((uchar*) key,
                                                              key_length)))
  {
    db_access=entry->access;
    VOID(pthread_mutex_unlock(&acl_cache->lock));
    DBUG_PRINT("exit", ("access: 0x%lx", db_access));
    DBUG_RETURN(db_access);
  }

  /*
    Check if there are some access rights for database and user
  */
  for (i=0 ; i < acl_dbs.elements ; i++)
  {
    ACL_DB *acl_db=dynamic_element(&acl_dbs,i,ACL_DB*);
    if (!acl_db->user || !strcmp(user,acl_db->user))
    {
      if (compare_hostname(&acl_db->host,host,ip))
      {
	if (!acl_db->db || !wild_compare(db,acl_db->db,db_is_pattern))
	{
	  db_access=acl_db->access;
	  if (acl_db->host.hostname)
	    goto exit;				// Fully specified. Take it
	  break; /* purecov: tested */
	}
      }
    }
  }
  if (!db_access)
    goto exit;					// Can't be better

  /*
    No host specified for user. Get hostdata from host table
  */
  host_access=0;				// Host must be found
  for (i=0 ; i < acl_hosts.elements ; i++)
  {
    ACL_HOST *acl_host=dynamic_element(&acl_hosts,i,ACL_HOST*);
    if (compare_hostname(&acl_host->host,host,ip))
    {
      if (!acl_host->db || !wild_compare(db,acl_host->db,db_is_pattern))
      {
	host_access=acl_host->access;		// Fully specified. Take it
	break;
      }
    }
  }
exit:
  /* Save entry in cache for quick retrieval */
  if (!db_is_pattern &&
      (entry= (acl_entry*) malloc(sizeof(acl_entry)+key_length)))
  {
    entry->access=(db_access & host_access);
    entry->length=key_length;
    memcpy((uchar*) entry->key,key,key_length);
    acl_cache->add(entry);
  }
  VOID(pthread_mutex_unlock(&acl_cache->lock));
  DBUG_PRINT("exit", ("access: 0x%lx", db_access & host_access));
  DBUG_RETURN(db_access & host_access);
}

/*
  Check if there are any possible matching entries for this host

  NOTES
    All host names without wild cards are stored in a hash table,
    entries with wildcards are stored in a dynamic array
*/

static void init_check_host(void)
{
  DBUG_ENTER("init_check_host");
  VOID(my_init_dynamic_array(&acl_wild_hosts,sizeof(struct acl_host_and_ip),
			  acl_users.elements,1));
  VOID(hash_init(&acl_check_hosts,system_charset_info,acl_users.elements,0,0,
		 (hash_get_key) check_get_key,0,0));
  if (!allow_all_hosts)
  {
    for (uint i=0 ; i < acl_users.elements ; i++)
    {
      ACL_USER *acl_user=dynamic_element(&acl_users,i,ACL_USER*);
      if (strchr(acl_user->host.hostname,wild_many) ||
	  strchr(acl_user->host.hostname,wild_one) ||
	  acl_user->host.ip_mask)
      {						// Has wildcard
	uint j;
	for (j=0 ; j < acl_wild_hosts.elements ; j++)
	{					// Check if host already exists
	  acl_host_and_ip *acl=dynamic_element(&acl_wild_hosts,j,
					       acl_host_and_ip *);
	  if (!my_strcasecmp(system_charset_info,
                             acl_user->host.hostname, acl->hostname))
	    break;				// already stored
	}
	if (j == acl_wild_hosts.elements)	// If new
	  (void) push_dynamic(&acl_wild_hosts,(uchar*) &acl_user->host);
      }
      else if (!hash_search(&acl_check_hosts,(uchar*) acl_user->host.hostname,
			    strlen(acl_user->host.hostname)))
      {
	if (my_hash_insert(&acl_check_hosts,(uchar*) acl_user))
	{					// End of memory
	  allow_all_hosts=1;			// Should never happen
	  DBUG_VOID_RETURN;
	}
      }
    }
  }
  freeze_size(&acl_wild_hosts);
  freeze_size(&acl_check_hosts.array);
  DBUG_VOID_RETURN;
}


/*
  Rebuild lists used for checking of allowed hosts

  We need to rebuild 'acl_check_hosts' and 'acl_wild_hosts' after adding,
  dropping or renaming user, since they contain pointers to elements of
  'acl_user' array, which are invalidated by drop operation, and use
  ACL_USER::host::hostname as a key, which is changed by rename.
*/
void rebuild_check_host(void)
{
  delete_dynamic(&acl_wild_hosts);
  hash_free(&acl_check_hosts);
  init_check_host();
}


/* Return true if there is no users that can match the given host */

bool acl_check_host(const char *host, const char *ip)
{
  if (allow_all_hosts)
    return 0;
  VOID(pthread_mutex_lock(&acl_cache->lock));

  if ((host && hash_search(&acl_check_hosts,(uchar*) host,strlen(host))) ||
      (ip && hash_search(&acl_check_hosts,(uchar*) ip, strlen(ip))))
  {
    VOID(pthread_mutex_unlock(&acl_cache->lock));
    return 0;					// Found host
  }
  for (uint i=0 ; i < acl_wild_hosts.elements ; i++)
  {
    acl_host_and_ip *acl=dynamic_element(&acl_wild_hosts,i,acl_host_and_ip*);
    if (compare_hostname(acl, host, ip))
    {
      VOID(pthread_mutex_unlock(&acl_cache->lock));
      return 0;					// Host ok
    }
  }
  VOID(pthread_mutex_unlock(&acl_cache->lock));
  return 1;					// Host is not allowed
}


/*
  Check if the user is allowed to change password

  SYNOPSIS:
    check_change_password()
    thd		THD
    host	hostname for the user
    user	user name
    new_password new password

  NOTE:
    new_password cannot be NULL

    RETURN VALUE
      0		OK
      1		ERROR  ; In this case the error is sent to the client.
*/

int check_change_password(THD *thd, const char *host, const char *user,
                           char *new_password, uint new_password_len)
{
  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    return(1);
  }
  if (!thd->slave_thread &&
      (strcmp(thd->security_ctx->user, user) ||
       my_strcasecmp(system_charset_info, host,
                     thd->security_ctx->priv_host)))
  {
    if (check_access(thd, UPDATE_ACL, "mysql",0,1,0,0))
      return(1);
  }
  if (!thd->slave_thread && !thd->security_ctx->user[0])
  {
    my_message(ER_PASSWORD_ANONYMOUS_USER, ER(ER_PASSWORD_ANONYMOUS_USER),
               MYF(0));
    return(1);
  }
  size_t len= strlen(new_password);
  if (len && len != SCRAMBLED_PASSWORD_CHAR_LENGTH &&
      len != SCRAMBLED_PASSWORD_CHAR_LENGTH_323)
  {
    my_error(ER_PASSWD_LENGTH, MYF(0), SCRAMBLED_PASSWORD_CHAR_LENGTH);
    return -1;
  }
  return(0);
}


/*
  Change a password for a user

  SYNOPSIS
    change_password()
    thd			Thread handle
    host		Hostname
    user		User name
    new_password	New password for host@user

  RETURN VALUES
    0	ok
    1	ERROR; In this case the error is sent to the client.
*/

bool change_password(THD *thd, const char *host, const char *user,
		     char *new_password)
{
  TABLE_LIST tables;
  TABLE *table;
  /* Buffer should be extended when password length is extended. */
  char buff[512];
  ulong query_length;
  bool save_binlog_row_based;
  uint new_password_len= (uint) strlen(new_password);
  bool result= 1;
  DBUG_ENTER("change_password");
  DBUG_PRINT("enter",("host: '%s'  user: '%s'  new_password: '%s'",
		      host,user,new_password));
  DBUG_ASSERT(host != 0);			// Ensured by parent

  if (check_change_password(thd, host, user, new_password, new_password_len))
    DBUG_RETURN(1);

  bzero((char*) &tables, sizeof(tables));
  tables.alias= tables.table_name= (char*) "user";
  tables.db= (char*) "mysql";

#ifdef HAVE_REPLICATION
  /*
    GRANT and REVOKE are applied the slave in/exclusion rules as they are
    some kind of updates to the mysql.% tables.
  */
  if (thd->slave_thread && rpl_filter->is_on())
  {
    /*
      The tables must be marked "updating" so that tables_ok() takes them into
      account in tests.  It's ok to leave 'updating' set after tables_ok.
    */
    tables.updating= 1;
    /* Thanks to bzero, tables.next==0 */
    if (!(thd->spcont || rpl_filter->tables_ok(0, &tables)))
      DBUG_RETURN(0);
  }
#endif

  if (!(table= open_ltable(thd, &tables, TL_WRITE, 0)))
    DBUG_RETURN(1);

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  if ((save_binlog_row_based= thd->current_stmt_binlog_row_based))
    thd->clear_current_stmt_binlog_row_based();

  VOID(pthread_mutex_lock(&acl_cache->lock));
  ACL_USER *acl_user;
  if (!(acl_user= find_acl_user(host, user, TRUE)))
  {
    VOID(pthread_mutex_unlock(&acl_cache->lock));
    my_message(ER_PASSWORD_NO_MATCH, ER(ER_PASSWORD_NO_MATCH), MYF(0));
    goto end;
  }
  /* update loaded acl entry: */
  set_user_salt(acl_user, new_password, new_password_len);

  if (update_user_table(thd, table,
			acl_user->host.hostname ? acl_user->host.hostname : "",
			acl_user->user ? acl_user->user : "",
			new_password, new_password_len))
  {
    VOID(pthread_mutex_unlock(&acl_cache->lock)); /* purecov: deadcode */
    goto end;
  }

  acl_cache->clear(1);				// Clear locked hostname cache
  VOID(pthread_mutex_unlock(&acl_cache->lock));
  result= 0;
  if (mysql_bin_log.is_open())
  {
    query_length= sprintf(buff, "SET PASSWORD FOR '%-.120s'@'%-.120s'='%-.120s'",
                          acl_user->user ? acl_user->user : "",
                          acl_user->host.hostname ? acl_user->host.hostname : "",
                          new_password);
    thd->clear_error();
    result= thd->binlog_query(THD::MYSQL_QUERY_TYPE, buff, query_length,
                              FALSE, FALSE, 0);
  }
end:
  close_thread_tables(thd);

  /* Restore the state of binlog format */
  DBUG_ASSERT(!thd->current_stmt_binlog_row_based);
  if (save_binlog_row_based)
    thd->set_current_stmt_binlog_row_based();

  DBUG_RETURN(result);
}


/*
  Find user in ACL

  SYNOPSIS
    is_acl_user()
    host                 host name
    user                 user name

  RETURN
   FALSE  user not fond
   TRUE   there are such user
*/

bool is_acl_user(const char *host, const char *user)
{
  bool res;

  /* --skip-grants */
  if (!initialized)
    return TRUE;

  VOID(pthread_mutex_lock(&acl_cache->lock));
  res= find_acl_user(host, user, TRUE) != NULL;
  VOID(pthread_mutex_unlock(&acl_cache->lock));
  return res;
}


/*
  Find first entry that matches the current user
*/

static ACL_USER *
find_acl_user(const char *host, const char *user, my_bool exact)
{
  DBUG_ENTER("find_acl_user");
  DBUG_PRINT("enter",("host: '%s'  user: '%s'",host,user));

  safe_mutex_assert_owner(&acl_cache->lock);

  for (uint i=0 ; i < acl_users.elements ; i++)
  {
    ACL_USER *acl_user=dynamic_element(&acl_users,i,ACL_USER*);
    DBUG_PRINT("info",("strcmp('%s','%s'), compare_hostname('%s','%s'),",
                       user, acl_user->user ? acl_user->user : "",
                       host,
                       acl_user->host.hostname ? acl_user->host.hostname :
                       ""));
    if ((!acl_user->user && !user[0]) ||
	(acl_user->user && !strcmp(user,acl_user->user)))
    {
      if (exact ? !my_strcasecmp(system_charset_info, host,
                                 acl_user->host.hostname ?
				 acl_user->host.hostname : "") :
          compare_hostname(&acl_user->host,host,host))
      {
	DBUG_RETURN(acl_user);
      }
    }
  }
  DBUG_RETURN(0);
}


/*
  Comparing of hostnames

  NOTES
  A hostname may be of type:
  hostname   (May include wildcards);   monty.pp.sci.fi
  ip	   (May include wildcards);   192.168.0.0
  ip/netmask			      192.168.0.0/255.255.255.0

  A net mask of 0.0.0.0 is not allowed.
*/

static const char *calc_ip(const char *ip, long *val, char end)
{
  long ip_val,tmp;
  if (!(ip=str2int(ip,10,0,255,&ip_val)) || *ip != '.')
    return 0;
  ip_val<<=24;
  if (!(ip=str2int(ip+1,10,0,255,&tmp)) || *ip != '.')
    return 0;
  ip_val+=tmp<<16;
  if (!(ip=str2int(ip+1,10,0,255,&tmp)) || *ip != '.')
    return 0;
  ip_val+=tmp<<8;
  if (!(ip=str2int(ip+1,10,0,255,&tmp)) || *ip != end)
    return 0;
  *val=ip_val+tmp;
  return ip;
}


static void update_hostname(acl_host_and_ip *host, const char *hostname)
{
  host->hostname=(char*) hostname;             // This will not be modified!
  if (!hostname ||
      (!(hostname=calc_ip(hostname,&host->ip,'/')) ||
       !(hostname=calc_ip(hostname+1,&host->ip_mask,'\0'))))
  {
    host->ip= host->ip_mask=0;			// Not a masked ip
  }
}


static bool compare_hostname(const acl_host_and_ip *host, const char *hostname,
			     const char *ip)
{
  long tmp;
  if (host->ip_mask && ip && calc_ip(ip,&tmp,'\0'))
  {
    return (tmp & host->ip_mask) == host->ip;
  }
  return (!host->hostname ||
	  (hostname && !wild_case_compare(system_charset_info,
                                          hostname, host->hostname)) ||
	  (ip && !wild_compare(ip, host->hostname, 0)));
}

bool hostname_requires_resolving(const char *hostname)
{
  char cur;
  if (!hostname)
    return FALSE;
  size_t namelen= strlen(hostname);
  size_t lhlen= strlen(my_localhost);
  if ((namelen == lhlen) &&
      !my_strnncoll(system_charset_info, (const uchar *)hostname,  namelen,
		    (const uchar *)my_localhost, strlen(my_localhost)))
    return FALSE;
  for (; (cur=*hostname); hostname++)
  {
    if ((cur != '%') && (cur != '_') && (cur != '.') && (cur != '/') &&
	((cur < '0') || (cur > '9')))
      return TRUE;
  }
  return FALSE;
}


/*
  Update record for user in mysql.user privilege table with new password.

  SYNOPSIS
    update_user_table()
      thd               Thread handle
      table             Pointer to TABLE object for open mysql.user table
      host/user         Hostname/username pair identifying user for which
                        new password should be set
      new_password      New password
      new_password_len  Length of new password
*/

static bool update_user_table(THD *thd, TABLE *table,
                              const char *host, const char *user,
			      const char *new_password, uint new_password_len)
{
  char user_key[MAX_KEY_LENGTH];
  int error;
  DBUG_ENTER("update_user_table");
  DBUG_PRINT("enter",("user: %s  host: %s",user,host));

  table->use_all_columns();
  table->field[0]->store(host,(uint) strlen(host), system_charset_info);
  table->field[1]->store(user,(uint) strlen(user), system_charset_info);
  key_copy((uchar *) user_key, table->record[0], table->key_info,
           table->key_info->key_length);

  if (table->file->index_read_idx_map(table->record[0], 0,
                                      (uchar *) user_key, HA_WHOLE_KEY,
                                      HA_READ_KEY_EXACT))
  {
    my_message(ER_PASSWORD_NO_MATCH, ER(ER_PASSWORD_NO_MATCH),
               MYF(0));	/* purecov: deadcode */
    DBUG_RETURN(1);				/* purecov: deadcode */
  }
  store_record(table,record[1]);
  table->field[2]->store(new_password, new_password_len, system_charset_info);
  if ((error=table->file->ha_update_row(table->record[1],table->record[0])) &&
      error != HA_ERR_RECORD_IS_THE_SAME)
  {
    table->file->print_error(error,MYF(0));	/* purecov: deadcode */
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


/*
  Return 1 if we are allowed to create new users
  the logic here is: INSERT_ACL is sufficient.
  It's also a requirement in opt_safe_user_create,
  otherwise CREATE_USER_ACL is enough.
*/

static bool test_if_create_new_users(THD *thd)
{
  Security_context *sctx= thd->security_ctx;
  bool create_new_users= test(sctx->master_access & INSERT_ACL) ||
                         (!opt_safe_user_create &&
                          test(sctx->master_access & CREATE_USER_ACL));
  if (!create_new_users)
  {
    TABLE_LIST tl;
    ulong db_access;
    bzero((char*) &tl,sizeof(tl));
    tl.db=	   (char*) "mysql";
    tl.table_name=  (char*) "user";
    create_new_users= 1;

    db_access=acl_get(sctx->host, sctx->ip,
		      sctx->priv_user, tl.db, 0);
    if (!(db_access & INSERT_ACL))
    {
      if (check_grant(thd, INSERT_ACL, &tl, 0, UINT_MAX, 1))
	create_new_users=0;
    }
  }
  return create_new_users;
}


/****************************************************************************
  Handle GRANT commands
****************************************************************************/

static int replace_user_table(THD *thd, TABLE *table, const LEX_USER &combo,
			      ulong rights, bool revoke_grant,
			      bool can_create_user, bool no_auto_create)
{
  int error = -1;
  bool old_row_exists=0;
  const char *password= "";
  uint password_len= 0;
  char what= (revoke_grant) ? 'N' : 'Y';
  uchar user_key[MAX_KEY_LENGTH];
  LEX *lex= thd->lex;
  DBUG_ENTER("replace_user_table");

  safe_mutex_assert_owner(&acl_cache->lock);

  if (combo.password.str && combo.password.str[0])
  {
    if (combo.password.length != SCRAMBLED_PASSWORD_CHAR_LENGTH &&
        combo.password.length != SCRAMBLED_PASSWORD_CHAR_LENGTH_323)
    {
      my_error(ER_PASSWD_LENGTH, MYF(0), SCRAMBLED_PASSWORD_CHAR_LENGTH);
      DBUG_RETURN(-1);
    }
    password_len= combo.password.length;
    password=combo.password.str;
  }

  table->use_all_columns();
  table->field[0]->store(combo.host.str,combo.host.length,
                         system_charset_info);
  table->field[1]->store(combo.user.str,combo.user.length,
                         system_charset_info);
  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);

  if (table->file->index_read_idx_map(table->record[0], 0, user_key,
                                      HA_WHOLE_KEY,
                                      HA_READ_KEY_EXACT))
  {
    /* what == 'N' means revoke */
    if (what == 'N')
    {
      my_error(ER_NONEXISTING_GRANT, MYF(0), combo.user.str, combo.host.str);
      goto end;
    }
    /*
      There are four options which affect the process of creation of
      a new user (mysqld option --safe-create-user, 'insert' privilege
      on 'mysql.user' table, using 'GRANT' with 'IDENTIFIED BY' and
      SQL_MODE flag NO_AUTO_CREATE_USER). Below is the simplified rule
      how it should work.
      if (safe-user-create && ! INSERT_priv) => reject
      else if (identified_by) => create
      else if (no_auto_create_user) => reject
      else create

      see also test_if_create_new_users()
    */
    else if (!password_len && no_auto_create)
    {
      my_error(ER_PASSWORD_NO_MATCH, MYF(0));
      goto end;
    }
    else if (!can_create_user)
    {
      my_error(ER_CANT_CREATE_USER_WITH_GRANT, MYF(0));
      goto end;
    }
    old_row_exists = 0;
    restore_record(table,s->default_values);
    table->field[0]->store(combo.host.str,combo.host.length,
                           system_charset_info);
    table->field[1]->store(combo.user.str,combo.user.length,
                           system_charset_info);
    table->field[2]->store(password, password_len,
                           system_charset_info);
  }
  else
  {
    old_row_exists = 1;
    store_record(table,record[1]);			// Save copy for update
    if (combo.password.str)			// If password given
      table->field[2]->store(password, password_len, system_charset_info);
    else if (!rights && !revoke_grant &&
             lex->ssl_type == SSL_TYPE_NOT_SPECIFIED &&
             !lex->mqh.specified_limits)
    {
      DBUG_RETURN(0);
    }
  }

  /* Update table columns with new privileges */

  Field **tmp_field;
  ulong priv;
  uint next_field;
  for (tmp_field= table->field+3, priv = SELECT_ACL;
       *tmp_field && (*tmp_field)->real_type() == MYSQL_TYPE_ENUM &&
	 ((Field_enum*) (*tmp_field))->typelib->count == 2 ;
       tmp_field++, priv <<= 1)
  {
    if (priv & rights)				 // set requested privileges
      (*tmp_field)->store(&what, 1, &my_charset_latin1);
  }
  rights= get_access(table, 3, &next_field);
  DBUG_PRINT("info",("table fields: %d",table->s->fields));
  if (table->s->fields >= 31)		/* From 4.0.0 we have more fields */
  {
    /* We write down SSL related ACL stuff */
    switch (lex->ssl_type) {
    case SSL_TYPE_ANY:
      table->field[next_field]->store(STRING_WITH_LEN("ANY"),
                                      &my_charset_latin1);
      table->field[next_field+1]->store("", 0, &my_charset_latin1);
      table->field[next_field+2]->store("", 0, &my_charset_latin1);
      table->field[next_field+3]->store("", 0, &my_charset_latin1);
      break;
    case SSL_TYPE_X509:
      table->field[next_field]->store(STRING_WITH_LEN("X509"),
                                      &my_charset_latin1);
      table->field[next_field+1]->store("", 0, &my_charset_latin1);
      table->field[next_field+2]->store("", 0, &my_charset_latin1);
      table->field[next_field+3]->store("", 0, &my_charset_latin1);
      break;
    case SSL_TYPE_SPECIFIED:
      table->field[next_field]->store(STRING_WITH_LEN("SPECIFIED"),
                                      &my_charset_latin1);
      table->field[next_field+1]->store("", 0, &my_charset_latin1);
      table->field[next_field+2]->store("", 0, &my_charset_latin1);
      table->field[next_field+3]->store("", 0, &my_charset_latin1);
      if (lex->ssl_cipher)
        table->field[next_field+1]->store(lex->ssl_cipher,
                                strlen(lex->ssl_cipher), system_charset_info);
      if (lex->x509_issuer)
        table->field[next_field+2]->store(lex->x509_issuer,
                                strlen(lex->x509_issuer), system_charset_info);
      if (lex->x509_subject)
        table->field[next_field+3]->store(lex->x509_subject,
                                strlen(lex->x509_subject), system_charset_info);
      break;
    case SSL_TYPE_NOT_SPECIFIED:
      break;
    case SSL_TYPE_NONE:
      table->field[next_field]->store("", 0, &my_charset_latin1);
      table->field[next_field+1]->store("", 0, &my_charset_latin1);
      table->field[next_field+2]->store("", 0, &my_charset_latin1);
      table->field[next_field+3]->store("", 0, &my_charset_latin1);
      break;
    }
    next_field+=4;

    USER_RESOURCES mqh= lex->mqh;
    if (mqh.specified_limits & USER_RESOURCES::QUERIES_PER_HOUR)
      table->field[next_field]->store((longlong) mqh.questions, TRUE);
    if (mqh.specified_limits & USER_RESOURCES::UPDATES_PER_HOUR)
      table->field[next_field+1]->store((longlong) mqh.updates, TRUE);
    if (mqh.specified_limits & USER_RESOURCES::CONNECTIONS_PER_HOUR)
      table->field[next_field+2]->store((longlong) mqh.conn_per_hour, TRUE);
    if (table->s->fields >= 36 &&
        (mqh.specified_limits & USER_RESOURCES::USER_CONNECTIONS))
      table->field[next_field+3]->store((longlong) mqh.user_conn, TRUE);
    mqh_used= mqh_used || mqh.questions || mqh.updates || mqh.conn_per_hour;
  }
  if (old_row_exists)
  {
    /*
      We should NEVER delete from the user table, as a uses can still
      use mysqld even if he doesn't have any privileges in the user table!
    */
    if (cmp_record(table,record[1]))
    {
      if ((error=
           table->file->ha_update_row(table->record[1],table->record[0])) &&
          error != HA_ERR_RECORD_IS_THE_SAME)
      {						// This should never happen
        table->file->print_error(error,MYF(0));	/* purecov: deadcode */
        error= -1;				/* purecov: deadcode */
        goto end;				/* purecov: deadcode */
      }
      else
        error= 0;
    }
  }
  else if ((error=table->file->ha_write_row(table->record[0]))) // insert
  {						// This should never happen
    if (table->file->is_fatal_error(error, HA_CHECK_DUP))
    {
      table->file->print_error(error,MYF(0));	/* purecov: deadcode */
      error= -1;				/* purecov: deadcode */
      goto end;					/* purecov: deadcode */
    }
  }
  error=0;					// Privileges granted / revoked

end:
  if (!error)
  {
    acl_cache->clear(1);			// Clear privilege cache
    if (old_row_exists)
      acl_update_user(combo.user.str, combo.host.str,
                      combo.password.str, password_len,
		      lex->ssl_type,
		      lex->ssl_cipher,
		      lex->x509_issuer,
		      lex->x509_subject,
		      &lex->mqh,
		      rights);
    else
      acl_insert_user(combo.user.str, combo.host.str, password, password_len,
		      lex->ssl_type,
		      lex->ssl_cipher,
		      lex->x509_issuer,
		      lex->x509_subject,
		      &lex->mqh,
		      rights);
  }
  DBUG_RETURN(error);
}


/*
  change grants in the mysql.db table
*/

static int replace_db_table(TABLE *table, const char *db,
			    const LEX_USER &combo,
			    ulong rights, bool revoke_grant)
{
  uint i;
  ulong priv,store_rights;
  bool old_row_exists=0;
  int error;
  char what= (revoke_grant) ? 'N' : 'Y';
  uchar user_key[MAX_KEY_LENGTH];
  DBUG_ENTER("replace_db_table");

  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    DBUG_RETURN(-1);
  }

  /* Check if there is such a user in user table in memory? */
  if (!find_acl_user(combo.host.str,combo.user.str, FALSE))
  {
    my_message(ER_PASSWORD_NO_MATCH, ER(ER_PASSWORD_NO_MATCH), MYF(0));
    DBUG_RETURN(-1);
  }

  table->use_all_columns();
  table->field[0]->store(combo.host.str,combo.host.length,
                         system_charset_info);
  table->field[1]->store(db,(uint) strlen(db), system_charset_info);
  table->field[2]->store(combo.user.str,combo.user.length,
                         system_charset_info);
  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);

  if (table->file->index_read_idx_map(table->record[0],0, user_key,
                                      HA_WHOLE_KEY,
                                      HA_READ_KEY_EXACT))
  {
    if (what == 'N')
    { // no row, no revoke
      my_error(ER_NONEXISTING_GRANT, MYF(0), combo.user.str, combo.host.str);
      goto abort;
    }
    old_row_exists = 0;
    restore_record(table, s->default_values);
    table->field[0]->store(combo.host.str,combo.host.length,
                           system_charset_info);
    table->field[1]->store(db,(uint) strlen(db), system_charset_info);
    table->field[2]->store(combo.user.str,combo.user.length,
                           system_charset_info);
  }
  else
  {
    old_row_exists = 1;
    store_record(table,record[1]);
  }

  store_rights=get_rights_for_db(rights);
  for (i= 3, priv= 1; i < table->s->fields; i++, priv <<= 1)
  {
    if (priv & store_rights)			// do it if priv is chosen
      table->field [i]->store(&what,1, &my_charset_latin1);// set requested privileges
  }
  rights=get_access(table,3);
  rights=fix_rights_for_db(rights);

  if (old_row_exists)
  {
    /* update old existing row */
    if (rights)
    {
      if ((error= table->file->ha_update_row(table->record[1],
                                             table->record[0])) &&
          error != HA_ERR_RECORD_IS_THE_SAME)
	goto table_error;			/* purecov: deadcode */
    }
    else	/* must have been a revoke of all privileges */
    {
      if ((error= table->file->ha_delete_row(table->record[1])))
	goto table_error;			/* purecov: deadcode */
    }
  }
  else if (rights && (error= table->file->ha_write_row(table->record[0])))
  {
    if (table->file->is_fatal_error(error, HA_CHECK_DUP_KEY))
      goto table_error; /* purecov: deadcode */
  }

  acl_cache->clear(1);				// Clear privilege cache
  if (old_row_exists)
    acl_update_db(combo.user.str,combo.host.str,db,rights);
  else
  if (rights)
    acl_insert_db(combo.user.str,combo.host.str,db,rights);
  DBUG_RETURN(0);

  /* This could only happen if the grant tables got corrupted */
table_error:
  table->file->print_error(error,MYF(0));	/* purecov: deadcode */

abort:
  DBUG_RETURN(-1);
}


class GRANT_COLUMN :public Sql_alloc
{
public:
  char *column;
  ulong rights;
  uint key_length;
  GRANT_COLUMN(String &c,  ulong y) :rights (y)
  {
    column= (char*) memdup_root(&memex,c.ptr(), key_length=c.length());
  }
};


static uchar* get_key_column(GRANT_COLUMN *buff, size_t *length,
			    my_bool not_used __attribute__((unused)))
{
  *length=buff->key_length;
  return (uchar*) buff->column;
}


class GRANT_NAME :public Sql_alloc
{
public:
  acl_host_and_ip host;
  char *db, *user, *tname, *hash_key;
  ulong privs;
  ulong sort;
  size_t key_length;
  GRANT_NAME(const char *h, const char *d,const char *u,
             const char *t, ulong p, bool is_routine);
  GRANT_NAME (TABLE *form, bool is_routine);
  virtual ~GRANT_NAME() {};
  virtual bool ok() { return privs != 0; }
  void set_user_details(const char *h, const char *d,
                        const char *u, const char *t,
                        bool is_routine);
};


class GRANT_TABLE :public GRANT_NAME
{
public:
  ulong cols;
  HASH hash_columns;

  GRANT_TABLE(const char *h, const char *d,const char *u,
              const char *t, ulong p, ulong c);
  GRANT_TABLE (TABLE *form, TABLE *col_privs);
  ~GRANT_TABLE();
  bool ok() { return privs != 0 || cols != 0; }
};


void GRANT_NAME::set_user_details(const char *h, const char *d,
                                  const char *u, const char *t,
                                  bool is_routine)
{
  /* Host given by user */
  update_hostname(&host, strdup_root(&memex, h));
  if (db != d)
  {
    db= strdup_root(&memex, d);
    if (lower_case_table_names)
      my_casedn_str(files_charset_info, db);
  }
  user = strdup_root(&memex,u);
  sort=  get_sort(3,host.hostname,db,user);
  if (tname != t)
  {
    tname= strdup_root(&memex, t);
    if (lower_case_table_names || is_routine)
      my_casedn_str(files_charset_info, tname);
  }
  key_length= strlen(d) + strlen(u)+ strlen(t)+3;
  hash_key=   (char*) alloc_root(&memex,key_length);
  strmov(strmov(strmov(hash_key,user)+1,db)+1,tname);
}

GRANT_NAME::GRANT_NAME(const char *h, const char *d,const char *u,
                       const char *t, ulong p, bool is_routine)
  :db(0), tname(0), privs(p)
{
  set_user_details(h, d, u, t, is_routine);
}

GRANT_TABLE::GRANT_TABLE(const char *h, const char *d,const char *u,
                	 const char *t, ulong p, ulong c)
  :GRANT_NAME(h,d,u,t,p, FALSE), cols(c)
{
  (void) hash_init2(&hash_columns,4,system_charset_info,
                   0,0,0, (hash_get_key) get_key_column,0,0);
}


GRANT_NAME::GRANT_NAME(TABLE *form, bool is_routine)
{
  update_hostname(&host, get_field(&memex, form->field[0]));
  db=    get_field(&memex,form->field[1]);
  user=  get_field(&memex,form->field[2]);
  if (!user)
    user= (char*) "";
  sort=  get_sort(3, host.hostname, db, user);
  tname= get_field(&memex,form->field[3]);
  if (!db || !tname)
  {
    /* Wrong table row; Ignore it */
    privs= 0;
    return;					/* purecov: inspected */
  }
  if (lower_case_table_names)
  {
    my_casedn_str(files_charset_info, db);
  }
  if (lower_case_table_names || is_routine)
  {
    my_casedn_str(files_charset_info, tname);
  }
  key_length= (strlen(db) + strlen(user) + strlen(tname) + 3);
  hash_key=   (char*) alloc_root(&memex, key_length);
  strmov(strmov(strmov(hash_key,user)+1,db)+1,tname);
  privs = (ulong) form->field[6]->val_int();
  privs = fix_rights_for_table(privs);
}


GRANT_TABLE::GRANT_TABLE(TABLE *form, TABLE *col_privs)
  :GRANT_NAME(form, FALSE)
{
  uchar key[MAX_KEY_LENGTH];

  if (!db || !tname)
  {
    /* Wrong table row; Ignore it */
    hash_clear(&hash_columns);                  /* allow for destruction */
    cols= 0;
    return;
  }
  cols= (ulong) form->field[7]->val_int();
  cols =  fix_rights_for_column(cols);

  (void) hash_init2(&hash_columns,4,system_charset_info,
                   0,0,0, (hash_get_key) get_key_column,0,0);
  if (cols)
  {
    uint key_prefix_len;
    KEY_PART_INFO *key_part= col_privs->key_info->key_part;
    col_privs->field[0]->store(host.hostname,
                               host.hostname ? (uint) strlen(host.hostname) :
                               0,
                               system_charset_info);
    col_privs->field[1]->store(db,(uint) strlen(db), system_charset_info);
    col_privs->field[2]->store(user,(uint) strlen(user), system_charset_info);
    col_privs->field[3]->store(tname,(uint) strlen(tname), system_charset_info);

    key_prefix_len= (key_part[0].store_length +
                     key_part[1].store_length +
                     key_part[2].store_length +
                     key_part[3].store_length);
    key_copy(key, col_privs->record[0], col_privs->key_info, key_prefix_len);
    col_privs->field[4]->store("",0, &my_charset_latin1);

    col_privs->file->ha_index_init(0, 1);
    if (col_privs->file->index_read_map(col_privs->record[0], (uchar*) key,
                                        (key_part_map)15, HA_READ_KEY_EXACT))
    {
      cols = 0; /* purecov: deadcode */
      col_privs->file->ha_index_end();
      return;
    }
    do
    {
      String *res,column_name;
      GRANT_COLUMN *mem_check;
      /* As column name is a string, we don't have to supply a buffer */
      res=col_privs->field[4]->val_str(&column_name);
      ulong priv= (ulong) col_privs->field[6]->val_int();
      if (!(mem_check = new GRANT_COLUMN(*res,
                                         fix_rights_for_column(priv))))
      {
        /* Don't use this entry */
        privs = cols = 0;			/* purecov: deadcode */
        return;				/* purecov: deadcode */
      }
      if (my_hash_insert(&hash_columns, (uchar *) mem_check))
      {
        /* Invalidate this entry */
        privs= cols= 0;
        return;
      }
    } while (!col_privs->file->index_next(col_privs->record[0]) &&
             !key_cmp_if_same(col_privs,key,0,key_prefix_len));
    col_privs->file->ha_index_end();
  }
}


GRANT_TABLE::~GRANT_TABLE()
{
  hash_free(&hash_columns);
}


static uchar* get_grant_table(GRANT_NAME *buff, size_t *length,
			     my_bool not_used __attribute__((unused)))
{
  *length=buff->key_length;
  return (uchar*) buff->hash_key;
}


void free_grant_table(GRANT_TABLE *grant_table)
{
  hash_free(&grant_table->hash_columns);
}


/* Search after a matching grant. Prefer exact grants before not exact ones */

static GRANT_NAME *name_hash_search(HASH *name_hash,
                                    const char *host,const char* ip,
                                    const char *db,
                                    const char *user, const char *tname,
                                    bool exact, bool name_tolower)
{
  char helping [NAME_LEN*2+USERNAME_LENGTH+3], *name_ptr;
  uint len;
  GRANT_NAME *grant_name,*found=0;
  HASH_SEARCH_STATE state;

  name_ptr= strmov(strmov(helping, user) + 1, db) + 1;
  len  = (uint) (strmov(name_ptr, tname) - helping) + 1;
  if (name_tolower)
    my_casedn_str(files_charset_info, name_ptr);
  for (grant_name= (GRANT_NAME*) hash_first(name_hash, (uchar*) helping,
                                            len, &state);
       grant_name ;
       grant_name= (GRANT_NAME*) hash_next(name_hash,(uchar*) helping,
                                           len, &state))
  {
    if (exact)
    {
      if (!grant_name->host.hostname ||
          (host &&
	   !my_strcasecmp(system_charset_info, host,
                          grant_name->host.hostname)) ||
	  (ip && !strcmp(ip, grant_name->host.hostname)))
	return grant_name;
    }
    else
    {
      if (compare_hostname(&grant_name->host, host, ip) &&
          (!found || found->sort < grant_name->sort))
	found=grant_name;					// Host ok
    }
  }
  return found;
}


inline GRANT_NAME *
routine_hash_search(const char *host, const char *ip, const char *db,
                 const char *user, const char *tname, bool proc, bool exact)
{
  return (GRANT_TABLE*)
    name_hash_search(proc ? &proc_priv_hash : &func_priv_hash,
		     host, ip, db, user, tname, exact, TRUE);
}


inline GRANT_TABLE *
table_hash_search(const char *host, const char *ip, const char *db,
		  const char *user, const char *tname, bool exact)
{
  return (GRANT_TABLE*) name_hash_search(&column_priv_hash, host, ip, db,
					 user, tname, exact, FALSE);
}


inline GRANT_COLUMN *
column_hash_search(GRANT_TABLE *t, const char *cname, uint length)
{
  return (GRANT_COLUMN*) hash_search(&t->hash_columns, (uchar*) cname,length);
}


static int replace_column_table(GRANT_TABLE *g_t,
				TABLE *table, const LEX_USER &combo,
				List <LEX_COLUMN> &columns,
				const char *db, const char *table_name,
				ulong rights, bool revoke_grant)
{
  int error=0,result=0;
  uchar key[MAX_KEY_LENGTH];
  uint key_prefix_length;
  KEY_PART_INFO *key_part= table->key_info->key_part;
  DBUG_ENTER("replace_column_table");

  table->use_all_columns();
  table->field[0]->store(combo.host.str,combo.host.length,
                         system_charset_info);
  table->field[1]->store(db,(uint) strlen(db),
                         system_charset_info);
  table->field[2]->store(combo.user.str,combo.user.length,
                         system_charset_info);
  table->field[3]->store(table_name,(uint) strlen(table_name),
                         system_charset_info);

  /* Get length of 4 first key parts */
  key_prefix_length= (key_part[0].store_length + key_part[1].store_length +
                      key_part[2].store_length + key_part[3].store_length);
  key_copy(key, table->record[0], table->key_info, key_prefix_length);

  rights&= COL_ACLS;				// Only ACL for columns

  /* first fix privileges for all columns in column list */

  List_iterator <LEX_COLUMN> iter(columns);
  class LEX_COLUMN *column;
  table->file->ha_index_init(0, 1);
  while ((column= iter++))
  {
    ulong privileges= column->rights;
    bool old_row_exists=0;
    uchar user_key[MAX_KEY_LENGTH];

    key_restore(table->record[0],key,table->key_info,
                key_prefix_length);
    table->field[4]->store(column->column.ptr(), column->column.length(),
                           system_charset_info);
    /* Get key for the first 4 columns */
    key_copy(user_key, table->record[0], table->key_info,
             table->key_info->key_length);

    if (table->file->index_read_map(table->record[0], user_key, HA_WHOLE_KEY,
                                    HA_READ_KEY_EXACT))
    {
      if (revoke_grant)
      {
	my_error(ER_NONEXISTING_TABLE_GRANT, MYF(0),
                 combo.user.str, combo.host.str,
                 table_name);                   /* purecov: inspected */
	result= -1;                             /* purecov: inspected */
	continue;                               /* purecov: inspected */
      }
      old_row_exists = 0;
      restore_record(table, s->default_values);		// Get empty record
      key_restore(table->record[0],key,table->key_info,
                  key_prefix_length);
      table->field[4]->store(column->column.ptr(),column->column.length(),
                             system_charset_info);
    }
    else
    {
      ulong tmp= (ulong) table->field[6]->val_int();
      tmp=fix_rights_for_column(tmp);

      if (revoke_grant)
	privileges = tmp & ~(privileges | rights);
      else
	privileges |= tmp;
      old_row_exists = 1;
      store_record(table,record[1]);			// copy original row
    }

    table->field[6]->store((longlong) get_rights_for_column(privileges), TRUE);

    if (old_row_exists)
    {
      GRANT_COLUMN *grant_column;
      if (privileges)
	error=table->file->ha_update_row(table->record[1],table->record[0]);
      else
	error=table->file->ha_delete_row(table->record[1]);
      if (error && error != HA_ERR_RECORD_IS_THE_SAME)
      {
	table->file->print_error(error,MYF(0)); /* purecov: inspected */
	result= -1;				/* purecov: inspected */
	goto end;				/* purecov: inspected */
      }
      else
        error= 0;
      grant_column= column_hash_search(g_t, column->column.ptr(),
                                       column->column.length());
      if (grant_column)				// Should always be true
	grant_column->rights= privileges;	// Update hash
    }
    else					// new grant
    {
      GRANT_COLUMN *grant_column;
      if ((error=table->file->ha_write_row(table->record[0])))
      {
	table->file->print_error(error,MYF(0)); /* purecov: inspected */
	result= -1;				/* purecov: inspected */
	goto end;				/* purecov: inspected */
      }
      grant_column= new GRANT_COLUMN(column->column,privileges);
      if (my_hash_insert(&g_t->hash_columns,(uchar*) grant_column))
      {
        result= -1;
        goto end;
      }
    }
  }

  /*
    If revoke of privileges on the table level, remove all such privileges
    for all columns
  */

  if (revoke_grant)
  {
    uchar user_key[MAX_KEY_LENGTH];
    key_copy(user_key, table->record[0], table->key_info,
             key_prefix_length);

    if (table->file->index_read_map(table->record[0], user_key,
                                    (key_part_map)15,
                                    HA_READ_KEY_EXACT))
      goto end;

    /* Scan through all rows with the same host,db,user and table */
    do
    {
      ulong privileges = (ulong) table->field[6]->val_int();
      privileges=fix_rights_for_column(privileges);
      store_record(table,record[1]);

      if (privileges & rights)	// is in this record the priv to be revoked ??
      {
	GRANT_COLUMN *grant_column = NULL;
	char  colum_name_buf[HOSTNAME_LENGTH+1];
	String column_name(colum_name_buf,sizeof(colum_name_buf),
                           system_charset_info);

	privileges&= ~rights;
	table->field[6]->store((longlong)
			       get_rights_for_column(privileges), TRUE);
	table->field[4]->val_str(&column_name);
	grant_column = column_hash_search(g_t,
					  column_name.ptr(),
					  column_name.length());
	if (privileges)
	{
	  int tmp_error;
	  if ((tmp_error=table->file->ha_update_row(table->record[1],
						    table->record[0])) &&
              tmp_error != HA_ERR_RECORD_IS_THE_SAME)
	  {					/* purecov: deadcode */
	    table->file->print_error(tmp_error,MYF(0)); /* purecov: deadcode */
	    result= -1;				/* purecov: deadcode */
	    goto end;				/* purecov: deadcode */
	  }
	  if (grant_column)
	    grant_column->rights  = privileges; // Update hash
	}
	else
	{
	  int tmp_error;
	  if ((tmp_error = table->file->ha_delete_row(table->record[1])))
	  {					/* purecov: deadcode */
	    table->file->print_error(tmp_error,MYF(0)); /* purecov: deadcode */
	    result= -1;				/* purecov: deadcode */
	    goto end;				/* purecov: deadcode */
	  }
	  if (grant_column)
	    hash_delete(&g_t->hash_columns,(uchar*) grant_column);
	}
      }
    } while (!table->file->index_next(table->record[0]) &&
	     !key_cmp_if_same(table, key, 0, key_prefix_length));
  }

end:
  table->file->ha_index_end();
  DBUG_RETURN(result);
}

static inline void get_grantor(THD *thd, char *grantor)
{
  const char *user= thd->security_ctx->user;
  const char *host= thd->security_ctx->host_or_ip;

#if defined(HAVE_REPLICATION)
  if (thd->slave_thread && thd->has_invoker())
  {
    user= thd->get_invoker_user().str;
    host= thd->get_invoker_host().str;
  }
#endif
  strxmov(grantor, user, "@", host, NullS);
}

static int replace_table_table(THD *thd, GRANT_TABLE *grant_table,
			       TABLE *table, const LEX_USER &combo,
			       const char *db, const char *table_name,
			       ulong rights, ulong col_rights,
			       bool revoke_grant)
{
  char grantor[USER_HOST_BUFF_SIZE];
  int old_row_exists = 1;
  int error=0;
  ulong store_table_rights, store_col_rights;
  uchar user_key[MAX_KEY_LENGTH];
  DBUG_ENTER("replace_table_table");

  get_grantor(thd, grantor);
  /*
    The following should always succeed as new users are created before
    this function is called!
  */
  if (!find_acl_user(combo.host.str,combo.user.str, FALSE))
  {
    my_message(ER_PASSWORD_NO_MATCH, ER(ER_PASSWORD_NO_MATCH),
               MYF(0));	/* purecov: deadcode */
    DBUG_RETURN(-1);				/* purecov: deadcode */
  }

  table->use_all_columns();
  restore_record(table, s->default_values);     // Get empty record
  table->field[0]->store(combo.host.str,combo.host.length,
                         system_charset_info);
  table->field[1]->store(db,(uint) strlen(db), system_charset_info);
  table->field[2]->store(combo.user.str,combo.user.length,
                         system_charset_info);
  table->field[3]->store(table_name,(uint) strlen(table_name),
                         system_charset_info);
  store_record(table,record[1]);			// store at pos 1
  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);

  if (table->file->index_read_idx_map(table->record[0], 0, user_key,
                                      HA_WHOLE_KEY,
                                      HA_READ_KEY_EXACT))
  {
    /*
      The following should never happen as we first check the in memory
      grant tables for the user.  There is however always a small change that
      the user has modified the grant tables directly.
    */
    if (revoke_grant)
    { // no row, no revoke
      my_error(ER_NONEXISTING_TABLE_GRANT, MYF(0),
               combo.user.str, combo.host.str,
               table_name);		        /* purecov: deadcode */
      DBUG_RETURN(-1);				/* purecov: deadcode */
    }
    old_row_exists = 0;
    restore_record(table,record[1]);			// Get saved record
  }

  store_table_rights= get_rights_for_table(rights);
  store_col_rights=   get_rights_for_column(col_rights);
  if (old_row_exists)
  {
    ulong j,k;
    store_record(table,record[1]);
    j = (ulong) table->field[6]->val_int();
    k = (ulong) table->field[7]->val_int();

    if (revoke_grant)
    {
      /* column rights are already fixed in mysql_table_grant */
      store_table_rights=j & ~store_table_rights;
    }
    else
    {
      store_table_rights|= j;
      store_col_rights|=   k;
    }
  }

  table->field[4]->store(grantor,(uint) strlen(grantor), system_charset_info);
  table->field[6]->store((longlong) store_table_rights, TRUE);
  table->field[7]->store((longlong) store_col_rights, TRUE);
  rights=fix_rights_for_table(store_table_rights);
  col_rights=fix_rights_for_column(store_col_rights);

  if (old_row_exists)
  {
    if (store_table_rights || store_col_rights)
    {
      if ((error=table->file->ha_update_row(table->record[1],
                                            table->record[0])) &&
          error != HA_ERR_RECORD_IS_THE_SAME)
	goto table_error;			/* purecov: deadcode */
    }
    else if ((error = table->file->ha_delete_row(table->record[1])))
      goto table_error;				/* purecov: deadcode */
  }
  else
  {
    error=table->file->ha_write_row(table->record[0]);
    if (table->file->is_fatal_error(error, HA_CHECK_DUP_KEY))
      goto table_error;				/* purecov: deadcode */
  }

  if (rights | col_rights)
  {
    grant_table->privs= rights;
    grant_table->cols=	col_rights;
  }
  else
  {
    hash_delete(&column_priv_hash,(uchar*) grant_table);
  }
  DBUG_RETURN(0);

  /* This should never happen */
table_error:
  table->file->print_error(error,MYF(0)); /* purecov: deadcode */
  DBUG_RETURN(-1); /* purecov: deadcode */
}


/**
  @retval       0  success
  @retval      -1  error
*/
static int replace_routine_table(THD *thd, GRANT_NAME *grant_name,
			      TABLE *table, const LEX_USER &combo,
			      const char *db, const char *routine_name,
			      bool is_proc, ulong rights, bool revoke_grant)
{
  char grantor[USER_HOST_BUFF_SIZE];
  int old_row_exists= 1;
  int error=0;
  ulong store_proc_rights;
  DBUG_ENTER("replace_routine_table");

  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    DBUG_RETURN(-1);
  }

  get_grantor(thd, grantor);
  /*
    New users are created before this function is called.

    There may be some cases where a routine's definer is removed but the
    routine remains.
  */

  table->use_all_columns();
  restore_record(table, s->default_values);		// Get empty record
  table->field[0]->store(combo.host.str,combo.host.length, &my_charset_latin1);
  table->field[1]->store(db,(uint) strlen(db), &my_charset_latin1);
  table->field[2]->store(combo.user.str,combo.user.length, &my_charset_latin1);
  table->field[3]->store(routine_name,(uint) strlen(routine_name),
                         &my_charset_latin1);
  table->field[4]->store((longlong)(is_proc ?
                                    TYPE_ENUM_PROCEDURE : TYPE_ENUM_FUNCTION),
                         TRUE);
  store_record(table,record[1]);			// store at pos 1

  if (table->file->index_read_idx_map(table->record[0], 0,
                                      (uchar*) table->field[0]->ptr,
                                      HA_WHOLE_KEY,
                                      HA_READ_KEY_EXACT))
  {
    /*
      The following should never happen as we first check the in memory
      grant tables for the user.  There is however always a small change that
      the user has modified the grant tables directly.
    */
    if (revoke_grant)
    { // no row, no revoke
      my_error(ER_NONEXISTING_PROC_GRANT, MYF(0),
               combo.user.str, combo.host.str, routine_name);
      DBUG_RETURN(-1);
    }
    old_row_exists= 0;
    restore_record(table,record[1]);			// Get saved record
  }

  store_proc_rights= get_rights_for_procedure(rights);
  if (old_row_exists)
  {
    ulong j;
    store_record(table,record[1]);
    j= (ulong) table->field[6]->val_int();

    if (revoke_grant)
    {
      /* column rights are already fixed in mysql_table_grant */
      store_proc_rights=j & ~store_proc_rights;
    }
    else
    {
      store_proc_rights|= j;
    }
  }

  table->field[5]->store(grantor,(uint) strlen(grantor), &my_charset_latin1);
  table->field[6]->store((longlong) store_proc_rights, TRUE);
  rights=fix_rights_for_procedure(store_proc_rights);

  if (old_row_exists)
  {
    if (store_proc_rights)
    {
      if ((error=table->file->ha_update_row(table->record[1],
                                            table->record[0])) &&
          error != HA_ERR_RECORD_IS_THE_SAME)
	goto table_error;
    }
    else if ((error= table->file->ha_delete_row(table->record[1])))
      goto table_error;
  }
  else
  {
    error=table->file->ha_write_row(table->record[0]);
    if (table->file->is_fatal_error(error, HA_CHECK_DUP_KEY))
      goto table_error;
  }

  if (rights)
  {
    grant_name->privs= rights;
  }
  else
  {
    hash_delete(is_proc ? &proc_priv_hash : &func_priv_hash,(uchar*) grant_name);
  }
  DBUG_RETURN(0);

  /* This should never happen */
table_error:
  table->file->print_error(error,MYF(0));
  DBUG_RETURN(-1);
}


/*
  Store table level and column level grants in the privilege tables

  SYNOPSIS
    mysql_table_grant()
    thd			Thread handle
    table_list		List of tables to give grant
    user_list		List of users to give grant
    columns		List of columns to give grant
    rights		Table level grant
    revoke_grant	Set to 1 if this is a REVOKE command

  RETURN
    FALSE ok
    TRUE  error
*/

int mysql_table_grant(THD *thd, TABLE_LIST *table_list,
		      List <LEX_USER> &user_list,
		      List <LEX_COLUMN> &columns, ulong rights,
		      bool revoke_grant)
{
  ulong column_priv= 0;
  List_iterator <LEX_USER> str_list (user_list);
  LEX_USER *Str, *tmp_Str;
  TABLE_LIST tables[3];
  bool create_new_users=0;
  char *db_name, *table_name;
  bool save_binlog_row_based;
  DBUG_ENTER("mysql_table_grant");

  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0),
             "--skip-grant-tables");	/* purecov: inspected */
    DBUG_RETURN(TRUE);				/* purecov: inspected */
  }
  if (rights & ~TABLE_ACLS)
  {
    my_message(ER_ILLEGAL_GRANT_FOR_TABLE, ER(ER_ILLEGAL_GRANT_FOR_TABLE),
               MYF(0));
    DBUG_RETURN(TRUE);
  }

  if (!revoke_grant)
  {
    if (columns.elements)
    {
      class LEX_COLUMN *column;
      List_iterator <LEX_COLUMN> column_iter(columns);

      if (open_and_lock_tables(thd, table_list))
        DBUG_RETURN(TRUE);

      while ((column = column_iter++))
      {
        uint unused_field_idx= NO_CACHED_FIELD_INDEX;
        TABLE_LIST *dummy;
        Field *f=find_field_in_table_ref(thd, table_list, column->column.ptr(),
                                         column->column.length(),
                                         column->column.ptr(), NULL, NULL,
                                         NULL, TRUE, FALSE,
                                         &unused_field_idx, FALSE, &dummy);
        if (f == (Field*)0)
        {
          my_error(ER_BAD_FIELD_ERROR, MYF(0),
                   column->column.c_ptr(), table_list->alias);
          DBUG_RETURN(TRUE);
        }
        if (f == (Field *)-1)
          DBUG_RETURN(TRUE);
        column_priv|= column->rights;
      }
      close_thread_tables(thd);
    }
    else
    {
      if (!(rights & CREATE_ACL))
      {
        char buf[FN_REFLEN + 1];
        build_table_filename(buf, sizeof(buf) - 1, table_list->db,
                             table_list->table_name, reg_ext, 0);
        fn_format(buf, buf, "", "", MY_UNPACK_FILENAME  | MY_RESOLVE_SYMLINKS |
                                    MY_RETURN_REAL_PATH | MY_APPEND_EXT);
        if (access(buf,F_OK))
        {
          my_error(ER_NO_SUCH_TABLE, MYF(0), table_list->db, table_list->alias);
          DBUG_RETURN(TRUE);
        }
      }
      if (table_list->grant.want_privilege)
      {
        char command[128];
        get_privilege_desc(command, sizeof(command),
                           table_list->grant.want_privilege);
        my_error(ER_TABLEACCESS_DENIED_ERROR, MYF(0),
                 command, thd->security_ctx->priv_user,
                 thd->security_ctx->host_or_ip, table_list->alias);
        DBUG_RETURN(-1);
      }
    }
  }

  /* open the mysql.tables_priv and mysql.columns_priv tables */

  bzero((char*) &tables,sizeof(tables));
  tables[0].alias=tables[0].table_name= (char*) "user";
  tables[1].alias=tables[1].table_name= (char*) "tables_priv";
  tables[2].alias=tables[2].table_name= (char*) "columns_priv";
  tables[0].next_local= tables[0].next_global= tables+1;
  /* Don't open column table if we don't need it ! */
  tables[1].next_local=
    tables[1].next_global= ((column_priv ||
			     (revoke_grant &&
			      ((rights & COL_ACLS) || columns.elements)))
			    ? tables+2 : 0);
  tables[0].lock_type=tables[1].lock_type=tables[2].lock_type=TL_WRITE;
  tables[0].db=tables[1].db=tables[2].db=(char*) "mysql";

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  save_binlog_row_based= thd->current_stmt_binlog_row_based;
  thd->clear_current_stmt_binlog_row_based();

#ifdef HAVE_REPLICATION
  /*
    GRANT and REVOKE are applied the slave in/exclusion rules as they are
    some kind of updates to the mysql.% tables.
  */
  if (thd->slave_thread && rpl_filter->is_on())
  {
    /*
      The tables must be marked "updating" so that tables_ok() takes them into
      account in tests.
    */
    tables[0].updating= tables[1].updating= tables[2].updating= 1;
    if (!(thd->spcont || rpl_filter->tables_ok(0, tables)))
    {
      /* Restore the state of binlog format */
      thd->current_stmt_binlog_row_based= save_binlog_row_based;
      DBUG_RETURN(FALSE);
    }
  }
#endif

  /* 
    The lock api is depending on the thd->lex variable which needs to be
    re-initialized.
  */
  Query_tables_list backup;
  thd->lex->reset_n_backup_query_tables_list(&backup);
  if (simple_open_n_lock_tables(thd,tables))
  {						// Should never happen
    close_thread_tables(thd);			/* purecov: deadcode */
    /* Restore the state of binlog format */
    thd->current_stmt_binlog_row_based= save_binlog_row_based;
    DBUG_RETURN(TRUE);				/* purecov: deadcode */
  }

  if (!revoke_grant)
    create_new_users= test_if_create_new_users(thd);
  bool result= FALSE;
  rw_wrlock(&LOCK_grant);
  pthread_mutex_lock(&acl_cache->lock);
  MEM_ROOT *old_root= thd->mem_root;
  thd->mem_root= &memex;
  grant_version++;

  while ((tmp_Str = str_list++))
  {
    int error;
    GRANT_TABLE *grant_table;
    if (!(Str= get_current_user(thd, tmp_Str)))
    {
      result= TRUE;
      continue;
    }  
    /* Create user if needed */
    error=replace_user_table(thd, tables[0].table, *Str,
			     0, revoke_grant, create_new_users,
                             test(thd->variables.sql_mode &
                                  MODE_NO_AUTO_CREATE_USER));
    if (error)
    {
      result= TRUE;				// Remember error
      continue;					// Add next user
    }

    db_name= table_list->get_db_name();
    table_name= table_list->get_table_name();

    /* Find/create cached table grant */
    grant_table= table_hash_search(Str->host.str, NullS, db_name,
				   Str->user.str, table_name, 1);
    if (!grant_table)
    {
      if (revoke_grant)
      {
	my_error(ER_NONEXISTING_TABLE_GRANT, MYF(0),
                 Str->user.str, Str->host.str, table_list->table_name);
	result= TRUE;
	continue;
      }
      grant_table = new GRANT_TABLE (Str->host.str, db_name,
				     Str->user.str, table_name,
				     rights,
				     column_priv);
      if (!grant_table ||
        my_hash_insert(&column_priv_hash,(uchar*) grant_table))
      {
	result= TRUE;				/* purecov: deadcode */
	continue;				/* purecov: deadcode */
      }
    }

    /* If revoke_grant, calculate the new column privilege for tables_priv */
    if (revoke_grant)
    {
      class LEX_COLUMN *column;
      List_iterator <LEX_COLUMN> column_iter(columns);
      GRANT_COLUMN *grant_column;

      /* Fix old grants */
      while ((column = column_iter++))
      {
	grant_column = column_hash_search(grant_table,
					  column->column.ptr(),
					  column->column.length());
	if (grant_column)
	  grant_column->rights&= ~(column->rights | rights);
      }
      /* scan trough all columns to get new column grant */
      column_priv= 0;
      for (uint idx=0 ; idx < grant_table->hash_columns.records ; idx++)
      {
	grant_column= (GRANT_COLUMN*) hash_element(&grant_table->hash_columns,
						   idx);
	grant_column->rights&= ~rights;		// Fix other columns
	column_priv|= grant_column->rights;
      }
    }
    else
    {
      column_priv|= grant_table->cols;
    }


    /* update table and columns */

    if (replace_table_table(thd, grant_table, tables[1].table, *Str,
			    db_name, table_name,
			    rights, column_priv, revoke_grant))
    {
      /* Should only happen if table is crashed */
      result= TRUE;			       /* purecov: deadcode */
    }
    else if (tables[2].table)
    {
      if ((replace_column_table(grant_table, tables[2].table, *Str,
				columns,
				db_name, table_name,
				rights, revoke_grant)))
      {
	result= TRUE;
      }
    }
  }
  thd->mem_root= old_root;
  pthread_mutex_unlock(&acl_cache->lock);

  if (!result) /* success */
  {
    result= write_bin_log(thd, TRUE, thd->query(), thd->query_length());
  }

  rw_unlock(&LOCK_grant);

  if (!result) /* success */
    my_ok(thd);

  /* Tables are automatically closed */
  thd->lex->restore_backup_query_tables_list(&backup);
  /* Restore the state of binlog format */
  thd->current_stmt_binlog_row_based= save_binlog_row_based;
  DBUG_RETURN(result);
}


/**
  Store routine level grants in the privilege tables

  @param thd Thread handle
  @param table_list List of routines to give grant
  @param is_proc Is this a list of procedures?
  @param user_list List of users to give grant
  @param rights Table level grant
  @param revoke_grant Is this is a REVOKE command?

  @return
    @retval FALSE Success.
    @retval TRUE An error occurred.
*/

bool mysql_routine_grant(THD *thd, TABLE_LIST *table_list, bool is_proc,
			 List <LEX_USER> &user_list, ulong rights,
			 bool revoke_grant, bool write_to_binlog)
{
  List_iterator <LEX_USER> str_list (user_list);
  LEX_USER *Str, *tmp_Str;
  TABLE_LIST tables[2];
  bool create_new_users=0, result=0;
  char *db_name, *table_name;
  bool save_binlog_row_based;
  DBUG_ENTER("mysql_routine_grant");

  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0),
             "--skip-grant-tables");
    DBUG_RETURN(TRUE);
  }
  if (rights & ~PROC_ACLS)
  {
    my_message(ER_ILLEGAL_GRANT_FOR_TABLE, ER(ER_ILLEGAL_GRANT_FOR_TABLE),
               MYF(0));
    DBUG_RETURN(TRUE);
  }

  if (!revoke_grant)
  {
    if (sp_exist_routines(thd, table_list, is_proc))
      DBUG_RETURN(TRUE);
  }

  /* open the mysql.user and mysql.procs_priv tables */

  bzero((char*) &tables,sizeof(tables));
  tables[0].alias=tables[0].table_name= (char*) "user";
  tables[1].alias=tables[1].table_name= (char*) "procs_priv";
  tables[0].next_local= tables[0].next_global= tables+1;
  tables[0].lock_type=tables[1].lock_type=TL_WRITE;
  tables[0].db=tables[1].db=(char*) "mysql";

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  save_binlog_row_based= thd->current_stmt_binlog_row_based;
  thd->clear_current_stmt_binlog_row_based();

#ifdef HAVE_REPLICATION
  /*
    GRANT and REVOKE are applied the slave in/exclusion rules as they are
    some kind of updates to the mysql.% tables.
  */
  if (thd->slave_thread && rpl_filter->is_on())
  {
    /*
      The tables must be marked "updating" so that tables_ok() takes them into
      account in tests.
    */
    tables[0].updating= tables[1].updating= 1;
    if (!(thd->spcont || rpl_filter->tables_ok(0, tables)))
    {
      /* Restore the state of binlog format */
      thd->current_stmt_binlog_row_based= save_binlog_row_based;
      DBUG_RETURN(FALSE);
    }
  }
#endif

  if (simple_open_n_lock_tables(thd,tables))
  {						// Should never happen
    close_thread_tables(thd);
    /* Restore the state of binlog format */
    thd->current_stmt_binlog_row_based= save_binlog_row_based;
    DBUG_RETURN(TRUE);
  }

  if (!revoke_grant)
    create_new_users= test_if_create_new_users(thd);
  rw_wrlock(&LOCK_grant);
  pthread_mutex_lock(&acl_cache->lock);
  MEM_ROOT *old_root= thd->mem_root;
  thd->mem_root= &memex;

  DBUG_PRINT("info",("now time to iterate and add users"));

  while ((tmp_Str= str_list++))
  {
    int error;
    GRANT_NAME *grant_name;
    if (!(Str= get_current_user(thd, tmp_Str)))
    {
      result= TRUE;
      continue;
    }  
    /* Create user if needed */
    error=replace_user_table(thd, tables[0].table, *Str,
			     0, revoke_grant, create_new_users,
                             test(thd->variables.sql_mode &
                                  MODE_NO_AUTO_CREATE_USER));
    if (error)
    {
      result= TRUE;				// Remember error
      continue;					// Add next user
    }

    db_name= table_list->db;
    table_name= table_list->table_name;

    grant_name= routine_hash_search(Str->host.str, NullS, db_name,
                                    Str->user.str, table_name, is_proc, 1);
    if (!grant_name)
    {
      if (revoke_grant)
      {
        my_error(ER_NONEXISTING_PROC_GRANT, MYF(0),
	         Str->user.str, Str->host.str, table_name);
	result= TRUE;
	continue;
      }
      grant_name= new GRANT_NAME(Str->host.str, db_name,
				 Str->user.str, table_name,
				 rights, TRUE);
      if (!grant_name ||
        my_hash_insert(is_proc ?
                       &proc_priv_hash : &func_priv_hash,(uchar*) grant_name))
      {
        result= TRUE;
	continue;
      }
    }

    if (replace_routine_table(thd, grant_name, tables[1].table, *Str,
                              db_name, table_name, is_proc, rights, 
                              revoke_grant) != 0)
    {
      result= TRUE;
      continue;
    }
  }
  thd->mem_root= old_root;
  pthread_mutex_unlock(&acl_cache->lock);

  if (write_to_binlog)
  {
    if (write_bin_log(thd, FALSE, thd->query(), thd->query_length()))
      result= TRUE;
  }

  rw_unlock(&LOCK_grant);
  /* Restore the state of binlog format */
  thd->current_stmt_binlog_row_based= save_binlog_row_based;

  /* Tables are automatically closed */
  DBUG_RETURN(result);
}


bool mysql_grant(THD *thd, const char *db, List <LEX_USER> &list,
                 ulong rights, bool revoke_grant)
{
  List_iterator <LEX_USER> str_list (list);
  LEX_USER *Str, *tmp_Str;
  char tmp_db[NAME_LEN+1];
  bool create_new_users=0;
  TABLE_LIST tables[2];
  bool save_binlog_row_based;
  DBUG_ENTER("mysql_grant");
  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0),
             "--skip-grant-tables");	/* purecov: tested */
    DBUG_RETURN(TRUE);				/* purecov: tested */
  }

  if (lower_case_table_names && db)
  {
    strmov(tmp_db,db);
    my_casedn_str(files_charset_info, tmp_db);
    db=tmp_db;
  }

  /* open the mysql.user and mysql.db tables */
  bzero((char*) &tables,sizeof(tables));
  tables[0].alias=tables[0].table_name=(char*) "user";
  tables[1].alias=tables[1].table_name=(char*) "db";
  tables[0].next_local= tables[0].next_global= tables+1;
  tables[0].lock_type=tables[1].lock_type=TL_WRITE;
  tables[0].db=tables[1].db=(char*) "mysql";

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  save_binlog_row_based= thd->current_stmt_binlog_row_based;
  thd->clear_current_stmt_binlog_row_based();

#ifdef HAVE_REPLICATION
  /*
    GRANT and REVOKE are applied the slave in/exclusion rules as they are
    some kind of updates to the mysql.% tables.
  */
  if (thd->slave_thread && rpl_filter->is_on())
  {
    /*
      The tables must be marked "updating" so that tables_ok() takes them into
      account in tests.
    */
    tables[0].updating= tables[1].updating= 1;
    if (!(thd->spcont || rpl_filter->tables_ok(0, tables)))
    {
      /* Restore the state of binlog format */
      thd->current_stmt_binlog_row_based= save_binlog_row_based;
      DBUG_RETURN(FALSE);
    }
  }
#endif

  if (simple_open_n_lock_tables(thd,tables))
  {						// This should never happen
    close_thread_tables(thd);			/* purecov: deadcode */
    /* Restore the state of binlog format */
    thd->current_stmt_binlog_row_based= save_binlog_row_based;
    DBUG_RETURN(TRUE);				/* purecov: deadcode */
  }

  if (!revoke_grant)
    create_new_users= test_if_create_new_users(thd);

  /* go through users in user_list */
  rw_wrlock(&LOCK_grant);
  VOID(pthread_mutex_lock(&acl_cache->lock));
  grant_version++;

  int result=0;
  while ((tmp_Str = str_list++))
  {
    if (!(Str= get_current_user(thd, tmp_Str)))
    {
      result= TRUE;
      continue;
    }
    /*
      No User, but a password?
      They did GRANT ... TO CURRENT_USER() IDENTIFIED BY ... !
      Get the current user, and shallow-copy the new password to them!
    */
    if (!tmp_Str->user.str && tmp_Str->password.str)
      Str->password= tmp_Str->password;
    if (replace_user_table(thd, tables[0].table, *Str,
                           (!db ? rights : 0), revoke_grant, create_new_users,
                           test(thd->variables.sql_mode &
                                MODE_NO_AUTO_CREATE_USER)))
      result= -1;
    else if (db)
    {
      ulong db_rights= rights & DB_ACLS;
      if (db_rights  == rights)
      {
	if (replace_db_table(tables[1].table, db, *Str, db_rights,
			     revoke_grant))
	  result= -1;
      }
      else
      {
	my_error(ER_WRONG_USAGE, MYF(0), "DB GRANT", "GLOBAL PRIVILEGES");
	result= -1;
      }
    }
  }
  VOID(pthread_mutex_unlock(&acl_cache->lock));

  if (!result)
  {
    result= write_bin_log(thd, TRUE, thd->query(), thd->query_length());
  }

  rw_unlock(&LOCK_grant);
  close_thread_tables(thd);

  if (!result)
    my_ok(thd);
  /* Restore the state of binlog format */
  thd->current_stmt_binlog_row_based= save_binlog_row_based;

  DBUG_RETURN(result);
}


/* Free grant array if possible */

void  grant_free(void)
{
  DBUG_ENTER("grant_free");
  hash_free(&column_priv_hash);
  hash_free(&proc_priv_hash);
  hash_free(&func_priv_hash);
  free_root(&memex,MYF(0));
  DBUG_VOID_RETURN;
}


/**
  @brief Initialize structures responsible for table/column-level privilege
   checking and load information for them from tables in the 'mysql' database.

  @return Error status
    @retval 0 OK
    @retval 1 Could not initialize grant subsystem.
*/

my_bool grant_init()
{
  THD  *thd;
  my_bool return_val;
  DBUG_ENTER("grant_init");

  if (!(thd= new THD))
    DBUG_RETURN(1);				/* purecov: deadcode */
  thd->thread_stack= (char*) &thd;
  thd->store_globals();
  lex_start(thd);
  return_val=  grant_reload(thd);
  delete thd;
  /* Remember that we don't have a THD */
  my_pthread_setspecific_ptr(THR_THD,  0);
  DBUG_RETURN(return_val);
}


/**
  @brief Helper function to grant_reload_procs_priv

  Reads the procs_priv table into memory hash.

  @param table A pointer to the procs_priv table structure.

  @see grant_reload
  @see grant_reload_procs_priv

  @return Error state
    @retval TRUE An error occurred
    @retval FALSE Success
*/

static my_bool grant_load_procs_priv(TABLE *p_table)
{
  MEM_ROOT *memex_ptr;
  my_bool return_val= 1;
  bool check_no_resolve= specialflag & SPECIAL_NO_RESOLVE;
  MEM_ROOT **save_mem_root_ptr= my_pthread_getspecific_ptr(MEM_ROOT**,
                                                           THR_MALLOC);
  DBUG_ENTER("grant_load_procs_priv");
  (void) hash_init(&proc_priv_hash, &my_charset_utf8_bin,
                   0,0,0, (hash_get_key) get_grant_table,
                   0,0);
  (void) hash_init(&func_priv_hash, &my_charset_utf8_bin,
                   0,0,0, (hash_get_key) get_grant_table,
                   0,0);
  p_table->file->ha_index_init(0, 1);
  p_table->use_all_columns();

  if (!p_table->file->index_first(p_table->record[0]))
  {
    memex_ptr= &memex;
    my_pthread_setspecific_ptr(THR_MALLOC, &memex_ptr);
    do
    {
      GRANT_NAME *mem_check;
      HASH *hash;
      if (!(mem_check=new (memex_ptr) GRANT_NAME(p_table, TRUE)))
      {
        /* This could only happen if we are out memory */
        goto end_unlock;
      }

      if (check_no_resolve)
      {
	if (hostname_requires_resolving(mem_check->host.hostname))
	{
          sql_print_warning("'procs_priv' entry '%s %s@%s' "
                            "ignored in --skip-name-resolve mode.",
                            mem_check->tname, mem_check->user,
                            mem_check->host.hostname ?
                            mem_check->host.hostname : "");
          continue;
        }
      }
      if (p_table->field[4]->val_int() == TYPE_ENUM_PROCEDURE)
      {
        hash= &proc_priv_hash;
      }
      else
      if (p_table->field[4]->val_int() == TYPE_ENUM_FUNCTION)
      {
        hash= &func_priv_hash;
      }
      else
      {
        sql_print_warning("'procs_priv' entry '%s' "
                          "ignored, bad routine type",
                          mem_check->tname);
        continue;
      }

      mem_check->privs= fix_rights_for_procedure(mem_check->privs);
      if (! mem_check->ok())
        delete mem_check;
      else if (my_hash_insert(hash, (uchar*) mem_check))
      {
        delete mem_check;
        goto end_unlock;
      }
    }
    while (!p_table->file->index_next(p_table->record[0]));
  }
  /* Return ok */
  return_val= 0;

end_unlock:
  p_table->file->ha_index_end();
  my_pthread_setspecific_ptr(THR_MALLOC, save_mem_root_ptr);
  DBUG_RETURN(return_val);
}


/**
  @brief Initialize structures responsible for table/column-level privilege
    checking and load information about grants from open privilege tables.

  @param thd Current thread
  @param tables List containing open "mysql.tables_priv" and
    "mysql.columns_priv" tables.

  @see grant_reload

  @return Error state
    @retval FALSE Success
    @retval TRUE Error
*/

static my_bool grant_load(THD *thd, TABLE_LIST *tables)
{
  MEM_ROOT *memex_ptr;
  my_bool return_val= 1;
  TABLE *t_table= 0, *c_table= 0;
  bool check_no_resolve= specialflag & SPECIAL_NO_RESOLVE;
  MEM_ROOT **save_mem_root_ptr= my_pthread_getspecific_ptr(MEM_ROOT**,
                                                           THR_MALLOC);
  ulong old_sql_mode= thd->variables.sql_mode;
  DBUG_ENTER("grant_load");

  thd->variables.sql_mode&= ~MODE_PAD_CHAR_TO_FULL_LENGTH;

  (void) hash_init(&column_priv_hash, &my_charset_utf8_bin,
                   0,0,0, (hash_get_key) get_grant_table,
                   (hash_free_key) free_grant_table,0);

  t_table = tables[0].table;
  c_table = tables[1].table;
  t_table->file->ha_index_init(0, 1);
  t_table->use_all_columns();
  c_table->use_all_columns();

  if (!t_table->file->index_first(t_table->record[0]))
  {
    memex_ptr= &memex;
    my_pthread_setspecific_ptr(THR_MALLOC, &memex_ptr);
    do
    {
      GRANT_TABLE *mem_check;
      if (!(mem_check=new (memex_ptr) GRANT_TABLE(t_table,c_table)))
      {
	/* This could only happen if we are out memory */
	goto end_unlock;
      }

      if (check_no_resolve)
      {
	if (hostname_requires_resolving(mem_check->host.hostname))
	{
          sql_print_warning("'tables_priv' entry '%s %s@%s' "
                            "ignored in --skip-name-resolve mode.",
                            mem_check->tname,
                            mem_check->user ? mem_check->user : "",
                            mem_check->host.hostname ?
                            mem_check->host.hostname : "");
	  continue;
	}
      }

      if (! mem_check->ok())
	delete mem_check;
      else if (my_hash_insert(&column_priv_hash,(uchar*) mem_check))
      {
	delete mem_check;
	goto end_unlock;
      }
    }
    while (!t_table->file->index_next(t_table->record[0]));
  }

  return_val=0;					// Return ok

end_unlock:
  thd->variables.sql_mode= old_sql_mode;
  t_table->file->ha_index_end();
  my_pthread_setspecific_ptr(THR_MALLOC, save_mem_root_ptr);
  DBUG_RETURN(return_val);
}


/**
  @brief Helper function to grant_reload. Reloads procs_priv table is it
    exists.

  @param thd A pointer to the thread handler object.

  @see grant_reload

  @return Error state
    @retval FALSE Success
    @retval TRUE An error has occurred.
*/

static my_bool grant_reload_procs_priv(THD *thd)
{
  HASH old_proc_priv_hash, old_func_priv_hash;
  TABLE_LIST table;
  my_bool return_val= FALSE;
  DBUG_ENTER("grant_reload_procs_priv");

  bzero((char*) &table, sizeof(table));
  table.alias= table.table_name= (char*) "procs_priv";
  table.db= (char *) "mysql";
  table.lock_type= TL_READ;
  table.skip_temporary= 1;

  if (simple_open_n_lock_tables(thd, &table))
  {
    close_thread_tables(thd);
    DBUG_RETURN(TRUE);
  }

  rw_wrlock(&LOCK_grant);
  /* Save a copy of the current hash if we need to undo the grant load */
  old_proc_priv_hash= proc_priv_hash;
  old_func_priv_hash= func_priv_hash;

  if ((return_val= grant_load_procs_priv(table.table)))
  {
    /* Error; Reverting to old hash */
    DBUG_PRINT("error",("Reverting to old privileges"));
    grant_free();
    proc_priv_hash= old_proc_priv_hash;
    func_priv_hash= old_func_priv_hash;
  }
  else
  {
    hash_free(&old_proc_priv_hash);
    hash_free(&old_func_priv_hash);
  }
  rw_unlock(&LOCK_grant);

  close_thread_tables(thd);
  DBUG_RETURN(return_val);
}


/**
  @brief Reload information about table and column level privileges if possible

  @param thd Current thread

  Locked tables are checked by acl_reload() and doesn't have to be checked
  in this call.
  This function is also used for initialization of structures responsible
  for table/column-level privilege checking.

  @return Error state
    @retval FALSE Success
    @retval TRUE  Error
*/

my_bool grant_reload(THD *thd)
{
  TABLE_LIST tables[2];
  HASH old_column_priv_hash;
  MEM_ROOT old_mem;
  my_bool return_val= 1;
  DBUG_ENTER("grant_reload");

  /* Don't do anything if running with --skip-grant-tables */
  if (!initialized)
    DBUG_RETURN(0);

  bzero((char*) tables, sizeof(tables));
  tables[0].alias= tables[0].table_name= (char*) "tables_priv";
  tables[1].alias= tables[1].table_name= (char*) "columns_priv";
  tables[0].db= tables[1].db= (char *) "mysql";
  tables[0].next_local= tables[0].next_global= tables+1;
  tables[0].lock_type= tables[1].lock_type= TL_READ;
  tables[0].skip_temporary= tables[1].skip_temporary= TRUE;
  /*
    To avoid deadlocks we should obtain table locks before
    obtaining LOCK_grant rwlock.
  */
  if (simple_open_n_lock_tables(thd, tables))
    goto end;

  rw_wrlock(&LOCK_grant);
  old_column_priv_hash= column_priv_hash;

  /*
    Create a new memory pool but save the current memory pool to make an undo
    opertion possible in case of failure.
  */
  old_mem= memex;
  init_sql_alloc(&memex, ACL_ALLOC_BLOCK_SIZE, 0);

  if ((return_val= grant_load(thd, tables)))
  {						// Error. Revert to old hash
    DBUG_PRINT("error",("Reverting to old privileges"));
    grant_free();				/* purecov: deadcode */
    column_priv_hash= old_column_priv_hash;	/* purecov: deadcode */
    memex= old_mem;				/* purecov: deadcode */
  }
  else
  {
    hash_free(&old_column_priv_hash);
    free_root(&old_mem,MYF(0));
  }
  rw_unlock(&LOCK_grant);
  close_thread_tables(thd);

  /*
    It is OK failing to load procs_priv table because we may be
    working with 4.1 privilege tables.
  */
  if (grant_reload_procs_priv(thd))
    return_val= 1;

  rw_wrlock(&LOCK_grant);
  grant_version++;
  rw_unlock(&LOCK_grant);

end:
  DBUG_RETURN(return_val);
}

/****************************************************************************
  Check table level grants

  SYNOPSIS
   bool check_grant()
   thd		Thread handler
   want_access  Bits of privileges user needs to have
   tables	List of tables to check. The user should have 'want_access'
		to all tables in list.
   show_table	<> 0 if we are in show table. In this case it's enough to have
	        any privilege for the table
   number	Check at most this number of tables.
   no_errors	If 0 then we write an error. The error is sent directly to
		the client

   RETURN
     0  ok
     1  Error: User did not have the requested privileges

   NOTE
     This functions assumes that either number of tables to be inspected
     by it is limited explicitly (i.e. is is not UINT_MAX) or table list
     used and thd->lex->query_tables_own_last value correspond to each
     other (the latter should be either 0 or point to next_global member
     of one of elements of this table list).
****************************************************************************/

bool check_grant(THD *thd, ulong want_access, TABLE_LIST *tables,
		 uint show_table, uint number, bool no_errors)
{
  TABLE_LIST *table, *first_not_own_table= thd->lex->first_not_own_table();
  Security_context *sctx= thd->security_ctx;
  uint i;
  ulong orig_want_access= want_access;
  DBUG_ENTER("check_grant");
  DBUG_ASSERT(number > 0);

  /*
    Walk through the list of tables that belong to the query and save the
    requested access (orig_want_privilege) to be able to use it when
    checking access rights to the underlying tables of a view. Our grant
    system gradually eliminates checked bits from want_privilege and thus
    after all checks are done we can no longer use it.
    The check that first_not_own_table is not reached is for the case when
    the given table list refers to the list for prelocking (contains tables
    of other queries). For simple queries first_not_own_table is 0.
  */
  for (i= 0, table= tables;
       i < number  && table != first_not_own_table;
       table= table->next_global, i++)
  {
    /* Remove SHOW_VIEW_ACL, because it will be checked during making view */
    table->grant.orig_want_privilege= (want_access & ~SHOW_VIEW_ACL);
  }

  rw_rdlock(&LOCK_grant);
  for (table= tables;
       table && number-- && table != first_not_own_table;
       table= table->next_global)
  {
    GRANT_TABLE *grant_table;
    sctx = test(table->security_ctx) ?
      table->security_ctx : thd->security_ctx;

    want_access= orig_want_access;
    want_access&= ~sctx->master_access;
    if (!want_access)
      continue;                                 // ok

    if (!(~table->grant.privilege & want_access) ||
        table->is_anonymous_derived_table() || table->schema_table)
    {
      /*
        It is subquery in the FROM clause. VIEW set table->derived after
        table opening, but this function always called before table opening.
      */
      if (!table->referencing_view)
      {
        /*
          If it's a temporary table created for a subquery in the FROM
          clause, or an INFORMATION_SCHEMA table, drop the request for
          a privilege.
        */
        table->grant.want_privilege= 0;
      }
      continue;
    }
    if (!(grant_table= table_hash_search(sctx->host, sctx->ip,
                                         table->get_db_name(), sctx->priv_user,
                                         table->get_table_name(), FALSE)))
    {
      want_access &= ~table->grant.privilege;
      goto err;					// No grants
    }
    if (show_table)
      continue;					// We have some priv on this

    table->grant.grant_table=grant_table;	// Remember for column test
    table->grant.version=grant_version;
    table->grant.privilege|= grant_table->privs;
    table->grant.want_privilege= ((want_access & COL_ACLS)
				  & ~table->grant.privilege);

    if (!(~table->grant.privilege & want_access))
      continue;

    if (want_access & ~(grant_table->cols | table->grant.privilege))
    {
      want_access &= ~(grant_table->cols | table->grant.privilege);
      goto err;					// impossible
    }
  }
  rw_unlock(&LOCK_grant);
  DBUG_RETURN(0);

err:
  rw_unlock(&LOCK_grant);
  if (!no_errors)				// Not a silent skip of table
  {
    char command[128];
    get_privilege_desc(command, sizeof(command), want_access);
    my_error(ER_TABLEACCESS_DENIED_ERROR, MYF(0),
             command,
             sctx->priv_user,
             sctx->host_or_ip,
             table ? table->get_table_name() : "unknown");
  }
  DBUG_RETURN(1);
}


/*
  Check column rights in given security context

  SYNOPSIS
    check_grant_column()
    thd                  thread handler
    grant                grant information structure
    db_name              db name
    table_name           table  name
    name                 column name
    length               column name length
    sctx                 security context

  RETURN
    FALSE OK
    TRUE  access denied
*/

bool check_grant_column(THD *thd, GRANT_INFO *grant,
			const char *db_name, const char *table_name,
			const char *name, uint length,  Security_context *sctx)
{
  GRANT_TABLE *grant_table;
  GRANT_COLUMN *grant_column;
  ulong want_access= grant->want_privilege & ~grant->privilege;
  DBUG_ENTER("check_grant_column");
  DBUG_PRINT("enter", ("table: %s  want_access: %lu", table_name, want_access));

  if (!want_access)
    DBUG_RETURN(0);				// Already checked

  rw_rdlock(&LOCK_grant);

  /* reload table if someone has modified any grants */

  if (grant->version != grant_version)
  {
    grant->grant_table=
      table_hash_search(sctx->host, sctx->ip, db_name,
			sctx->priv_user,
			table_name, 0);         /* purecov: inspected */
    grant->version= grant_version;		/* purecov: inspected */
  }
  if (!(grant_table= grant->grant_table))
    goto err;					/* purecov: deadcode */

  grant_column=column_hash_search(grant_table, name, length);
  if (grant_column && !(~grant_column->rights & want_access))
  {
    rw_unlock(&LOCK_grant);
    DBUG_RETURN(0);
  }

err:
  rw_unlock(&LOCK_grant);
  char command[128];
  get_privilege_desc(command, sizeof(command), want_access);
  my_error(ER_COLUMNACCESS_DENIED_ERROR, MYF(0),
           command,
           sctx->priv_user,
           sctx->host_or_ip,
           name,
           table_name);
  DBUG_RETURN(1);
}


/*
  Check the access right to a column depending on the type of table.

  SYNOPSIS
    check_column_grant_in_table_ref()
    thd              thread handler
    table_ref        table reference where to check the field
    name             name of field to check
    length           length of name

  DESCRIPTION
    Check the access rights to a column depending on the type of table
    reference where the column is checked. The function provides a
    generic interface to check column access rights that hides the
    heterogeneity of the column representation - whether it is a view
    or a stored table colum.

  RETURN
    FALSE OK
    TRUE  access denied
*/

bool check_column_grant_in_table_ref(THD *thd, TABLE_LIST * table_ref,
                                     const char *name, uint length)
{
  GRANT_INFO *grant;
  const char *db_name;
  const char *table_name;
  Security_context *sctx= test(table_ref->security_ctx) ?
                          table_ref->security_ctx : thd->security_ctx;

  if (table_ref->view || table_ref->field_translation)
  {
    /* View or derived information schema table. */
    ulong view_privs;
    grant= &(table_ref->grant);
    db_name= table_ref->view_db.str;
    table_name= table_ref->view_name.str;
    if (table_ref->belong_to_view && 
        thd->lex->sql_command == SQLCOM_SHOW_FIELDS)
    {
      view_privs= get_column_grant(thd, grant, db_name, table_name, name);
      if (view_privs & VIEW_ANY_ACL)
      {
        table_ref->belong_to_view->allowed_show= TRUE;
        return FALSE;
      }
      table_ref->belong_to_view->allowed_show= FALSE;
      my_message(ER_VIEW_NO_EXPLAIN, ER(ER_VIEW_NO_EXPLAIN), MYF(0));
      return TRUE;
    }
  }
  else
  {
    /* Normal or temporary table. */
    TABLE *table= table_ref->table;
    grant= &(table->grant);
    db_name= table->s->db.str;
    table_name= table->s->table_name.str;
  }

  if (grant->want_privilege)
    return check_grant_column(thd, grant, db_name, table_name, name,
                              length, sctx);
  else
    return FALSE;

}


/** 
  @brief check if a query can access a set of columns

  @param  thd  the current thread
  @param  want_access_arg  the privileges requested
  @param  fields an iterator over the fields of a table reference.
  @return Operation status
    @retval 0 Success
    @retval 1 Falure
  @details This function walks over the columns of a table reference 
   The columns may originate from different tables, depending on the kind of
   table reference, e.g. join, view.
   For each table it will retrieve the grant information and will use it
   to check the required access privileges for the fields requested from it.
*/    
bool check_grant_all_columns(THD *thd, ulong want_access_arg, 
                             Field_iterator_table_ref *fields)
{
  Security_context *sctx= thd->security_ctx;
  ulong want_access= want_access_arg;
  const char *table_name= NULL;

  const char* db_name; 
  GRANT_INFO *grant;
  /* Initialized only to make gcc happy */
  GRANT_TABLE *grant_table= NULL;
  /* 
     Flag that gets set if privilege checking has to be performed on column
     level.
  */
  bool using_column_privileges= FALSE;

  rw_rdlock(&LOCK_grant);

  for (; !fields->end_of_fields(); fields->next())
  {
    const char *field_name= fields->name();

    if (table_name != fields->get_table_name())
    {
      table_name= fields->get_table_name();
      db_name= fields->get_db_name();
      grant= fields->grant();
      /* get a fresh one for each table */
      want_access= want_access_arg & ~grant->privilege;
      if (want_access)
      {
        /* reload table if someone has modified any grants */
        if (grant->version != grant_version)
        {
          grant->grant_table=
            table_hash_search(sctx->host, sctx->ip, db_name,
                              sctx->priv_user,
                              table_name, 0);	/* purecov: inspected */
          grant->version= grant_version;	/* purecov: inspected */
        }

        grant_table= grant->grant_table;
        DBUG_ASSERT (grant_table);
      }
    }

    if (want_access)
    {
      GRANT_COLUMN *grant_column= 
        column_hash_search(grant_table, field_name,
                           (uint) strlen(field_name));
      if (grant_column)
        using_column_privileges= TRUE;
      if (!grant_column || (~grant_column->rights & want_access))
        goto err;
    }
  }
  rw_unlock(&LOCK_grant);
  return 0;

err:
  rw_unlock(&LOCK_grant);

  char command[128];
  get_privilege_desc(command, sizeof(command), want_access);
  /*
    Do not give an error message listing a column name unless the user has
    privilege to see all columns.
  */
  if (using_column_privileges)
    my_error(ER_TABLEACCESS_DENIED_ERROR, MYF(0),
             command, sctx->priv_user,
             sctx->host_or_ip, table_name); 
  else
    my_error(ER_COLUMNACCESS_DENIED_ERROR, MYF(0),
             command,
             sctx->priv_user,
             sctx->host_or_ip,
             fields->name(),
             table_name);
  return 1;
}


static bool check_grant_db_routine(THD *thd, const char *db, HASH *hash)
{
  Security_context *sctx= thd->security_ctx;

  for (uint idx= 0; idx < hash->records; ++idx)
  {
    GRANT_NAME *item= (GRANT_NAME*) hash_element(hash, idx);

    if (strcmp(item->user, sctx->priv_user) == 0 &&
        strcmp(item->db, db) == 0 &&
        compare_hostname(&item->host, sctx->host, sctx->ip))
    {
      return FALSE;
    }
  }

  return TRUE;
}


/*
  Check if a user has the right to access a database
  Access is accepted if he has a grant for any table/routine in the database
  Return 1 if access is denied
*/

bool check_grant_db(THD *thd,const char *db)
{
  Security_context *sctx= thd->security_ctx;
  char helping [NAME_LEN+USERNAME_LENGTH+2];
  uint len;
  bool error= TRUE;
  size_t copy_length;

  copy_length= (size_t) (strlen(sctx->priv_user ? sctx->priv_user : "") +
                 strlen(db ? db : ""));

  /*
    Make sure that strmov() operations do not result in buffer overflow.
  */
  if (copy_length >= (NAME_LEN+USERNAME_LENGTH+2))
    return 1;

  len= (uint) (strmov(strmov(helping, sctx->priv_user) + 1, db) - helping) + 1;

  rw_rdlock(&LOCK_grant);

  for (uint idx=0 ; idx < column_priv_hash.records ; idx++)
  {
    GRANT_TABLE *grant_table= (GRANT_TABLE*) hash_element(&column_priv_hash,
							  idx);
    if (len < grant_table->key_length &&
	!memcmp(grant_table->hash_key,helping,len) &&
        compare_hostname(&grant_table->host, sctx->host, sctx->ip))
    {
      error= FALSE; /* Found match. */
      break;
    }
  }

  if (error)
    error= check_grant_db_routine(thd, db, &proc_priv_hash) &&
           check_grant_db_routine(thd, db, &func_priv_hash);

  rw_unlock(&LOCK_grant);

  return error;
}


/****************************************************************************
  Check routine level grants

  SYNPOSIS
   bool check_grant_routine()
   thd		Thread handler
   want_access  Bits of privileges user needs to have
   procs	List of routines to check. The user should have 'want_access'
   is_proc	True if the list is all procedures, else functions
   no_errors	If 0 then we write an error. The error is sent directly to
		the client

   RETURN
     0  ok
     1  Error: User did not have the requested privielges
****************************************************************************/

bool check_grant_routine(THD *thd, ulong want_access,
			 TABLE_LIST *procs, bool is_proc, bool no_errors)
{
  TABLE_LIST *table;
  Security_context *sctx= thd->security_ctx;
  char *user= sctx->priv_user;
  char *host= sctx->priv_host;
  DBUG_ENTER("check_grant_routine");

  want_access&= ~sctx->master_access;
  if (!want_access)
    DBUG_RETURN(0);                             // ok

  rw_rdlock(&LOCK_grant);
  for (table= procs; table; table= table->next_global)
  {
    GRANT_NAME *grant_proc;
    if ((grant_proc= routine_hash_search(host, sctx->ip, table->db, user,
					 table->table_name, is_proc, 0)))
      table->grant.privilege|= grant_proc->privs;

    if (want_access & ~table->grant.privilege)
    {
      want_access &= ~table->grant.privilege;
      goto err;
    }
  }
  rw_unlock(&LOCK_grant);
  DBUG_RETURN(0);
err:
  rw_unlock(&LOCK_grant);
  if (!no_errors)
  {
    char buff[1024];
    const char *command="";
    if (table)
      strxmov(buff, table->db, ".", table->table_name, NullS);
    if (want_access & EXECUTE_ACL)
      command= "execute";
    else if (want_access & ALTER_PROC_ACL)
      command= "alter routine";
    else if (want_access & GRANT_ACL)
      command= "grant";
    my_error(ER_PROCACCESS_DENIED_ERROR, MYF(0),
             command, user, host, table ? buff : "unknown");
  }
  DBUG_RETURN(1);
}


/*
  Check if routine has any of the 
  routine level grants
  
  SYNPOSIS
   bool    check_routine_level_acl()
   thd	        Thread handler
   db           Database name
   name         Routine name

  RETURN
   0            Ok 
   1            error
*/

bool check_routine_level_acl(THD *thd, const char *db, const char *name, 
                             bool is_proc)
{
  bool no_routine_acl= 1;
  GRANT_NAME *grant_proc;
  Security_context *sctx= thd->security_ctx;
  rw_rdlock(&LOCK_grant);
  if ((grant_proc= routine_hash_search(sctx->priv_host,
                                       sctx->ip, db,
                                       sctx->priv_user,
                                       name, is_proc, 0)))
    no_routine_acl= !(grant_proc->privs & SHOW_PROC_ACLS);
  rw_unlock(&LOCK_grant);
  return no_routine_acl;
}


/*****************************************************************************
  Functions to retrieve the grant for a table/column  (for SHOW functions)
*****************************************************************************/

ulong get_table_grant(THD *thd, TABLE_LIST *table)
{
  ulong privilege;
  Security_context *sctx= thd->security_ctx;
  const char *db = table->db ? table->db : thd->db;
  GRANT_TABLE *grant_table;

  rw_rdlock(&LOCK_grant);
#ifdef EMBEDDED_LIBRARY
  grant_table= NULL;
#else
  grant_table= table_hash_search(sctx->host, sctx->ip, db, sctx->priv_user,
				 table->table_name, 0);
#endif
  table->grant.grant_table=grant_table; // Remember for column test
  table->grant.version=grant_version;
  if (grant_table)
    table->grant.privilege|= grant_table->privs;
  privilege= table->grant.privilege;
  rw_unlock(&LOCK_grant);
  return privilege;
}


/*
  Determine the access priviliges for a field.

  SYNOPSIS
    get_column_grant()
    thd         thread handler
    grant       grants table descriptor
    db_name     name of database that the field belongs to
    table_name  name of table that the field belongs to
    field_name  name of field

  DESCRIPTION
    The procedure may also modify: grant->grant_table and grant->version.

  RETURN
    The access priviliges for the field db_name.table_name.field_name
*/

ulong get_column_grant(THD *thd, GRANT_INFO *grant,
                       const char *db_name, const char *table_name,
                       const char *field_name)
{
  GRANT_TABLE *grant_table;
  GRANT_COLUMN *grant_column;
  ulong priv;

  rw_rdlock(&LOCK_grant);
  /* reload table if someone has modified any grants */
  if (grant->version != grant_version)
  {
    Security_context *sctx= thd->security_ctx;
    grant->grant_table=
      table_hash_search(sctx->host, sctx->ip,
                        db_name, sctx->priv_user,
			table_name, 0);	        /* purecov: inspected */
    grant->version= grant_version;              /* purecov: inspected */
  }

  if (!(grant_table= grant->grant_table))
    priv= grant->privilege;
  else
  {
    grant_column= column_hash_search(grant_table, field_name,
                                     (uint) strlen(field_name));
    if (!grant_column)
      priv= (grant->privilege | grant_table->privs);
    else
      priv= (grant->privilege | grant_table->privs | grant_column->rights);
  }
  rw_unlock(&LOCK_grant);
  return priv;
}


/* Help function for mysql_show_grants */

static void add_user_option(String *grant, ulong value, const char *name)
{
  if (value)
  {
    char buff[22], *p; // just as in int2str
    grant->append(' ');
    grant->append(name, strlen(name));
    grant->append(' ');
    p=int10_to_str(value, buff, 10);
    grant->append(buff,p-buff);
  }
}

static const char *command_array[]=
{
  "SELECT", "INSERT", "UPDATE", "DELETE", "CREATE", "DROP", "RELOAD",
  "SHUTDOWN", "PROCESS","FILE", "GRANT", "REFERENCES", "INDEX",
  "ALTER", "SHOW DATABASES", "SUPER", "CREATE TEMPORARY TABLES",
  "LOCK TABLES", "EXECUTE", "REPLICATION SLAVE", "REPLICATION CLIENT",
  "CREATE VIEW", "SHOW VIEW", "CREATE ROUTINE", "ALTER ROUTINE",
  "CREATE USER", "EVENT", "TRIGGER"
};

static uint command_lengths[]=
{
  6, 6, 6, 6, 6, 4, 6, 8, 7, 4, 5, 10, 5, 5, 14, 5, 23, 11, 7, 17, 18, 11, 9,
  14, 13, 11, 5, 7
};


static int show_routine_grants(THD *thd, LEX_USER *lex_user, HASH *hash,
                               const char *type, int typelen,
                               char *buff, int buffsize);


/*
  SHOW GRANTS;  Send grants for a user to the client

  IMPLEMENTATION
   Send to client grant-like strings depicting user@host privileges
*/

bool mysql_show_grants(THD *thd,LEX_USER *lex_user)
{
  ulong want_access;
  uint counter,index;
  int  error = 0;
  ACL_USER *acl_user;
  ACL_DB *acl_db;
  char buff[1024];
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("mysql_show_grants");

  LINT_INIT(acl_user);
  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    DBUG_RETURN(TRUE);
  }

  rw_rdlock(&LOCK_grant);
  VOID(pthread_mutex_lock(&acl_cache->lock));

  acl_user= find_acl_user(lex_user->host.str, lex_user->user.str, TRUE);
  if (!acl_user)
  {
    VOID(pthread_mutex_unlock(&acl_cache->lock));
    rw_unlock(&LOCK_grant);

    my_error(ER_NONEXISTING_GRANT, MYF(0),
             lex_user->user.str, lex_user->host.str);
    DBUG_RETURN(TRUE);
  }

  Item_string *field=new Item_string("",0,&my_charset_latin1);
  List<Item> field_list;
  field->name=buff;
  field->max_length=1024;
  strxmov(buff,"Grants for ",lex_user->user.str,"@",
	  lex_user->host.str,NullS);
  field_list.push_back(field);
  if (protocol->send_fields(&field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
  {
    VOID(pthread_mutex_unlock(&acl_cache->lock));
    rw_unlock(&LOCK_grant);

    DBUG_RETURN(TRUE);
  }

  /* Add first global access grants */
  {
    String global(buff,sizeof(buff),system_charset_info);
    global.length(0);
    global.append(STRING_WITH_LEN("GRANT "));

    want_access= acl_user->access;
    if (test_all_bits(want_access, (GLOBAL_ACLS & ~ GRANT_ACL)))
      global.append(STRING_WITH_LEN("ALL PRIVILEGES"));
    else if (!(want_access & ~GRANT_ACL))
      global.append(STRING_WITH_LEN("USAGE"));
    else
    {
      bool found=0;
      ulong j,test_access= want_access & ~GRANT_ACL;
      for (counter=0, j = SELECT_ACL;j <= GLOBAL_ACLS;counter++,j <<= 1)
      {
	if (test_access & j)
	{
	  if (found)
	    global.append(STRING_WITH_LEN(", "));
	  found=1;
	  global.append(command_array[counter],command_lengths[counter]);
	}
      }
    }
    global.append (STRING_WITH_LEN(" ON *.* TO '"));
    global.append(lex_user->user.str, lex_user->user.length,
		  system_charset_info);
    global.append (STRING_WITH_LEN("'@'"));
    global.append(lex_user->host.str,lex_user->host.length,
		  system_charset_info);
    global.append ('\'');
    if (acl_user->salt_len)
    {
      char passwd_buff[SCRAMBLED_PASSWORD_CHAR_LENGTH+1];
      if (acl_user->salt_len == SCRAMBLE_LENGTH)
        make_password_from_salt(passwd_buff, acl_user->salt);
      else
        make_password_from_salt_323(passwd_buff, (ulong *) acl_user->salt);
      global.append(STRING_WITH_LEN(" IDENTIFIED BY PASSWORD '"));
      global.append(passwd_buff);
      global.append('\'');
    }
    /* "show grants" SSL related stuff */
    if (acl_user->ssl_type == SSL_TYPE_ANY)
      global.append(STRING_WITH_LEN(" REQUIRE SSL"));
    else if (acl_user->ssl_type == SSL_TYPE_X509)
      global.append(STRING_WITH_LEN(" REQUIRE X509"));
    else if (acl_user->ssl_type == SSL_TYPE_SPECIFIED)
    {
      int ssl_options = 0;
      global.append(STRING_WITH_LEN(" REQUIRE "));
      if (acl_user->x509_issuer)
      {
	ssl_options++;
	global.append(STRING_WITH_LEN("ISSUER \'"));
	global.append(acl_user->x509_issuer,strlen(acl_user->x509_issuer));
	global.append('\'');
      }
      if (acl_user->x509_subject)
      {
	if (ssl_options++)
	  global.append(' ');
	global.append(STRING_WITH_LEN("SUBJECT \'"));
	global.append(acl_user->x509_subject,strlen(acl_user->x509_subject),
                      system_charset_info);
	global.append('\'');
      }
      if (acl_user->ssl_cipher)
      {
	if (ssl_options++)
	  global.append(' ');
	global.append(STRING_WITH_LEN("CIPHER '"));
	global.append(acl_user->ssl_cipher,strlen(acl_user->ssl_cipher),
                      system_charset_info);
	global.append('\'');
      }
    }
    if ((want_access & GRANT_ACL) ||
	(acl_user->user_resource.questions ||
         acl_user->user_resource.updates ||
         acl_user->user_resource.conn_per_hour ||
         acl_user->user_resource.user_conn))
    {
      global.append(STRING_WITH_LEN(" WITH"));
      if (want_access & GRANT_ACL)
	global.append(STRING_WITH_LEN(" GRANT OPTION"));
      add_user_option(&global, acl_user->user_resource.questions,
		      "MAX_QUERIES_PER_HOUR");
      add_user_option(&global, acl_user->user_resource.updates,
		      "MAX_UPDATES_PER_HOUR");
      add_user_option(&global, acl_user->user_resource.conn_per_hour,
		      "MAX_CONNECTIONS_PER_HOUR");
      add_user_option(&global, acl_user->user_resource.user_conn,
		      "MAX_USER_CONNECTIONS");
    }
    protocol->prepare_for_resend();
    protocol->store(global.ptr(),global.length(),global.charset());
    if (protocol->write())
    {
      error= -1;
      goto end;
    }
  }

  /* Add database access */
  for (counter=0 ; counter < acl_dbs.elements ; counter++)
  {
    const char *user, *host;

    acl_db=dynamic_element(&acl_dbs,counter,ACL_DB*);
    if (!(user=acl_db->user))
      user= "";
    if (!(host=acl_db->host.hostname))
      host= "";

    /*
      We do not make SHOW GRANTS case-sensitive here (like REVOKE),
      but make it case-insensitive because that's the way they are
      actually applied, and showing fewer privileges than are applied
      would be wrong from a security point of view.
    */

    if (!strcmp(lex_user->user.str,user) &&
	!my_strcasecmp(system_charset_info, lex_user->host.str, host))
    {
      want_access=acl_db->access;
      if (want_access)
      {
	String db(buff,sizeof(buff),system_charset_info);
	db.length(0);
	db.append(STRING_WITH_LEN("GRANT "));

	if (test_all_bits(want_access,(DB_ACLS & ~GRANT_ACL)))
	  db.append(STRING_WITH_LEN("ALL PRIVILEGES"));
	else if (!(want_access & ~GRANT_ACL))
	  db.append(STRING_WITH_LEN("USAGE"));
	else
	{
	  int found=0, cnt;
	  ulong j,test_access= want_access & ~GRANT_ACL;
	  for (cnt=0, j = SELECT_ACL; j <= DB_ACLS; cnt++,j <<= 1)
	  {
	    if (test_access & j)
	    {
	      if (found)
		db.append(STRING_WITH_LEN(", "));
	      found = 1;
	      db.append(command_array[cnt],command_lengths[cnt]);
	    }
	  }
	}
	db.append (STRING_WITH_LEN(" ON "));
	append_identifier(thd, &db, acl_db->db, strlen(acl_db->db));
	db.append (STRING_WITH_LEN(".* TO '"));
	db.append(lex_user->user.str, lex_user->user.length,
		  system_charset_info);
	db.append (STRING_WITH_LEN("'@'"));
	// host and lex_user->host are equal except for case
	db.append(host, strlen(host), system_charset_info);
	db.append ('\'');
	if (want_access & GRANT_ACL)
	  db.append(STRING_WITH_LEN(" WITH GRANT OPTION"));
	protocol->prepare_for_resend();
	protocol->store(db.ptr(),db.length(),db.charset());
	if (protocol->write())
	{
	  error= -1;
	  goto end;
	}
      }
    }
  }

  /* Add table & column access */
  for (index=0 ; index < column_priv_hash.records ; index++)
  {
    const char *user, *host;
    GRANT_TABLE *grant_table= (GRANT_TABLE*) hash_element(&column_priv_hash,
							  index);

    if (!(user=grant_table->user))
      user= "";
    if (!(host= grant_table->host.hostname))
      host= "";

    /*
      We do not make SHOW GRANTS case-sensitive here (like REVOKE),
      but make it case-insensitive because that's the way they are
      actually applied, and showing fewer privileges than are applied
      would be wrong from a security point of view.
    */

    if (!strcmp(lex_user->user.str,user) &&
	!my_strcasecmp(system_charset_info, lex_user->host.str, host))
    {
      ulong table_access= grant_table->privs;
      if ((table_access | grant_table->cols) != 0)
      {
	String global(buff, sizeof(buff), system_charset_info);
	ulong test_access= (table_access | grant_table->cols) & ~GRANT_ACL;

	global.length(0);
	global.append(STRING_WITH_LEN("GRANT "));

	if (test_all_bits(table_access, (TABLE_ACLS & ~GRANT_ACL)))
	  global.append(STRING_WITH_LEN("ALL PRIVILEGES"));
	else if (!test_access)
	  global.append(STRING_WITH_LEN("USAGE"));
	else
	{
          /* Add specific column access */
	  int found= 0;
	  ulong j;

	  for (counter= 0, j= SELECT_ACL; j <= TABLE_ACLS; counter++, j<<= 1)
	  {
	    if (test_access & j)
	    {
	      if (found)
		global.append(STRING_WITH_LEN(", "));
	      found= 1;
	      global.append(command_array[counter],command_lengths[counter]);

	      if (grant_table->cols)
	      {
		uint found_col= 0;
		for (uint col_index=0 ;
		     col_index < grant_table->hash_columns.records ;
		     col_index++)
		{
		  GRANT_COLUMN *grant_column = (GRANT_COLUMN*)
		    hash_element(&grant_table->hash_columns,col_index);
		  if (grant_column->rights & j)
		  {
		    if (!found_col)
		    {
		      found_col= 1;
		      /*
			If we have a duplicated table level privilege, we
			must write the access privilege name again.
		      */
		      if (table_access & j)
		      {
			global.append(STRING_WITH_LEN(", "));
			global.append(command_array[counter],
				      command_lengths[counter]);
		      }
		      global.append(STRING_WITH_LEN(" ("));
		    }
		    else
		      global.append(STRING_WITH_LEN(", "));
		    global.append(grant_column->column,
				  grant_column->key_length,
				  system_charset_info);
		  }
		}
		if (found_col)
		  global.append(')');
	      }
	    }
	  }
	}
	global.append(STRING_WITH_LEN(" ON "));
	append_identifier(thd, &global, grant_table->db,
			  strlen(grant_table->db));
	global.append('.');
	append_identifier(thd, &global, grant_table->tname,
			  strlen(grant_table->tname));
	global.append(STRING_WITH_LEN(" TO '"));
	global.append(lex_user->user.str, lex_user->user.length,
		      system_charset_info);
	global.append(STRING_WITH_LEN("'@'"));
	// host and lex_user->host are equal except for case
	global.append(host, strlen(host), system_charset_info);
	global.append('\'');
	if (table_access & GRANT_ACL)
	  global.append(STRING_WITH_LEN(" WITH GRANT OPTION"));
	protocol->prepare_for_resend();
	protocol->store(global.ptr(),global.length(),global.charset());
	if (protocol->write())
	{
	  error= -1;
	  break;
	}
      }
    }
  }

  if (show_routine_grants(thd, lex_user, &proc_priv_hash, 
                          STRING_WITH_LEN("PROCEDURE"), buff, sizeof(buff)))
  {
    error= -1;
    goto end;
  }

  if (show_routine_grants(thd, lex_user, &func_priv_hash,
                          STRING_WITH_LEN("FUNCTION"), buff, sizeof(buff)))
  {
    error= -1;
    goto end;
  }

end:
  VOID(pthread_mutex_unlock(&acl_cache->lock));
  rw_unlock(&LOCK_grant);

  my_eof(thd);
  DBUG_RETURN(error);
}

static int show_routine_grants(THD* thd, LEX_USER *lex_user, HASH *hash,
                               const char *type, int typelen,
                               char *buff, int buffsize)
{
  uint counter, index;
  int error= 0;
  Protocol *protocol= thd->protocol;
  /* Add routine access */
  for (index=0 ; index < hash->records ; index++)
  {
    const char *user, *host;
    GRANT_NAME *grant_proc= (GRANT_NAME*) hash_element(hash, index);

    if (!(user=grant_proc->user))
      user= "";
    if (!(host= grant_proc->host.hostname))
      host= "";

    /*
      We do not make SHOW GRANTS case-sensitive here (like REVOKE),
      but make it case-insensitive because that's the way they are
      actually applied, and showing fewer privileges than are applied
      would be wrong from a security point of view.
    */

    if (!strcmp(lex_user->user.str,user) &&
	!my_strcasecmp(system_charset_info, lex_user->host.str, host))
    {
      ulong proc_access= grant_proc->privs;
      if (proc_access != 0)
      {
	String global(buff, buffsize, system_charset_info);
	ulong test_access= proc_access & ~GRANT_ACL;

	global.length(0);
	global.append(STRING_WITH_LEN("GRANT "));

	if (!test_access)
 	  global.append(STRING_WITH_LEN("USAGE"));
	else
	{
          /* Add specific procedure access */
	  int found= 0;
	  ulong j;

	  for (counter= 0, j= SELECT_ACL; j <= PROC_ACLS; counter++, j<<= 1)
	  {
	    if (test_access & j)
	    {
	      if (found)
		global.append(STRING_WITH_LEN(", "));
	      found= 1;
	      global.append(command_array[counter],command_lengths[counter]);
	    }
	  }
	}
	global.append(STRING_WITH_LEN(" ON "));
        global.append(type,typelen);
        global.append(' ');
	append_identifier(thd, &global, grant_proc->db,
			  strlen(grant_proc->db));
	global.append('.');
	append_identifier(thd, &global, grant_proc->tname,
			  strlen(grant_proc->tname));
	global.append(STRING_WITH_LEN(" TO '"));
	global.append(lex_user->user.str, lex_user->user.length,
		      system_charset_info);
	global.append(STRING_WITH_LEN("'@'"));
	// host and lex_user->host are equal except for case
	global.append(host, strlen(host), system_charset_info);
	global.append('\'');
	if (proc_access & GRANT_ACL)
	  global.append(STRING_WITH_LEN(" WITH GRANT OPTION"));
	protocol->prepare_for_resend();
	protocol->store(global.ptr(),global.length(),global.charset());
	if (protocol->write())
	{
	  error= -1;
	  break;
	}
      }
    }
  }
  return error;
}

/*
  Make a clear-text version of the requested privilege.
*/

void get_privilege_desc(char *to, uint max_length, ulong access)
{
  uint pos;
  char *start=to;
  DBUG_ASSERT(max_length >= 30);		// For end ',' removal

  if (access)
  {
    max_length--;				// Reserve place for end-zero
    for (pos=0 ; access ; pos++, access>>=1)
    {
      if ((access & 1) &&
	  command_lengths[pos] + (uint) (to-start) < max_length)
      {
	to= strmov(to, command_array[pos]);
	*to++=',';
      }
    }
    to--;					// Remove end ','
  }
  *to=0;
}


void get_mqh(const char *user, const char *host, USER_CONN *uc)
{
  ACL_USER *acl_user;

  pthread_mutex_lock(&acl_cache->lock);

  if (initialized && (acl_user= find_acl_user(host,user, FALSE)))
    uc->user_resources= acl_user->user_resource;
  else
    bzero((char*) &uc->user_resources, sizeof(uc->user_resources));

  pthread_mutex_unlock(&acl_cache->lock);
}

/*
  Open the grant tables.

  SYNOPSIS
    open_grant_tables()
    thd                         The current thread.
    tables (out)                The 4 elements array for the opened tables.

  DESCRIPTION
    Tables are numbered as follows:
    0 user
    1 db
    2 tables_priv
    3 columns_priv

  RETURN
    1           Skip GRANT handling during replication.
    0           OK.
    < 0         Error.
*/

#define GRANT_TABLES 5
int open_grant_tables(THD *thd, TABLE_LIST *tables)
{
  DBUG_ENTER("open_grant_tables");

  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    DBUG_RETURN(-1);
  }

  bzero((char*) tables, GRANT_TABLES*sizeof(*tables));
  tables->alias= tables->table_name= (char*) "user";
  (tables+1)->alias= (tables+1)->table_name= (char*) "db";
  (tables+2)->alias= (tables+2)->table_name= (char*) "tables_priv";
  (tables+3)->alias= (tables+3)->table_name= (char*) "columns_priv";
  (tables+4)->alias= (tables+4)->table_name= (char*) "procs_priv";
  tables->next_local= tables->next_global= tables+1;
  (tables+1)->next_local= (tables+1)->next_global= tables+2;
  (tables+2)->next_local= (tables+2)->next_global= tables+3;
  (tables+3)->next_local= (tables+3)->next_global= tables+4;
  tables->lock_type= (tables+1)->lock_type=
    (tables+2)->lock_type= (tables+3)->lock_type= 
    (tables+4)->lock_type= TL_WRITE;
  tables->db= (tables+1)->db= (tables+2)->db= 
    (tables+3)->db= (tables+4)->db= (char*) "mysql";

#ifdef HAVE_REPLICATION
  /*
    GRANT and REVOKE are applied the slave in/exclusion rules as they are
    some kind of updates to the mysql.% tables.
  */
  if (thd->slave_thread && rpl_filter->is_on())
  {
    /*
      The tables must be marked "updating" so that tables_ok() takes them into
      account in tests.
    */
    tables[0].updating=tables[1].updating=tables[2].updating=
      tables[3].updating=tables[4].updating=1;
    if (!(thd->spcont || rpl_filter->tables_ok(0, tables)))
      DBUG_RETURN(1);
    tables[0].updating=tables[1].updating=tables[2].updating=
      tables[3].updating=tables[4].updating=0;;
  }
#endif

  if (simple_open_n_lock_tables(thd, tables))
  {						// This should never happen
    close_thread_tables(thd);
    DBUG_RETURN(-1);
  }

  DBUG_RETURN(0);
}

ACL_USER *check_acl_user(LEX_USER *user_name,
			 uint *acl_acl_userdx)
{
  ACL_USER *acl_user= 0;
  uint counter;

  safe_mutex_assert_owner(&acl_cache->lock);

  for (counter= 0 ; counter < acl_users.elements ; counter++)
  {
    const char *user,*host;
    acl_user= dynamic_element(&acl_users, counter, ACL_USER*);
    if (!(user=acl_user->user))
      user= "";
    if (!(host=acl_user->host.hostname))
      host= "";
    if (!strcmp(user_name->user.str,user) &&
	!my_strcasecmp(system_charset_info, user_name->host.str, host))
      break;
  }
  if (counter == acl_users.elements)
    return 0;

  *acl_acl_userdx= counter;
  return acl_user;
}

/*
  Modify a privilege table.

  SYNOPSIS
    modify_grant_table()
    table                       The table to modify.
    host_field                  The host name field.
    user_field                  The user name field.
    user_to                     The new name for the user if to be renamed,
                                NULL otherwise.

  DESCRIPTION
  Update user/host in the current record if user_to is not NULL.
  Delete the current record if user_to is NULL.

  RETURN
    0           OK.
    != 0        Error.
*/

static int modify_grant_table(TABLE *table, Field *host_field,
                              Field *user_field, LEX_USER *user_to)
{
  int error;
  DBUG_ENTER("modify_grant_table");

  if (user_to)
  {
    /* rename */
    store_record(table, record[1]);
    host_field->store(user_to->host.str, user_to->host.length,
                      system_charset_info);
    user_field->store(user_to->user.str, user_to->user.length,
                      system_charset_info);
    if ((error= table->file->ha_update_row(table->record[1], 
                                           table->record[0])) &&
        error != HA_ERR_RECORD_IS_THE_SAME)
      table->file->print_error(error, MYF(0));
    else
      error= 0;
  }
  else
  {
    /* delete */
    if ((error=table->file->ha_delete_row(table->record[0])))
      table->file->print_error(error, MYF(0));
  }

  DBUG_RETURN(error);
}

/*
  Handle a privilege table.

  SYNOPSIS
    handle_grant_table()
    tables                      The array with the four open tables.
    table_no                    The number of the table to handle (0..4).
    drop                        If user_from is to be dropped.
    user_from                   The the user to be searched/dropped/renamed.
    user_to                     The new name for the user if to be renamed,
                                NULL otherwise.

  DESCRIPTION
    Scan through all records in a grant table and apply the requested
    operation. For the "user" table, a single index access is sufficient,
    since there is an unique index on (host, user).
    Delete from grant table if drop is true.
    Update in grant table if drop is false and user_to is not NULL.
    Search in grant table if drop is false and user_to is NULL.
    Tables are numbered as follows:
    0 user
    1 db
    2 tables_priv
    3 columns_priv
    4 procs_priv

  RETURN
    > 0         At least one record matched.
    0           OK, but no record matched.
    < 0         Error.
*/

static int handle_grant_table(TABLE_LIST *tables, uint table_no, bool drop,
                              LEX_USER *user_from, LEX_USER *user_to)
{
  int result= 0;
  int error;
  TABLE *table= tables[table_no].table;
  Field *host_field= table->field[0];
  Field *user_field= table->field[table_no ? 2 : 1];
  char *host_str= user_from->host.str;
  char *user_str= user_from->user.str;
  const char *host;
  const char *user;
  uchar user_key[MAX_KEY_LENGTH];
  uint key_prefix_length;
  DBUG_ENTER("handle_grant_table");
  THD *thd= current_thd;

  table->use_all_columns();
  if (! table_no) // mysql.user table
  {
    /*
      The 'user' table has an unique index on (host, user).
      Thus, we can handle everything with a single index access.
      The host- and user fields are consecutive in the user table records.
      So we set host- and user fields of table->record[0] and use the
      pointer to the host field as key.
      index_read_idx() will replace table->record[0] (its first argument)
      by the searched record, if it exists.
    */
    DBUG_PRINT("info",("read table: '%s'  search: '%s'@'%s'",
                       table->s->table_name.str, user_str, host_str));
    host_field->store(host_str, user_from->host.length, system_charset_info);
    user_field->store(user_str, user_from->user.length, system_charset_info);

    key_prefix_length= (table->key_info->key_part[0].store_length +
                        table->key_info->key_part[1].store_length);
    key_copy(user_key, table->record[0], table->key_info, key_prefix_length);

    if ((error= table->file->index_read_idx_map(table->record[0], 0,
                                                user_key, (key_part_map)3,
                                                HA_READ_KEY_EXACT)))
    {
      if (error != HA_ERR_KEY_NOT_FOUND && error != HA_ERR_END_OF_FILE)
      {
        table->file->print_error(error, MYF(0));
        result= -1;
      }
    }
    else
    {
      /* If requested, delete or update the record. */
      result= ((drop || user_to) &&
               modify_grant_table(table, host_field, user_field, user_to)) ?
        -1 : 1; /* Error or found. */
    }
    DBUG_PRINT("info",("read result: %d", result));
  }
  else
  {
    /*
      The non-'user' table do not have indexes on (host, user).
      And their host- and user fields are not consecutive.
      Thus, we need to do a table scan to find all matching records.
    */
    if ((error= table->file->ha_rnd_init(1)))
    {
      table->file->print_error(error, MYF(0));
      result= -1;
    }
    else
    {
#ifdef EXTRA_DEBUG
      DBUG_PRINT("info",("scan table: '%s'  search: '%s'@'%s'",
                         table->s->table_name.str, user_str, host_str));
#endif
      while ((error= table->file->rnd_next(table->record[0])) != 
             HA_ERR_END_OF_FILE)
      {
        if (error)
        {
          /* Most probable 'deleted record'. */
          DBUG_PRINT("info",("scan error: %d", error));
          continue;
        }
        if (! (host= get_field(thd->mem_root, host_field)))
          host= "";
        if (! (user= get_field(thd->mem_root, user_field)))
          user= "";

#ifdef EXTRA_DEBUG
        DBUG_PRINT("loop",("scan fields: '%s'@'%s' '%s' '%s' '%s'",
                           user, host,
                           get_field(thd->mem_root, table->field[1]) /*db*/,
                           get_field(thd->mem_root, table->field[3]) /*table*/,
                           get_field(thd->mem_root,
                                     table->field[4]) /*column*/));
#endif
        if (strcmp(user_str, user) ||
            my_strcasecmp(system_charset_info, host_str, host))
          continue;

        /* If requested, delete or update the record. */
        result= ((drop || user_to) &&
                 modify_grant_table(table, host_field, user_field, user_to)) ?
          -1 : result ? result : 1; /* Error or keep result or found. */
        /* If search is requested, we do not need to search further. */
        if (! drop && ! user_to)
          break ;
      }
      (void) table->file->ha_rnd_end();
      DBUG_PRINT("info",("scan result: %d", result));
    }
  }

  DBUG_RETURN(result);
}


/**
  Handle an in-memory privilege structure.

  @param struct_no  The number of the structure to handle (0..4).
  @param drop       If user_from is to be dropped.
  @param user_from  The the user to be searched/dropped/renamed.
  @param user_to    The new name for the user if to be renamed, NULL otherwise.

  @note
    Scan through all elements in an in-memory grant structure and apply
    the requested operation.
    Delete from grant structure if drop is true.
    Update in grant structure if drop is false and user_to is not NULL.
    Search in grant structure if drop is false and user_to is NULL.
    Structures are enumerated as follows:
    0 ACL_USER
    1 ACL_DB
    2 COLUMN_PRIVILEGES_HASH
    3 PROC_PRIVILEGES_HASH
    4 FUNC_PRIVILEGES_HASH

  @retval > 0  At least one element matched.
  @retval 0    OK, but no element matched.
  @retval -1   Wrong arguments to function or Out of Memory
*/

static int handle_grant_struct(enum enum_acl_lists struct_no, bool drop,
                               LEX_USER *user_from, LEX_USER *user_to)
{
  int result= 0;
  uint idx;
  uint elements;
  const char *user;
  const char *host;
  ACL_USER *acl_user= NULL;
  ACL_DB *acl_db= NULL;
  GRANT_NAME *grant_name= NULL;
  /*
    Dynamic array acl_grant_name used to store pointers to all
    GRANT_NAME objects
  */
  Dynamic_array<GRANT_NAME *> acl_grant_name;
  HASH *grant_name_hash= NULL;
  DBUG_ENTER("handle_grant_struct");
  DBUG_PRINT("info",("scan struct: %u  search: '%s'@'%s'",
                     struct_no, user_from->user.str, user_from->host.str));

  LINT_INIT(user);
  LINT_INIT(host);

  safe_mutex_assert_owner(&acl_cache->lock);

  /* Get the number of elements in the in-memory structure. */
  switch (struct_no) {
  case USER_ACL:
    elements= acl_users.elements;
    break;
  case DB_ACL:
    elements= acl_dbs.elements;
    break;
  case COLUMN_PRIVILEGES_HASH:
    elements= column_priv_hash.records;
    grant_name_hash= &column_priv_hash;
    break;
  case PROC_PRIVILEGES_HASH:
    elements= proc_priv_hash.records;
    grant_name_hash= &proc_priv_hash;
    break;
  case FUNC_PRIVILEGES_HASH:
    elements= func_priv_hash.records;
    grant_name_hash= &func_priv_hash;
    break;
  default:
    return -1;
  }

#ifdef EXTRA_DEBUG
    DBUG_PRINT("loop",("scan struct: %u  search    user: '%s'  host: '%s'",
                       struct_no, user_from->user.str, user_from->host.str));
#endif
  /* Loop over all elements. */
  for (idx= 0; idx < elements; idx++)
  {
    /*
      Get a pointer to the element.
    */
    switch (struct_no) {
    case USER_ACL:
      acl_user= dynamic_element(&acl_users, idx, ACL_USER*);
      user= acl_user->user;
      host= acl_user->host.hostname;
    break;

    case DB_ACL:
      acl_db= dynamic_element(&acl_dbs, idx, ACL_DB*);
      user= acl_db->user;
      host= acl_db->host.hostname;
      break;

    case COLUMN_PRIVILEGES_HASH:
    case PROC_PRIVILEGES_HASH:
    case FUNC_PRIVILEGES_HASH:
      grant_name= (GRANT_NAME*) hash_element(grant_name_hash, idx);
      user= grant_name->user;
      host= grant_name->host.hostname;
      break;

    default:
      MY_ASSERT_UNREACHABLE();
    }
    if (! user)
      user= "";
    if (! host)
      host= "";

#ifdef EXTRA_DEBUG
    DBUG_PRINT("loop",("scan struct: %u  index: %u  user: '%s'  host: '%s'",
                       struct_no, idx, user, host));
#endif
    if (strcmp(user_from->user.str, user) ||
        my_strcasecmp(system_charset_info, user_from->host.str, host))
      continue;

    result= 1; /* At least one element found. */
    if ( drop )
    {
      switch ( struct_no ) {
      case USER_ACL:
        delete_dynamic_element(&acl_users, idx);
        elements--;
        /*
        - If we are iterating through an array then we just have moved all
          elements after the current element one position closer to its head.
          This means that we have to take another look at the element at
          current position as it is a new element from the array's tail.
        - This is valid for USER_ACL, DB_ACL.
        */
        idx--;
        break;

      case DB_ACL:
        delete_dynamic_element(&acl_dbs, idx);
        elements--;
        idx--;
        break;

      case COLUMN_PRIVILEGES_HASH:
      case PROC_PRIVILEGES_HASH:
      case FUNC_PRIVILEGES_HASH:
        /*
          Deleting while traversing a hash table is not valid procedure and
          hence we save pointers to GRANT_NAME objects for later processing.
        */
        if (acl_grant_name.append(grant_name))
          DBUG_RETURN(-1);
	break;
      }
    }
    else if ( user_to )
    {
      switch ( struct_no ) {
      case USER_ACL:
        acl_user->user= strdup_root(&mem, user_to->user.str);
        acl_user->host.hostname= strdup_root(&mem, user_to->host.str);
        break;

      case DB_ACL:
        acl_db->user= strdup_root(&mem, user_to->user.str);
        acl_db->host.hostname= strdup_root(&mem, user_to->host.str);
        break;

      case COLUMN_PRIVILEGES_HASH:
      case PROC_PRIVILEGES_HASH:
      case FUNC_PRIVILEGES_HASH:
        /*
          Updating while traversing a hash table is not valid procedure and
          hence we save pointers to GRANT_NAME objects for later processing.
        */
        if (acl_grant_name.append(grant_name))
          DBUG_RETURN(-1);
        break;
      }
    }
    else
    {
      /* If search is requested, we do not need to search further. */
      break;
    }
  }

  if (drop || user_to)
  {
    /*
      Traversing the elements stored in acl_grant_name dynamic array
      to either delete or update them.
    */
    for (int i= 0; i < acl_grant_name.elements(); ++i)
    {
      grant_name= acl_grant_name.at(i);

      if (drop)
      {
        my_hash_delete(grant_name_hash, (uchar *) grant_name);
      }
      else
      {
        /*
          Save old hash key and its length to be able properly update
          element position in hash.
        */
        char *old_key= grant_name->hash_key;
        size_t old_key_length= grant_name->key_length;

        /*
          Update the grant structure with the new user name and host name.
        */
        grant_name->set_user_details(user_to->host.str, grant_name->db,
                                     user_to->user.str, grant_name->tname,
                                     TRUE);

        /*
          Since username is part of the hash key, when the user name
          is renamed, the hash key is changed. Update the hash to
          ensure that the position matches the new hash key value
        */
        my_hash_update(grant_name_hash, (uchar*) grant_name, (uchar*) old_key,
                       old_key_length);
      }
    }
  }

#ifdef EXTRA_DEBUG
  DBUG_PRINT("loop",("scan struct: %u  result %d", struct_no, result));
#endif

  DBUG_RETURN(result);
}


/*
  Handle all privilege tables and in-memory privilege structures.

  SYNOPSIS
    handle_grant_data()
    tables                      The array with the four open tables.
    drop                        If user_from is to be dropped.
    user_from                   The the user to be searched/dropped/renamed.
    user_to                     The new name for the user if to be renamed,
                                NULL otherwise.

  DESCRIPTION
    Go through all grant tables and in-memory grant structures and apply
    the requested operation.
    Delete from grant data if drop is true.
    Update in grant data if drop is false and user_to is not NULL.
    Search in grant data if drop is false and user_to is NULL.

  RETURN
    > 0         At least one element matched.
    0           OK, but no element matched.
    < 0         Error.
*/

static int handle_grant_data(TABLE_LIST *tables, bool drop,
                             LEX_USER *user_from, LEX_USER *user_to)
{
  int result= 0;
  int found;
  int ret;
  DBUG_ENTER("handle_grant_data");

  /* Handle user table. */
  if ((found= handle_grant_table(tables, 0, drop, user_from, user_to)) < 0)
  {
    /* Handle of table failed, don't touch the in-memory array. */
    result= -1;
  }
  else
  {
    /* Handle user array. */
    if (((ret= handle_grant_struct(USER_ACL, drop, user_from, user_to) > 0) &&
         ! result) || found)
    {
      result= 1; /* At least one record/element found. */
      /* If search is requested, we do not need to search further. */
      if (! drop && ! user_to)
        goto end;
    }
    else if (ret < 0)
    {
      result= -1;
      goto end;
    }
  }

  /* Handle db table. */
  if ((found= handle_grant_table(tables, 1, drop, user_from, user_to)) < 0)
  {
    /* Handle of table failed, don't touch the in-memory array. */
    result= -1;
  }
  else
  {
    /* Handle db array. */
    if ((((ret= handle_grant_struct(DB_ACL, drop, user_from, user_to) > 0) &&
          ! result) || found) && ! result)
    {
      result= 1; /* At least one record/element found. */
      /* If search is requested, we do not need to search further. */
      if (! drop && ! user_to)
        goto end;
    }
    else if (ret < 0)
    {
      result= -1;
      goto end;
    }
  }

  /* Handle stored routines table. */
  if ((found= handle_grant_table(tables, 4, drop, user_from, user_to)) < 0)
  {
    /* Handle of table failed, don't touch in-memory array. */
    result= -1;
  }
  else
  {
    /* Handle procs array. */
    if ((((ret= handle_grant_struct(PROC_PRIVILEGES_HASH, drop, user_from,
                                    user_to) > 0) && ! result) || found) &&
        ! result)
    {
      result= 1; /* At least one record/element found. */
      /* If search is requested, we do not need to search further. */
      if (! drop && ! user_to)
        goto end;
    }
    else if (ret < 0)
    {
      result= -1;
      goto end;
    }
    /* Handle funcs array. */
    if ((((ret= handle_grant_struct(FUNC_PRIVILEGES_HASH, drop, user_from,
                                    user_to) > 0) && ! result) || found) &&
        ! result)
    {
      result= 1; /* At least one record/element found. */
      /* If search is requested, we do not need to search further. */
      if (! drop && ! user_to)
        goto end;
    }
    else if (ret < 0)
    {
      result= -1;
      goto end;
    }
  }

  /* Handle tables table. */
  if ((found= handle_grant_table(tables, 2, drop, user_from, user_to)) < 0)
  {
    /* Handle of table failed, don't touch columns and in-memory array. */
    result= -1;
  }
  else
  {
    if (found && ! result)
    {
      result= 1; /* At least one record found. */
      /* If search is requested, we do not need to search further. */
      if (! drop && ! user_to)
        goto end;
    }

    /* Handle columns table. */
    if ((found= handle_grant_table(tables, 3, drop, user_from, user_to)) < 0)
    {
      /* Handle of table failed, don't touch the in-memory array. */
      result= -1;
    }
    else
    {
      /* Handle columns hash. */
      if ((((ret= handle_grant_struct(COLUMN_PRIVILEGES_HASH, drop, user_from,
                                      user_to) > 0) && ! result) || found) &&
          ! result)
        result= 1; /* At least one record/element found. */
      else if (ret < 0)
        result= -1;
    }
  }
 end:
  DBUG_RETURN(result);
}


static void append_user(String *str, LEX_USER *user)
{
  if (str->length())
    str->append(',');
  str->append('\'');
  str->append(user->user.str);
  str->append(STRING_WITH_LEN("'@'"));
  str->append(user->host.str);
  str->append('\'');
}


/*
  Create a list of users.

  SYNOPSIS
    mysql_create_user()
    thd                         The current thread.
    list                        The users to create.

  RETURN
    FALSE       OK.
    TRUE        Error.
*/

bool mysql_create_user(THD *thd, List <LEX_USER> &list)
{
  int result;
  String wrong_users;
  LEX_USER *user_name, *tmp_user_name;
  List_iterator <LEX_USER> user_list(list);
  TABLE_LIST tables[GRANT_TABLES];
  bool some_users_created= FALSE;
  bool save_binlog_row_based;
  DBUG_ENTER("mysql_create_user");

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  save_binlog_row_based= thd->current_stmt_binlog_row_based;
  thd->clear_current_stmt_binlog_row_based();

  /* CREATE USER may be skipped on replication client. */
  if ((result= open_grant_tables(thd, tables)))
  {
    /* Restore the state of binlog format */
    thd->current_stmt_binlog_row_based= save_binlog_row_based;
    DBUG_RETURN(result != 1);
  }

  rw_wrlock(&LOCK_grant);
  VOID(pthread_mutex_lock(&acl_cache->lock));

  while ((tmp_user_name= user_list++))
  {
    if (!(user_name= get_current_user(thd, tmp_user_name)))
    {
      result= TRUE;
      continue;
    }

    /*
      Search all in-memory structures and grant tables
      for a mention of the new user name.
    */
    if (handle_grant_data(tables, 0, user_name, NULL))
    {
      append_user(&wrong_users, user_name);
      result= TRUE;
      continue;
    }

    some_users_created= TRUE;
    if (replace_user_table(thd, tables[0].table, *user_name, 0, 0, 1, 0))
    {
      append_user(&wrong_users, user_name);
      result= TRUE;
    }
  }

  VOID(pthread_mutex_unlock(&acl_cache->lock));

  if (result)
    my_error(ER_CANNOT_USER, MYF(0), "CREATE USER", wrong_users.c_ptr_safe());

  if (some_users_created)
    result |= write_bin_log(thd, FALSE, thd->query(), thd->query_length());

  rw_unlock(&LOCK_grant);
  close_thread_tables(thd);
  /* Restore the state of binlog format */
  thd->current_stmt_binlog_row_based= save_binlog_row_based;
  DBUG_RETURN(result);
}


/*
  Drop a list of users and all their privileges.

  SYNOPSIS
    mysql_drop_user()
    thd                         The current thread.
    list                        The users to drop.

  RETURN
    FALSE       OK.
    TRUE        Error.
*/

bool mysql_drop_user(THD *thd, List <LEX_USER> &list)
{
  int result;
  String wrong_users;
  LEX_USER *user_name, *tmp_user_name;
  List_iterator <LEX_USER> user_list(list);
  TABLE_LIST tables[GRANT_TABLES];
  bool some_users_deleted= FALSE;
  ulong old_sql_mode= thd->variables.sql_mode;
  bool save_binlog_row_based;
  DBUG_ENTER("mysql_drop_user");

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  save_binlog_row_based= thd->current_stmt_binlog_row_based;
  thd->clear_current_stmt_binlog_row_based();

  /* DROP USER may be skipped on replication client. */
  if ((result= open_grant_tables(thd, tables)))
  {
    /* Restore the state of binlog format */
    thd->current_stmt_binlog_row_based= save_binlog_row_based;
    DBUG_RETURN(result != 1);
  }

  thd->variables.sql_mode&= ~MODE_PAD_CHAR_TO_FULL_LENGTH;

  rw_wrlock(&LOCK_grant);
  VOID(pthread_mutex_lock(&acl_cache->lock));

  while ((tmp_user_name= user_list++))
  {
    if (!(user_name= get_current_user(thd, tmp_user_name)))
    {
      result= TRUE;
      continue;
    }  
    if (handle_grant_data(tables, 1, user_name, NULL) <= 0)
    {
      append_user(&wrong_users, user_name);
      result= TRUE;
      continue;
    }
    some_users_deleted= TRUE;
  }

  /* Rebuild 'acl_check_hosts' since 'acl_users' has been modified */
  rebuild_check_host();

  VOID(pthread_mutex_unlock(&acl_cache->lock));

  if (result)
    my_error(ER_CANNOT_USER, MYF(0), "DROP USER", wrong_users.c_ptr_safe());

  if (some_users_deleted)
    result |= write_bin_log(thd, FALSE, thd->query(), thd->query_length());

  rw_unlock(&LOCK_grant);
  close_thread_tables(thd);
  thd->variables.sql_mode= old_sql_mode;
  /* Restore the state of binlog format */
  thd->current_stmt_binlog_row_based= save_binlog_row_based;
  DBUG_RETURN(result);
}


/*
  Rename a user.

  SYNOPSIS
    mysql_rename_user()
    thd                         The current thread.
    list                        The user name pairs: (from, to).

  RETURN
    FALSE       OK.
    TRUE        Error.
*/

bool mysql_rename_user(THD *thd, List <LEX_USER> &list)
{
  int result;
  String wrong_users;
  LEX_USER *user_from, *tmp_user_from;
  LEX_USER *user_to, *tmp_user_to;
  List_iterator <LEX_USER> user_list(list);
  TABLE_LIST tables[GRANT_TABLES];
  bool some_users_renamed= FALSE;
  bool save_binlog_row_based;
  DBUG_ENTER("mysql_rename_user");

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  save_binlog_row_based= thd->current_stmt_binlog_row_based;
  thd->clear_current_stmt_binlog_row_based();

  /* RENAME USER may be skipped on replication client. */
  if ((result= open_grant_tables(thd, tables)))
  {
    /* Restore the state of binlog format */
    thd->current_stmt_binlog_row_based= save_binlog_row_based;
    DBUG_RETURN(result != 1);
  }

  rw_wrlock(&LOCK_grant);
  VOID(pthread_mutex_lock(&acl_cache->lock));

  while ((tmp_user_from= user_list++))
  {
    if (!(user_from= get_current_user(thd, tmp_user_from)))
    {
      result= TRUE;
      continue;
    }  
    tmp_user_to= user_list++;
    if (!(user_to= get_current_user(thd, tmp_user_to)))
    {
      result= TRUE;
      continue;
    }  
    DBUG_ASSERT(user_to != 0); /* Syntax enforces pairs of users. */

    /*
      Search all in-memory structures and grant tables
      for a mention of the new user name.
    */
    if (handle_grant_data(tables, 0, user_to, NULL) ||
        handle_grant_data(tables, 0, user_from, user_to) <= 0)
    {
      append_user(&wrong_users, user_from);
      result= TRUE;
      continue;
    }
    some_users_renamed= TRUE;
  }
  
  /* Rebuild 'acl_check_hosts' since 'acl_users' has been modified */
  rebuild_check_host();

  VOID(pthread_mutex_unlock(&acl_cache->lock));

  if (result)
    my_error(ER_CANNOT_USER, MYF(0), "RENAME USER", wrong_users.c_ptr_safe());
  
  if (some_users_renamed && mysql_bin_log.is_open())
    result |= write_bin_log(thd, FALSE, thd->query(), thd->query_length());

  rw_unlock(&LOCK_grant);
  close_thread_tables(thd);
  /* Restore the state of binlog format */
  thd->current_stmt_binlog_row_based= save_binlog_row_based;
  DBUG_RETURN(result);
}


/*
  Revoke all privileges from a list of users.

  SYNOPSIS
    mysql_revoke_all()
    thd                         The current thread.
    list                        The users to revoke all privileges from.

  RETURN
    > 0         Error. Error message already sent.
    0           OK.
    < 0         Error. Error message not yet sent.
*/

bool mysql_revoke_all(THD *thd,  List <LEX_USER> &list)
{
  uint counter, revoked, is_proc;
  int result;
  ACL_DB *acl_db;
  TABLE_LIST tables[GRANT_TABLES];
  bool save_binlog_row_based;
  DBUG_ENTER("mysql_revoke_all");

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  save_binlog_row_based= thd->current_stmt_binlog_row_based;
  thd->clear_current_stmt_binlog_row_based();

  if ((result= open_grant_tables(thd, tables)))
  {
    /* Restore the state of binlog format */
    thd->current_stmt_binlog_row_based= save_binlog_row_based;
    DBUG_RETURN(result != 1);
  }

  rw_wrlock(&LOCK_grant);
  VOID(pthread_mutex_lock(&acl_cache->lock));

  LEX_USER *lex_user, *tmp_lex_user;
  List_iterator <LEX_USER> user_list(list);
  while ((tmp_lex_user= user_list++))
  {
    if (!(lex_user= get_current_user(thd, tmp_lex_user)))
    {
      result= -1;
      continue;
    }  
    if (!find_acl_user(lex_user->host.str, lex_user->user.str, TRUE))
    {
      result= -1;
      continue;
    }

    if (replace_user_table(thd, tables[0].table,
			   *lex_user, ~(ulong)0, 1, 0, 0))
    {
      result= -1;
      continue;
    }

    /* Remove db access privileges */
    /*
      Because acl_dbs and column_priv_hash shrink and may re-order
      as privileges are removed, removal occurs in a repeated loop
      until no more privileges are revoked.
     */
    do
    {
      for (counter= 0, revoked= 0 ; counter < acl_dbs.elements ; )
      {
	const char *user,*host;

	acl_db=dynamic_element(&acl_dbs,counter,ACL_DB*);
	if (!(user=acl_db->user))
	  user= "";
	if (!(host=acl_db->host.hostname))
	  host= "";

	if (!strcmp(lex_user->user.str,user) &&
            !strcmp(lex_user->host.str, host))
	{
	  if (!replace_db_table(tables[1].table, acl_db->db, *lex_user,
                                ~(ulong)0, 1))
	  {
	    /*
	      Don't increment counter as replace_db_table deleted the
	      current element in acl_dbs.
	     */
	    revoked= 1;
	    continue;
	  }
	  result= -1; // Something went wrong
	}
	counter++;
      }
    } while (revoked);

    /* Remove column access */
    do
    {
      for (counter= 0, revoked= 0 ; counter < column_priv_hash.records ; )
      {
	const char *user,*host;
	GRANT_TABLE *grant_table= (GRANT_TABLE*)hash_element(&column_priv_hash,
							     counter);
	if (!(user=grant_table->user))
	  user= "";
	if (!(host=grant_table->host.hostname))
	  host= "";

	if (!strcmp(lex_user->user.str,user) &&
            !strcmp(lex_user->host.str, host))
	{
	  if (replace_table_table(thd,grant_table,tables[2].table,*lex_user,
				  grant_table->db,
				  grant_table->tname,
				  ~(ulong)0, 0, 1))
	  {
	    result= -1;
	  }
	  else
	  {
	    if (!grant_table->cols)
	    {
	      revoked= 1;
	      continue;
	    }
	    List<LEX_COLUMN> columns;
	    if (!replace_column_table(grant_table,tables[3].table, *lex_user,
				      columns,
				      grant_table->db,
				      grant_table->tname,
				      ~(ulong)0, 1))
	    {
	      revoked= 1;
	      continue;
	    }
	    result= -1;
	  }
	}
	counter++;
      }
    } while (revoked);

    /* Remove procedure access */
    for (is_proc=0; is_proc<2; is_proc++) do {
      HASH *hash= is_proc ? &proc_priv_hash : &func_priv_hash;
      for (counter= 0, revoked= 0 ; counter < hash->records ; )
      {
	const char *user,*host;
	GRANT_NAME *grant_proc= (GRANT_NAME*) hash_element(hash, counter);
	if (!(user=grant_proc->user))
	  user= "";
	if (!(host=grant_proc->host.hostname))
	  host= "";

	if (!strcmp(lex_user->user.str,user) &&
            !strcmp(lex_user->host.str, host))
	{
	  if (replace_routine_table(thd,grant_proc,tables[4].table,*lex_user,
				  grant_proc->db,
				  grant_proc->tname,
                                  is_proc,
				  ~(ulong)0, 1) == 0)
	  {
	    revoked= 1;
	    continue;
	  }
	  result= -1;	// Something went wrong
	}
	counter++;
      }
    } while (revoked);
  }

  VOID(pthread_mutex_unlock(&acl_cache->lock));

  if (result)
    my_message(ER_REVOKE_GRANTS, ER(ER_REVOKE_GRANTS), MYF(0));

  result= result |
    write_bin_log(thd, FALSE, thd->query(), thd->query_length());

  rw_unlock(&LOCK_grant);
  close_thread_tables(thd);

  /* Restore the state of binlog format */
  thd->current_stmt_binlog_row_based= save_binlog_row_based;

  DBUG_RETURN(result);
}




/**
  If the defining user for a routine does not exist, then the ACL lookup
  code should raise two errors which we should intercept.  We convert the more
  descriptive error into a warning, and consume the other.

  If any other errors are raised, then we set a flag that should indicate
  that there was some failure we should complain at a higher level.
*/
class Silence_routine_definer_errors : public Internal_error_handler
{
public:
  Silence_routine_definer_errors()
    : is_grave(FALSE)
  {}

  virtual ~Silence_routine_definer_errors()
  {}

  virtual bool handle_error(uint sql_errno, const char *message,
                            MYSQL_ERROR::enum_warning_level level,
                            THD *thd);

  bool has_errors() { return is_grave; }

private:
  bool is_grave;
};

bool
Silence_routine_definer_errors::handle_error(uint sql_errno,
                                       const char *message,
                                       MYSQL_ERROR::enum_warning_level level,
                                       THD *thd)
{
  if (level == MYSQL_ERROR::WARN_LEVEL_ERROR)
  {
    switch (sql_errno)
    {
      case ER_NONEXISTING_PROC_GRANT:
        /* Convert the error into a warning. */
        push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, sql_errno, message);
        return TRUE;
      default:
        is_grave= TRUE;
    }
  }

  return FALSE;
}


/**
  Revoke privileges for all users on a stored procedure.  Use an error handler
  that converts errors about missing grants into warnings.

  @param
    thd                         The current thread.
  @param
    db				DB of the stored procedure
  @param
    name			Name of the stored procedure

  @retval
    0           OK.
  @retval
    < 0         Error. Error message not yet sent.
*/

bool sp_revoke_privileges(THD *thd, const char *sp_db, const char *sp_name,
                          bool is_proc)
{
  uint counter, revoked;
  int result;
  TABLE_LIST tables[GRANT_TABLES];
  HASH *hash= is_proc ? &proc_priv_hash : &func_priv_hash;
  Silence_routine_definer_errors error_handler;
  bool save_binlog_row_based;
  DBUG_ENTER("sp_revoke_privileges");

  if ((result= open_grant_tables(thd, tables)))
    DBUG_RETURN(result != 1);

  /* Be sure to pop this before exiting this scope! */
  thd->push_internal_handler(&error_handler);

  rw_wrlock(&LOCK_grant);
  VOID(pthread_mutex_lock(&acl_cache->lock));

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  save_binlog_row_based= thd->current_stmt_binlog_row_based;
  thd->clear_current_stmt_binlog_row_based();

  /* Remove procedure access */
  do
  {
    for (counter= 0, revoked= 0 ; counter < hash->records ; )
    {
      GRANT_NAME *grant_proc= (GRANT_NAME*) hash_element(hash, counter);
      if (!my_strcasecmp(&my_charset_utf8_bin, grant_proc->db, sp_db) &&
	  !my_strcasecmp(system_charset_info, grant_proc->tname, sp_name))
      {
        LEX_USER lex_user;
	lex_user.user.str= grant_proc->user;
	lex_user.user.length= strlen(grant_proc->user);
	lex_user.host.str= grant_proc->host.hostname ?
	  grant_proc->host.hostname : (char*)"";
	lex_user.host.length= grant_proc->host.hostname ?
	  strlen(grant_proc->host.hostname) : 0;

	if (replace_routine_table(thd,grant_proc,tables[4].table,lex_user,
				  grant_proc->db, grant_proc->tname,
                                  is_proc, ~(ulong)0, 1) == 0)
	{
	  revoked= 1;
	  continue;
	}
      }
      counter++;
    }
  } while (revoked);

  VOID(pthread_mutex_unlock(&acl_cache->lock));
  rw_unlock(&LOCK_grant);
  close_thread_tables(thd);

  thd->pop_internal_handler();
  /* Restore the state of binlog format */
  thd->current_stmt_binlog_row_based= save_binlog_row_based;

  DBUG_RETURN(error_handler.has_errors());
}


/**
  Grant EXECUTE,ALTER privilege for a stored procedure

  @param thd The current thread.
  @param sp_db
  @param sp_name
  @param is_proc

  @return
    @retval FALSE Success
    @retval TRUE An error occured. Error message not yet sent.
*/

bool sp_grant_privileges(THD *thd, const char *sp_db, const char *sp_name,
                         bool is_proc)
{
  Security_context *sctx= thd->security_ctx;
  LEX_USER *combo;
  TABLE_LIST tables[1];
  List<LEX_USER> user_list;
  bool result;
  ACL_USER *au;
  char passwd_buff[SCRAMBLED_PASSWORD_CHAR_LENGTH+1];
  Dummy_error_handler error_handler;
  DBUG_ENTER("sp_grant_privileges");

  if (!(combo=(LEX_USER*) thd->alloc(sizeof(st_lex_user))))
    DBUG_RETURN(TRUE);

  combo->user.str= sctx->user;

  VOID(pthread_mutex_lock(&acl_cache->lock));

  if ((au= find_acl_user(combo->host.str=(char*)sctx->host_or_ip,combo->user.str,FALSE)))
    goto found_acl;
  if ((au= find_acl_user(combo->host.str=(char*)sctx->host, combo->user.str,FALSE)))
    goto found_acl;
  if ((au= find_acl_user(combo->host.str=(char*)sctx->ip, combo->user.str,FALSE)))
    goto found_acl;
  if((au= find_acl_user(combo->host.str=(char*)"%", combo->user.str, FALSE)))
    goto found_acl;

  VOID(pthread_mutex_unlock(&acl_cache->lock));
  DBUG_RETURN(TRUE);

 found_acl:
  VOID(pthread_mutex_unlock(&acl_cache->lock));

  bzero((char*)tables, sizeof(TABLE_LIST));
  user_list.empty();

  tables->db= (char*)sp_db;
  tables->table_name= tables->alias= (char*)sp_name;

  combo->host.length= strlen(combo->host.str);
  combo->user.length= strlen(combo->user.str);
  combo->host.str= thd->strmake(combo->host.str,combo->host.length);
  combo->user.str= thd->strmake(combo->user.str,combo->user.length);


  if(au && au->salt_len)
  {
    if (au->salt_len == SCRAMBLE_LENGTH)
    {
      make_password_from_salt(passwd_buff, au->salt);
      combo->password.length= SCRAMBLED_PASSWORD_CHAR_LENGTH;
    }
    else if (au->salt_len == SCRAMBLE_LENGTH_323)
    {
      make_password_from_salt_323(passwd_buff, (ulong *) au->salt);
      combo->password.length= SCRAMBLED_PASSWORD_CHAR_LENGTH_323;
    }
    else
    {
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                          ER_PASSWD_LENGTH,
                          ER(ER_PASSWD_LENGTH),
                          SCRAMBLED_PASSWORD_CHAR_LENGTH);
      return TRUE;
    }
    combo->password.str= passwd_buff;
  }
  else
  {
    combo->password.str= (char*)"";
    combo->password.length= 0;
  }

  if (user_list.push_back(combo))
    DBUG_RETURN(TRUE);

  thd->lex->ssl_type= SSL_TYPE_NOT_SPECIFIED;
  thd->lex->ssl_cipher= thd->lex->x509_subject= thd->lex->x509_issuer= 0;
  bzero((char*) &thd->lex->mqh, sizeof(thd->lex->mqh));

  /*
    Only care about whether the operation failed or succeeded
    as all errors will be handled later.
  */
  thd->push_internal_handler(&error_handler);
  result= mysql_routine_grant(thd, tables, is_proc, user_list,
                              DEFAULT_CREATE_PROC_ACLS, FALSE, FALSE);
  thd->pop_internal_handler();
  DBUG_RETURN(result);
}


/*****************************************************************************
  Instantiate used templates
*****************************************************************************/

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class List_iterator<LEX_COLUMN>;
template class List_iterator<LEX_USER>;
template class List<LEX_COLUMN>;
template class List<LEX_USER>;
#endif

#endif /*NO_EMBEDDED_ACCESS_CHECKS */


int wild_case_compare(CHARSET_INFO *cs, const char *str,const char *wildstr)
{
  reg3 int flag;
  DBUG_ENTER("wild_case_compare");
  DBUG_PRINT("enter",("str: '%s'  wildstr: '%s'",str,wildstr));
  while (*wildstr)
  {
    while (*wildstr && *wildstr != wild_many && *wildstr != wild_one)
    {
      if (*wildstr == wild_prefix && wildstr[1])
	wildstr++;
      if (my_toupper(cs, *wildstr++) !=
          my_toupper(cs, *str++)) DBUG_RETURN(1);
    }
    if (! *wildstr ) DBUG_RETURN (*str != 0);
    if (*wildstr++ == wild_one)
    {
      if (! *str++) DBUG_RETURN (1);	/* One char; skip */
    }
    else
    {						/* Found '*' */
      if (!*wildstr) DBUG_RETURN(0);		/* '*' as last char: OK */
      flag=(*wildstr != wild_many && *wildstr != wild_one);
      do
      {
	if (flag)
	{
	  char cmp;
	  if ((cmp= *wildstr) == wild_prefix && wildstr[1])
	    cmp=wildstr[1];
	  cmp=my_toupper(cs, cmp);
	  while (*str && my_toupper(cs, *str) != cmp)
	    str++;
	  if (!*str) DBUG_RETURN (1);
	}
	if (wild_case_compare(cs, str,wildstr) == 0) DBUG_RETURN (0);
      } while (*str++);
      DBUG_RETURN(1);
    }
  }
  DBUG_RETURN (*str != '\0');
}


#ifndef NO_EMBEDDED_ACCESS_CHECKS
static bool update_schema_privilege(THD *thd, TABLE *table, char *buff,
                                    const char* db, const char* t_name,
                                    const char* column, uint col_length,
                                    const char *priv, uint priv_length,
                                    const char* is_grantable)
{
  int i= 2;
  CHARSET_INFO *cs= system_charset_info;
  restore_record(table, s->default_values);
  table->field[0]->store(buff, (uint) strlen(buff), cs);
  if (db)
    table->field[i++]->store(db, (uint) strlen(db), cs);
  if (t_name)
    table->field[i++]->store(t_name, (uint) strlen(t_name), cs);
  if (column)
    table->field[i++]->store(column, col_length, cs);
  table->field[i++]->store(priv, priv_length, cs);
  table->field[i]->store(is_grantable, strlen(is_grantable), cs);
  return schema_table_store_record(thd, table);
}
#endif


int fill_schema_user_privileges(THD *thd, TABLE_LIST *tables, COND *cond)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  int error= 0;
  uint counter;
  ACL_USER *acl_user;
  ulong want_access;
  char buff[100];
  TABLE *table= tables->table;
  bool no_global_access= check_access(thd, SELECT_ACL, "mysql",0,1,1,0);
  char *curr_host= thd->security_ctx->priv_host_name();
  DBUG_ENTER("fill_schema_user_privileges");

  if (!initialized)
    DBUG_RETURN(0);
  pthread_mutex_lock(&acl_cache->lock);

  for (counter=0 ; counter < acl_users.elements ; counter++)
  {
    const char *user,*host, *is_grantable="YES";
    acl_user=dynamic_element(&acl_users,counter,ACL_USER*);
    if (!(user=acl_user->user))
      user= "";
    if (!(host=acl_user->host.hostname))
      host= "";

    if (no_global_access &&
        (strcmp(thd->security_ctx->priv_user, user) ||
         my_strcasecmp(system_charset_info, curr_host, host)))
      continue;
      
    want_access= acl_user->access;
    if (!(want_access & GRANT_ACL))
      is_grantable= "NO";

    strxmov(buff,"'",user,"'@'",host,"'",NullS);
    if (!(want_access & ~GRANT_ACL))
    {
      if (update_schema_privilege(thd, table, buff, 0, 0, 0, 0,
                                  STRING_WITH_LEN("USAGE"), is_grantable))
      {
        error= 1;
        goto err;
      }
    }
    else
    {
      uint priv_id;
      ulong j,test_access= want_access & ~GRANT_ACL;
      for (priv_id=0, j = SELECT_ACL;j <= GLOBAL_ACLS; priv_id++,j <<= 1)
      {
	if (test_access & j)
        {
          if (update_schema_privilege(thd, table, buff, 0, 0, 0, 0, 
                                      command_array[priv_id],
                                      command_lengths[priv_id], is_grantable))
          {
            error= 1;
            goto err;
          }
        }
      }
    }
  }
err:
  pthread_mutex_unlock(&acl_cache->lock);

  DBUG_RETURN(error);
#else
  return(0);
#endif
}


int fill_schema_schema_privileges(THD *thd, TABLE_LIST *tables, COND *cond)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  int error= 0;
  uint counter;
  ACL_DB *acl_db;
  ulong want_access;
  char buff[100];
  TABLE *table= tables->table;
  bool no_global_access= check_access(thd, SELECT_ACL, "mysql",0,1,1,0);
  char *curr_host= thd->security_ctx->priv_host_name();
  DBUG_ENTER("fill_schema_schema_privileges");

  if (!initialized)
    DBUG_RETURN(0);
  pthread_mutex_lock(&acl_cache->lock);

  for (counter=0 ; counter < acl_dbs.elements ; counter++)
  {
    const char *user, *host, *is_grantable="YES";

    acl_db=dynamic_element(&acl_dbs,counter,ACL_DB*);
    if (!(user=acl_db->user))
      user= "";
    if (!(host=acl_db->host.hostname))
      host= "";

    if (no_global_access &&
        (strcmp(thd->security_ctx->priv_user, user) ||
         my_strcasecmp(system_charset_info, curr_host, host)))
      continue;

    want_access=acl_db->access;
    if (want_access)
    {
      if (!(want_access & GRANT_ACL))
      {
        is_grantable= "NO";
      }
      strxmov(buff,"'",user,"'@'",host,"'",NullS);
      if (!(want_access & ~GRANT_ACL))
      {
        if (update_schema_privilege(thd, table, buff, acl_db->db, 0, 0,
                                    0, STRING_WITH_LEN("USAGE"), is_grantable))
        {
          error= 1;
          goto err;
        }
      }
      else
      {
        int cnt;
        ulong j,test_access= want_access & ~GRANT_ACL;
        for (cnt=0, j = SELECT_ACL; j <= DB_ACLS; cnt++,j <<= 1)
          if (test_access & j)
          {
            if (update_schema_privilege(thd, table, buff, acl_db->db, 0, 0, 0,
                                        command_array[cnt], command_lengths[cnt],
                                        is_grantable))
            {
              error= 1;
              goto err;
            }
          }
      }
    }
  }
err:
  pthread_mutex_unlock(&acl_cache->lock);

  DBUG_RETURN(error);
#else
  return (0);
#endif
}


int fill_schema_table_privileges(THD *thd, TABLE_LIST *tables, COND *cond)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  int error= 0;
  uint index;
  char buff[100];
  TABLE *table= tables->table;
  bool no_global_access= check_access(thd, SELECT_ACL, "mysql",0,1,1,0);
  char *curr_host= thd->security_ctx->priv_host_name();
  DBUG_ENTER("fill_schema_table_privileges");

  rw_rdlock(&LOCK_grant);

  for (index=0 ; index < column_priv_hash.records ; index++)
  {
    const char *user, *host, *is_grantable= "YES";
    GRANT_TABLE *grant_table= (GRANT_TABLE*) hash_element(&column_priv_hash,
							  index);
    if (!(user=grant_table->user))
      user= "";
    if (!(host= grant_table->host.hostname))
      host= "";

    if (no_global_access &&
        (strcmp(thd->security_ctx->priv_user, user) ||
         my_strcasecmp(system_charset_info, curr_host, host)))
      continue;

    ulong table_access= grant_table->privs;
    if (table_access)
    {
      ulong test_access= table_access & ~GRANT_ACL;
      /*
        We should skip 'usage' privilege on table if
        we have any privileges on column(s) of this table
      */
      if (!test_access && grant_table->cols)
        continue;
      if (!(table_access & GRANT_ACL))
        is_grantable= "NO";

      strxmov(buff, "'", user, "'@'", host, "'", NullS);
      if (!test_access)
      {
        if (update_schema_privilege(thd, table, buff, grant_table->db,
                                    grant_table->tname, 0, 0,
                                    STRING_WITH_LEN("USAGE"), is_grantable))
        {
          error= 1;
          goto err;
        }
      }
      else
      {
        ulong j;
        int cnt;
        for (cnt= 0, j= SELECT_ACL; j <= TABLE_ACLS; cnt++, j<<= 1)
        {
          if (test_access & j)
          {
            if (update_schema_privilege(thd, table, buff, grant_table->db,
                                        grant_table->tname, 0, 0,
                                        command_array[cnt],
                                        command_lengths[cnt], is_grantable))
            {
              error= 1;
              goto err;
            }
          }
        }
      }
    }   
  }
err:
  rw_unlock(&LOCK_grant);

  DBUG_RETURN(error);
#else
  return (0);
#endif
}


int fill_schema_column_privileges(THD *thd, TABLE_LIST *tables, COND *cond)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  int error= 0;
  uint index;
  char buff[100];
  TABLE *table= tables->table;
  bool no_global_access= check_access(thd, SELECT_ACL, "mysql",0,1,1,0);
  char *curr_host= thd->security_ctx->priv_host_name();
  DBUG_ENTER("fill_schema_table_privileges");

  rw_rdlock(&LOCK_grant);

  for (index=0 ; index < column_priv_hash.records ; index++)
  {
    const char *user, *host, *is_grantable= "YES";
    GRANT_TABLE *grant_table= (GRANT_TABLE*) hash_element(&column_priv_hash,
							  index);
    if (!(user=grant_table->user))
      user= "";
    if (!(host= grant_table->host.hostname))
      host= "";

    if (no_global_access &&
        (strcmp(thd->security_ctx->priv_user, user) ||
         my_strcasecmp(system_charset_info, curr_host, host)))
      continue;

    ulong table_access= grant_table->cols;
    if (table_access != 0)
    {
      if (!(grant_table->privs & GRANT_ACL))
        is_grantable= "NO";

      ulong test_access= table_access & ~GRANT_ACL;
      strxmov(buff, "'", user, "'@'", host, "'", NullS);
      if (!test_access)
        continue;
      else
      {
        ulong j;
        int cnt;
        for (cnt= 0, j= SELECT_ACL; j <= TABLE_ACLS; cnt++, j<<= 1)
        {
          if (test_access & j)
          {
            for (uint col_index=0 ;
                 col_index < grant_table->hash_columns.records ;
                 col_index++)
            {
              GRANT_COLUMN *grant_column = (GRANT_COLUMN*)
                hash_element(&grant_table->hash_columns,col_index);
              if ((grant_column->rights & j) && (table_access & j))
              {
                if (update_schema_privilege(thd, table, buff, grant_table->db,
                                            grant_table->tname,
                                            grant_column->column,
                                            grant_column->key_length,
                                            command_array[cnt],
                                            command_lengths[cnt], is_grantable))
                {
                  error= 1;
                  goto err;
                }
              }
            }
          }
        }
      }
    }
  }
err:
  rw_unlock(&LOCK_grant);

  DBUG_RETURN(error);
#else
  return (0);
#endif
}


#ifndef NO_EMBEDDED_ACCESS_CHECKS
/*
  fill effective privileges for table

  SYNOPSIS
    fill_effective_table_privileges()
    thd     thread handler
    grant   grants table descriptor
    db      db name
    table   table name
*/

void fill_effective_table_privileges(THD *thd, GRANT_INFO *grant,
                                     const char *db, const char *table)
{
  Security_context *sctx= thd->security_ctx;
  DBUG_ENTER("fill_effective_table_privileges");
  DBUG_PRINT("enter", ("Host: '%s', Ip: '%s', User: '%s', table: `%s`.`%s`",
                       sctx->priv_host, (sctx->ip ? sctx->ip : "(NULL)"),
                       (sctx->priv_user ? sctx->priv_user : "(NULL)"),
                       db, table));
  /* --skip-grants */
  if (!initialized)
  {
    DBUG_PRINT("info", ("skip grants"));
    grant->privilege= ~NO_ACCESS;             // everything is allowed
    DBUG_PRINT("info", ("privilege 0x%lx", grant->privilege));
    DBUG_VOID_RETURN;
  }

  /* global privileges */
  grant->privilege= sctx->master_access;

  if (!sctx->priv_user)
  {
    DBUG_PRINT("info", ("privilege 0x%lx", grant->privilege));
    DBUG_VOID_RETURN;                         // it is slave
  }

  /* db privileges */
  grant->privilege|= acl_get(sctx->host, sctx->ip, sctx->priv_user, db, 0);

  /* table privileges */
  rw_rdlock(&LOCK_grant);
  if (grant->version != grant_version)
  {
    grant->grant_table=
      table_hash_search(sctx->host, sctx->ip, db,
			sctx->priv_user,
			table, 0);              /* purecov: inspected */
    grant->version= grant_version;              /* purecov: inspected */
  }
  if (grant->grant_table != 0)
  {
    grant->privilege|= grant->grant_table->privs;
  }
  rw_unlock(&LOCK_grant);

  DBUG_PRINT("info", ("privilege 0x%lx", grant->privilege));
  DBUG_VOID_RETURN;
}

#else /* NO_EMBEDDED_ACCESS_CHECKS */

/****************************************************************************
 Dummy wrappers when we don't have any access checks
****************************************************************************/

bool check_routine_level_acl(THD *thd, const char *db, const char *name,
                             bool is_proc)
{
  return FALSE;
}

#endif
