/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#include <sched.h>

#define JAM_FILE_ID 324


int
main(int argc, char * const argv[]){
  struct sched_param p;
  p.sched_priority = 1;
  
  int ret = sched_setscheduler(getpid(), SCHED_RR, &p);
  printf("ref = %d\n", ret);

  execv(argv[1], &argv[1]);
  return 0;
}
