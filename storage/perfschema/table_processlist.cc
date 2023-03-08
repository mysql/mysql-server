/* Copyright (c) 2008, 2023, Oracle and/or its affiliates.

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
  @file storage/perfschema/table_processlist.cc
  TABLE PROCESSLIST.
*/

#include "my_global.h"

#include "table_processlist.h"

#include <assert.h>
// #include "lex_string.h"
#include "my_compiler.h"

#include "my_thread.h"
#include "auth_acls.h"
#include "field.h"
#include "sql_class.h"
#include "sql_parse.h"
#include "table.h"
#include "pfs_buffer_container.h"
#include "pfs_global.h"
#include "pfs_instr.h"
#include "pfs_instr_class.h"

THR_LOCK table_processlist::m_table_lock;

static const TABLE_FIELD_TYPE field_types[] =
{
  {
    { C_STRING_WITH_LEN("ID") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("USER") },
    { C_STRING_WITH_LEN("varchar(" USERNAME_CHAR_LENGTH_STR ")") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("HOST") },
    { C_STRING_WITH_LEN("varchar(" HOST_AND_PORT_LENGTH_STR ")") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("DB") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("COMMAND") },
    { C_STRING_WITH_LEN("varchar(16)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("TIME") },
    { C_STRING_WITH_LEN("bigint(20)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("STATE") },
    { C_STRING_WITH_LEN("varchar(64)") },
    { NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("INFO") },
    { C_STRING_WITH_LEN("longtext") },
    { NULL, 0}
  },
};

TABLE_FIELD_DEF
table_processlist::m_field_def = {8, field_types};

PFS_engine_table_share_state
table_processlist::m_share_state = {
  false /* m_checked */
};

PFS_engine_table_share table_processlist::m_share = {
  {C_STRING_WITH_LEN("processlist")},
  &pfs_readonly_processlist_acl,
  table_processlist::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  cursor_by_thread::get_row_count,
  sizeof(PFS_simple_index), /* ref length */
  &m_table_lock,
  &m_field_def,
  false, /* m_perpetual */
  true, /* m_optional */
  &m_share_state
};

PFS_engine_table *table_processlist::create() {
  return new table_processlist();
}

table_processlist::table_processlist()
    : cursor_by_thread(&m_share), m_row_exists(false) {
  m_row_priv.m_auth = PROCESSLIST_DENIED;
}

int table_processlist::set_access(void) {
  THD *thd = current_thd;
  if (thd == NULL) {
    /* Robustness, no user session. */
    m_row_priv.m_auth = PROCESSLIST_DENIED;
    return 0;
  }

  if (thd->security_context()->check_access(PROCESS_ACL)) {
    /* PROCESS_ACL granted. */
    m_row_priv.m_auth = PROCESSLIST_ALL;
    return 0;
  }

  LEX_CSTRING client_priv_user = thd->security_context()->priv_user();
  if (client_priv_user.length == 0) {
    /* Anonymous user. */
    m_row_priv.m_auth = PROCESSLIST_DENIED;
    return 0;
  }

  /* Authenticated user, PROCESS_ACL not granted. */
  m_row_priv.m_auth = PROCESSLIST_USER_ONLY;
  m_row_priv.m_priv_user_length =
      std::min(client_priv_user.length, sizeof(m_row_priv.m_priv_user));
  memcpy(m_row_priv.m_priv_user, client_priv_user.str,
         m_row_priv.m_priv_user_length);
  return 0;
}

int table_processlist::rnd_init(bool scan) {
  set_access();
  return 0;
}

void table_processlist::make_row(PFS_thread *pfs) {
  pfs_optimistic_state lock;
  pfs_optimistic_state session_lock;
  pfs_optimistic_state stmt_lock;
  PFS_stage_class *stage_class;
  PFS_thread_class *safe_class;

  m_row_exists = false;

  if (m_row_priv.m_auth == PROCESSLIST_DENIED) {
    return;
  }

  /* Protect this reader against thread termination */
  pfs->m_lock.begin_optimistic_lock(&lock);

  safe_class = sanitize_thread_class(pfs->m_class);
  if (unlikely(safe_class == NULL)) {
    return;
  }

  /* Ignore background threads. */
  if (pfs->m_username_length == 0 || pfs->m_processlist_id == 0) return;

  m_row.m_processlist_id = pfs->m_processlist_id;

  /* Protect this reader against session attribute changes */
  pfs->m_session_lock.begin_optimistic_lock(&session_lock);

  /* Maintain user/host compatibility with the legacy SHOW PROCESSLIST. */
  const char *username = pfs->m_username;
  uint username_len = pfs->m_username_length;
  uint hostname_len = pfs->m_hostname_length;

  if (pfs->m_class->is_system_thread()) {
    if (username_len == 0 ||
        (!strncmp(username, "root", 4) && username_len == 4)) {
      username = "system user";
      username_len = strlen(username);
      hostname_len = 0;
    }
  } else {
    if (username_len == 0) {
      username = "unauthenticated user";
      username_len = strlen(username);
      hostname_len = 0;
    }
  }

  m_row.m_username_length = username_len;
  if (unlikely(m_row.m_username_length > sizeof(m_row.m_username))) {
    return;
  }

  if (m_row.m_username_length != 0) {
    memcpy(m_row.m_username, username, username_len);
  }

  m_row.m_hostname_length = hostname_len;
  if (unlikely(m_row.m_hostname_length > sizeof(m_row.m_hostname))) {
    return;
  }

  if (m_row.m_hostname_length != 0) {
    memcpy(m_row.m_hostname, pfs->m_hostname, m_row.m_hostname_length);
  }

  if (!pfs->m_session_lock.end_optimistic_lock(&session_lock)) {
    /*
      One of the columns:
      - USER
      - HOST
      is being updated.
      Do not discard the entire row.
      Do not loop waiting for a stable value.
      Just return NULL values.
    */
    m_row.m_username_length = 0;
    m_row.m_hostname_length = 0;
  }

  /* Enforce row filtering. */
  if (m_row_priv.m_auth == PROCESSLIST_USER_ONLY) {
    if (m_row.m_username_length != m_row_priv.m_priv_user_length) {
      return;
    }
    if (strncmp(m_row.m_username, m_row_priv.m_priv_user,
                m_row_priv.m_priv_user_length) != 0) {
      return;
    }
  }

  /* Protect this reader against statement attributes changes */
  pfs->m_stmt_lock.begin_optimistic_lock(&stmt_lock);

  m_row.m_dbname_length = pfs->m_dbname_length;
  if (unlikely(m_row.m_dbname_length > sizeof(m_row.m_dbname))) {
    return;
  }

  if (m_row.m_dbname_length != 0) {
    memcpy(m_row.m_dbname, pfs->m_dbname, m_row.m_dbname_length);
  }

  m_row.m_processlist_info_ptr = &pfs->m_processlist_info[0];
  m_row.m_processlist_info_length = pfs->m_processlist_info_length;

  if (!pfs->m_stmt_lock.end_optimistic_lock(&stmt_lock)) {
    /*
      One of the columns:
      - DB
      - INFO
      is being updated.
      Do not discard the entire row.
      Do not loop waiting for a stable value.
      Just return NULL values.
    */
    m_row.m_dbname_length = 0;
    m_row.m_processlist_info_length = 0;
  }

  /* Dirty read, sanitize the command. */
  m_row.m_command = pfs->m_command;
  if ((m_row.m_command < 0) || (m_row.m_command > COM_END)) {
    m_row.m_command = COM_END;
  }

  m_row.m_start_time = pfs->m_start_time;

  stage_class = find_stage_class(pfs->m_stage);
  if (stage_class != NULL) {
    m_row.m_processlist_state_ptr =
        stage_class->m_name + stage_class->m_prefix_length;
    m_row.m_processlist_state_length =
        stage_class->m_name_length - stage_class->m_prefix_length;
    if (m_row.m_processlist_state_length > 64) {
      /*
        Column STATE is VARCHAR(64)
        for compatibility reasons with the historical
        INFORMATION_SCHEMA.PROCESSLIST table.
        Stages however can have longer names.
        We silently truncate data here,
        so it fits into STATE.
      */
      m_row.m_processlist_state_length = 64;
    }
  } else {
    m_row.m_processlist_state_ptr = "";
    m_row.m_processlist_state_length = 0;
  }

  m_row.m_port = pfs->m_peer_port;

  if (m_row.m_hostname_length > 0 && m_row.m_port != 0) {
    /* Create HOST:PORT. */
    char str_port[10];
    sprintf(str_port, ":%d", m_row.m_port);
    std::string host(m_row.m_hostname, m_row.m_hostname_length);
    std::string host_ip = host + str_port;
    m_row.m_hostname_length =
        std::min((int)HOST_AND_PORT_LENGTH, (int)host_ip.length());
    memcpy(m_row.m_hostname, host_ip.c_str(), m_row.m_hostname_length);
  }

  if (!pfs->m_lock.end_optimistic_lock(&lock)) {
    return;
  }

  m_row_exists = true;
  return;
}

int table_processlist::read_row_values(TABLE *table, unsigned char *buf,
                                       Field **fields, bool read_all) {
  Field *f;

  if (unlikely(!m_row_exists)) {
    return HA_ERR_RECORD_DELETED;
  }

  /* Set the null bits */
  assert(table->s->null_bytes == 1);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /* ID */
          set_field_ulonglong(f, m_row.m_processlist_id);
          break;
        case 1: /* USER */
          if (m_row.m_username_length > 0) {
            set_field_varchar_utf8(f, m_row.m_username,
                                   m_row.m_username_length);
          } else {
            f->set_null();
          }
          break;
        case 2: /* HOST */
          if (m_row.m_hostname_length > 0) {
            set_field_varchar_utf8(f, m_row.m_hostname,
                                   m_row.m_hostname_length);
          } else {
            f->set_null();
          }
          break;
        case 3: /* DB */
          if (m_row.m_dbname_length > 0) {
            set_field_varchar_utf8(f, m_row.m_dbname, m_row.m_dbname_length);
          } else {
            f->set_null();
          }
          break;
        case 4: /* COMMAND */
          set_field_varchar_utf8(f, command_name[m_row.m_command].str,
                                 command_name[m_row.m_command].length);
          break;
        case 5: /* TIME */
          if (m_row.m_start_time) {
            time_t now = my_time(0);
            ulonglong elapsed =
                (now > m_row.m_start_time ? now - m_row.m_start_time : 0);
            set_field_ulonglong(f, elapsed);
          } else {
            f->set_null();
          }
          break;
        case 6: /* STATE */
          /* For compatibility, leave blank if state is NULL. */
          set_field_varchar_utf8(f, m_row.m_processlist_state_ptr,
                                 m_row.m_processlist_state_length);
          break;
        case 7: /* INFO */
          if (m_row.m_processlist_info_length > 0)
            set_field_blob(f, m_row.m_processlist_info_ptr,
                           m_row.m_processlist_info_length);
          else {
            f->set_null();
          }
          break;
        default:
          assert(false);
      }
    }
  }
  return 0;
}
