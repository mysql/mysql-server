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

/* open a MYMERGE_-database */

#include "mymrgdef.h"
#include <stddef.h>
#include <errno.h>
#ifdef VMS
#include "mrg_static.c"
#endif

/*	open a MYMERGE_-database.

	if handle_locking is 0 then exit with error if some database is locked
	if handle_locking is 1 then wait if database is locked
*/


MYRG_INFO *myrg_open(
const char *name,
int mode,
int handle_locking)
{
  int save_errno,errpos;
  uint files,dir_length,length;
  ulonglong file_offset;
  char name_buff[FN_REFLEN*2],buff[FN_REFLEN],*end;
  MYRG_INFO *m_info;
  File fd;
  IO_CACHE file;
  MI_INFO *isam;
  DBUG_ENTER("myrg_open");

  LINT_INIT(m_info);
  isam=0;
  errpos=files=0;
  bzero((char*) &file,sizeof(file));
  if ((fd=my_open(fn_format(name_buff,name,"",MYRG_NAME_EXT,4),
		  O_RDONLY | O_SHARE,MYF(0))) < 0)
    goto err;
  errpos=1;
  if (init_io_cache(&file, fd, 4*IO_SIZE, READ_CACHE, 0, 0,
		    MYF(MY_WME | MY_NABP)))
    goto err;
  errpos=2;
  dir_length=dirname_part(name_buff,name);
  while ((length=my_b_gets(&file,buff,FN_REFLEN-1)))
  {
    if ((end=buff+length)[-1] == '\n')
      end[-1]='\0';
    if (buff[0] && buff[0] != '#')	/* Skipp empty lines and comments */
    {
      files++;
    }
  }

  if (!(m_info= (MYRG_INFO*) my_malloc(sizeof(MYRG_INFO)+
				       files*sizeof(MYRG_TABLE),
				       MYF(MY_WME|MY_ZEROFILL))))
    goto err;
  errpos=3;
  m_info->open_tables=(files) ? (MYRG_TABLE *) (m_info+1) : 0;
  m_info->tables=files;
  files=0;
  file_offset=0;

  my_b_seek(&file, 0);
  while ((length=my_b_gets(&file,buff,FN_REFLEN-1)))
  {
    if ((end=buff+length)[-1] == '\n')
      end[-1]='\0';
    if (buff[0] && buff[0] == '#')	/* Skipp empty lines and comments */
      continue;
    if (!test_if_hard_path(buff))
    {
      VOID(strmake(name_buff+dir_length,buff,
                   sizeof(name_buff)-1-dir_length));
      VOID(cleanup_dirname(buff,name_buff));
    }
    if (!(isam=mi_open(buff,mode,test(handle_locking))))
      goto err;
    m_info->open_tables[files].table= isam;
    m_info->open_tables[files].file_offset=(my_off_t) file_offset;
    file_offset+=isam->state->data_file_length;
    files++;
    if (m_info->reclength && (m_info->reclength != isam->s->base.reclength))
    {
      my_errno=HA_ERR_WRONG_IN_RECORD;
      goto err;
    }
    m_info->reclength=isam->s->base.reclength;
    m_info->options|= isam->s->options;
    m_info->records+= isam->state->records;
    m_info->del+= isam->state->del;
    m_info->data_file_length+= isam->state->data_file_length;
  }

  if (sizeof(my_off_t) == 4 && file_offset > (ulonglong) (ulong) ~0L)
  {
    my_errno=HA_ERR_RECORD_FILE_FULL;
    goto err;
  }
  m_info->keys=(files) ? isam->s->base.keys : 0;
  bzero((char*) &m_info->by_key,sizeof(m_info->by_key));

  /* this works ok if the table list is empty */
  m_info->end_table=m_info->open_tables+files;
  m_info->last_used_table=m_info->open_tables;

  VOID(my_close(fd,MYF(0)));
  end_io_cache(&file);
  m_info->open_list.data=(void*) m_info;
  pthread_mutex_lock(&THR_LOCK_open);
  myrg_open_list=list_add(myrg_open_list,&m_info->open_list);
  pthread_mutex_unlock(&THR_LOCK_open);
  DBUG_RETURN(m_info);

err:
  save_errno=my_errno;
  switch (errpos) {
  case 3:
    while (files)
      mi_close(m_info->open_tables[--files].table);
    my_free((char*) m_info,MYF(0));
    /* Fall through */
  case 2:
    end_io_cache(&file);
    /* Fall through */
  case 1:
    VOID(my_close(fd,MYF(0)));
  }
  my_errno=save_errno;
  DBUG_RETURN (NULL);
}
