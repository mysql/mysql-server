/* Copyright (C) 2005 MySQL AB

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
  my_strchr(cs, str, end, c) returns a pointer to the first place in
  str where c (1-byte character) occurs, or NULL if c does not occur
  in str. This function is multi-byte safe.
  TODO: should be moved to CHARSET_INFO if it's going to be called
  frequently.
*/

#include <my_global.h>
#include "m_string.h"
#include "m_ctype.h"


char *my_strchr(CHARSET_INFO *cs, const char *str, const char *end,
                pchar c)
{
  uint mbl;
  while (str < end)
  {
    mbl= my_mbcharlen(cs, *(uchar *)str);
    if (mbl < 2)
    {
      if (*str == c)
        return((char *)str);
      str++;
    }
    else
      str+= mbl;
  }
  return(0);
}

