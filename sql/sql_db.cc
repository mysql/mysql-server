/*
   Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_db.h"

#include "mysys_err.h"       // EE_*
#include "auth_common.h"     // SELECT_ACL
#include "binlog.h"          // mysql_bin_log
#include "current_thd.h"
#include "debug_sync.h"      // DEBUG_SYNC
#include "derror.h"          // ER_THD
#include "error_handler.h"   // Drop_table_error_handler
#include "events.h"          // Events
#include "lock.h"            // lock_schema_name
#include "log.h"             // sql_print_error
#include "log_event.h"       // Query_log_event
#include "mysqld.h"          // key_file_misc
#include "psi_memory_key.h"  // key_memory_THD_db
#include "sp.h"              // lock_db_routines
#include "sql_base.h"        // lock_table_names
#include "sql_cache.h"       // query_cache
#include "sql_class.h"       // THD
#include "sql_handler.h"     // mysql_ha_rm_tables
#include "sql_rename.h"      // mysql_rename_tables
#include "sql_table.h"       // build_table_filename
#include "table.h"           // TABLE_LIST

#include "dd/dd.h"                      // dd::get_dictionary()
#include "dd/dd_schema.h"               // dd::create_schema
#include "dd/dictionary.h"              // dd::Dictionary
#include "dd/iterator.h"                // dd::Iterator
#include "dd/cache/dictionary_client.h" // Dictionary_client

#ifdef _WIN32
#include <direct.h>
#endif

#include "pfs_file_provider.h"
#include "mysql/psi/mysql_file.h"

static const size_t MAX_DROP_TABLE_Q_LEN= 1024;

/*
  .frm is left in this list so that any orphan files can be removed on upgrade.
  .SDI needs to be there for now... need to investigate why...
*/
const char *del_exts[]= {".frm", ".BAK", ".TMD", ".opt", ".OLD", ".cfg", ".SDI", NullS};
static TYPELIB deletable_extentions=
{array_elements(del_exts)-1,"del_exts", del_exts, NULL};

static bool find_db_tables_and_rm_known_files(THD *thd, MY_DIR *dirp,
                                              const char *db,
                                              const char *path,
                                              TABLE_LIST **tables,
                                              bool *found_other_files);

static long mysql_rm_arc_files(THD *thd, MY_DIR *dirp, const char *org_path);
static my_bool rm_dir_w_symlink(const char *org_path, my_bool send_error);
static void mysql_change_db_impl(THD *thd,
                                 const LEX_CSTRING &new_db_name,
                                 ulong new_db_access,
                                 const CHARSET_INFO *new_db_charset);


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


/**
  Return default database collation.

  @param       thd          Thread context.
  @param       db_name      Database name.
  @param [out] collation    Charset object pointer if object exists else NULL.

  @return      false No error.
               true  Error (thd->is_error is assumed to be set.)
*/

bool get_default_db_collation(THD *thd,
                              const char *db_name,
                              const CHARSET_INFO **collation)
{
  // We must make sure the schema is released and unlocked in the right order.
  dd::Schema_MDL_locker mdl_handler(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Schema *sch_obj= NULL;

  if (mdl_handler.ensure_locked(db_name) ||
      thd->dd_client()->acquire<dd::Schema>(db_name, &sch_obj))
    return true;

  DEBUG_SYNC(thd, "acquired_schema_while_getting_collation");

  if (sch_obj)
  {
    *collation= get_charset(sch_obj->default_collation_id(), MYF(0));
    if (*collation == NULL)
    {
      char buff[STRING_BUFFER_USUAL_SIZE];
      my_error(ER_UNKNOWN_COLLATION, MYF(0),
               llstr(sch_obj->default_collation_id(), buff));
      return true;
    }
  }
  return false;
}


/**
  Create a database

  @param thd		Thread handler
  @param db		Name of database to create
                        Function assumes that this is already validated.
  @param create_info	Database create options (like character set)

  SIDE-EFFECTS
   1. Report back to client that command succeeded (my_ok)
   2. Report errors to client
   3. Log event to binary log

  @retval false ok
  @retval true  Error
*/

bool mysql_create_db(THD *thd, const char *db, HA_CREATE_INFO *create_info)
{
  DBUG_ENTER("mysql_create_db");

  /* do not create 'information_schema' db */
  if (is_infoschema_db(db))
  {
    my_error(ER_DB_CREATE_EXISTS, MYF(0), db);
    DBUG_RETURN(true);
  }

  /*
    When creating the schema, we must lock the schema name without case (for
    correct MDL locking) when l_c_t_n == 2.
  */
  char name_buf[NAME_LEN + 1];
  const char *lock_db_name= db;
  if (lower_case_table_names == 2)
  {
    my_stpcpy(name_buf, db);
    my_casedn_str(&my_charset_utf8_tolower_ci, name_buf);
    lock_db_name= name_buf;
  }
  if (lock_schema_name(thd, lock_db_name))
    DBUG_RETURN(true);

  /* Check directory */
  char	 path[FN_REFLEN+16];
  bool   was_truncated;
  size_t path_len= build_table_filename(path, sizeof(path) - 1, db, "", "", 0,
                                        &was_truncated);
  if (was_truncated)
  {
    my_error(ER_IDENT_CAUSES_TOO_LONG_PATH, MYF(0), sizeof(path)-1, path);
    DBUG_RETURN(true);
  }
  path[path_len-1]= 0;                    // Remove last '/' from path

  // If we are creating the system schema, then we create it physically
  // only during first time server initialization. During ordinary restart,
  // we still execute the CREATE statement to initialize the meta data, but
  // the physical representation of the schema is not re-created since it
  // already exists.
  MY_STAT stat_info;
  bool store_in_dd= true;
  bool schema_exists= (mysql_file_stat(key_file_misc,
                                       path, &stat_info, MYF(0)) != NULL);
  if (thd->is_dd_system_thread() && !opt_initialize &&
      dd::get_dictionary()->is_dd_schema_name(db))
  {
    if (!schema_exists)
    {
      sql_print_error("System schema directory does not exist.");
      // Must set OK explicitly to avoid send_statement_status() failing.
      // Calling my_error() does not set the error status this early in the
      // server startup sequence.
      my_ok(thd);
      DBUG_RETURN(true);
    }
  }
  else if (schema_exists)
  {
    if (!(create_info->options & HA_LEX_CREATE_IF_NOT_EXISTS))
    {
      my_error(ER_DB_CREATE_EXISTS, MYF(0), db);
      DBUG_RETURN(true);
    }
    push_warning_printf(thd, Sql_condition::SL_NOTE,
                        ER_DB_CREATE_EXISTS,
                        ER_THD(thd, ER_DB_CREATE_EXISTS), db);
    store_in_dd= false;
  }
  else
  {
    if (my_errno() != ENOENT)
    {
      char errbuf[MYSYS_STRERROR_SIZE];
      my_error(EE_STAT, MYF(0), path,
               my_errno(), my_strerror(errbuf, sizeof(errbuf), my_errno()));
      DBUG_RETURN(true);
    }
    if (my_mkdir(path,0777,MYF(0)) < 0)
    {
      char errbuf[MYSQL_ERRMSG_SIZE];
      my_error(ER_CANT_CREATE_DB, MYF(0), db, my_errno(),
               my_strerror(errbuf, MYSQL_ERRMSG_SIZE, my_errno()));
      DBUG_RETURN(true);
    }
  }

  ha_binlog_log_query(thd, 0, LOGCOM_CREATE_DB, thd->query().str,
                      thd->query().length, db, "");

  if (mysql_bin_log.is_open())
  {
    int errcode= query_error_code(thd, TRUE);
    Query_log_event qinfo(thd, thd->query().str, thd->query().length,
                          false, true, /* suppress_use */ true, errcode);

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
      rm_dir_w_symlink(path, true);
      DBUG_RETURN(true);
    }
  }

  /*
    Create schema in DD. This is done even when initializing the server
    and creating the system schema. In that case, the shared cache will
    store the object without storing it to disk. When the DD tables have
    been created, the cached objects will be stored persistently.
  */

  if (store_in_dd)
  {
    if (!create_info->default_table_charset)
      create_info->default_table_charset= thd->variables.collation_server;

    if (dd::create_schema(thd, db, create_info))
    {
      /*
        We could be here due an deadlock or some error reported
        by DD API framework. We remove the database directory
        which we just created above.

        It is expected that rm_dir_w_symlink() would not fail as
        we already old MDL lock on database and no parallel
        thread can remove the table before the current create
        database operation. Even if the call fails due to some
        other error we ignore the error as we anyway return
        failure (true) here.
      */
      rm_dir_w_symlink(path, true);
      DBUG_RETURN(true);
    }
  }

  my_ok(thd, 1);
  DBUG_RETURN(false);
}


/* db-name is already validated when we come here */

bool mysql_alter_db(THD *thd, const char *db, HA_CREATE_INFO *create_info)
{
  DBUG_ENTER("mysql_alter_db");

  if (lock_schema_name(thd, db))
    DBUG_RETURN(true);

  if (!create_info->default_table_charset)
    create_info->default_table_charset= thd->variables.collation_server;

  /*
    Do the change in the dd first to catch failures that should prevent
    writing binlog.
  */
  if (dd::alter_schema(thd, db, create_info))
  {
    // The error has been reported already.
    DBUG_RETURN(true);
  }

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
    if (mysql_bin_log.write_event(&qinfo))
      DBUG_RETURN(true);
  }

  my_ok(thd, 1);
  DBUG_RETURN(false);
}


/**
  Drop all tables, routines and events in a database and the database itself.

  @param  thd        Thread handle
  @param  db         Database name in the case given by user
                     It's already validated and set to lower case
                     (if needed) when we come here
  @param  if_exists  Don't give error if database doesn't exists

  @note   We do a "best effort" - try to drop as much as possible.
          If dropping the database itself fails, we try to binlog
          the drop of the tables we managed to do.

  @retval  false  OK (Database dropped)
  @retval  true   Error
*/

bool mysql_rm_db(THD *thd,const LEX_CSTRING &db, bool if_exists)
{
  ulong deleted_tables= 0;
  bool error= false;
  char	path[2 * FN_REFLEN + 16];
  MY_DIR *dirp;
  bool found_other_files= false;
  TABLE_LIST *tables= NULL;
  TABLE_LIST *table;
  Drop_table_error_handler err_handler;
  DBUG_ENTER("mysql_rm_db");

  if (lock_schema_name(thd, db.str))
    DBUG_RETURN(true);

  build_table_filename(path, sizeof(path) - 1, db.str, "", "", 0);

  /* See if the directory exists */
  bool sch_exists;
  if (dd::schema_exists(thd, db.str, &sch_exists))
    DBUG_RETURN(true);

  if (!(dirp= my_dir(path,MYF(MY_DONT_SORT))) ||
      !sch_exists)
  {
    my_dirend(dirp);
    if (!if_exists)
    {
      my_error(ER_DB_DROP_EXISTS, MYF(0), db.str);
      DBUG_RETURN(true);
    }
    else
    {
      push_warning_printf(thd, Sql_condition::SL_NOTE,
			  ER_DB_DROP_EXISTS,
                          ER_THD(thd, ER_DB_DROP_EXISTS), db.str);
      goto update_binlog;
    }
  }

  if (find_db_tables_and_rm_known_files(thd, dirp, db.str, path, &tables,
                                        &found_other_files))
  {
    my_dirend(dirp);
    DBUG_RETURN(true);
  }
  my_dirend(dirp);

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
        DBUG_RETURN(true);
      }
    }
  }

  /* Lock all tables and stored routines about to be dropped. */
  if (lock_table_names(thd, tables, NULL, thd->variables.lock_wait_timeout, 0)
#ifndef EMBEDDED_LIBRARY
      || Events::lock_schema_events(thd, db.str)
#endif
      || lock_db_routines(thd, db.str))
    DBUG_RETURN(true);

  /* mysql_ha_rm_tables() requires a non-null TABLE_LIST. */
  if (tables)
    mysql_ha_rm_tables(thd, tables);

  for (table= tables; table; table= table->next_local)
  {
    tdc_remove_table(thd, TDC_RT_REMOVE_ALL, table->db, table->table_name,
                     false);
    deleted_tables++;
  }

  if (thd->killed)
    DBUG_RETURN(true);

  thd->push_internal_handler(&err_handler);
  if (!(tables &&
        (error= mysql_rm_table_no_locks(thd, tables, true, false, true, true))))
  {
    /*
      We temporarily disable the binary log while dropping SPs
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
    thd->clear_error(); /* @todo Do not ignore errors */
    tmp_disable_binlog(thd);
    query_cache.invalidate(thd, db.str);
#ifndef EMBEDDED_LIBRARY
    error= Events::drop_schema_events(thd, db.str);
#endif
    error= (error || (sp_drop_db_routines(thd, db.str) != SP_OK));
    reenable_binlog(thd);

    /*
      If the directory is a symbolic link, remove the link first, then
      remove the directory the symbolic link pointed at
    */
    if (error || found_other_files)
      error= true;
    else
      error= rm_dir_w_symlink(path, true);
  }
  thd->pop_internal_handler();

  //
  // Remove schema in DD
  //

  /*
    If database exists and there was no error we should
    remove DD entry.
  */
  if (!error)
  {
    if (dd::drop_schema(thd, db.str))
      DBUG_RETURN(true);
  }

update_binlog:
  if (mysql_bin_log.is_open())
  {
    if (!error)
    {
      int errcode= query_error_code(thd, TRUE);
      Query_log_event qinfo(thd, thd->query().str, thd->query().length,
                            false, true, /* suppress_use */ true, errcode);
      /*
        Write should use the database being dropped as the "current
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
        DBUG_RETURN(true);
    }
    else
    {
      char *query, *query_pos, *query_end, *query_data_start;
      char temp_identifier[ 2 * FN_REFLEN + 2];

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
        DBUG_RETURN(true);
      }

      DBUG_PRINT("info", ("DROP DATABASE failed; generating DROP TABLE statement(s) in the binlog"));

      if (!(query= (char*) thd->alloc(MAX_DROP_TABLE_Q_LEN)))
      {
        // @todo: abort on out of memory instead
        /* not much else we can do */
        DBUG_RETURN(true); /* purecov: inspected */
      }
      query_pos= query_data_start= my_stpcpy(query,"DROP TABLE IF EXISTS ");
      query_end= query + MAX_DROP_TABLE_Q_LEN;

      for (TABLE_LIST *tbl= tables; tbl; tbl= tbl->next_local)
      {
        // Only write drop table to the binlog for tables that no longer exist.
        bool exists;
        if (check_if_table_exists(thd, tbl, &exists))
          DBUG_RETURN(true);

        if (exists)
          continue;

        /* 3 for the quotes and the comma*/
        size_t tbl_name_len= strlen(tbl->table_name) + 3;
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
            DBUG_RETURN(true);

          query_pos= query_data_start;
        }
        size_t id_length= my_strmov_quoted_identifier(thd, temp_identifier,
                                                      tbl->table_name, 0);
        temp_identifier[id_length]= '\0';
        query_pos= my_stpcpy(query_pos, (char*)&temp_identifier);
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
          DBUG_RETURN(true);
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
    char errbuf[MYSQL_ERRMSG_SIZE];
    my_error(ER_DB_DROP_RMDIR, MYF(0), path, EEXIST,
               my_strerror(errbuf, MYSQL_ERRMSG_SIZE, EEXIST));
    DBUG_RETURN(true);
  }

  if (error)
    DBUG_RETURN(true);

  /*
    If this database was the client's selected database, we silently
    change the client's selected database to nothing (to have an empty
    SELECT DATABASE() in the future). For this we free() thd->db and set
    it to 0.
  */
  if (thd->db().str && !strcmp(thd->db().str, db.str))
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

  thd->server_status|= SERVER_STATUS_DB_DROPPED;
  my_ok(thd, deleted_tables);
  DBUG_RETURN(false);
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
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Schema *sch_obj= NULL;
  if (thd->dd_client()->acquire<dd::Schema>(db, &sch_obj))
    DBUG_RETURN(true);

  DBUG_ASSERT(sch_obj);

  std::vector<const dd::Abstract_table*> sch_tables;
  if (thd->dd_client()->fetch_schema_components(sch_obj, &sch_tables))
    DBUG_RETURN(true);

  for (const dd::Abstract_table *table : sch_tables)
  {
    TABLE_LIST *table_list=(TABLE_LIST*)
      thd->mem_calloc(sizeof(*table_list));

    if (!table_list)
      DBUG_RETURN(true); /* purecov: inspected */

    table_list->db= thd->mem_strdup(db);
    table_list->db_length= strlen(db);
    table_list->table_name= thd->mem_strdup(table->name().c_str());
    table_list->table_name_length= table->name().length();

    table_list->open_type= OT_BASE_ONLY;

    /* To be able to correctly look up the table in the table cache. */
    if (lower_case_table_names)
      my_casedn_str(files_charset_info,
                    const_cast<char*>(table_list->table_name));

    table_list->alias= table_list->table_name;	// If lower_case_table_names=2
    table_list->internal_tmp_table= is_prefix(table->name().c_str(), tmp_file_prefix);
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

  delete_container_pointers(sch_tables);

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
    char errbuf[MYSQL_ERRMSG_SIZE];
    my_error(ER_DB_DROP_RMDIR, MYF(0), path, errno,
             my_strerror(errbuf, MYSQL_ERRMSG_SIZE, errno));
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
  Check if there is a file system directory for the schema name.

  @param db_name     Name of the schema to check.

  @return            true if the directory does not exist; otherwise, false
*/

static bool check_db_dir_existence(const char *db_name)
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
    @retval false Success
    @retval true  Error
*/

bool mysql_change_db(THD *thd, const LEX_CSTRING &new_db_name,
                     bool force_switch)
{
  LEX_STRING new_db_file_name;
  LEX_CSTRING new_db_file_name_cstr;

  Security_context *sctx= thd->security_context();
  ulong db_access= sctx->db_access();
  const CHARSET_INFO *db_default_cl= NULL;

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
      my_error(ER_NO_DB_ERROR, MYF(0));

      DBUG_RETURN(true);
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
    DBUG_RETURN(true);                             /* the error is set */

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
    DBUG_RETURN(true);
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
                                   ER_DEFAULT(ER_DBACCESS_DENIED_ERROR),
                                   sctx->priv_user().str,
                                   sctx->priv_host().str,
                                   new_db_file_name.str);
    my_free(new_db_file_name.str);
    DBUG_RETURN(true);
  }
#endif

  if (check_db_dir_existence(new_db_file_name.str))
  {
    if (force_switch)
    {
      /* Throw a warning and free new_db_file_name. */

      push_warning_printf(thd, Sql_condition::SL_NOTE,
                          ER_BAD_DB_ERROR, ER_THD(thd, ER_BAD_DB_ERROR),
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

      DBUG_RETURN(true);
    }
  }

  /*
    Note that checking for meta data existence is done implicitly
    in get_default_db_collation(): If the meta data does not exist,
    the collation is set to NULL.
  */

  if (get_default_db_collation(thd, new_db_file_name.str,
                               &db_default_cl))
  {
    my_free(new_db_file_name.str);
    DBUG_ASSERT(thd->is_error() || thd->killed);
    DBUG_RETURN(true);
  }

  db_default_cl= db_default_cl ? db_default_cl : thd->collation();

  /*
    NOTE: in mysql_change_db_impl() new_db_file_name is assigned to THD
    attributes and will be freed in THD::~THD().
  */
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
  DBUG_RETURN(false);
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
