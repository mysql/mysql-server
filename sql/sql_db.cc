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

static long mysql_rm_known_files(THD *thd, MY_DIR *dirp, const char *path,
				 uint level);

/* db-name is already validated when we come here */

int mysql_create_db(THD *thd, char *db, uint create_options)
{
  char	 path[FN_REFLEN+16];
  MY_DIR *dirp;
  long result=1;
  int error = 0;
  DBUG_ENTER("mysql_create_db");
  
  VOID(pthread_mutex_lock(&LOCK_mysql_create_db));
  VOID(pthread_mutex_lock(&LOCK_open));

  // do not create database if another thread is holding read lock
  if (global_read_lock)
  {
    if (thd->global_read_lock)
    {
      net_printf(&thd->net, ER_CREATE_DB_WITH_READ_LOCK);
      VOID(pthread_mutex_unlock(&LOCK_open));
      goto exit;
    }
    while (global_read_lock && ! thd->killed)
    {
      (void) pthread_cond_wait(&COND_refresh,&LOCK_open);
    }
    
    if (thd->killed)
    {
      net_printf(&thd->net, ER_SERVER_SHUTDOWN);
      VOID(pthread_mutex_unlock(&LOCK_open));
      goto exit;
    }

  }
  
  VOID(pthread_mutex_unlock(&LOCK_open));

  /* Check directory */
  (void)sprintf(path,"%s/%s", mysql_data_home, db);
  unpack_dirname(path,path);			// Convert if not unix
  if ((dirp = my_dir(path,MYF(MY_DONT_SORT))))
  {
    my_dirend(dirp);
    if (!(create_options & HA_LEX_CREATE_IF_NOT_EXISTS))
    {
      if(thd)
        net_printf(&thd->net,ER_DB_CREATE_EXISTS,db);
      error = 1;
      goto exit;
    }
    result = 0;
  }
  else
  {
    strend(path)[-1]=0;				// Remove last '/' from path
    if (my_mkdir(path,0777,MYF(0)) < 0)
    {
      if(thd)
        net_printf(&thd->net,ER_CANT_CREATE_DB,db,my_errno);
      error = 1;
      goto exit;
    }
  }

  if(thd)
  {
    if (!thd->query)
    {
      thd->query = path;
      thd->query_length = (uint) (strxmov(path,"create database ", db, NullS)-
				  path);
    }
    {
      mysql_update_log.write(thd,thd->query, thd->query_length);
      if (mysql_bin_log.is_open())
      {
	Query_log_event qinfo(thd, thd->query);
	mysql_bin_log.write(&qinfo);
      }
    }
    if (thd->query == path)
    {
      thd->query = 0; // just in case
      thd->query_length = 0;
    }
    send_ok(&thd->net, result);
  }
exit:
  VOID(pthread_mutex_unlock(&LOCK_mysql_create_db));
  DBUG_RETURN(error);
}

const char *del_exts[]=
{".frm",".ISM",".ISD",".ISM",".HSH",".DAT",".MRG",".MYI",".MYD", ".db", ".BAK", NullS};
static TYPELIB deletable_extentions=
{array_elements(del_exts)-1,"del_exts", del_exts};


/* db-name is already validated when we come here */
/* If thd == 0, do not write any messages
   This is useful in replication when we want to remove
   a stale database before replacing it with the new one
*/
int mysql_rm_db(THD *thd,char *db,bool if_exists)
{
  long deleted=0;
  int error = 0;
  char	path[FN_REFLEN+16];
  MY_DIR *dirp;
  DBUG_ENTER("mysql_rm_db");

  VOID(pthread_mutex_lock(&LOCK_mysql_create_db));
  VOID(pthread_mutex_lock(&LOCK_open));

  // do not drop database if another thread is holding read lock
  if (global_read_lock)
  {
    if (thd->global_read_lock)
    {
      net_printf(&thd->net, ER_DROP_DB_WITH_READ_LOCK);
      goto exit;
    }
    while (global_read_lock && ! thd->killed)
    {
      (void) pthread_cond_wait(&COND_refresh,&LOCK_open);
    }

    if (thd->killed)
    {
      net_printf(&thd->net, ER_SERVER_SHUTDOWN);
      goto exit;
    }
  }

  (void) sprintf(path,"%s/%s",mysql_data_home,db);
  unpack_dirname(path,path);			// Convert if not unix
  /* See if the directory exists */
  if (!(dirp = my_dir(path,MYF(MY_WME | MY_DONT_SORT))))
  {
    if(thd)
    {
      if (!if_exists)
	net_printf(&thd->net,ER_DB_DROP_EXISTS,db);
      else 
	send_ok(&thd->net,0);
    }
    error = !if_exists;
    goto exit;
  }
  remove_db_from_cache(db);

  if ((deleted=mysql_rm_known_files(thd, dirp, path,0)) >= 0 && thd)
  {
    if (!thd->query)
    {
      thd->query = path;
      thd->query_length = (uint) (strxmov(path,"drop database ", db, NullS)-
				  path);
    }
    mysql_update_log.write(thd, thd->query, thd->query_length);
    if (mysql_bin_log.is_open())
    {
      Query_log_event qinfo(thd, thd->query);
      mysql_bin_log.write(&qinfo);
    }
    if (thd->query == path)
    {
      thd->query = 0; // just in case
      thd->query_length = 0;
    }
    
    send_ok(&thd->net,(ulong) deleted);
  }

exit:
  VOID(pthread_mutex_unlock(&LOCK_open));
  VOID(pthread_mutex_unlock(&LOCK_mysql_create_db));
  DBUG_RETURN(error);
}

/*
  Removes files with known extensions plus all found subdirectories that
  are 2 digits (raid directories).
*/

/* This one also needs to work with thd == 0 for replication */
static long mysql_rm_known_files(THD *thd, MY_DIR *dirp, const char *org_path,
				 uint level)
{
  long deleted=0;
  ulong found_other_files=0;
  char filePath[FN_REFLEN];
  DBUG_ENTER("mysql_rm_known_files");
  DBUG_PRINT("enter",("path: %s", org_path));
  /* remove all files with known extensions */

  for (uint idx=2 ;
       idx < (uint) dirp->number_off_files && (!thd || !thd->killed) ;
       idx++)
  {
    FILEINFO *file=dirp->dir_entry+idx;
    DBUG_PRINT("info",("Examining: %s", file->name));

    /* Check if file is a raid directory */
    if (isdigit(file->name[0]) && isdigit(file->name[1]) &&
	!file->name[2] && !level)
    {
      char newpath[FN_REFLEN];
      MY_DIR *new_dirp;
      strxmov(newpath,org_path,"/",file->name,NullS);
      unpack_filename(newpath,newpath);
      if ((new_dirp = my_dir(newpath,MYF(MY_DONT_SORT))))
      {
	DBUG_PRINT("my",("New subdir found: %s", newpath));
	if ((mysql_rm_known_files(thd,new_dirp,newpath,1)) < 0)
	{
	  my_dirend(dirp);
	  DBUG_RETURN(-1);
	}
      }
      continue;
    }
    if (find_type(fn_ext(file->name),&deletable_extentions,1+2) <= 0)
    {
      found_other_files++;
      continue;
    }
    strxmov(filePath,org_path,"/",file->name,NullS);
    unpack_filename(filePath,filePath);
    if (my_delete_with_symlink(filePath,MYF(MY_WME)))
    {
      if(thd)
        net_printf(&thd->net,ER_DB_DROP_DELETE,filePath,my_error);
      my_dirend(dirp);
      DBUG_RETURN(-1);
    }
    deleted++;
  }

  my_dirend(dirp);

  if (thd && thd->killed)
  {
    send_error(&thd->net,ER_SERVER_SHUTDOWN);
    DBUG_RETURN(-1);
  }

  /*
    If the directory is a symbolic link, remove the link first, then
    remove the directory the symbolic link pointed at
  */
  if (!found_other_files)
  {
    char tmp_path[FN_REFLEN];
    char *path=unpack_filename(tmp_path,org_path);
#ifdef HAVE_READLINK
    int linkcount = readlink(path,filePath,sizeof(filePath)-1);
    if (linkcount > 0)			// If the path was a symbolic link
    {
      *(filePath + linkcount) = '\0';
      if (my_delete(path,MYF(!level ? MY_WME : 0)))
      {
	/* Don't give errors if we can't delete 'RAID' directory */
	if (level)
	  DBUG_RETURN(deleted);
	if(thd)
	  send_error(&thd->net);
	DBUG_RETURN(-1);
      }
      path=filePath;
    }
#endif
    /* Remove last FN_LIBCHAR to not cause a probelm on OS/2 */
    char *pos=strend(path);
    if (pos > path && pos[-1] == FN_LIBCHAR)
      *--pos=0;
    /* Don't give errors if we can't delete 'RAID' directory */
    if (rmdir(path) < 0 && !level)
    {
      if(thd)
        net_printf(&thd->net,ER_DB_DROP_RMDIR, path,errno);
      DBUG_RETURN(-1);
    }
  }
  DBUG_RETURN(deleted);
}


bool mysql_change_db(THD *thd,const char *name)
{
  int length;
  char *dbname=my_strdup((char*) name,MYF(MY_WME));
  char	path[FN_REFLEN];
  uint db_access;
  DBUG_ENTER("mysql_change_db");

  if (!dbname || !(length=stripp_sp(dbname)))
  {
    x_free(dbname);				/* purecov: inspected */
    send_error(&thd->net,ER_NO_DB_ERROR);	/* purecov: inspected */
    DBUG_RETURN(1);				/* purecov: inspected */
  }
  if ((length > NAME_LEN) || check_db_name(dbname))
  {
    net_printf(&thd->net,ER_WRONG_DB_NAME, dbname);
    x_free(dbname);
    DBUG_RETURN(1);
  }
  DBUG_PRINT("general",("Use database: %s", dbname));
  if (test_all_bits(thd->master_access,DB_ACLS))
    db_access=DB_ACLS;
  else
    db_access= (acl_get(thd->host,thd->ip,(char*) &thd->remote.sin_addr,
			thd->priv_user,dbname) |
		thd->master_access);
  if (!(db_access & DB_ACLS) && (!grant_option || check_grant_db(thd,dbname)))
  {
    net_printf(&thd->net,ER_DBACCESS_DENIED_ERROR,
	       thd->priv_user,
	       thd->host ? thd->host : thd->ip ? thd->ip : "unknown",
	       dbname);
    mysql_log.write(thd,COM_INIT_DB,ER(ER_DBACCESS_DENIED_ERROR),
		    thd->priv_user,
		    thd->host ? thd->host : thd->ip ? thd->ip : "unknown",
		    dbname);
    my_free(dbname,MYF(0));
    DBUG_RETURN(1);
  }

  (void) sprintf(path,"%s/%s",mysql_data_home,dbname);
  unpack_dirname(path,path);			// Convert if not unix
  if (access(path,F_OK))
  {
    net_printf(&thd->net,ER_BAD_DB_ERROR,dbname);
    my_free(dbname,MYF(0));
    DBUG_RETURN(1);
  }
  send_ok(&thd->net);
  x_free(thd->db);
  thd->db=dbname;
  thd->db_access=db_access;
  DBUG_RETURN(0);
}
