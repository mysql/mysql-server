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

/* T|mmer en str{ng p{ slut_space */

#include "mysys_priv.h"

/*
	strip_sp(my_string str)
	Strips end-space from string and returns new length.
*/

size_s strip_sp(register my_string str)
{
  reg2 my_string found;
  reg3 my_string start;

  start=found=str;

  while (*str)
  {
    if (*str != ' ')
    {
      while (*++str && *str != ' ') {};
      if (!*str)
	return (size_s) (str-start);	/* Return stringlength */
    }
    found=str;
    while (*++str == ' ') {};
  }
  *found= '\0';				/* Stripp at first space */
  return (size_s) (found-start);
} /* strip_sp */
