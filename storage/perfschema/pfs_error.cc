/* Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file storage/perfschema/pfs_error.cc
  Server error instrument data structures (implementation).
*/

#include "storage/perfschema/pfs_error.h"

#include "my_sys.h"
#include "mysql_com.h"
#include "storage/perfschema/pfs_account.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_builtin_memory.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_host.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_user.h"

uint max_server_errors;
uint pfs_to_server_error_map[PFS_MAX_SERVER_ERRORS];

static void fct_reset_events_errors_by_thread(PFS_thread *thread) {
  PFS_account *account = sanitize_account(thread->m_account);
  PFS_user *user = sanitize_user(thread->m_user);
  PFS_host *host = sanitize_host(thread->m_host);
  aggregate_thread_errors(thread, account, user, host);
}

server_error error_names_array[] = {
#ifndef IN_DOXYGEN
    {0, 0, 0, 0, 0, 0},  // NULL ROW
#include <mysqld_ername.h>

    {0, 0, 0, 0, 0, 0}  // DUMMY ROW
#endif                  /* IN_DOXYGEN */
};

int init_error(const PFS_global_param *param) {
  /* Set the number of errors to be instrumented */
  max_server_errors = param->m_error_sizing;

  /* initialize global stats for errors */
  global_error_stat.init(&builtin_memory_global_errors);

  /* Initialize error index mapping */
  for (int i = 0; i < total_error_count + 1; i++) {
    if (error_names_array[i].error_index != 0) {
      pfs_to_server_error_map[error_names_array[i].error_index] = i;
    }
  }

  return 0;
}

void cleanup_error() {
  /* cleanup global stats for errors */
  global_error_stat.cleanup(&builtin_memory_global_errors);
}

/** Reset table EVENTS_ERRORS_SUMMARY_BY_THREAD_BY_ERROR data. */
void reset_events_errors_by_thread() {
  global_thread_container.apply(fct_reset_events_errors_by_thread);
}

static void fct_reset_events_errors_by_account(PFS_account *pfs) {
  PFS_user *user = sanitize_user(pfs->m_user);
  PFS_host *host = sanitize_host(pfs->m_host);
  pfs->aggregate_errors(user, host);
}

/** Reset table EVENTS_ERRORS_SUMMARY_BY_ACCOUNT_BY_ERROR data. */
void reset_events_errors_by_account() {
  global_account_container.apply(fct_reset_events_errors_by_account);
}

static void fct_reset_events_errors_by_user(PFS_user *pfs) {
  pfs->aggregate_errors();
}

/** Reset table EVENTS_ERRORS_SUMMARY_BY_USER_BY_ERROR data. */
void reset_events_errors_by_user() {
  global_user_container.apply(fct_reset_events_errors_by_user);
}

static void fct_reset_events_errors_by_host(PFS_host *pfs) {
  pfs->aggregate_errors();
}

/** Reset table EVENTS_ERRORS_SUMMARY_BY_HOST_BY_ERROR data. */
void reset_events_errors_by_host() {
  global_host_container.apply(fct_reset_events_errors_by_host);
}

/** Reset table EVENTS_ERRORS_GLOBAL_BY_ERROR data. */
void reset_events_errors_global() { global_error_stat.reset(); }

/*
   Function to lookup for the index of this particular error
   in errors' stats array.
*/
uint lookup_error_stat_index(uint mysql_errno) {
  uint offset = 0; /* Position where the current section starts in the array. */
  uint index = 0;

  if (mysql_errno < (uint)errmsg_section_start[0]) {
    return error_names_array[0].error_index;
  }

  for (uint i = 0; i < NUM_SECTIONS; i++) {
    if (mysql_errno >= (uint)errmsg_section_start[i] &&
        mysql_errno <
            (uint)(errmsg_section_start[i] + errmsg_section_size[i])) {
      /* Following +1 is to accomodate NULL row in error_names_array */
      index = mysql_errno - errmsg_section_start[i] + offset + 1;
      break;
    }
    offset += errmsg_section_size[i];
  }

  return error_names_array[index].error_index;
}
