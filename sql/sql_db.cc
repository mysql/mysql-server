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


/* create and drop of databases */

#include "mysql_priv.h"
#include "sql_acl.h"
#include <my_dir.h>
#include <m_ctype.h>
#ifdef __WIN__
#include <direct.h>
#endif

static long mysql_rm_known_files(THD *thd, MY_DIR *dirp,
				 const char *db, const char *path,
				 uint level);

/* db-name is already validated when we come here */

int mysql_create_db(THD *thd, char *db, uint create_options, bool silent)
{
  char	 path[FN_REFLEN+16];
  MY_DIR *dirp;
  long result=1;
  int error = 0;
  DBUG_ENTER("mysql_create_db");

  VOID(pthread_mutex_lock(&LOCK_mysql_create_db));

  // do not create database if another thread is holding read lock
  if (wait_if_global_read_lock(thd, 0, 1))
  {
    error= -1;
    goto exit2;
  }

  /* Check directory */
  (void)sprintf(path,"%s/%s", mysql_data_home, db);
  unpack_dirname(path,path);			// Convert if not unix
  if ((dirp = my_dir(path,MYF(MY_DONT_SORT))))
  {
    my_dirend(dirp);
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
    strend(path)[-1]=0;				// Remove last '/' from path
    if (my_mkdir(path,0777,MYF(0)) < 0)
    {
      my_error(ER_CANT_CREATE_DB,MYF(0),db,my_errno);
      error = -1;
      goto exit;
    }
  }

  if (!silent)
  {
    if (!thd->query)
    {
      /* The client used the old obsolete mysql_create_db() call */
      thd->query_length= (uint) (strxmov(path,"create database `", db, "`",
					 NullS) - path);
      thd->query= path;
    }
    {
      mysql_update_log.write(thd,thd->query, thd->query_length);
      if (mysql_bin_log.is_open())
      {
        thd->clear_error();
	Query_log_event qinfo(thd, thd->query, thd->query_length, 0);
	mysql_bin_log.write(&qinfo);
      }
    }
    if (thd->query == path)
    {
      VOID(pthread_mutex_lock(&LOCK_thread_count));
      thd->query= 0;
      thd->query_length= 0;
      VOID(pthread_mutex_unlock(&LOCK_thread_count));
    }
    send_ok(&thd->net, result);
  }

exit:
  start_waiting_global_read_lock(thd);
exit2:
  VOID(pthread_mutex_unlock(&LOCK_mysql_create_db));
  DBUG_RETURN(error);
}

const char *del_exts[]= {".frm", ".BAK", ".TMD", NullS};
static TYPELIB deletable_extentions=
{array_elements(del_exts)-1,"del_exts", del_exts};

const char *known_exts[]=
{".ISM",".ISD",".ISM",".MRG",".MYI",".MYD",".db",NullS};
static TYPELIB known_extentions=
{array_elements(known_exts)-1,"known_exts", known_exts};


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
  if (wait_if_global_read_lock(thd, 0, 1))
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
    }
    else if (!silent)
      send_ok(&thd->net,0);
    goto exit;
  }
  if (lower_case_table_names)
  {
    /* Convert database to lower case */
    strmov(tmp_db, db);
    casedn_str(tmp_db);
    db= tmp_db;
  }

  pthread_mutex_lock(&LOCK_open);
  remove_db_from_cache(db);
  pthread_mutex_unlock(&LOCK_open);

  error = -1;
  if ((deleted=mysql_rm_known_files(thd, dirp, db, path,0)) >= 0 && thd)
  {
    ha_drop_database(path);
    query_cache_invalidate1(db);  
    if (!silent)
    {
      if (!thd->query)
      {
	thd->query_length= (uint) (strxmov(path,"drop database `", db, "`",
					   NullS)-
				   path);
	thd->query= path;
      }
      mysql_update_log.write(thd, thd->query, thd->query_length);
      if (mysql_bin_log.is_open())
      {
        thd->clear_error();
	Query_log_event qinfo(thd, thd->query, thd->query_length, 0);
	mysql_bin_log.write(&qinfo);
      }
      if (thd->query == path)
      {
	VOID(pthread_mutex_lock(&LOCK_thread_count));
	thd->query= 0;
	thd->query_length= 0;
	VOID(pthread_mutex_unlock(&LOCK_thread_count));
      }
      send_ok(&thd->net,(ulong) deleted);
    }
    error = 0;
  }

exit:
  start_waiting_global_read_lock(thd);
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
    if ((isdigit(file->name[0]) ||
	 (file->name[0] >= 'a' && file->name[0] <= 'f')) &&
	(isdigit(file->name[1]) ||
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
	    !(dir= new String(copy_of_path, length)) ||
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
    if (db && !my_strcasecmp(extension, reg_ext))
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
    }
    else
    {
      strxmov(filePath, org_path, "/", file->name, NullS);
      if (my_delete_with_symlink(filePath,MYF(MY_WME)))
      {
	goto err;
      }
      deleted++;
    }
  }
  if (thd->killed ||
      (tot_list && mysql_rm_table_part2_with_lock(thd, tot_list, 1, 1)))
  {
    goto err;
  }

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
  Changes the current database.

  NOTES
    Do as little as possible in this function, as it is not called for the
    replication slave SQL thread (for that thread, setting of thd->db is done
    in ::exec_event() methods of log_event.cc).
*/

bool mysql_change_db(THD *thd,const char *name)
{
  int length, db_length;
  char *dbname=my_strdup((char*) name,MYF(MY_WME));
  char	path[FN_REFLEN];
  ulong db_access;
  DBUG_ENTER("mysql_change_db");

  if (!dbname || !(db_length= strlen(dbname)))
  {
    x_free(dbname);				/* purecov: inspected */
    send_error(&thd->net,ER_NO_DB_ERROR);	/* purecov: inspected */
    DBUG_RETURN(1);				/* purecov: inspected */
  }
  if (check_db_name(dbname))
  {
    net_printf(&thd->net,ER_WRONG_DB_NAME, dbname);
    x_free(dbname);
    DBUG_RETURN(1);
  }
  DBUG_PRINT("info",("Use database: %s", dbname));
  if (test_all_bits(thd->master_access,DB_ACLS))
    db_access=DB_ACLS;
  else
    db_access= (acl_get(thd->host,thd->ip,(char*) &thd->remote.sin_addr,
			thd->priv_user,dbname,0) |
		thd->master_access);
  if (!(db_access & DB_ACLS) && (!grant_option || check_grant_db(thd,dbname)))
  {
    net_printf(&thd->net,ER_DBACCESS_DENIED_ERROR,
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

  (void) sprintf(path,"%s/%s",mysql_data_home,dbname);
  length=unpack_dirname(path,path);		// Convert if not unix
  if (length && path[length-1] == FN_LIBCHAR)
    path[length-1]=0;				// remove ending '\'
  if (access(path,F_OK))
  {
    net_printf(&thd->net,ER_BAD_DB_ERROR,dbname);
    my_free(dbname,MYF(0));
    DBUG_RETURN(1);
  }
  send_ok(&thd->net);
  x_free(thd->db);
  thd->db=dbname;
  thd->db_length=db_length;
  thd->db_access=db_access;
  DBUG_RETURN(0);
}
