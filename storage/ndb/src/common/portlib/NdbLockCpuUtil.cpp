/*
   Copyright (c) 2013, 2023, Oracle and/or its affiliates.

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

#include "util/require.h"
#include <ndb_global.h>
#include <NdbThread.h>
#include <NdbLockCpuUtil.h>
#include <NdbMutex.h>
#include <memory>

#define UNDEFINED_PROCESSOR_SET 0xFFFF

static unsigned int num_processor_sets = 64;

struct processor_set_handler
{
  int ref_count;
  struct NdbCpuSet *ndb_cpu_set;
  const Uint32 *cpu_ids;
  Uint32 num_cpu_ids;
  Uint32 index;
  int is_exclusive;
};
static processor_set_handler* proc_set_array = nullptr;
static NdbMutex *ndb_lock_cpu_mutex = nullptr;
 
/* Used from Ndb_Lock* functions */
static void
remove_use_processor_set(Uint32 proc_set_id)
{
  processor_set_handler *handler = proc_set_array + proc_set_id;

  assert(proc_set_id < num_processor_sets);
  handler->ref_count--;
  assert(handler->ref_count >= 0);
  if (handler->ref_count == 0)
  {
    if (handler->is_exclusive)
    {
      NdbThread_LockDestroyCPUSetExclusive(handler->ndb_cpu_set);
    }
    else
    {
      NdbThread_LockDestroyCPUSet(handler->ndb_cpu_set);
    }
    delete[] handler->cpu_ids;
    handler->num_cpu_ids = 0;
    handler->cpu_ids = nullptr;
    handler->is_exclusive = false;
  }
}

static
Uint32
find_processor_set(struct NdbThread *pThread)
{
  const processor_set_handler *handler =
    NdbThread_LockGetCPUSetKey(pThread);
  if (handler == nullptr)
    return UNDEFINED_PROCESSOR_SET;
  return handler->index;
}

static void
init_handler(processor_set_handler *handler, unsigned int i)
{
  handler->ref_count = 0;
  handler->cpu_ids = nullptr;
  handler->num_cpu_ids = 0;
  handler->index = i;
  handler->is_exclusive = false;
}

static int
use_processor_set(const Uint32 *cpu_ids,
                  Uint32 num_cpu_ids,
                  Uint32 *proc_set_id,
                  int is_exclusive)
{
  int ret = 0;
  Uint32 i;
  processor_set_handler *handler;

  for (i = 0; i < num_processor_sets; i++)
  {
    handler = proc_set_array + i;
    if (handler->num_cpu_ids == num_cpu_ids &&
        (memcmp(cpu_ids,
                handler->cpu_ids,
                sizeof(Uint32) * num_cpu_ids) == 0))
    {
      if (handler->is_exclusive != is_exclusive)
      {
        return CPU_SET_MIX_EXCLUSIVE_ERROR;
      }
      handler->ref_count++;
      *proc_set_id = i;
      return 0;
    }
  }

  while (1)
  {
    for (i = 0; i < num_processor_sets; i++)
    {
      handler = proc_set_array + i;
      if (handler->ref_count == 0)
      {
        std::unique_ptr<Uint32[]> new_cpu_ids(new (std::nothrow)
                                                  Uint32[num_cpu_ids]);
        if (!new_cpu_ids)
        {
          require(errno != 0);
          return errno;
        }
        if (is_exclusive)
        {
          ret = NdbThread_LockCreateCPUSetExclusive(cpu_ids,
                                                 num_cpu_ids,
                                                 &handler->ndb_cpu_set);
        }
        else
        {
         ret = NdbThread_LockCreateCPUSet(cpu_ids,
                                          num_cpu_ids,
                                          &handler->ndb_cpu_set);
        }
        if (ret != 0)
        {
          handler->num_cpu_ids = 0;
          handler->cpu_ids = nullptr;
          return ret;
        }
        memcpy(new_cpu_ids.get(), cpu_ids, num_cpu_ids * sizeof(Uint32));
        handler->ref_count = 1;
        handler->cpu_ids = new_cpu_ids.release();
        handler->num_cpu_ids = num_cpu_ids;
        handler->is_exclusive = is_exclusive;
        *proc_set_id = i;
        return 0;
      }
    }
    /**
     * The current array of processor set handlers is too small, double its
     * size and try again.
     */
    processor_set_handler* new_proc_set_array =
        new (std::nothrow) processor_set_handler[2 * num_processor_sets];

    if (!new_proc_set_array)
    {
      require(errno != 0);
      return errno;
    }
    memcpy(new_proc_set_array,
           proc_set_array,
           sizeof(processor_set_handler) * num_processor_sets);

    for (i = num_processor_sets; i < 2 * num_processor_sets; i++)
    {
      init_handler(new_proc_set_array + i, i);
    }

    delete[] proc_set_array;
    proc_set_array = new_proc_set_array;
    num_processor_sets *= 2;
  }
  /* Should never arrive here */
  require(false);
  return -1;
}

int
Ndb_UnlockCPU(struct NdbThread* pThread)
{
  int error_no = 0;
  Uint32 proc_set_id;

  NdbMutex_Lock(ndb_lock_cpu_mutex);
  proc_set_id = find_processor_set(pThread);

  if (proc_set_id != UNDEFINED_PROCESSOR_SET)
  {
    processor_set_handler *handler = proc_set_array + proc_set_id;
    NdbThread_UnassignFromCPUSet(pThread, handler->ndb_cpu_set);
  }
  error_no = NdbThread_UnlockCPU(pThread);
  if (proc_set_id != UNDEFINED_PROCESSOR_SET)
  {
    remove_use_processor_set(proc_set_id);
  }
  NdbMutex_Unlock(ndb_lock_cpu_mutex);
  return error_no;
}

int
Ndb_LockCPUSet(struct NdbThread* pThread,
               const Uint32 *cpu_ids,
               Uint32 num_cpu_ids,
               int is_exclusive)
{
  int error_no;
  Uint32 proc_set_id;
  processor_set_handler *handler;

  NdbMutex_Lock(ndb_lock_cpu_mutex);
  if ((error_no = use_processor_set(cpu_ids,
                                    num_cpu_ids,
                                    &proc_set_id,
                                    is_exclusive)))
  {
    goto end;
  }

  handler = proc_set_array + proc_set_id;
  if (is_exclusive)
  {
    if ((error_no = NdbThread_LockCPUSetExclusive(pThread,
                                                  handler->ndb_cpu_set,
                                                  handler)))
    {
      remove_use_processor_set(proc_set_id);
    }
  }
  else
  {
    if ((error_no = NdbThread_LockCPUSet(pThread,
                                         handler->ndb_cpu_set,
                                         handler)))
    {
      remove_use_processor_set(proc_set_id);
    }
  }

end:
  NdbMutex_Unlock(ndb_lock_cpu_mutex);
  return error_no;
}

int
Ndb_LockCPU(struct NdbThread* pThread, Uint32 cpu_id)
{
  int error_no;
  NdbMutex_Lock(ndb_lock_cpu_mutex);
  error_no = NdbThread_LockCPU(pThread,
                               cpu_id,
                               nullptr);
  NdbMutex_Unlock(ndb_lock_cpu_mutex);
  return error_no;
}

/* Initialisation function */
int
NdbLockCpu_Init()
{
  Uint32 i;
  assert(proc_set_array == nullptr);
  delete[] proc_set_array;
  proc_set_array = new (std::nothrow)
                           processor_set_handler[num_processor_sets];

  if (!proc_set_array)
  {
    return 1;
  }

  for (i = 0; i < num_processor_sets; i++)
  {
    init_handler(proc_set_array + i, i);
  }
  ndb_lock_cpu_mutex = NdbMutex_Create();
  if (ndb_lock_cpu_mutex == nullptr)
  {
    delete[] proc_set_array;
    proc_set_array = nullptr;
    return 1;
  }
  return 0;
}

/* Function called at process end */
void
NdbLockCpu_End()
{
  Uint32 i;
  processor_set_handler *handler;

  NdbMutex_Lock(ndb_lock_cpu_mutex);
  for (i = 0; i < num_processor_sets; i++)
  {
    handler = proc_set_array + i;
    if (handler->ref_count != 0)
    {
      abort();
    }
  }
  delete[] proc_set_array;
  proc_set_array = nullptr;
  NdbMutex_Unlock(ndb_lock_cpu_mutex);
  NdbMutex_Destroy(ndb_lock_cpu_mutex);
  ndb_lock_cpu_mutex = nullptr;
}
