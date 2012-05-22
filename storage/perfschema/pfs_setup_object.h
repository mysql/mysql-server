/* Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_SETUP_OBJECT_H
#define PFS_SETUP_OBJECT_H

/**
  @file storage/perfschema/pfs_setup_object.h
  Performance schema setup object (declarations).
*/

#include "pfs_lock.h"
#include "lf.h"

class String;
struct PFS_global_param;

/**
  @addtogroup Performance_schema_buffers
  @{
*/

/** Hash key for @sa PFS_setup_object. */
struct PFS_setup_object_key
{
  /**
    Hash search key.
    This has to be a string for LF_HASH,
    the format is "<enum_object_type><schema_name><0x00><object_name><0x00>"
  */
  char m_hash_key[1 + NAME_LEN + 1 + NAME_LEN + 1];
  uint m_key_length;
};

/** A setup_object record. */
struct PFS_ALIGNED PFS_setup_object
{
  enum_object_type get_object_type()
  {
    return (enum_object_type) m_key.m_hash_key[0];
  }

  /** Internal lock. */
  pfs_lock m_lock;
  /** Hash key. */
  PFS_setup_object_key m_key;
  /** Schema name. Points inside m_key. */
  const char *m_schema_name;
  /** Length of @c m_schema_name. */
  uint m_schema_name_length;
  /** Object name. Points inside m_key. */
  const char *m_object_name;
  /** Length of @c m_object_name. */
  uint m_object_name_length;
  /** ENABLED flag. */
  bool m_enabled;
  /** TIMED flag. */
  bool m_timed;
};

int init_setup_object(const PFS_global_param *param);
void cleanup_setup_object(void);
int init_setup_object_hash(void);
void cleanup_setup_object_hash(void);

int insert_setup_object(enum_object_type object_type, const String *schema,
                        const String *object, bool enabled, bool timed);
int delete_setup_object(enum_object_type object_type, const String *schema,
                        const String *object);
int reset_setup_object(void);
long setup_object_count(void);

void lookup_setup_object(PFS_thread *thread,
                         enum_object_type object_type,
                         const char *schema_name, int schema_name_length,
                         const char *object_name, int object_name_length,
                         bool *enabled, bool *timed);

/* For iterators and show status. */

extern ulong setup_object_max;

/* Exposing the data directly, for iterators. */

extern PFS_setup_object *setup_object_array;

extern LF_HASH setup_object_hash;

/** @} */
#endif

