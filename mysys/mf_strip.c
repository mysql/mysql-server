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

/* T|mmer en str{ng p{ slut_space */

#include "mysys_priv.h"

/*
	strip_sp(char * str)
	Strips end-space from string and returns new length.
*/

size_t strip_sp(register char * str)
{
  reg2 char * found;
  reg3 char * start;

  start=found=str;

  while (*str)
  {
    if (*str != ' ')
    {
      while (*++str && *str != ' ') {};
      if (!*str)
	return (size_t) (str-start);	/* Return stringlength */
    }
    found=str;
    while (*++str == ' ') {};
  }
  *found= '\0';				/* Stripp at first space */
  return (size_t) (found-start);
} /* strip_sp */
