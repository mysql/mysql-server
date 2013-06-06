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

#ifndef PFS_ROUTINE_H
#define PFS_ROUTINE_H

/**
  @file storage/perfschema/pfs_program.h
  Stored Program data structures (declarations).
*/

#include "pfs_column_types.h"
#include "pfs_stat.h"

#define OBJECT_NAME_LENGTH 80
#define SCHEMA_NAME_LENGTH 80
#define HASH_KEY_LENGTH sizeof(enum_object_type) + OBJECT_NAME_LENGTH + 1 + SCHEMA_NAME_LENGTH + 1

extern LF_HASH program_hash;
extern ulong program_max;
extern ulong program_lost;

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
  char m_hash_key[HASH_KEY_LENGTH];
  uint m_key_length;
}; 

struct PFS_program : public PFS_instr
{
  /** Object type. */
  enum_object_type m_type;

  /** Object name. */
  char m_object_name[OBJECT_NAME_LENGTH];
  int m_object_name_length;
 
  /** Object Schema name. */
  char m_schema_name[SCHEMA_NAME_LENGTH];
  int m_schema_name_length;

  /** Sub statement stat. */
  PFS_statement_stat m_stmt_stat;

  /** Stored program stat. */
  PFS_sp_stat m_sp_stat;

  /** Hash key */
  PFS_program_key m_key;

  /** Referesh setup object flags. */
  void referesh_setup_object_flags(PFS_thread* thread);

  /** Reset data for this record. */                                            
  void reset_data(); 
};

extern PFS_program *program_array;

int init_program(const PFS_global_param *param);
void cleanup_program(void);
int init_program_hash(void);
void cleanup_program_hash(void);

void reset_esms_by_program();

PFS_program*
find_or_create_program(PFS_thread *thread,                                      
                      enum_object_type object_type,                                         
                      const char *object_name,                                  
                      uint object_name_length,                                  
                      const char *schema,                                       
                      uint schema_length,
                      my_bool fromSP);

int
drop_program(PFS_thread *thread,
             enum_object_type object_type,
             const char *object_name,
             uint object_name_length,
             const char *schema_name,
             uint schema_name_length);
#endif
