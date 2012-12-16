/*
   Copyright (c) 2000, 2011, Oracle and/or its affiliates.

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


/* create and drop of databases */

#include "my_global.h"                          /* NO_EMBEDDED_ACCESS_CHECKS */
#include "sql_priv.h"
#include "unireg.h"
#include "sql_db.h"
#include "sql_cache.h"                   // query_cache_*
#include "lock.h"                        // lock_schema_name
#include "sql_table.h"                   // build_table_filename,
                                         // filename_to_tablename
#include "sql_rename.h"                  // mysql_rename_tables
#include "sql_acl.h"                     // SELECT_ACL, DB_ACLS,
                                         // acl_get, check_grant_db
#include "log_event.h"                   // Query_log_event
#include "sql_base.h"                    // lock_table_names, tdc_remove_table
#include "sql_handler.h"                 // mysql_ha_rm_tables
#include <mysys_err.h>
#include "sp_head.h"
#include "sp.h"
#include "events.h"
#include "sql_handler.h"
#include <my_dir.h>
#include <m_ctype.h>
#include "log.h"
#ifdef __WIN__
#include <direct.h>
#endif
#include "debug_sync.h"

#define MAX_DROP_TABLE_Q_LEN      1024

const char *del_exts[]= {".frm", ".BAK", ".TMD",".opt", NullS};
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
                                 LEX_STRING *new_db_name,
                                 ulong new_db_access,
                                 CHARSET_INFO *new_db_charset);


/* Database options hash */
static HASH dboptions;
static my_bool dboptions_init= 0;
static mysql_rwlock_t LOCK_dboptions;

/* Structure for database options */
typedef struct my_dbopt_st
{
  char *name;			/* Database name                  */
  uint name_length;		/* Database length name           */
  CHARSET_INFO *charset;	/* Database default character set */
} my_dbopt_t;


/*
  Function we use in the creation of our hash to get key.
*/

extern "C" uchar* dboptions_get_key(my_dbopt_t *opt, size_t *length,
                                    my_bool not_used);

uchar* dboptions_get_key(my_dbopt_t *opt, size_t *length,
                         my_bool not_used __attribute__((unused)))
{
  *length= opt->name_length;
  return (uchar*) opt->name;
}


/*
  Helper function to write a query to binlog used by mysql_rm_db()
*/

static inline int write_to_binlog(THD *thd, char *query, uint q_len,
                                  char *db, uint db_len)
{
  Query_log_event qinfo(thd, query, q_len, FALSE, TRUE, FALSE, 0);
  qinfo.db= db;
  qinfo.db_len= db_len;
  return mysql_bin_log.write(&qinfo);
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

  if (PSI_server == NULL)
    return;

  count= array_elements(all_database_names_rwlocks);
  PSI_server->register_rwlock(category, all_database_names_rwlocks, count);
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
    error= my_hash_init(&dboptions, lower_case_table_names ?
                        &my_charset_bin : system_charset_info,
                        32, 0, 0, (my_hash_get_key) dboptions_get_key,
                        free_dbopt,0);
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
  my_hash_init(&dboptions, lower_case_table_names ? 
               &my_charset_bin : system_charset_info,
               32, 0, 0, (my_hash_get_key) dboptions_get_key,
               free_dbopt,0);
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
  uint length;
  my_bool error= 1;
  
  length= (uint) strlen(dbname);
  
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
  uint length;
  my_bool error= 0;
  DBUG_ENTER("put_dbopt");

  length= (uint) strlen(dbname);
  
  mysql_rwlock_wrlock(&LOCK_dboptions);
  if (!(opt= (my_dbopt_t*) my_hash_search(&dboptions, (uchar*) dbname,
                                          length)))
  { 
    /* Options are not in the hash, insert them */
    char *tmp_name;
    if (!my_multi_malloc(MYF(MY_WME | MY_ZEROFILL),
                         &opt, (uint) sizeof(*opt), &tmp_name, (uint) length+1,
                         NullS))
    {
      error= 1;
      goto end;
    }
    
    opt->name= tmp_name;
    strmov(opt->name, dbname);
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
  register File file;
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

  bzero((char*) create,sizeof(*create));
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

CHARSET_INFO *get_default_db_collation(THD *thd, const char *db_name)
{
  HA_CREATE_INFO db_info;

  if (thd->db != NULL && strcmp(db_name, thd->db) == 0)
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

int mysql_create_db(THD *thd, char *db, HA_CREATE_INFO *create_info,
                     bool silent)
{
  char	 path[FN_REFLEN+16];
  long result= 1;
  int error= 0;
  MY_STAT stat_info;
  uint create_options= create_info ? create_info->options : 0;
  uint path_len;
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
  path_len= build_table_filename(path, sizeof(path) - 1, db, "", "", 0);
  path[path_len-1]= 0;                    // Remove last '/' from path

  if (mysql_file_stat(key_file_misc, path, &stat_info, MYF(0)))
  {
    if (!(create_options & HA_LEX_CREATE_IF_NOT_EXISTS))
    {
      my_error(ER_DB_CREATE_EXISTS, MYF(0), db);
      error= -1;
      goto exit;
    }
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
			ER_DB_CREATE_EXISTS, ER(ER_DB_CREATE_EXISTS), db);
    error= 0;
    goto not_silent;
  }
  else
  {
    if (my_errno != ENOENT)
    {
      my_error(EE_STAT, MYF(0), path, my_errno);
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
    char *query;
    uint query_length;

    query=        thd->query();
    query_length= thd->query_length();
    DBUG_ASSERT(query);

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

      /*
        These DDL methods and logging are protected with the exclusive
        metadata lock on the schema
      */
      if (mysql_bin_log.write(&qinfo))
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

  if (thd->db && !strcmp(thd->db,db))
  {
    thd->db_charset= create_info->default_table_charset ?
		     create_info->default_table_charset :
		     thd->variables.collation_server;
    thd->variables.collation_database= thd->db_charset;
  }

  ha_binlog_log_query(thd, 0, LOGCOM_ALTER_DB,
                      thd->query(), thd->query_length(),
                      db, "");

  if (mysql_bin_log.is_open())
  {
    int errcode= query_error_code(thd, TRUE); 
    Query_log_event qinfo(thd, thd->query(), thd->query_length(), FALSE, TRUE,
			  /* suppress_use */ TRUE, errcode);
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
    if ((error= mysql_bin_log.write(&qinfo)))
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

bool mysql_rm_db(THD *thd,char *db,bool if_exists, bool silent)
{
  ulong deleted_tables= 0;
  bool error= true;
  char	path[FN_REFLEN+16];
  MY_DIR *dirp;
  uint length;
  bool found_other_files= false;
  TABLE_LIST *tables= NULL;
  TABLE_LIST *table;
  Drop_table_error_handler err_handler;
  DBUG_ENTER("mysql_rm_db");


  if (lock_schema_name(thd, db))
    DBUG_RETURN(true);

  length= build_table_filename(path, sizeof(path) - 1, db, "", "", 0);
  strmov(path+length, MY_DB_OPT_FILE);		// Append db option file name
  del_dbopt(path);				// Remove dboption hash entry
  path[length]= '\0';				// Remove file name

  /* See if the directory exists */
  if (!(dirp= my_dir(path,MYF(MY_DONT_SORT))))
  {
    if (!if_exists)
    {
      my_error(ER_DB_DROP_EXISTS, MYF(0), db);
      DBUG_RETURN(true);
    }
    else
    {
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
			  ER_DB_DROP_EXISTS, ER(ER_DB_DROP_EXISTS), db);
      error= false;
      goto update_binlog;
    }
  }

  if (find_db_tables_and_rm_known_files(thd, dirp, db, path, &tables,
                                        &found_other_files))
    goto exit;

  /*
    Disable drop of enabled log tables, must be done before name locking.
    This check is only needed if we are dropping the "mysql" database.
  */
  if ((my_strcasecmp(system_charset_info, MYSQL_SCHEMA_NAME.str, db) == 0))
  {
    for (table= tables; table; table= table->next_local)
    {
      if (check_if_log_table(table->db_length, table->db,
                             table->table_name_length, table->table_name, true))
      {
        my_error(ER_BAD_LOG_STATEMENT, MYF(0), "DROP");
        goto exit;
      }
    }
  }

  /* Lock all tables and stored routines about to be dropped. */
  if (lock_table_names(thd, tables, NULL, thd->variables.lock_wait_timeout,
                       MYSQL_OPEN_SKIP_TEMPORARY) ||
      lock_db_routines(thd, db))
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
    query_cache_invalidate1(thd, db);
    (void) sp_drop_db_routines(thd, db); /* @todo Do not ignore errors */
#ifdef HAVE_EVENT_SCHEDULER
    Events::drop_schema_events(thd, db);
#endif
    reenable_binlog(thd);

    /*
      If the directory is a symbolic link, remove the link first, then
      remove the directory the symbolic link pointed at
    */
    if (found_other_files)
      my_error(ER_DB_DROP_RMDIR, MYF(0), path, EEXIST);
    else
      error= rm_dir_w_symlink(path, true);
  }
  thd->pop_internal_handler();

update_binlog:
  if (!silent && !error)
  {
    const char *query;
    ulong query_length;

    query= thd->query();
    query_length= thd->query_length();
    DBUG_ASSERT(query);

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
      qinfo.db     = db;
      qinfo.db_len = strlen(db);

      /*
        These DDL methods and logging are protected with the exclusive
        metadata lock on the schema.
      */
      if (mysql_bin_log.write(&qinfo))
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
    TABLE_LIST *tbl;
    uint db_len;

    if (!(query= (char*) thd->alloc(MAX_DROP_TABLE_Q_LEN)))
      goto exit; /* not much else we can do */
    query_pos= query_data_start= strmov(query,"drop table ");
    query_end= query + MAX_DROP_TABLE_Q_LEN;
    db_len= strlen(db);

    for (tbl= tables; tbl; tbl= tbl->next_local)
    {
      uint tbl_name_len;
      bool exists;
      char quoted_name[FN_REFLEN+3];

      // Only write drop table to the binlog for tables that no longer exist.
      if (check_if_table_exists(thd, tbl, 0, &exists))
      {
        error= true;
        goto exit;
      }
      if (exists)
        continue;

      my_snprintf(quoted_name, sizeof(quoted_name), "%`s", tbl->table_name);
      tbl_name_len= strlen(quoted_name) + 1; /* +1 for the comma */
      if (query_pos + tbl_name_len + 1 >= query_end)
      {
        /*
          These DDL methods and logging are protected with the exclusive
          metadata lock on the schema.
        */
        if (write_to_binlog(thd, query, query_pos -1 - query, db, db_len))
        {
          error= true;
          goto exit;
        }
        query_pos= query_data_start;
      }

      query_pos= strmov(query_pos, quoted_name);
      *query_pos++ = ',';
    }

    if (query_pos != query_data_start)
    {
      /*
        These DDL methods and logging are protected with the exclusive
        metadata lock on the schema.
      */
      if (write_to_binlog(thd, query, query_pos -1 - query, db, db_len))
      {
        error= true;
        goto exit;
      }
    }
  }

exit:
  /*
    If this database was the client's selected database, we silently
    change the client's selected database to nothing (to have an empty
    SELECT DATABASE() in the future). For this we free() thd->db and set
    it to 0.
  */
  if (thd->db && !strcmp(thd->db, db) && !error)
    mysql_change_db_impl(thd, NULL, 0, thd->variables.collation_server);
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

  tot_list_next_local= tot_list_next_global= &tot_list;

  for (uint idx=0 ;
       idx < (uint) dirp->number_off_files && !thd->killed ;
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
      if (find_type(extension, ha_known_exts(), FIND_TYPE_NO_PREFIX) <= 0)
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
                              thd->calloc(sizeof(*table_list) + 
                                          strlen(db) + 1 +
                                          MYSQL50_TABLE_NAME_PREFIX_LENGTH + 
                                          strlen(file->name) + 1);

      if (!table_list)
        DBUG_RETURN(true);
      table_list->db= (char*) (table_list+1);
      table_list->db_length= strmov(table_list->db, db) - table_list->db;
      table_list->table_name= table_list->db + table_list->db_length + 1;
      table_list->table_name_length= filename_to_tablename(file->name,
                                       table_list->table_name,
                                       MYSQL50_TABLE_NAME_PREFIX_LENGTH +
                                       strlen(file->name) + 1);
      table_list->open_type= OT_BASE_ONLY;

      /* To be able to correctly look up the table in the table cache. */
      if (lower_case_table_names)
        table_list->table_name_length= my_casedn_str(files_charset_info,
                                                     table_list->table_name);

      table_list->alias= table_list->table_name;	// If lower_case_table_names=2
      table_list->internal_tmp_table= is_prefix(file->name, tmp_file_prefix);
      table_list->mdl_request.init(MDL_key::TABLE, table_list->db,
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
          my_errno != ENOENT)
      {
        my_error(EE_DELETE, MYF(0), filePath, my_errno);
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
       idx < (uint) dirp->number_off_files && !thd->killed ;
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
                                 LEX_STRING *new_db_name,
                                 ulong new_db_access,
                                 CHARSET_INFO *new_db_charset)
{
  /* 1. Change current database in THD. */

  if (new_db_name == NULL)
  {
    /*
      THD::set_db() does all the job -- it frees previous database name and
      sets the new one.
    */

    thd->set_db(NULL, 0);
  }
  else if (new_db_name == &INFORMATION_SCHEMA_NAME)
  {
    /*
      Here we must use THD::set_db(), because we want to copy
      INFORMATION_SCHEMA_NAME constant.
    */

    thd->set_db(INFORMATION_SCHEMA_NAME.str, INFORMATION_SCHEMA_NAME.length);
  }
  else
  {
    /*
      Here we already have a copy of database name to be used in THD. So,
      we just call THD::reset_db(). Since THD::reset_db() does not releases
      the previous database name, we should do it explicitly.
    */
    my_free(thd->db);

    thd->reset_db(new_db_name->str, new_db_name->length);
  }

  /* 2. Update security context. */

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  thd->security_ctx->db_access= new_db_access;
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
  if (!thd->db)
  {
    /* No current (default) database selected. */

    saved_db_name->str= NULL;
    saved_db_name->length= 0;
  }
  else
  {
    strmake(saved_db_name->str, thd->db, saved_db_name->length - 1);
    saved_db_name->length= thd->db_length;
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
  return ((!db1_name && !db2_name) ||
          (db1_name && db2_name &&
           my_strcasecmp(system_charset_info, db1_name, db2_name) == 0));
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

bool mysql_change_db(THD *thd, const LEX_STRING *new_db_name, bool force_switch)
{
  LEX_STRING new_db_file_name;

  Security_context *sctx= thd->security_ctx;
  ulong db_access= sctx->db_access;
  CHARSET_INFO *db_default_cl;
  DBUG_ENTER("mysql_change_db");

  if (new_db_name->length == 0)
  {
    if (force_switch)
    {
      /*
        This can happen only if we're switching the current database back
        after loading stored program. The thing is that loading of stored
        program can happen when there is no current database.

        In case of stored program, new_db_name->str == "" and
        new_db_name->length == 0.
      */

      mysql_change_db_impl(thd, NULL, 0, thd->variables.collation_server);

      DBUG_RETURN(FALSE);
    }
    else
    {
      my_message(ER_NO_DB_ERROR, ER(ER_NO_DB_ERROR), MYF(0));

      DBUG_RETURN(TRUE);
    }
  }
  DBUG_PRINT("enter",("name: '%s'", new_db_name->str));

  if (is_infoschema_db(new_db_name->str, new_db_name->length))
  {
    /* Switch the current database to INFORMATION_SCHEMA. */

    mysql_change_db_impl(thd, &INFORMATION_SCHEMA_NAME, SELECT_ACL,
                         system_charset_info);

    DBUG_RETURN(FALSE);
  }

  /*
    Now we need to make a copy because check_db_name requires a
    non-constant argument. Actually, it takes database file name.

    TODO: fix check_db_name().
  */

  new_db_file_name.str= my_strndup(new_db_name->str, new_db_name->length,
                                   MYF(MY_WME));
  new_db_file_name.length= new_db_name->length;

  if (new_db_file_name.str == NULL)
    DBUG_RETURN(TRUE);                             /* the error is set */

  /*
    NOTE: if check_db_name() fails, we should throw an error in any case,
    even if we are called from sp_head::execute().

    It's next to impossible however to get this error when we are called
    from sp_head::execute(). But let's switch the current database to NULL
    in this case to be sure.
  */

  if (check_db_name(&new_db_file_name))
  {
    my_error(ER_WRONG_DB_NAME, MYF(0), new_db_file_name.str);
    my_free(new_db_file_name.str);

    if (force_switch)
      mysql_change_db_impl(thd, NULL, 0, thd->variables.collation_server);

    DBUG_RETURN(TRUE);
  }

  DBUG_PRINT("info",("Use database: %s", new_db_file_name.str));

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  db_access=
    test_all_bits(sctx->master_access, DB_ACLS) ?
    DB_ACLS :
    acl_get(sctx->host,
            sctx->ip,
            sctx->priv_user,
            new_db_file_name.str,
            FALSE) | sctx->master_access;

  if (!force_switch &&
      !(db_access & DB_ACLS) &&
      check_grant_db(thd, new_db_file_name.str))
  {
    my_error(ER_DBACCESS_DENIED_ERROR, MYF(0),
             sctx->priv_user,
             sctx->priv_host,
             new_db_file_name.str);
    general_log_print(thd, COM_INIT_DB, ER(ER_DBACCESS_DENIED_ERROR),
                      sctx->priv_user, sctx->priv_host, new_db_file_name.str);
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

      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                          ER_BAD_DB_ERROR, ER(ER_BAD_DB_ERROR),
                          new_db_file_name.str);

      my_free(new_db_file_name.str);

      /* Change db to NULL. */

      mysql_change_db_impl(thd, NULL, 0, thd->variables.collation_server);

      /* The operation succeed. */

      DBUG_RETURN(FALSE);
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

  mysql_change_db_impl(thd, &new_db_file_name, db_access, db_default_cl);

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
                         const LEX_STRING *new_db_name,
                         LEX_STRING *saved_db_name,
                         bool force_switch,
                         bool *cur_db_changed)
{
  *cur_db_changed= !cmp_db_names(thd->db, new_db_name->str);

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
bool mysql_upgrade_db(THD *thd, LEX_STRING *old_db)
{
  int error= 0, change_to_newdb= 0;
  char path[FN_REFLEN+16];
  uint length;
  HA_CREATE_INFO create_info;
  MY_DIR *dirp;
  TABLE_LIST *table_list;
  SELECT_LEX *sl= thd->lex->current_select;
  LEX_STRING new_db;
  DBUG_ENTER("mysql_upgrade_db");

  if ((old_db->length <= MYSQL50_TABLE_NAME_PREFIX_LENGTH) ||
      (strncmp(old_db->str,
              MYSQL50_TABLE_NAME_PREFIX,
              MYSQL50_TABLE_NAME_PREFIX_LENGTH) != 0))
  {
    my_error(ER_WRONG_USAGE, MYF(0),
             "ALTER DATABASE UPGRADE DATA DIRECTORY NAME",
             "name");
    DBUG_RETURN(1);
  }

  /* `#mysql50#<name>` converted to encoded `<name>` */
  new_db.str= old_db->str + MYSQL50_TABLE_NAME_PREFIX_LENGTH;
  new_db.length= old_db->length - MYSQL50_TABLE_NAME_PREFIX_LENGTH;

  /* Lock the old name, the new name will be locked by mysql_create_db().*/
  if (lock_schema_name(thd, old_db->str))
    DBUG_RETURN(1);

  /*
    Let's remember if we should do "USE newdb" afterwards.
    thd->db will be cleared in mysql_rename_db()
  */
  if (thd->db && !strcmp(thd->db, old_db->str))
    change_to_newdb= 1;

  build_table_filename(path, sizeof(path)-1,
                       old_db->str, "", MY_DB_OPT_FILE, 0);
  if ((load_db_opt(thd, path, &create_info)))
    create_info.default_table_charset= thd->variables.collation_server;

  length= build_table_filename(path, sizeof(path)-1, old_db->str, "", "", 0);
  if (length && path[length-1] == FN_LIBCHAR)
    path[length-1]=0;                            // remove ending '\'
  if ((error= my_access(path,F_OK)))
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), old_db->str);
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
      LEX_STRING table_str;
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
      Table_ident *old_ident= new Table_ident(thd, *old_db, table_str, 0);
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
                           old_db->str, "", file->name, 0);
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
  error= mysql_rm_db(thd, old_db->str, 0, 1);

  /* Step8: logging */
  if (mysql_bin_log.is_open())
  {
    int errcode= query_error_code(thd, TRUE);
    Query_log_event qinfo(thd, thd->query(), thd->query_length(),
                          FALSE, TRUE, TRUE, errcode);
    thd->clear_error();
    error|= mysql_bin_log.write(&qinfo);
  }

  /* Step9: Let's do "use newdb" if we renamed the current database */
  if (change_to_newdb)
    error|= mysql_change_db(thd, & new_db, FALSE);

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
  uint db_dir_path_len;

  db_dir_path_len= build_table_filename(db_dir_path, sizeof(db_dir_path) - 1,
                                        db_name, "", "", 0);

  if (db_dir_path_len && db_dir_path[db_dir_path_len - 1] == FN_LIBCHAR)
    db_dir_path[db_dir_path_len - 1]= 0;

  /* Check access. */

  return my_access(db_dir_path, F_OK);
}
