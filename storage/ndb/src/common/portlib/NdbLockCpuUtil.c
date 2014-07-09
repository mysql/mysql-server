/*
   Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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


#include <ndb_global.h>
#include <NdbThread.h>
#include <NdbLockCpuUtil.h>
#include <NdbMem.h>
#include <NdbMutex.h>

#define MAX_PROCESSOR_SETS 64

struct processor_set_handler
{
  int ref_count;
  struct NdbCpuSet *ndb_cpu_set;
  const Uint32 *cpu_ids;
  Uint32 num_cpu_ids;
  Uint32 index;
};
static struct processor_set_handler proc_set_array[MAX_PROCESSOR_SETS];
static NdbMutex *g_ndb_lock_cpu_mutex = 0;
 
/* Used from Ndb_Lock* functions */
static void
remove_use_processor_set(Uint32 proc_set_id)
{
  struct processor_set_handler *handler = &proc_set_array[proc_set_id];

  handler->ref_count--;
  assert(handler->ref_count >= 0);
  if (handler->ref_count == 0)
  {
    NdbThread_LockDestroyCPUSet(handler->ndb_cpu_set);
    free((void*)handler->cpu_ids);
    handler->num_cpu_ids = 0;
    handler->cpu_ids = NULL;
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

static int
use_processor_set(const Uint32 *cpu_ids,
                  Uint32 num_cpu_ids,
                  Uint32 *proc_set_id)
{
  int ret = 0;
  Uint32 i;
  Uint32 ret_proc_set_id = UNDEFINED_PROCESSOR_SET;
  struct processor_set_handler *handler;

  for (i = 0; i < MAX_PROCESSOR_SETS; i++)
  {
    handler = &proc_set_array[i];
    if (handler->num_cpu_ids == num_cpu_ids &&
        (memcmp(cpu_ids,
                handler->cpu_ids,
                sizeof(Uint32) * num_cpu_ids) == 0))
    {
      handler->ref_count++;
      ret_proc_set_id = i;
      goto end;
    }
  }
  for (i = 0; i < MAX_PROCESSOR_SETS; i++)
  {
    handler = &proc_set_array[i];
    if (handler->ref_count == 0)
    {
      handler->cpu_ids = malloc(sizeof(Uint32) * num_cpu_ids);
      if (handler->cpu_ids == NULL)
      {
        ret = errno;
        goto end;
      }
      if ((ret = NdbThread_LockCreateCPUSet(cpu_ids,
                                            num_cpu_ids,
                                            &handler->ndb_cpu_set) != 0))
      {
        free((void*)handler->cpu_ids);
        handler->num_cpu_ids = 0;
        handler->cpu_ids = NULL;
        goto end;
      }
      handler->ref_count = 1;
      handler->num_cpu_ids = num_cpu_ids;
      ret_proc_set_id = i;
      goto end;
    }
  }
end:
  if (!ret)
    *proc_set_id = ret_proc_set_id;
  return ret;
}

int
Ndb_UnlockCPU(struct NdbThread* pThread)
{
  int error_no;
  Uint32 proc_set_id;

  NdbMutex_Lock(g_ndb_lock_cpu_mutex);
  error_no = NdbThread_UnlockCPU(pThread);
  proc_set_id = find_processor_set(pThread);
  if (proc_set_id == UNDEFINED_PROCESSOR_SET)
    goto end;
  remove_use_processor_set(proc_set_id);
end:
  NdbMutex_Unlock(g_ndb_lock_cpu_mutex);
  return error_no;
}

int
Ndb_LockCPUSet(struct NdbThread* pThread,
               const Uint32 *cpu_ids,
               Uint32 num_cpu_ids)
{
  int error_no;
  Uint32 proc_set_id;

  NdbMutex_Lock(g_ndb_lock_cpu_mutex);
  if ((error_no = use_processor_set(cpu_ids,
                                    num_cpu_ids,
                                    &proc_set_id)))
  {
    goto end;
  }
  if ((error_no = NdbThread_LockCPUSet(pThread,
                         proc_set_array[proc_set_id].ndb_cpu_set,
                         &proc_set_array[proc_set_id])))
  {
    remove_use_processor_set(proc_set_id);
  }

end:
  NdbMutex_Unlock(g_ndb_lock_cpu_mutex);
  return error_no;
}

int
Ndb_LockCPU(struct NdbThread* pThread, Uint32 cpu_id)
{
  int error_no;
  NdbMutex_Lock(g_ndb_lock_cpu_mutex);
  error_no = NdbThread_LockCPU(pThread,
                               cpu_id,
                               NULL);
  NdbMutex_Unlock(g_ndb_lock_cpu_mutex);
  return error_no;
}

/* Initialisation function */
int
NdbLockCpu_Init()
{
  Uint32 i;
  struct processor_set_handler *handler;

  for (i = 0; i < MAX_PROCESSOR_SETS; i++)
  {
    handler = &proc_set_array[i];
    handler->ref_count = 0;
    handler->cpu_ids = NULL;
    handler->num_cpu_ids = 0;
    handler->index = i;
  }
  g_ndb_lock_cpu_mutex = NdbMutex_Create();
  return 0;
}

/* Function called at process end */
void
NdbLockCpu_End()
{
  Uint32 i;
  struct processor_set_handler *handler;

  NdbMutex_Lock(g_ndb_lock_cpu_mutex);
  for (i = 0; i < MAX_PROCESSOR_SETS; i++)
  {
    handler = &proc_set_array[i];
    if (handler->ref_count != 0)
    {
      abort();
    }
  }
  NdbMutex_Unlock(g_ndb_lock_cpu_mutex);
  if (g_ndb_lock_cpu_mutex)
  {
    NdbMutex_Destroy(g_ndb_lock_cpu_mutex);
  }
}
