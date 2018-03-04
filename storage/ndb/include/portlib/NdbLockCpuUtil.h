/*
   Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_LOCK_CPU_H
#define NDB_LOCK_CPU_H

#include <ndb_global.h>

/**
 * Lock/Unlock Thread to CPU
 */
#ifdef __cplusplus
extern "C" {
#endif

struct NdbThread;

/**
 * Interface to lock a thread to a certain set of CPUs.
 * Input:
 *   NdbThread* thread: This is the internal representation of the thread
 *   cpu_ids: This is an array of CPU ids
 *     For the most part this is a direct translation of CPU ids used in the
 *     OS. An exception is Windows where we start counting from 0 and start
 *     each new NUMA group on the next multiple of 64.
 *   num_cpu_ids: Number of CPUs in the array
 *   is_exclusive: If true use exclusive CPU locking.
 *     Exclusive CPU locking is currently supported on Solaris, thus the
 *     OS will ensure that no other thread or even interrupt will use the
 *     same CPUs as this thread. Non-exclusive locking is supported on
 *     Linux, Windows, Solaris and FreeBSD. In this case the thread is
 *     bound to use the set of CPUs specified here, but it will not affect
 *     other threads from other programs.
 */
int Ndb_LockCPUSet(struct NdbThread* thread,
                   const Uint32 *cpu_ids,
                   Uint32 num_cpu_ids,
                   int is_exclusive);
/**
 * A simple interface to bind one thread to one CPU non-exclusively.
 * Has similar effect as calling above function with 1 CPU in the
 * array and setting is_exclusive to FALSE.
 */
int Ndb_LockCPU(struct NdbThread*, Uint32 cpu);

/**
 * Interface to UNDO a previous call to Ndb_LockCPUSet.
 * We will attempt also to restore the locking of the thread that was
 * in place before the Ndb_LockCPUSet call. This can be important e.g.
 * if the user had provided special bindings to the process before
 * starting it. It is important that we restore these if we decide to
 * unlock the CPU.
 *
 * The new locking we set above can be set to different CPUs than what
 * the process is locked to.
 *
 * Release all memory attached to the thread regarding its CPU locking.
 */
int Ndb_UnlockCPU(struct NdbThread*);
#ifdef __cplusplus
}
#endif

#endif
