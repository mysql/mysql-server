/*
   Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


/* create and drop of databases */

#include "my_global.h"                          /* NO_EMBEDDED_ACCESS_CHECKS */
#include "sql_db.h"
#include "sql_cache.h"                   // query_cache_*
#include "lock.h"                        // lock_schema_name
#include "sql_table.h"                   // build_table_filename,
                                         // filename_to_tablename
#include "sql_rename.h"                  // mysql_rename_tables
#include "auth_common.h"                 // acl_get, check_grant_db
                                         // SELECT_ACL, DB_ACLS,
#include "log_event.h"                   // Query_log_event
#include "sql_base.h"                    // lock_table_names, tdc_remove_table
#include "sql_handler.h"                 // mysql_ha_rm_tables
#include <mysys_err.h>
#include "sp.h"
#include "events.h"
#include <my_dir.h>
#include <m_ctype.h>
#include "log.h"
#include "binlog.h"                             // mysql_bin_log
#include "log_event.h"
#ifdef _WIN32
#include <direct.h>
#endif
#include "debug_sync.h"

#include "pfs_file_provider.h"
#include "mysql/psi/mysql_file.h"

#define MAX_DROP_TABLE_Q_LEN      1024

const char *del_exts[]= {".frm", ".BAK", ".TMD", ".opt", ".OLD", ".cfg", NullS};
static TYPELIB deletable_extentions=
{array_elements(del_exts)-1,"del_exts", del_exts, NULL};

static bool find_db_tables_and_rm_known_files(THD *thd, MY_DIR *dirp,
                                              const char *db,
                                              const char *path,
                                              TABLE_LIST **tables,
                                              bool *found_other_files);

long mysql_rm_arc_files(THD *thd, MY_DIR *dirp, const char *org_path);
static my_bool rm_dir_w_symlink(const char *org_path, my_bool send_error);
static void mysql_change_db_impl(THD *thd,
                                 const LEX_CSTRING &new_db_name,
                                 ulong new_db_access,
                                 const CHARSET_INFO *new_db_charset);


/* Database options hash */
static HASH dboptions;
static my_bool dboptions_init= 0;
static mysql_rwlock_t LOCK_dboptions;

/* Structure for database options */
typedef struct my_dbopt_st
{
  char *name;			/* Database name                  */
  uint name_length;		/* Database length name           */
  const CHARSET_INFO *charset;	/* Database default character set */
} my_dbopt_t;


/*
  Function we use in the creation of our hash to get key.
*/

extern "C" uchar* dboptions_get_key(my_dbopt_t *opt, size_t *length,
                                    my_bool not_used);

uchar* dboptions_get_key(my_dbopt_t *opt, size_t *length,
                         my_bool not_used MY_ATTRIBUTE((unused)))
{
  *length= opt->name_length;
  return (uchar*) opt->name;
}


/*
  Helper function to write a query to binlog used by mysql_rm_db()
*/

static inline int write_to_binlog(THD *thd, char *query, size_t q_len,
                                  const char *db, size_t db_len)
{
  Query_log_event qinfo(thd, query, q_len, FALSE, TRUE, FALSE, 0);
  qinfo.db= db;
  qinfo.db_len= db_len;
  int error= mysql_bin_log.write_event(&qinfo);
  if (!error)
    error= mysql_bin_log.commit(thd, false);
  return error;
} 


/*
  Function to free dboptions hash element
*/

extern "C" void free_dbopt(void *dbopt);

void free_dbopt(void *dbopt)
{
  my_free(dbopt);
}

#ifdef HAVE_PSI_INTERFACE
static PSI_rwlock_key key_rwlock_LOCK_dboptions;

static PSI_rwlock_info all_database_names_rwlocks[]=
{
  { &key_rwlock_LOCK_dboptions, "LOCK_dboptions", PSI_FLAG_GLOBAL}
};

static void init_database_names_psi_keys(void)
{
  const char* category= "sql";
  int count;

  count= array_elements(all_database_names_rwlocks);
  mysql_rwlock_register(category, all_database_names_rwlocks, count);
}
#endif

/**
  Initialize database option cache.

  @note Must be called before any other database function is called.

  @retval  0	ok
  @retval  1	Fatal error
*/

bool my_dboptions_cache_init(void)
{
#ifdef HAVE_PSI_INTERFACE
  init_database_names_psi_keys();
#endif

  bool error= 0;
  mysql_rwlock_init(key_rwlock_LOCK_dboptions, &LOCK_dboptions);
  if (!dboptions_init)
  {
    dboptions_init= 1;
    /*
      For lower_case_table_names=0 we should use case sensitive search
      (my_charset_bin), for 1 and 2 - case insensitive (system_charset_info).
    */
    error= my_hash_init(&dboptions, lower_case_table_names ?
                        system_charset_info : &my_charset_bin,
                        32, 0, 0, (my_hash_get_key) dboptions_get_key,
                        free_dbopt, 0,
                        key_memory_dboptions_hash);
  }
  return error;
}



/**
  Free database option hash and locked databases hash.
*/

void my_dboptions_cache_free(void)
{
  if (dboptions_init)
  {
    dboptions_init= 0;
    my_hash_free(&dboptions);
    mysql_rwlock_destroy(&LOCK_dboptions);
  }
}


/**
  Cleanup cached options.
*/

void my_dbopt_cleanup(void)
{
  mysql_rwlock_wrlock(&LOCK_dboptions);
  my_hash_free(&dboptions);
  /*
    For lower_case_table_names=0 we should use case sensitive search
    (my_charset_bin), for 1 and 2 - case insensitive (system_charset_info).
  */
  my_hash_init(&dboptions, lower_case_table_names ? 
               system_charset_info : &my_charset_bin,
               32, 0, 0, (my_hash_get_key) dboptions_get_key,
               free_dbopt, 0,
               key_memory_dboptions_hash);
  mysql_rwlock_unlock(&LOCK_dboptions);
}


/*
  Find database options in the hash.
  
  DESCRIPTION
    Search a database options in the hash, usings its path.
    Fills "create" on success.
  
  RETURN VALUES
    0 on success.
    1 on error.
*/

static my_bool get_dbopt(const char *dbname, HA_CREATE_INFO *create)
{
  my_dbopt_t *opt;
  size_t length;
  my_bool error= 1;

  length= strlen(dbname);

  mysql_rwlock_rdlock(&LOCK_dboptions);
  if ((opt= (my_dbopt_t*) my_hash_search(&dboptions, (uchar*) dbname, length)))
  {
    create->default_table_charset= opt->charset;
    error= 0;
  }
  mysql_rwlock_unlock(&LOCK_dboptions);
  return error;
}


/*
  Writes database options into the hash.
  
  DESCRIPTION
    Inserts database options into the hash, or updates
    options if they are already in the hash.
  
  RETURN VALUES
    0 on success.
    1 on error.
*/

static my_bool put_dbopt(const char *dbname, HA_CREATE_INFO *create)
{
  my_dbopt_t *opt;
  size_t length;
  my_bool error= 0;
  DBUG_ENTER("put_dbopt");

  length= strlen(dbname);

  mysql_rwlock_wrlock(&LOCK_dboptions);
  if (!(opt= (my_dbopt_t*) my_hash_search(&dboptions, (uchar*) dbname,
                                          length)))
  { 
    /* Options are not in the hash, insert them */
    char *tmp_name;
    if (!my_multi_malloc(key_memory_dboptions_hash,
                         MYF(MY_WME | MY_ZEROFILL),
                         &opt, sizeof(*opt), &tmp_name, length+1,
                         NullS))
    {
      error= 1;
      goto end;
    }
    
    opt->name= tmp_name;
    my_stpcpy(opt->name, dbname);
    opt->name_length= length;
    
    if ((error= my_hash_insert(&dboptions, (uchar*) opt)))
    {
      my_free(opt);
      goto end;
    }
  }

  /* Update / write options in hash */
  opt->charset= create->default_table_charset;

end:
  mysql_rwlock_unlock(&LOCK_dboptions);
  DBUG_RETURN(error);
}


/*
  Deletes database options from the hash.
*/

static void del_dbopt(const char *path)
{
  my_dbopt_t *opt;
  mysql_rwlock_wrlock(&LOCK_dboptions);
  if ((opt= (my_dbopt_t *)my_hash_search(&dboptions, (const uchar*) path,
                                         strlen(path))))
    my_hash_delete(&dboptions, (uchar*) opt);
  mysql_rwlock_unlock(&LOCK_dboptions);
}


/*
  Create database options file:

  DESCRIPTION
    Currently database default charset is only stored there.

  RETURN VALUES
  0	ok
  1	Could not create file or write to it.  Error sent through my_error()
*/

static bool write_db_opt(THD *thd, const char *path, HA_CREATE_INFO *create)
{
  File file;
  char buf[256]; // Should be enough for one option
  bool error=1;

  if (!create->default_table_charset)
    create->default_table_charset= thd->variables.collation_server;

  if (put_dbopt(path, create))
    return 1;

  if ((file= mysql_file_create(key_file_dbopt, path, CREATE_MODE,
                               O_RDWR | O_TRUNC, MYF(MY_WME))) >= 0)
  {
    ulong length;
    length= (ulong) (strxnmov(buf, sizeof(buf)-1, "default-character-set=",
                              create->default_table_charset->csname,
                              "\ndefault-collation=",
                              create->default_table_charset->name,
                              "\n", NullS) - buf);

    /* Error is written by mysql_file_write */
    if (!mysql_file_write(file, (uchar*) buf, length, MYF(MY_NABP+MY_WME)))
      error=0;
    mysql_file_close(file, MYF(0));
  }
  return error;
}


/*
  Load database options file

  load_db_opt()
  path		Path for option file
  create	Where to store the read options

  DESCRIPTION

  RETURN VALUES
  0	File found
  1	No database file or could not open it

*/

bool load_db_opt(THD *thd, const char *path, HA_CREATE_INFO *create)
{
  File file;
  char buf[256];
  DBUG_ENTER("load_db_opt");
  bool error=1;
  uint nbytes;

  new (create) HA_CREATE_INFO;
  create->default_table_charset= thd->variables.collation_server;

  /* Check if options for this database are already in the hash */
  if (!get_dbopt(path, create))
    DBUG_RETURN(0);

  /* Otherwise, load options from the .opt file */
  if ((file= mysql_file_open(key_file_dbopt,
                             path, O_RDONLY | O_SHARE, MYF(0))) < 0)
    goto err1;

  IO_CACHE cache;
  if (init_io_cache(&cache, file, IO_SIZE, READ_CACHE, 0, 0, MYF(0)))
    goto err2;

  while ((int) (nbytes= my_b_gets(&cache, (char*) buf, sizeof(buf))) > 0)
  {
    char *pos= buf+nbytes-1;
    /* Remove end space and control characters */
    while (pos > buf && !my_isgraph(&my_charset_latin1, pos[-1]))
      pos--;
    *pos=0;
    if ((pos= strchr(buf, '=')))
    {
      if (!strncmp(buf,"default-character-set", (pos-buf)))
      {
        /*
           Try character set name, and if it fails
           try collation name, probably it's an old
           4.1.0 db.opt file, which didn't have
           separate default-character-set and
           default-collation commands.
        */
        if (!(create->default_table_charset=
        get_charset_by_csname(pos+1, MY_CS_PRIMARY, MYF(0))) &&
            !(create->default_table_charset=
              get_charset_by_name(pos+1, MYF(0))))
        {
          sql_print_error("Error while loading database options: '%s':",path);
          sql_print_error(ER(ER_UNKNOWN_CHARACTER_SET),pos+1);
          create->default_table_charset= default_charset_info;
        }
      }
      else if (!strncmp(buf,"default-collation", (pos-buf)))
      {
        if (!(create->default_table_charset= get_charset_by_name(pos+1,
                                                           MYF(0))))
        {
          sql_print_error("Error while loading database options: '%s':",path);
          sql_print_error(ER(ER_UNKNOWN_COLLATION),pos+1);
          create->default_table_charset= default_charset_info;
        }
      }
    }
  }
  /*
    Put the loaded value into the hash.
    Note that another thread could've added the same
    entry to the hash after we called get_dbopt(),
    but it's not an error, as put_dbopt() takes this
    possibility into account.
  */
  error= put_dbopt(path, create);

  end_io_cache(&cache);
err2:
  mysql_file_close(file, MYF(0));
err1:
  DBUG_RETURN(error);
}


/*
  Retrieve database options by name. Load database options file or fetch from
  cache.

  SYNOPSIS
    load_db_opt_by_name()
    db_name         Database name
    db_create_info  Where to store the database options

  DESCRIPTION
    load_db_opt_by_name() is a shortcut for load_db_opt().

  NOTE
    Although load_db_opt_by_name() (and load_db_opt()) returns status of
    the operation, it is useless usually and should be ignored. The problem
    is that there are 1) system databases ("mysql") and 2) virtual
    databases ("information_schema"), which do not contain options file.
    So, load_db_opt[_by_name]() returns FALSE for these databases, but this
    is not an error.

    load_db_opt[_by_name]() clears db_create_info structure in any case, so
    even on failure it contains valid data. So, common use case is just
    call load_db_opt[_by_name]() without checking return value and use
    db_create_info right after that.

  RETURN VALUES (read NOTE!)
    FALSE   Success
    TRUE    Failed to retrieve options
*/

bool load_db_opt_by_name(THD *thd, const char *db_name,
                         HA_CREATE_INFO *db_create_info)
{
  char db_opt_path[FN_REFLEN + 1];

  /*
    Pass an empty file name, and the database options file name as extension
    to avoid table name to file name encoding.
  */
  (void) build_table_filename(db_opt_path, sizeof(db_opt_path) - 1,
                              db_name, "", MY_DB_OPT_FILE, 0);

  return load_db_opt(thd, db_opt_path, db_create_info);
}


/**
  Return default database collation.

  @param thd     Thread context.
  @param db_name Database name.

  @return CHARSET_INFO object. The operation always return valid character
    set, even if the database does not exist.
*/

const CHARSET_INFO *get_default_db_collation(THD *thd, const char *db_name)
{
  HA_CREATE_INFO db_info;

  if (thd->db().str != NULL && strcmp(db_name, thd->db().str) == 0)
    return thd->db_charset;

  load_db_opt_by_name(thd, db_name, &db_info);

  /*
    NOTE: even if load_db_opt_by_name() fails,
    db_info.default_table_charset contains valid character set
    (collation_server). We should not fail if load_db_opt_by_name() fails,
    because it is valid case. If a database has been created just by
    "mkdir", it does not contain db.opt file, but it is valid database.
  */

  return db_info.default_table_charset;
}


/*
  Create a database

  SYNOPSIS
  mysql_create_db()
  thd		Thread handler
  db		Name of database to create
		Function assumes that this is already validated.
  create_info	Database create options (like character set)
  silent	Used by replication when internally creating a database.
		In this case the entry should not be logged.

  SIDE-EFFECTS
   1. Report back to client that command succeeded (my_ok)
   2. Report errors to client
   3. Log event to binary log
   (The 'silent' flags turns off 1 and 3.)

  RETURN VALUES
  FALSE ok
  TRUE  Error

*/

int mysql_create_db(THD *thd, const char *db, HA_CREATE_INFO *create_info,
                     bool silent)
{
  char	 path[FN_REFLEN+16];
  char	 tmp_query[FN_REFLEN+16];
  long result= 1;
  int error= 0;
  MY_STAT stat_info;
  uint create_options= create_info ? create_info->options : 0;
  size_t path_len;
  bool was_truncated;
  DBUG_ENTER("mysql_create_db");

  /* do not create 'information_schema' db */
  if (is_infoschema_db(db))
  {
    my_error(ER_DB_CREATE_EXISTS, MYF(0), db);
    DBUG_RETURN(-1);
  }

  if (lock_schema_name(thd, db))
    DBUG_RETURN(-1);

  /* Check directory */
  path_len= build_table_filename(path, sizeof(path) - 1, db, "", "", 0,
                                 &was_truncated);
  if (was_truncated)
  {
    my_error(ER_IDENT_CAUSES_TOO_LONG_PATH, MYF(0), sizeof(path)-1, path);
    DBUG_RETURN(-1);
  }
  path[path_len-1]= 0;                    // Remove last '/' from path

  if (mysql_file_stat(key_file_misc, path, &stat_info, MYF(0)))
  {
    if (!(create_options & HA_LEX_CREATE_IF_NOT_EXISTS))
    {
      my_error(ER_DB_CREATE_EXISTS, MYF(0), db);
      error= -1;
      goto exit;
    }
    push_warning_printf(thd, Sql_condition::SL_NOTE,
			ER_DB_CREATE_EXISTS, ER(ER_DB_CREATE_EXISTS), db);
    error= 0;
    goto not_silent;
  }
  else
  {
    if (my_errno() != ENOENT)
    {
      char errbuf[MYSYS_STRERROR_SIZE];
      my_error(EE_STAT, MYF(0), path,
               my_errno(), my_strerror(errbuf, sizeof(errbuf), my_errno()));
      goto exit;
    }
    if (my_mkdir(path,0777,MYF(0)) < 0)
    {
      my_error(ER_CANT_CREATE_DB, MYF(0), db, my_errno);
      error= -1;
      goto exit;
    }
  }

  path[path_len-1]= FN_LIBCHAR;
  strmake(path+path_len, MY_DB_OPT_FILE, sizeof(path)-path_len-1);
  if (write_db_opt(thd, path, create_info))
  {
    /*
      Could not create options file.
      Restore things to beginning.
    */
    path[path_len]= 0;
    if (rmdir(path) >= 0)
    {
      error= -1;
      goto exit;
    }
    /*
      We come here when we managed to create the database, but not the option
      file.  In this case it's best to just continue as if nothing has
      happened.  (This is a very unlikely senario)
    */
    thd->clear_error();
  }

not_silent:
  if (!silent)
  {
    const char *query;
    size_t query_length;
    char db_name_quoted[2 * FN_REFLEN + sizeof("create database ") + 2];
    size_t id_len= 0;

    if (!thd->query().str)                          // Only in replication
    {
      id_len= my_strmov_quoted_identifier(thd, (char *) db_name_quoted, db,
                                          0);
      db_name_quoted[id_len]= '\0';
      query= tmp_query;
      query_length=
        static_cast<size_t>(strxmov(tmp_query,"create database ",
                                    db_name_quoted, NullS) - tmp_query);
    }
    else
    {
      query=        thd->query().str;
      query_length= thd->query().length;
    }

    ha_binlog_log_query(thd, 0, LOGCOM_CREATE_DB,
                        query, query_length,
                        db, "");

    if (mysql_bin_log.is_open())
    {
      int errcode= query_error_code(thd, TRUE);
      Query_log_event qinfo(thd, query, query_length, FALSE, TRUE,
			    /* suppress_use */ TRUE, errcode);

      /*
	Write should use the database being created as the "current
        database" and not the threads current database, which is the
        default. If we do not change the "current database" to the
        database being created, the CREATE statement will not be
        replicated when using --binlog-do-db to select databases to be
        replicated. 

	An example (--binlog-do-db=sisyfos):
       
          CREATE DATABASE bob;        # Not replicated
          USE bob;                    # 'bob' is the current database
          CREATE DATABASE sisyfos;    # Not replicated since 'bob' is
                                      # current database.
          USE sisyfos;                # Will give error on slave since
                                      # database does not exist.
      */
      qinfo.db     = db;
      qinfo.db_len = strlen(db);
      thd->add_to_binlog_accessed_dbs(db);
      /*
        These DDL methods and logging are protected with the exclusive
        metadata lock on the schema
      */
      if (mysql_bin_log.write_event(&qinfo))
      {
        error= -1;
        goto exit;
      }
    }
    my_ok(thd, result);
  }

exit:
  DBUG_RETURN(error);
}


/* db-name is already validated when we come here */

bool mysql_alter_db(THD *thd, const char *db, HA_CREATE_INFO *create_info)
{
  char path[FN_REFLEN+16];
  long result=1;
  int error= 0;
  DBUG_ENTER("mysql_alter_db");

  if (lock_schema_name(thd, db))
    DBUG_RETURN(TRUE);

  /* 
     Recreate db options file: /dbpath/.db.opt
     We pass MY_DB_OPT_FILE as "extension" to avoid
     "table name to file name" encoding.
  */
  build_table_filename(path, sizeof(path) - 1, db, "", MY_DB_OPT_FILE, 0);
  if ((error=write_db_opt(thd, path, create_info)))
    goto exit;

  /* Change options if current database is being altered. */

  if (thd->db().str && !strcmp(thd->db().str,db))
  {
    thd->db_charset= create_info->default_table_charset ?
		     create_info->default_table_charset :
		     thd->variables.collation_server;
    thd->variables.collation_database= thd->db_charset;
  }

  ha_binlog_log_query(thd, 0, LOGCOM_ALTER_DB,
                      thd->query().str, thd->query().length,
                      db, "");

  if (mysql_bin_log.is_open())
  {
    int errcode= query_error_code(thd, TRUE); 
    Query_log_event qinfo(thd, thd->query().str, thd->query().length,
                          false, true, /* suppress_use */ true, errcode);
    /*
      Write should use the database being created as the "current
      database" and not the threads current database, which is the
      default.
    */
    qinfo.db     = db;
    qinfo.db_len = strlen(db);

    /*
      These DDL methods and logging are protected with the exclusive
      metadata lock on the schema.
    */
    if ((error= mysql_bin_log.write_event(&qinfo)))
      goto exit;
  }
  my_ok(thd, result);

exit:
  DBUG_RETURN(error);
}


/**
  Drop all tables, routines and events in a database and the database itself.

  @param  thd        Thread handle
  @param  db         Database name in the case given by user
                     It's already validated and set to lower case
                     (if needed) when we come here
  @param  if_exists  Don't give error if database doesn't exists
  @param  silent     Don't write the statement to the binary log and don't
                     send ok packet to the client

  @retval  false  OK (Database dropped)
  @retval  true   Error
*/

bool mysql_rm_db(THD *thd,const LEX_CSTRING &db,bool if_exists, bool silent)
{
  ulong deleted_tables= 0;
  bool error= true;
  char	path[2 * FN_REFLEN + 16];
  MY_DIR *dirp;
  size_t length;
  bool found_other_files= false;
  TABLE_LIST *tables= NULL;
  TABLE_LIST *table;
  Drop_table_error_handler err_handler;
  DBUG_ENTER("mysql_rm_db");


  if (lock_schema_name(thd, db.str))
    DBUG_RETURN(true);

  length= build_table_filename(path, sizeof(path) - 1, db.str, "", "", 0);
  my_stpcpy(path+length, MY_DB_OPT_FILE);		// Append db option file name
  del_dbopt(path);				// Remove dboption hash entry
  path[length]= '\0';				// Remove file name

  /* See if the directory exists */
  if (!(dirp= my_dir(path,MYF(MY_DONT_SORT))))
  {
    if (!if_exists)
    {
      my_error(ER_DB_DROP_EXISTS, MYF(0), db.str);
      DBUG_RETURN(true);
    }
    else
    {
      push_warning_printf(thd, Sql_condition::SL_NOTE,
			  ER_DB_DROP_EXISTS, ER(ER_DB_DROP_EXISTS), db.str);
      error= false;
      goto update_binlog;
    }
  }

  if (find_db_tables_and_rm_known_files(thd, dirp, db.str, path, &tables,
                                        &found_other_files))
    goto exit;

  /*
    Disable drop of enabled log tables, must be done before name locking.
    This check is only needed if we are dropping the "mysql" database.
  */
  if ((my_strcasecmp(system_charset_info, MYSQL_SCHEMA_NAME.str, db.str) == 0))
  {
    for (table= tables; table; table= table->next_local)
    {
      if (query_logger.check_if_log_table(table, true))
      {
        my_error(ER_BAD_LOG_STATEMENT, MYF(0), "DROP");
        goto exit;
      }
    }
  }

  /* Lock all tables and stored routines about to be dropped. */
  if (lock_table_names(thd, tables, NULL, thd->variables.lock_wait_timeout, 0) ||
      lock_db_routines(thd, db.str))
    goto exit;

  /* mysql_ha_rm_tables() requires a non-null TABLE_LIST. */
  if (tables)
    mysql_ha_rm_tables(thd, tables);

  for (table= tables; table; table= table->next_local)
  {
    tdc_remove_table(thd, TDC_RT_REMOVE_ALL, table->db, table->table_name,
                     false);
    deleted_tables++;
  }

  thd->push_internal_handler(&err_handler);
  if (!thd->killed &&
      !(tables &&
        mysql_rm_table_no_locks(thd, tables, true, false, true, true)))
  {
    /*
      We temporarily disable the binary log while dropping the objects
      in the database. Since the DROP DATABASE statement is always
      replicated as a statement, execution of it will drop all objects
      in the database on the slave as well, so there is no need to
      replicate the removal of the individual objects in the database
      as well.

      This is more of a safety precaution, since normally no objects
      should be dropped while the database is being cleaned, but in
      the event that a change in the code to remove other objects is
      made, these drops should still not be logged.

      Notice that the binary log have to be enabled over the call to
      ha_drop_database(), since NDB otherwise detects the binary log
      as disabled and will not log the drop database statement on any
      other connected server.
    */

    ha_drop_database(path);
    tmp_disable_binlog(thd);
    query_cache.invalidate(db.str);
    (void) sp_drop_db_routines(thd, db.str); /* @todo Do not ignore errors */
#ifndef EMBEDDED_LIBRARY
    Events::drop_schema_events(thd, db.str);
#endif
    reenable_binlog(thd);

    /*
      If the directory is a symbolic link, remove the link first, then
      remove the directory the symbolic link pointed at
    */
    if (!found_other_files)
      error= rm_dir_w_symlink(path, true);
  }
  thd->pop_internal_handler();

update_binlog:
  if (!silent && !error)
  {
    const char *query;
    size_t query_length;
    // quoted db name + wraping quote
    char buffer_temp [2 * FN_REFLEN + 2];
    size_t id_len= 0;
    if (!thd->query().str)
    {
      /* The client used the old obsolete mysql_drop_db() call */
      query= path;
      id_len= my_strmov_quoted_identifier(thd, buffer_temp, db.str, db.length);
      buffer_temp[id_len] ='\0';
      query_length=
        static_cast<size_t>(strxmov(path, "DROP DATABASE ", buffer_temp, "",
                                    NullS) - path);
    }
    else
    {
      query= thd->query().str;
      query_length= thd->query().length;
    }
    if (mysql_bin_log.is_open())
    {
      int errcode= query_error_code(thd, TRUE);
      Query_log_event qinfo(thd, query, query_length, FALSE, TRUE,
			    /* suppress_use */ TRUE, errcode);
      /*
        Write should use the database being created as the "current
        database" and not the threads current database, which is the
        default.
      */
      qinfo.db     = db.str;
      qinfo.db_len = db.length;

      /*
        These DDL methods and logging are protected with the exclusive
        metadata lock on the schema.
      */
      if (mysql_bin_log.write_event(&qinfo))
      {
        error= true;
        goto exit;
      }
    }
    thd->clear_error();
    thd->server_status|= SERVER_STATUS_DB_DROPPED;
    my_ok(thd, deleted_tables);
  }
  else if (mysql_bin_log.is_open() && !silent)
  {
    char *query, *query_pos, *query_end, *query_data_start;
    char temp_identifier[ 2 * FN_REFLEN + 2];
    TABLE_LIST *tbl;
    size_t id_length=0;

    /*
      If GTID_NEXT=='UUID:NUMBER', we must not log an incomplete
      statement.  However, the incomplete DROP has already 'committed'
      (some tables were removed).  So we generate an error and let
      user fix the situation.
    */
    if (thd->variables.gtid_next.type == GTID_GROUP)
    {
      char gtid_buf[Gtid::MAX_TEXT_LENGTH + 1];
      thd->variables.gtid_next.gtid.to_string(global_sid_map, gtid_buf,
                                              true);
      my_error(ER_CANNOT_LOG_PARTIAL_DROP_DATABASE_WITH_GTID, MYF(0),
               path, gtid_buf, db.str);
      error= true;
      goto exit;
    }

    DBUG_PRINT("info", ("DROP DATABASE failed; generating DROP TABLE statement(s) in the binlog"));

    if (!(query= (char*) thd->alloc(MAX_DROP_TABLE_Q_LEN)))
      // @todo: abort on out of memory instead
      goto exit; /* not much else we can do */
    query_pos= query_data_start= my_stpcpy(query,"DROP TABLE IF EXISTS ");
    query_end= query + MAX_DROP_TABLE_Q_LEN;

    for (tbl= tables; tbl; tbl= tbl->next_local)
    {
      size_t tbl_name_len;
      bool exists;

      // Only write drop table to the binlog for tables that no longer exist.
      if (check_if_table_exists(thd, tbl, &exists))
      {
        error= true;
        goto exit;
      }
      if (exists)
        continue;

      /* 3 for the quotes and the comma*/
      tbl_name_len= strlen(tbl->table_name) + 3;
      if (query_pos + tbl_name_len + 1 >= query_end)
      {
        DBUG_PRINT("info", ("Need multiple DROP TABLE statements in the binlog"));
        thd->variables.gtid_next.dbug_print("gtid_next", true);
        /*
          These DDL methods and logging are protected with the exclusive
          metadata lock on the schema.
        */

        thd->is_commit_in_middle_of_statement= true;
        int ret= write_to_binlog(thd, query, query_pos -1 - query, db.str,
                                 db.length);
        thd->is_commit_in_middle_of_statement= false;
        if (ret)
        {
          error= true;
          goto exit;
        }
        query_pos= query_data_start;
      }
      id_length= my_strmov_quoted_identifier(thd, (char *)temp_identifier,
                                      tbl->table_name, 0);
      temp_identifier[id_length]= '\0';
      query_pos= my_stpcpy(query_pos,(char *)&temp_identifier);
      *query_pos++ = ',';
    }

    if (query_pos != query_data_start)
    {
      thd->add_to_binlog_accessed_dbs(db.str);
      /*
        These DDL methods and logging are protected with the exclusive
        metadata lock on the schema.
      */
      if (write_to_binlog(thd, query, query_pos -1 - query, db.str,
                          db.length))
      {
        error= true;
        goto exit;
      }
    }
  }

  /*
    We have postponed generating the error until now, since if the
    error ER_CANNOT_LOG_PARTIAL_DROP_DATABASE_WITH_GTID occurs we
    should report that instead.
   */
  if (found_other_files)
  {
    my_error(ER_DB_DROP_RMDIR, MYF(0), path, EEXIST);
    error= true;
    goto exit;
  }

exit:
  /*
    If this database was the client's selected database, we silently
    change the client's selected database to nothing (to have an empty
    SELECT DATABASE() in the future). For this we free() thd->db and set
    it to 0.
  */
  if (thd->db().str && !strcmp(thd->db().str, db.str) && !error)
  {
    mysql_change_db_impl(thd, NULL_CSTR, 0, thd->variables.collation_server);
    /*
      Check if current database tracker is enabled. If so, set the 'changed' flag.
    */
    if (thd->session_tracker.get_tracker(CURRENT_SCHEMA_TRACKER)->is_enabled())
    {
      LEX_CSTRING dummy= { C_STRING_WITH_LEN("") };
      dummy.length= dummy.length*1;
      thd->session_tracker.get_tracker(CURRENT_SCHEMA_TRACKER)->mark_as_changed(thd, &dummy);
    }
  }
  my_dirend(dirp);
  DBUG_RETURN(error);
}


static bool find_db_tables_and_rm_known_files(THD *thd, MY_DIR *dirp,
                                              const char *db,
                                              const char *path,
                                              TABLE_LIST **tables,
                                              bool *found_other_files)
{
  char filePath[FN_REFLEN];
  TABLE_LIST *tot_list=0, **tot_list_next_local, **tot_list_next_global;
  DBUG_ENTER("find_db_tables_and_rm_known_files");
  DBUG_PRINT("enter",("path: %s", path));
  TYPELIB *known_extensions= ha_known_exts();

  tot_list_next_local= tot_list_next_global= &tot_list;

  for (uint idx=0 ;
       idx < dirp->number_off_files && !thd->killed ;
       idx++)
  {
    FILEINFO *file=dirp->dir_entry+idx;
    char *extension;
    DBUG_PRINT("info",("Examining: %s", file->name));

    /* skiping . and .. */
    if (file->name[0] == '.' && (!file->name[1] ||
       (file->name[1] == '.' &&  !file->name[2])))
      continue;

    if (file->name[0] == 'a' && file->name[1] == 'r' &&
             file->name[2] == 'c' && file->name[3] == '\0')
    {
      /* .frm archive:
        Those archives are obsolete, but following code should
        exist to remove existent "arc" directories.
      */
      char newpath[FN_REFLEN];
      MY_DIR *new_dirp;
      strxmov(newpath, path, "/", "arc", NullS);
      (void) unpack_filename(newpath, newpath);
      if ((new_dirp = my_dir(newpath, MYF(MY_DONT_SORT))))
      {
	DBUG_PRINT("my",("Archive subdir found: %s", newpath));
	if ((mysql_rm_arc_files(thd, new_dirp, newpath)) < 0)
	  DBUG_RETURN(true);
	continue;
      }
      *found_other_files= true;
      continue;
    }
    if (!(extension= strrchr(file->name, '.')))
      extension= strend(file->name);
    if (find_type(extension, &deletable_extentions, FIND_TYPE_NO_PREFIX) <= 0)
    {
      if (find_type(extension, known_extensions, FIND_TYPE_NO_PREFIX) <= 0)
        *found_other_files= true;
      continue;
    }
    /* just for safety we use files_charset_info */
    if (db && !my_strcasecmp(files_charset_info,
                             extension, reg_ext))
    {
      /* Drop the table nicely */
      *extension= 0;			// Remove extension
      TABLE_LIST *table_list=(TABLE_LIST*)
                              thd->mem_calloc(sizeof(*table_list) + 
                                          strlen(db) + 1 +
                                          MYSQL50_TABLE_NAME_PREFIX_LENGTH + 
                                          strlen(file->name) + 1);

      if (!table_list)
        DBUG_RETURN(true);
      table_list->db= (char*) (table_list+1);
      table_list->db_length= my_stpcpy(const_cast<char*>(table_list->db),
                                       db) - table_list->db;
      table_list->table_name= table_list->db + table_list->db_length + 1;
      table_list->table_name_length= filename_to_tablename(file->name,
                                     const_cast<char*>(table_list->table_name),
                                     MYSQL50_TABLE_NAME_PREFIX_LENGTH +
                                     strlen(file->name) + 1);
      table_list->open_type= OT_BASE_ONLY;

      /* To be able to correctly look up the table in the table cache. */
      if (lower_case_table_names)
        table_list->table_name_length= my_casedn_str(files_charset_info,
                                    const_cast<char*>(table_list->table_name));

      table_list->alias= table_list->table_name;	// If lower_case_table_names=2
      table_list->internal_tmp_table= is_prefix(file->name, tmp_file_prefix);
      MDL_REQUEST_INIT(&table_list->mdl_request,
                       MDL_key::TABLE, table_list->db,
                       table_list->table_name, MDL_EXCLUSIVE,
                       MDL_TRANSACTION);
      /* Link into list */
      (*tot_list_next_local)= table_list;
      (*tot_list_next_global)= table_list;
      tot_list_next_local= &table_list->next_local;
      tot_list_next_global= &table_list->next_global;
    }
    else
    {
      strxmov(filePath, path, "/", file->name, NullS);
      /*
        We ignore ENOENT error in order to skip files that was deleted
        by concurrently running statement like REAPIR TABLE ...
      */
      if (my_delete_with_symlink(filePath, MYF(0)) &&
          my_errno() != ENOENT)
      {
        char errbuf[MYSYS_STRERROR_SIZE];
        my_error(EE_DELETE, MYF(0), filePath,
                 my_errno(), my_strerror(errbuf, sizeof(errbuf), my_errno()));
        DBUG_RETURN(true);
      }
    }
  }
  *tables= tot_list;
  DBUG_RETURN(false);
}


/*
  Remove directory with symlink

  SYNOPSIS
    rm_dir_w_symlink()
    org_path    path of derictory
    send_error  send errors
  RETURN
    0 OK
    1 ERROR
*/

static my_bool rm_dir_w_symlink(const char *org_path, my_bool send_error)
{
  char tmp_path[FN_REFLEN], *pos;
  char *path= tmp_path;
  DBUG_ENTER("rm_dir_w_symlink");
  unpack_filename(tmp_path, org_path);
#ifdef HAVE_READLINK
  int error;
  char tmp2_path[FN_REFLEN];

  /* Remove end FN_LIBCHAR as this causes problem on Linux in readlink */
  pos= strend(path);
  if (pos > path && pos[-1] == FN_LIBCHAR)
    *--pos=0;

  if ((error= my_readlink(tmp2_path, path, MYF(MY_WME))) < 0)
    DBUG_RETURN(1);
  if (!error)
  {
    if (mysql_file_delete(key_file_misc, path, MYF(send_error ? MY_WME : 0)))
    {
      DBUG_RETURN(send_error);
    }
    /* Delete directory symbolic link pointed at */
    path= tmp2_path;
  }
#endif
  /* Remove last FN_LIBCHAR to not cause a problem on OS/2 */
  pos= strend(path);

  if (pos > path && pos[-1] == FN_LIBCHAR)
    *--pos=0;
  if (rmdir(path) < 0 && send_error)
  {
    my_error(ER_DB_DROP_RMDIR, MYF(0), path, errno);
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


/*
  Remove .frm archives from directory

  SYNOPSIS
    thd       thread handler
    dirp      list of files in archive directory
    db        data base name
    org_path  path of archive directory

  RETURN
    > 0 number of removed files
    -1  error

  NOTE
    A support of "arc" directories is obsolete, however this
    function should exist to remove existent "arc" directories.
*/
long mysql_rm_arc_files(THD *thd, MY_DIR *dirp, const char *org_path)
{
  long deleted= 0;
  ulong found_other_files= 0;
  char filePath[FN_REFLEN];
  DBUG_ENTER("mysql_rm_arc_files");
  DBUG_PRINT("enter", ("path: %s", org_path));

  for (uint idx=0 ;
       idx < dirp->number_off_files && !thd->killed ;
       idx++)
  {
    FILEINFO *file=dirp->dir_entry+idx;
    char *extension, *revision;
    DBUG_PRINT("info",("Examining: %s", file->name));

    /* skiping . and .. */
    if (file->name[0] == '.' && (!file->name[1] ||
       (file->name[1] == '.' &&  !file->name[2])))
      continue;

    extension= fn_ext(file->name);
    if (extension[0] != '.' ||
        extension[1] != 'f' || extension[2] != 'r' ||
        extension[3] != 'm' || extension[4] != '-')
    {
      found_other_files++;
      continue;
    }
    revision= extension+5;
    while (*revision && my_isdigit(system_charset_info, *revision))
      revision++;
    if (*revision)
    {
      found_other_files++;
      continue;
    }
    strxmov(filePath, org_path, "/", file->name, NullS);
    if (mysql_file_delete_with_symlink(key_file_misc, filePath, MYF(MY_WME)))
    {
      goto err;
    }
    deleted++;
  }
  if (thd->killed)
    goto err;

  my_dirend(dirp);

  /*
    If the directory is a symbolic link, remove the link first, then
    remove the directory the symbolic link pointed at
  */
  if (!found_other_files &&
      rm_dir_w_symlink(org_path, 0))
    DBUG_RETURN(-1);
  DBUG_RETURN(deleted);

err:
  my_dirend(dirp);
  DBUG_RETURN(-1);
}


/**
  @brief Internal implementation: switch current database to a valid one.

  @param thd            Thread context.
  @param new_db_name    Name of the database to switch to. The function will
                        take ownership of the name (the caller must not free
                        the allocated memory). If the name is NULL, we're
                        going to switch to NULL db.
  @param new_db_access  Privileges of the new database.
  @param new_db_charset Character set of the new database.
*/

static void mysql_change_db_impl(THD *thd,
                                 const LEX_CSTRING &new_db_name,
                                 ulong new_db_access,
                                 const CHARSET_INFO *new_db_charset)
{
  /* 1. Change current database in THD. */

  if (new_db_name.str == NULL)
  {
    /*
      THD::set_db() does all the job -- it frees previous database name and
      sets the new one.
    */

    thd->set_db(NULL_CSTR);
  }
  else if (!strcmp(new_db_name.str,INFORMATION_SCHEMA_NAME.str))
  {
    /*
      Here we must use THD::set_db(), because we want to copy
      INFORMATION_SCHEMA_NAME constant.
    */

    thd->set_db(to_lex_cstring(INFORMATION_SCHEMA_NAME));
  }
  else
  {
    /*
      Here we already have a copy of database name to be used in THD. So,
      we just call THD::reset_db(). Since THD::reset_db() does not releases
      the previous database name, we should do it explicitly.
    */
    mysql_mutex_lock(&thd->LOCK_thd_data);
    if (thd->db().str)
      my_free(const_cast<char*>(thd->db().str));
    DEBUG_SYNC(thd, "after_freeing_thd_db");
    thd->reset_db(new_db_name);
    mysql_mutex_unlock(&thd->LOCK_thd_data);
  }

  /* 2. Update security context. */

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  thd->security_context()->set_db_access(new_db_access);
#endif

  /* 3. Update db-charset environment variables. */

  thd->db_charset= new_db_charset;
  thd->variables.collation_database= new_db_charset;
}



/**
  Backup the current database name before switch.

  @param[in]      thd             thread handle
  @param[in, out] saved_db_name   IN: "str" points to a buffer where to store
                                  the old database name, "length" contains the
                                  buffer size
                                  OUT: if the current (default) database is
                                  not NULL, its name is copied to the
                                  buffer pointed at by "str"
                                  and "length" is updated accordingly.
                                  Otherwise "str" is set to NULL and
                                  "length" is set to 0.
*/

static void backup_current_db_name(THD *thd,
                                   LEX_STRING *saved_db_name)
{
  if (!thd->db().str)
  {
    /* No current (default) database selected. */

    saved_db_name->str= NULL;
    saved_db_name->length= 0;
  }
  else
  {
    strmake(saved_db_name->str, thd->db().str, saved_db_name->length - 1);
    saved_db_name->length= thd->db().length;
  }
}


/**
  Return TRUE if db1_name is equal to db2_name, FALSE otherwise.

  The function allows to compare database names according to the MySQL
  rules. The database names db1 and db2 are equal if:
     - db1 is NULL and db2 is NULL;
     or
     - db1 is not-NULL, db2 is not-NULL, db1 is equal (ignoring case) to
       db2 in system character set (UTF8).
*/

static inline bool
cmp_db_names(const char *db1_name,
             const char *db2_name)
{
  return
         /* db1 is NULL and db2 is NULL */
         (!db1_name && !db2_name) ||

         /* db1 is not-NULL, db2 is not-NULL, db1 == db2. */
         (db1_name && db2_name &&
         my_strcasecmp(system_charset_info, db1_name, db2_name) == 0);
}


/**
  @brief Change the current database and its attributes unconditionally.

  @param thd          thread handle
  @param new_db_name  database name
  @param force_switch if force_switch is FALSE, then the operation will fail if

                        - new_db_name is NULL or empty;

                        - OR new database name is invalid
                          (check_db_name() failed);

                        - OR user has no privilege on the new database;

                        - OR new database does not exist;

                      if force_switch is TRUE, then

                        - if new_db_name is NULL or empty, the current
                          database will be NULL, @@collation_database will
                          be set to @@collation_server, the operation will
                          succeed.

                        - if new database name is invalid
                          (check_db_name() failed), the current database
                          will be NULL, @@collation_database will be set to
                          @@collation_server, but the operation will fail;

                        - user privileges will not be checked
                          (THD::db_access however is updated);

                          TODO: is this really the intention?
                                (see sp-security.test).

                        - if new database does not exist,the current database
                          will be NULL, @@collation_database will be set to
                          @@collation_server, a warning will be thrown, the
                          operation will succeed.

  @details The function checks that the database name corresponds to a
  valid and existent database, checks access rights and changes the current
  database with database attributes (@@collation_database session variable,
  THD::db_access).

  This function is not the only way to switch the database that is
  currently employed. When the replication slave thread switches the
  database before executing a query, it calls thd->set_db directly.
  However, if the query, in turn, uses a stored routine, the stored routine
  will use this function, even if it's run on the slave.

  This function allocates the name of the database on the system heap: this
  is necessary to be able to uniformly change the database from any module
  of the server. Up to 5.0 different modules were using different memory to
  store the name of the database, and this led to memory corruption:
  a stack pointer set by Stored Procedures was used by replication after
  the stack address was long gone.

  @return Operation status
    @retval FALSE Success
    @retval TRUE  Error
*/

bool mysql_change_db(THD *thd, const LEX_CSTRING &new_db_name,
                     bool force_switch)
{
  LEX_STRING new_db_file_name;
  LEX_CSTRING new_db_file_name_cstr;

  Security_context *sctx= thd->security_context();
  ulong db_access= sctx->db_access();
  const CHARSET_INFO *db_default_cl;

  DBUG_ENTER("mysql_change_db");
  DBUG_PRINT("enter",("name: '%s'", new_db_name.str));

  if (new_db_name.str == NULL ||
      new_db_name.length == 0)
  {
    if (force_switch)
    {
      /*
        This can happen only if we're switching the current database back
        after loading stored program. The thing is that loading of stored
        program can happen when there is no current database.

        TODO: actually, new_db_name and new_db_name->str seem to be always
        non-NULL. In case of stored program, new_db_name->str == "" and
        new_db_name->length == 0.
      */

      mysql_change_db_impl(thd, NULL_CSTR, 0, thd->variables.collation_server);

      goto done;
    }
    else
    {
      my_message(ER_NO_DB_ERROR, ER(ER_NO_DB_ERROR), MYF(0));

      DBUG_RETURN(TRUE);
    }
  }

  if (is_infoschema_db(new_db_name.str, new_db_name.length))
  {
    /* Switch the current database to INFORMATION_SCHEMA. */

    mysql_change_db_impl(thd, to_lex_cstring(INFORMATION_SCHEMA_NAME),
                         SELECT_ACL, system_charset_info);
    goto done;
  }

  /*
    Now we need to make a copy because check_db_name requires a
    non-constant argument. Actually, it takes database file name.

    TODO: fix check_db_name().
  */

  new_db_file_name.str= my_strndup(key_memory_THD_db,
                                   new_db_name.str, new_db_name.length,
                                   MYF(MY_WME));
  new_db_file_name.length= new_db_name.length;

  if (new_db_file_name.str == NULL)
    DBUG_RETURN(TRUE);                             /* the error is set */

  /*
    NOTE: if check_db_name() fails, we should throw an error in any case,
    even if we are called from sp_head::execute().

    It's next to impossible however to get this error when we are called
    from sp_head::execute(). But let's switch the current database to NULL
    in this case to be sure.
  */

  if (check_and_convert_db_name(&new_db_file_name, FALSE) != IDENT_NAME_OK)
  {
    my_free(new_db_file_name.str);

    if (force_switch)
      mysql_change_db_impl(thd, NULL_CSTR, 0, thd->variables.collation_server);
    DBUG_RETURN(TRUE);
  }

  DBUG_PRINT("info",("Use database: %s", new_db_file_name.str));

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  db_access= sctx->check_access(DB_ACLS) ?
    DB_ACLS :
    acl_get(sctx->host().str,
            sctx->ip().str,
            sctx->priv_user().str,
            new_db_file_name.str,
            false) | sctx->master_access();

  if (!force_switch &&
      !(db_access & DB_ACLS) &&
      check_grant_db(thd, new_db_file_name.str))
  {
    my_error(ER_DBACCESS_DENIED_ERROR, MYF(0),
             sctx->priv_user().str,
             sctx->priv_host().str,
             new_db_file_name.str);
    query_logger.general_log_print(thd, COM_INIT_DB,
                                   ER(ER_DBACCESS_DENIED_ERROR),
                                   sctx->priv_user().str,
                                   sctx->priv_host().str,
                                   new_db_file_name.str);
    my_free(new_db_file_name.str);
    DBUG_RETURN(TRUE);
  }
#endif

  DEBUG_SYNC(thd, "before_db_dir_check");

  if (check_db_dir_existence(new_db_file_name.str))
  {
    if (force_switch)
    {
      /* Throw a warning and free new_db_file_name. */

      push_warning_printf(thd, Sql_condition::SL_NOTE,
                          ER_BAD_DB_ERROR, ER(ER_BAD_DB_ERROR),
                          new_db_file_name.str);

      my_free(new_db_file_name.str);

      /* Change db to NULL. */
      mysql_change_db_impl(thd, NULL_CSTR, 0, thd->variables.collation_server);

      /* The operation succeed. */
      goto done;
    }
    else
    {
      /* Report an error and free new_db_file_name. */

      my_error(ER_BAD_DB_ERROR, MYF(0), new_db_file_name.str);
      my_free(new_db_file_name.str);

      /* The operation failed. */

      DBUG_RETURN(TRUE);
    }
  }

  /*
    NOTE: in mysql_change_db_impl() new_db_file_name is assigned to THD
    attributes and will be freed in THD::~THD().
  */

  db_default_cl= get_default_db_collation(thd, new_db_file_name.str);

  new_db_file_name_cstr.str= new_db_file_name.str;
  new_db_file_name_cstr.length= new_db_file_name.length;
  mysql_change_db_impl(thd, new_db_file_name_cstr, db_access, db_default_cl);

done:
  /*
    Check if current database tracker is enabled. If so, set the 'changed' flag.
  */
  if (thd->session_tracker.get_tracker(CURRENT_SCHEMA_TRACKER)->is_enabled())
  {
    LEX_CSTRING dummy= { C_STRING_WITH_LEN("") };
    dummy.length= dummy.length*1;
    thd->session_tracker.get_tracker(CURRENT_SCHEMA_TRACKER)->mark_as_changed(thd, &dummy);
  }
  if (thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)->is_enabled())
    thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)->mark_as_changed(thd, NULL);
  DBUG_RETURN(FALSE);
}


/**
  Change the current database and its attributes if needed.

  @param          thd             thread handle
  @param          new_db_name     database name
  @param[in, out] saved_db_name   IN: "str" points to a buffer where to store
                                  the old database name, "length" contains the
                                  buffer size
                                  OUT: if the current (default) database is
                                  not NULL, its name is copied to the
                                  buffer pointed at by "str"
                                  and "length" is updated accordingly.
                                  Otherwise "str" is set to NULL and
                                  "length" is set to 0.
  @param          force_switch    @see mysql_change_db()
  @param[out]     cur_db_changed  out-flag to indicate whether the current
                                  database has been changed (valid only if
                                  the function suceeded)
*/

bool mysql_opt_change_db(THD *thd,
                         const LEX_CSTRING &new_db_name,
                         LEX_STRING *saved_db_name,
                         bool force_switch,
                         bool *cur_db_changed)
{
  *cur_db_changed= !cmp_db_names(thd->db().str, new_db_name.str);

  if (!*cur_db_changed)
    return FALSE;

  backup_current_db_name(thd, saved_db_name);

  return mysql_change_db(thd, new_db_name, force_switch);
}


/**
  Upgrade a 5.0 database.
  This function is invoked whenever an ALTER DATABASE UPGRADE query is executed:
    ALTER DATABASE 'olddb' UPGRADE DATA DIRECTORY NAME.

  If we have managed to rename (move) tables to the new database
  but something failed on a later step, then we store the
  RENAME DATABASE event in the log. mysql_rename_db() is atomic in
  the sense that it will rename all or none of the tables.

  @param thd Current thread
  @param old_db 5.0 database name, in #mysql50#name format
  @return 0 on success, 1 on error
*/
bool mysql_upgrade_db(THD *thd, const LEX_CSTRING &old_db)
{
  int error= 0, change_to_newdb= 0;
  char path[FN_REFLEN+16];
  size_t length;
  HA_CREATE_INFO create_info;
  MY_DIR *dirp;
  TABLE_LIST *table_list;
  SELECT_LEX *sl= thd->lex->current_select();
  LEX_CSTRING new_db;
  DBUG_ENTER("mysql_upgrade_db");

  if ((old_db.length <= MYSQL50_TABLE_NAME_PREFIX_LENGTH) ||
      (strncmp(old_db.str,
              MYSQL50_TABLE_NAME_PREFIX,
              MYSQL50_TABLE_NAME_PREFIX_LENGTH) != 0))
  {
    my_error(ER_WRONG_USAGE, MYF(0),
             "ALTER DATABASE UPGRADE DATA DIRECTORY NAME",
             "name");
    DBUG_RETURN(1);
  }

  /* `#mysql50#<name>` converted to encoded `<name>` */
  new_db.str= old_db.str + MYSQL50_TABLE_NAME_PREFIX_LENGTH;
  new_db.length= old_db.length - MYSQL50_TABLE_NAME_PREFIX_LENGTH;

  /* Lock the old name, the new name will be locked by mysql_create_db().*/
  if (lock_schema_name(thd, old_db.str))
    DBUG_RETURN(true);

  /*
    Let's remember if we should do "USE newdb" afterwards.
    thd->db will be cleared in mysql_rename_db()
  */
  if (thd->db().str && !strcmp(thd->db().str, old_db.str))
    change_to_newdb= 1;

  build_table_filename(path, sizeof(path)-1,
                       old_db.str, "", MY_DB_OPT_FILE, 0);
  if ((load_db_opt(thd, path, &create_info)))
    create_info.default_table_charset= thd->variables.collation_server;

  length= build_table_filename(path, sizeof(path)-1, old_db.str, "", "", 0);
  if (length && path[length-1] == FN_LIBCHAR)
    path[length-1]=0;                            // remove ending '\'
  if ((error= my_access(path,F_OK)))
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), old_db.str);
    goto exit;
  }

  /* Step1: Create the new database */
  if ((error= mysql_create_db(thd, new_db.str, &create_info, 1)))
    goto exit;

  /* Step2: Move tables to the new database */
  if ((dirp = my_dir(path,MYF(MY_DONT_SORT))))
  {
    uint nfiles= (uint) dirp->number_off_files;
    for (uint idx=0 ; idx < nfiles && !thd->killed ; idx++)
    {
      FILEINFO *file= dirp->dir_entry + idx;
      char *extension, tname[FN_REFLEN + 1];
      LEX_CSTRING table_str;
      DBUG_PRINT("info",("Examining: %s", file->name));

      /* skiping non-FRM files */
      if (my_strcasecmp(files_charset_info,
                        (extension= fn_rext(file->name)), reg_ext))
        continue;

      /* A frm file found, add the table info rename list */
      *extension= '\0';

      table_str.length= filename_to_tablename(file->name,
                                              tname, sizeof(tname)-1);
      table_str.str= (char*) sql_memdup(tname, table_str.length + 1);
      Table_ident *old_ident= new Table_ident(thd, old_db, table_str, 0);
      Table_ident *new_ident= new Table_ident(thd, new_db, table_str, 0);
      if (!old_ident || !new_ident ||
          !sl->add_table_to_list(thd, old_ident, NULL,
                                 TL_OPTION_UPDATING, TL_IGNORE,
                                 MDL_EXCLUSIVE) ||
          !sl->add_table_to_list(thd, new_ident, NULL,
                                 TL_OPTION_UPDATING, TL_IGNORE,
                                 MDL_EXCLUSIVE))
      {
        error= 1;
        my_dirend(dirp);
        goto exit;
      }
    }
    my_dirend(dirp);  
  }

  if ((table_list= thd->lex->query_tables) &&
      (error= mysql_rename_tables(thd, table_list, 1)))
  {
    /*
      Failed to move all tables from the old database to the new one.
      In the best case mysql_rename_tables() moved all tables back to the old
      database. In the worst case mysql_rename_tables() moved some tables
      to the new database, then failed, then started to move the tables back,
      and then failed again. In this situation we have some tables in the
      old database and some tables in the new database.
      Let's delete the option file, and then the new database directory.
      If some tables were left in the new directory, rmdir() will fail.
      It garantees we never loose any tables.
    */
    build_table_filename(path, sizeof(path)-1,
                         new_db.str,"",MY_DB_OPT_FILE, 0);
    mysql_file_delete(key_file_dbopt, path, MYF(MY_WME));
    length= build_table_filename(path, sizeof(path)-1, new_db.str, "", "", 0);
    if (length && path[length-1] == FN_LIBCHAR)
      path[length-1]=0;                            // remove ending '\'
    rmdir(path);
    goto exit;
  }


  /*
    Step3: move all remaining files to the new db's directory.
    Skip db opt file: it's been created by mysql_create_db() in
    the new directory, and will be dropped by mysql_rm_db() in the old one.
    Trigger TRN and TRG files are be moved as regular files at the moment,
    without any special treatment.

    Triggers without explicit database qualifiers in table names work fine: 
      use d1;
      create trigger trg1 before insert on t2 for each row set @a:=1
      rename database d1 to d2;

    TODO: Triggers, having the renamed database explicitely written
    in the table qualifiers.
    1. when the same database is renamed:
        create trigger d1.trg1 before insert on d1.t1 for each row set @a:=1;
        rename database d1 to d2;
      Problem: After database renaming, the trigger's body
               still points to the old database d1.
    2. when another database is renamed:
        create trigger d3.trg1 before insert on d3.t1 for each row
          insert into d1.t1 values (...);
        rename database d1 to d2;
      Problem: After renaming d1 to d2, the trigger's body
               in the database d3 still points to database d1.
  */

  if ((dirp = my_dir(path,MYF(MY_DONT_SORT))))
  {
    uint nfiles= (uint) dirp->number_off_files;
    for (uint idx=0 ; idx < nfiles ; idx++)
    {
      FILEINFO *file= dirp->dir_entry + idx;
      char oldname[FN_REFLEN + 1], newname[FN_REFLEN + 1];
      DBUG_PRINT("info",("Examining: %s", file->name));

      /* skiping . and .. and MY_DB_OPT_FILE */
      if ((file->name[0] == '.' &&
           (!file->name[1] || (file->name[1] == '.' && !file->name[2]))) ||
          !my_strcasecmp(files_charset_info, file->name, MY_DB_OPT_FILE))
        continue;

      /* pass empty file name, and file->name as extension to avoid encoding */
      build_table_filename(oldname, sizeof(oldname)-1,
                           old_db.str, "", file->name, 0);
      build_table_filename(newname, sizeof(newname)-1,
                           new_db.str, "", file->name, 0);
      mysql_file_rename(key_file_misc, oldname, newname, MYF(MY_WME));
    }
    my_dirend(dirp);
  }

  /*
    Step7: drop the old database.
    query_cache_invalidate(olddb) is done inside mysql_rm_db(), no need
    to execute them again.
    mysql_rm_db() also "unuses" if we drop the current database.
  */
  error= mysql_rm_db(thd, old_db, 0, 1);

  /* Step8: logging */
  if (mysql_bin_log.is_open())
  {
    int errcode= query_error_code(thd, TRUE);
    Query_log_event qinfo(thd, thd->query().str, thd->query().length,
                          FALSE, TRUE, TRUE, errcode);
    thd->clear_error();
    error|= mysql_bin_log.write_event(&qinfo);
  }

  /* Step9: Let's do "use newdb" if we renamed the current database */
  if (change_to_newdb)
    error|= mysql_change_db(thd, new_db, FALSE);

exit:
  DBUG_RETURN(error);
}



/*
  Check if there is directory for the database name.

  SYNOPSIS
    check_db_dir_existence()
    db_name   database name

  RETURN VALUES
    FALSE   There is directory for the specified database name.
    TRUE    The directory does not exist.
*/

bool check_db_dir_existence(const char *db_name)
{
  char db_dir_path[FN_REFLEN + 1];
  size_t db_dir_path_len;

  db_dir_path_len= build_table_filename(db_dir_path, sizeof(db_dir_path) - 1,
                                        db_name, "", "", 0);

  if (db_dir_path_len && db_dir_path[db_dir_path_len - 1] == FN_LIBCHAR)
    db_dir_path[db_dir_path_len - 1]= 0;

  /* Check access. */

  return my_access(db_dir_path, F_OK);
}
