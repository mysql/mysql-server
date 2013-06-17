/* Copyright (c) 2011, Monty Program Ab

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <my_global.h>
#include <my_sys.h>
#include "tap.h"


int main(int argc __attribute__((unused)),char *argv[])
{
  char tmp_dir[MAX_PATH];
  char tmp_filename[MAX_PATH];
  HANDLE h, h2;

  MY_INIT(argv[0]);

  plan(6);

  GetTempPathA(MAX_PATH, tmp_dir);
  ok(GetTempFileNameA(tmp_dir, "foo", 0,  tmp_filename) != 0, "create temp file");
  ok(my_delete(tmp_filename,MYF(0)) == 0, "Delete closed file");


  /* Delete an open file */
  ok(GetTempFileNameA(tmp_dir, "foo", 0,  tmp_filename) != 0, "create temp file 2");
  h = CreateFileA(tmp_filename, GENERIC_READ|GENERIC_WRITE, 
      FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
  ok (h != INVALID_HANDLE_VALUE || h != 0, "open temp file");
  ok(my_delete(tmp_filename, MYF(0)) == 0, "Delete open file");


  /* 
    Check if it is possible to reuse file name after delete (not all handles 
    to it are closed.
  */
  h2 = CreateFileA(tmp_filename, GENERIC_READ|GENERIC_WRITE, 
        FILE_SHARE_DELETE, NULL, CREATE_NEW, FILE_FLAG_DELETE_ON_CLOSE, NULL);
  ok(h2 != 0 && h2 != INVALID_HANDLE_VALUE, "Reuse file name");
  CloseHandle(h);
  CloseHandle(h2);

  my_end(0);
  return exit_status();
}

