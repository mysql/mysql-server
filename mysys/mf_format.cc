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

/**
  @file mysys/mf_format.cc
*/

#include <string.h>
#include <sys/types.h>

#include "m_string.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_macros.h"
#include "my_sys.h"

/*
  Formats a filename with possible replace of directory of extension
  Function can handle the case where 'to' == 'name'
  For a description of the flag values, consult my_sys.h
  The arguments should be in unix format.
*/

char * fn_format(char * to, const char *name, const char *dir,
		    const char *extension, uint flag)
{
  char dev[FN_REFLEN], buff[FN_REFLEN], *pos, *startpos;
  const char *ext;
  size_t length;
  size_t dev_length;
  DBUG_ENTER("fn_format");
  DBUG_ASSERT(name != NULL);
  DBUG_ASSERT(extension != NULL);
  DBUG_PRINT("enter",("name: %s  dir: %s  extension: %s  flag: %d",
		       name,dir,extension,flag));

  /* Copy and skip directory */
  name+=(length=dirname_part(dev, (startpos=(char *) name), &dev_length));
  if (length == 0 || (flag & MY_REPLACE_DIR))
  {
    DBUG_ASSERT(dir != NULL);
    /* Use given directory */
    convert_dirname(dev,dir,NullS);		/* Fix to this OS */
  }
  else if ((flag & MY_RELATIVE_PATH) && !test_if_hard_path(dev))
  {
    DBUG_ASSERT(dir != NULL);
    /* Put 'dir' before the given path */
    strmake(buff,dev,sizeof(buff)-1);
    pos=convert_dirname(dev,dir,NullS);
    strmake(pos,buff,sizeof(buff)-1- (int) (pos-dev));
  }

  if (flag & MY_PACK_FILENAME)
    pack_dirname(dev,dev);			/* Put in ./.. and ~/.. */
  if (flag & MY_UNPACK_FILENAME)
    (void) unpack_dirname(dev,dev);		/* Replace ~/.. with dir */

  if (!(flag & MY_APPEND_EXT) &&
      (pos= (char*) strchr(name,FN_EXTCHAR)) != NullS)
  {
    if ((flag & MY_REPLACE_EXT) == 0)		/* If we should keep old ext */
    {
      length=strlength(name);			/* Use old extension */
      ext = "";
    }
    else
    {
      length= (size_t) (pos-(char*) name);	/* Change extension */
      ext= extension;
    }
  }
  else
  {
    length=strlength(name);			/* No ext, use the now one */
    ext=extension;
  }

  if (strlen(dev)+length+strlen(ext) >= FN_REFLEN || length >= FN_LEN )
  {
    /* To long path, return original or NULL */
    size_t tmp_length;
    if (flag & MY_SAFE_PATH)
      DBUG_RETURN(NullS);
    tmp_length= strlength(startpos);
    DBUG_PRINT("error",("dev: '%s'  ext: '%s'  length: %u",dev,ext,
                        (uint) length));
    (void) strmake(to, startpos, MY_MIN(tmp_length, FN_REFLEN-1));
  }
  else
  {
    if (to == startpos)
    {
      memmove(buff, name, length);              /* Save name for last copy */
      name=buff;
    }
    pos=strmake(my_stpcpy(to,dev),name,length);
    (void) my_stpcpy(pos,ext);			/* Don't convert extension */
  }
  /*
    If MY_RETURN_REAL_PATH and MY_RESOLVE_SYMLINK is given, only do
    realpath if the file is a symbolic link
  */
  if (flag & MY_RETURN_REAL_PATH)
    (void) my_realpath(to, to, MYF(flag & MY_RESOLVE_SYMLINKS ?
				   MY_RESOLVE_LINK: 0));
  else if (flag & MY_RESOLVE_SYMLINKS)
  {
    my_stpcpy(buff,to);
    (void) my_readlink(to, buff, MYF(0));
  }
  DBUG_RETURN(to);
} /* fn_format */


/*
  strlength(const string str)
  Return length of string with end-space:s not counted.
*/

size_t strlength(const char *str)
{
  const char * pos;
  const char * found;
  DBUG_ENTER("strlength");

  pos= found= str;

  while (*pos)
  {
    if (*pos != ' ')
    {
      while (*++pos && *pos != ' ') {};
      if (!*pos)
      {
	found=pos;			/* String ends here */
	break;
      }
    }
    found=pos;
    while (*++pos == ' ') {};
  }
  DBUG_RETURN((size_t) (found - str));
} /* strlength */
