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



int my_vsnprintf(char* str, size_t n, const char* fmt, va_list ap)
{
  uint		olen = 0, plen;
  const char *tpos;
  reg1 char	*endpos;
  reg2 char		* par;
  char* ebuff = str;
  
  endpos=ebuff;
  tpos = fmt;

  while (*tpos)
  {
    if (tpos[0] != '%')
    {
      if(olen + 1 >= n)
	break;
      
      *endpos++= *tpos++;	/* Copy ordinary char */
      olen++;
      continue;
    }
    if (*++tpos == '%')		/* test if %% */
    {
      olen--;
    }
    else
    {
      /* Skipp if max size is used (to be compatible with printf) */
      while (isdigit(*tpos) || *tpos == '.' || *tpos == '-')
	tpos++;
      if (*tpos == 's')				/* String parameter */
      {
	par = va_arg(ap, char *);
	plen = (uint) strlen(par);
	if (olen + plen < n)		/* Replace if possible */
	{
	  endpos=strmov(endpos,par);
	  tpos++;
	  olen+=plen;
	  continue;
	}
      }
      else if (*tpos == 'd' || *tpos == 'u')	/* Integer parameter */
      {
	register int iarg;
	iarg = va_arg(ap, int);
	if(olen + 16 >= n) break;
	
	if (*tpos == 'd')
	  plen= (uint) (int2str((long) iarg,endpos, -10) - endpos);
	else
	  plen= (uint) (int2str((long) (uint) iarg,endpos,10)- endpos);
	if (olen + plen < n) /* Replace parameter if possible */
	{
	  endpos+=plen;
	  tpos++;
	  olen+=plen;
	  continue;
	}
      }
    }
    *endpos++='%';		/* % used as % or unknown code */
  }
  *endpos='\0';
  /* End of errmessage */
 return olen;
}

