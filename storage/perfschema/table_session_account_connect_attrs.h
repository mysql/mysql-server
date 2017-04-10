/* Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_SESSION_ACCOUNT_CONNECT_ATTRS_H
#define TABLE_SESSION_ACCOUNT_CONNECT_ATTRS_H

/**
  @file storage/perfschema/table_session_account_connect_attrs.h
  TABLE SESSION_ACCOUNT_CONNECT_ATTRS.
*/

#include "table_session_connect.h"
#include "thr_lock.h"

/**
  @addtogroup performance_schema_tables
  @{
*/

/** Table PERFORMANCE_SCHEMA.SESSION_ACCOUNT_CONNECT_ATTRS. */
class table_session_account_connect_attrs : public table_session_connect
{
public:
  /** Table share */
  static PFS_engine_table_share m_share;
  /** Table builder */
  static PFS_engine_table* create();

protected:
  table_session_account_connect_attrs();

public:
  ~table_session_account_connect_attrs()
  {
  }

protected:
  virtual bool thread_fits(PFS_thread* thread);

private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
};

/** @} */
#endif
