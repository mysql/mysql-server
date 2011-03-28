/*
   Copyright (C) 2003-2007 MySQL AB
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

#ifndef NDB_TICK_H
#define NDB_TICK_H

#include <ndb_types.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef Uint64 NDB_TICKS; 

/**
 * Returns the current millisecond since 1970
 */
NDB_TICKS NdbTick_CurrentMillisecond(void);

/**
 * Get current micro second
 * Second method is simply abstraction on top of the first
 * 
 * Returns 0 - Success
 */
int NdbTick_CurrentMicrosecond(NDB_TICKS * secs, Uint32 * micros);

struct MicroSecondTimer {
  NDB_TICKS seconds;
  NDB_TICKS micro_seconds;
};

/**
 * Get time between start and stop time in microseconds
 * Abstraction to get time in struct
 *
 * 0 means stop happened at or before start time
 */
NDB_TICKS NdbTick_getMicrosPassed(struct MicroSecondTimer start,
                            struct MicroSecondTimer stop);
int NdbTick_getMicroTimer(struct MicroSecondTimer* time_now);


/**
 * Returns the current nanosecond
 */
NDB_TICKS NdbTick_CurrentNanosecond(void);

#ifdef	__cplusplus
}
#endif

#endif

