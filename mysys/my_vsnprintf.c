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

#include "mysys_priv.h"
#include "mysys_err.h"
#include <m_string.h>
#include <stdarg.h>
#include <m_ctype.h>

int my_vsnprintf(char *to, size_t n, const char* fmt, va_list ap)
{
  char *start=to, *end=to+n-1;

  for (; *fmt ; fmt++)
  {
    if (fmt[0] != '%')
    {
      if (to == end)			/* End of buffer */
	break;
      *to++= *fmt;			/* Copy ordinary char */
      continue;
    }
    /* Skipp if max size is used (to be compatible with printf) */
    while (isdigit(*fmt) || *fmt == '.' || *fmt == '-')
      fmt++;
    if (*fmt == 's')				/* String parameter */
    {
      reg2 char	*par = va_arg(ap, char *);
      uint plen = (uint) strlen(par);
      if ((uint) (end-to) > plen)	/* Replace if possible */
      {
	to=strmov(to,par);
	continue;
      }
    }
    else if (*fmt == 'd' || *fmt == 'u')	/* Integer parameter */
    {
      register int iarg;
      if ((uint) (end-to) < 16)
	break;
      iarg = va_arg(ap, int);
      if (*fmt == 'd')
	to=int10_to_str((long) iarg,to, -10);
      else
	to=int10_to_str((long) (uint) iarg,to,10);
      continue;
    }
    /* We come here on '%%', unknown code or too long parameter */
    if (to == end)
      break;
    *to++='%';				/* % used as % or unknown code */
  }
  *to='\0';				/* End of errmessage */
  return (uint) (to - start);
}
