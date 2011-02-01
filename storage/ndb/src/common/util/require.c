/*
   Copyright 2009 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>

void require_failed(int exitcode, RequirePrinter printer,
                    const char* expr, const char* file, int line)
{
#define FMT "%s:%d: require(%s) failed\n", file, line, expr
  if (!printer)
  {
    fprintf(stderr, FMT);
    fflush(stderr);
  }
  else
  {
    printer(FMT);
  }
#ifdef _WIN32
  DebugBreak();
#endif
  if(exitcode)
  {
    exit(exitcode);
  }
  abort();
}

#ifdef TEST
int main()
{
  require(1);
  require(0);
}
#endif
