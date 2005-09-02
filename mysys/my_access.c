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

#include "mysys_priv.h"
#include <m_string.h>

#ifdef __WIN__

/*
  Check a file or path for accessability.
 
  SYNOPSIS
    file_access()
    path 	Path to file
    amode	Access method
 
  DESCRIPTION
    This function wraps the normal access method because the access 
    available in MSVCRT> +reports that filenames such as LPT1 and 
    COM1 are valid (they are but should not be so for us).
 
  RETURN VALUES
  0    ok
  -1   error  (We use -1 as my_access is mapped to access on other platforms)
*/

int my_access(const char *path, int amode) 
{ 
  WIN32_FILE_ATTRIBUTE_DATA fileinfo;
  BOOL result;
	
  result= GetFileAttributesEx(path, GetFileExInfoStandard, &fileinfo);
  if (! result ||
      (fileinfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY) && (amode & W_OK))
  {
    my_errno= errno= EACCES;
    return -1;
  }
  return 0;
}

#endif /* __WIN__ */

#if defined(MSDOS) || defined(__WIN__) || defined(__EMX__)

/*
  List of file names that causes problem on windows

  NOTE that one can also not have file names of type CON.TXT
*/

static const char *reserved_names[]=
{
  "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6",
  "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6",
  "LPT7", "LPT8", "LPT9", "CLOCK$",
  NullS
};

#define MAX_RESERVED_NAME_LENGTH 6

/*
  Check if a path will access a reserverd file name that may cause problems
 
  SYNOPSIS
    check_if_legal_filename
    path 	Path to file

  RETURN
    0  ok
    1  reserved file name
*/

int check_if_legal_filename(const char *path)
{
  const char *end;
  const char **reserved_name;
  DBUG_ENTER("check_if_legal_filename");

  path+= dirname_length(path);                  /* To start of filename */
  if (!(end= strchr(path, FN_EXTCHAR)))
    end= strend(path);
  if (path == end || (uint) (end - path) > MAX_RESERVED_NAME_LENGTH)
    DBUG_RETURN(0);                             /* Simplify inner loop */

  for (reserved_name= reserved_names; *reserved_name; reserved_name++)
  {
    const char *reserved= *reserved_name;       /* never empty */
    const char *name= path;
    
    do
    {
      if (*reserved != my_toupper(&my_charset_latin1, *name))
        break;
      if (++name == end && !reserved[1])
        DBUG_RETURN(1);                         /* Found wrong path */
    } while (*++reserved);
  }
  DBUG_RETURN(0);
}
#endif


#ifdef OS2
int check_if_legal_filename(const char *path)
{
  return 0;
}
#endif /* OS2 */
