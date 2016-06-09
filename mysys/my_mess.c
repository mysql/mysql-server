/* Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "mysys_priv.h"
#include "my_sys.h"

/**
  Print an error message on stderr.
  Prefixed with the binary's name (sans .exe, where applicable,
  and without path, both to keep our test cases sane).
  The name is intended to aid debugging by clarifying which
  binary reported an error, especially in cases like mysql_upgrade
  which calls several other tools whose messages should be
  distinguishable from each other's, and from mysql_upgrade's.

  This is low-level, in most cases, you should use my_message_local()
  instead (which by default goes through my_message_local_stderr(),
  which is a wrapper around this function that adds a severity level).

  @param error    The error number. Currently unused.
  @param str      The message to print. Not trailing \n needed.
  @param MyFlags  ME_BELL to beep, or 0.
*/
void my_message_stderr(uint error MY_ATTRIBUTE((unused)),
                       const char *str, myf MyFlags)
{
  DBUG_ENTER("my_message_stderr");
  DBUG_PRINT("enter",("message: %s",str));
  (void) fflush(stdout);
  if (MyFlags & ME_BELL)
    (void) fputc('\007', stderr);
  if (my_progname)
  {
    size_t l;
    const char *r;

    if ((r= strrchr(my_progname, FN_LIBCHAR)))
      r++;
    else
      r= my_progname;

    l= strlen(r);
 #ifdef _WIN32
    if ((l > 4) && !strcmp(&r[l - 4], ".exe"))
      l-= 4; /* purecov: inspected */  /* Windows-only */
 #endif
    fprintf(stderr, "%.*s: ", (int) l, r);
  }
  (void)fputs(str,stderr);
  (void)fputc('\n',stderr);
  (void)fflush(stderr);
  DBUG_VOID_RETURN;
}
