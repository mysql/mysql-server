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

	/* Returns full load-path for a file. to may be = path */
	/* if path is a hard-path return path */
	/* if path starts with home-dir return path */
	/* if path starts with current dir or parent-dir unpack path */
	/* if there is no path, prepend with own_path_prefix if given */
	/* else unpack path according to current dir */

char * my_load_path(char * to, const char *path,
		       const char *own_path_prefix)
{
  char buff[FN_REFLEN];
  int is_cur;
  DBUG_ENTER("my_load_path");
  DBUG_PRINT("enter",("path: %s  prefix: %s",path,
		      own_path_prefix ? own_path_prefix : ""));

  if ((path[0] == FN_HOMELIB && path[1] == FN_LIBCHAR) ||
      test_if_hard_path(path))
    (void) strmov(buff,path);
  else if ((is_cur=(path[0] == FN_CURLIB && path[1] == FN_LIBCHAR)) ||
	   (is_prefix(path,FN_PARENTDIR)) ||
	   ! own_path_prefix)
  {
    if (is_cur)
      is_cur=2;					/* Remove current dir */
    if (! my_getwd(buff,(uint) (FN_REFLEN-strlen(path)+is_cur),MYF(0)))
      (void) strcat(buff,path+is_cur);
    else
      (void) strmov(buff,path);			/* Return org file name */
  }
  else
    (void) strxmov(buff,own_path_prefix,path,NullS);
  strmov(to,buff);
  DBUG_PRINT("exit",("to: %s",to));
  DBUG_RETURN(to);
} /* my_load_path */
