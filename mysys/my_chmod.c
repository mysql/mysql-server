/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA */


#include "mysys_priv.h"
#include "mysys_err.h"
#include <my_dir.h>

#ifdef _WIN32
typedef int MY_MODE;
#else
typedef mode_t MY_MODE;
#endif /* _WIN32 */

/*
  Generate MY_MODE representation from PermFlags.

  @param PermFlags Permission information

  @return Permission in MY_STAT format
*/
static
MY_MODE get_file_perm(ulong PermFlags)
{
  MY_MODE file_perm= 0;
  if (PermFlags <= 0)
    return file_perm;

#if defined _WIN32
  if (PermFlags & (USER_READ | GROUP_READ | OTHERS_READ))
    file_perm|= _S_IREAD;
  if (PermFlags & (USER_WRITE | GROUP_WRITE | OTHERS_WRITE))
    file_perm|= _S_IWRITE;
#else
  if (PermFlags & USER_READ)
    file_perm|= S_IRUSR;
  if (PermFlags & USER_WRITE)
    file_perm|= S_IWUSR;
  if (PermFlags & USER_EXECUTE)
    file_perm|= S_IXUSR;
  if (PermFlags & GROUP_READ)
    file_perm|= S_IRGRP;
  if (PermFlags & GROUP_WRITE)
    file_perm|= S_IWGRP;
  if (PermFlags & GROUP_EXECUTE)
    file_perm|= S_IXGRP;
  if (PermFlags & OTHERS_READ)
    file_perm|= S_IROTH;
  if (PermFlags & OTHERS_WRITE)
    file_perm|= S_IWOTH;
  if (PermFlags & OTHERS_EXECUTE)
    file_perm|= S_IXOTH;
#endif

  return file_perm;
}

/*
  my_chmod : Change permission on a file

  @param filename : Name of the file
  @param PermFlags : Permission information
  @param my_flags : Error handling

  @return
    @retval TRUE : Error changing file permission
    @retval FALSE : File permission changed successfully
*/

my_bool my_chmod(const char *filename, ulong PermFlags, myf my_flags)
{
  int ret_val;
  MY_MODE file_perm;
  DBUG_ENTER("my_chmod");
  DBUG_ASSERT(filename && filename[0]);

  file_perm= get_file_perm(PermFlags);
#ifdef _WIN32
  ret_val= _chmod(filename, file_perm); 
#else
  ret_val= chmod(filename, file_perm);
#endif

  if (ret_val && (my_flags & (MY_FAE+MY_WME)))
  {
    char errbuf[MYSYS_STRERROR_SIZE];
    my_errno= errno;
    my_error(EE_CHANGE_PERMISSIONS, MYF(0), filename,
             errno, my_strerror(errbuf, sizeof(errbuf), errno));
  }

  DBUG_RETURN(ret_val ? TRUE : FALSE);
}
