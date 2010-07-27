/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file storage/perfschema/pfs_server.cc
  Private interface for the server (implementation).
*/

#include "my_global.h"
#include "my_sys.h"
#include "mysys_err.h"
#include "pfs_server.h"
#include "pfs.h"
#include "pfs_global.h"
#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "pfs_events_waits.h"
#include "pfs_timer.h"

PFS_global_param pfs_param;

C_MODE_START
static void destroy_pfs_thread(void *key);
C_MODE_END

static void cleanup_performance_schema(void);

struct PSI_bootstrap*
initialize_performance_schema(const PFS_global_param *param)
{
  pfs_initialized= false;

  if (! param->m_enabled)
  {
    /*
      The performance schema is disabled in the startup command line.
      All the instrumentation is turned off.
    */
    return NULL;
  }

  init_timers();
  PFS_atomic::init();

  if (pthread_key_create(&THR_PFS, destroy_pfs_thread))
    return NULL;

  THR_PFS_initialized= true;

  if (init_sync_class(param->m_mutex_class_sizing,
                      param->m_rwlock_class_sizing,
                      param->m_cond_class_sizing) ||
      init_thread_class(param->m_thread_class_sizing) ||
      init_table_share(param->m_table_share_sizing) ||
      init_file_class(param->m_file_class_sizing) ||
      init_instruments(param) ||
      init_events_waits_history_long(
        param->m_events_waits_history_long_sizing) ||
      init_file_hash() ||
      init_table_share_hash())
  {
    /*
      The performance schema initialization failed.
      Free the memory used, and disable the instrumentation.
    */
    cleanup_performance_schema();
    return NULL;
  }

  pfs_initialized= true;
  return &PFS_bootstrap;
}

static void destroy_pfs_thread(void *key)
{
  PFS_thread* pfs= reinterpret_cast<PFS_thread*> (key);
  DBUG_ASSERT(pfs);
  /*
    This automatic cleanup is a last resort and best effort to avoid leaks,
    and may not work on windows due to the implementation of pthread_key_create().
    Please either use:
    - my_thread_end()
    - or PSI_server->delete_current_thread()
    in the instrumented code, to explicitly cleanup the instrumentation.

    Avoid invalid writes when the main() thread completes after shutdown:
    the memory pointed by pfs is already released.
  */
  if (pfs_initialized)
    destroy_thread(pfs);
}

static void cleanup_performance_schema(void)
{
  cleanup_instruments();
  cleanup_sync_class();
  cleanup_thread_class();
  cleanup_table_share();
  cleanup_file_class();
  cleanup_events_waits_history_long();
  cleanup_table_share_hash();
  cleanup_file_hash();
  PFS_atomic::cleanup();
}

void shutdown_performance_schema(void)
{
  pfs_initialized= false;
  cleanup_performance_schema();
  /*
    Be careful to not delete un-initialized keys,
    this would affect key 0, which is THR_KEY_mysys,
  */
  if (THR_PFS_initialized)
  {
    my_pthread_setspecific_ptr(THR_PFS, NULL);
    pthread_key_delete(THR_PFS);
    THR_PFS_initialized= false;
  }
}

