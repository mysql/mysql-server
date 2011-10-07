/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_GET_RUSAGE_H
#define NDB_GET_RUSAGE_H

#include <ndb_global.h>

struct ndb_rusage
{
  Uint64 ru_utime;
  Uint64 ru_stime;
  Uint64 ru_minflt;
  Uint64 ru_majflt;
  Uint64 ru_nvcsw;
  Uint64 ru_nivcsw;
};

#ifdef	__cplusplus
extern "C" {
#endif

  /**
   * Get resource usage for calling thread
   */
  int Ndb_GetRUSage(ndb_rusage * dst);

#ifdef	__cplusplus
}
#endif

#endif
