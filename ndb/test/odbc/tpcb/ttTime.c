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

static const volatile char cvsid[] = "$Id: ttTime.c,v 1.1 2003/09/23 12:43:46 johan Exp $";
/*
 * $Revision: 1.1 $
 * (c) Copyright 1996-2003, TimesTen, Inc.
 * All rights reserved.
 *
 */


/* Contains functions for performing elapsed-time calculations
   in a portable manner */

#include "ttTime.h"

#ifdef WIN32

#include <stdio.h>
#include <mapiutil.h>

/*------------*/
/* NT VERSION */
/*------------*/

/*********************************************************************
 *
 *  FUNCTION:       ttGetThreadTimes
 *
 *  DESCRIPTION:    This function sets the supplied parameter's
 *                  user and kernel time for the current thread.
 *
 *  PARAMETERS:     ttThreadTimes* timesP   thread time structure
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void
ttGetThreadTimes(ttThreadTimes* timesP)
{
  BOOL rc;
  HANDLE curThread;
  FILETIME creationTime;
  FILETIME exitTime;
  FILETIME kTime;
  FILETIME uTime;

  memset (&kTime, 0, sizeof (FILETIME));
  memset (&uTime, 0, sizeof (FILETIME));

  curThread = GetCurrentThread();
  rc = GetThreadTimes(curThread,
                      &creationTime,
                      &exitTime,
                      &kTime,
                      &uTime);

  timesP->kernelTime = kTime;
  timesP->userTime = uTime;

}

/*********************************************************************
 *
 *  FUNCTION:       ttCalcElapsedThreadTimes
 *
 *  DESCRIPTION:    This function calculates the user and kernel
 *                  time deltas.
 *
 *  PARAMETERS:     ttThreadTimes* beforeP  beginning timestamp (IN)
 *                  ttThreadTimes* afterP   ending timestamp (IN)
 *                  double* kernelDeltaP     kernel time delta (OUT)
 *                  double* userDeltaP       user time delta (OUT)
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void
ttCalcElapsedThreadTimes(ttThreadTimes* beforeP,
                         ttThreadTimes* afterP,
                         double* kernelDeltaP,
                         double* userDeltaP)
{
  static const double secPerHi = (double) 4.294967296; /* 2**32 * 10**-9 */
  FILETIME *before, *after;

  before = &beforeP->kernelTime;
  after = &afterP->kernelTime;
  *kernelDeltaP = (double) ((after->dwHighDateTime - before->dwHighDateTime) * secPerHi
                           + (after->dwLowDateTime - before->dwLowDateTime) * 100e-9);
  before = &beforeP->userTime;
  after = &afterP->userTime;
  *userDeltaP = (double) ((after->dwHighDateTime - before->dwHighDateTime) * secPerHi
                         + (after->dwLowDateTime - before->dwLowDateTime) * 100e-9);
}

/*********************************************************************
 *
 *  FUNCTION:       ttGetWallClockTime
 *
 *  DESCRIPTION:    This function gets the current wall-clock time.
 *
 *  PARAMETERS:     ttWallClockTime* timeP  tms time structure (OUT)
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void
ttGetWallClockTime(ttWallClockTime* timeP)
{
  LARGE_INTEGER frequency;
  if ( QueryPerformanceFrequency(&frequency) ) {
    QueryPerformanceCounter(&(timeP->time64));
  }
  else {
    _ftime(&(timeP->notSoLargeTime));
  }
}

/*********************************************************************
 *
 *  FUNCTION:       ttCalcElapsedWallClockTime
 *
 *  DESCRIPTION:    This function calculates the elapsed wall-clock
 *                  time in msec.
 *
 *  PARAMETERS:     ttWallClockTime* beforeP        starting timestamp
 *                  ttWallClockTime* afterP         ending timestamp
 *                  double* nmillisecondsP           elapsed time (OUT)
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void
ttCalcElapsedWallClockTime(ttWallClockTime* beforeP,
                           ttWallClockTime* afterP,
                           double* nmillisecondsP)
{
  LARGE_INTEGER frequency;

  if ( QueryPerformanceFrequency(&frequency) ) {
    *nmillisecondsP = 1000 * ((double) (afterP->time64.QuadPart
                                       - beforeP->time64.QuadPart))
      / frequency.QuadPart;

  }
  else {
    double start;
    double end;

    start = (double) beforeP->notSoLargeTime.time * 1000. +
      (double) beforeP->notSoLargeTime.millitm;
    end   = (double) afterP->notSoLargeTime.time  * 1000. +
      (double) afterP->notSoLargeTime.millitm;

    *nmillisecondsP = (double) (end - start);
  }
}

#elif defined (RTSYS_VXWORKS)

/*-----------------*/
/* VxWorks VERSION */
/*-----------------*/

/*
 * The TimeBase registers have a period of 60ns, i.e.
 * 0.00000006 or (6e-8) seconds.
 */
#define TIMER_MSEC_PER_CYC (6e-5)

void
ttGetWallClockTime(ttWallClockTime* timeP)
{
  vxTimeBaseGet(&timeP->sep.upper32, &timeP->sep.lower32);
}


void
ttCalcElapsedWallClockTime(ttWallClockTime* beforeP,
                           ttWallClockTime* afterP,
                           double* nmillisecondsP)
{
  *nmillisecondsP = (double)(afterP->val - beforeP->val) * TIMER_MSEC_PER_CYC;
}


#else

/*--------------*/
/* UNIX VERSION */
/*--------------*/

#include <unistd.h>

/*********************************************************************
 *
 *  FUNCTION:       ttGetThreadTimes
 *
 *  DESCRIPTION:    This function sets the supplied parameter's
 *                  tms structure.
 *
 *  PARAMETERS:     ttThreadTimes* timesP   tms time structure
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

#ifdef SB_P_OS_CHORUS
void ttGetThreadTimes(ttThreadTimes* timesP)
{
  KnCap actorCap;

  if (acap (agetId(), &actorCap) == -1) {
    timesP->ins.tmSec  = 0;
    timesP->ins.tmNSec = 0;
    timesP->ext.tmSec  = 0;
    timesP->ext.tmNSec = 0;
  }
  else {
    (void) threadTimes (&actorCap, K_ALLACTORTHREADS,
                        &timesP->ins, &timesP->ext);
  }
}
#else
void ttGetThreadTimes(ttThreadTimes* timesP)
{
  (void) times(timesP);
}
#endif

/*********************************************************************
 *
 *  FUNCTION:       ttCalcElapsedThreadTimes
 *
 *  DESCRIPTION:    This function calculates the user and kernel
 *                  time deltas.
 *
 *  PARAMETERS:     ttThreadTimes* beforeP  beginning timestamp (IN)
 *                  ttThreadTimes* afterP   ending timestamp (IN)
 *                  double* kernelDeltaP     kernel time delta (OUT)
 *                  double* userDeltaP       user time delta (OUT)
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

#ifdef SB_P_OS_CHORUS
void
ttCalcElapsedThreadTimes(ttThreadTimes* beforeP,
                         ttThreadTimes* afterP,
                         double* kernelDeltaP,
                         double* userDeltaP)
{
  double kernelBefore;
  double kernelAfter;
  double userBefore;
  double userAfter;

  kernelBefore = (beforeP->ext.tmSec) + (beforeP->ext.tmNSec / 1e9);
  kernelAfter  = (afterP->ext.tmSec) + (afterP->ext.tmNSec  / 1e9);
  *kernelDeltaP = kernelAfter - kernelBefore;

  userBefore = (beforeP->ins.tmSec) + (beforeP->ins.tmNSec / 1e9);
  userAfter  = (afterP->ins.tmSec) + (afterP->ins.tmNSec  / 1e9);
  *userDeltaP = userAfter - userBefore;

}
#else
void
ttCalcElapsedThreadTimes(ttThreadTimes* beforeP,
                         ttThreadTimes* afterP,
                         double* kernelDeltaP,
                         double* userDeltaP)
{
  double ticks = (double)sysconf(_SC_CLK_TCK);

  *kernelDeltaP = (afterP->tms_stime - beforeP->tms_stime) / ticks;
  *userDeltaP = (afterP->tms_utime - beforeP->tms_utime) / ticks;
}
#endif

/*********************************************************************
 *
 *  FUNCTION:       ttGetWallClockTime
 *
 *  DESCRIPTION:    This function gets the current wall-clock time.
 *
 *  PARAMETERS:     ttWallClockTime* timeP  tms time structure (OUT)
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void
ttGetWallClockTime(ttWallClockTime* timeP)
{
  gettimeofday(timeP, NULL);
}

/*********************************************************************
 *
 *  FUNCTION:       ttCalcElapsedWallClockTime
 *
 *  DESCRIPTION:    This function calculates the elapsed wall-clock
 *                  time is msec.
 *
 *  PARAMETERS:     ttWallClockTime* beforeP        starting timestamp
 *                  ttWallClockTime* afterP         ending timestamp
 *                  double* nmillisecondsP           elapsed time (OUT)
 *
 *  RETURNS:        void
 *
 *  NOTES:          NONE
 *
 *********************************************************************/

void
ttCalcElapsedWallClockTime(ttWallClockTime* beforeP,
                           ttWallClockTime* afterP,
                           double* nmillisP)
{
  *nmillisP = (afterP->tv_sec - beforeP->tv_sec)*1000.0 +
    (afterP->tv_usec - beforeP->tv_usec)/1000.0;
}

#endif

/* Emacs variable settings */
/* Local Variables: */
/* tab-width:8 */
/* indent-tabs-mode:nil */
/* c-basic-offset:2 */
/* End: */
