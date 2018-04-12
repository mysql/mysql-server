/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/basic_ostream.h"
#include "my_inttypes.h"
#include "mysql/components/services/log_shared.h"
#include "mysql/psi/mysql_file.h"
#include "mysqld_error.h"
#include "sql/log.h"

IO_CACHE_ostream::IO_CACHE_ostream() {
  memset(&m_io_cache, 0, sizeof(m_io_cache));
}
IO_CACHE_ostream::~IO_CACHE_ostream() { close(); }

bool IO_CACHE_ostream::open(
#ifdef HAVE_PSI_INTERFACE
    PSI_file_key log_file_key,
#endif
    const char *file_name, myf flags) {
  File file = -1;

  if ((file = mysql_file_open(log_file_key, file_name, O_CREAT | O_WRONLY,
                              MYF(MY_WME))) < 0)
    return true;

  if (init_io_cache(&m_io_cache, file, IO_SIZE, WRITE_CACHE, 0, 0, flags)) {
    mysql_file_close(file, MYF(0));
    return true;
  }
  return false;
}

bool IO_CACHE_ostream::close() {
  if (my_b_inited(&m_io_cache)) {
    int ret = end_io_cache(&m_io_cache);
    ret |= mysql_file_close(m_io_cache.file, MYF(MY_WME));
    return ret != 0;
  }
  return false;
}

my_off_t IO_CACHE_ostream::tell() {
  DBUG_ASSERT(my_b_inited(&m_io_cache));
  return my_b_tell(&m_io_cache);
}

bool IO_CACHE_ostream::seek(my_off_t offset) {
  DBUG_ASSERT(my_b_inited(&m_io_cache));
  return reinit_io_cache(&m_io_cache, WRITE_CACHE, offset, false, true);
}

bool IO_CACHE_ostream::write(const unsigned char *buffer, my_off_t length) {
  DBUG_ASSERT(my_b_inited(&m_io_cache));
  return my_b_safe_write(&m_io_cache, buffer, length);
}

bool IO_CACHE_ostream::truncate(my_off_t offset) {
  DBUG_ASSERT(my_b_inited(&m_io_cache));
  DBUG_ASSERT(m_io_cache.file != -1);

  if (my_chsize(m_io_cache.file, offset, 0, MYF(MY_WME))) return true;

  reinit_io_cache(&m_io_cache, WRITE_CACHE, offset, false, true);
  return false;
}

bool IO_CACHE_ostream::flush() {
  DBUG_ASSERT(my_b_inited(&m_io_cache));
  return flush_io_cache(&m_io_cache);
}

bool IO_CACHE_ostream::sync() {
  DBUG_ASSERT(my_b_inited(&m_io_cache));
  return mysql_file_sync(m_io_cache.file, MYF(MY_WME)) != 0;
}
