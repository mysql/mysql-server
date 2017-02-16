/* Copyright (C) 2006-2008 MySQL AB

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

#include "../maria_def.h"
#include <my_dir.h>
#ifdef _WIN32
#include <direct.h> /* rmdir */
#endif

my_bool maria_log_remove(const char *testdir)
{
  MY_DIR *dirp;
  uint i;
  MY_STAT stat_buff;
  char file_name[FN_REFLEN];

  /* Removes control file */
  if (fn_format(file_name, CONTROL_FILE_BASE_NAME,
                maria_data_root, "", MYF(MY_WME)) == NullS)
    return 1;
  if (my_stat(file_name, &stat_buff, MYF(0)) &&
      my_delete(file_name, MYF(MY_WME)) != 0)
    return 1;

  /* Finds and removes transaction log files */
  if (!(dirp = my_dir(maria_data_root, MYF(MY_DONT_SORT))))
    return 1;

  for (i= 0; i < dirp->number_off_files; i++)
  {
    char *file= dirp->dir_entry[i].name;
    if (strncmp(file, "aria_log.", 9) == 0 &&
        file[9] >= '0' && file[9] <= '9' &&
        file[10] >= '0' && file[10] <= '9' &&
        file[11] >= '0' && file[11] <= '9' &&
        file[12] >= '0' && file[12] <= '9' &&
        file[13] >= '0' && file[13] <= '9' &&
        file[14] >= '0' && file[14] <= '9' &&
        file[15] >= '0' && file[15] <= '9' &&
        file[16] >= '0' && file[16] <= '9' &&
        file[17] == '\0')
    {
      if (fn_format(file_name, file,
                    maria_data_root, "", MYF(MY_WME)) == NullS ||
          my_delete(file_name, MYF(MY_WME)) != 0)
      {
        my_dirend(dirp);
        return 1;
      }
    }
  }
  my_dirend(dirp);
  if (testdir)
    rmdir(testdir);
  return 0;
}

char *create_tmpdir(const char *progname)
{
  static char test_dirname[FN_REFLEN];
  char tmp_name[FN_REFLEN];
  uint length;

  /* Create a temporary directory of name TMP-'executable', but without the -t extension */
  fn_format(tmp_name, progname, "", "", MY_REPLACE_DIR | MY_REPLACE_EXT);
  length= strlen(tmp_name);
  if (length > 2 && tmp_name[length-2] == '-' && tmp_name[length-1] == 't')
    tmp_name[length-2]= 0;
  strxmov(test_dirname, "TMP-", tmp_name, NullS);

  /*
    Don't give an error if we can't create dir, as it may already exist from a previously aborted
    run
  */
  (void) my_mkdir(test_dirname, 0777, MYF(0));
  return test_dirname;
}
