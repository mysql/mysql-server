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

/*  File   : strinstr.c
    Author : Monty & David
    Updated: 1986.12.08
    Defines: strinstr()

    strinstr(src, pat) looks for an instance of pat in src.  pat is not a
    regex(3) pattern, it is a literal string which must be matched exactly.
    The result 0 if the pattern was not found else it is the start char of
    the pattern counted from the beginning of the string, where the first
    char is 1.
*/

#include <my_global.h>
#include "m_string.h"

uint strinstr(reg1 const char *str,reg4 const char *search)
{
  reg2 my_string i,j;
  my_string start = (my_string) str;

 skip:
  while (*str != '\0')
  {
    if (*str++ == *search)
    {
      i=(my_string) str; j= (my_string) search+1;
      while (*j)
	if (*i++ != *j++) goto skip;
      return ((uint) (str - start));
    }
  }
  return (0);
}
