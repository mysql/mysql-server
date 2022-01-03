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

#ifndef PFS_EVENTS_STAGES_H
#define PFS_EVENTS_STAGES_H

/**
  @file storage/perfschema/pfs_events_stages.h
  Events waits data structures (declarations).
*/

#include <sys/types.h>
#include <atomic>

#include "mysql/components/services/bits/psi_stage_bits.h"
#include "storage/perfschema/pfs_events.h"
#include "storage/perfschema/pfs_global.h"

struct PFS_thread;
struct PFS_account;
struct PFS_user;
struct PFS_host;

/** A stage record. */
struct PFS_events_stages : public PFS_events {
  PSI_stage_progress m_progress;
};

void insert_events_stages_history(PFS_thread *thread, PFS_events_stages *stage);
void insert_events_stages_history_long(PFS_events_stages *stage);

extern bool flag_events_stages_current;
extern bool flag_events_stages_history;
extern bool flag_events_stages_history_long;

extern bool events_stages_history_long_full;
extern PFS_cacheline_atomic_uint32 events_stages_history_long_index;
extern PFS_events_stages *events_stages_history_long_array;
extern ulong events_stages_history_long_size;

int init_events_stages_history_long(uint events_stages_history_long_sizing);
void cleanup_events_stages_history_long();

void reset_events_stages_current();
void reset_events_stages_history();
void reset_events_stages_history_long();
void reset_events_stages_by_thread();
void reset_events_stages_by_account();
void reset_events_stages_by_user();
void reset_events_stages_by_host();
void reset_events_stages_global();
void aggregate_account_stages(PFS_account *account);
void aggregate_user_stages(PFS_user *user);
void aggregate_host_stages(PFS_host *host);

#endif
