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

#ifndef PORT_DEFS_H
#define PORT_DEFS_H
/*
   This file contains varoius declarations/definitions needed in the port of AXEVM to NT, as well as backporting
   to Solaris...

  $Id: PortDefs.h,v 1.5 2003/10/07 07:59:59 mikael Exp $
*/
#ifdef NDB_WIN32
#include <time.h>


struct tms 
{
  time_t      tms_utime;  /* user time */
  time_t      tms_stime;  /* system time */
  time_t      tms_cutime; /* user time of children */
  time_t      tms_cstime; /* system time of children */
};

struct timespec 
{
  long        tv_sec;  /* Seconds */
  long        tv_nsec; /* Nanoseconds */
};

#define strcasecmp(a,b) _strcmpi(a,b)

/* Exports a WIN32 getopt function */
extern int  optind;
extern char *optarg;
int getopt(int, char **, char *opts);
#endif /* NDB_WIN32 */

#ifdef NDB_ALPHA
#ifdef NDB_GCC /* only for NDB_ALPHA */
extern int gnuShouldNotUseRPCC();
#define RPCC() gnuShouldNotUseRPCC();
#else 
#ifdef NDB_WIN32
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
    u_int64 __asm(char *, ...);
    double __dasm(char *, ...);
    float __fasm(char *, ...);
    void _AcquireSpinLock(long *);
    void _ReleaseSpinLock(long *);
    int __ADD_ATOMIC_LONG2(void *, int);
#ifdef __cplusplus
};
#endif /* __cplusplus */
#pragma intrinsic (__asm, __dasm, __fasm)
#pragma intrinsic(_ReleaseSpinLock, _AcquireSpinLock)
#pragma intrinsic(__ADD_ATOMIC_LONG2)
#endif /* NDB_WIN32 */

#define RPCC() ((int)__asm(" rpcc v0;"))
#define MB() __asm(" mb;");
#define WMB() __asm(" wmb;");
#ifdef USE_INITIALSP
#define IS_IP() (__asm(" mov sp,v0;") < IPinitialSP)
#else /* USE_INITIALSP */
#define IS_IP() (((__asm(" rpcc v0;") >> 32) & 0x7) == IP_CPU)
#endif
#endif /* NDB_GCC */
#else /* NDB_ALPHA */
#if defined NDB_SPARC
#define MB() asm ("membar 0x0;");  /* LoadLoad */
#define WMB() asm ("membar 0x3;"); /* StoreStore */
#else /* NDB_SPARC */
#define MB()
#define WMB()
#endif /* NDB_SPARC */
#define IS_IP() (1==1)
extern int shouldNotUseRPCC();
#define RPCC() shouldNotUseRPCC();
#endif /* NDB_ALPHA */

#endif
