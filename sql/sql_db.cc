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


/* create and drop of databases */

#include "mysql_priv.h"
#include <mysys_err.h>
#include "sql_acl.h"
#include "sp.h"
#include <my_dir.h>
#include <m_ctype.h>
#ifdef __WIN__
#include <direct.h>
#endif

const char *del_exts[]= {".frm", ".BAK", ".TMD",".opt", NullS};
static TYPELIB deletable_extentions=
{array_elements(del_exts)-1,"del_exts", del_exts};

const char *known_exts[]=
{".ISM",".ISD",".ISM",".MRG",".MYI",".MYD",".db", ".ibd", NullS};
static TYPELIB known_extentions=
{array_elements(known_exts)-1,"known_exts", known_exts};

static long mysql_rm_known_files(THD *thd, MY_DIR *dirp,
				 const char *db, const char *path,
				 uint level);

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

  if ((file=my_create(path, CREATE_MODE,O_RDWR | O_TRUNC,MYF(MY_WME))) >= 0)
  {
    ulong length;
    CHARSET_INFO *cs= ((create && create->default_table_charset) ? 
		       create->default_table_charset :
		       thd->variables.collation_server);
    length= my_sprintf(buf,(buf,
			    "default-character-set=%s\ndefault-collation=%s\n",
			    cs->csname,cs->name));

    /* Error is written by my_write */
    if (!my_write(file,(byte*) buf, length, MYF(MY_NABP+MY_WME)))
      error=0;
    my_close(file,MYF(0));
  }
  return error;
}


/* 
  Load database options file

  load_db_opt()
  path		Path for option file
  create	Where to store the read options

  DESCRIPTION
    For now, only default-character-set is read.

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
  if ((file=my_open(path, O_RDONLY | O_SHARE, MYF(0))) >= 0)
  {
    IO_CACHE cache;
    init_io_cache(&cache, file, IO_SIZE, READ_CACHE, 0, 0, MYF(0));

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
	  if (!(create->default_table_charset=
		get_charset_by_csname(pos+1, 
				      MY_CS_PRIMARY,
				      MYF(0))))
	  {
	    sql_print_error("Error while loading database options: '%s':",path);
	    sql_print_error(ER(ER_UNKNOWN_CHARACTER_SET),pos+1);
	  }
	}
	else if (!strncmp(buf,"default-collation", (pos-buf)))
	{
	  if (!(create->default_table_charset= get_charset_by_name(pos+1,
								   MYF(0))))
	  {
	    sql_print_error("Error while loading database options: '%s':",path);
	    sql_print_error(ER(ER_UNKNOWN_COLLATION),pos+1);
	  }
	}
      }
    }
    error=0;
    end_io_cache(&cache);
    my_close(file,MYF(0));
  }
  DBUG_RETURN(error);
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

  RETURN VALUES
  0	ok
  -1	Error

*/

int mysql_create_db(THD *thd, char *db, HA_CREATE_INFO *create_info,
		    bool silent)
{
  char	 path[FN_REFLEN+16];
  long result=1;
  int error = 0;
  MY_STAT stat_info;
  uint create_options = create_info ? create_info->options : 0;
  DBUG_ENTER("mysql_create_db");
  
  VOID(pthread_mutex_lock(&LOCK_mysql_create_db));

  // do not create database if another thread is holding read lock
  if (wait_if_global_read_lock(thd,0))
  {
    error= -1;
    goto exit2;
  }

  /* Check directory */
  strxmov(path, mysql_data_home, "/", db, NullS);
  unpack_dirname(path,path);			// Convert if not unix

  if (my_stat(path,&stat_info,MYF(0)))
  {
   if (!(create_options & HA_LEX_CREATE_IF_NOT_EXISTS))
    {
      my_error(ER_DB_CREATE_EXISTS,MYF(0),db);
      error = -1;
      goto exit;
    }
    result = 0;
  }
  else
  {
    if (my_errno != ENOENT)
    {
      my_error(EE_STAT, MYF(0),path,my_errno);
      goto exit;
    }
    strend(path)[-1]=0;				// Remove last '/' from path
    if (my_mkdir(path,0777,MYF(0)) < 0)
    {
      my_error(ER_CANT_CREATE_DB,MYF(0),db,my_errno);
      error = -1;
      goto exit;
    }
  }

  unpack_dirname(path, path);
  strcat(path,MY_DB_OPT_FILE);
  if (write_db_opt(thd, path, create_info))
  {
    /*
      Could not create options file.
      Restore things to beginning.
    */
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
  }
  
  if (!silent)
  {
    char *query;
    uint query_length;

    if (!thd->query)				// Only in replication
    {
      query= 	     path;
      query_length= (uint) (strxmov(path,"create database `", db, "`", NullS) -
			    path);
    }
    else
    {
      query= 	    thd->query;
      query_length= thd->query_length;
    }
    if (mysql_bin_log.is_open())
    {
      Query_log_event qinfo(thd, query, query_length, 0);
      mysql_bin_log.write(&qinfo);
    }
    send_ok(thd, result);
  }

exit:
  start_waiting_global_read_lock(thd);
exit2:
  VOID(pthread_mutex_unlock(&LOCK_mysql_create_db));
  DBUG_RETURN(error);
}


/* db-name is already validated when we come here */

int mysql_alter_db(THD *thd, const char *db, HA_CREATE_INFO *create_info)
{
  char path[FN_REFLEN+16];
  long result=1;
  int error = 0;
  DBUG_ENTER("mysql_alter_db");

  VOID(pthread_mutex_lock(&LOCK_mysql_create_db));

  // do not alter database if another thread is holding read lock
  if ((error=wait_if_global_read_lock(thd,0)))
    goto exit2;

  /* Check directory */
  (void)sprintf(path,"%s/%s/%s", mysql_data_home, db, MY_DB_OPT_FILE);
  fn_format(path, path, "", "", MYF(MY_UNPACK_FILENAME));
  if ((error=write_db_opt(thd, path, create_info)))
    goto exit;

  /* 
     Change options if current database is being altered
     TODO: Delete this code
  */
  if (thd->db && !strcmp(thd->db,db))
  {
    thd->db_charset= (create_info && create_info->default_table_charset) ?
		     create_info->default_table_charset : 
		     thd->variables.collation_server;
    thd->variables.collation_database= thd->db_charset;
  }

  if (mysql_bin_log.is_open())
  {
    Query_log_event qinfo(thd, thd->query, thd->query_length, 0);
    thd->clear_error();
    mysql_bin_log.write(&qinfo);
  }
  send_ok(thd, result);

exit:
  start_waiting_global_read_lock(thd);
exit2:
  VOID(pthread_mutex_unlock(&LOCK_mysql_create_db));
  DBUG_RETURN(error ? -1 : 0); /* -1 to delegate send_error() */
}


/*
  Drop all tables in a database and the database itself

  SYNOPSIS
    mysql_rm_db()
    thd			Thread handle
    db			Database name in the case given by user
		        It's already validated when we come here
    if_exists		Don't give error if database doesn't exists
    silent		Don't generate errors

  RETURN
    0   ok (Database dropped)
    -1	Error generated
*/


int mysql_rm_db(THD *thd,char *db,bool if_exists, bool silent)
{
  long deleted=0;
  int error = 0;
  char	path[FN_REFLEN+16], tmp_db[NAME_LEN+1];
  MY_DIR *dirp;
  DBUG_ENTER("mysql_rm_db");

  VOID(pthread_mutex_lock(&LOCK_mysql_create_db));

  // do not drop database if another thread is holding read lock
  if (wait_if_global_read_lock(thd,0))
  {
    error= -1;
    goto exit2;
  }

  (void) sprintf(path,"%s/%s",mysql_data_home,db);
  unpack_dirname(path,path);			// Convert if not unix
  /* See if the directory exists */
  if (!(dirp = my_dir(path,MYF(MY_DONT_SORT))))
  {
    if (!if_exists)
    {
      error= -1;
      my_error(ER_DB_DROP_EXISTS,MYF(0),db);
      goto exit;
    }
    else
      push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
			  ER_DB_DROP_EXISTS, ER(ER_DB_DROP_EXISTS), db);
  }
  else
  {
    pthread_mutex_lock(&LOCK_open);
    remove_db_from_cache(db);
    pthread_mutex_unlock(&LOCK_open);
    
    error= -1;
    if ((deleted= mysql_rm_known_files(thd, dirp, db, path, 0)) >= 0)
    {
      ha_drop_database(path);
      query_cache_invalidate1(db);  
      error = 0;
    }
  }
  if (lower_case_table_names)
  {
    /* Convert database to lower case */
    strmov(tmp_db, db);
    my_casedn_str(files_charset_info, tmp_db);
    db= tmp_db;
  }
  if (!silent && deleted>=0 && thd)
  {
    const char *query;
    ulong query_length;
    if (!thd->query)
    {
      /* The client used the old obsolete mysql_drop_db() call */
      query= path;
      query_length= (uint) (strxmov(path, "drop database `", db, "`",
                                     NullS) - path);
    }
    else
    {
      query =thd->query;
      query_length= thd->query_length;
    }
    mysql_update_log.write(thd, query, query_length);
    if (mysql_bin_log.is_open())
    {
      Query_log_event qinfo(thd, query, query_length, 0);
      thd->clear_error();
      mysql_bin_log.write(&qinfo);
    }
    send_ok(thd, (ulong) deleted);
  }

exit:
  (void)sp_drop_db_routines(thd, db); /* QQ Ignore errors for now  */
  start_waiting_global_read_lock(thd);
  /*
    If this database was the client's selected database, we silently change the
    client's selected database to nothing (to have an empty SELECT DATABASE()
    in the future). For this we free() thd->db and set it to 0. But we don't do
    free() for the slave thread. Indeed, doing a x_free() on it leads to nasty
    problems (i.e. long painful debugging) because in this thread, thd->db is
    the same as data_buf and db of the Query_log_event which is dropping the
    database. So if you free() thd->db, you're freeing data_buf. You set
    thd->db to 0 but not data_buf (thd->db and data_buf are two distinct
    pointers which point to the same place). Then in ~Query_log_event(), we
    have 'if (data_buf) free(data_buf)' data_buf is !=0 so this makes a
    DOUBLE free().
    Side effects of this double free() are, randomly (depends on the machine),
    when the slave is replicating a DROP DATABASE: 
    - garbage characters in the error message:
    "Error 'Can't drop database 'test2'; database doesn't exist' on query
    'h4zI©'"
    - segfault
    - hang in "free(vio)" (yes!) in the I/O or SQL slave threads (so slave
    server hangs at shutdown etc).
  */
  if (thd->db && !strcmp(thd->db, db))
  {
    if (!(thd->slave_thread)) /* a slave thread will free it itself */
      x_free(thd->db);
    thd->db= 0;
    thd->db_length= 0;
  }
exit2:
  VOID(pthread_mutex_unlock(&LOCK_mysql_create_db));

  DBUG_RETURN(error);
}

/*
  Removes files with known extensions plus all found subdirectories that
  are 2 hex digits (raid directories).
  thd MUST be set when calling this function!
*/

static long mysql_rm_known_files(THD *thd, MY_DIR *dirp, const char *db,
				 const char *org_path, uint level)
{
  long deleted=0;
  ulong found_other_files=0;
  char filePath[FN_REFLEN];
  TABLE_LIST *tot_list=0, **tot_list_next;
  List<String> raid_dirs;
  DBUG_ENTER("mysql_rm_known_files");
  DBUG_PRINT("enter",("path: %s", org_path));

  tot_list_next= &tot_list;

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

    /* Check if file is a raid directory */
    if ((my_isdigit(&my_charset_latin1, file->name[0]) ||
	 (file->name[0] >= 'a' && file->name[0] <= 'f')) &&
	(my_isdigit(&my_charset_latin1, file->name[1]) ||
	 (file->name[1] >= 'a' && file->name[1] <= 'f')) &&
	!file->name[2] && !level)
    {
      char newpath[FN_REFLEN], *copy_of_path;
      MY_DIR *new_dirp;
      String *dir;
      uint length;

      strxmov(newpath,org_path,"/",file->name,NullS);
      length= unpack_filename(newpath,newpath);
      if ((new_dirp = my_dir(newpath,MYF(MY_DONT_SORT))))
      {
	DBUG_PRINT("my",("New subdir found: %s", newpath));
	if ((mysql_rm_known_files(thd, new_dirp, NullS, newpath,1)) < 0)
	  goto err;
	if (!(copy_of_path= thd->memdup(newpath, length+1)) ||
	    !(dir= new (&thd->mem_root) String(copy_of_path, length,
					       &my_charset_bin)) ||
	    raid_dirs.push_back(dir))
	  goto err;
	continue;
      }
      found_other_files++;
      continue;
    }
    extension= fn_ext(file->name);
    if (find_type(extension, &deletable_extentions,1+2) <= 0)
    {
      if (find_type(extension, &known_extentions,1+2) <= 0)
	found_other_files++;
      continue;
    }
    // just for safety we use files_charset_info
    if (db && !my_strcasecmp(files_charset_info,
                             extension, reg_ext))
    {
      /* Drop the table nicely */
      *extension= 0;			// Remove extension
      TABLE_LIST *table_list=(TABLE_LIST*)
	thd->calloc(sizeof(*table_list)+ strlen(db)+strlen(file->name)+2);
      if (!table_list)
	goto err;
      table_list->db= (char*) (table_list+1);
      strmov(table_list->real_name= strmov(table_list->db,db)+1, file->name);
      table_list->alias= table_list->real_name;	// If lower_case_table_names=2
      /* Link into list */
      (*tot_list_next)= table_list;
      tot_list_next= &table_list->next;
      deleted++;
    }
    else
    {
      strxmov(filePath, org_path, "/", file->name, NullS);
      if (my_delete_with_symlink(filePath,MYF(MY_WME)))
      {
	goto err;
      }
    }
  }
  if (thd->killed ||
      (tot_list && mysql_rm_table_part2_with_lock(thd, tot_list, 1, 0, 1)))
    goto err;

  /* Remove RAID directories */
  {
    List_iterator<String> it(raid_dirs);
    String *dir;
    while ((dir= it++))
      if (rmdir(dir->c_ptr()) < 0)
	found_other_files++;
  }
  my_dirend(dirp);  
  
  /*
    If the directory is a symbolic link, remove the link first, then
    remove the directory the symbolic link pointed at
  */
  if (found_other_files)
  {
    my_error(ER_DB_DROP_RMDIR, MYF(0), org_path, EEXIST);
    DBUG_RETURN(-1);
  }
  else
  {
    char tmp_path[FN_REFLEN], *pos;
    char *path= tmp_path;
    unpack_filename(tmp_path,org_path);
#ifdef HAVE_READLINK
    int error;
    
    /* Remove end FN_LIBCHAR as this causes problem on Linux in readlink */
    pos=strend(path);
    if (pos > path && pos[-1] == FN_LIBCHAR)
      *--pos=0;

    if ((error=my_readlink(filePath, path, MYF(MY_WME))) < 0)
      DBUG_RETURN(-1);
    if (!error)
    {
      if (my_delete(path,MYF(!level ? MY_WME : 0)))
      {
	/* Don't give errors if we can't delete 'RAID' directory */
	if (level)
	  DBUG_RETURN(deleted);
	DBUG_RETURN(-1);
      }
      /* Delete directory symbolic link pointed at */
      path= filePath;
    }
#endif
    /* Remove last FN_LIBCHAR to not cause a problem on OS/2 */
    pos=strend(path);

    if (pos > path && pos[-1] == FN_LIBCHAR)
      *--pos=0;
    /* Don't give errors if we can't delete 'RAID' directory */
    if (rmdir(path) < 0 && !level)
    {
      my_error(ER_DB_DROP_RMDIR, MYF(0), path, errno);
      DBUG_RETURN(-1);
    }
  }
  DBUG_RETURN(deleted);

err:
  my_dirend(dirp);
  DBUG_RETURN(-1);
}


/*
  Change default database.

  SYNOPSIS
    mysql_change_db()
    thd		Thread handler
    name	Databasename

  DESCRIPTION
    Becasue the database name may have been given directly from the
    communication packet (in case of 'connect' or 'COM_INIT_DB')
    we have to do end space removal in this function.

  RETURN VALUES
    0	ok
    1	error
*/

bool mysql_change_db(THD *thd, const char *name)
{
  int length, db_length;
  char *dbname=my_strdup((char*) name,MYF(MY_WME));
  char	path[FN_REFLEN];
  ulong db_access;
  HA_CREATE_INFO create;
  DBUG_ENTER("mysql_change_db");

  if (!dbname || !(db_length= strlen(dbname)))
  {
    x_free(dbname);				/* purecov: inspected */
    send_error(thd,ER_NO_DB_ERROR);	/* purecov: inspected */
    DBUG_RETURN(1);				/* purecov: inspected */
  }
  if (check_db_name(dbname))
  {
    net_printf(thd, ER_WRONG_DB_NAME, dbname);
    x_free(dbname);
    DBUG_RETURN(1);
  }
  DBUG_PRINT("info",("Use database: %s", dbname));
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (test_all_bits(thd->master_access,DB_ACLS))
    db_access=DB_ACLS;
  else
    db_access= (acl_get(thd->host,thd->ip, thd->priv_user,dbname,0) |
		thd->master_access);
  if (!(db_access & DB_ACLS) && (!grant_option || check_grant_db(thd,dbname)))
  {
    net_printf(thd,ER_DBACCESS_DENIED_ERROR,
	       thd->priv_user,
	       thd->priv_host,
	       dbname);
    mysql_log.write(thd,COM_INIT_DB,ER(ER_DBACCESS_DENIED_ERROR),
		    thd->priv_user,
		    thd->priv_host,
		    dbname);
    my_free(dbname,MYF(0));
    DBUG_RETURN(1);
  }
#endif
  (void) sprintf(path,"%s/%s",mysql_data_home,dbname);
  length=unpack_dirname(path,path);		// Convert if not unix
  if (length && path[length-1] == FN_LIBCHAR)
    path[length-1]=0;				// remove ending '\'
  if (access(path,F_OK))
  {
    net_printf(thd,ER_BAD_DB_ERROR,dbname);
    my_free(dbname,MYF(0));
    DBUG_RETURN(1);
  }
  send_ok(thd);
  x_free(thd->db);
  thd->db=dbname;				// THD::~THD will free this
  thd->db_length=db_length;
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  thd->db_access=db_access;
#endif
  strmov(path+unpack_dirname(path,path), MY_DB_OPT_FILE);
  load_db_opt(thd, path, &create);
  thd->db_charset= create.default_table_charset ?
		   create.default_table_charset :
		   thd->variables.collation_server;
  thd->variables.collation_database= thd->db_charset;
  DBUG_RETURN(0);
}

