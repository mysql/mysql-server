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

/*
 * $Revision: 1.1 $
 * (c) Copyright 1996-2003, TimesTen, Inc.
 * All rights reserved.
 *
 */

#ifndef __TT_TIME
#define __TT_TIME


#ifdef WIN32

#include <windows.h>
#include <sys/types.h>
#include <sys/timeb.h>

typedef struct {
  FILETIME kernelTime;
  FILETIME userTime;
} ttThreadTimes;


typedef union {
  LARGE_INTEGER time64;
  struct _timeb notSoLargeTime;
} ttWallClockTime;

#elif defined(RTSYS_VXWORKS)

#define srand48(x) sb_srand48((x))
#define drand48() sb_drand48()

#ifdef SB_P_OS_VXPPC
/* For little-endian switch the lower, upper fields */
typedef union {
  struct {
    unsigned int upper32;
    unsigned int lower32;
  } sep;
  long long val;
} ttWallClockTime;

/*
 * This is a VxWorks private function to read the PPC's 64 bit Time Base
 * Register.  This is the assembler dump of this function.
    001126e4  7cad42e6                 mftb        r5, TBU
    001126e8  7ccc42e6                 mftb        r6, TBL
    001126ec  7ced42e6                 mftb        r7, TBU
    001126f0  7c053800                 cmp         crf0, 0, r5, r7
    001126f4  4082fff0                 bc          0x4, 0x2, vxTimeBaseGet
    001126f8  90a30000                 stw         r5, 0x0(r3)
    001126fc  90c40000                 stw         r6, 0x0(r4)
    00112700  4e800020                 blr         
 * This is a fine grained timer with a period of 60ns.
 */
void vxTimeBaseGet(unsigned int* pUpper32, unsigned int* pLower32);
#endif /* SB_P_OS_VXPPC */

#elif defined(SB_P_OS_CHORUS)
#include <sys/types.h>
#include <sys/times.h>
#include <sys/time.h>

#include <vtimer/chVtimer.h>

struct chrTimes {
  KnTimeVal ins;
  KnTimeVal ext;
};
typedef struct chrTimes ttThreadTimes;

typedef struct timeval ttWallClockTime;

#else
/* UNIX version */

#include <sys/times.h>
#include <sys/time.h>

typedef struct tms ttThreadTimes;

typedef struct timeval ttWallClockTime;

#endif /* NT, VxWorks, Chorus, Unix */


#ifndef RTSYS_VXWORKS
void ttGetThreadTimes(ttThreadTimes* timesP);
void ttCalcElapsedThreadTimes(ttThreadTimes* beforeP, ttThreadTimes* afterP,
                              double* kernelDeltaP, double* userDeltaP);
#endif /* ! VXWORKS */
void ttGetWallClockTime(ttWallClockTime* timeP);
void ttCalcElapsedWallClockTime(ttWallClockTime* beforeP,
                                ttWallClockTime* afterP,
                                double* nmillisecondsP);





#endif /* __TT_TIME */

/* Emacs variable settings */
/* Local Variables: */
/* tab-width:8 */
/* indent-tabs-mode:nil */
/* c-basic-offset:2 */
/* End: */
