/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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
#include "pfs_buffer_container.h"
#include <string.h>

/**
  Initialize table PREPARED_STATEMENTS_INSTANCE.
  @param param performance schema sizing
*/
int init_prepared_stmt(const PFS_global_param *param)
{
  if (global_prepared_stmt_container.init(param->m_prepared_stmt_sizing))
    return 1;

  reset_prepared_stmt_instances();
  return 0;
}

/** Cleanup table PREPARED_STATEMENTS_INSTANCE. */
void cleanup_prepared_stmt(void)
{
  global_prepared_stmt_container.cleanup();
}

void PFS_prepared_stmt::reset_data()
{
  m_prepare_stat.reset();
  m_reprepare_stat.reset();
  m_execute_stat.reset();
}

static void fct_reset_prepared_stmt_instances(PFS_prepared_stmt *pfs)
{
  pfs->reset_data();
}

void reset_prepared_stmt_instances()
{
  global_prepared_stmt_container.apply_all(fct_reset_prepared_stmt_instances);
}

PFS_prepared_stmt*
create_prepared_stmt(void *identity,
                     PFS_thread *thread, PFS_program *pfs_program,
                     PFS_events_statements *pfs_stmt, uint stmt_id,
                     const char* stmt_name, uint stmt_name_length,
                     const char* sqltext, uint sqltext_length)
{
  PFS_prepared_stmt *pfs= NULL;
  pfs_dirty_state dirty_state;

  /* Create a new record in prepared stmt stat array. */
  pfs= global_prepared_stmt_container.allocate(& dirty_state);
  if (pfs != NULL)
  {
    /* Reset the stats. */
    pfs->reset_data();
    /* Do the assignments. */
    pfs->m_identity= identity;
    /* Set query text if available, else it will be set later. */
    if (sqltext_length > 0)
      strncpy(pfs->m_sqltext, sqltext, sqltext_length);

    pfs->m_sqltext_length= sqltext_length;

    if (stmt_name != NULL)
    {
      pfs->m_stmt_name_length= stmt_name_length;
      if (pfs->m_stmt_name_length > PS_NAME_LENGTH)
        pfs->m_stmt_name_length= PS_NAME_LENGTH;
      strncpy(pfs->m_stmt_name, stmt_name, pfs->m_stmt_name_length);
    }
    else
      pfs->m_stmt_name_length= 0;

    pfs->m_stmt_id= stmt_id;
    pfs->m_owner_thread_id= thread->m_thread_internal_id;

    /* If this statement prepare is called from a SP. */
    if (pfs_program)
    {
      pfs->m_owner_object_type= pfs_program->m_type;
      strncpy(pfs->m_owner_object_schema, pfs_program->m_schema_name, pfs_program->m_schema_name_length);
      pfs->m_owner_object_schema_length= pfs_program->m_schema_name_length;
      strncpy(pfs->m_owner_object_name, pfs_program->m_object_name, pfs_program->m_object_name_length);
      pfs->m_owner_object_name_length= pfs_program->m_object_name_length;
    }
    else
    {
      pfs->m_owner_object_type= NO_OBJECT_TYPE;
      pfs->m_owner_object_schema_length= 0;
      pfs->m_owner_object_name_length= 0;
    }

    if (pfs_stmt)
    {
      if (pfs_program)
        pfs->m_owner_event_id= pfs_stmt->m_nesting_event_id;
      else
        pfs->m_owner_event_id= pfs_stmt->m_event_id;
    }

    /* Insert this record. */
    pfs->m_lock.dirty_to_allocated(& dirty_state);
  }

  return pfs;
}

void delete_prepared_stmt(PFS_prepared_stmt *pfs)
{
  global_prepared_stmt_container.deallocate(pfs);
  return;
}
