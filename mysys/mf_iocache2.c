/* Copyright (C) 2000 MySQL AB

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

/*
  More functions to be used with IO_CACHE files
*/

#define MAP_TO_USE_RAID
#include "mysys_priv.h"
#include <m_string.h>
#include <stdarg.h>
#include <m_ctype.h>

my_off_t my_b_append_tell(IO_CACHE* info)
{
  /*
    Prevent optimizer from putting res in a register when debugging
    we need this to be able to see the value of res when the assert fails
  */
  dbug_volatile my_off_t res; 

  /*
    We need to lock the append buffer mutex to keep flush_io_cache()
    from messing with the variables that we need in order to provide the
    answer to the question.
  */
#ifdef THREAD
  pthread_mutex_lock(&info->append_buffer_lock);
#endif
#ifndef DBUG_OFF
  /*
    Make sure EOF is where we think it is. Note that we cannot just use
    my_tell() because we have a reader thread that could have left the
    file offset in a non-EOF location
  */
  {
    volatile my_off_t save_pos;
    save_pos = my_tell(info->file,MYF(0));
    my_seek(info->file,(my_off_t)0,MY_SEEK_END,MYF(0));
    /*
      Save the value of my_tell in res so we can see it when studying coredump
    */
    DBUG_ASSERT(info->end_of_file - (info->append_read_pos-info->write_buffer)
		== (res=my_tell(info->file,MYF(0))));
    my_seek(info->file,save_pos,MY_SEEK_SET,MYF(0));
  }
#endif  
  res = info->end_of_file + (info->write_pos-info->append_read_pos);
#ifdef THREAD
  pthread_mutex_unlock(&info->append_buffer_lock);
#endif
  return res;
}

my_off_t my_b_safe_tell(IO_CACHE *info)
{
  if (unlikely(info->type == SEQ_READ_APPEND))
    return my_b_append_tell(info);
  return my_b_tell(info);
}

/*
  Make next read happen at the given position
  For write cache, make next write happen at the given position
*/

void my_b_seek(IO_CACHE *info,my_off_t pos)
{
  my_off_t offset;
  DBUG_ENTER("my_b_seek");
  DBUG_PRINT("enter",("pos: %lu", (ulong) pos));

  /*
    TODO:
       Verify that it is OK to do seek in the non-append
       area in SEQ_READ_APPEND cache
     a) see if this always works
     b) see if there is a better way to make it work
  */
  if (info->type == SEQ_READ_APPEND)
    VOID(flush_io_cache(info));

  offset=(pos - info->pos_in_file);

  if (info->type == READ_CACHE || info->type == SEQ_READ_APPEND)
  {
    /* TODO: explain why this works if pos < info->pos_in_file */
    if ((ulonglong) offset < (ulonglong) (info->read_end - info->buffer))
    {
      /* The read is in the current buffer; Reuse it */
      info->read_pos = info->buffer + offset;
      DBUG_VOID_RETURN;
    }
    else
    {
      /* Force a new read on next my_b_read */
      info->read_pos=info->read_end=info->buffer;
    }
  }
  else if (info->type == WRITE_CACHE)
  {
    /* If write is in current buffer, reuse it */
    if ((ulonglong) offset <
	(ulonglong) (info->write_end - info->write_buffer))
    {
      info->write_pos = info->write_buffer + offset;
      DBUG_VOID_RETURN;
    }
    VOID(flush_io_cache(info));
    /* Correct buffer end so that we write in increments of IO_SIZE */
    info->write_end=(info->write_buffer+info->buffer_length-
		     (pos & (IO_SIZE-1)));
  }
  info->pos_in_file=pos;
  info->seek_not_done=1;
  DBUG_VOID_RETURN;
}


/*
  Fill buffer.  Note that this assumes that you have already used
  all characters in the CACHE, independent of the read_pos value!
  return:  0 on error or EOF (info->error = -1 on error)
  number of characters
*/

uint my_b_fill(IO_CACHE *info)
{
  my_off_t pos_in_file=(info->pos_in_file+
			(uint) (info->read_end - info->buffer));
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
  info->read_pos=info->buffer;
  info->read_end=info->buffer+length;
  info->pos_in_file=pos_in_file;
  return length;
}


/*
  Read a string ended by '\n' into a buffer of 'max_length' size.
  Returns number of characters read, 0 on error.
  last byte is set to '\0'
  If buffer is full then to[max_length-1] will be set to \0.
*/

uint my_b_gets(IO_CACHE *info, char *to, uint max_length)
{
  char *start = to;
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
    for (pos=info->read_pos,end=pos+length ; pos < end ;)
    {
      if ((*to++ = *pos++) == '\n')
      {
	info->read_pos=pos;
	*to='\0';
	return (uint) (to-start);
      }
    }
    if (!(max_length-=length))
    {
     /* Found enough charcters;  Return found string */
      info->read_pos=pos;
      *to='\0';
      return (uint) (to-start);
    }
    if (!(length=my_b_fill(info)))
      return 0;
  }
}


my_off_t my_b_filelength(IO_CACHE *info)
{
  if (info->type == WRITE_CACHE)
  {
    return my_b_tell(info);
  }
  else
  {
    info->seek_not_done=1;
    return my_seek(info->file,0L,MY_SEEK_END,MYF(0));
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
  uint minimum_width; /* as yet unimplemented */
  uint minimum_width_sign;
  uint precision; /* as yet unimplemented for anything but %b */

  /*
    Store the location of the beginning of a format directive, for the
    case where we learn we shouldn't have been parsing a format string
    at all, and we don't want to lose the flag/precision/width/size
    information.
   */
  const char* backtrack;

  for (; *fmt != '\0'; fmt++)
  {
    /* Copy everything until '%' or end of string */
    const char *start=fmt;
    uint length;
    
    for (; (*fmt != '\0') && (*fmt != '%'); fmt++) ;

    length= (uint) (fmt - start);
    out_length+=length;
    if (my_b_write(info, start, length))
      goto err;

    if (*fmt == '\0')				/* End of format */
    {
      return out_length;
    }

    /* 
      By this point, *fmt must be a percent;  Keep track of this location and
      skip over the percent character. 
    */
    DBUG_ASSERT(*fmt == '%');
    backtrack= fmt;
    fmt++;

    minimum_width= 0;
    precision= 0;
    minimum_width_sign= 1;
    /* Skip if max size is used (to be compatible with printf) */
    while (*fmt == '-') { fmt++; minimum_width_sign= -1; }
    if (*fmt == '*') {
      precision= (int) va_arg(args, int);
      fmt++;
    } else {
      while (my_isdigit(&my_charset_latin1, *fmt)) {
        minimum_width=(minimum_width * 10) + (*fmt - '0');
        fmt++;
      }
    }
    minimum_width*= minimum_width_sign;

    if (*fmt == '.') {
      fmt++;
      if (*fmt == '*') {
        precision= (int) va_arg(args, int);
        fmt++;
      } else {
        while (my_isdigit(&my_charset_latin1, *fmt)) {
          precision=(precision * 10) + (*fmt - '0');
          fmt++;
        }
      }
    }

    if (*fmt == 's')				/* String parameter */
    {
      reg2 char *par = va_arg(args, char *);
      uint length2 = (uint) strlen(par);
      /* TODO: implement minimum width and precision */
      out_length+= length2;
      if (my_b_write(info, par, length2))
	goto err;
    }
    else if (*fmt == 'b')                       /* Sized buffer parameter, only precision makes sense */
    {
      char *par = va_arg(args, char *);
      out_length+= precision;
      if (my_b_write(info, par, precision))
        goto err;
    }
    else if (*fmt == 'd' || *fmt == 'u')	/* Integer parameter */
    {
      register int iarg;
      uint length2;
      char buff[17];

      iarg = va_arg(args, int);
      if (*fmt == 'd')
	length2= (uint) (int10_to_str((long) iarg,buff, -10) - buff);
      else
	length2= (uint) (int10_to_str((long) (uint) iarg,buff,10)- buff);
      out_length+= length2;
      if (my_b_write(info, buff, length2))
	goto err;
    }
    else if ((*fmt == 'l' && fmt[1] == 'd') || fmt[1] == 'u')
      /* long parameter */
    {
      register long iarg;
      uint length2;
      char buff[17];

      iarg = va_arg(args, long);
      if (*++fmt == 'd')
	length2= (uint) (int10_to_str(iarg,buff, -10) - buff);
      else
	length2= (uint) (int10_to_str(iarg,buff,10)- buff);
      out_length+= length2;
      if (my_b_write(info, buff, length2))
	goto err;
    }
    else
    {
      /* %% or unknown code */
      if (my_b_write(info, backtrack, fmt-backtrack))
        goto err;
      out_length+= fmt-backtrack;
    }
  }
  return out_length;

err:
  return (uint) -1;
}
