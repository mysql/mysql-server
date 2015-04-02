/*
   Copyright (c) 2013, 2015 Oracle and/or its affiliates. All rights reserved.

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

int Ndb_LockCPUSet(struct NdbThread*,
                   const Uint32 *cpu_ids,
                   Uint32 num_cpu_ids);
int Ndb_LockCPU(struct NdbThread*, Uint32 cpu);
int Ndb_UnlockCPU(struct NdbThread*);
int NdbLockCpu_Init();
void NdbLockCpu_End();
#ifdef __cplusplus
}
#endif

#endif
