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

/* Read and write key blocks */

#include "myisamdef.h"
#ifdef	__WIN__
#include <errno.h>
#endif

	/* Fetch a key-page in memory */

uchar *_mi_fetch_keypage(register MI_INFO *info, MI_KEYDEF *keyinfo,
			 my_off_t page, uchar *buff, int return_buffer)
{
  uchar *tmp;
  uint page_size;
  tmp=(uchar*) key_cache_read(info->s->kfile,page,(byte*) buff,
			     (uint) keyinfo->block_length,
			     (uint) keyinfo->block_length,
			     return_buffer);
  if (tmp == info->buff)
    info->buff_used=1;
  else if (!tmp)
  {
    DBUG_PRINT("error",("Got errno: %d from key_cache_read",my_errno));
    info->last_keypage=HA_OFFSET_ERROR;
    my_errno=HA_ERR_CRASHED;
    return 0;
  }
  info->last_keypage=page;
  page_size=mi_getint(tmp);
  if (page_size < 4 || page_size > keyinfo->block_length)
  {
    DBUG_PRINT("error",("page %lu had wrong page length: %u",
			(ulong) page, page_size));
    info->last_keypage = HA_OFFSET_ERROR;
    my_errno = HA_ERR_CRASHED;
    tmp = 0;
  }
  return tmp;
} /* _mi_fetch_keypage */


	/* Write a key-page on disk */

int _mi_write_keypage(register MI_INFO *info, register MI_KEYDEF *keyinfo,
		      my_off_t page, uchar *buff)
{
  reg3 uint length;
#ifndef FAST					/* Safety check */
  if (page < info->s->base.keystart ||
      page+keyinfo->block_length > info->state->key_file_length ||
      page & (myisam_block_size-1))
  {
    DBUG_PRINT("error",("Trying to write outside key region: %lu",
			(long) page));
    my_errno=EINVAL;
    return(-1);
  }
  DBUG_PRINT("page",("write page at: %lu",(long) page,buff));
  DBUG_DUMP("buff",(byte*) buff,mi_getint(buff));
#endif

  if ((length=keyinfo->block_length) > IO_SIZE*2 &&
      info->state->key_file_length != page+length)
    length= ((mi_getint(buff)+IO_SIZE-1) & (uint) ~(IO_SIZE-1));
#ifdef HAVE_purify
  {
    length=mi_getint(buff);
    bzero((byte*) buff+length,keyinfo->block_length-length);
    length=keyinfo->block_length;
  }
#endif
  return (key_cache_write(info->s->kfile,page,(byte*) buff,length,
			 (uint) keyinfo->block_length,
			 (int) ((info->lock_type != F_UNLCK) ||
				info->s->delay_key_write)));
} /* mi_write_keypage */


	/* Remove page from disk */

int _mi_dispose(register MI_INFO *info, MI_KEYDEF *keyinfo, my_off_t pos)
{
  my_off_t old_link;
  char buff[8];
  DBUG_ENTER("_mi_dispose");
  DBUG_PRINT("enter",("pos: %ld", (long) pos));

  old_link=info->s->state.key_del[keyinfo->block_size];
  info->s->state.key_del[keyinfo->block_size]=pos;
  mi_sizestore(buff,old_link);
  info->s->state.changed|= STATE_NOT_SORTED_PAGES;
  DBUG_RETURN(key_cache_write(info->s->kfile,pos,buff,
			      sizeof(buff),
			      (uint) keyinfo->block_length,
			      (int) (info->lock_type != F_UNLCK)));
} /* _mi_dispose */


	/* Make new page on disk */

my_off_t _mi_new(register MI_INFO *info, MI_KEYDEF *keyinfo)
{
  my_off_t pos;
  char buff[8];
  DBUG_ENTER("_mi_new");

  if ((pos=info->s->state.key_del[keyinfo->block_size]) == HA_OFFSET_ERROR)
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
    if (!key_cache_read(info->s->kfile,pos,
			buff,
			(uint) sizeof(buff),
			(uint) keyinfo->block_length,0))
      pos= HA_OFFSET_ERROR;
    else
      info->s->state.key_del[keyinfo->block_size]=mi_sizekorr(buff);
  }
  info->s->state.changed|= STATE_NOT_SORTED_PAGES;
  DBUG_PRINT("exit",("Pos: %ld",(long) pos));
  DBUG_RETURN(pos);
} /* _mi_new */
