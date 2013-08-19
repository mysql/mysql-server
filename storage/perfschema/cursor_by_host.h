/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef CURSOR_BY_HOST_H
#define CURSOR_BY_HOST_H

/**
  @file storage/perfschema/cursor_by_host.h
  Cursor CURSOR_BY_HOST (declarations).
*/

#include "pfs_engine_table.h"
#include "pfs_host.h"
#include "table_helper.h"

/**
  @addtogroup Performance_schema_tables
  @{
*/

/** Cursor CURSOR_BY_HOST. */
class cursor_by_host : public PFS_engine_table
{
public:
  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

protected:
  cursor_by_host(const PFS_engine_table_share *share);

public:
  ~cursor_by_host()
  {}

protected:
  virtual void make_row(PFS_host *host)= 0;

private:
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;
};

/** @} */
#endif
