/* Copyright (C) 2003 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

#include "log.h"
#include <my_global.h>
#include <stdarg.h>
#include <m_string.h>
#include <my_sys.h>

/*
  TODO:
  - add flexible header support
  - rewrite all fprintf with fwrite
  - think about using 'write' instead of fwrite/fprintf on POSIX systems
*/

/*
  Format log entry and write it to the given stream.
  SYNOPSYS
    log()
*/
  
static inline void log(FILE *file,  const char *format, va_list args) 
{
  /*
    log() should be thread-safe; it implies that we either call fprintf()
    once per log(), or use flockfile()/funlockfile(). But flockfile() is
    POSIX, not ANSI C, so we try to vsnprintf the whole message to the
    stack, and if stack buffer is not enough, to malloced string. When
    message is formatted, it is fprintf()'ed to the file.
  */

  /* Format time like MYSQL_LOG does. */
  time_t now= time(0);
  struct tm bd_time;                            // broken-down time
  localtime_r(&now, &bd_time);

  char buff_date[32];
  sprintf(buff_date, "%02d%02d%02d %2d:%02d:%02d\t",
                            bd_time.tm_year % 100,
                            bd_time.tm_mon + 1,
                            bd_time.tm_mday,
                            bd_time.tm_hour,
                            bd_time.tm_min,
                            bd_time.tm_sec);
  /* Format the message */
  char buff_stack[256];

  int n= vsnprintf(buff_stack, sizeof(buff_stack), format, args);
  /*
    return value of vsnprintf can vary, according to various standards;
    try to check all cases.
  */
  char *buff_msg= buff_stack;
  if (n < 0 || n == sizeof(buff_stack))
  {
    int size= sizeof(buff_stack) * 2;
    buff_msg= (char *) my_malloc(size, 0);
    while (true)
    {
      if (buff_msg == 0)
      {
        strmake(buff_stack, "log(): message is too big, my_malloc() failed",
                sizeof(buff_stack));
        buff_msg= buff_stack;
        break;
      }
      n = vsnprintf(buff_msg, size, format, args);
      if (n >= 0 && n < size)
        break;
      size*= 2;
      /* realloc() does unnecessary memcpy */
      my_free(buff_msg, 0);
      buff_msg= (char *) my_malloc(size, 0);
    }
  }
  else if ((size_t) n > sizeof(buff_stack))
  {
    buff_msg= (char *) my_malloc(n + 1, 0);
#ifdef DBUG
    DBUG_ASSERT(n == vsnprintf(buff_msg, n + 1, format, args));
#else
   vsnprintf(buff_msg, n + 1, format, args); 
#endif
  }
  fprintf(file, "%s%s\n", buff_date, buff_msg);
  if (buff_msg != buff_stack)
    my_free(buff_msg, 0);

  /* don't fflush() the file: buffering strategy is set in log_init() */
}


void log_error(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  log(stderr, format, args);
  va_end(args);
}


void log_info(const char *format, ...) 
{
  va_list args;
  va_start(args, format);
  log(stdout, format, args);
  va_end(args);
}

/* TODO: rewrite with buffering print */
void print_info(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vfprintf(stdout, format, args);
  va_end(args);
}

void print_error(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
}

/*
    log_init()
  RETURN VALUE
    0 ok
   !0 error
*/

void log_init()
{
  /*
    stderr is unbuffered by default; there is no good of line buffering,
    as all logging is performed linewise - so remove buffering from stdout
    also
  */
  setbuf(stdout, 0);
}

void die(const char *format, ...)
{
  va_list args;
  fprintf(stderr,"%s: ", my_progname);
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fprintf(stderr, "\n");
  exit(1);
}
