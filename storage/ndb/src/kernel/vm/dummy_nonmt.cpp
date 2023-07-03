/*
   Copyright (c) 2008, 2022, Oracle and/or its affiliates.

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
#include <cstring>
#include <ndb_types.h>
#include "mt.hpp"

void
mt_init_thr_map()
{
  assert(false);
}

void
mt_add_thr_map(Uint32, Uint32)
{
  assert(false);
}

void
mt_finalize_thr_map()
{
  assert(false);
}

Uint32
mt_get_instance_count(Uint32 block)
{
  assert(false);
  return 0;
}

Uint32
mt_get_extra_send_buffer_pages(Uint32 curr_num_pages,
                               Uint32 extra_mem_pages)
{
  (void)curr_num_pages;
  (void)extra_mem_pages;
  return 0;
}

Uint32
compute_jb_pages(struct EmulatorData*)
{
  return 0;
}


bool
NdbIsMultiThreaded()
{
  return false;
}

#include <BlockNumbers.h>

#define JAM_FILE_ID 222


Uint32
mt_get_blocklist(class SimulatedBlock * block, Uint32 arr[], Uint32 len)
{
  (void)block;
  for (Uint32 i = 0; i<NO_OF_BLOCKS; i++)
  {
    arr[i] = numberToBlock(MIN_BLOCK_NO + i, 0);
  }
  return NO_OF_BLOCKS;
}

void
mt_get_thr_stat(class SimulatedBlock *, ndb_thr_stat* dst)
{
  std::memset(dst, 0, sizeof(* dst));
  dst->name = "main";
}

void mt_get_spin_stat(class SimulatedBlock *, ndb_spin_stat *dst)
{
  memset(dst, 0, sizeof(* dst));
}

void mt_set_spin_stat(class SimulatedBlock *, ndb_spin_stat *dst)
{
}
