/* Copyright (c) 2010, 2019, Oracle and/or its affiliates. All rights reserved.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PFS_SETUP_OBJECT_H
#define PFS_SETUP_OBJECT_H

/**
  @file storage/perfschema/pfs_setup_object.h
  Performance schema setup object (declarations).
*/

#include <sys/types.h>

#include "lf.h"
#include "storage/perfschema/pfs_column_types.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_lock.h"

class String;
struct PFS_global_param;
class PFS_opaque_container_page;

/**
  @addtogroup performance_schema_buffers
  @{
*/

/** Hash key for @sa PFS_setup_object. */
struct PFS_setup_object_key {
  /**
    Hash search key.
    This has to be a string for @c LF_HASH,
    the format is @c "<enum_object_type><schema_name><0x00><object_name><0x00>"
  */
  char m_hash_key[1 + NAME_LEN + 1 + NAME_LEN + 1];
  uint m_key_length;
};

/** A setup_object record. */
struct PFS_ALIGNED PFS_setup_object {
  enum_object_type get_object_type() {
    return (enum_object_type)m_key.m_hash_key[0];
  }

  /** Internal lock. */
  pfs_lock m_lock;
  /** Hash key. */
  PFS_setup_object_key m_key;
  /** Schema name. Points inside @c m_key. */
  const char *m_schema_name;
  /** Length of @c m_schema_name. */
  uint m_schema_name_length;
  /** Object name. Points inside @c m_key. */
  const char *m_object_name;
  /** Length of @c m_object_name. */
  uint m_object_name_length;
  /** ENABLED flag. */
  bool m_enabled;
  /** TIMED flag. */
  bool m_timed;
  /** Container page. */
  PFS_opaque_container_page *m_page;
};

int init_setup_object(const PFS_global_param *param);
void cleanup_setup_object(void);
int init_setup_object_hash(const PFS_global_param *param);
void cleanup_setup_object_hash(void);

int insert_setup_object(enum_object_type object_type, const String *schema,
                        const String *object, bool enabled, bool timed);
int delete_setup_object(enum_object_type object_type, const String *schema,
                        const String *object);
int reset_setup_object(void);
long setup_object_count(void);

void lookup_setup_object(PFS_thread *thread, enum_object_type object_type,
                         const char *schema_name, int schema_name_length,
                         const char *object_name, int object_name_length,
                         bool *enabled, bool *timed);

/* For show status. */

extern LF_HASH setup_object_hash;

/** @} */
#endif
