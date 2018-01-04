/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <stddef.h>

#include "m_string.h"
#include "my_dbug.h"
#include "my_io.h"
#include "my_sys.h"  // IWYU pragma: keep

/**
  @file mysys/mf_dirname.cc
*/

size_t dirname_length(const char *name)
{
  char *pos, *gpos;
#ifdef _WIN32
  CHARSET_INFO *fs= fs_character_set();
#endif
#ifdef FN_DEVCHAR
  if ((pos=(char*)strrchr(name,FN_DEVCHAR)) == 0)
#endif
    pos=(char*) name-1;

  gpos= pos++;
  for ( ; *pos ; pos++)				/* Find last FN_LIBCHAR */
  {
#ifdef _WIN32
    uint l;
    if (use_mb(fs) && (l= my_ismbchar(fs, pos, pos + 3)))
    {
      pos+= l - 1;
      continue;
    }
#endif
    if (is_directory_separator(*pos))
      gpos=pos;
  }
  return (size_t) (gpos+1-(char*) name);
}


/*
  Gives directory part of filename. Directory ends with '/'

  SYNOPSIS
    dirname_part()
    to		Store directory name here
    name	Original name
    to_length	Store length of 'to' here

  RETURN
   #  Length of directory part in 'name'
*/

size_t dirname_part(char *to, const char *name, size_t *to_res_length)
{
  size_t length;
  DBUG_ENTER("dirname_part");
  DBUG_PRINT("enter",("'%s'",name));

  length=dirname_length(name);
  *to_res_length= (size_t) (convert_dirname(to, name, name+length) - to);
  DBUG_RETURN(length);
} /* dirname */


/*
  Convert directory name to use under this system

  SYNPOSIS
    convert_dirname()
    to				Store result here. Must be at least of size
    				min(FN_REFLEN, strlen(from) + 1) to make room
    				for adding FN_LIBCHAR at the end.
    from			Original filename. May be == to
    from_end			Pointer at end of filename (normally end \0)

  IMPLEMENTATION
    If Windows converts '/' to '\'
    Adds a FN_LIBCHAR to end if the result string if there isn't one
    and the last isn't dev_char.
    Copies data from 'from' until ASCII(0) for until from == from_end
    If you want to use the whole 'from' string, just send NullS as the
    last argument.

    If the result string is larger than FN_REFLEN -1, then it's cut.

  RETURN
   Returns pointer to end \0 in to
*/

#ifndef FN_DEVCHAR
#define FN_DEVCHAR '\0'				/* For easier code */
#endif

char *convert_dirname(char *to, const char *from, const char *from_end)
{
  char *to_org=to;
#ifdef _WIN32
  CHARSET_INFO *fs= fs_character_set();
#endif
  DBUG_ENTER("convert_dirname");

  /* We use -2 here, becasue we need place for the last FN_LIBCHAR */
  if (!from_end || (from_end - from) > FN_REFLEN-2)
    from_end=from+FN_REFLEN -2;

#if FN_LIBCHAR != '/'
  {
    for (; from < from_end && *from ; from++)
    {
      if (*from == '/')
	*to++= FN_LIBCHAR;
      else
      {
#ifdef _WIN32
        uint l;
        if (use_mb(fs) && (l= my_ismbchar(fs, from, from + 3)))
        {
          memmove(to, from, l);
          to+= l;
          from+= l - 1;
          to_org= to; /* Don't look inside mbchar */
        }
        else
#endif
        {
          *to++= *from;
        }
      }
    }
    *to=0;
  }
#else
  /* This is ok even if to == from, becasue we need to cut the string */
  to= strmake(to, from, (size_t) (from_end-from));
#endif

  /* Add FN_LIBCHAR to the end of directory path */
  if (to != to_org && (to[-1] != FN_LIBCHAR && to[-1] != FN_DEVCHAR))
  {
    *to++=FN_LIBCHAR;
    *to=0;
  }
  DBUG_RETURN(to);                              /* Pointer to end of dir */
} /* convert_dirname */
