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

void my_message_stderr(uint error __attribute__((unused)),
                       const char *str, myf MyFlags)
{
  DBUG_ENTER("my_message_stderr");
  DBUG_PRINT("enter",("message: %s",str));
  (void) fflush(stdout);
  if (MyFlags & ME_BELL)
    (void) fputc('\007', stderr);
  if (my_progname)
  {
    (void)fputs(my_progname,stderr); (void)fputs(": ",stderr);
  }
  (void)fputs(str,stderr);
  (void)fputc('\n',stderr);
  (void)fflush(stderr);
  DBUG_VOID_RETURN;
}
