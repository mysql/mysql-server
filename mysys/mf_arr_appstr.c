/* Copyright (C) 2007 MySQL AB

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

#include "mysys_priv.h"
#include <m_string.h>                           /* strcmp() */


/**
  Append str to array, or move to the end if it already exists

  @param str    String to be appended
  @param array  The array, terminated by a NULL element, all unused elements
                pre-initialized to NULL
  @param size   Size of the array; array must be terminated by a NULL
                pointer, so can hold size - 1 elements

  @retval FALSE  Success
  @retval TRUE   Failure, array is full
*/

my_bool array_append_string_unique(const char *str,
                                   const char **array, size_t size)
{
  const char **p;
  /* end points at the terminating NULL element */
  const char **end= array + size - 1;
  DBUG_ASSERT(*end == NULL);

  for (p= array; *p; ++p)
  {
    if (strcmp(*p, str) == 0)
      break;
  }
  if (p >= end)
    return TRUE;                               /* Array is full */

  DBUG_ASSERT(*p == NULL || strcmp(*p, str) == 0);

  while (*(p + 1))
  {
    *p= *(p + 1);
    ++p;
  }

  DBUG_ASSERT(p < end);
  *p= str;

  return FALSE;                                 /* Success */
}
