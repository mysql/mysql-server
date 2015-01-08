/* Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


#include "my_global.h"
#include "my_thread.h"
#include "pfs_con_slice.h"
#include "pfs_stat.h"
#include "pfs_global.h"
#include "pfs_instr_class.h"

/**
  @file storage/perfschema/pfs_con_slice.cc
  Performance schema connection slice (implementation).
*/

/**
  @addtogroup Performance_schema_buffers
  @{
*/

PFS_single_stat *
PFS_connection_slice::alloc_waits_slice(uint sizing)
{
  PFS_single_stat *slice= NULL;
  uint index;

  if (sizing > 0)
  {
    slice= PFS_MALLOC_ARRAY(sizing, PFS_single_stat, MYF(MY_ZEROFILL));
    if (unlikely(slice == NULL))
      return NULL;

    for (index= 0; index < sizing; index++)
      slice[index].reset();
  }

  return slice;
}

PFS_stage_stat *
PFS_connection_slice::alloc_stages_slice(uint sizing)
{
  PFS_stage_stat *slice= NULL;
  uint index;

  if (sizing > 0)
  {
    slice= PFS_MALLOC_ARRAY(sizing, PFS_stage_stat, MYF(MY_ZEROFILL));
    if (unlikely(slice == NULL))
      return NULL;

    for (index= 0; index < sizing; index++)
      slice[index].reset();
  }

  return slice;
}

PFS_statement_stat *
PFS_connection_slice::alloc_statements_slice(uint sizing)
{
  PFS_statement_stat *slice= NULL;
  uint index;

  if (sizing > 0)
  {
    slice= PFS_MALLOC_ARRAY(sizing, PFS_statement_stat, MYF(MY_ZEROFILL));
    if (unlikely(slice == NULL))
      return NULL;

    for (index= 0; index < sizing; index++)
      slice[index].reset();
  }

  return slice;
}

PFS_transaction_stat *
PFS_connection_slice::alloc_transactions_slice(uint sizing)
{
  PFS_transaction_stat *slice= NULL;
  uint index;

  if (sizing > 0)
  {
    slice= PFS_MALLOC_ARRAY(sizing, PFS_transaction_stat, MYF(MY_ZEROFILL));
    if (unlikely(slice == NULL))
      return NULL;

    for (index= 0; index < sizing; index++)
      slice[index].reset();
  }

  return slice;
}

PFS_memory_stat *
PFS_connection_slice::alloc_memory_slice(uint sizing)
{
  PFS_memory_stat *slice= NULL;
  uint index;

  if (sizing > 0)
  {
    slice= PFS_MALLOC_ARRAY(sizing, PFS_memory_stat, MYF(MY_ZEROFILL));
    if (unlikely(slice == NULL))
      return NULL;

    for (index= 0; index < sizing; index++)
      slice[index].reset();
  }

  return slice;
}

void PFS_connection_slice::reset_waits_stats()
{
  PFS_single_stat *stat= m_instr_class_waits_stats;
  PFS_single_stat *stat_last= stat + wait_class_max;
  for ( ; stat < stat_last; stat++)
    stat->reset();
}

void PFS_connection_slice::reset_stages_stats()
{
  PFS_stage_stat *stat= m_instr_class_stages_stats;
  PFS_stage_stat *stat_last= stat + stage_class_max;
  for ( ; stat < stat_last; stat++)
    stat->reset();
}

void PFS_connection_slice::reset_statements_stats()
{
  PFS_statement_stat *stat= m_instr_class_statements_stats;
  PFS_statement_stat *stat_last= stat + statement_class_max;
  for ( ; stat < stat_last; stat++)
    stat->reset();
}

void PFS_connection_slice::reset_transactions_stats()
{
  PFS_transaction_stat *stat=
                    &m_instr_class_transactions_stats[GLOBAL_TRANSACTION_INDEX];
  if (stat)
    stat->reset();
}

void PFS_connection_slice::rebase_memory_stats()
{
  PFS_memory_stat *stat= m_instr_class_memory_stats;
  PFS_memory_stat *stat_last= stat + memory_class_max;
  for ( ; stat < stat_last; stat++)
    stat->reset();
}

/** @} */

