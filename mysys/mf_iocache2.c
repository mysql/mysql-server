/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/*
  More functions to be used with IO_CACHE files
*/

#define MAP_TO_USE_RAID
#include "mysys_priv.h"
#include <m_string.h>
#include <stdarg.h>
#include <m_ctype.h>

/*
** Fix that next read will be made at certain position
** For write cache, make next write happen at a certain position
*/

void my_b_seek(IO_CACHE *info,my_off_t pos)
{
  if (info->type == READ_CACHE)
  {
    info->rc_pos=info->rc_end=info->buffer;
  }
  else if (info->type == WRITE_CACHE)
  {
    byte* try_rc_pos;
    try_rc_pos = info->rc_pos + (pos - info->pos_in_file);
    if (try_rc_pos >= info->buffer && try_rc_pos <= info->rc_end)
      info->rc_pos = try_rc_pos;
    else
      flush_io_cache(info);
  }
  info->pos_in_file=pos;
  info->seek_not_done=1;
}

/*
**  Fill buffer.  Note that this assumes that you have already used
**  all characters in the CACHE, independent of the rc_pos value!
**  return:  0 on error or EOF (info->error = -1 on error)
**           number of characters
*/

uint my_b_fill(IO_CACHE *info)
{
  my_off_t pos_in_file=info->pos_in_file+(uint) (info->rc_end - info->buffer);
  my_off_t max_length;
  uint diff_length,length;
  if (info->seek_not_done)
  {					/* File touched, do seek */
    if (my_seek(info->file,pos_in_file,MY_SEEK_SET,MYF(0)) ==
	MY_FILEPOS_ERROR)
    {
      info->error= 0;
      return 0;
    }
    info->seek_not_done=0;
  }
  diff_length=(uint) (pos_in_file & (IO_SIZE-1));
  max_length= (my_off_t) (info->end_of_file - pos_in_file);
  if (max_length > (my_off_t) (info->read_length-diff_length))
    max_length=(my_off_t) (info->read_length-diff_length);
  if (!max_length)
  {
    info->error= 0;
    return 0;					/* EOF */
  }
   else if ((length=my_read(info->file,info->buffer,(uint) max_length,
			   info->myflags)) == (uint) -1)
  {
    info->error= -1;
    return 0;
  }
  info->rc_pos=info->buffer;
  info->rc_end=info->buffer+length;
  info->pos_in_file=pos_in_file;
  return length;
}

/*
** Read a string ended by '\n' into a buffer of 'max_length' size.
** Returns number of characters read, 0 on error.
** last byte is set to '\0'
*/

uint my_b_gets(IO_CACHE *info, char *to, uint max_length)
{
  uint length;
  max_length--;					/* Save place for end \0 */
  /* Calculate number of characters in buffer */
  if (!(length= my_b_bytes_in_cache(info)) &&
      !(length= my_b_fill(info)))
    return 0;
  for (;;)
  {
    char *pos,*end;
    if (length > max_length)
      length=max_length;
    for (pos=info->rc_pos,end=pos+length ; pos < end ;)
    {
      if ((*to++ = *pos++) == '\n')
      {
	length= (uint) (pos-info->rc_pos);
	info->rc_pos=pos;
	*to='\0';
	return length;
      }
    }
    if (!(max_length-=length))
    {
     /* Found enough charcters;  Return found string */
      info->rc_pos=pos;
      *to='\0';
      return length;
    }
    if (!(length=my_b_fill(info)))
      return 0;
  }
}

/*
  Simple printf version.  Supports '%s', '%d', '%u', "%ld" and "%lu"
  Used for logging in MySQL
  returns number of written character, or (uint) -1 on error
*/

uint my_b_printf(IO_CACHE *info, const char* fmt, ...)
{
  int result;
  va_list args;
  va_start(args,fmt);
  result=my_b_vprintf(info, fmt, args);
  va_end(args);
  return result;
}


uint my_b_vprintf(IO_CACHE *info, const char* fmt, va_list args)
{
  uint out_length=0;

  for (; *fmt ; fmt++)
  {
    if (*fmt++ != '%')
    {
      /* Copy everything until '%' or end of string */
      const char *start=fmt-1;
      uint length;
      for (; *fmt && *fmt != '%' ; fmt++ ) ;
      length= (uint) (fmt - start);
      out_length+=length;
      if (my_b_write(info, start, length))
	goto err;
      if (!*fmt)				/* End of format */
      {
	return out_length;
      }
      fmt++;
      /* Found one '%' */
    }
    /* Skipp if max size is used (to be compatible with printf) */
    while (isdigit(*fmt) || *fmt == '.' || *fmt == '-')
      fmt++;
    if (*fmt == 's')				/* String parameter */
    {
      reg2 char *par = va_arg(args, char *);
      uint length = (uint) strlen(par);
      out_length+=length;
      if (my_b_write(info, par, length))
	goto err;
    }
    else if (*fmt == 'd' || *fmt == 'u')	/* Integer parameter */
    {
      register int iarg;
      uint length;
      char buff[17];

      iarg = va_arg(args, int);
      if (*fmt == 'd')
	length= (uint) (int10_to_str((long) iarg,buff, -10) - buff);
      else
	length= (uint) (int10_to_str((long) (uint) iarg,buff,10)- buff);
      out_length+=length;
      if (my_b_write(info, buff, length))
	goto err;
    }
    else if ((*fmt == 'l' && fmt[1] == 'd') || fmt[1] == 'u')
      /* long parameter */
    {
      register long iarg;
      uint length;
      char buff[17];

      iarg = va_arg(args, long);
      if (*++fmt == 'd')
	length= (uint) (int10_to_str(iarg,buff, -10) - buff);
      else
	length= (uint) (int10_to_str(iarg,buff,10)- buff);
      out_length+=length;
      if (my_b_write(info, buff, length))
	goto err;
    }
    else
    {
      /* %% or unknown code */
      if (my_b_write(info, "%", 1))
	goto err;
      out_length++;
    }
  }
  return out_length;

err:
  return (uint) -1;
}
