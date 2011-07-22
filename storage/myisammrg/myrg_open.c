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

/* open a MyISAM MERGE table */

#include "myrg_def.h"
#include <stddef.h>
#include <errno.h>

/*
	open a MyISAM MERGE table
	if handle_locking is 0 then exit with error if some table is locked
	if handle_locking is 1 then wait if table is locked

        NOTE: This function is not used in the MySQL server. It is for
        MERGE use independent from MySQL. Currently there is some code
        duplication between myrg_open() and myrg_parent_open() +
        myrg_attach_children(). Please duplicate changes in these
        functions or make common sub-functions.
*/

MYRG_INFO *myrg_open(const char *name, int mode, int handle_locking)
{
  int save_errno,errpos=0;
  uint files= 0, i, dir_length, length, UNINIT_VAR(key_parts), min_keys= 0;
  ulonglong file_offset=0;
  char name_buff[FN_REFLEN*2],buff[FN_REFLEN],*end;
  MYRG_INFO *m_info=0;
  File fd;
  IO_CACHE file;
  MI_INFO *isam=0;
  uint found_merge_insert_method= 0;
  size_t name_buff_length;
  my_bool bad_children= FALSE;
  DBUG_ENTER("myrg_open");

  bzero((char*) &file,sizeof(file));
  if ((fd= mysql_file_open(rg_key_file_MRG,
                           fn_format(name_buff, name, "", MYRG_NAME_EXT,
                                     MY_UNPACK_FILENAME|MY_APPEND_EXT),
                           O_RDONLY | O_SHARE, MYF(0))) < 0)
    goto err;
  errpos=1;
  if (init_io_cache(&file, fd, 4*IO_SIZE, READ_CACHE, 0, 0,
		    MYF(MY_WME | MY_NABP)))
    goto err;
  errpos=2;
  dir_length=dirname_part(name_buff, name, &name_buff_length);
  while ((length=my_b_gets(&file,buff,FN_REFLEN-1)))
  {
    if ((end=buff+length)[-1] == '\n')
      end[-1]='\0';
    if (buff[0] && buff[0] != '#')
      files++;
  }

  my_b_seek(&file, 0);
  while ((length=my_b_gets(&file,buff,FN_REFLEN-1)))
  {
    if ((end=buff+length)[-1] == '\n')
      *--end='\0';
    if (!buff[0])
      continue;		/* Skip empty lines */
    if (buff[0] == '#')
    {
      if (!strncmp(buff+1,"INSERT_METHOD=",14))
      {			/* Lookup insert method */
	int tmp= find_type(buff + 15, &merge_insert_method, FIND_TYPE_BASIC);
	found_merge_insert_method = (uint) (tmp >= 0 ? tmp : 0);
      }
      continue;		/* Skip comments */
    }

    if (!has_path(buff))
    {
      (void) strmake(name_buff+dir_length,buff,
                   sizeof(name_buff)-1-dir_length);
      (void) cleanup_dirname(buff,name_buff);
    }
    else
      fn_format(buff, buff, "", "", 0);
    if (!(isam=mi_open(buff,mode,(handle_locking?HA_OPEN_WAIT_IF_LOCKED:0))))
    {
      if (handle_locking & HA_OPEN_FOR_REPAIR)
      {
        myrg_print_wrong_table(buff);
        bad_children= TRUE;
        continue;
      }
      goto bad_children;
    }
    if (!m_info)                                /* First file */
    {
      key_parts=isam->s->base.key_parts;
      if (!(m_info= (MYRG_INFO*) my_malloc(sizeof(MYRG_INFO) +
                                           files*sizeof(MYRG_TABLE) +
                                           key_parts*sizeof(long),
                                           MYF(MY_WME|MY_ZEROFILL))))
        goto err;
      DBUG_ASSERT(files);
      m_info->open_tables=(MYRG_TABLE *) (m_info+1);
      m_info->rec_per_key_part=(ulong *) (m_info->open_tables+files);
      m_info->tables= files;
      files= 0;
      m_info->reclength=isam->s->base.reclength;
      min_keys= isam->s->base.keys;
      errpos=3;
    }
    m_info->open_tables[files].table= isam;
    m_info->open_tables[files].file_offset=(my_off_t) file_offset;
    file_offset+=isam->state->data_file_length;
    files++;
    if (m_info->reclength != isam->s->base.reclength)
    {
      if (handle_locking & HA_OPEN_FOR_REPAIR)
      {
        myrg_print_wrong_table(buff);
        bad_children= TRUE;
        continue;
      }
      goto bad_children;
    }
    m_info->options|= isam->s->options;
    m_info->records+= isam->state->records;
    m_info->del+= isam->state->del;
    m_info->data_file_length+= isam->state->data_file_length;
    if (min_keys > isam->s->base.keys)
      min_keys= isam->s->base.keys;
    for (i=0; i < key_parts; i++)
      m_info->rec_per_key_part[i]+= (isam->s->state.rec_per_key_part[i] /
                                     m_info->tables);
  }

  if (bad_children)
    goto bad_children;
  if (!m_info && !(m_info= (MYRG_INFO*) my_malloc(sizeof(MYRG_INFO),
                                                  MYF(MY_WME | MY_ZEROFILL))))
    goto err;
  /* Don't mark table readonly, for ALTER TABLE ... UNION=(...) to work */
  m_info->options&= ~(HA_OPTION_COMPRESS_RECORD | HA_OPTION_READ_ONLY_DATA);
  m_info->merge_insert_method= found_merge_insert_method;

  if (sizeof(my_off_t) == 4 && file_offset > (ulonglong) (ulong) ~0L)
  {
    my_errno=HA_ERR_RECORD_FILE_FULL;
    goto err;
  }
  m_info->keys= min_keys;
  bzero((char*) &m_info->by_key,sizeof(m_info->by_key));

  /* this works ok if the table list is empty */
  m_info->end_table=m_info->open_tables+files;
  m_info->last_used_table=m_info->open_tables;
  m_info->children_attached= TRUE;

  (void) mysql_file_close(fd, MYF(0));
  end_io_cache(&file);
  mysql_mutex_init(rg_key_mutex_MYRG_INFO_mutex,
                   &m_info->mutex, MY_MUTEX_INIT_FAST);
  m_info->open_list.data=(void*) m_info;
  mysql_mutex_lock(&THR_LOCK_open);
  myrg_open_list=list_add(myrg_open_list,&m_info->open_list);
  mysql_mutex_unlock(&THR_LOCK_open);
  DBUG_RETURN(m_info);

bad_children:
  my_errno= HA_ERR_WRONG_MRG_TABLE_DEF;
err:
  save_errno=my_errno;
  switch (errpos) {
  case 3:
    while (files)
      (void) mi_close(m_info->open_tables[--files].table);
    my_free(m_info);
    /* Fall through */
  case 2:
    end_io_cache(&file);
    /* Fall through */
  case 1:
    (void) mysql_file_close(fd, MYF(0));
  }
  my_errno=save_errno;
  DBUG_RETURN (NULL);
}


/**
  @brief Open parent table of a MyISAM MERGE table.

  @detail Open MERGE meta file to get the table name paths for the child
    tables. Count the children. Allocate and initialize MYRG_INFO
    structure. Call a callback function for each child table.

  @param[in]    parent_name     merge table name path as "database/table"
  @param[in]    callback        function to call for each child table
  @param[in]    callback_param  data pointer to give to the callback

  @return MYRG_INFO pointer
    @retval     != NULL         OK
    @retval     NULL            Error

  @note: Currently there is some code duplication between myrg_open()
    and myrg_parent_open() + myrg_attach_children(). Please duplicate
    changes in these functions or make common sub-functions.
*/

MYRG_INFO *myrg_parent_open(const char *parent_name,
                            int (*callback)(void*, const char*),
                            void *callback_param)
{
  MYRG_INFO *UNINIT_VAR(m_info);
  int       rc;
  int       errpos;
  int       save_errno;
  int       insert_method;
  uint      length;
  uint      child_count;
  File      fd;
  IO_CACHE  file_cache;
  char      parent_name_buff[FN_REFLEN * 2];
  char      child_name_buff[FN_REFLEN];
  DBUG_ENTER("myrg_parent_open");

  rc= 1;
  errpos= 0;
  bzero((char*) &file_cache, sizeof(file_cache));

  /* Open MERGE meta file. */
  if ((fd= mysql_file_open(rg_key_file_MRG,
                           fn_format(parent_name_buff, parent_name,
                                     "", MYRG_NAME_EXT,
                                     MY_UNPACK_FILENAME|MY_APPEND_EXT),
                           O_RDONLY | O_SHARE, MYF(0))) < 0)
    goto err; /* purecov: inspected */
  errpos= 1;

  if (init_io_cache(&file_cache, fd, 4 * IO_SIZE, READ_CACHE, 0, 0,
                    MYF(MY_WME | MY_NABP)))
    goto err; /* purecov: inspected */
  errpos= 2;

  /* Count children. Determine insert method. */
  child_count= 0;
  insert_method= 0;
  while ((length= my_b_gets(&file_cache, child_name_buff, FN_REFLEN - 1)))
  {
    /* Remove line terminator. */
    if (child_name_buff[length - 1] == '\n')
      child_name_buff[--length]= '\0';

    /* Skip empty lines. */
    if (!child_name_buff[0])
      continue; /* purecov: inspected */

    /* Skip comments, but evaluate insert method. */
    if (child_name_buff[0] == '#')
    {
      if (!strncmp(child_name_buff + 1, "INSERT_METHOD=", 14))
      {
        /* Compare buffer with global methods list: merge_insert_method. */
        insert_method= find_type(child_name_buff + 15,
                                 &merge_insert_method, FIND_TYPE_BASIC);
      }
      continue;
    }

    /* Count the child. */
    child_count++;
  }

  /* Allocate MERGE parent table structure. */
  if (!(m_info= (MYRG_INFO*) my_malloc(sizeof(MYRG_INFO) +
                                       child_count * sizeof(MYRG_TABLE),
                                       MYF(MY_WME | MY_ZEROFILL))))
    goto err; /* purecov: inspected */
  errpos= 3;
  m_info->open_tables= (MYRG_TABLE*) (m_info + 1);
  m_info->tables= child_count;
  m_info->merge_insert_method= insert_method > 0 ? insert_method : 0;
  /* This works even if the table list is empty. */
  m_info->end_table= m_info->open_tables + child_count;
  if (!child_count)
  {
    /* Do not attach/detach an empty child list. */
    m_info->children_attached= TRUE;
  }

  /* Call callback for each child. */
  my_b_seek(&file_cache, 0);
  while ((length= my_b_gets(&file_cache, child_name_buff, FN_REFLEN - 1)))
  {
    /* Remove line terminator. */
    if (child_name_buff[length - 1] == '\n')
      child_name_buff[--length]= '\0';

    /* Skip empty lines and comments. */
    if (!child_name_buff[0] || (child_name_buff[0] == '#'))
      continue;

    DBUG_PRINT("info", ("child: '%s'", child_name_buff));

    /* Callback registers child with handler table. */
    if ((rc= (*callback)(callback_param, child_name_buff)))
      goto err; /* purecov: inspected */
  }

  end_io_cache(&file_cache);
  (void) mysql_file_close(fd, MYF(0));
  mysql_mutex_init(rg_key_mutex_MYRG_INFO_mutex,
                   &m_info->mutex, MY_MUTEX_INIT_FAST);

  m_info->open_list.data= (void*) m_info;
  mysql_mutex_lock(&THR_LOCK_open);
  myrg_open_list= list_add(myrg_open_list, &m_info->open_list);
  mysql_mutex_unlock(&THR_LOCK_open);

  DBUG_RETURN(m_info);

  /* purecov: begin inspected */
 err:
  save_errno= my_errno;
  switch (errpos) {
  case 3:
    my_free(m_info);
    /* Fall through */
  case 2:
    end_io_cache(&file_cache);
    /* Fall through */
  case 1:
    (void) mysql_file_close(fd, MYF(0));
  }
  my_errno= save_errno;
  DBUG_RETURN (NULL);
  /* purecov: end */
}


/**
  @brief Attach children to a MyISAM MERGE parent table.

  @detail Call a callback function for each child table.
    The callback returns the MyISAM table handle of the child table.
    Check table definition match.

  @param[in]    m_info            MERGE parent table structure
  @param[in]    handle_locking    if contains HA_OPEN_FOR_REPAIR, warn about
                                  incompatible child tables, but continue
  @param[in]    callback          function to call for each child table
  @param[in]    callback_param    data pointer to give to the callback
  @param[in]    need_compat_check pointer to ha_myisammrg::need_compat_check
                                  (we need this one to decide if previously
                                  allocated buffers can be reused).

  @return status
    @retval     0               OK
    @retval     != 0            Error

  @note: Currently there is some code duplication between myrg_open()
    and myrg_parent_open() + myrg_attach_children(). Please duplicate
    changes in these functions or make common sub-functions.
*/

int myrg_attach_children(MYRG_INFO *m_info, int handle_locking,
                         MI_INFO *(*callback)(void*),
                         void *callback_param, my_bool *need_compat_check)
{
  ulonglong  file_offset;
  MI_INFO    *myisam;
  int        errpos;
  int        save_errno;
  uint       idx;
  uint       child_nr;
  uint       UNINIT_VAR(key_parts);
  uint       min_keys;
  my_bool    bad_children= FALSE;
  my_bool    first_child= TRUE;
  DBUG_ENTER("myrg_attach_children");
  DBUG_PRINT("myrg", ("handle_locking: %d", handle_locking));

  /*
    This function can be called while another thread is trying to abort
    locks of this MERGE table. If the processor reorders instructions or
    write to memory, 'children_attached' could be set before
    'open_tables' has all the pointers to the children. Use of a mutex
    here and in ha_myisammrg::store_lock() forces consistent data.
  */
  mysql_mutex_lock(&m_info->mutex);
  errpos= 0;
  file_offset= 0;
  min_keys= 0;
  for (child_nr= 0; child_nr < m_info->tables; child_nr++)
  {
    if (! (myisam= (*callback)(callback_param)))
    {
      if (handle_locking & HA_OPEN_FOR_REPAIR)
      {
        /* An appropriate error should've been already pushed by callback. */
        bad_children= TRUE;
        continue;
      }
      goto bad_children;
    }

    DBUG_PRINT("myrg", ("child_nr: %u  table: '%s'",
                        child_nr, myisam->filename));

    /* Special handling when the first child is attached. */
    if (first_child)
    {
      first_child= FALSE;
      m_info->reclength= myisam->s->base.reclength;
      min_keys=  myisam->s->base.keys;
      key_parts= myisam->s->base.key_parts;
      if (*need_compat_check && m_info->rec_per_key_part)
      {
        my_free(m_info->rec_per_key_part);
        m_info->rec_per_key_part= NULL;
      }
      if (!m_info->rec_per_key_part)
      {
        if(!(m_info->rec_per_key_part= (ulong*)
             my_malloc(key_parts * sizeof(long), MYF(MY_WME))))
          goto err; /* purecov: inspected */
        errpos= 1;
      }
      bzero((char*) m_info->rec_per_key_part, key_parts * sizeof(long));
    }

    /* Add MyISAM table info. */
    m_info->open_tables[child_nr].table= myisam;
    m_info->open_tables[child_nr].file_offset= (my_off_t) file_offset;
    file_offset+= myisam->state->data_file_length;

    /* Check table definition match. */
    if (m_info->reclength != myisam->s->base.reclength)
    {
      DBUG_PRINT("error", ("definition mismatch table: '%s'  repair: %d",
                           myisam->filename,
                           (handle_locking & HA_OPEN_FOR_REPAIR)));
      if (handle_locking & HA_OPEN_FOR_REPAIR)
      {
        myrg_print_wrong_table(myisam->filename);
        bad_children= TRUE;
        continue;
      }
      goto bad_children;
    }

    m_info->options|= myisam->s->options;
    m_info->records+= myisam->state->records;
    m_info->del+= myisam->state->del;
    m_info->data_file_length+= myisam->state->data_file_length;
    if (min_keys > myisam->s->base.keys)
      min_keys= myisam->s->base.keys; /* purecov: inspected */
    for (idx= 0; idx < key_parts; idx++)
      m_info->rec_per_key_part[idx]+= (myisam->s->state.rec_per_key_part[idx] /
                                       m_info->tables);
  }

  if (bad_children)
    goto bad_children;

  if (sizeof(my_off_t) == 4 && file_offset > (ulonglong) (ulong) ~0L)
  {
    my_errno= HA_ERR_RECORD_FILE_FULL;
    goto err;
  }
  /* Don't mark table readonly, for ALTER TABLE ... UNION=(...) to work */
  m_info->options&= ~(HA_OPTION_COMPRESS_RECORD | HA_OPTION_READ_ONLY_DATA);
  m_info->keys= min_keys;
  m_info->last_used_table= m_info->open_tables;
  m_info->children_attached= TRUE;
  mysql_mutex_unlock(&m_info->mutex);
  DBUG_RETURN(0);

bad_children:
  my_errno= HA_ERR_WRONG_MRG_TABLE_DEF;
err:
  save_errno= my_errno;
  switch (errpos) {
  case 1:
    my_free(m_info->rec_per_key_part);
    m_info->rec_per_key_part= NULL;
  }
  mysql_mutex_unlock(&m_info->mutex);
  my_errno= save_errno;
  DBUG_RETURN(1);
}


/**
  @brief Detach children from a MyISAM MERGE parent table.

  @param[in]    m_info          MERGE parent table structure

  @note Detach must not touch the children in any way.
    They may have been closed at ths point already.
    All references to the children should be removed.

  @return status
    @retval     0               OK
*/

int myrg_detach_children(MYRG_INFO *m_info)
{
  DBUG_ENTER("myrg_detach_children");
  /* For symmetry with myrg_attach_children() we use the mutex here. */
  mysql_mutex_lock(&m_info->mutex);
  if (m_info->tables)
  {
    /* Do not attach/detach an empty child list. */
    m_info->children_attached= FALSE;
    bzero((char*) m_info->open_tables, m_info->tables * sizeof(MYRG_TABLE));
  }
  m_info->records= 0;
  m_info->del= 0;
  m_info->data_file_length= 0;
  m_info->options= 0;
  mysql_mutex_unlock(&m_info->mutex);
  DBUG_RETURN(0);
}

