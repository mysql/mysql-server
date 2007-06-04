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
  Author : David
  strintstr(src, from, pat) looks for an instance of pat in src
  backwards from pos from.  pat is not a regex(3) pattern, it is a literal
  string which must be matched exactly.
  The result 0 if the pattern was not found else it is the start char of
  the pattern counted from the begining of the string.
*/

#include <my_global.h>
#include "m_string.h"

size_t r_strinstr(reg1 const char * str, size_t from, reg4 const char * search)
{
  reg2 const char *i, *j;
  size_t	len = strlen(search);
  /* pointer to the last char of buff */
  const char *	start = str + from - 1;
  /* pointer to the last char of search */
  const char *	search_end = search + len - 1;

 skip:
  while (start >= str)		/* Cant be != because the first char */
  {
    if (*start-- == *search_end)
    {
      i = start; j = search_end - 1;
      while (j >= search && start > str)
	if (*i-- != *j--)
	  goto skip;
      return (size_t) ((start - len) - str + 3);
    }
  }
  return (0);
}
