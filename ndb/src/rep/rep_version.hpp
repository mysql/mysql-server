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

#ifndef REP_VERSION_HPP
#define REP_VERSION_HPP

/**
 * Block number for REP
 */
#define SSREPBLOCKNO 1
#define PSREPBLOCKNO 2

#define DBUG

#include <ndb_version.h>

extern "C"
void 
DBUG_PRINT__(const char * fmt, ...);

extern "C"
void 
replog(const char * fmt, ...);

extern "C"
void 
rlog(const char * fmt, ...);

#define RLOG(ARGS) \
  do { if (replogEnabled) { \
         rlog ARGS; \
         ndbout << " (" << __FILE__ << ":" << __LINE__ << ")" << endl; \
       } \
     } while (0)

/**
 * Replication logging on or off
 */
extern int replogEnabled;

/**
 * Used for config id
 */
#define REP_VERSION_ID NDB_VERSION

#define MAX_NODE_GROUPS 6

#define REPABORT(string) \
  { \
    ndbout_c("\nInternal error in %s:%d: %s", __FILE__, __LINE__, string); \
    abort(); \
  }
#define REPABORT1(string, data1) \
  { \
    ndbout_c("\nInternal error in %s:%d: %s" \
	     "\n  (data1: %d)", \
	     __FILE__, __LINE__, string, data1); \
    abort(); \
  }
#define REPABORT2(string, data1, data2) \
  { \
    ndbout_c("\nInternal error in %s:%d: %s" \
	     "\n  (data1: %d, data2: %d)", \
              __FILE__, __LINE__, string, data1, data2); \
    abort(); \
  }
#define REPABORT3(string, data1, data2, data3) \
  { \
    ndbout_c("\nInternal error in %s:%d: %s" \
	     "\n  (data1: %d, data2: %d data3: %d)", \
              __FILE__, __LINE__, string, data1, data2, data3); \
    abort(); \
  }

#endif
