/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/pfs_prepared_statement.h
  Stored Program data structures (declarations).
*/

#include "pfs_stat.h"
#include "include/mysql/psi/psi.h"
#include "include/mysql/psi/mysql_ps.h"

#define PREPARED_STMT_HASH_KEY_LENGTH 80 /* Mayank TODO: write proper value. */
#define OBJECT_NAME_LENGTH NAME_LEN                                             
#define SCHEMA_NAME_LENGTH NAME_LEN

extern LF_HASH prepared_stmt_hash;
extern ulong prepared_stmt_max;
extern ulong prepared_stmt_lost;

/**
  Hash key for a prepared stmt.
*/
struct PFS_prepared_stmt_key 
{
  /**
    Hash search key.
    This has to be a string for LF_HASH,
    the format is "???" Mayank TODO.
  */
  char m_hash_key[PREPARED_STMT_HASH_KEY_LENGTH];
  uint m_key_length;
}; 

struct PFS_ALIGNED PFS_prepared_stmt : public PFS_instr
{
  /** Column OBJECT_INSTANCE_BEGIN */
  const void *m_identity;

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
  char m_owner_object_schema[SCHEMA_NAME_LENGTH];
  uint m_owner_object_schema_length;

  /** Column OBJECT_OWNER_NAME. */
  char m_owner_object_name[OBJECT_NAME_LENGTH];
  uint m_owner_object_name_length;

  //`TIMER_PREPARE` bigint(20) unsigned NOT NULL,

  /** Hash key */
  PFS_prepared_stmt_key m_key;

  /** Prepared stmt stat. */
  PFS_statement_stat m_prepared_stmt_execute_stat;

  /** Prepared stmt stat. */
  PFS_statement_stat m_prepared_stmt_stat;

  /** Reset data for this record. */                                            
  void reset_data(); 
};

extern PFS_prepared_stmt *prepared_stmt_array;

int init_prepared_stmt(const PFS_global_param *param);
void cleanup_prepared_stmt(void);
int init_prepared_stmt_hash(void);
void cleanup_prepared_stmt_hash(void);

void reset_prepared_stmt_instances();

PFS_prepared_stmt*
find_or_create_prepared_stmt(void *identity,
                             PFS_thread *thread,
                             PFS_events_statements *pfs_stmt,
                             char* sqltext, uint sqltext_length);
void delete_prepared_stmt(PFS_thread *thread, PFS_prepared_stmt *pfs_ps);
#endif
