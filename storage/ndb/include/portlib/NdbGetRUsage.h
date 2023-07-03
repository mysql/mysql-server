/*
   Copyright (c) 2011, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_GET_RUSAGE_H
#define NDB_GET_RUSAGE_H

#include <ndb_global.h>

//Enable DEBUG_RSS to measure memory consumption in various parts
//#define DEBUG_RSS 1
struct ndb_rusage
{
  Uint64 ru_utime;
  Uint64 ru_stime;
  Uint64 ru_minflt;
  Uint64 ru_majflt;
  Uint64 ru_nvcsw;
  Uint64 ru_nivcsw;
#ifdef DEBUG_RSS
  Uint64 ru_rss;
#endif
};

#ifdef	__cplusplus
extern "C" {
#endif

  /**
   * Get resource usage for calling thread
   */
  int Ndb_GetRUsage(ndb_rusage * dst, bool process);

#ifdef	__cplusplus
}
#endif

#endif
