/*
   Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#include "util/require.h"
#include "ndb_global.h" // NDB_O_DIRECT_WRITE_ALIGNMENT
#include "kernel/ndb_limits.h"
#include "portlib/ndb_file.h"

#include <cstdlib>

#ifndef require
static inline void require(bool cond)
{
  if (cond)
    return;
  std::abort();
}
#endif

ndb_file::ndb_file()
: m_handle(os_invalid_handle),
  m_open_flags(0),
  m_write_need_sync(false),
  m_os_syncs_each_write(false),
  m_block_size(0),
  m_block_alignment(0),
  m_direct_io_block_size(0),
  m_direct_io_block_alignment(0),
  m_autosync_period(0)
{
  m_write_byte_count.store(0);
}

ndb_file::~ndb_file()
{
  require(m_handle == os_invalid_handle);
}

void ndb_file::init()
{
  m_handle = os_invalid_handle;
  m_open_flags = 0;
  m_write_need_sync = false;
  m_os_syncs_each_write = false;
  m_block_size = 0;
  m_block_alignment = 0;
  m_direct_io_block_size = 0;
  m_direct_io_block_alignment = 0;
  m_autosync_period = 0;
  m_write_byte_count.store(0);
}

int ndb_file::append(const void* buf, ndb_file::size_t count)
{
  require(check_block_size_and_alignment(buf, count, get_pos()));
  return write_forward(buf, count);
}

int ndb_file::set_autosync(size_t size)
{
  m_autosync_period = size;
  return 0;
}

int ndb_file::do_sync_after_write(size_t written_bytes)
{
  if (m_os_syncs_each_write)
  {
    return 0;
  }

  m_write_byte_count.fetch_add(written_bytes);

  if (!m_write_need_sync &&
      (m_autosync_period == 0 || m_write_byte_count.load() <= m_autosync_period))
  {
    return 0;
  }

  size_t unsynced = m_write_byte_count.exchange(0);
  if (unsynced == 0)
  {
    return 0;
  }

  return do_sync();
}

int ndb_file::sync()
{
  if (m_os_syncs_each_write)
  {
    return 0;
  }

  size_t unsynced = m_write_byte_count.exchange(0);
  if (unsynced == 0)
  {
    return 0;
  }

  return do_sync();
}
