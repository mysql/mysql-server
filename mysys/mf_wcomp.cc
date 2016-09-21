/* Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

/**
  @file mysys/mf_wcomp.cc
  Functions for comparing with wild-cards.
*/
#include "mf_wcomp.h"

#include "my_dbug.h"

int wild_compare_full(const char *str, const char *wildstr, bool str_is_pattern,
                      char w_prefix, char w_one, char w_many)
{
  char cmp;
  DBUG_ENTER("wild_compare");

  while (*wildstr)
  {
    while (*wildstr && *wildstr != w_many && *wildstr != w_one)
    {
      if (*wildstr == w_prefix && wildstr[1])
      {
	wildstr++;
        if (str_is_pattern && *str++ != w_prefix)
          DBUG_RETURN(1);
      }
      if (*wildstr++ != *str++)
        DBUG_RETURN(1);
    }
    if (! *wildstr )
      DBUG_RETURN(*str != 0);
    if (*wildstr++ == w_one)
    {
      if (! *str || (str_is_pattern && *str == w_many))
        DBUG_RETURN(1);                     /* One char; skip */
      if (*str++ == w_prefix && str_is_pattern && *str)
        str++;
    }
    else
    {						/* Found '*' */
      while (str_is_pattern && *str == w_many)
        str++;
      for (; *wildstr ==  w_many || *wildstr == w_one; wildstr++)
        if (*wildstr == w_many)
        {
          while (str_is_pattern && *str == w_many)
            str++;
        }
        else
        {
          if (str_is_pattern && *str == w_prefix && str[1])
            str+=2;
          else if (! *str++)
            DBUG_RETURN (1);
        }
      if (!*wildstr)
        DBUG_RETURN(0);		/* '*' as last char: OK */
      if ((cmp= *wildstr) == w_prefix && wildstr[1] && !str_is_pattern)
        cmp=wildstr[1];
      for (;;str++)
      {
        while (*str && *str != cmp)
          str++;
        if (!*str)
          DBUG_RETURN (1);
	if (wild_compare_full(str, wildstr, str_is_pattern, w_prefix, w_one,
                              w_many) == 0)
          DBUG_RETURN (0);
      }
      /* We will never come here */
    }
  }
  DBUG_RETURN (*str != 0);
} /* wild_compare */


int wild_compare(const char *str, const char *wildstr, bool str_is_pattern)
{
  return wild_compare_full(str, wildstr, str_is_pattern, wild_prefix, wild_one,
                           wild_many);
}
