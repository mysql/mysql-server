/* Copyright (c) 2012, 2022, Oracle and/or its affiliates.

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

#ifndef TABLE_SESSION_CONNECT_ATTRS_H
#define TABLE_SESSION_CONNECT_ATTRS_H

/**
  @file storage/perfschema/table_session_connect_attrs.h
  TABLE SESSION_CONNECT_ATTRS.
*/

#include "storage/perfschema/table_session_connect.h"
#include "thr_lock.h"

/**
  @addtogroup performance_schema_tables
  @{
*/

/** Table PERFORMANCE_SCHEMA.SESSION_CONNECT_ATTRS. */
class table_session_connect_attrs : public table_session_connect {
 public:
  /** Table share */
  static PFS_engine_table_share m_share;
  /** Table builder */
  static PFS_engine_table *create(PFS_engine_table_share *);

 protected:
  table_session_connect_attrs();

 public:
  ~table_session_connect_attrs() override = default;

 private:
  /** Table share lock. */
  static THR_LOCK m_table_lock;
  /** Table definition. */
  static Plugin_table m_table_def;
};

/** @} */
#endif
