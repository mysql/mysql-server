/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "NdbDaemon.h"

#define NdbDaemon_ErrorSize 500
long NdbDaemon_DaemonPid;
int NdbDaemon_ErrorCode;
char NdbDaemon_ErrorText[NdbDaemon_ErrorSize];

int
NdbDaemon_Make(const char* lockfile, const char* logfile, unsigned flags)
{
  // XXX do something
  return 0;
}

#ifdef NDB_DAEMON_TEST

int
main()
{
  if (NdbDaemon_Make("test.pid", "test.log", 0) == -1) {
    fprintf(stderr, "NdbDaemon_Make: %s\n", NdbDaemon_ErrorText);
    return 1;
  }
  sleep(10);
  return 0;
}

#endif
