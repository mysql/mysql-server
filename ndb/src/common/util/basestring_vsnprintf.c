/* Copyright (C) 2003 MySQL AB

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

/* define on IRIX to get posix compliant vsnprintf */
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <basestring_vsnprintf.h>
#include <my_config.h>

int
basestring_snprintf(char *str, size_t size, const char *format, ...)
{
  int ret;
  va_list ap;
  va_start(ap, format);
  ret= basestring_vsnprintf(str, size, format, ap);
  va_end(ap);
  return(ret);
}

#ifdef HAVE_SNPRINTF
  #define BASESTRING_VSNPRINTF_FUNC(a,b,c,d) vsnprintf(a,b,c,d)
#else
  #define SNPRINTF_RETURN_TRUNC
  #define BASESTRING_VSNPRINTF_FUNC(a,b,c,d) my_vsnprintf(a,b,c,d)
  extern int my_vsnprintf(char *str, size_t size, const char *format, va_list ap);
#endif
#ifdef SNPRINTF_RETURN_TRUNC
static char basestring_vsnprintf_buf[16*1024];
#endif
int
basestring_vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
  int ret= BASESTRING_VSNPRINTF_FUNC(str, size, format, ap);
#ifdef SNPRINTF_RETURN_TRUNC
  if (ret == size-1) {
    ret= BASESTRING_VSNPRINTF_FUNC(basestring_vsnprintf_buf,
				   sizeof(basestring_vsnprintf_buf),
				   format, ap);
  }
#endif
  return ret;
}
