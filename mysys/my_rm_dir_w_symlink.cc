/*
   Copyright (c) 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifdef _WIN32
#include <direct.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <errno.h>
#include "m_string.h"
#include "my_io.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysql/psi/mysql_file.h"
#include "mysql_com.h"

#ifdef HAVE_PSI_INTERFACE
extern PSI_file_key key_file_misc;
#endif

/**
   Deletes the specified directory. In case of linux, if it is
   symbolic link, it deletes the link first and then deletes
   the original directory pointed by it.

   @param[in]   directory_path            Path to the directory/symbolic link
   @param[in]   send_error                Flag that specifies if this function
   should send error when deleting symbolic link (if any) and directory.
   @param[in]   send_intermediate_errors  Flag that specifies if this function
   should should send error when reading the symbolic link.
   @param[out]  directory_deletion_failed Flag that specifies if the directory
   deletion step failed.

   @retval false  success
   @retval true   failure
*/

bool my_rm_dir_w_symlink(const char *directory_path, bool send_error,
                         bool send_intermediate_errors [[maybe_unused]],
                         bool &directory_deletion_failed) {
  char tmp_path[FN_REFLEN], *pos;
  char *path = tmp_path;
  unpack_filename(tmp_path, directory_path);
#ifndef _WIN32
  int error;
  char tmp2_path[FN_REFLEN];

  /* Remove end FN_LIBCHAR as this causes problem on Linux in readlink */
  pos = strend(path);
  if (pos > path && pos[-1] == FN_LIBCHAR) *--pos = 0;

  if ((error = my_readlink(tmp2_path, path,
                           MYF(send_intermediate_errors ? MY_WME : 0))) < 0)
    return true;
  if (error == 0) {
    if (mysql_file_delete(
            key_file_misc, path,
            MYF((send_error && send_intermediate_errors) ? MY_WME : 0))) {
      return send_error;
    }
    /* Delete directory symbolic link pointed at */
    path = tmp2_path;
  }
#endif
  /* Remove last FN_LIBCHAR to not cause a problem on OS/2 */
  pos = strend(path);

  if (pos > path && pos[-1] == FN_LIBCHAR) *--pos = 0;
  if (rmdir(path) < 0 && send_error) {
    directory_deletion_failed = true;
    set_my_errno(errno);
    return true;
  }
  return false;
}
