/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include "my_sys.h"
#include "mysys_err.h"
#include <my_dir.h>
#include "my_thread_local.h"

/*
  Generate MY_MODE representation from perm_flags.

  @param perm_flags Permission information

  @return Permission in MY_STAT format
*/

MY_MODE get_file_perm(ulong perm_flags)
{
  MY_MODE file_perm= 0;
  if (perm_flags <= 0)
    return file_perm;

#if defined _WIN32
  if (perm_flags & (USER_READ | GROUP_READ | OTHERS_READ))
    file_perm|= _S_IREAD;
  if (perm_flags & (USER_WRITE | GROUP_WRITE | OTHERS_WRITE))
    file_perm|= _S_IWRITE;
#else
  if (perm_flags & USER_READ)
    file_perm|= S_IRUSR;
  if (perm_flags & USER_WRITE)
    file_perm|= S_IWUSR;
  if (perm_flags & USER_EXECUTE)
    file_perm|= S_IXUSR;
  if (perm_flags & GROUP_READ)
    file_perm|= S_IRGRP;
  if (perm_flags & GROUP_WRITE)
    file_perm|= S_IWGRP;
  if (perm_flags & GROUP_EXECUTE)
    file_perm|= S_IXGRP;
  if (perm_flags & OTHERS_READ)
    file_perm|= S_IROTH;
  if (perm_flags & OTHERS_WRITE)
    file_perm|= S_IWOTH;
  if (perm_flags & OTHERS_EXECUTE)
    file_perm|= S_IXOTH;
#endif

  return file_perm;
}

/*
  my_chmod : Change permission on a file

  @param filename : Name of the file
  @param perm_flags : Permission information
  @param my_flags : Error handling

  @return
    @retval TRUE : Error changing file permission
    @retval FALSE : File permission changed successfully
*/

my_bool my_chmod(const char *filename, ulong perm_flags, myf my_flags)
{
  int ret_val;
  MY_MODE file_perm;
  DBUG_ENTER("my_chmod");
  DBUG_ASSERT(filename && filename[0]);

  file_perm= get_file_perm(perm_flags);
#ifdef _WIN32
  ret_val= _chmod(filename, file_perm);
#else
  ret_val= chmod(filename, file_perm);
#endif

  if (ret_val && (my_flags & (MY_FAE+MY_WME)))
  {
    char errbuf[MYSYS_STRERROR_SIZE];
    set_my_errno(errno);
    my_error(EE_CHANGE_PERMISSIONS, MYF(0), filename,
             errno, my_strerror(errbuf, sizeof(errbuf), errno));
  }

  DBUG_RETURN(ret_val ? TRUE : FALSE);
}
