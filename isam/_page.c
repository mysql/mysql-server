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

/* L{ser och skriver nyckelblock */

#include "isamdef.h"
#ifdef	__WIN__
#include <errno.h>
#endif

	/* Fetch a key-page in memory */

uchar *_nisam_fetch_keypage(register N_INFO *info, N_KEYDEF *keyinfo,
			    my_off_t page, uchar *buff, int return_buffer)
{
  uchar *tmp;
  tmp=(uchar*) key_cache_read(info->s->kfile,page,(byte*) buff,
			     (uint) keyinfo->base.block_length,
			     (uint) keyinfo->base.block_length,
			     return_buffer);
  if (tmp == info->buff)
  {
    info->update|=HA_STATE_BUFF_SAVED;
    info->int_pos=(ulong) page;
    info->buff_used=1;
  }
  else
  {
    info->update&= ~HA_STATE_BUFF_SAVED;
    if (tmp)
      info->int_pos=(ulong) page;
    else
    {
      info->int_pos=NI_POS_ERROR;
      DBUG_PRINT("error",("Got errno: %d from key_cache_read",my_errno));
      my_errno=HA_ERR_CRASHED;
    }
  }
  return tmp;
} /* _nisam_fetch_keypage */


	/* Write a key-page on disk */

int _nisam_write_keypage(register N_INFO *info, register N_KEYDEF *keyinfo,
		      my_off_t page, uchar *buff)
{
  reg3 uint length;
#ifndef QQ					/* Safety check */
  if (page < info->s->base.keystart ||
      page+keyinfo->base.block_length > info->s->state.key_file_length ||
      page & (nisam_block_size-1))
  {
    DBUG_PRINT("error",("Trying to write outside key region: %lu",
			(long) page));
    my_errno=EINVAL;
    return(-1);
  }
  DBUG_PRINT("page",("write page at: %lu",(long) page,buff));
  DBUG_DUMP("buff",(byte*) buff,getint(buff));
#endif

  if ((length=keyinfo->base.block_length) > IO_SIZE*2 &&
       info->s->state.key_file_length != page+length)
    length= ((getint(buff)+IO_SIZE-1) & (uint) ~(IO_SIZE-1));
#ifdef HAVE_purify
  {
    length=getint(buff);
    bzero((byte*) buff+length,keyinfo->base.block_length-length);
    length=keyinfo->base.block_length;
  }
#endif
  return (key_cache_write(info->s->kfile,page,(byte*) buff,length,
			 (uint) keyinfo->base.block_length,
			 (int) (info->lock_type != F_UNLCK)));
} /* nisam_write_keypage */


	/* Remove page from disk */

int _nisam_dispose(register N_INFO *info, N_KEYDEF *keyinfo, my_off_t pos)
{
  uint keynr= (uint) (keyinfo - info->s->keyinfo);
  ulong old_link;				/* ulong is ok here */
  DBUG_ENTER("_nisam_dispose");

  old_link=info->s->state.key_del[keynr];
  info->s->state.key_del[keynr]=(ulong) pos;
  DBUG_RETURN(key_cache_write(info->s->kfile,pos,(byte*) &old_link,
			      sizeof(long),
			      (uint) keyinfo->base.block_length,
			      (int) (info->lock_type != F_UNLCK)));
} /* _nisam_dispose */


	/* Make new page on disk */

ulong _nisam_new(register N_INFO *info, N_KEYDEF *keyinfo)
{
  uint keynr= (uint) (keyinfo - info->s->keyinfo);
  ulong pos;
  DBUG_ENTER("_nisam_new");

  if ((pos=info->s->state.key_del[keynr]) == NI_POS_ERROR)
  {
    if (info->s->state.key_file_length >= info->s->base.max_key_file_length)
    {
      my_errno=HA_ERR_INDEX_FILE_FULL;
      DBUG_RETURN(NI_POS_ERROR);
    }
    pos=info->s->state.key_file_length;
    info->s->state.key_file_length+= keyinfo->base.block_length;
  }
  else
  {
    if (!key_cache_read(info->s->kfile,pos,
			(byte*) &info->s->state.key_del[keynr],
			(uint) sizeof(long),
			(uint) keyinfo->base.block_length,0))
      pos= NI_POS_ERROR;
  }
  DBUG_PRINT("exit",("Pos: %d",pos));
  DBUG_RETURN(pos);
} /* _nisam_new */
