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

#define USES_TYPES
#include "mysys_priv.h"
#include <my_dir.h>
#include "mysys_err.h"

#undef my_rename
	/* On unix rename deletes to file if it exists */

int my_rename(const char *from, const char *to, myf MyFlags)
{
  int error = 0;
  DBUG_ENTER("my_rename");
  DBUG_PRINT("my",("from %s to %s MyFlags %d", from, to, MyFlags));

#if defined(HAVE_FILE_VERSIONS)
  {				/* Check that there isn't a old file */
    int save_errno;
    MY_STAT my_stat_result;
    save_errno=my_errno;
    if (my_stat(to,&my_stat_result,MYF(0)))
    {
      my_errno=EEXIST;
      error= -1;
      if (MyFlags & MY_FAE+MY_WME)
	my_error(EE_LINK, MYF(ME_BELL+ME_WAITTANG),from,to,my_errno);
      DBUG_RETURN(error);
    }
    my_errno=save_errno;
  }
#endif
#if defined(HAVE_RENAME)
  if (rename(from,to))
#else
  if (link(from, to) || unlink(from))
#endif
  {
    my_errno=errno;
    error = -1;
    if (MyFlags & (MY_FAE+MY_WME))
      my_error(EE_LINK, MYF(ME_BELL+ME_WAITTANG),from,to,my_errno);
  }
  DBUG_RETURN(error);
} /* my_rename */
