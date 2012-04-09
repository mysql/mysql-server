/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_CONDITION_H
#define NDB_CONDITION_H

#include "NdbMutex.h"

#ifdef	__cplusplus
extern "C" {
#endif

struct NdbCondition
{
  pthread_cond_t cond;
};

/**
 * Create a condition
 *
 * returnvalue: pointer to the condition structure
 */
struct NdbCondition* NdbCondition_Create(void);

/**
 * Initialize a condition created with file-storage or on the stack
 *
 * returnvalue: 0 = success
 */
int NdbCondition_Init(struct NdbCondition* p_cond);

/**
 * Wait for a condition, allows a thread to wait for
 * a condition and atomically releases the associated mutex.
 *
 * p_cond: pointer to the condition structure
 * p_mutex: pointer to the mutex structure
 * returnvalue: 0 = succeeded, 1 = failed
 */
int NdbCondition_Wait(struct NdbCondition* p_cond,
		      NdbMutex* p_mutex);

/*
 * Wait for a condition with timeout, allows a thread to
 *  wait for a condition and atomically releases the associated mutex.
 *
 * @param p_cond - pointer to the condition structure
 * @param p_mutex - pointer to the mutex structure
 * @param msec - Wait for msec milli seconds the most
 * @return 0 = succeeded, 1 = failed
 * @
 */
int
NdbCondition_WaitTimeout(struct NdbCondition* p_cond,
			 NdbMutex* p_mutex,
			 int msec);
/*
 * same as NdbCondition_WaitTimeout only that
 * endtime is a absolute time computed using
 * NdbCondition_ComputeAbsTime
 */
int
NdbCondition_WaitTimeoutAbs(struct NdbCondition* p_cond,
			 NdbMutex* p_mutex,
			 const struct timespec * endtime);

/**
 * compute an absolute time suitable for use with NdbCondition_WaitTimeoutAbs
 * and store it in <em>dst</em> <em>ms</em> specifies milliseconds from now
 */
void
NdbCondition_ComputeAbsTime(struct timespec * dst, unsigned ms);

/**
 * Signal a condition
 *
 * p_cond: pointer to the condition structure
 * returnvalue: 0 = succeeded, 1 = failed
 */
int NdbCondition_Signal(struct NdbCondition* p_cond);


/**
 * Broadcast a condition
 *
 * p_cond: pointer to the condition structure
 * returnvalue: 0 = succeeded, 1 = failed
 */
int NdbCondition_Broadcast(struct NdbCondition* p_cond);

/**
 * Destroy a condition
 *
 * p_cond: pointer to the condition structure
 * returnvalue: 0 = succeeded, 1 = failed
 */
int NdbCondition_Destroy(struct NdbCondition* p_cond);

#ifdef	__cplusplus
}
#endif

#endif


