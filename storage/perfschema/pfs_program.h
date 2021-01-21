/* Copyright (c) 2013, 2021, Oracle and/or its affiliates.

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
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef PFS_PROGRAM_H
#define PFS_PROGRAM_H

/**
  @file storage/perfschema/pfs_program.h
  Stored Program data structures (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_stat.h"

#define PROGRAM_HASH_KEY_LENGTH sizeof(enum_object_type) + COL_OBJECT_NAME_SIZE + 1 + COL_OBJECT_SCHEMA_SIZE + 1

extern LF_HASH program_hash;

/**
  Hash key for a program.
*/
struct PFS_program_key
{
  /**
    Hash search key.
    This has to be a string for LF_HASH,
    the format is "<object_type><0x00><object_name><0x00><schema_name><0x00>"
  */
  char m_hash_key[PROGRAM_HASH_KEY_LENGTH];
  uint m_key_length;
};

struct PFS_ALIGNED PFS_program : public PFS_instr
{
  /** Object type. */
  enum_object_type m_type;

  /** Object name. */
  const char *m_object_name;
  int m_object_name_length;

  /** Object Schema name. */
  const char *m_schema_name;
  int m_schema_name_length;

  /** Hash key */
  PFS_program_key m_key;

  /** Sub statement stat. */
  PFS_statement_stat m_stmt_stat;

  /** Stored program stat. */
  PFS_sp_stat m_sp_stat;

  /** Referesh setup object flags. */
  void refresh_setup_object_flags(PFS_thread* thread);

  /** Reset data for this record. */
  void reset_data();
};

int init_program(const PFS_global_param *param);
void cleanup_program(void);
int init_program_hash(const PFS_global_param *param);
void cleanup_program_hash(void);

void reset_esms_by_program();

PFS_program*
find_or_create_program(PFS_thread *thread,
                      enum_object_type object_type,
                      const char *object_name,
                      uint object_name_length,
                      const char *schema,
                      uint schema_length);

void
drop_program(PFS_thread *thread,
             enum_object_type object_type,
             const char *object_name,
             uint object_name_length,
             const char *schema_name,
             uint schema_name_length);
#endif
