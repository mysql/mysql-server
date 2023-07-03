/* Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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
#include "storage/perfschema/pfs_name.h"

class String;
struct PFS_global_param;
class PFS_opaque_container_page;

/**
  @addtogroup performance_schema_buffers
  @{
*/

/** Hash key for @sa PFS_setup_object. */
struct PFS_setup_object_key {
  enum_object_type m_object_type;
  PFS_schema_name m_schema_name;
  PFS_object_name m_object_name;
};

/** A setup_object record. */
struct PFS_ALIGNED PFS_setup_object {
  /** Internal lock. */
  pfs_lock m_lock;
  /** Hash key. */
  PFS_setup_object_key m_key;
  /** ENABLED flag. */
  bool m_enabled;
  /** TIMED flag. */
  bool m_timed;
  /** Container page. */
  PFS_opaque_container_page *m_page;
};

int init_setup_object(const PFS_global_param *param);
void cleanup_setup_object();
int init_setup_object_hash(const PFS_global_param *param);
void cleanup_setup_object_hash();

int insert_setup_object(enum_object_type object_type,
                        const PFS_schema_name *schema,
                        const PFS_object_name *object, bool enabled,
                        bool timed);
int delete_setup_object(enum_object_type object_type,
                        const PFS_schema_name *schema,
                        const PFS_object_name *object);
int reset_setup_object();
long setup_object_count();

void lookup_setup_object_table(PFS_thread *thread, enum_object_type object_type,
                               const PFS_schema_name *schema_name,
                               const PFS_table_name *table_name, bool *enabled,
                               bool *timed);

void lookup_setup_object_routine(PFS_thread *thread,
                                 enum_object_type object_type,
                                 const PFS_schema_name *schema,
                                 const PFS_routine_name *routine_name,
                                 bool *enabled, bool *timed);

/* For show status. */

extern LF_HASH setup_object_hash;

/** @} */
#endif
