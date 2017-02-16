/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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


#include "mysys_priv.h"
#include <m_string.h>

/*
  Return a pointer to the extension of the filename.

  SYNOPSIS
    fn_ext()
    name		Name of file

  DESCRIPTION
    The extension is defined as everything after the first extension character
    (normally '.') after the directory name.

  RETURN VALUES
    Pointer to the extension character. If there isn't any extension,
    points at the end ASCII(0) of the filename.
*/

char *fn_ext(const char *name)
{
  register const char *pos, *gpos;
  DBUG_ENTER("fn_ext");
  DBUG_PRINT("mfunkt",("name: '%s'",name));

#if defined(FN_DEVCHAR) || defined(BASKSLASH_MBTAIL)
  {
    char buff[FN_REFLEN];
    size_t res_length;
    gpos= name+ dirname_part(buff,(char*) name, &res_length);
  }
#else
  if (!(gpos= strrchr(name, FN_LIBCHAR)))
    gpos= name;
#endif
  pos= strchr(gpos, FN_EXTCHAR);
  DBUG_RETURN((char*) (pos ? pos : strend(gpos)));
} /* fn_ext */


/*
  Return a pointer to the extension of the filename.

  SYNOPSIS
    fn_ext2()
    name		Name of file

  DESCRIPTION
    The extension is defined as everything after the last extension character
    (normally '.') after the directory name.

  RETURN VALUES
    Pointer to the extension character. If there isn't any extension,
    points at the end ASCII(0) of the filename.
*/

char *fn_ext2(const char *name)
{
  register const char *pos, *gpos;
  DBUG_ENTER("fn_ext");
  DBUG_PRINT("mfunkt",("name: '%s'",name));

#if defined(FN_DEVCHAR) || defined(BASKSLASH_MBTAIL)
  {
    char buff[FN_REFLEN];
    size_t res_length;
    gpos= name+ dirname_part(buff,(char*) name, &res_length);
  }
#else
  if (!(gpos= strrchr(name, FN_LIBCHAR)))
    gpos= name;
#endif
  // locate the last occurence of FN_EXTCHAR
  pos= strrchr(gpos, FN_EXTCHAR);
  DBUG_RETURN((char*) (pos ? pos : strend(gpos)));
} /* fn_ext2 */

