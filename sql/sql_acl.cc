/* Copyright (C) 2000-2003 MySQL AB

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
#ifdef HAVE_REPLICATION
#include "sql_repl.h" //for tables_ok()
#endif
#include <m_ctype.h>
#include <stdarg.h>
#include "sp_head.h"
#include "sp.h"

#ifndef NO_EMBEDDED_ACCESS_CHECKS

class acl_entry :public hash_filo_element
{
public:
  ulong access;
  uint16 length;
  char key[1];					// Key will be stored here
};


static byte* acl_entry_get_key(acl_entry *entry,uint *length,
			       my_bool not_used __attribute__((unused)))
{
  *length=(uint) entry->length;
  return (byte*) entry->key;
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
static ACL_USER *find_acl_user(const char *host, const char *user,
                               my_bool exact);
static bool update_user_table(THD *thd, TABLE *table,
                              const char *host, const char *user,
			      const char *new_password, uint new_password_len);
static void update_hostname(acl_host_and_ip *host, const char *hostname);
static bool compare_hostname(const acl_host_and_ip *host,const char *hostname,
			     const char *ip);
static my_bool acl_load(THD *thd, TABLE_LIST *tables);
static my_bool grant_load(TABLE_LIST *tables);

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
                           (hash_free_key) free, system_charset_info);
  if (dont_read_acl_tables)
  {
    DBUG_RETURN(0); /* purecov: tested */
  }

  /*
    To be able to run this from boot, we allocate a temporary THD
  */
  if (!(thd=new THD))
    DBUG_RETURN(1); /* purecov: inspected */
  thd->store_globals();
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
  my_bool return_val= 1;
  bool check_no_resolve= specialflag & SPECIAL_NO_RESOLVE;
  char tmp_name[NAME_LEN+1];
  int password_length;
  DBUG_ENTER("acl_load");

  grant_version++; /* Privileges updated */
  mysql_proc_table_exists= 1;			// Assume mysql.proc exists

  acl_cache->clear(1);				// Clear locked hostname cache

  init_sql_alloc(&mem, ACL_ALLOC_BLOCK_SIZE, 0);
  init_read_record(&read_record_info,thd,table= tables[0].table,NULL,1,0);
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
                          host.host.hostname, host.db);
    }
    host.access= get_access(table,2);
    host.access= fix_rights_for_db(host.access);
    host.sort=	 get_sort(2,host.host.hostname,host.db);
    if (check_no_resolve && hostname_requires_resolving(host.host.hostname))
    {
      sql_print_warning("'host' entry '%s|%s' "
		      "ignored in --skip-name-resolve mode.",
		      host.host.hostname, host.db?host.db:"");
      continue;
    }
#ifndef TO_BE_REMOVED
    if (table->s->fields == 8)
    {						// Without grant
      if (host.access & CREATE_ACL)
	host.access|=REFERENCES_ACL | INDEX_ACL | ALTER_ACL | CREATE_TMP_ACL;
    }
#endif
    VOID(push_dynamic(&acl_hosts,(gptr) &host));
  }
  qsort((gptr) dynamic_element(&acl_hosts,0,ACL_HOST*),acl_hosts.elements,
	sizeof(ACL_HOST),(qsort_cmp) acl_compare);
  end_read_record(&read_record_info);
  freeze_size(&acl_hosts);

  init_read_record(&read_record_info,thd,table=tables[1].table,NULL,1,0);
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
		      user.user, user.host.hostname);
      continue;
    }

    const char *password= get_field(&mem, table->field[2]);
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

      user.sort= get_sort(2,user.host.hostname,user.user);
      user.hostname_length= (user.host.hostname ?
                             (uint) strlen(user.host.hostname) : 0);

      /* Starting from 4.0.2 we have more fields */
      if (table->s->fields >= 31)
      {
        char *ssl_type=get_field(&mem, table->field[next_field++]);
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

        char *ptr = get_field(&mem, table->field[next_field++]);
        user.user_resource.questions=ptr ? atoi(ptr) : 0;
        ptr = get_field(&mem, table->field[next_field++]);
        user.user_resource.updates=ptr ? atoi(ptr) : 0;
        ptr = get_field(&mem, table->field[next_field++]);
        user.user_resource.conn_per_hour= ptr ? atoi(ptr) : 0;
        if (user.user_resource.questions || user.user_resource.updates ||
            user.user_resource.conn_per_hour)
          mqh_used=1;

        if (table->s->fields >= 36)
        {
          /* Starting from 5.0.3 we have max_user_connections field */
          ptr= get_field(&mem, table->field[next_field++]);
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
      VOID(push_dynamic(&acl_users,(gptr) &user));
      if (!user.host.hostname || user.host.hostname[0] == wild_many &&
          !user.host.hostname[1])
        allow_all_hosts=1;			// Anyone can connect
    }
  }
  qsort((gptr) dynamic_element(&acl_users,0,ACL_USER*),acl_users.elements,
	sizeof(ACL_USER),(qsort_cmp) acl_compare);
  end_read_record(&read_record_info);
  freeze_size(&acl_users);

  init_read_record(&read_record_info,thd,table=tables[2].table,NULL,1,0);
  VOID(my_init_dynamic_array(&acl_dbs,sizeof(ACL_DB),50,100));
  while (!(read_record_info.read_record(&read_record_info)))
  {
    ACL_DB db;
    update_hostname(&db.host,get_field(&mem, table->field[0]));
    db.db=get_field(&mem, table->field[1]);
    if (!db.db)
    {
      sql_print_warning("Found an entry in the 'db' table with empty database name; Skipped");
      continue;
    }
    db.user=get_field(&mem, table->field[2]);
    if (check_no_resolve && hostname_requires_resolving(db.host.hostname))
    {
      sql_print_warning("'db' entry '%s %s@%s' "
		        "ignored in --skip-name-resolve mode.",
		        db.db, db.user, db.host.hostname);
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
		          db.db, db.user, db.host.hostname, db.host.hostname);
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
    VOID(push_dynamic(&acl_dbs,(gptr) &db));
  }
  qsort((gptr) dynamic_element(&acl_dbs,0,ACL_DB*),acl_dbs.elements,
	sizeof(ACL_DB),(qsort_cmp) acl_compare);
  end_read_record(&read_record_info);
  freeze_size(&acl_dbs);
  init_check_host();

  initialized=1;
  return_val=0;

end:
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
  my_bool return_val= 1;
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

  if (simple_open_n_lock_tables(thd, tables))
  {
    sql_print_error("Fatal error: Can't open and lock privilege tables: %s",
		    thd->net.last_error);
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
       *pos && (*pos)->real_type() == FIELD_TYPE_ENUM &&
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
	if (*str == wild_many || *str == wild_one || *str == wild_prefix)
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
	  SSL_get_peer_certificate(ssl))
	user_access= acl_user->access;
      break;
    case SSL_TYPE_SPECIFIED: /* Client should have specified attrib */
      /*
	We do not check for absence of SSL because without SSL it does
	not pass all checks here anyway.
	If cipher name is specified, we compare it to actual cipher in
	use.
      */
      X509 *cert;
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
            sql_print_information("X509 subject mismatch: '%s' vs '%s'",
                            acl_user->x509_subject, ptr);
        }
        else
          user_access= acl_user->access;
        free(ptr);
      }
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
      strmake(sctx->priv_host, acl_user->host.hostname, MAX_HOSTNAME);
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

  /*
     Find acl entry in user database.
     This is specially tailored to suit the check we do for CALL of
     a stored procedure; user is set to what is actually a
     priv_user, which can be ''.
  */
  for (i=0 ; i < acl_users.elements ; i++)
  {
    acl_user= dynamic_element(&acl_users,i,ACL_USER*);
    if ((!acl_user->user && (!user || !user[0])) ||
	(acl_user->user && strcmp(user, acl_user->user) == 0))
    {
      if (compare_hostname(&acl_user->host, host, ip))
      {
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
	  if (!acl_db->db || (db && !strcmp(acl_db->db, db)))
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
      strmake(sctx->priv_host, acl_user->host.hostname, MAX_HOSTNAME);
    else
      *sctx->priv_host= 0;
  }
  VOID(pthread_mutex_unlock(&acl_cache->lock));
  DBUG_RETURN(res);
}

static byte* check_get_key(ACL_USER *buff,uint *length,
			   my_bool not_used __attribute__((unused)))
{
  *length=buff->hostname_length;
  return (byte*) buff->host.hostname;
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
  for (uint i=0 ; i < acl_users.elements ; i++)
  {
    ACL_USER *acl_user=dynamic_element(&acl_users,i,ACL_USER*);
    if (!acl_user->user && !user[0] ||
	acl_user->user &&
	!strcmp(user,acl_user->user))
    {
      if (!acl_user->host.hostname && !host[0] ||
	  acl_user->host.hostname &&
	  !my_strcasecmp(system_charset_info, host, acl_user->host.hostname))
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

  VOID(push_dynamic(&acl_users,(gptr) &acl_user));
  if (!acl_user.host.hostname || acl_user.host.hostname[0] == wild_many
      && !acl_user.host.hostname[1])
    allow_all_hosts=1;		// Anyone can connect /* purecov: tested */
  qsort((gptr) dynamic_element(&acl_users,0,ACL_USER*),acl_users.elements,
	sizeof(ACL_USER),(qsort_cmp) acl_compare);

  /* We must free acl_check_hosts as its memory is mapped to acl_user */
  delete_dynamic(&acl_wild_hosts);
  hash_free(&acl_check_hosts);
  init_check_host();
}


static void acl_update_db(const char *user, const char *host, const char *db,
			  ulong privileges)
{
  for (uint i=0 ; i < acl_dbs.elements ; i++)
  {
    ACL_DB *acl_db=dynamic_element(&acl_dbs,i,ACL_DB*);
    if (!acl_db->user && !user[0] ||
	acl_db->user &&
	!strcmp(user,acl_db->user))
    {
      if (!acl_db->host.hostname && !host[0] ||
	  acl_db->host.hostname &&
	  !my_strcasecmp(system_charset_info, host, acl_db->host.hostname))
      {
	if (!acl_db->db && !db[0] ||
	    acl_db->db && !strcmp(db,acl_db->db))
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
  update_hostname(&acl_db.host,strdup_root(&mem,host));
  acl_db.db=strdup_root(&mem,db);
  acl_db.access=privileges;
  acl_db.sort=get_sort(3,acl_db.host.hostname,acl_db.db,acl_db.user);
  VOID(push_dynamic(&acl_dbs,(gptr) &acl_db));
  qsort((gptr) dynamic_element(&acl_dbs,0,ACL_DB*),acl_dbs.elements,
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
  uint i,key_length;
  char key[ACL_KEY_LENGTH],*tmp_db,*end;
  acl_entry *entry;
  DBUG_ENTER("acl_get");

  VOID(pthread_mutex_lock(&acl_cache->lock));
  end=strmov((tmp_db=strmov(strmov(key, ip ? ip : "")+1,user)+1),db);
  if (lower_case_table_names)
  {
    my_casedn_str(files_charset_info, tmp_db);
    db=tmp_db;
  }
  key_length=(uint) (end-key);
  if (!db_is_pattern && (entry=(acl_entry*) acl_cache->search(key,key_length)))
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
    memcpy((gptr) entry->key,key,key_length);
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
	  (void) push_dynamic(&acl_wild_hosts,(char*) &acl_user->host);
      }
      else if (!hash_search(&acl_check_hosts,(byte*) &acl_user->host,
			    (uint) strlen(acl_user->host.hostname)))
      {
	if (my_hash_insert(&acl_check_hosts,(byte*) acl_user))
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


/* Return true if there is no users that can match the given host */

bool acl_check_host(const char *host, const char *ip)
{
  if (allow_all_hosts)
    return 0;
  VOID(pthread_mutex_lock(&acl_cache->lock));

  if (host && hash_search(&acl_check_hosts,(byte*) host,(uint) strlen(host)) ||
      ip && hash_search(&acl_check_hosts,(byte*) ip,(uint) strlen(ip)))
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

bool check_change_password(THD *thd, const char *host, const char *user,
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
  uint len=strlen(new_password);
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
  uint new_password_len= strlen(new_password);
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
  if (thd->slave_thread && table_rules_on)
  {
    /*
      The tables must be marked "updating" so that tables_ok() takes them into
      account in tests.  It's ok to leave 'updating' set after tables_ok.
    */
    tables.updating= 1;
    /* Thanks to bzero, tables.next==0 */
    if (!tables_ok(thd, &tables))
      DBUG_RETURN(0);
  }
#endif

  if (!(table= open_ltable(thd, &tables, TL_WRITE)))
    DBUG_RETURN(1);

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
    query_length=
      my_sprintf(buff,
                 (buff,"SET PASSWORD FOR \"%-.120s\"@\"%-.120s\"=\"%-.120s\"",
                  acl_user->user ? acl_user->user : "",
                  acl_user->host.hostname ? acl_user->host.hostname : "",
                  new_password));
    thd->clear_error();
    Query_log_event qinfo(thd, buff, query_length, 0, FALSE);
    mysql_bin_log.write(&qinfo);
  }
end:
  close_thread_tables(thd);
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
  for (uint i=0 ; i < acl_users.elements ; i++)
  {
    ACL_USER *acl_user=dynamic_element(&acl_users,i,ACL_USER*);
    DBUG_PRINT("info",("strcmp('%s','%s'), compare_hostname('%s','%s'),",
		       user,
		       acl_user->user ? acl_user->user : "",
		       host,
		       acl_user->host.hostname ? acl_user->host.hostname :
		       ""));
    if (!acl_user->user && !user[0] ||
	acl_user->user && !strcmp(user,acl_user->user))
    {
      if (exact ? !my_strcasecmp(&my_charset_latin1, host,
                                 acl_user->host.hostname) :
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
  host->hostname=(char*) hostname;		// This will not be modified!
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
                                          hostname,host->hostname)) ||
	  (ip && !wild_compare(ip,host->hostname,0)));
}

bool hostname_requires_resolving(const char *hostname)
{
  char cur;
  if (!hostname)
    return FALSE;
  int namelen= strlen(hostname);
  int lhlen= strlen(my_localhost);
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

  table->field[0]->store(host,(uint) strlen(host), system_charset_info);
  table->field[1]->store(user,(uint) strlen(user), system_charset_info);
  key_copy((byte *) user_key, table->record[0], table->key_info,
           table->key_info->key_length);

  table->file->extra(HA_EXTRA_RETRIEVE_ALL_COLS);
  if (table->file->index_read_idx(table->record[0], 0,
				  (byte *) user_key, table->key_info->key_length,
				  HA_READ_KEY_EXACT))
  {
    my_message(ER_PASSWORD_NO_MATCH, ER(ER_PASSWORD_NO_MATCH),
               MYF(0));	/* purecov: deadcode */
    DBUG_RETURN(1);				/* purecov: deadcode */
  }
  store_record(table,record[1]);
  table->field[2]->store(new_password, new_password_len, system_charset_info);
  if ((error=table->file->update_row(table->record[1],table->record[0])))
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
  byte user_key[MAX_KEY_LENGTH];
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

  table->field[0]->store(combo.host.str,combo.host.length, system_charset_info);
  table->field[1]->store(combo.user.str,combo.user.length, system_charset_info);
  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);

  table->file->extra(HA_EXTRA_RETRIEVE_ALL_COLS);
  if (table->file->index_read_idx(table->record[0], 0,
                                  user_key, table->key_info->key_length,
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
      my_error(ER_PASSWORD_NO_MATCH, MYF(0), combo.user.str, combo.host.str);
      goto end;
    }
    else if (!can_create_user)
    {
      my_error(ER_CANT_CREATE_USER_WITH_GRANT, MYF(0),
               thd->security_ctx->user, thd->security_ctx->host_or_ip);
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
       *tmp_field && (*tmp_field)->real_type() == FIELD_TYPE_ENUM &&
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
      table->field[next_field]->store("ANY", 3, &my_charset_latin1);
      table->field[next_field+1]->store("", 0, &my_charset_latin1);
      table->field[next_field+2]->store("", 0, &my_charset_latin1);
      table->field[next_field+3]->store("", 0, &my_charset_latin1);
      break;
    case SSL_TYPE_X509:
      table->field[next_field]->store("X509", 4, &my_charset_latin1);
      table->field[next_field+1]->store("", 0, &my_charset_latin1);
      table->field[next_field+2]->store("", 0, &my_charset_latin1);
      table->field[next_field+3]->store("", 0, &my_charset_latin1);
      break;
    case SSL_TYPE_SPECIFIED:
      table->field[next_field]->store("SPECIFIED", 9, &my_charset_latin1);
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
      table->field[next_field+3]->store((longlong) mqh.user_conn);
    mqh_used= mqh_used || mqh.questions || mqh.updates || mqh.conn_per_hour;
  }
  if (old_row_exists)
  {
    /*
      We should NEVER delete from the user table, as a uses can still
      use mysqld even if he doesn't have any privileges in the user table!
    */
    table->file->extra(HA_EXTRA_RETRIEVE_ALL_COLS);
    if (cmp_record(table,record[1]) &&
	(error=table->file->update_row(table->record[1],table->record[0])))
    {						// This should never happen
      table->file->print_error(error,MYF(0));	/* purecov: deadcode */
      error= -1;				/* purecov: deadcode */
      goto end;					/* purecov: deadcode */
    }
  }
  else if ((error=table->file->write_row(table->record[0]))) // insert
  {						// This should never happen
    if (error && error != HA_ERR_FOUND_DUPP_KEY &&
	error != HA_ERR_FOUND_DUPP_UNIQUE)	/* purecov: inspected */
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
  byte user_key[MAX_KEY_LENGTH];
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

  table->field[0]->store(combo.host.str,combo.host.length, system_charset_info);
  table->field[1]->store(db,(uint) strlen(db), system_charset_info);
  table->field[2]->store(combo.user.str,combo.user.length, system_charset_info);
  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);

  table->file->extra(HA_EXTRA_RETRIEVE_ALL_COLS);
  if (table->file->index_read_idx(table->record[0],0,
                                  user_key, table->key_info->key_length,
                                  HA_READ_KEY_EXACT))
  {
    if (what == 'N')
    { // no row, no revoke
      my_error(ER_NONEXISTING_GRANT, MYF(0), combo.user.str, combo.host.str);
      goto abort;
    }
    old_row_exists = 0;
    restore_record(table, s->default_values);
    table->field[0]->store(combo.host.str,combo.host.length, system_charset_info);
    table->field[1]->store(db,(uint) strlen(db), system_charset_info);
    table->field[2]->store(combo.user.str,combo.user.length, system_charset_info);
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
      table->file->extra(HA_EXTRA_RETRIEVE_ALL_COLS);
      if ((error=table->file->update_row(table->record[1],table->record[0])))
	goto table_error;			/* purecov: deadcode */
    }
    else	/* must have been a revoke of all privileges */
    {
      if ((error = table->file->delete_row(table->record[1])))
	goto table_error;			/* purecov: deadcode */
    }
  }
  else if (rights && (error=table->file->write_row(table->record[0])))
  {
    if (error && error != HA_ERR_FOUND_DUPP_KEY) /* purecov: inspected */
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
    column= memdup_root(&memex,c.ptr(), key_length=c.length());
  }
};


static byte* get_key_column(GRANT_COLUMN *buff,uint *length,
			    my_bool not_used __attribute__((unused)))
{
  *length=buff->key_length;
  return (byte*) buff->column;
}


class GRANT_NAME :public Sql_alloc
{
public:
  acl_host_and_ip host;
  char *db, *user, *tname, *hash_key;
  ulong privs;
  ulong sort;
  uint key_length;
  GRANT_NAME(const char *h, const char *d,const char *u,
             const char *t, ulong p);
  GRANT_NAME (TABLE *form);
  virtual ~GRANT_NAME() {};
  virtual bool ok() { return privs != 0; }
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



GRANT_NAME::GRANT_NAME(const char *h, const char *d,const char *u,
                       const char *t, ulong p)
  :privs(p)
{
  /* Host given by user */
  update_hostname(&host, strdup_root(&memex, h));
  db =   strdup_root(&memex,d);
  user = strdup_root(&memex,u);
  sort=  get_sort(3,host.hostname,db,user);
  tname= strdup_root(&memex,t);
  if (lower_case_table_names)
  {
    my_casedn_str(files_charset_info, db);
    my_casedn_str(files_charset_info, tname);
  }
  key_length =(uint) strlen(d)+(uint) strlen(u)+(uint) strlen(t)+3;
  hash_key = (char*) alloc_root(&memex,key_length);
  strmov(strmov(strmov(hash_key,user)+1,db)+1,tname);
}


GRANT_TABLE::GRANT_TABLE(const char *h, const char *d,const char *u,
                	 const char *t, ulong p, ulong c)
  :GRANT_NAME(h,d,u,t,p), cols(c)
{
  (void) hash_init(&hash_columns,system_charset_info,
                   0,0,0, (hash_get_key) get_key_column,0,0);
}


GRANT_NAME::GRANT_NAME(TABLE *form)
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
    my_casedn_str(files_charset_info, tname);
  }
  key_length = ((uint) strlen(db) + (uint) strlen(user) +
                (uint) strlen(tname) + 3);
  hash_key = (char*) alloc_root(&memex,key_length);
  strmov(strmov(strmov(hash_key,user)+1,db)+1,tname);
  privs = (ulong) form->field[6]->val_int();
  privs = fix_rights_for_table(privs);
}


GRANT_TABLE::GRANT_TABLE(TABLE *form, TABLE *col_privs)
  :GRANT_NAME(form)
{
  byte key[MAX_KEY_LENGTH];

  if (!db || !tname)
  {
    /* Wrong table row; Ignore it */
    hash_clear(&hash_columns);                  /* allow for destruction */
    cols= 0;
    return;
  }
  cols= (ulong) form->field[7]->val_int();
  cols =  fix_rights_for_column(cols);

  (void) hash_init(&hash_columns,system_charset_info,
                   0,0,0, (hash_get_key) get_key_column,0,0);
  if (cols)
  {
    uint key_prefix_len;
    KEY_PART_INFO *key_part= col_privs->key_info->key_part;
    col_privs->field[0]->store(host.hostname,
                               host.hostname ? (uint) strlen(host.hostname) : 0,
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

    col_privs->file->ha_index_init(0);
    if (col_privs->file->index_read(col_privs->record[0],
                                    (byte*) key,
                                    key_prefix_len, HA_READ_KEY_EXACT))
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
      my_hash_insert(&hash_columns, (byte *) mem_check);
    } while (!col_privs->file->index_next(col_privs->record[0]) &&
             !key_cmp_if_same(col_privs,key,0,key_prefix_len));
    col_privs->file->ha_index_end();
  }
}


GRANT_TABLE::~GRANT_TABLE()
{
  hash_free(&hash_columns);
}


static byte* get_grant_table(GRANT_NAME *buff,uint *length,
			     my_bool not_used __attribute__((unused)))
{
  *length=buff->key_length;
  return (byte*) buff->hash_key;
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
				      bool exact)
{
  char helping [NAME_LEN*2+USERNAME_LENGTH+3];
  uint len;
  GRANT_NAME *grant_name,*found=0;

  len  = (uint) (strmov(strmov(strmov(helping,user)+1,db)+1,tname)-helping)+ 1;
  for (grant_name=(GRANT_NAME*) hash_search(name_hash,
					      (byte*) helping,
					      len) ;
       grant_name ;
       grant_name= (GRANT_NAME*) hash_next(name_hash,(byte*) helping,
					     len))
  {
    if (exact)
    {
      if (compare_hostname(&grant_name->host, host, ip))
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
		     host, ip, db, user, tname, exact);
}


inline GRANT_TABLE *
table_hash_search(const char *host, const char *ip, const char *db,
		  const char *user, const char *tname, bool exact)
{
  return (GRANT_TABLE*) name_hash_search(&column_priv_hash, host, ip, db,
					 user, tname, exact);
}


inline GRANT_COLUMN *
column_hash_search(GRANT_TABLE *t, const char *cname, uint length)
{
  return (GRANT_COLUMN*) hash_search(&t->hash_columns, (byte*) cname,length);
}


static int replace_column_table(GRANT_TABLE *g_t,
				TABLE *table, const LEX_USER &combo,
				List <LEX_COLUMN> &columns,
				const char *db, const char *table_name,
				ulong rights, bool revoke_grant)
{
  int error=0,result=0;
  byte key[MAX_KEY_LENGTH];
  uint key_prefix_length;
  KEY_PART_INFO *key_part= table->key_info->key_part;
  DBUG_ENTER("replace_column_table");

  table->field[0]->store(combo.host.str,combo.host.length,
                         system_charset_info);
  table->field[1]->store(db,(uint) strlen(db),
                         system_charset_info);
  table->field[2]->store(combo.user.str,combo.user.length,
                         system_charset_info);
  table->field[3]->store(table_name,(uint) strlen(table_name),
                         system_charset_info);

  /* Get length of 3 first key parts */
  key_prefix_length= (key_part[0].store_length + key_part[1].store_length +
                      key_part[2].store_length + key_part[3].store_length);
  key_copy(key, table->record[0], table->key_info, key_prefix_length);

  rights&= COL_ACLS;				// Only ACL for columns

  /* first fix privileges for all columns in column list */

  List_iterator <LEX_COLUMN> iter(columns);
  class LEX_COLUMN *column;
  table->file->ha_index_init(0);
  while ((column= iter++))
  {
    ulong privileges= column->rights;
    bool old_row_exists=0;
    byte user_key[MAX_KEY_LENGTH];

    key_restore(table->record[0],key,table->key_info,
                key_prefix_length);
    table->field[4]->store(column->column.ptr(), column->column.length(),
                           system_charset_info);
    /* Get key for the first 4 columns */
    key_copy(user_key, table->record[0], table->key_info,
             table->key_info->key_length);

    table->file->extra(HA_EXTRA_RETRIEVE_ALL_COLS);
    if (table->file->index_read(table->record[0], user_key,
				table->key_info->key_length,
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
	error=table->file->update_row(table->record[1],table->record[0]);
      else
	error=table->file->delete_row(table->record[1]);
      if (error)
      {
	table->file->print_error(error,MYF(0)); /* purecov: inspected */
	result= -1;				/* purecov: inspected */
	goto end;				/* purecov: inspected */
      }
      grant_column= column_hash_search(g_t, column->column.ptr(),
                                       column->column.length());
      if (grant_column)				// Should always be true
	grant_column->rights= privileges;	// Update hash
    }
    else					// new grant
    {
      GRANT_COLUMN *grant_column;
      if ((error=table->file->write_row(table->record[0])))
      {
	table->file->print_error(error,MYF(0)); /* purecov: inspected */
	result= -1;				/* purecov: inspected */
	goto end;				/* purecov: inspected */
      }
      grant_column= new GRANT_COLUMN(column->column,privileges);
      my_hash_insert(&g_t->hash_columns,(byte*) grant_column);
    }
  }

  /*
    If revoke of privileges on the table level, remove all such privileges
    for all columns
  */

  if (revoke_grant)
  {
    byte user_key[MAX_KEY_LENGTH];
    key_copy(user_key, table->record[0], table->key_info,
             key_prefix_length);

    table->file->extra(HA_EXTRA_RETRIEVE_ALL_COLS);
    if (table->file->index_read(table->record[0], user_key,
				key_prefix_length,
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
	  if ((tmp_error=table->file->update_row(table->record[1],
						 table->record[0])))
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
	  if ((tmp_error = table->file->delete_row(table->record[1])))
	  {					/* purecov: deadcode */
	    table->file->print_error(tmp_error,MYF(0)); /* purecov: deadcode */
	    result= -1;				/* purecov: deadcode */
	    goto end;				/* purecov: deadcode */
	  }
	  if (grant_column)
	    hash_delete(&g_t->hash_columns,(byte*) grant_column);
	}
      }
    } while (!table->file->index_next(table->record[0]) &&
	     !key_cmp_if_same(table, key, 0, key_prefix_length));
  }

end:
  table->file->ha_index_end();
  DBUG_RETURN(result);
}


static int replace_table_table(THD *thd, GRANT_TABLE *grant_table,
			       TABLE *table, const LEX_USER &combo,
			       const char *db, const char *table_name,
			       ulong rights, ulong col_rights,
			       bool revoke_grant)
{
  char grantor[HOSTNAME_LENGTH+USERNAME_LENGTH+2];
  int old_row_exists = 1;
  int error=0;
  ulong store_table_rights, store_col_rights;
  byte user_key[MAX_KEY_LENGTH];
  DBUG_ENTER("replace_table_table");

  strxmov(grantor, thd->security_ctx->user, "@",
          thd->security_ctx->host_or_ip, NullS);

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

  restore_record(table, s->default_values);     // Get empty record
  table->field[0]->store(combo.host.str,combo.host.length, system_charset_info);
  table->field[1]->store(db,(uint) strlen(db), system_charset_info);
  table->field[2]->store(combo.user.str,combo.user.length, system_charset_info);
  table->field[3]->store(table_name,(uint) strlen(table_name), system_charset_info);
  store_record(table,record[1]);			// store at pos 1
  key_copy(user_key, table->record[0], table->key_info,
           table->key_info->key_length);

  table->file->extra(HA_EXTRA_RETRIEVE_ALL_COLS);
  if (table->file->index_read_idx(table->record[0], 0,
                                  user_key, table->key_info->key_length,
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
      if ((error=table->file->update_row(table->record[1],table->record[0])))
	goto table_error;			/* purecov: deadcode */
    }
    else if ((error = table->file->delete_row(table->record[1])))
      goto table_error;				/* purecov: deadcode */
  }
  else
  {
    error=table->file->write_row(table->record[0]);
    if (error && error != HA_ERR_FOUND_DUPP_KEY)
      goto table_error;				/* purecov: deadcode */
  }

  if (rights | col_rights)
  {
    grant_table->privs= rights;
    grant_table->cols=	col_rights;
  }
  else
  {
    hash_delete(&column_priv_hash,(byte*) grant_table);
  }
  DBUG_RETURN(0);

  /* This should never happen */
table_error:
  table->file->print_error(error,MYF(0)); /* purecov: deadcode */
  DBUG_RETURN(-1); /* purecov: deadcode */
}


static int replace_routine_table(THD *thd, GRANT_NAME *grant_name,
			      TABLE *table, const LEX_USER &combo,
			      const char *db, const char *routine_name,
			      bool is_proc, ulong rights, bool revoke_grant)
{
  char grantor[HOSTNAME_LENGTH+USERNAME_LENGTH+2];
  int old_row_exists= 1;
  int error=0;
  ulong store_proc_rights;
  DBUG_ENTER("replace_routine_table");

  if (!initialized)
  {
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--skip-grant-tables");
    DBUG_RETURN(-1);
  }

  strxmov(grantor, thd->security_ctx->user, "@",
          thd->security_ctx->host_or_ip, NullS);

  /*
    The following should always succeed as new users are created before
    this function is called!
  */
  if (!find_acl_user(combo.host.str, combo.user.str, FALSE))
  {
    my_error(ER_PASSWORD_NO_MATCH,MYF(0));
    DBUG_RETURN(-1);
  }

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

  if (table->file->index_read_idx(table->record[0],0,
				  (byte*) table->field[0]->ptr,0,
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
      if ((error=table->file->update_row(table->record[1],table->record[0])))
	goto table_error;
    }
    else if ((error= table->file->delete_row(table->record[1])))
      goto table_error;
  }
  else
  {
    error=table->file->write_row(table->record[0]);
    if (error && error != HA_ERR_FOUND_DUPP_KEY)
      goto table_error;
  }

  if (rights)
  {
    grant_name->privs= rights;
  }
  else
  {
    hash_delete(is_proc ? &proc_priv_hash : &func_priv_hash,(byte*) grant_name);
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

bool mysql_table_grant(THD *thd, TABLE_LIST *table_list,
		      List <LEX_USER> &user_list,
		      List <LEX_COLUMN> &columns, ulong rights,
		      bool revoke_grant)
{
  ulong column_priv= 0;
  List_iterator <LEX_USER> str_list (user_list);
  LEX_USER *Str;
  TABLE_LIST tables[3];
  bool create_new_users=0;
  char *db_name, *table_name;
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
                                         column->column.ptr(), NULL, NULL,
                                         column->column.length(), 0, 1, 1, 0,
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
        char buf[FN_REFLEN];
        sprintf(buf,"%s/%s/%s.frm",mysql_data_home, table_list->db,
                table_list->table_name);
        fn_format(buf,buf,"","",4+16+32);
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

#ifdef HAVE_REPLICATION
  /*
    GRANT and REVOKE are applied the slave in/exclusion rules as they are
    some kind of updates to the mysql.% tables.
  */
  if (thd->slave_thread && table_rules_on)
  {
    /*
      The tables must be marked "updating" so that tables_ok() takes them into
      account in tests.
    */
    tables[0].updating= tables[1].updating= tables[2].updating= 1;
    if (!tables_ok(thd, tables))
      DBUG_RETURN(FALSE);
  }
#endif

  if (simple_open_n_lock_tables(thd,tables))
  {						// Should never happen
    close_thread_tables(thd);			/* purecov: deadcode */
    DBUG_RETURN(TRUE);				/* purecov: deadcode */
  }

  if (!revoke_grant)
    create_new_users= test_if_create_new_users(thd);
  bool result= FALSE;
  rw_wrlock(&LOCK_grant);
  MEM_ROOT *old_root= thd->mem_root;
  thd->mem_root= &memex;
  grant_version++;

  while ((Str = str_list++))
  {
    int error;
    GRANT_TABLE *grant_table;
    if (Str->host.length > HOSTNAME_LENGTH ||
	Str->user.length > USERNAME_LENGTH)
    {
      my_message(ER_GRANT_WRONG_HOST_OR_USER, ER(ER_GRANT_WRONG_HOST_OR_USER),
                 MYF(0));
      result= TRUE;
      continue;
    }
    /* Create user if needed */
    pthread_mutex_lock(&acl_cache->lock);
    error=replace_user_table(thd, tables[0].table, *Str,
			     0, revoke_grant, create_new_users,
                             test(thd->variables.sql_mode &
                                  MODE_NO_AUTO_CREATE_USER));
    pthread_mutex_unlock(&acl_cache->lock);
    if (error)
    {
      result= TRUE;				// Remember error
      continue;					// Add next user
    }

    db_name= (table_list->view_db.length ?
	      table_list->view_db.str :
	      table_list->db);
    table_name= (table_list->view_name.length ?
		table_list->view_name.str :
		table_list->table_name);

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
      if (!grant_table)				// end of memory
      {
	result= TRUE;				/* purecov: deadcode */
	continue;				/* purecov: deadcode */
      }
      my_hash_insert(&column_priv_hash,(byte*) grant_table);
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
  grant_option=TRUE;
  thd->mem_root= old_root;
  rw_unlock(&LOCK_grant);
  if (!result)
    send_ok(thd);
  /* Tables are automatically closed */
  DBUG_RETURN(result);
}


/*
  Store routine level grants in the privilege tables

  SYNOPSIS
    mysql_routine_grant()
    thd			Thread handle
    table_list		List of routines to give grant
    is_proc             true indicates routine list are procedures
    user_list		List of users to give grant
    rights		Table level grant
    revoke_grant	Set to 1 if this is a REVOKE command

  RETURN
    0	ok
    1	error
*/

bool mysql_routine_grant(THD *thd, TABLE_LIST *table_list, bool is_proc,
			 List <LEX_USER> &user_list, ulong rights,
			 bool revoke_grant, bool no_error)
{
  List_iterator <LEX_USER> str_list (user_list);
  LEX_USER *Str;
  TABLE_LIST tables[2];
  bool create_new_users=0, result=0;
  char *db_name, *table_name;
  DBUG_ENTER("mysql_routine_grant");

  if (!initialized)
  {
    if (!no_error)
      my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0),
               "--skip-grant-tables");
    DBUG_RETURN(TRUE);
  }
  if (rights & ~PROC_ACLS)
  {
    if (!no_error)
      my_message(ER_ILLEGAL_GRANT_FOR_TABLE, ER(ER_ILLEGAL_GRANT_FOR_TABLE),
        	 MYF(0));
    DBUG_RETURN(TRUE);
  }

  if (!revoke_grant)
  {
    if (sp_exists_routine(thd, table_list, is_proc, no_error)<0)
      DBUG_RETURN(TRUE);
  }

  /* open the mysql.user and mysql.procs_priv tables */

  bzero((char*) &tables,sizeof(tables));
  tables[0].alias=tables[0].table_name= (char*) "user";
  tables[1].alias=tables[1].table_name= (char*) "procs_priv";
  tables[0].next_local= tables[0].next_global= tables+1;
  tables[0].lock_type=tables[1].lock_type=TL_WRITE;
  tables[0].db=tables[1].db=(char*) "mysql";

#ifdef HAVE_REPLICATION
  /*
    GRANT and REVOKE are applied the slave in/exclusion rules as they are
    some kind of updates to the mysql.% tables.
  */
  if (thd->slave_thread && table_rules_on)
  {
    /*
      The tables must be marked "updating" so that tables_ok() takes them into
      account in tests.
    */
    tables[0].updating= tables[1].updating= 1;
    if (!tables_ok(thd, tables))
      DBUG_RETURN(FALSE);
  }
#endif

  if (simple_open_n_lock_tables(thd,tables))
  {						// Should never happen
    close_thread_tables(thd);
    DBUG_RETURN(TRUE);
  }

  if (!revoke_grant)
    create_new_users= test_if_create_new_users(thd);
  rw_wrlock(&LOCK_grant);
  MEM_ROOT *old_root= thd->mem_root;
  thd->mem_root= &memex;

  DBUG_PRINT("info",("now time to iterate and add users"));

  while ((Str= str_list++))
  {
    int error;
    GRANT_NAME *grant_name;
    if (Str->host.length > HOSTNAME_LENGTH ||
	Str->user.length > USERNAME_LENGTH)
    {
      if (!no_error)
	my_message(ER_GRANT_WRONG_HOST_OR_USER, ER(ER_GRANT_WRONG_HOST_OR_USER),
                   MYF(0));
      result= TRUE;
      continue;
    }
    /* Create user if needed */
    pthread_mutex_lock(&acl_cache->lock);
    error=replace_user_table(thd, tables[0].table, *Str,
			     0, revoke_grant, create_new_users,
                             test(thd->variables.sql_mode &
                                  MODE_NO_AUTO_CREATE_USER));
    pthread_mutex_unlock(&acl_cache->lock);
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
        if (!no_error)
          my_error(ER_NONEXISTING_PROC_GRANT, MYF(0),
		   Str->user.str, Str->host.str, table_name);
	result= TRUE;
	continue;
      }
      grant_name= new GRANT_NAME(Str->host.str, db_name,
				 Str->user.str, table_name,
				 rights);
      if (!grant_name)
      {
        result= TRUE;
	continue;
      }
      my_hash_insert(is_proc ? &proc_priv_hash : &func_priv_hash,(byte*) grant_name);
    }

    if (replace_routine_table(thd, grant_name, tables[1].table, *Str,
			   db_name, table_name, is_proc, rights, revoke_grant))
    {
      result= TRUE;
      continue;
    }
  }
  grant_option=TRUE;
  thd->mem_root= old_root;
  rw_unlock(&LOCK_grant);
  if (!result && !no_error)
    send_ok(thd);
  /* Tables are automatically closed */
  DBUG_RETURN(result);
}


bool mysql_grant(THD *thd, const char *db, List <LEX_USER> &list,
                 ulong rights, bool revoke_grant)
{
  List_iterator <LEX_USER> str_list (list);
  LEX_USER *Str;
  char tmp_db[NAME_LEN+1];
  bool create_new_users=0;
  TABLE_LIST tables[2];
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

#ifdef HAVE_REPLICATION
  /*
    GRANT and REVOKE are applied the slave in/exclusion rules as they are
    some kind of updates to the mysql.% tables.
  */
  if (thd->slave_thread && table_rules_on)
  {
    /*
      The tables must be marked "updating" so that tables_ok() takes them into
      account in tests.
    */
    tables[0].updating= tables[1].updating= 1;
    if (!tables_ok(thd, tables))
      DBUG_RETURN(FALSE);
  }
#endif

  if (simple_open_n_lock_tables(thd,tables))
  {						// This should never happen
    close_thread_tables(thd);			/* purecov: deadcode */
    DBUG_RETURN(TRUE);				/* purecov: deadcode */
  }

  if (!revoke_grant)
    create_new_users= test_if_create_new_users(thd);

  /* go through users in user_list */
  rw_wrlock(&LOCK_grant);
  VOID(pthread_mutex_lock(&acl_cache->lock));
  grant_version++;

  int result=0;
  while ((Str = str_list++))
  {
    if (Str->host.length > HOSTNAME_LENGTH ||
	Str->user.length > USERNAME_LENGTH)
    {
      my_message(ER_GRANT_WRONG_HOST_OR_USER, ER(ER_GRANT_WRONG_HOST_OR_USER),
                 MYF(0));
      result= -1;
      continue;
    }
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
  rw_unlock(&LOCK_grant);
  close_thread_tables(thd);

  if (!result)
    send_ok(thd);
  DBUG_RETURN(result);
}


/* Free grant array if possible */

void  grant_free(void)
{
  DBUG_ENTER("grant_free");
  grant_option = FALSE;
  hash_free(&column_priv_hash);
  hash_free(&proc_priv_hash);
  hash_free(&func_priv_hash);
  free_root(&memex,MYF(0));
  DBUG_VOID_RETURN;
}


/*
  Initialize structures responsible for table/column-level privilege checking
  and load information for them from tables in the 'mysql' database.

  SYNOPSIS
    grant_init()

  RETURN VALUES
    0	ok
    1	Could not initialize grant's
*/

my_bool grant_init()
{
  THD  *thd;
  my_bool return_val;
  DBUG_ENTER("grant_init");

  if (!(thd= new THD))
    DBUG_RETURN(1);				/* purecov: deadcode */
  thd->store_globals();
  return_val=  grant_reload(thd);
  delete thd;
  /* Remember that we don't have a THD */
  my_pthread_setspecific_ptr(THR_THD,  0);
  DBUG_RETURN(return_val);
}


/*
  Initialize structures responsible for table/column-level privilege
  checking and load information about grants from open privilege tables.

  SYNOPSIS
    grant_load()
      thd     Current thread
      tables  List containing open "mysql.tables_priv" and
              "mysql.columns_priv" tables.

  RETURN VALUES
    FALSE - success
    TRUE  - error
*/

static my_bool grant_load(TABLE_LIST *tables)
{
  MEM_ROOT *memex_ptr;
  my_bool return_val= 1;
  TABLE *t_table, *c_table, *p_table;
  bool check_no_resolve= specialflag & SPECIAL_NO_RESOLVE;
  MEM_ROOT **save_mem_root_ptr= my_pthread_getspecific_ptr(MEM_ROOT**,
                                                           THR_MALLOC);
  DBUG_ENTER("grant_load");

  grant_option = FALSE;
  (void) hash_init(&column_priv_hash,system_charset_info,
		   0,0,0, (hash_get_key) get_grant_table,
		   (hash_free_key) free_grant_table,0);
  (void) hash_init(&proc_priv_hash,system_charset_info,
		   0,0,0, (hash_get_key) get_grant_table,
		   0,0);
  (void) hash_init(&func_priv_hash,system_charset_info,
		   0,0,0, (hash_get_key) get_grant_table,
		   0,0);
  init_sql_alloc(&memex, ACL_ALLOC_BLOCK_SIZE, 0);

  t_table = tables[0].table; c_table = tables[1].table;
  p_table= tables[2].table;
  t_table->file->ha_index_init(0);
  p_table->file->ha_index_init(0);
  if (!t_table->file->index_first(t_table->record[0]))
  {
    memex_ptr= &memex;
    my_pthread_setspecific_ptr(THR_MALLOC, &memex_ptr);
    do
    {
      GRANT_TABLE *mem_check;
      if (!(mem_check=new GRANT_TABLE(t_table,c_table)))
      {
	/* This could only happen if we are out memory */
	grant_option= FALSE;
	goto end_unlock;
      }

      if (check_no_resolve)
      {
	if (hostname_requires_resolving(mem_check->host.hostname))
	{
          sql_print_warning("'tables_priv' entry '%s %s@%s' "
                            "ignored in --skip-name-resolve mode.",
                            mem_check->tname, mem_check->user,
                            mem_check->host, mem_check->host);
	  continue;
	}
      }

      if (! mem_check->ok())
	delete mem_check;
      else if (my_hash_insert(&column_priv_hash,(byte*) mem_check))
      {
	delete mem_check;
	grant_option= FALSE;
	goto end_unlock;
      }
    }
    while (!t_table->file->index_next(t_table->record[0]));
  }
  if (!p_table->file->index_first(p_table->record[0]))
  {
    memex_ptr= &memex;
    my_pthread_setspecific_ptr(THR_MALLOC, &memex_ptr);
    do
    {
      GRANT_NAME *mem_check;
      HASH *hash;
      if (!(mem_check=new GRANT_NAME(p_table)))
      {
	/* This could only happen if we are out memory */
	grant_option= FALSE;
	goto end_unlock;
      }

      if (check_no_resolve)
      {
	if (hostname_requires_resolving(mem_check->host.hostname))
	{
          sql_print_warning("'procs_priv' entry '%s %s@%s' "
                            "ignored in --skip-name-resolve mode.",
                            mem_check->tname, mem_check->user,
                            mem_check->host);
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
      else if (my_hash_insert(hash, (byte*) mem_check))
      {
	delete mem_check;
	grant_option= FALSE;
	goto end_unlock;
      }
    }
    while (!p_table->file->index_next(p_table->record[0]));
  }
  grant_option= TRUE;
  return_val=0;					// Return ok

end_unlock:
  t_table->file->ha_index_end();
  p_table->file->ha_index_end();
  my_pthread_setspecific_ptr(THR_MALLOC, save_mem_root_ptr);
  DBUG_RETURN(return_val);
}


/*
  Reload information about table and column level privileges if possible.

  SYNOPSIS
    grant_reload()
      thd  Current thread

  NOTES
    Locked tables are checked by acl_reload() and doesn't have to be checked
    in this call.
    This function is also used for initialization of structures responsible
    for table/column-level privilege checking.

  RETURN VALUE
    FALSE Success
    TRUE  Error
*/

my_bool grant_reload(THD *thd)
{
  TABLE_LIST tables[3];
  HASH old_column_priv_hash, old_proc_priv_hash, old_func_priv_hash;
  bool old_grant_option;
  MEM_ROOT old_mem;
  my_bool return_val= 1;
  DBUG_ENTER("grant_reload");

  /* Don't do anything if running with --skip-grant-tables */
  if (!initialized)
    DBUG_RETURN(0);

  bzero((char*) tables, sizeof(tables));
  tables[0].alias= tables[0].table_name= (char*) "tables_priv";
  tables[1].alias= tables[1].table_name= (char*) "columns_priv";
  tables[2].alias= tables[2].table_name= (char*) "procs_priv";
  tables[0].db= tables[1].db= tables[2].db= (char *) "mysql";
  tables[0].next_local= tables[0].next_global= tables+1;
  tables[1].next_local= tables[1].next_global= tables+2;
  tables[0].lock_type= tables[1].lock_type= tables[2].lock_type= TL_READ;

  /*
    To avoid deadlocks we should obtain table locks before
    obtaining LOCK_grant rwlock.
  */
  if (simple_open_n_lock_tables(thd, tables))
    goto end;

  rw_wrlock(&LOCK_grant);
  grant_version++;
  old_column_priv_hash= column_priv_hash;
  old_proc_priv_hash= proc_priv_hash;
  old_func_priv_hash= func_priv_hash;
  old_grant_option= grant_option;
  old_mem= memex;

  if ((return_val= grant_load(tables)))
  {						// Error. Revert to old hash
    DBUG_PRINT("error",("Reverting to old privileges"));
    grant_free();				/* purecov: deadcode */
    column_priv_hash= old_column_priv_hash;	/* purecov: deadcode */
    proc_priv_hash= old_proc_priv_hash;
    func_priv_hash= old_func_priv_hash;
    grant_option= old_grant_option;		/* purecov: deadcode */
    memex= old_mem;				/* purecov: deadcode */
  }
  else
  {
    hash_free(&old_column_priv_hash);
    hash_free(&old_proc_priv_hash);
    hash_free(&old_func_priv_hash);
    free_root(&old_mem,MYF(0));
  }
  rw_unlock(&LOCK_grant);
end:
  close_thread_tables(thd);
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
****************************************************************************/

bool check_grant(THD *thd, ulong want_access, TABLE_LIST *tables,
		 uint show_table, uint number, bool no_errors)
{
  TABLE_LIST *table;
  Security_context *sctx= thd->security_ctx;
  DBUG_ENTER("check_grant");
  DBUG_ASSERT(number > 0);

  want_access&= ~sctx->master_access;
  if (!want_access)
    DBUG_RETURN(0);                             // ok

  rw_rdlock(&LOCK_grant);
  for (table= tables; table && number--; table= table->next_global)
  {
    GRANT_TABLE *grant_table;
    if (!(~table->grant.privilege & want_access) || 
        table->derived || table->schema_table || table->belong_to_view)
    {
      /*
        It is subquery in the FROM clause. VIEW set table->derived after
        table opening, but this function always called before table opening.
      */
      table->grant.want_privilege= 0;
      continue;					// Already checked
    }
    if (!(grant_table= table_hash_search(sctx->host, sctx->ip,
                                         table->db, sctx->priv_user,
                                         table->table_name,0)))
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
             table ? table->table_name : "unknown");
  }
  DBUG_RETURN(1);
}


bool check_grant_column(THD *thd, GRANT_INFO *grant,
			const char *db_name, const char *table_name,
			const char *name, uint length, uint show_tables)
{
  Security_context *sctx= thd->security_ctx;
  GRANT_TABLE *grant_table;
  GRANT_COLUMN *grant_column;
  ulong want_access= grant->want_privilege & ~grant->privilege;
  DBUG_ENTER("check_grant_column");
  DBUG_PRINT("enter", ("table: %s  want_access: %u", table_name, want_access));

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
#ifdef NOT_USED
  if (show_tables && (grant_column || grant->privilege & COL_ACLS))
  {
    rw_unlock(&LOCK_grant);			/* purecov: deadcode */
    DBUG_RETURN(0);				/* purecov: deadcode */
  }
#endif

err:
  rw_unlock(&LOCK_grant);
  if (!show_tables)
  {
    char command[128];
    get_privilege_desc(command, sizeof(command), want_access);
    my_error(ER_COLUMNACCESS_DENIED_ERROR, MYF(0),
             command,
             sctx->priv_user,
             sctx->host_or_ip,
             name,
             table_name);
  }
  DBUG_RETURN(1);
}


bool check_grant_all_columns(THD *thd, ulong want_access, GRANT_INFO *grant,
                             const char* db_name, const char *table_name,
                             Field_iterator *fields)
{
  Security_context *sctx= thd->security_ctx;
  GRANT_TABLE *grant_table;
  GRANT_COLUMN *grant_column;

  want_access &= ~grant->privilege;
  if (!want_access)
    return 0;				// Already checked
  if (!grant_option)
    goto err2;

  rw_rdlock(&LOCK_grant);

  /* reload table if someone has modified any grants */

  if (grant->version != grant_version)
  {
    grant->grant_table=
      table_hash_search(sctx->host, sctx->ip, db_name,
			sctx->priv_user,
			table_name, 0);	/* purecov: inspected */
    grant->version= grant_version;		/* purecov: inspected */
  }
  /* The following should always be true */
  if (!(grant_table= grant->grant_table))
    goto err;					/* purecov: inspected */

  for (; !fields->end_of_fields(); fields->next())
  {
    const char *field_name= fields->name();
    grant_column= column_hash_search(grant_table, field_name,
				    (uint) strlen(field_name));
    if (!grant_column || (~grant_column->rights & want_access))
      goto err;
  }
  rw_unlock(&LOCK_grant);
  return 0;

err:
  rw_unlock(&LOCK_grant);
err2:
  char command[128];
  get_privilege_desc(command, sizeof(command), want_access);
  my_error(ER_COLUMNACCESS_DENIED_ERROR, MYF(0),
           command,
           sctx->priv_user,
           sctx->host_or_ip,
           fields->name(),
           table_name);
  return 1;
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
  bool error= 1;

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
      error=0;					// Found match
      break;
    }
  }
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
  if (grant_option)
  {
    GRANT_NAME *grant_proc;
    Security_context *sctx= thd->security_ctx;
    rw_rdlock(&LOCK_grant);
    if ((grant_proc= routine_hash_search(sctx->priv_host,
                                         sctx->ip, db,
                                         sctx->priv_user,
                                         name, is_proc, 0)))
      no_routine_acl= !(grant_proc->privs & SHOW_PROC_ACLS);
    rw_unlock(&LOCK_grant);
  }
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
  "CREATE USER"
};

static uint command_lengths[]=
{
  6, 6, 6, 6, 6, 4, 6, 8, 7, 4, 5, 10, 5, 5, 14, 5, 23, 11, 7, 17, 18, 11, 9,
  14, 13, 11
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

  if (!lex_user->host.str)
  {
    lex_user->host.str= (char*) "%";
    lex_user->host.length=1;
  }
  if (lex_user->host.length > HOSTNAME_LENGTH ||
      lex_user->user.length > USERNAME_LENGTH)
  {
    my_message(ER_GRANT_WRONG_HOST_OR_USER, ER(ER_GRANT_WRONG_HOST_OR_USER),
               MYF(0));
    DBUG_RETURN(TRUE);
  }

  for (counter=0 ; counter < acl_users.elements ; counter++)
  {
    const char *user,*host;
    acl_user=dynamic_element(&acl_users,counter,ACL_USER*);
    if (!(user=acl_user->user))
      user= "";
    if (!(host=acl_user->host.hostname))
      host= "";
    if (!strcmp(lex_user->user.str,user) &&
	!my_strcasecmp(system_charset_info, lex_user->host.str, host))
      break;
  }
  if (counter == acl_users.elements)
  {
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
    DBUG_RETURN(TRUE);

  rw_wrlock(&LOCK_grant);
  VOID(pthread_mutex_lock(&acl_cache->lock));

  /* Add first global access grants */
  {
    String global(buff,sizeof(buff),system_charset_info);
    global.length(0);
    global.append("GRANT ",6);

    want_access= acl_user->access;
    if (test_all_bits(want_access, (GLOBAL_ACLS & ~ GRANT_ACL)))
      global.append("ALL PRIVILEGES",14);
    else if (!(want_access & ~GRANT_ACL))
      global.append("USAGE",5);
    else
    {
      bool found=0;
      ulong j,test_access= want_access & ~GRANT_ACL;
      for (counter=0, j = SELECT_ACL;j <= GLOBAL_ACLS;counter++,j <<= 1)
      {
	if (test_access & j)
	{
	  if (found)
	    global.append(", ",2);
	  found=1;
	  global.append(command_array[counter],command_lengths[counter]);
	}
      }
    }
    global.append (" ON *.* TO '",12);
    global.append(lex_user->user.str, lex_user->user.length,
		  system_charset_info);
    global.append ("'@'",3);
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
      global.append(" IDENTIFIED BY PASSWORD '",25);
      global.append(passwd_buff);
      global.append('\'');
    }
    /* "show grants" SSL related stuff */
    if (acl_user->ssl_type == SSL_TYPE_ANY)
      global.append(" REQUIRE SSL",12);
    else if (acl_user->ssl_type == SSL_TYPE_X509)
      global.append(" REQUIRE X509",13);
    else if (acl_user->ssl_type == SSL_TYPE_SPECIFIED)
    {
      int ssl_options = 0;
      global.append(" REQUIRE ",9);
      if (acl_user->x509_issuer)
      {
	ssl_options++;
	global.append("ISSUER \'",8);
	global.append(acl_user->x509_issuer,strlen(acl_user->x509_issuer));
	global.append('\'');
      }
      if (acl_user->x509_subject)
      {
	if (ssl_options++)
	  global.append(' ');
	global.append("SUBJECT \'",9);
	global.append(acl_user->x509_subject,strlen(acl_user->x509_subject),
                      system_charset_info);
	global.append('\'');
      }
      if (acl_user->ssl_cipher)
      {
	if (ssl_options++)
	  global.append(' ');
	global.append("CIPHER '",8);
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
      global.append(" WITH",5);
      if (want_access & GRANT_ACL)
	global.append(" GRANT OPTION",13);
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

    if (!strcmp(lex_user->user.str,user) &&
	!my_strcasecmp(system_charset_info, lex_user->host.str, host))
    {
      want_access=acl_db->access;
      if (want_access)
      {
	String db(buff,sizeof(buff),system_charset_info);
	db.length(0);
	db.append("GRANT ",6);

	if (test_all_bits(want_access,(DB_ACLS & ~GRANT_ACL)))
	  db.append("ALL PRIVILEGES",14);
	else if (!(want_access & ~GRANT_ACL))
	  db.append("USAGE",5);
	else
	{
	  int found=0, cnt;
	  ulong j,test_access= want_access & ~GRANT_ACL;
	  for (cnt=0, j = SELECT_ACL; j <= DB_ACLS; cnt++,j <<= 1)
	  {
	    if (test_access & j)
	    {
	      if (found)
		db.append(", ",2);
	      found = 1;
	      db.append(command_array[cnt],command_lengths[cnt]);
	    }
	  }
	}
	db.append (" ON ",4);
	append_identifier(thd, &db, acl_db->db, strlen(acl_db->db));
	db.append (".* TO '",7);
	db.append(lex_user->user.str, lex_user->user.length,
		  system_charset_info);
	db.append ("'@'",3);
	db.append(lex_user->host.str, lex_user->host.length,
                  system_charset_info);
	db.append ('\'');
	if (want_access & GRANT_ACL)
	  db.append(" WITH GRANT OPTION",18);
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
    const char *user;
    GRANT_TABLE *grant_table= (GRANT_TABLE*) hash_element(&column_priv_hash,
							  index);

    if (!(user=grant_table->user))
      user= "";

    if (!strcmp(lex_user->user.str,user) &&
	!my_strcasecmp(system_charset_info, lex_user->host.str,
                       grant_table->host.hostname))
    {
      ulong table_access= grant_table->privs;
      if ((table_access | grant_table->cols) != 0)
      {
	String global(buff, sizeof(buff), system_charset_info);
	ulong test_access= (table_access | grant_table->cols) & ~GRANT_ACL;

	global.length(0);
	global.append("GRANT ",6);

	if (test_all_bits(table_access, (TABLE_ACLS & ~GRANT_ACL)))
	  global.append("ALL PRIVILEGES",14);
	else if (!test_access)
 	  global.append("USAGE",5);
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
		global.append(", ",2);
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
			global.append(", ", 2);
			global.append(command_array[counter],
				      command_lengths[counter]);
		      }
		      global.append(" (",2);
		    }
		    else
		      global.append(", ",2);
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
	global.append(" ON ",4);
	append_identifier(thd, &global, grant_table->db,
			  strlen(grant_table->db));
	global.append('.');
	append_identifier(thd, &global, grant_table->tname,
			  strlen(grant_table->tname));
	global.append(" TO '",5);
	global.append(lex_user->user.str, lex_user->user.length,
		      system_charset_info);
	global.append("'@'",3);
	global.append(lex_user->host.str,lex_user->host.length,
		      system_charset_info);
	global.append('\'');
	if (table_access & GRANT_ACL)
	  global.append(" WITH GRANT OPTION",18);
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
                          "PROCEDURE", 9, buff, sizeof(buff)))
  {
    error= -1;
    goto end;
  }

  if (show_routine_grants(thd, lex_user, &func_priv_hash,
                          "FUNCTION", 8, buff, sizeof(buff)))
  {
    error= -1;
    goto end;
  }

end:
  VOID(pthread_mutex_unlock(&acl_cache->lock));
  rw_unlock(&LOCK_grant);

  send_eof(thd);
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
    const char *user;
    GRANT_NAME *grant_proc= (GRANT_NAME*) hash_element(hash, index);

    if (!(user=grant_proc->user))
      user= "";

    if (!strcmp(lex_user->user.str,user) &&
	!my_strcasecmp(system_charset_info, lex_user->host.str,
                       grant_proc->host.hostname))
    {
      ulong proc_access= grant_proc->privs;
      if (proc_access != 0)
      {
	String global(buff, buffsize, system_charset_info);
	ulong test_access= proc_access & ~GRANT_ACL;

	global.length(0);
	global.append("GRANT ",6);

	if (!test_access)
 	  global.append("USAGE",5);
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
		global.append(", ",2);
	      found= 1;
	      global.append(command_array[counter],command_lengths[counter]);
	    }
	  }
	}
	global.append(" ON ",4);
        global.append(type,typelen);
        global.append(' ');
	append_identifier(thd, &global, grant_proc->db,
			  strlen(grant_proc->db));
	global.append('.');
	append_identifier(thd, &global, grant_proc->tname,
			  strlen(grant_proc->tname));
	global.append(" TO '",5);
	global.append(lex_user->user.str, lex_user->user.length,
		      system_charset_info);
	global.append("'@'",3);
	global.append(lex_user->host.str,lex_user->host.length,
		      system_charset_info);
	global.append('\'');
	if (proc_access & GRANT_ACL)
	  global.append(" WITH GRANT OPTION",18);
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
  if (initialized && (acl_user= find_acl_user(host,user, FALSE)))
    uc->user_resources= acl_user->user_resource;
  else
    bzero((char*) &uc->user_resources, sizeof(uc->user_resources));
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
  if (thd->slave_thread && table_rules_on)
  {
    /*
      The tables must be marked "updating" so that tables_ok() takes them into
      account in tests.
    */
    tables[0].updating=tables[1].updating=tables[2].updating=
      tables[3].updating=tables[4].updating=1;
    if (!tables_ok(thd, tables))
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

  for (counter= 0 ; counter < acl_users.elements ; counter++)
  {
    const char *user,*host;
    acl_user= dynamic_element(&acl_users, counter, ACL_USER*);
    if (!(user=acl_user->user))
      user= "";
    if (!(host=acl_user->host.hostname))
      host= "%";
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
    if ((error= table->file->update_row(table->record[1], table->record[0])))
      table->file->print_error(error, MYF(0));
  }
  else
  {
    /* delete */
    if ((error=table->file->delete_row(table->record[0])))
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
  byte user_key[MAX_KEY_LENGTH];
  uint key_prefix_length;
  DBUG_ENTER("handle_grant_table");

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
                       table->s->table_name, user_str, host_str));
    host_field->store(host_str, user_from->host.length, system_charset_info);
    user_field->store(user_str, user_from->user.length, system_charset_info);

    key_prefix_length= (table->key_info->key_part[0].store_length +
                        table->key_info->key_part[1].store_length);
    key_copy(user_key, table->record[0], table->key_info, key_prefix_length);

    if ((error= table->file->index_read_idx(table->record[0], 0,
                                            user_key, key_prefix_length,
                                            HA_READ_KEY_EXACT)))
    {
      if (error != HA_ERR_KEY_NOT_FOUND)
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
                         table->s->table_name, user_str, host_str));
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
        if (! (host= get_field(&mem, host_field)))
          host= "";
        if (! (user= get_field(&mem, user_field)))
          user= "";

#ifdef EXTRA_DEBUG
        DBUG_PRINT("loop",("scan fields: '%s'@'%s' '%s' '%s' '%s'",
                           user, host,
                           get_field(&mem, table->field[1]) /*db*/,
                           get_field(&mem, table->field[3]) /*table*/,
                           get_field(&mem, table->field[4]) /*column*/));
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


/*
  Handle an in-memory privilege structure.

  SYNOPSIS
    handle_grant_struct()
    struct_no                   The number of the structure to handle (0..3).
    drop                        If user_from is to be dropped.
    user_from                   The the user to be searched/dropped/renamed.
    user_to                     The new name for the user if to be renamed,
                                NULL otherwise.

  DESCRIPTION
    Scan through all elements in an in-memory grant structure and apply
    the requested operation.
    Delete from grant structure if drop is true.
    Update in grant structure if drop is false and user_to is not NULL.
    Search in grant structure if drop is false and user_to is NULL.
    Structures are numbered as follows:
    0 acl_users
    1 acl_dbs
    2 column_priv_hash
    3 procs_priv_hash

  RETURN
    > 0         At least one element matched.
    0           OK, but no element matched.
    -1		Wrong arguments to function
*/

static int handle_grant_struct(uint struct_no, bool drop,
                               LEX_USER *user_from, LEX_USER *user_to)
{
  int result= 0;
  uint idx;
  uint elements;
  const char *user;
  const char *host;
  ACL_USER *acl_user;
  ACL_DB *acl_db;
  GRANT_NAME *grant_name;
  DBUG_ENTER("handle_grant_struct");
  DBUG_PRINT("info",("scan struct: %u  search: '%s'@'%s'",
                     struct_no, user_from->user.str, user_from->host.str));

  LINT_INIT(acl_user);
  LINT_INIT(acl_db);
  LINT_INIT(grant_name);

  /* Get the number of elements in the in-memory structure. */
  switch (struct_no) {
  case 0:
    elements= acl_users.elements;
    break;
  case 1:
    elements= acl_dbs.elements;
    break;
  case 2:
    elements= column_priv_hash.records;
    break;
  case 3:
    elements= proc_priv_hash.records;
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
      Unfortunaltely, the host default differs for the structures.
    */
    switch (struct_no) {
    case 0:
      acl_user= dynamic_element(&acl_users, idx, ACL_USER*);
      user= acl_user->user;
      if (!(host= acl_user->host.hostname))
        host= "%";
      break;

    case 1:
      acl_db= dynamic_element(&acl_dbs, idx, ACL_DB*);
      user= acl_db->user;
      if (!(host= acl_db->host.hostname))
        host= "%";
      break;

    case 2:
      grant_name= (GRANT_NAME*) hash_element(&column_priv_hash, idx);
      user= grant_name->user;
      if (!(host= grant_name->host.hostname))
        host= "%";
      break;

    case 3:
      grant_name= (GRANT_NAME*) hash_element(&proc_priv_hash, idx);
      user= grant_name->user;
      if (!(host= grant_name->host.hostname))
        host= "%";
      break;
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
      switch ( struct_no )
      {
      case 0:
        delete_dynamic_element(&acl_users, idx);
        break;

      case 1:
        delete_dynamic_element(&acl_dbs, idx);
        break;

      case 2:
        hash_delete(&column_priv_hash, (byte*) grant_name);
	break;

      case 3:
        hash_delete(&proc_priv_hash, (byte*) grant_name);
	break;
      }
      elements--;
      idx--;
    }
    else if ( user_to )
    {
      switch ( struct_no ) {
      case 0:
        acl_user->user= strdup_root(&mem, user_to->user.str);
        acl_user->host.hostname= strdup_root(&mem, user_to->host.str);
        break;

      case 1:
        acl_db->user= strdup_root(&mem, user_to->user.str);
        acl_db->host.hostname= strdup_root(&mem, user_to->host.str);
        break;

      case 2:
      case 3:
        grant_name->user= strdup_root(&mem, user_to->user.str);
        update_hostname(&grant_name->host,
                        strdup_root(&mem, user_to->host.str));
	break;
      }
    }
    else
    {
      /* If search is requested, we do not need to search further. */
      break;
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
    if ((handle_grant_struct(0, drop, user_from, user_to) && ! result) ||
        found)
    {
      result= 1; /* At least one record/element found. */
      /* If search is requested, we do not need to search further. */
      if (! drop && ! user_to)
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
    if (((handle_grant_struct(1, drop, user_from, user_to) && ! result) ||
         found) && ! result)
    {
      result= 1; /* At least one record/element found. */
      /* If search is requested, we do not need to search further. */
      if (! drop && ! user_to)
        goto end;
    }
  }

  /* Handle procedures table. */
  if ((found= handle_grant_table(tables, 4, drop, user_from, user_to)) < 0)
  {
    /* Handle of table failed, don't touch in-memory array. */
    result= -1;
  }
  else
  {
    /* Handle procs array. */
    if (((handle_grant_struct(3, drop, user_from, user_to) && ! result) ||
         found) && ! result)
    {
      result= 1; /* At least one record/element found. */
      /* If search is requested, we do not need to search further. */
      if (! drop && ! user_to)
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
      if (((handle_grant_struct(2, drop, user_from, user_to) && ! result) ||
           found) && ! result)
        result= 1; /* At least one record/element found. */
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
  str->append("'@'");
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
  ulong sql_mode;
  LEX_USER *user_name;
  List_iterator <LEX_USER> user_list(list);
  TABLE_LIST tables[GRANT_TABLES];
  DBUG_ENTER("mysql_create_user");

  /* CREATE USER may be skipped on replication client. */
  if ((result= open_grant_tables(thd, tables)))
    DBUG_RETURN(result != 1);

  rw_wrlock(&LOCK_grant);
  VOID(pthread_mutex_lock(&acl_cache->lock));

  while ((user_name= user_list++))
  {
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

    sql_mode= thd->variables.sql_mode;
    if (replace_user_table(thd, tables[0].table, *user_name, 0, 0, 1, 0))
    {
      append_user(&wrong_users, user_name);
      result= TRUE;
    }
  }

  VOID(pthread_mutex_unlock(&acl_cache->lock));
  rw_unlock(&LOCK_grant);
  close_thread_tables(thd);
  if (result)
    my_error(ER_CANNOT_USER, MYF(0), "CREATE USER", wrong_users.c_ptr_safe());
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
  LEX_USER *user_name;
  List_iterator <LEX_USER> user_list(list);
  TABLE_LIST tables[GRANT_TABLES];
  DBUG_ENTER("mysql_drop_user");

  /* DROP USER may be skipped on replication client. */
  if ((result= open_grant_tables(thd, tables)))
    DBUG_RETURN(result != 1);

  rw_wrlock(&LOCK_grant);
  VOID(pthread_mutex_lock(&acl_cache->lock));

  while ((user_name= user_list++))
  {
    if (handle_grant_data(tables, 1, user_name, NULL) <= 0)
    {
      append_user(&wrong_users, user_name);
      result= TRUE;
    }
  }

  VOID(pthread_mutex_unlock(&acl_cache->lock));
  rw_unlock(&LOCK_grant);
  close_thread_tables(thd);
  if (result)
    my_error(ER_CANNOT_USER, MYF(0), "DROP USER", wrong_users.c_ptr_safe());
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
  int result= 0;
  String wrong_users;
  LEX_USER *user_from;
  LEX_USER *user_to;
  List_iterator <LEX_USER> user_list(list);
  TABLE_LIST tables[GRANT_TABLES];
  DBUG_ENTER("mysql_rename_user");

  /* RENAME USER may be skipped on replication client. */
  if ((result= open_grant_tables(thd, tables)))
    DBUG_RETURN(result != 1);

  rw_wrlock(&LOCK_grant);
  VOID(pthread_mutex_lock(&acl_cache->lock));

  while ((user_from= user_list++))
  {
    user_to= user_list++;
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
    }
  }

  VOID(pthread_mutex_unlock(&acl_cache->lock));
  rw_unlock(&LOCK_grant);
  close_thread_tables(thd);
  if (result)
    my_error(ER_CANNOT_USER, MYF(0), "RENAME USER", wrong_users.c_ptr_safe());
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
  DBUG_ENTER("mysql_revoke_all");

  if ((result= open_grant_tables(thd, tables)))
    DBUG_RETURN(result != 1);

  rw_wrlock(&LOCK_grant);
  VOID(pthread_mutex_lock(&acl_cache->lock));

  LEX_USER *lex_user;
  List_iterator <LEX_USER> user_list(list);
  while ((lex_user=user_list++))
  {
    if (!check_acl_user(lex_user, &counter))
    {
      sql_print_error("REVOKE ALL PRIVILEGES, GRANT: User '%s'@'%s' does not "
                      "exists", lex_user->user.str, lex_user->host.str);
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
	    !my_strcasecmp(system_charset_info, lex_user->host.str, host))
	{
	  if (!replace_db_table(tables[1].table, acl_db->db, *lex_user, ~(ulong)0, 1))
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
	    !my_strcasecmp(system_charset_info, lex_user->host.str, host))
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
	    !my_strcasecmp(system_charset_info, lex_user->host.str, host))
	{
	  if (!replace_routine_table(thd,grant_proc,tables[4].table,*lex_user,
				  grant_proc->db,
				  grant_proc->tname,
                                  is_proc,
				  ~(ulong)0, 1))
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
  rw_unlock(&LOCK_grant);
  close_thread_tables(thd);

  if (result)
    my_message(ER_REVOKE_GRANTS, ER(ER_REVOKE_GRANTS), MYF(0));

  DBUG_RETURN(result);
}


/*
  Revoke privileges for all users on a stored procedure

  SYNOPSIS
    sp_revoke_privileges()
    thd                         The current thread.
    db				DB of the stored procedure
    name			Name of the stored procedure

  RETURN
    0           OK.
    < 0         Error. Error message not yet sent.
*/

bool sp_revoke_privileges(THD *thd, const char *sp_db, const char *sp_name,
                          bool is_proc)
{
  uint counter, revoked;
  int result;
  TABLE_LIST tables[GRANT_TABLES];
  HASH *hash= is_proc ? &proc_priv_hash : &func_priv_hash;
  DBUG_ENTER("sp_revoke_privileges");

  if ((result= open_grant_tables(thd, tables)))
    DBUG_RETURN(result != 1);

  rw_wrlock(&LOCK_grant);
  VOID(pthread_mutex_lock(&acl_cache->lock));

  /* Remove procedure access */
  do
  {
    for (counter= 0, revoked= 0 ; counter < hash->records ; )
    {
      GRANT_NAME *grant_proc= (GRANT_NAME*) hash_element(hash, counter);
      if (!my_strcasecmp(system_charset_info, grant_proc->db, sp_db) &&
	  !my_strcasecmp(system_charset_info, grant_proc->tname, sp_name))
      {
        LEX_USER lex_user;
	lex_user.user.str= grant_proc->user;
	lex_user.user.length= strlen(grant_proc->user);
	lex_user.host.str= grant_proc->host.hostname;
	lex_user.host.length= strlen(grant_proc->host.hostname);
	if (!replace_routine_table(thd,grant_proc,tables[4].table,lex_user,
				   grant_proc->db, grant_proc->tname,
                                   is_proc, ~(ulong)0, 1))
	{
	  revoked= 1;
	  continue;
	}
	result= -1;	// Something went wrong
      }
      counter++;
    }
  } while (revoked);

  VOID(pthread_mutex_unlock(&acl_cache->lock));
  rw_unlock(&LOCK_grant);
  close_thread_tables(thd);

  if (result)
    my_message(ER_REVOKE_GRANTS, ER(ER_REVOKE_GRANTS), MYF(0));

  DBUG_RETURN(result);
}


/*
  Grant EXECUTE,ALTER privilege for a stored procedure

  SYNOPSIS
    sp_grant_privileges()
    thd                         The current thread.
    db				DB of the stored procedure
    name			Name of the stored procedure

  RETURN
    0           OK.
    < 0         Error. Error message not yet sent.
*/

bool sp_grant_privileges(THD *thd, const char *sp_db, const char *sp_name,
                         bool is_proc)
{
  Security_context *sctx= thd->security_ctx;
  LEX_USER *combo;
  TABLE_LIST tables[1];
  List<LEX_USER> user_list;
  bool result;
  DBUG_ENTER("sp_grant_privileges");

  if (!(combo=(LEX_USER*) thd->alloc(sizeof(st_lex_user))))
    DBUG_RETURN(TRUE);

  combo->user.str= sctx->user;
  
  if (!find_acl_user(combo->host.str=(char*)sctx->host_or_ip, combo->user.str,
                     FALSE) &&
      !find_acl_user(combo->host.str=(char*)sctx->host, combo->user.str,
                     FALSE) &&
      !find_acl_user(combo->host.str=(char*)sctx->ip, combo->user.str,
                     FALSE) &&
      !find_acl_user(combo->host.str=(char*)"%", combo->user.str, FALSE))
    DBUG_RETURN(TRUE);

  bzero((char*)tables, sizeof(TABLE_LIST));
  user_list.empty();

  tables->db= (char*)sp_db;
  tables->table_name= tables->alias= (char*)sp_name;
  
  combo->host.length= strlen(combo->host.str);
  combo->user.length= strlen(combo->user.str);
  combo->host.str= thd->strmake(combo->host.str,combo->host.length);
  combo->user.str= thd->strmake(combo->user.str,combo->user.length);
  combo->password.str= (char*)"";
  combo->password.length= 0;

  if (user_list.push_back(combo))
    DBUG_RETURN(TRUE);

  thd->lex->ssl_type= SSL_TYPE_NOT_SPECIFIED;
  bzero((char*) &thd->lex->mqh, sizeof(thd->lex->mqh));

  result= mysql_routine_grant(thd, tables, is_proc, user_list,
  				DEFAULT_CREATE_PROC_ACLS, 0, 1);
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


void update_schema_privilege(TABLE *table, char *buff, const char* db,
                             const char* t_name, const char* column,
                             uint col_length, const char *priv, 
                             uint priv_length, const char* is_grantable)
{
  int i= 2;
  CHARSET_INFO *cs= system_charset_info;
  restore_record(table, s->default_values);
  table->field[0]->store(buff, strlen(buff), cs);
  if (db)
    table->field[i++]->store(db, strlen(db), cs);
  if (t_name)
    table->field[i++]->store(t_name, strlen(t_name), cs);
  if (column)
    table->field[i++]->store(column, col_length, cs);
  table->field[i++]->store(priv, priv_length, cs);
  table->field[i]->store(is_grantable, strlen(is_grantable), cs);
  table->file->write_row(table->record[0]);
}


int fill_schema_user_privileges(THD *thd, TABLE_LIST *tables, COND *cond)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  uint counter;
  ACL_USER *acl_user;
  ulong want_access;
  char buff[100];
  TABLE *table= tables->table;
  bool no_global_access= check_access(thd, SELECT_ACL, "mysql",0,1,1,0);
  char *curr_host= thd->security_ctx->priv_host_name();
  DBUG_ENTER("fill_schema_user_privileges");

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
      update_schema_privilege(table, buff, 0, 0, 0, 0, "USAGE", 5, is_grantable);
    else
    {
      uint priv_id;
      ulong j,test_access= want_access & ~GRANT_ACL;
      for (priv_id=0, j = SELECT_ACL;j <= GLOBAL_ACLS; priv_id++,j <<= 1)
      {
	if (test_access & j)
          update_schema_privilege(table, buff, 0, 0, 0, 0, 
                                  command_array[priv_id],
                                  command_lengths[priv_id], is_grantable);
      }
    }
  }
  DBUG_RETURN(0);
#else
  return(0);
#endif
}


int fill_schema_schema_privileges(THD *thd, TABLE_LIST *tables, COND *cond)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  uint counter;
  ACL_DB *acl_db;
  ulong want_access;
  char buff[100];
  TABLE *table= tables->table;
  bool no_global_access= check_access(thd, SELECT_ACL, "mysql",0,1,1,0);
  char *curr_host= thd->security_ctx->priv_host_name();
  DBUG_ENTER("fill_schema_schema_privileges");

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
        update_schema_privilege(table, buff, acl_db->db, 0, 0,
                                0, "USAGE", 5, is_grantable);
      else
      {
        int cnt;
        ulong j,test_access= want_access & ~GRANT_ACL;
        for (cnt=0, j = SELECT_ACL; j <= DB_ACLS; cnt++,j <<= 1)
          if (test_access & j)
            update_schema_privilege(table, buff, acl_db->db, 0, 0, 0,
                                    command_array[cnt], command_lengths[cnt],
                                    is_grantable);
      }
    }
  }
  DBUG_RETURN(0);
#else
  return (0);
#endif
}


int fill_schema_table_privileges(THD *thd, TABLE_LIST *tables, COND *cond)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  uint index;
  char buff[100];
  TABLE *table= tables->table;
  bool no_global_access= check_access(thd, SELECT_ACL, "mysql",0,1,1,0);
  char *curr_host= thd->security_ctx->priv_host_name();
  DBUG_ENTER("fill_schema_table_privileges");

  for (index=0 ; index < column_priv_hash.records ; index++)
  {
    const char *user, *is_grantable= "YES";
    GRANT_TABLE *grant_table= (GRANT_TABLE*) hash_element(&column_priv_hash,
							  index);
    if (!(user=grant_table->user))
      user= "";

    if (no_global_access &&
        (strcmp(thd->security_ctx->priv_user, user) ||
         my_strcasecmp(system_charset_info, curr_host,
                       grant_table->host.hostname)))
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

      strxmov(buff,"'",user,"'@'",grant_table->host.hostname,"'",NullS);
      if (!test_access)
        update_schema_privilege(table, buff, grant_table->db, grant_table->tname,
                                0, 0, "USAGE", 5, is_grantable);
      else
      {
        ulong j;
        int cnt;
        for (cnt= 0, j= SELECT_ACL; j <= TABLE_ACLS; cnt++, j<<= 1)
        {
          if (test_access & j)
            update_schema_privilege(table, buff, grant_table->db, 
                                    grant_table->tname, 0, 0, command_array[cnt],
                                    command_lengths[cnt], is_grantable);
        }
      }
    }
  }
  DBUG_RETURN(0);
#else
  return (0);
#endif
}


int fill_schema_column_privileges(THD *thd, TABLE_LIST *tables, COND *cond)
{
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  uint index;
  char buff[100];
  TABLE *table= tables->table;
  bool no_global_access= check_access(thd, SELECT_ACL, "mysql",0,1,1,0);
  char *curr_host= thd->security_ctx->priv_host_name();
  DBUG_ENTER("fill_schema_table_privileges");

  for (index=0 ; index < column_priv_hash.records ; index++)
  {
    const char *user, *is_grantable= "YES";
    GRANT_TABLE *grant_table= (GRANT_TABLE*) hash_element(&column_priv_hash,
							  index);
    if (!(user=grant_table->user))
      user= "";

    if (no_global_access &&
        (strcmp(thd->security_ctx->priv_user, user) ||
         my_strcasecmp(system_charset_info, curr_host,
                       grant_table->host.hostname)))
      continue;

    ulong table_access= grant_table->cols;
    if (table_access != 0)
    {
      if (!(grant_table->privs & GRANT_ACL))
        is_grantable= "NO";

      ulong test_access= table_access & ~GRANT_ACL;
      strxmov(buff,"'",user,"'@'",grant_table->host.hostname,"'",NullS);
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
                  update_schema_privilege(table, buff, grant_table->db,
                                          grant_table->tname,
                                          grant_column->column,
                                          grant_column->key_length,
                                          command_array[cnt],
                                          command_lengths[cnt], is_grantable);
            }
          }
        }
      }
    }
  }
  DBUG_RETURN(0);
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
  /* --skip-grants */
  if (!initialized)
  {
    grant->privilege= ~NO_ACCESS;             // everything is allowed
    return;
  }

  /* global privileges */
  grant->privilege= sctx->master_access;

  if (!sctx->priv_user)
    return;                                   // it is slave

  /* db privileges */
  grant->privilege|= acl_get(sctx->host, sctx->ip, sctx->priv_user, db, 0);

  if (!grant_option)
    return;

  /* table privileges */
  if (grant->version != grant_version)
  {
    rw_rdlock(&LOCK_grant);
    grant->grant_table=
      table_hash_search(sctx->host, sctx->ip, db,
			sctx->priv_user,
			table, 0);              /* purecov: inspected */
    grant->version= grant_version;              /* purecov: inspected */
    rw_unlock(&LOCK_grant);
  }
  if (grant->grant_table != 0)
  {
    grant->privilege|= grant->grant_table->privs;
  }
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
