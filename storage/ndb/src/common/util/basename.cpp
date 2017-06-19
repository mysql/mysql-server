/*
   Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>

/* 
   Iterate backwards from the end of the string until a separator is found.
   Treat both forward slash and backslash as path separators.
   Either of them might appear in Windows environments.
*/

static inline bool is_separator(char c)
{
  return (c == '/' || c == '\\');
}

const char *
ndb_basename(const char * path)
{
  if (path == NULL)
    return NULL;

  const char * p = path + strlen(path);
  while (p > path && ! is_separator(p[0]))
    p--;

  if (is_separator(p[0]))
    return p + 1;

  return p;
}
