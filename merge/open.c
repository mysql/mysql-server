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

/* open a MERGE-database */

#include "mrgdef.h"
#include <stddef.h>
#include <errno.h>
#ifdef VMS
#include "static.c"
#endif

/*	open a MERGE-database.

	if handle_locking is 0 then exit with error if some database is locked
	if handle_locking is 1 then wait if database is locked
*/


MRG_INFO *mrg_open(name,mode,handle_locking)
const char *name;
int mode;
int handle_locking;
{
  int save_errno,i,errpos;
  uint files,dir_length;
  ulonglong file_offset;
  char name_buff[FN_REFLEN*2],buff[FN_REFLEN],*end;
  MRG_INFO info,*m_info;
  FILE *file;
  N_INFO *isam,*last_isam;
  DBUG_ENTER("mrg_open");

  LINT_INIT(last_isam);
  isam=0;
  errpos=files=0;
  bzero((gptr) &info,sizeof(info));
  if (!(file=my_fopen(fn_format(name_buff,name,"",MRG_NAME_EXT,4),
		      O_RDONLY | O_SHARE,MYF(0))))
    goto err;
  errpos=1;
  dir_length=dirname_part(name_buff,name);
  info.reclength=0;
  while (fgets(buff,FN_REFLEN-1,file))
  {
    if ((end=strend(buff))[-1] == '\n')
      end[-1]='\0';
    if (buff[0])		/* Skipp empty lines */
    {
      last_isam=isam;
      if (!test_if_hard_path(buff))
      {
	VOID(strmake(name_buff+dir_length,buff,
		     sizeof(name_buff)-1-dir_length));
	VOID(cleanup_dirname(buff,name_buff));
      }
      if (!(isam=nisam_open(buff,mode,test(handle_locking))))
	goto err;
      files++;
    }
    last_isam=isam;
    if (info.reclength && info.reclength != isam->s->base.reclength)
    {
      my_errno=HA_ERR_WRONG_IN_RECORD;
      goto err;
    }
    info.reclength=isam->s->base.reclength;
  }
  if (!(m_info= (MRG_INFO*) my_malloc(sizeof(MRG_INFO)+files*sizeof(MRG_TABLE),
				      MYF(MY_WME))))
    goto err;
  *m_info=info;
  m_info->open_tables=(MRG_TABLE *) (m_info+1);
  m_info->tables=files;

  for (i=files ; i-- > 0 ; )
  {
    m_info->open_tables[i].table=isam;
    m_info->options|=isam->s->base.options;
    m_info->records+=isam->s->state.records;
    m_info->del+=isam->s->state.del;
    m_info->data_file_length=isam->s->state.data_file_length;
    if (i)
      isam=(N_INFO*) (isam->open_list.next->data);
  }
  /* Fix fileinfo for easyer debugging (actually set by rrnd) */
  file_offset=0;
  for (i=0 ; (uint) i < files ; i++)
  {
    m_info->open_tables[i].file_offset=(my_off_t) file_offset;
    file_offset+=m_info->open_tables[i].table->s->state.data_file_length;
  }
  if (sizeof(my_off_t) == 4 && file_offset > (ulonglong) (ulong) ~0L)
  {
    my_errno=HA_ERR_RECORD_FILE_FULL;
    my_free((char*) m_info,MYF(0));
    goto err;
  }

  m_info->end_table=m_info->open_tables+files;
  m_info->last_used_table=m_info->open_tables;

  VOID(my_fclose(file,MYF(0)));
  m_info->open_list.data=(void*) m_info;
  pthread_mutex_lock(&THR_LOCK_open);
  mrg_open_list=list_add(mrg_open_list,&m_info->open_list);
  pthread_mutex_unlock(&THR_LOCK_open);
  DBUG_RETURN(m_info);

err:
  save_errno=my_errno;
  switch (errpos) {
  case 1:
    VOID(my_fclose(file,MYF(0)));
    for (i=files ; i-- > 0 ; )
    {
      isam=last_isam;
      if (i)
	last_isam=(N_INFO*) (isam->open_list.next->data);
      nisam_close(isam);
    }
  }
  my_errno=save_errno;
  DBUG_RETURN (NULL);
}
