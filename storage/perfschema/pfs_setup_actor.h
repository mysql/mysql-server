/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

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
#include "sql_string.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_lock.h"

struct PFS_global_param;
struct PFS_thread;
class PFS_opaque_container_page;

/* WL#988 Roles Not implemented yet */
#define ROLENAME_LENGTH 64

/**
  @addtogroup performance_schema_buffers
  @{
*/

/** Hash key for @sa PFS_setup_actor. */
struct PFS_setup_actor_key
{
  /**
    Hash search key.
    This has to be a string for @c LF_HASH,
    the format is @c "<username><0x00><hostname><0x00><rolename><0x00>"
  */
  char
    m_hash_key[USERNAME_LENGTH + 1 + HOSTNAME_LENGTH + 1 + ROLENAME_LENGTH + 1];
  /** Length of @c m_hash_key. */
  uint m_key_length;
};

/** A setup_actor record. */
struct PFS_ALIGNED PFS_setup_actor
{
  /** Internal lock. */
  pfs_lock m_lock;
  /** Hash key. */
  PFS_setup_actor_key m_key;
  /** User name. This points inside the hash key. */
  const char *m_username;
  /** Length of @c m_username. */
  uint m_username_length;
  /** Host name. This points inside the hash key. */
  const char *m_hostname;
  /** Length of @c m_hostname. */
  uint m_hostname_length;
  /** Role name. This points inside the hash key. */
  const char *m_rolename;
  /** Length of @c m_rolename. */
  uint m_rolename_length;
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

int insert_setup_actor(const String *user,
                       const String *host,
                       const String *role,
                       bool enabled,
                       bool history);
int delete_setup_actor(const String *user,
                       const String *host,
                       const String *role);
int reset_setup_actor(void);
long setup_actor_count(void);

void lookup_setup_actor(PFS_thread *thread,
                        const char *user,
                        uint user_length,
                        const char *host,
                        uint host_length,
                        bool *enabled,
                        bool *history);

/** Update derived flags for all setup_actors. */
int update_setup_actors_derived_flags();

/* For show status. */

extern LF_HASH setup_actor_hash;

/** @} */
#endif
