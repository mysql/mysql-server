/* Copyright (c) 2011, 2019, Oracle and/or its affiliates. All rights reserved.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
  */

#ifndef CURSOR_BY_HOST_H
#define CURSOR_BY_HOST_H

/**
  @file storage/perfschema/cursor_by_host.h
  Cursor CURSOR_BY_HOST (declarations).
*/

#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/pfs_host.h"
#include "storage/perfschema/table_helper.h"

/**
  @addtogroup performance_schema_tables
  @{
*/

class PFS_index_hosts : public PFS_engine_index {
 public:
  PFS_index_hosts(PFS_engine_key *key_1) : PFS_engine_index(key_1) {}

  virtual ~PFS_index_hosts() {}

  virtual bool match(PFS_host *pfs) = 0;
};

/** Cursor CURSOR_BY_HOST. */
class cursor_by_host : public PFS_engine_table {
 public:
  static ha_rows get_row_count();

  virtual void reset_position(void);

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);

  virtual int index_next();

 protected:
  cursor_by_host(const PFS_engine_table_share *share);

 public:
  ~cursor_by_host() {}

 protected:
  virtual int make_row(PFS_host *host) = 0;

 private:
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

 protected:
  PFS_index_hosts *m_opened_index;
};

/** @} */
#endif
