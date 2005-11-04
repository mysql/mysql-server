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

/*
  Formats a filename with possible replace of directory of extension
  Function can handle the case where 'to' == 'name'
  For a description of the flag values, consult my_sys.h
  The arguments should be in unix format.
*/

my_string fn_format(my_string to, const char *name, const char *dir,
		    const char *extension, uint flag)
{
  reg1 uint length;
  char dev[FN_REFLEN], buff[FN_REFLEN], *pos, *startpos;
  const char *ext;
  DBUG_ENTER("fn_format");
  DBUG_PRINT("enter",("name: %s  dir: %s  extension: %s  flag: %d",
		       name,dir,extension,flag));

  /* Copy and skip directory */
  name+=(length=dirname_part(dev,(startpos=(my_string) name)));
  if (length == 0 || (flag & MY_REPLACE_DIR))
  {
    /* Use given directory */
    convert_dirname(dev,dir,NullS);		/* Fix to this OS */
  }
  else if ((flag & MY_RELATIVE_PATH) && !test_if_hard_path(dev))
  {
    /* Put 'dir' before the given path */
    strmake(buff,dev,sizeof(buff)-1);
    pos=convert_dirname(dev,dir,NullS);
    strmake(pos,buff,sizeof(buff)-1- (int) (pos-dev));
  }

  if (flag & MY_PACK_FILENAME)
    pack_dirname(dev,dev);			/* Put in ./.. and ~/.. */
  if (flag & MY_UNPACK_FILENAME)
    (void) unpack_dirname(dev,dev);		/* Replace ~/.. with dir */

  if ((pos= (char*) strchr(name,FN_EXTCHAR)) != NullS)
  {
    if ((flag & MY_REPLACE_EXT) == 0)		/* If we should keep old ext */
    {
      length=strlength(name);			/* Use old extension */
      ext = "";
    }
    else
    {
      length=(uint) (pos-(char*) name);		/* Change extension */
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
    uint tmp_length;
    if (flag & MY_SAFE_PATH)
      return NullS;
    tmp_length=strlength(startpos);
    DBUG_PRINT("error",("dev: '%s'  ext: '%s'  length: %d",dev,ext,length));
    (void) strmake(to,startpos,min(tmp_length,FN_REFLEN-1));
  }
  else
  {
    if (to == startpos)
    {
      bmove(buff,(char*) name,length);		/* Save name for last copy */
      name=buff;
    }
    pos=strmake(strmov(to,dev),name,length);
    (void) strmov(pos,ext);			/* Don't convert extension */
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
    strmov(buff,to);
    (void) my_readlink(to, buff, MYF(0));
  }
  DBUG_RETURN(to);
} /* fn_format */


	/*
	  strlength(const string str)
	  Return length of string with end-space:s not counted.
	  */

size_s strlength(const char *str)
{
  reg1 my_string pos;
  reg2 my_string found;
  DBUG_ENTER("strlength");

  pos=found=(char*) str;

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
  DBUG_RETURN((size_s) (found-(char*) str));
} /* strlength */
