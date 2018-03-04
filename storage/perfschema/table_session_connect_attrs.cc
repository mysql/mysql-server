/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/table_session_connect_attrs.cc
  TABLE SESSION_CONNECT_ATTRS.
*/

#include "storage/perfschema/table_session_connect_attrs.h"

#include <stddef.h>

THR_LOCK table_session_connect_attrs::m_table_lock;

Plugin_table table_session_connect_attrs::m_table_def(
  /* Schema name */
  "performance_schema",
  /* Name */
  "session_connect_attrs",
  /* Definition */
  "  PROCESSLIST_ID BIGINT UNSIGNED NOT NULL,\n"
  "  ATTR_NAME VARCHAR(32) NOT NULL,\n"
  "  ATTR_VALUE VARCHAR(1024),\n"
  "  ORDINAL_POSITION INT,\n"
  "  PRIMARY KEY (PROCESSLIST_ID, ATTR_NAME)\n",
  /* Options */
  " ENGINE=PERFORMANCE_SCHEMA CHARACTER SET utf8 COLLATE utf8_bin",
  /* Tablespace */
  nullptr);

PFS_engine_table_share table_session_connect_attrs::m_share = {
  &pfs_readonly_acl,
  table_session_connect_attrs::create,
  NULL, /* write_row */
  NULL, /* delete_all_rows */
  cursor_by_thread_connect_attr::get_row_count,
  sizeof(pos_connect_attr_by_thread_by_attr), /* ref length */
  &m_table_lock,
  &m_table_def,
  false, /* perpetual */
  PFS_engine_table_proxy(),
  {0},
  false /* m_in_purgatory */
};

PFS_engine_table*
table_session_connect_attrs::create(PFS_engine_table_share*)
{
  return new table_session_connect_attrs();
}

table_session_connect_attrs::table_session_connect_attrs()
  : table_session_connect(&m_share)
{
}
