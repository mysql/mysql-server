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

/* Functions for read record cacheing with myisam */
/* Used instead of my_b_read() to allow for no-cacheed seeks */

#include "myisamdef.h"

	/* Copy block from cache if it`s in it. If re_read_if_possibly is */
	/* set read to cache (if after current file-position) else read to */
	/* buff								  */

int _mi_read_cache(IO_CACHE *info, byte *buff, my_off_t pos, uint length,
		   int flag)
{
  uint read_length,in_buff_length;
  my_off_t offset;
  char *in_buff_pos;
  DBUG_ENTER("_mi_read_cache");

  if (pos < info->pos_in_file)
  {
    read_length=length;
    if ((my_off_t) read_length > (my_off_t) (info->pos_in_file-pos))
      read_length=(uint) (info->pos_in_file-pos);
    info->seek_not_done=1;
    if (my_pread(info->file,buff,read_length,pos,MYF(MY_NABP)))
      DBUG_RETURN(1);
    if (!(length-=read_length))
      DBUG_RETURN(0);
    pos+=read_length;
    buff+=read_length;
  }
  if ((offset= (my_off_t) (pos - info->pos_in_file)) <
      (my_off_t) (info->rc_end - info->rc_request_pos))
  {
    in_buff_pos=info->rc_request_pos+(uint) offset;
    in_buff_length= min(length,(uint) (info->rc_end-in_buff_pos));
    memcpy(buff,info->rc_request_pos+(uint) offset,(size_t) in_buff_length);
    if (!(length-=in_buff_length))
      DBUG_RETURN(0);
    pos+=in_buff_length;
    buff+=in_buff_length;
  }
  else
    in_buff_length=0;
  if (flag & READING_NEXT)
  {
    if (pos != ((info)->pos_in_file +
		(uint) ((info)->rc_end - (info)->rc_request_pos)))
    {
      info->pos_in_file=pos;				/* Force start here */
      info->rc_pos=info->rc_end=info->rc_request_pos;	/* Everything used */
      info->seek_not_done=1;
    }
    else
      info->rc_pos=info->rc_end;			/* All block used */
    if (!(*info->read_function)(info,buff,length))
      DBUG_RETURN(0);
    if (!(flag & READING_HEADER) || info->error == -1 ||
	(uint) info->error+in_buff_length < 3)
    {
      DBUG_PRINT("error",
		 ("Error %d reading next-multi-part block (Got %d bytes)",
		  my_errno, info->error));
      if (!my_errno)
	my_errno=HA_ERR_WRONG_IN_RECORD;
      DBUG_RETURN(1);
    }
    bzero(buff+info->error,MI_BLOCK_INFO_HEADER_LENGTH - in_buff_length -
	  (uint) info->error);
    DBUG_RETURN(0);
  }
  info->seek_not_done=1;
  if ((read_length=my_pread(info->file,buff,length,pos,MYF(0))) == length)
    DBUG_RETURN(0);
  if (!(flag & READING_HEADER) || (int) read_length == -1 ||
      read_length+in_buff_length < 3)
  {
    DBUG_PRINT("error",
	       ("Error %d reading new block (Got %d bytes)",
		my_errno, (int) read_length));
    if (!my_errno)
      my_errno=HA_ERR_WRONG_IN_RECORD;
    DBUG_RETURN(1);
  }
  bzero(buff+read_length,MI_BLOCK_INFO_HEADER_LENGTH - in_buff_length -
	read_length);
  DBUG_RETURN(0);
} /* _mi_read_cache */
