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

#ifndef NDB_CONDITION_H
#define NDB_CONDITION_H

#include "NdbMutex.h"

#ifdef	__cplusplus
extern "C" {
#endif

struct NdbCondition;


/**
 * Create a condition
 *
 * returnvalue: pointer to the condition structure
 */
struct NdbCondition* NdbCondition_Create(void);

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


