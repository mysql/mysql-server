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
#include "mysys_err.h"

/**
   @brief Change mode of file.

   @fn my_chmod()
   @param name  	Filename
   @param mode_t        Mode
   @param my_flags	Flags

   @notes
   The  mode of the file given by path or referenced by fildes is changed

   @retval 0	Ok
   @retval #	Error
*/

int my_chmod(const char *name, mode_t mode, myf my_flags)
{
  DBUG_ENTER("my_chmod");
  DBUG_PRINT("my",("name: %s  mode: %lu  flags: %d", name, (ulong) mode,
                   my_flags));

  if (chmod(name, mode))
  {
    my_errno= errno;
    if (my_flags & MY_WME)
      my_error(EE_CANT_CHMOD, MYF(0), name, (ulong) mode, my_errno);
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}
