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

/*
  Copy contents of an IO_CACHE to a file.

  SYNOPSIS
    my_b_copy_to_file()
    cache  IO_CACHE to copy from
    file   File to copy to

  DESCRIPTION
    Copy the contents of the cache to the file. The cache will be
    re-inited to a read cache and will read from the beginning of the
    cache.

    If a failure to write fully occurs, the cache is only copied
    partially.

  TODO
    Make this function solid by handling partial reads from the cache
    in a correct manner: it should be atomic.

  RETURN VALUE
    0  All OK
    1  An error occured
*/
int
my_b_copy_to_file(IO_CACHE *cache, FILE *file)
{
  size_t bytes_in_cache;
  DBUG_ENTER("my_b_copy_to_file");

  /* Reinit the cache to read from the beginning of the cache */
  if (reinit_io_cache(cache, READ_CACHE, 0L, FALSE, FALSE))
    DBUG_RETURN(1);
  bytes_in_cache= my_b_bytes_in_cache(cache);
  do
  {
    if (my_fwrite(file, cache->read_pos, bytes_in_cache,
                  MYF(MY_WME | MY_NABP)) == (size_t) -1)
      DBUG_RETURN(1);
    cache->read_pos= cache->read_end;
  } while ((bytes_in_cache= my_b_fill(cache)));
  DBUG_RETURN(0);
}


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
  Fill buffer of the cache.

  NOTES
    This assumes that you have already used all characters in the CACHE,
    independent of the read_pos value!

  RETURN
  0  On error or EOF (info->error = -1 on error)
  #  Number of characters
*/


size_t my_b_fill(IO_CACHE *info)
{
  my_off_t pos_in_file=(info->pos_in_file+
			(size_t) (info->read_end - info->buffer));
  size_t diff_length, length, max_length;

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
  diff_length=(size_t) (pos_in_file & (IO_SIZE-1));
  max_length=(info->read_length-diff_length);
  if (max_length >= (info->end_of_file - pos_in_file))
    max_length= (size_t) (info->end_of_file - pos_in_file);

  if (!max_length)
  {
    info->error= 0;
    return 0;					/* EOF */
  }
  if ((length= my_read(info->file,info->buffer,max_length,
                       info->myflags)) == (size_t) -1)
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

size_t my_b_gets(IO_CACHE *info, char *to, size_t max_length)
{
  char *start = to;
  size_t length;
  max_length--;					/* Save place for end \0 */

  /* Calculate number of characters in buffer */
  if (!(length= my_b_bytes_in_cache(info)) &&
      !(length= my_b_fill(info)))
    return 0;

  for (;;)
  {
    uchar *pos, *end;
    if (length > max_length)
      length=max_length;
    for (pos=info->read_pos,end=pos+length ; pos < end ;)
    {
      if ((*to++ = *pos++) == '\n')
      {
	info->read_pos=pos;
	*to='\0';
	return (size_t) (to-start);
      }
    }
    if (!(max_length-=length))
    {
     /* Found enough charcters;  Return found string */
      info->read_pos=pos;
      *to='\0';
      return (size_t) (to-start);
    }
    if (!(length=my_b_fill(info)))
      return 0;
  }
}


my_off_t my_b_filelength(IO_CACHE *info)
{
  if (info->type == WRITE_CACHE)
    return my_b_tell(info);

  info->seek_not_done= 1;
  return my_seek(info->file, 0L, MY_SEEK_END, MYF(0));
}


/*
  Simple printf version.  Supports '%s', '%d', '%u', "%ld" and "%lu"
  Used for logging in MySQL
  returns number of written character, or (size_t) -1 on error
*/

size_t my_b_printf(IO_CACHE *info, const char* fmt, ...)
{
  size_t result;
  va_list args;
  va_start(args,fmt);
  result=my_b_vprintf(info, fmt, args);
  va_end(args);
  return result;
}


size_t my_b_vprintf(IO_CACHE *info, const char* fmt, va_list args)
{
  size_t out_length= 0;
  uint minimum_width; /* as yet unimplemented */
  uint minimum_width_sign;
  uint precision; /* as yet unimplemented for anything but %b */
  my_bool is_zero_padded;

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
    size_t length;
    
    for (; (*fmt != '\0') && (*fmt != '%'); fmt++) ;

    length= (size_t) (fmt - start);
    out_length+=length;
    if (my_b_write(info, (const uchar*) start, length))
      goto err;

    if (*fmt == '\0')				/* End of format */
      return out_length;

    /* 
      By this point, *fmt must be a percent;  Keep track of this location and
      skip over the percent character. 
    */
    DBUG_ASSERT(*fmt == '%');
    backtrack= fmt;
    fmt++;

    is_zero_padded= FALSE;
    minimum_width_sign= 1;
    minimum_width= 0;
    precision= 0;
    /* Skip if max size is used (to be compatible with printf) */

process_flags:
    switch (*fmt)
    {
      case '-': 
        minimum_width_sign= -1; fmt++; goto process_flags;
      case '0':
        is_zero_padded= TRUE; fmt++; goto process_flags;
      case '#':
        /** @todo Implement "#" conversion flag. */  fmt++; goto process_flags;
      case ' ':
        /** @todo Implement " " conversion flag. */  fmt++; goto process_flags;
      case '+':
        /** @todo Implement "+" conversion flag. */  fmt++; goto process_flags;
    }

    if (*fmt == '*')
    {
      precision= (int) va_arg(args, int);
      fmt++;
    }
    else
    {
      while (my_isdigit(&my_charset_latin1, *fmt)) {
        minimum_width=(minimum_width * 10) + (*fmt - '0');
        fmt++;
      }
    }
    minimum_width*= minimum_width_sign;

    if (*fmt == '.')
    {
      fmt++;
      if (*fmt == '*') {
        precision= (int) va_arg(args, int);
        fmt++;
      }
      else
      {
        while (my_isdigit(&my_charset_latin1, *fmt)) {
          precision=(precision * 10) + (*fmt - '0');
          fmt++;
        }
      }
    }

    if (*fmt == 's')				/* String parameter */
    {
      reg2 char *par = va_arg(args, char *);
      size_t length2 = strlen(par);
      /* TODO: implement precision */
      out_length+= length2;
      if (my_b_write(info, (uchar*) par, length2))
	goto err;
    }
    else if (*fmt == 'b')                       /* Sized buffer parameter, only precision makes sense */
    {
      char *par = va_arg(args, char *);
      out_length+= precision;
      if (my_b_write(info, (uchar*) par, precision))
        goto err;
    }
    else if (*fmt == 'd' || *fmt == 'u')	/* Integer parameter */
    {
      register int iarg;
      size_t length2;
      char buff[17];

      iarg = va_arg(args, int);
      if (*fmt == 'd')
	length2= (size_t) (int10_to_str((long) iarg,buff, -10) - buff);
      else
        length2= (uint) (int10_to_str((long) (uint) iarg,buff,10)- buff);

      /* minimum width padding */
      if (minimum_width > length2) 
      {
        char *buffz;
                    
        buffz= my_alloca(minimum_width - length2);
        if (is_zero_padded)
          memset(buffz, '0', minimum_width - length2);
        else
          memset(buffz, ' ', minimum_width - length2);
        my_b_write(info, buffz, minimum_width - length2);
        my_afree(buffz);
      }

      out_length+= length2;
      if (my_b_write(info, (uchar*) buff, length2))
	goto err;
    }
    else if ((*fmt == 'l' && fmt[1] == 'd') || fmt[1] == 'u')
      /* long parameter */
    {
      register long iarg;
      size_t length2;
      char buff[17];

      iarg = va_arg(args, long);
      if (*++fmt == 'd')
	length2= (size_t) (int10_to_str(iarg,buff, -10) - buff);
      else
	length2= (size_t) (int10_to_str(iarg,buff,10)- buff);
      out_length+= length2;
      if (my_b_write(info, (uchar*) buff, length2))
	goto err;
    }
    else
    {
      /* %% or unknown code */
      if (my_b_write(info, (uchar*) backtrack, (size_t) (fmt-backtrack)))
        goto err;
      out_length+= fmt-backtrack;
    }
  }
  return out_length;

err:
  return (size_t) -1;
}
