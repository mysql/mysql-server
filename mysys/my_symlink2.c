/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/*
  Advanced symlink handling.
  This is used in MyISAM to let users symlinks tables to different disk.
  The main idea with these functions is to automaticly create, delete and
  rename files and symlinks like they would be one unit.
*/

#include "mysys_priv.h"
#include <m_string.h>

File my_create_with_symlink(const char *linkname, const char *filename,
			    int createflags, int access_flags, myf MyFlags)
{
  File file;
  int tmp_errno;
  DBUG_ENTER("my_create_with_symlink");
  if ((file=my_create(filename, createflags, access_flags, MyFlags)) >= 0)
  {
    /* Test if we should create a link */
    if (linkname && strcmp(linkname,filename))
    {
      /* Delete old link/file */
      if (MyFlags & MY_DELETE_OLD)
	my_delete(linkname, MYF(0));
      /* Create link */
      if (my_symlink(filename, linkname, MyFlags))
      {
	/* Fail, remove everything we have done */
	tmp_errno=my_errno;
	my_close(file,MYF(0));
	my_delete(filename, MYF(0));
	file= -1;
	my_errno=tmp_errno;
      }
    }
  }
  DBUG_RETURN(file);
}

/*
  If the file was a symlink, delete both symlink and the file which the
  symlink pointed to.
*/

int my_delete_with_symlink(const char *name, myf MyFlags)
{
  char link_name[FN_REFLEN];
  int was_symlink= (!my_disable_symlinks &&
		    !my_readlink(link_name, name, MYF(0)));
  int result;
  DBUG_ENTER("my_delete_with_symlink");

  if (!(result=my_delete(name, MyFlags)))
  {
    if (was_symlink)
      result=my_delete(link_name, MyFlags);
  }
  DBUG_RETURN(result);
}

/*
  If the file is a normal file, just rename it.
  If the file is a symlink:
   - Create a new file with the name 'to' that points at
     symlink_dir/basename(to)
   - Rename the symlinked file to symlink_dir/basename(to)
   - Delete 'from'
   If something goes wrong, restore everything.
*/

int my_rename_with_symlink(const char *from, const char *to, myf MyFlags)
{
#ifdef HAVE_READLINK
  return my_rename(from, to, MyFlags);
#else
  char link_name[FN_REFLEN], tmp_name[FN_REFLEN];
  int was_symlink= (!my_disable_symlinks &&
		    !my_readlink(link_name, name, MYF(0)));
  int result;
  DBUG_ENTER("my_rename_with_symlink");

  if (!was_symlink)
    DBUG_RETURN(my_rename(from, to, MyFlags));

  /* Change filename that symlink pointed to */
  strmov(tmp_name, to);
  fn_same(tmp_name,link_name,1);		/* Copy dir */

  /* Create new symlink */
  if (my_symlink(tmp_name, to, MyFlags))
    DBUG_RETURN(-1);

  /*
    Rename symlinked file if the base name didn't change.
    This can happen if you use this function where 'from' and 'to' has
    the same basename and different directories.
   */

  if (strcmp(link_name, tmp_name) && my_rename(link_name, tmp_name, MyFlags))
  {
    my_delete(to, MyFlags);			/* Remove created symlink */
    DBUG_RETURN(-1);
  }

  /* Remove original symlink */
  if (my_delete(from, MyFlags))
  {
    /* Remove created link */
    my_delete(to, MyFlags);
    /* Rename file back */
    if (strcmp(link_name, tmp_name))
      (void) my_rename(tmp_name, link_name, MyFlags);
  }
  DBUG_RETURN(result);
#endif /* HAVE_READLINK */
}
