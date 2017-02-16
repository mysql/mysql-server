/*
   Copyright (c) 2000, 2010, Oracle and/or its affiliates

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

/* Read and write key blocks */

#include "myisamdef.h"

	/* Fetch a key-page in memory */

uchar *_mi_fetch_keypage(register MI_INFO *info, MI_KEYDEF *keyinfo,
			 my_off_t page, int level, 
                         uchar *buff, int return_buffer)
{
  uchar *tmp;
  uint page_size;
  DBUG_ENTER("_mi_fetch_keypage");
  DBUG_PRINT("enter",("page: %ld", (long) page));

  tmp=(uchar*) key_cache_read(info->s->key_cache,
                             info->s->kfile, page, level, (uchar*) buff,
			     (uint) keyinfo->block_length,
			     (uint) keyinfo->block_length,
			     return_buffer);
  if (tmp == info->buff)
    info->buff_used=1;
  else if (!tmp)
  {
    DBUG_PRINT("error",("Got errno: %d from key_cache_read",my_errno));
    info->last_keypage=HA_OFFSET_ERROR;
    mi_print_error(info->s, HA_ERR_CRASHED);
    my_errno=HA_ERR_CRASHED;
    DBUG_RETURN(0);
  }
  info->last_keypage=page;
  page_size=mi_getint(tmp);
  if (page_size < 4 || page_size > keyinfo->block_length)
  {
    DBUG_PRINT("error",("page %lu had wrong page length: %u",
			(ulong) page, page_size));
    DBUG_DUMP("page", tmp, keyinfo->block_length);
    info->last_keypage = HA_OFFSET_ERROR;
    mi_print_error(info->s, HA_ERR_CRASHED);
    my_errno = HA_ERR_CRASHED;
    tmp = 0;
  }
  DBUG_RETURN(tmp);
} /* _mi_fetch_keypage */


	/* Write a key-page on disk */

int _mi_write_keypage(register MI_INFO *info, register MI_KEYDEF *keyinfo,
		      my_off_t page, int level, uchar *buff)
{
  reg3 uint length;
  DBUG_ENTER("_mi_write_keypage");

#ifndef FAST					/* Safety check */
  if (page < info->s->base.keystart ||
      page+keyinfo->block_length > info->state->key_file_length ||
      (page & (MI_MIN_KEY_BLOCK_LENGTH-1)))
  {
    DBUG_PRINT("error",("Trying to write inside key status region: key_start: %lu  length: %lu  page: %lu",
			(long) info->s->base.keystart,
			(long) info->state->key_file_length,
			(long) page));
    my_errno=EINVAL;
    DBUG_RETURN((-1));
  }
  DBUG_PRINT("page",("write page at: %lu",(long) page));
  DBUG_DUMP("buff",(uchar*) buff,mi_getint(buff));
#endif

  if ((length=keyinfo->block_length) > IO_SIZE*2 &&
      info->state->key_file_length != page+length)
    length= ((mi_getint(buff)+IO_SIZE-1) & (uint) ~(IO_SIZE-1));
  DBUG_RETURN((key_cache_write(info->s->key_cache,
			       info->s->kfile, &info->s->dirty_part_map,
                               page, level, (uchar*) buff, length,
			       (uint) keyinfo->block_length,
			       (int) ((info->lock_type != F_UNLCK) ||
				     info->s->delay_key_write))));
} /* mi_write_keypage */


	/* Remove page from disk */

int _mi_dispose(register MI_INFO *info, MI_KEYDEF *keyinfo, my_off_t pos,
                int level)
{
  my_off_t old_link;
  uchar buff[8];
  DBUG_ENTER("_mi_dispose");
  DBUG_PRINT("enter",("pos: %ld", (long) pos));

  old_link= info->s->state.key_del[keyinfo->block_size_index];
  info->s->state.key_del[keyinfo->block_size_index]= pos;
  mi_sizestore(buff,old_link);
  info->s->state.changed|= STATE_NOT_SORTED_PAGES;
  DBUG_RETURN(key_cache_write(info->s->key_cache,
                              info->s->kfile, &info->s->dirty_part_map,
                              pos , level, buff,
			      sizeof(buff),
			      (uint) keyinfo->block_length,
			      (int) (info->lock_type != F_UNLCK)));
} /* _mi_dispose */


	/* Make new page on disk */

my_off_t _mi_new(register MI_INFO *info, MI_KEYDEF *keyinfo, int level)
{
  my_off_t pos;
  uchar buff[8];
  DBUG_ENTER("_mi_new");

  if ((pos= info->s->state.key_del[keyinfo->block_size_index]) ==
      HA_OFFSET_ERROR)
  {
    if (info->state->key_file_length >=
	info->s->base.max_key_file_length - keyinfo->block_length)
    {
      my_errno=HA_ERR_INDEX_FILE_FULL;
      DBUG_RETURN(HA_OFFSET_ERROR);
    }
    pos=info->state->key_file_length;
    info->state->key_file_length+= keyinfo->block_length;
  }
  else
  {
    if (!key_cache_read(info->s->key_cache,
                        info->s->kfile, pos, level,
			buff,
			(uint) sizeof(buff),
			(uint) keyinfo->block_length,0))
      pos= HA_OFFSET_ERROR;
    else
      info->s->state.key_del[keyinfo->block_size_index]= mi_sizekorr(buff);
  }
  info->s->state.changed|= STATE_NOT_SORTED_PAGES;
  DBUG_PRINT("exit",("Pos: %ld",(long) pos));
  DBUG_RETURN(pos);
} /* _mi_new */
