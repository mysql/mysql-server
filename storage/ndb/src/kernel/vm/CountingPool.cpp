/*
   Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifdef TAP_TEST

#include <ndb_global.h>
#include <NdbTap.hpp>
#include "CountingPool.hpp"
#include "Pool.hpp"
#include "RWPool.hpp"
#include "test_context.hpp"
#include "WOPool.hpp"

#define JAM_FILE_ID 304

struct record
{
  int dummy;
};

template class CountingPool<record, RecordPool<record, RWPool> >;
template class CountingPool<record, RecordPool<record, WOPool> >;

TAPTEST(CountingPool)
{
  (void)test_context(100);
  // Only compile test. See template instantiations above.

  OK(true);

  return 1;
}

#endif

