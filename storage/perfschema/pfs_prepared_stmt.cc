/* Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/pfs_prepared_stmt.cc
  Prepared Statement data structures (implementation).
*/

/*
  This code needs extra visibility in the lexer structures
*/

#include "my_global.h"
#include "my_sys.h"
#include "pfs_instr.h"
#include "pfs_prepared_stmt.h"
#include "pfs_global.h"
#include "sql_string.h"
#include <string.h>

/** PREPARED_STATEMENTS_INSTANCE. */
PFS_prepared_stmt *prepared_stmt_array= NULL;

/** Max size of the prepared stmt array. */
ulong prepared_stmt_max= 0;
/** Number of prepared statement instances lost. */
ulong prepared_stmt_lost= 0;
/** True when prepared stmt array is full. */
bool prepared_stmt_full;

/**
  Initialize table PREPARED_STATEMENTS_INSTANCE.
  @param param performance schema sizing
*/
int init_prepared_stmt(const PFS_global_param *param)
{
  /*
    Allocate memory for prepared_stmt_array based on
    performance_schema_max_prepared_stmt_instances value.
  */
  prepared_stmt_max= param->m_prepared_stmt_sizing;
  prepared_stmt_lost= 0;
  prepared_stmt_full= false;

  if (prepared_stmt_max == 0)
    return 0;

  prepared_stmt_array=
    PFS_MALLOC_ARRAY(prepared_stmt_max, PFS_prepared_stmt,
                     MYF(MY_ZEROFILL));
  if (unlikely(prepared_stmt_array == NULL))
    return 1;

  PFS_prepared_stmt *pfs= prepared_stmt_array;
  PFS_prepared_stmt *pfs_last= prepared_stmt_array + prepared_stmt_max;
  
  for (; pfs < pfs_last ; pfs++)
  {
    pfs->reset_data();
  }

  return 0;
}

/** Cleanup table PREPARED_STATEMENTS_INSTANCE. */
void cleanup_prepared_stmt(void)
{
  /*  Free memory allocated to prepared_stmt_array. */
  pfs_free(prepared_stmt_array);
  prepared_stmt_array= NULL;
}

void PFS_prepared_stmt::reset_data()
{
  m_prepared_stmt_stat.reset();
}

void reset_prepared_stmt_instances()
{
  if (prepared_stmt_array == NULL)
    return;

  PFS_prepared_stmt *pfs= prepared_stmt_array;
  PFS_prepared_stmt *pfs_last= prepared_stmt_array + prepared_stmt_max;
  
  /* Reset statistics in prepared_stmt_array. */
  for (; pfs < pfs_last ; pfs++)
  {
    pfs->reset_data();
  }
}

PFS_prepared_stmt*
create_prepared_stmt(void *identity,
                     PFS_thread *thread,
                     PFS_events_statements *pfs_stmt,
                     char* sqltext, uint sqltext_length)
{
  if (prepared_stmt_array == NULL || prepared_stmt_max == 0)
    return NULL;

  PFS_prepared_stmt *pfs= NULL;
  static uint PFS_ALIGNED prepared_stmt_monotonic_index= 0;
  ulong index= 0;
  ulong attempts= 0;
  pfs_dirty_state dirty_state;

  if(prepared_stmt_full)
  {
    prepared_stmt_lost++;
    return NULL;
  }

  /* Create a new record in prepared stmt stat array. */
  while (++attempts <= prepared_stmt_max)
  {
    index= PFS_atomic::add_u32(& prepared_stmt_monotonic_index, 1) % prepared_stmt_max;
    pfs= prepared_stmt_array + index;
    
    if (pfs->m_lock.is_free())
    {
      if (pfs->m_lock.free_to_dirty(& dirty_state))
      {
        /* Do the assignments. */
        pfs->m_identity= identity;
        strncpy(pfs->m_sqltext, sqltext, sqltext_length);
        pfs->m_sqltext_length= sqltext_length;
        pfs->m_owner_thread_id= thread->m_thread_internal_id;

        DBUG_ASSERT(pfs_stmt != NULL);
        /* If this statement prepare is called from a SP. */
        if (pfs_stmt->m_schema_name_length > 0)
        {
          pfs->m_owner_event_id= pfs_stmt->m_nesting_event_id;
          pfs->m_owner_object_type= pfs_stmt->m_sp_type;
          strncpy(pfs->m_owner_object_schema, pfs_stmt->m_schema_name, pfs_stmt->m_schema_name_length);
          pfs->m_owner_object_schema_length= pfs_stmt->m_schema_name_length;
          strncpy(pfs->m_owner_object_name, pfs_stmt->m_object_name, pfs_stmt->m_object_name_length); 
          pfs->m_owner_object_name_length= pfs_stmt->m_object_name_length;
        }
        else
          pfs->m_owner_event_id= pfs_stmt->m_event_id;
 
        /* Insert this record. */
        pfs->m_lock.dirty_to_allocated(& dirty_state);
        return pfs;
      }
    }
  }
  prepared_stmt_lost++;
  prepared_stmt_full= true;
  return NULL;
}

void delete_prepared_stmt(PFS_thread *thread, PFS_prepared_stmt *pfs_ps)
{
  pfs_ps->m_lock.allocated_to_free();
  prepared_stmt_full= false;
  return;
}
