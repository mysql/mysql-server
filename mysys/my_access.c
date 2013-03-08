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


/*
  List of file names that causes problem on windows

  NOTE that one can also not have file names of type CON.TXT
  
  NOTE: it is important to keep "CLOCK$" on the first place,
  we skip it in check_if_legal_tablename.
*/
static const char *reserved_names[]=
{
  "CLOCK$",
  "CON", "PRN", "AUX", "NUL",
  "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
  "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
  NullS
};

#define MAX_RESERVED_NAME_LENGTH 6


/*
  Looks up a null-terminated string in a list,
  case insensitively.
 
  SYNOPSIS
    str_list_find()
    list        list of items
    str         item to find

  RETURN
    0  ok
    1  reserved file name
*/
static int str_list_find(const char **list, const char *str)
{
  const char **name;
  for (name= list; *name; name++)
  {
    if (!my_strcasecmp(&my_charset_latin1, *name, str))
      return 1;
  }
  return 0;
}


/*
  A map for faster reserved_names lookup,
  helps to avoid loops in many cases.
  1 - can be the first letter
  2 - can be the second letter
  4 - can be the third letter
*/
static char reserved_map[256]=
{
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ................ */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ................ */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /*  !"#$%&'()*+,-./ */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0123456789:;<=>? */
  0,1,0,1,0,0,0,0,0,0,0,0,7,4,5,2, /* @ABCDEFGHIJKLMNO */
  3,0,2,0,4,2,0,0,4,0,0,0,0,0,0,0, /* PQRSTUVWXYZ[\]^_ */
  0,1,0,1,0,0,0,0,0,0,0,0,7,4,5,2, /* bcdefghijklmno */
  3,0,2,0,4,2,0,0,4,0,0,0,0,0,0,0, /* pqrstuvwxyz{|}~. */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ................ */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ................ */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ................ */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ................ */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ................ */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ................ */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ................ */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0  /* ................ */
};


/*
  Check if a table name may cause problems
 
  SYNOPSIS
    check_if_legal_tablename
    name 	Table name (without any extensions)

  DESCRIPTION
    We don't check 'CLOCK$' because dollar sign is encoded as @0024,
    making table file name 'CLOCK@0024', which is safe.
    This is why we start lookup from the second element
    (i.e. &reserver_name[1])

  RETURN
    0  ok
    1  reserved file name
*/

int check_if_legal_tablename(const char *name)
{
  DBUG_ENTER("check_if_legal_tablename");
  DBUG_RETURN(name[0] != 0 && name[1] != 0 &&
              (reserved_map[(uchar) name[0]] & 1) &&
              (reserved_map[(uchar) name[1]] & 2) &&
              (reserved_map[(uchar) name[2]] & 4) &&
              str_list_find(&reserved_names[1], name));
}


#ifdef __WIN__
/**
  Checks if the drive letter supplied is valid or not. Valid drive
  letters are A to Z, both lower case and upper case.

  @param drive_letter : The drive letter to validate.
 
  @return TRUE if the drive exists, FALSE otherwise.
*/
static my_bool does_drive_exists(char drive_letter)
{
  DWORD drive_mask= GetLogicalDrives();
  drive_letter= toupper(drive_letter);

  return (drive_letter >= 'A' && drive_letter <= 'Z') &&
         (drive_mask & (0x1 << (drive_letter - 'A')));
}
#endif

/**
  Verifies if the file name supplied is allowed or not. On Windows
  file names with a colon (:) are not allowed because such file names
  store data in Alternate Data Streams which can be used to hide 
  the data.

  @param name contains the file name with or without path
  @param length contains the length of file name
 
  @return TRUE if the file name is allowed, FALSE otherwise.
*/
my_bool is_filename_allowed(const char *name __attribute__((unused)),
                            size_t length __attribute__((unused)))
{
#ifdef __WIN__
  /* 
    For Windows, check if the file name contains : character.
    Start from end of path and search if the file name contains :
  */
  const char* ch = NULL;
  for(ch= name + length - 1; ch >= name; --ch)
  {
    if (FN_LIBCHAR == *ch || '/' == *ch)
      break;
    else if (':' == *ch)
    {
      /*
        File names like C:foobar.txt are allowed since the syntax means
        file foobar.txt in current directory of C drive. However file
        names likes CC:foobar are not allowed since this syntax means ADS
        foobar in file CC.
      */
      return ((ch - name == 1) && does_drive_exists(*name));
    }
  }
  return TRUE;
#else
  /* For other platforms, file names can contain colon : */
  return TRUE;
#endif
} /* is_filename_allowed */

#if defined(__WIN__) || defined(__EMX__)


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

  if(!is_filename_allowed(path, strlen(path)))
    DBUG_RETURN(1);

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

#endif /* defined(__WIN__) || defined(__EMX__) */
