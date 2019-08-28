/*
   Copyright (c) 2013, 2017, Oracle and/or its affiliates. All rights reserved.

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


#include <ndb_global.h>
#include <NdbThread.h>
#include <NdbLockCpuUtil.h>
#include <NdbMutex.h>

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
static struct processor_set_handler *proc_set_array = NULL;
static NdbMutex *ndb_lock_cpu_mutex = 0;
 
/* Used from Ndb_Lock* functions */
static void
remove_use_processor_set(Uint32 proc_set_id)
{
  struct processor_set_handler *handler = &proc_set_array[proc_set_id];

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
    free((void*)handler->cpu_ids);
    handler->num_cpu_ids = 0;
    handler->cpu_ids = NULL;
    handler->is_exclusive = FALSE;
  }
}

static
Uint32
find_processor_set(struct NdbThread *pThread)
{
  const struct processor_set_handler *handler =
    NdbThread_LockGetCPUSetKey(pThread);
  if (handler == NULL)
    return UNDEFINED_PROCESSOR_SET;
  return handler->index;
}

static void
init_handler(struct processor_set_handler *handler,
             unsigned int i)
{
  handler->ref_count = 0;
  handler->cpu_ids = NULL;
  handler->num_cpu_ids = 0;
  handler->index = i;
  handler->is_exclusive = FALSE;
}

static int
use_processor_set(const Uint32 *cpu_ids,
                  Uint32 num_cpu_ids,
                  Uint32 *proc_set_id,
                  int is_exclusive)
{
  int ret = 0;
  Uint32 i;
  struct processor_set_handler *handler;
  struct processor_set_handler *new_proc_set_array;

  for (i = 0; i < num_processor_sets; i++)
  {
    handler = &proc_set_array[i];
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
      handler = &proc_set_array[i];
      if (handler->ref_count == 0)
      {
        handler->cpu_ids = malloc(sizeof(Uint32) * num_cpu_ids);
        if (handler->cpu_ids == NULL)
        {
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
          free((void*)handler->cpu_ids);
          handler->num_cpu_ids = 0;
          handler->cpu_ids = NULL;
          return ret;
        }
        handler->ref_count = 1;
        handler->num_cpu_ids = num_cpu_ids;
        handler->is_exclusive = is_exclusive;
        memcpy((void*)handler->cpu_ids, cpu_ids, num_cpu_ids * sizeof(Uint32));
        *proc_set_id = i;
        return 0;
      }
    }
    /**
     * The current array of processor set handlers is too small, double its
     * size and try again.
     */
    new_proc_set_array = malloc(2 * num_processor_sets *
                                sizeof(struct processor_set_handler));

    if (new_proc_set_array == NULL)
    {
      return errno;
    }
    memcpy(new_proc_set_array,
           proc_set_array,
           sizeof(struct processor_set_handler) * num_processor_sets);

    for (i = num_processor_sets; i < 2 * num_processor_sets; i++)
    {
      init_handler(&new_proc_set_array[i], i);
    }

    free(proc_set_array);
    proc_set_array = new_proc_set_array;
    num_processor_sets *= 2;
  }
  /* Should never arrive here */
  return ret;
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
    NdbThread_UnassignFromCPUSet(pThread, 
                             proc_set_array[proc_set_id].ndb_cpu_set);
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

  NdbMutex_Lock(ndb_lock_cpu_mutex);
  if ((error_no = use_processor_set(cpu_ids,
                                    num_cpu_ids,
                                    &proc_set_id,
                                    is_exclusive)))
  {
    goto end;
  }
  if (is_exclusive)
  {
    if ((error_no = NdbThread_LockCPUSetExclusive(pThread,
                           proc_set_array[proc_set_id].ndb_cpu_set,
                           &proc_set_array[proc_set_id])))
    {
      remove_use_processor_set(proc_set_id);
    }
  }
  else
  {
    if ((error_no = NdbThread_LockCPUSet(pThread,
                           proc_set_array[proc_set_id].ndb_cpu_set,
                           &proc_set_array[proc_set_id])))
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
                               NULL);
  NdbMutex_Unlock(ndb_lock_cpu_mutex);
  return error_no;
}

/* Initialisation function */
int
NdbLockCpu_Init()
{
  Uint32 i;
  proc_set_array = malloc(num_processor_sets *
                          sizeof(struct processor_set_handler));

  if (proc_set_array == NULL)
  {
    return 1;
  }

  for (i = 0; i < num_processor_sets; i++)
  {
    init_handler(&proc_set_array[i], i);
  }
  ndb_lock_cpu_mutex = NdbMutex_Create();
  if (ndb_lock_cpu_mutex == NULL)
  {
    free(proc_set_array);
    return 1;
  }
  return 0;
}

/* Function called at process end */
void
NdbLockCpu_End()
{
  Uint32 i;
  struct processor_set_handler *handler;

  NdbMutex_Lock(ndb_lock_cpu_mutex);
  for (i = 0; i < num_processor_sets; i++)
  {
    handler = &proc_set_array[i];
    if (handler->ref_count != 0)
    {
      abort();
    }
  }
  free(proc_set_array);
  proc_set_array = NULL;
  NdbMutex_Unlock(ndb_lock_cpu_mutex);
  NdbMutex_Destroy(ndb_lock_cpu_mutex);
  ndb_lock_cpu_mutex = NULL;
}
