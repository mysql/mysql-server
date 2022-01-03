/* Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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

#ifndef PFS_SETUP_ACTOR_H
#define PFS_SETUP_ACTOR_H

/**
  @file storage/perfschema/pfs_setup_actor.h
  Performance schema setup actors (declarations).
*/

#include <sys/types.h>

#include "lf.h"
#include "my_hostname.h" /* HOSTNAME_LENGTH */
#include "mysql_com.h"
#include "sql_string.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_lock.h"
#include "storage/perfschema/pfs_name.h"

struct PFS_global_param;
struct PFS_thread;
class PFS_opaque_container_page;

/**
  @addtogroup performance_schema_buffers
  @{
*/

/** Hash key for @sa PFS_setup_actor. */
struct PFS_setup_actor_key {
  PFS_user_name m_user_name;
  PFS_host_name m_host_name;
  PFS_role_name m_role_name;
};

/** A setup_actor record. */
struct PFS_ALIGNED PFS_setup_actor {
  /** Internal lock. */
  pfs_lock m_lock;
  /** Hash key. */
  PFS_setup_actor_key m_key;
  /** ENABLED flag. */
  bool m_enabled;
  /** HISTORY flag. */
  bool m_history;
  /** Container page. */
  PFS_opaque_container_page *m_page;
};

int init_setup_actor(const PFS_global_param *param);
void cleanup_setup_actor(void);
int init_setup_actor_hash(const PFS_global_param *param);
void cleanup_setup_actor_hash(void);

int insert_setup_actor(const PFS_user_name *user, const PFS_host_name *host,
                       const PFS_role_name *role, bool enabled, bool history);
int delete_setup_actor(const PFS_user_name *user, const PFS_host_name *host,
                       const PFS_role_name *role);
int reset_setup_actor(void);
long setup_actor_count(void);

void lookup_setup_actor(PFS_thread *thread, const PFS_user_name *user,
                        const PFS_host_name *host, bool *enabled,
                        bool *history);

/** Update derived flags for all setup_actors. */
int update_setup_actors_derived_flags();

/* For show status. */

extern LF_HASH setup_actor_hash;

/** @} */
#endif
