/* Copyright (C) 2011 Monty Program Ab

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <my_sys.h>
/**
   @brief retrieve last component of the filename.
   Loosely based on Single Unix Spec definition.

   @fn my_basename()
   @param filename  Filename
*/
const char *my_basename(const char *filename)
{
  const char *last;
  const char *s=filename;

  /* Handle basename()'s special cases, as per single unix spec */
  if (!filename || !filename[0])
    return "."; 
  if(filename[0] == '/' && filename[1]== '\0')
    return filename;

  for(last= s; *s; s++)
  {
    if (*s == '/' || *s == '\\')
      last= s + 1;
  }
  return last;
}
