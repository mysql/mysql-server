/* Copyright (C) 2000 MySQL AB

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

#include <my_global.h>
#include "m_ctype.h"

#ifdef USE_MB


void my_caseup_str_mb(CHARSET_INFO * cs, char *str)
{
  register uint32 l;
  register char *end=str+strlen(str); /* BAR TODO: remove strlen() call */
  while (*str)
  {
    if ((l=my_ismbchar(cs, str,end)))
      str+=l;
    else
    { 
      *str=(char)my_toupper(cs,(uchar)*str);
      str++;
    }
  }
}

void my_casedn_str_mb(CHARSET_INFO * cs, char *str)
{
  register uint32 l;
  register char *end=str+strlen(str);
  while (*str)
  {
    if ((l=my_ismbchar(cs, str,end)))
      str+=l;
    else
    {
      *str=(char)my_tolower(cs,(uchar)*str);
      str++;
    }
  }
}

void my_caseup_mb(CHARSET_INFO * cs, char *str, uint length)
{
  register uint32 l;
  register char *end=str+length;
  while (str<end)
  {
    if ((l=my_ismbchar(cs, str,end)))
      str+=l;
    else 
    {
      *str=(char)my_toupper(cs,(uchar)*str);
      str++;
    }
  }
}

void my_casedn_mb(CHARSET_INFO * cs, char *str, uint length)
{
  register uint32 l;
  register char *end=str+length;
  while (str<end)
  {
    if ((l=my_ismbchar(cs, str,end)))
      str+=l;
    else
    {
      *str=(char)my_tolower(cs,(uchar)*str);
      str++;
    }
  }
}

int my_strcasecmp_mb(CHARSET_INFO * cs,const char *s, const char *t)
{
  register uint32 l;
  register const char *end=s+strlen(s);
  while (s<end)
  {
    if ((l=my_ismbchar(cs, s,end)))
    {
      while (l--)
        if (*s++ != *t++) 
          return 1;
    }
    else if (my_ismbhead(cs, *t)) 
      return 1;
    else if (my_toupper(cs,(uchar) *s++) != my_toupper(cs,(uchar) *t++))
      return 1;
  }
  return *t;
}


int my_strncasecmp_mb(CHARSET_INFO * cs,
				const char *s, const char *t, uint len)
{
  register uint32 l;
  register const char *end=s+len;
  while (s<end)
  {
    if ((l=my_ismbchar(cs, s,end)))
    {
      while (l--)
        if (*s++ != *t++) 
          return 1;
    }
    else if (my_ismbhead(cs, *t)) 
      return 1;
    else if (my_toupper(cs,(uchar) *s++) != my_toupper(cs,(uchar) *t++)) 
      return 1;
  }
  return 0;
}

#endif
