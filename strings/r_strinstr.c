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
  Author : David
  strintstr(src, from, pat) looks for an instance of pat in src
  backwards from pos from.  pat is not a regex(3) pattern, it is a literal
  string which must be matched exactly.
  The result 0 if the pattern was not found else it is the start char of
  the pattern counted from the begining of the string.
*/

#include <my_global.h>
#include "m_string.h"

uint r_strinstr(reg1 my_string str,int from, reg4 my_string search)
{
  reg2 my_string	i, j;
  uint		len = (uint) strlen(search);
  /* pointer to the last char of buff */
  my_string	start = str + from - 1;
  /* pointer to the last char of search */
  my_string	search_end = search + len - 1;

 skipp:
  while (start >= str)		/* Cant be != because the first char */
  {
    if (*start-- == *search_end)
    {
      i = start; j = search_end - 1;
      while (j >= search && start > str)
	if (*i-- != *j--)
	  goto skipp;
      return (uint) ((start - len) - str + 3);
    }
  }
  return (0);
}
