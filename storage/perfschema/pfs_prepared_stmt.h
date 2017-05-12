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

#ifndef PFS_PS_H
#define PFS_PS_H

/**
  @file storage/perfschema/pfs_prepared_stmt.h
  Stored Program data structures (declarations).
*/

#include <sys/types.h>

#include "include/mysql/psi/mysql_ps.h"
#include "my_inttypes.h"
#include "pfs_program.h"
#include "pfs_stat.h"

#define PS_NAME_LENGTH NAME_LEN

struct PFS_ALIGNED PFS_prepared_stmt : public PFS_instr
{
  /** Column OBJECT_INSTANCE_BEGIN */
  const void *m_identity;

  /** STATEMENT_ID */
  ulonglong m_stmt_id;

  /** STATEMENT_NAME */
  char m_stmt_name[PS_NAME_LENGTH];
  uint m_stmt_name_length;

  /** SQL_TEXT */
  char m_sqltext[COL_INFO_SIZE];
  uint m_sqltext_length;

  /** Column OWNER_THREAD_ID */
  ulonglong m_owner_thread_id;

  /** Column OWNER_EVENT_ID. */
  ulonglong m_owner_event_id;

  /** Column OBJECT_OWNER_TYPE. */
  enum_object_type m_owner_object_type;

  /** Column OBJECT_OWNER_SCHEMA. */
  char m_owner_object_schema[COL_OBJECT_SCHEMA_SIZE];
  uint m_owner_object_schema_length;

  /** Column OBJECT_OWNER_NAME. */
  char m_owner_object_name[COL_OBJECT_NAME_SIZE];
  uint m_owner_object_name_length;

  /** COLUMN TIMER_PREPARE. Prepared statement prepare stat. */
  PFS_single_stat m_prepare_stat;

  /** COLUMN COUNT_REPREPARE. Prepared statement re-prepare stat. */
  PFS_single_stat m_reprepare_stat;

  /** Prepared statement execution stat. */
  PFS_statement_stat m_execute_stat;

  /** Reset data for this record. */
  void reset_data();
};

int init_prepared_stmt(const PFS_global_param *param);
void cleanup_prepared_stmt(void);

void reset_prepared_stmt_instances();

PFS_prepared_stmt *create_prepared_stmt(void *identity,
                                        PFS_thread *thread,
                                        PFS_program *pfs_program,
                                        PFS_events_statements *pfs_stmt,
                                        uint stmt_id,
                                        const char *stmt_name,
                                        uint stmt_name_length,
                                        const char *sqltext,
                                        uint sqltext_length);
void delete_prepared_stmt(PFS_prepared_stmt *pfs_ps);
#endif
