/* Copyright (c) 2000, 2007 MySQL AB
   Use is subject to license terms

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

/*
  echo is a replacement for the "echo" command builtin to cmd.exe
  on Windows, to get a Unix eqvivalent behaviour when running commands
  like:
    $> echo "hello" | mysql

  The windows "echo" would have sent "hello" to mysql while
  Unix echo will send hello without the enclosing hyphens

  This is a very advanced high tech program so take care when
  you change it and remember to valgrind it before production
  use.

*/

#include <stdio.h>

int main(int argc, char **argv)
{
  int i;
  for (i= 1; i < argc-1; i++)
    fprintf(stdout, "%s ", argv[i]);
  
  fprintf(stdout, "%s\n", argv[i]);
  return 0;
}
