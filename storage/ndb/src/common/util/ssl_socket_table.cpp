/*
   Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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
#include <assert.h>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include "portlib/ndb_socket.h"
#include "util/ssl_socket_table.h"

#if NDB_USE_SSL_SOCKET_TABLE

static constexpr int ssl_table_size = NDB_SSL_FIXED_TABLE_SIZE;

static struct ssl_st * ssl_table[ssl_table_size];
std::unordered_map<uint, struct ssl_st *> ssl_overflow_table;
std::shared_mutex ssl_table_mutex;  // a read/write lock


/* A Windows socket_t is an offset into a table of 32-bit values. So, the first
   Unix fds are 0, 1, and 2; but the first Win32 fds are 0x0, 0x4, 0x8.

   You could right-shift the Win32 fd two places (to divide it by four) in
   calculating the index. This would make more efficient use of the space in
   the fixed table, but with the overflow table available it is not strictly
   necessary.
*/
#define socket_to_index(s) (uint)s


void socket_table_set_ssl(socket_t s, struct ssl_st *ssl)
{
  assert(s != INVALID_SOCKET);
  uint fd = socket_to_index(s);

  std::unique_lock write_lock(ssl_table_mutex);     // unique lock

  if(fd < ssl_table_size)
  {
    assert(ssl_table[fd] == nullptr);
    ssl_table[fd] = ssl;
  }
  else
  {
    assert(ssl_overflow_table.count(fd) == 0);
    ssl_overflow_table[fd] = ssl;
  }
}

void socket_table_clear_ssl(socket_t s)
{
  assert(s != INVALID_SOCKET);
  uint fd = socket_to_index(s);

  std::unique_lock write_lock(ssl_table_mutex);     // unique lock

  if(fd < ssl_table_size)
  {
    assert(ssl_table[fd]);
    ssl_table[fd] = nullptr;
  }
  else
  {
    assert(ssl_overflow_table.count(fd) == 1);
    ssl_overflow_table.erase(fd);
  }
}

struct ssl_st * socket_table_get_ssl(socket_t s, bool expected [[maybe_unused]])
{
  struct ssl_st * ptr = nullptr;
  if(s == INVALID_SOCKET) return ptr;
  uint fd = socket_to_index(s);

  std::shared_lock read_lock(ssl_table_mutex);      // shared lock

  if(fd < ssl_table_size)
  {
    ptr = ssl_table[fd];
  }
  else
  {
    auto it = ssl_overflow_table.find(fd);
    if(it != ssl_overflow_table.end()) ptr = it->second;
  }

  assert(expected || ! ptr);
  return ptr;
}

#endif /* NDB_USE_SSL_SOCKET_TABLE */
